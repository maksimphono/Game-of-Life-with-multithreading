#include "life.h"
#include <pthread.h>
#include <semaphore.h>

typedef enum {False, True} boolean;

typedef struct cyclic_barrier_t {
    pthread_mutex_t barrier_mutex;
    pthread_cond_t barrier_cond;
    int participants;
    int outstanding;
    void (*barrier_function) (void);
} cyclic_barrier_t;

typedef struct worker_param_t {
    int start_row;
    int start_col;
    int length;
    cyclic_barrier_t* barrier;
} worker_param_t;

int cyclic_barrier_init(cyclic_barrier_t* barrier, pthread_mutexattr_t* barrier_mutexattr, pthread_condattr_t* barrier_condattr,  int participants, void (*barrier_function)(void)) {
    pthread_mutex_init(&(barrier->barrier_mutex), barrier_mutexattr);
    pthread_cond_init(&(barrier->barrier_cond), barrier_condattr);

    barrier->participants = participants;
    barrier->outstanding = participants;
    barrier->barrier_function = barrier_function;

    return 0;
}