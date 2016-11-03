#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#define NBYTES       666
#define SEED_DEFAULT 666

typedef void (*BADBOY) ();

int main(int argc, char **argv) {
  unsigned char fn_data[NBYTES];            /* Holds garbage program "TEXT" */
  BADBOY        fn_ptr = (BADBOY) &fn_data; /* Allows execution of same.    */

  mprotect( (void *) ((((long) fn_data) / getpagesize()) * getpagesize()),
            ((NBYTES / getpagesize()) + 1 ) * getpagesize(),
            PROT_READ | PROT_WRITE | PROT_EXEC);

  srand( argc > 1 ? atoi(argv[1]) : SEED_DEFAULT );
  for (int i=0; i < NBYTES; i++)
    fn_data[i] = rand() & 0xFF;

  fn_ptr ();
}
