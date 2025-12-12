#include "life.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

typedef enum {False, True} boolean;

#define START 0
#define END 1

typedef struct {
    pthread_mutex_t m;
    pthread_cond_t  cv;
    int arrived;
    int total;
    int generation;
} barrier_t;

void barrier_wait(barrier_t *b) {
    pthread_mutex_lock(&b->m);
    int gen = b->generation;

    b->arrived++;
    if (b->arrived == b->total) {
        b->arrived = 0;
        b->generation++;
        pthread_cond_broadcast(&b->cv);
    } else {
        while (gen == b->generation)
            pthread_cond_wait(&b->cv, &b->m);
    }

    pthread_mutex_unlock(&b->m);
}

void barrier_init(barrier_t *b, int total_threads) {
    pthread_mutex_init(&b->m, NULL);
    pthread_cond_init(&b->cv, NULL);
    b->arrived = 0;
    b->total = total_threads;
    b->generation = 0;
}

barrier_t start_barrier;
barrier_t finish_barrier;


LifeCell create_next_cell(LifeBoard *state, int x, int y) {
    int live_neighbors = count_live_neighbors(state, x, y);
    LifeCell current_cell = at(state, x, y);
    return (live_neighbors == 3) || (live_neighbors == 4 && current_cell == 1) ? 1 : 0;
    
}

boolean is_border(LifeBoard* board, int x, int y) {
    return (x <= 0) || (x >= board->width - 1) || (y <= 0) || (y >= board->height - 1);
}

void calculate_slice(LifeBoard* next_state, LifeBoard* old_state, int range[2]) {
    // will be used by each thread to perform calculations on it's assigned slice
    int y = 0;
    int x = 0;
    LifeCell next_cell = 0;

    for (int i = range[START]; i < range[END]; i++) {
        y = i / old_state->width;
        x = i % old_state->width;
        if (is_border(old_state, x, y) == False) {
            next_cell = create_next_cell(old_state, x, y);
            set_at(next_state, x, y, next_cell);
        }
    }
}

typedef struct {
    LifeBoard* next_state;
    LifeBoard* old_state;
    int range[2];
} worker_arguments_t;

//pthread_barrier_t start_barrier;

//pthread_barrier_waitksks(start_barrier);

void* worker_function(void* args) {
    // this function is a start for each thread, each thread will run infinite loop here
    worker_arguments_t* arguments = (worker_arguments_t*)args;
    LifeBoard* next_state = arguments->next_state;
    LifeBoard* old_state = arguments->old_state;
    int range[2] = {arguments->range[0], arguments->range[1]};

forever: // this loop will run forever, barrier will control it, when basically on each step of the game, barrier will wait for all threads to come and wait for all of them to finish their respective slice of the job
    // TODO: use 2 barriers here to wait for all threads to get ready first, then for all threads to finish

    // barrier 1
    barrier_wait(&start_barrier);

    calculate_slice(next_state, old_state, range);

    // barrier 2
    barrier_wait(&finish_barrier);

    goto forever;
}

void init_workers(LifeBoard* next_state, LifeBoard *state, pthread_t workers[], int workers_num) {
    int board_size = state->width * state->height;
    int range_length = board_size / workers_num;
    int range[2] = {};
    worker_arguments_t* args_arr = (worker_arguments_t*)malloc(sizeof(worker_arguments_t) * workers_num);
    //void* args = malloc(sizeof(worker_arguments_t));
    int i = 0;
    for (; i < workers_num - 1; i++) {
        // TODO: calculate start of the range, respecting the borders (borders must be 0 always)
        range[START] = i * range_length;
        range[END] = range[START] + range_length;
        worker_arguments_t args = {next_state, state, range};
        args_arr[i] = args;
        pthread_create(workers[i], NULL, worker_function, (void*)(args_arr + i));
    }
    range[START] = i * range_length;
    range[END] = board_size;
    worker_arguments_t args = {next_state, state, range};
    pthread_create(workers[workers_num - 1], NULL, worker_function, (void*)(&args));
}

void simulate_life_parallel(int threads, LifeBoard *state, int steps) {
    // TODO: implement this with multithreading
    if (steps == 0) return;
    pthread_t workers[threads];
    LifeBoard *next_state = create_life_board(state->width, state->height);
    if (next_state == NULL) {
        fprintf(stderr, "Failed to allocate memory for next state.\n");
        return;
    }

    init_workers(next_state, state, workers, threads);
    barrier_init(&start_barrier, threads + 1);
    barrier_init(&finish_barrier, threads + 1);

    for (int step = 0; step < steps; step++) {
        // TODO: perform main logic of synchronization with barriers here
        
        // barrier 1
        barrier_wait(&start_barrier);

        // barrier 2
        barrier_wait(&finish_barrier);
        
        swap(next_state, state);
    }
}
