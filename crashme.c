/* -*- Mode: C; indent-tabs-mode: nil -*- */

char *crashme_version = "-1";

/*
 *             COPYRIGHT (c) 1990-2014 BY        *
 *  GEORGE J. CARRETTE, CONCORD, MASSACHUSETTS.  *
 *             ALL RIGHTS RESERVED               *

Permission to use, copy, modify, distribute and sell this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all copies
and that both that copyright notice and this permission notice appear
in supporting documentation, and that the name of the author
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
HE BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <getopt.h>
#include <stdarg.h>

#define TRUE  1
#define FALSE 0

#define BYTES_DEFAULT    8192
#define SEED_DEFAULT     666
#define NSUBS_DEFAULT    4
#define ATTEMPTS_DEFAULT 100

#define LONGJMP_RETURN   3
jmp_buf jbuf;


#define V_ERR       1
#define V_IMPORTANT 2
#define V_INFO      3
#define V_EXTRA     5
#define V_DEBUG     6

#define VERBOSE_DEFAULT  V_INFO

typedef void (*BADBOY) ();

typedef struct status_list
{
  long status;
  long count;
  struct status_list *next;
} status_list;

status_list *slist = NULL;

unsigned char static_data[BYTES_DEFAULT]; /* Use if we get --static argument */
unsigned char *bb_data; /* Filled by compute_badboy()                        */
BADBOY        badboy;   /* Then cast as a function pointer                   */

int worker_flag  = FALSE; /* Parent or Child?                                 */
int static_flag  = FALSE; /* Replaces (nbytes == MAGIC_NBYTES_STATIC)         */


long nbytes           = BYTES_DEFAULT;
long nseed            = SEED_DEFAULT;
long nsubs            = NSUBS_DEFAULT;
long attempts         = ATTEMPTS_DEFAULT;
long verbose          = VERBOSE_DEFAULT;
int attempt           = 0;
long child_kill_count = 0;

void PRINT(int level, char *fmt, ...);
void copyright (void);
void usage(void);

unsigned char *bad_malloc (long n);
void spawn_crashmetoos(char *cmd);
void badboy_loop (void);

int
main (int argc, char **argv)
{
  int i = 0;
  int opt = 0;
  int option_index = 0;
  char *cmd = argv[0]; // FIXME global.

  static struct option long_options[] = {
    { "worker",   no_argument,       &worker_flag, TRUE },
    { "static",   no_argument,       &static_flag, TRUE },
    { "seed",     optional_argument, NULL, 's' },
    { "bytes",    optional_argument, NULL, 'b' },
    { "attempts", optional_argument, NULL, 'a' },
    { "verbose",  optional_argument, NULL, 'v' },
    { "fork",     optional_argument, NULL, 'j'},
  };

  while ((opt = getopt_long(argc, argv, "s:b:a:v:j:",
                            long_options, &option_index)) != -1)
    {
      switch (opt) {
      case 's':
        nseed = atol(optarg);
        break;
      case 'b':
        nbytes = atol(optarg);
        break;
      case 'a':
        attempts = atol(optarg);
        break;
      case 'v':
        verbose = atol(optarg);
        break;
      case 'j':
        nsubs = atol(optarg);
        break;
      default:
        continue; // FIXME FIXME FIXME FIXME FIXME
        for (i=0; i<argc; i++)
          printf("%s ", argv[i]);
        printf("\n");
        printf("%c, %d", opt, option_index);
        fflush(stdout);
        usage();
        exit(EXIT_FAILURE);
      }
    }

  /* Are we a child process? */
  if (worker_flag) {

    PRINT(V_INFO, "PID=%d\t[-]\tBYTES=%ld\tSEED=%ld\tATTEMPTS=%ld",
          getpid(), nbytes, nseed, attempts); // FIXME static_flag

    if (static_flag)
      bb_data = static_data;
    else
      bb_data = bad_malloc ((nbytes < 0) ? -nbytes : nbytes); // FIXME fixup nbytes sign.

    badboy = (BADBOY) (bb_data);
    PRINT(V_INFO, "PID=%d\t[-]\tBADBOY 0x%lX",
          getpid(), (long) badboy);

    srand(nseed);
    badboy_loop ();
  }

  /* Or must we spawn children? */
  else {
    PRINT(V_INFO,
          "PID=%d\t[+]\tCREATING %ld CRASHME WORKERS",
          getpid(), nsubs);
    srand(nseed);
    spawn_crashmetoos(cmd);
  }

  return (EXIT_SUCCESS);
}

