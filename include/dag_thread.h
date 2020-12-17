#ifndef _DAG_PTHREAD_H
#define _DAG_PTHREAD_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/errno.h>
#include <stdint.h>
#include <unistd.h>

#define Gethostname(name, len) { \
    if(gethostname(name, len)) { \
        perror("Error in gethostname"); \
    } \
}

#define Pthread_create(thread, attr, start_routine, arg) { \
    if(pthread_create(thread, attr, start_routine, arg)) { \
        fprintf(stderr, "Tried to join invalid thread at %s:%d (in %s)", \
                        __FILE__, __LINE__, __func__); \
        exit(1); \
    } \
}

#define Pthread_join(thread, value_ptr) { \
    if(pthread_join(thread, value_ptr)) { \
        fprintf(stderr, "Tried to join invalid thread at %s:%d (in %s)", \
                        __FILE__, __LINE__, __func__); \
        exit(1); \
    } \
}

#define Pthread_mutex_init(m, attr) { \
    int r; \
    if( (r = pthread_mutex_init(m, attr))) { \
        fprintf(stderr, "Failed pthread_mutex_init at %s:%d (in %s)" \
                        " -- err = %d\n", \
                        __FILE__, __LINE__, __func__, r); \
        exit(1); \
    } \
}
#define Pthread_mutex_destroy(m) { \
    int r = pthread_mutex_destroy(m); \
    if(r) { \
        { \
            fprintf(stderr, "Failed pthread_mutex_destroy at %s:%d (in %s)" \
                            " -- err = %d\n", \
                            __FILE__, __LINE__, __func__, r); \
            exit(1); \
        } \
    } \
}

typedef void (*thread_init_f)(int rank, int nodes, void *data, void *info);
typedef void *(*thread_work_f)(void *data);

static int run_threaded(int n, size_t size, thread_init_f setup,
                        thread_work_f work, thread_init_f dtor, void *info) {
    int i = 0, ret = 1;
    int8_t *data = (int8_t *) calloc(n, size + sizeof(pthread_t));
    pthread_t *tid = (pthread_t *)(data + n*size);
    if(data == NULL) {
        goto err;
    }

    if(setup != NULL) {
        for(i=0; i<n; i++) {
            setup(i, n, (void *)(data + i*size), info);
        }
    }
    for(i=0; i<n; i++) {
        if(pthread_create(&tid[i], NULL, work, (void *)(data + i*size))) {
            break;
        }
    }
    n = i;
    for(i--; i>=0; i--) {
        Pthread_join(tid[i], NULL);
    }
    if(dtor != NULL) {
        for(i=0; i<n; i++) {
            dtor(i, n, (void *)(data + i*size), info);
        }
    }
    ret = 0;

err:
    if(ret) { // encountered error - must destroy all threads < i
        while(--i >= 0) {
            Pthread_join(tid[i], NULL);
        }
    }
    if(data) free(data);
    return ret;
}

#endif
