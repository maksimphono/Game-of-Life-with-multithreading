#include "life.h"
#include <pthread.h>
#include <semaphore.h>

typedef enum {False, True} boolean;

#define START 0
#define END 1

typedef struct {
    pthread_mutex_t m;
    pthread_cond_t  cv;
    int arrived;
    int total;
    int generation;
    char name;
} barrier_t;

typedef struct {
    LifeBoard* next_state;
    LifeBoard* old_state;
    int range[2];
    boolean terminated;
} worker_arguments_t;

void barrier_wait(barrier_t *b) {
    pthread_mutex_lock(&b->m);
    int gen = b->generation;

    b->arrived++;
    if (b->arrived == b->total) {
        //printf("Barrier %c arrived: %d/%d, get to work\n", b->name, b->arrived, b->total);
        //fflush(stdout);
        b->arrived = 0;
        b->generation++;
        pthread_cond_broadcast(&b->cv);
    } else {
        //printf("Barrier %c arrived: %d/%d\n", b->name, b->arrived, b->total);
        //fflush(stdout);
        while (gen == b->generation)
            pthread_cond_wait(&b->cv, &b->m);
    }

    pthread_mutex_unlock(&b->m);
}

void barrier_init(barrier_t *b, int total_threads, char name) {
    pthread_mutex_init(&b->m, NULL);
    pthread_cond_init(&b->cv, NULL);
    b->arrived = 0;
    b->total = total_threads;
    b->generation = 0;
    b->name = name;
}

barrier_t start_barrier;
barrier_t finish_barrier;
boolean terminated = False;


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

void* worker_function(void* args) {
    // this function is a start for each thread, each thread will run infinite loop here
    worker_arguments_t* arguments = (worker_arguments_t*)args;
    LifeBoard* next_state = arguments->next_state;
    LifeBoard* old_state = arguments->old_state;
    int range[2] = {arguments->range[0], arguments->range[1]};

    while (1) { // this loop will run forever, barrier will control it, when basically on each step of the game, barrier will wait for all threads to come and wait for all of them to finish their respective slice of the job
        // TODO: use 2 barriers here to wait for all threads to get ready first, then for all threads to finish

        // barrier 1
        barrier_wait(&start_barrier);

        if (arguments->terminated) {
            break; // stop worker
        }

        calculate_slice(next_state, old_state, range);

        // barrier 2
        barrier_wait(&finish_barrier);
    }
    return NULL;
}

worker_arguments_t* args_arr;

worker_arguments_t create_worker_arguments(LifeBoard* next_state, LifeBoard *state, int* range) {
    worker_arguments_t args = {next_state, state, {range[0], range[1]}, False};
    return args;
}



void init_workers(LifeBoard* next_state, LifeBoard *state, pthread_t workers[], int workers_num) {
    int board_size = state->width * state->height;
    int range_length = board_size / workers_num;
    int range[2] = {};
    args_arr = (worker_arguments_t*)malloc(sizeof(worker_arguments_t) * workers_num);
    int i = 0;
    for (; i < workers_num - 1; i++) {
        // TODO: calculate start of the range, respecting the borders (borders must be 0 always)
        range[START] = i * range_length;
        range[END] = range[START] + range_length;
        worker_arguments_t args = create_worker_arguments(next_state, state, range);
        args_arr[i] = args;
        pthread_create(&workers[i], NULL, worker_function, (void*)(args_arr + i));
    }
    range[START] = i * range_length;
    range[END] = board_size;
    worker_arguments_t args = create_worker_arguments(next_state, state, range);;
    args_arr[i] = args;
    pthread_create(&workers[workers_num - 1], NULL, worker_function, (void*)(args_arr + i));
}

void simulate_life_parallel(int threads, LifeBoard *state, int steps) {
    // TODO: implement this with multithreading
    if (steps == 0) return;
    int step = 0;
    LifeBoard *next_state = create_life_board(state->width, state->height);
    pthread_t workers[threads];
    if (next_state == NULL) {
        fprintf(stderr, "Failed to allocate memory for next state.\n");
        return;
    }

    //pthread_mutex_init(&term_lock, NULL);
    barrier_init(&start_barrier, threads + 1, 'S');
    barrier_init(&finish_barrier, threads + 1, 'F');
    init_workers(next_state, state, workers, threads);

    while (step < steps) {
        // TODO: perform main logic of synchronization with barriers here

        // barrier 1
        barrier_wait(&start_barrier);

        // barrier 2
        barrier_wait(&finish_barrier);

        swap(next_state, state);
        step++;
    }

    for (int i = 0; i < threads; i++)
        args_arr[i].terminated = True;

    // Let workers wake up from start_barrier
    barrier_wait(&start_barrier);

    // Wait for workers to exit their loop
    //barrier_wait(&finish_barrier);

    // Now safely join threads
    for (int i = 0; i < threads; i++) {
        pthread_join(workers[i], NULL);
    }
}