void
PRINT(int level, char *fmt, ...)
{
  if (level > verbose)
    return;

  static char buffer[4192]; /* Arbitrary dimension. */
  va_list args;

  va_start(args, fmt);
  vsprintf(buffer, fmt, args);
  va_end(args);
  printf("%s\n", buffer);  // FIXME
  fflush(stdout);

}

void
longjmperror(void)
{
  PRINT(V_ERR, "ERROR: longjmperr");
  exit(EXIT_FAILURE);
}

void
set_exec_prot (unsigned char *data, long n)
{
  int pagesize;
  pagesize = getpagesize ();
  if (mprotect ((void *) ((((long) data) / pagesize) * pagesize),
                ((n / pagesize) + 1) * pagesize,
                PROT_READ | PROT_WRITE | PROT_EXEC))
    perror ("mprotect");
}

unsigned char *
bad_malloc (long n)
{
  unsigned char *data;

  if (static_flag)
    {
      data = &static_data[0];
      set_exec_prot(data, BYTES_DEFAULT);
    }
  else
    {
      data = (unsigned char *) malloc (n);
      if (data == NULL)
        {
          perror ("malloc");
          exit (EXIT_FAILURE);
        }
      set_exec_prot (data, n);
    }

  return (data);
}

void
signal_handler (int sig)
{
  PRINT(V_DEBUG, "Signal Handler Entrypoint.");
  char *ss;
  switch (sig)
    {
    case SIGILL:
      ss = "Illegal Instruction";
      break;
    case SIGTRAP:
      ss = "Trace Trap";
      break;
    case SIGFPE:
      ss = "Arithmetic Exception";
      break;
    case SIGBUS:
      ss = "Bus Error";
      break;
    case SIGSEGV:
      ss = "Segmentation Violation";
      break;
    case SIGIOT:
      ss = "IOT Instruction";
      break;
    case SIGALRM:
      ss = "Alarm Clock";
      break;
    case SIGINT:
      ss = "Interrupt";
      break;
    default:
      ss = " ";
    }
  PRINT(V_INFO, "PID=%d\t#%d/#%d\tSIGNAL %d %s",
        getpid(), attempt, attempts, sig, ss);
  longjmp (jbuf, 3);
}

void
register_signal (int sig, void (*func) ())
{
  struct sigaction act;
  memset (&act, 0, sizeof (act));
  act.sa_handler = func;
  act.sa_flags = SA_RESETHAND | SA_RESTART | SA_NODEFER;
  sigaction (sig, &act, 0);
}

void
register_signals (void)
{
  register_signal (SIGILL,  signal_handler);
  register_signal (SIGTRAP, signal_handler);
  register_signal (SIGFPE,  signal_handler);
  register_signal (SIGBUS,  signal_handler);
  register_signal (SIGSEGV, signal_handler);
  register_signal (SIGIOT,  signal_handler);
  register_signal (SIGALRM, signal_handler);
  register_signal (SIGINT,  signal_handler);
}

void
compute_badboy (void)
{
  long j;
  if (static_flag == FALSE)
    bb_data = bad_malloc (nbytes);
  for (j = 0; j < nbytes; ++j)
    bb_data[j] = rand () & 0xFF;

  badboy = (BADBOY) (bb_data);
}

void
copyright()
{
  if (worker_flag == FALSE) {
    PRINT(V_INFO, "Crashme: (c) Copyright 1990-2012 George J. Carrette");
    PRINT(V_INFO, "From http://alum.mit.edu/www/gjc/crashme.html");
    PRINT(V_INFO, "Crashme-- Changes, Errors, Regressions: 2016 https://github.com/28mm");
    PRINT(V_INFO, "From http://github.com/28mm/crashme--");
  }
}

void usage (void) {
  PRINT(V_ERR, "Usage: ");
  /* FIXME actually print usage */
}

