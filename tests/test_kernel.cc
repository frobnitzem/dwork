#include <stdio.h>
#include <stdlib.h>

struct Kernel {
    long iterations;
    void *x;
    Kernel(long y) : iterations(y) {}
};

double execute_kernel_compute(const Kernel &k);

#include <sys/time.h>
inline int64_t time_in_us() {
    struct timeval  time;
    struct timezone zone;
    gettimeofday(&time, &zone);
    return time.tv_sec*1000000 + time.tv_usec;
}

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
    int64_t t0 = time_in_us();
    for(long i=0; i<nrun; i++) {
        Kernel k(iter);
        x += execute_kernel_compute(k);
    }
    int64_t t1 = time_in_us();
    printf("x = %e\n", x);
    printf("GFLOP = %e, GFLOPS = %f\n", (double)nrun*(iter*128+64)*1e-9,
                                        (double)nrun*(iter*128+64)*1e-3 / (t1-t0));
    printf("time (us) = %ld\n", t1-t0);

    return 0;
}
