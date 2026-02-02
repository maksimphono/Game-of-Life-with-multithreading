#ifndef _WORKERS_H_
#define _WORKERS_H_

#include "cyclic_barrier.h"
#include <stdlib.h>

typedef struct worker_param_t {
    int start_row;
    int start_col;
    int length;
    cyclic_barrier_t* barrier;
} worker_param_t;

typedef void* (*worker_function_t)(void*);

void workers_init(pthread_t* workers, worker_param_t* parameters, cyclic_barrier_t* barrier, worker_function_t worker_function, int number_of_threads);

void destroy_workers(pthread_t* workers, worker_param_t* parameters);

#endif