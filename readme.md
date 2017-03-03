# Crashme--

Crashme-- is a simplified fork of George Carrette's [crashme](people.delphiforums.com/gjc/crashme.html) program, which executes random bytecode as a test of system stability. As its name suggests, crashme-- is less featureful than crashme: it removes suppport for VMS, Windows, and HP_UX; it doesn't have its own prng routines; it won't log in as much detail; and it supports fewer configurable options.

This is a work in progresss.

### How it Works

Crashme-- fills an array of bytes with random values, and executes it via a function pointer. This is very simple to do, as the listing below illustrates. Note that `mprotect(2)` must be called to disable the [NX bit](https://en.wikipedia.org/wiki/NX_bit) on the memory pages containing fn_data.

```c
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
```

### Building Crashme--

```bash
$ git clone https://github.com/28mm/crashme--
$ cd crashme--
$ make
```

### Running Crashme--


Selinux may prevent crashme-- from working correctly.

```bash
$ sudo setenforce 0
```

Crashme-- has two modes: a worker mode, which executes random data; and a supervisor mode that spawns a specified number of children in worker mode.

```bash
$ crashme-- --bytes=1024 --seed=666 --worker
```

```bash
$ crashme-- --bytes=1024 --seed=666 --fork=4
```
