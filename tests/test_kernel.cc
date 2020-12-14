#include <stdio.h>
#include <stdlib.h>

struct Kernel {
    long iterations;
    void *x;
    Kernel(long y) : iterations(y) {}
};

double execute_kernel_compute(const Kernel &k);

// Note: The taskbench paper uses this kernel
//       in the N=(cores) x T=1000 stencil pattern.
//
//       The number of iterations is the independent
//       variable of the graphs.  Task granularity
//       = time for a single CPU to execute the task
//       = walltime divided by T=1000.
//
//       The kernel is either 128*iterations+64
//                  or        128*iterations+128
//       (depending on whether or not initial set is counted).


int main(int argc, char *argv[]) {
    double x = 0.0;
    //const long iter = 1<<9;
    const long iter = atol(argv[1]);
    const long nrun = atol(argv[2]);
    for(long i=0; i<nrun; i++) {
        Kernel k(iter);
        x += execute_kernel_compute(k);
    }
    printf("GFLOP = %e\n", (double)nrun*(iter*128+64)*1e-9);
    printf("x = %e\n", x);
    return 0;
}