void
badboy_loop (void)
{
  for (attempt = 0; attempt < attempts; ++attempt)
    {
      compute_badboy ();

      PRINT(V_INFO, "PID=%d\t#%d/#%d\tBADBOY 0x%lX",
            getpid(), attempt, attempts, (long) badboy);

      if (setjmp (jbuf) == LONGJMP_RETURN)
        PRINT(V_EXTRA, "PID=%d\t#%d/#%d\tBARFED BLAMO SPLAT",
              getpid(), attempt, attempts);
      else
        {
          register_signals (); /* Trap SIGSEGV, SIGILL, other likely signals.  */
          alarm (10);          /* In case `badboy' doesn't halt: SIGALRM.      */
          PRINT(V_DEBUG, "made it this far!");
          (*badboy) ();        /* Execute `badboy'. */
          PRINT(V_DEBUG, "didn't get here, though!");
          PRINT(V_EXTRA, "PID=%d\t#%d\tDidn't barf!", getpid(), attempt);
        }
    }
}

void
record_status (long n)
{
  struct status_list *l;
  for (l = slist; l != NULL; l = l->next)
    if (n == l->status)
      {
        ++l->count;
        return;
      }
  l = (struct status_list *) malloc (sizeof (struct status_list));
  l->count  = 1;
  l->status = n;
  l->next   = slist;
  slist     = l;
}

void
summarize_status (void)
{
  struct status_list *l;
  char *status_str = "";
  int n = 0;

  PRINT(V_ERR, "child_kill_count %ld", child_kill_count);
  PRINT(V_ERR, "exit status ... number of cases");

  for (l = slist; l != NULL; l = l->next)
    {
      PRINT(V_INFO, "%11ld ... %5ld %s",
            (long) l->status,
            (long) l->count,
            status_str);
      ++n;
    }

  PRINT(V_ERR, "Number of distinct cases = %d", n);
}

long monitor_pid = 0;
long monitor_period = 5;
long monitor_limit = 6;         /* 30 second limit on a subprocess */
long monitor_count = 0;
long monitor_active = 0;

void
monitor_fcn (int sig)
{
  long status;
  register_signal (SIGALRM, monitor_fcn);
  alarm (monitor_period);
  if (monitor_active)
    {
      ++monitor_count;
      if (monitor_count >= monitor_limit)
        {
          PRINT(V_ERR, "time limit reached on pid %ld 0x%lX. using kill.",
                (long) monitor_pid, (long) monitor_pid);

          if ((status = kill (monitor_pid, SIGKILL)) < 0)
            PRINT(V_ERR, "failed to kill process");
          else
            ++child_kill_count;

          monitor_active = 0;
        }
    }
}

void
spawn_crashmetoos(char *cmd)
{
  long j, pid, total_time, days, hours, mins, secs;
  char bytes_str[50], seed_str[50], attempts_str[50], verbose_str[50];
  int status;
  long pstatus;
  time_t before_time, after_time;

  register_signal (SIGALRM, monitor_fcn);
  alarm (monitor_period);

  time (&before_time);

  /* except for seed, all agruments to child processes are the same.*/
  sprintf(bytes_str, "--bytes=%ld", nbytes);
  sprintf(attempts_str, "--attempts=%ld", attempts);
  sprintf(verbose_str, "--verbose=%ld", verbose);

  for (j = 0; j < nsubs; ++j)
    {
      sprintf(seed_str, "--seed=%d", rand()); /* New seed for each child. */
      if ((pstatus = fork ()) == 0)
        {
          if ((status = execlp (cmd, cmd, "--worker", bytes_str,
                                seed_str, attempts_str,
                                verbose_str, NULL)) == -1)
            {
              perror (cmd);        /* execlp(...) failure    */
              exit (EXIT_FAILURE);
            }
        }

      else if (pstatus < 0) {
        perror ("fork");          /* fork(...) failure       */
        exit(EXIT_FAILURE);
      }

      else                        /* OKAY fork() and exec()  */
        {
          PRINT(V_ERR, "PID=%ld\t WORKER=%ld", pstatus,
                j + 1);

          monitor_pid = pstatus;
          monitor_count = 0;
          monitor_active = 1;
          while ((pid = wait (&status)) > 0)
            {
              monitor_active = 0;
              PRINT(V_ERR, "PID=%ld\tEXIT %d",
                    (long) pid, status);
              record_status (status);
            }
        }
    }

  time (&after_time);
  secs  = after_time - before_time;
  mins  = secs  / 60;
  hours = mins  / 60;
  days  = hours / 24;
  secs  = secs  % 60;
  mins  = mins  % 60;
  hours = hours % 24;

  PRINT(V_INFO,
        "Test complete, total real time: %ld seconds (%ld %02ld:%02ld:%02ld)",
        (long) total_time, (long) days, (long) hours, (long) mins, (long) secs);

  summarize_status ();

}
