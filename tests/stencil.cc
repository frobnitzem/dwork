#include <dag.hh>
#include <math.h>
#include <stdint.h>
#include <proto.hh>

#ifndef M_PI
#define M_PI (3.141592653589793)
#endif

using namespace std;

long iter = 0;

struct Kernel {
    long iterations;
    void *x;
    Kernel(long y) : iterations(y) {}
};

double execute_kernel_compute(const Kernel &k);

// test 6
task_t *call_kernel(void *a, void *b) {
    double x;
    if(iter > 0) {
        Kernel k(iter);
        x = execute_kernel_compute(k);
        if(x > 10.0) a = a+1;
    }
    return NULL;
};
int stencil(long N, long T, int nthreads) {
    int ret = 1, j = 0, width = N*T;
    int64_t t2, t1, t0 = time_in_us();
    int *ids = (int *)malloc(sizeof(int)*width);
    task_t *start = start_task();
    task_t *tasks = new_tasks(width);

    if(start == NULL || tasks == NULL) {
        printf("# memory alloc error.\n");
        return 1;
    }
    long k = 0;
    for(long i=0; i<N; i++,k++) {
        ids[k] = k+1;
        set_task_info(tasks+k, &ids[k]);
        j += link_task(tasks+k, start);
    }
    for(long t=1; t<T; t++) {
    for(long i=0; i<N; i++,k++) {
        ids[k] = k+1;
        set_task_info(tasks+k, &ids[k]);
        j += link_task(tasks+k, tasks+k-N);
        if(i > 0)   j += link_task(tasks+k, tasks+k-N-1);
        if(i < N-1) j += link_task(tasks+k, tasks+k-N+1);
    } }
    printf("# linked %d tasks\n", j);
    long links = N + (T-1)*(N == 1 ? 1 : 3*N-2);
    if(j != links) {
        goto err;
    }

    t1 = time_in_us();
    exec_dag2(start, call_kernel, nullptr, nullptr, nthreads, nullptr);
    t2 = time_in_us();
    printf("init = %lu exec = %lu\n", t1-t0, t2-t1);
    
    ret = 0;

err:
    del_tasks(width, tasks);
    free(ids);

    return ret;
}
int main(int argc, char *argv[]) {
    iter = atol(argv[1]);
    long N = atol(argv[2]);
    long T = atol(argv[3]);
    int threads = atol(argv[4]);

    return stencil(N, T, threads);
}
