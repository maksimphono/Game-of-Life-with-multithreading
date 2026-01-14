#include "workers.h"

void workers_init(pthread_t* workers, worker_param_t* parameters, cyclic_barrier_t* barrier, worker_function_t worker_function, int number_of_threads) {
    for (int i = 0; i < number_of_threads; i++) {
        parameters[i].barrier = barrier;
        pthread_create(&workers[i], NULL, worker_function, &parameters[i]);
    }
}

void destroy_workers(pthread_t* workers, worker_param_t* parameters) {
    free(workers);
    free(parameters);
}