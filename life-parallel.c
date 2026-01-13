#include "life.h"
#include <pthread.h>
#include <semaphore.h>

// reference: https://github.com/dogmelons/Threaded-Game-Of-Life

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



int cyclic_barrier_await(cyclic_barrier_t* barrier) {
    pthread_mutex_lock(&(barrier->barrier_mutex));

    barrier->outstanding -= 1;

    if (barrier->outstanding) {
        pthread_cond_wait(&(barrier->barrier_cond), &(barrier->barrier_mutex));
    } else {
        (barrier->barrier_function)();
        barrier->outstanding = barrier->participants;
        pthread_cond_broadcast(&(barrier->barrier_cond));
    }

    pthread_mutex_unlock(&(barrier->barrier_mutex));

    return 0;
}

int cyclic_barrier_destroy(cyclic_barrier_t* barrier) {
    pthread_cond_destroy(&(barrier->barrier_cond));
    pthread_mutex_destroy(&(barrier->barrier_mutex));

    return 0;
}

int split_board(worker_param_t* params, int workers_num, int rows, int columns) {
    // if there are equal/less threads than rows
    if ((rows / workers_num) != 0) {
        int extra = rows % workers_num;
        int maxPer = rows / workers_num;

        int curr_row = 0;
        for (int i = 0; i < workers_num; i++) {
            if (extra > 0) {
                params[i].start_row = curr_row;
                params[i].start_col = 0;
                params[i].length = (maxPer + 1) * columns;
                curr_row += maxPer + 1;
                extra -= 1;
            } else {
                params[i].start_row = curr_row;
                params[i].start_col = 0;
                params[i].length = maxPer * columns;
                curr_row += maxPer;
            }
        }
    } else if (workers_num > (rows * columns)) {
        // more threads than cells (unlikely)
        int overflow = 0;
        overflow = workers_num - (rows * columns);
        int curr_row = 0;
        int curr_col = 0;

        for (int i = 0; i < workers_num; i++) {
            if ((workers_num - i) > overflow) {
                params[i].start_row = curr_row;
                params[i].start_col = curr_col;
                params[i].length = 1;

                if ((curr_col + 1) >= columns) {
                    curr_col = 0;
                    curr_row += 1;
                } else {
                    curr_col += 1;
                }
            } else {
                // this may need tweaking, but for now having a length of 0 should suffice for extra threads
                params[i].start_row = 0;
                params[i].start_col = 0;
                params[i].length = 0;
            }
        }
    } else {
        // split rows will be more efficient
        int curr_row = 0;
        int curr_col = 0;

        for (int i = 0; i < workers_num; i++) {
            // assign one cell to each extra thread
            if ((workers_num - i) > (rows - curr_row)) {
                params[i].start_row = curr_row;
                params[i].start_col = curr_col;
                params[i].length = 1;

                if ((curr_col + 1) >= columns) {
                    curr_col = 0;
                    curr_row += 1;
                } else {
                    curr_col += 1;
                }
            } else {
                // if we're transitioning from assigning cells to assigning rows
                if (curr_col != 0) {
                    int columnsLeft = columns - curr_col;
                    params[i].start_row = curr_row;
                    params[i].start_col = curr_col;
                    params[i].length = columnsLeft;

                    curr_col = 0;
                    curr_row += 1;
                } else {
                    params[i].start_row = curr_row;
                    params[i].start_col = curr_col;
                    params[i].length = columns;

                    curr_row += 1;
                }
            }
        }
    }

    return 0;
}


int rows = 0;
int columns = 0;
int generations = 0;
int size = 0;
LifeBoard* primary_board;
LifeBoard* secondary_board;

// processes a single step, used in the workers
void process_step(worker_param_t* params) {
    // Calculate the actual safe inner range for this thread
    int min_row = 1;
    int max_row = rows - 2;
    int min_col = 1;

    int start_row = params->start_row;
    int start_col = params->start_col;

    // Clip to inner region
    if (min_row > start_row) 
        start_row = min_row;

    if (min_col > start_col) 
        start_col = min_col;

    // Compute how many rows/columns this thread actually processes
    int rows_to_process = params->length / columns;  // approximate
    int end_row = start_row + rows_to_process;
    if (end_row > max_row + 1) end_row = max_row + 1;

    // loop ignoring borders
    for (int r = start_row; r < end_row; r++) {
        for (int c = 1; c < columns - 1; c++) {
            int index = r * columns + c;

            int neighbors = 0;

            // row above
            neighbors += primary_board->cells[index - columns + 1];
            neighbors += primary_board->cells[index - columns];
            neighbors += primary_board->cells[index - columns - 1];

            // this row
            neighbors += primary_board->cells[index + 1];
            neighbors += primary_board->cells[index];
            neighbors += primary_board->cells[index - 1];

            // row below
            neighbors += primary_board->cells[index + columns + 1];
            neighbors += primary_board->cells[index + columns];
            neighbors += primary_board->cells[index + columns - 1];

            secondary_board->cells[index] = ((neighbors == 3) || (neighbors == 4 && primary_board->cells[index] == 1)) ? 1 : 0;
        }
    }
}

void* worker_function(void* args) {
    worker_param_t* params = (worker_param_t*)args;

    for (int step = 0; step < generations; step++) {
        process_step(params);

        cyclic_barrier_await(params->barrier);
    }

    return NULL;
}

void workers_init(pthread_t* workers, worker_param_t* parameters, cyclic_barrier_t* barrier, int number_of_threads) {
    for (int i = 0; i < number_of_threads; i++) {
        parameters[i].barrier = barrier;
        pthread_create(&workers[i], NULL, worker_function, &parameters[i]);
    }
}

void sync_board() {
    swap(primary_board, secondary_board);
}

void destroy_workers(pthread_t* workers, worker_param_t* parameters) {
    free(workers);
    free(parameters);
}

void simulate_life_parallel(int number_of_threads, LifeBoard* initial_state, int steps) {
    generations = steps;
    primary_board = initial_state;
    columns = initial_state->width;
    rows = initial_state->height;
    size = columns * rows;

    secondary_board = create_life_board(primary_board->width, primary_board->height);

    cyclic_barrier_t barrier;
    cyclic_barrier_init(&barrier, NULL, NULL, number_of_threads, sync_board);

    pthread_t* workers = (pthread_t*)malloc(number_of_threads * sizeof(pthread_t));
    worker_param_t* parameters = (worker_param_t*)malloc(number_of_threads * sizeof(worker_param_t));

    split_board(parameters, number_of_threads, rows, columns);

    workers_init(workers, parameters, &barrier, number_of_threads);

    for (int i = 0; i < number_of_threads; i++) {
        pthread_join(workers[i], NULL);
    }

    cyclic_barrier_destroy(&barrier);

    destroy_life_board(secondary_board);
    destroy_workers(workers, parameters);
}
