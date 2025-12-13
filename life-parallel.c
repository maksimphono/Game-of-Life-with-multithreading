#include "life.h"
#include <pthread.h>
#include <semaphore.h>
#include <string.h>

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
    LifeBoard* next_board;
    LifeBoard* old_board;
    int range[2];
    boolean terminated;
} worker_state_t;

typedef struct {
    pthread_t* workers; // threads themselves
    worker_state_t* states; // workers states (will be passed to werker function as arguments)
    int length;
} worker_list_t;

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
worker_list_t workers;

LifeCell create_next_cell(LifeBoard *state, int x, int y) {
    int live_neighbors = count_live_neighbors(state, x, y);
    LifeCell current_cell = at(state, x, y);
    return (live_neighbors == 3) || (live_neighbors == 4 && current_cell == 1) ? 1 : 0;
    
}

boolean is_border(LifeBoard* board, int x, int y) {
    return (x <= 0) || (x >= board->width - 1) || (y <= 0) || (y >= board->height - 1);
}

void calculate_slice(LifeBoard* next_board, LifeBoard* old_board, int range[2]) {
    // will be used by each thread to perform calculations on it's assigned slice
    int y = 0;
    int x = 0;
    LifeCell next_cell = 0;

    for (int i = range[START]; i < range[END]; i++) {
        y = i / old_board->width;
        x = i % old_board->width;
        if (is_border(old_board, x, y) == False) {
            next_cell = create_next_cell(old_board, x, y);
            set_at(next_board, x, y, next_cell);
        }
    }
}

void* worker_function(void* state) {
    // this function is a start for each thread, each thread will run infinite loop here
    worker_state_t* worker_state = (worker_state_t*)state;
    LifeBoard* next_board = worker_state->next_board;
    LifeBoard* old_board = worker_state->old_board;

    while (1) { // this loop will run forever, barrier will control it, when basically on each step of the game, barrier will wait for all threads to come and wait for all of them to finish their respective slice of the job
        // TODO: use 2 barriers here to wait for all threads to get ready first, then for all threads to finish

        // barrier 1
        barrier_wait(&start_barrier);

        if (worker_state->terminated) {
            break; // stop worker
        }

        calculate_slice(next_board, old_board, worker_state->range);

        // barrier 2
        barrier_wait(&finish_barrier);
    }
    return NULL;
}

worker_state_t create_worker(worker_list_t* worker_list, LifeBoard* next_board, LifeBoard *state, int range_start, int range_end){
    worker_state_t worker_state = {next_board, state, {range_start, range_end}, False};
    worker_list->states[worker_list->length] = worker_state;
    //printf("size: %d", sizeof(worker_list->workers));
    pthread_create(&worker_list->workers[worker_list->length], NULL, worker_function, (void*)(worker_list->states + worker_list->length));
    worker_list->length += 1;
    return worker_state;
}

void terminate_workers(worker_list_t worker_list) {
    for (int i = 0; i < worker_list.length; i++)
        worker_list.states[i].terminated = True;
}

void join_workers(worker_list_t worker_list) {
    for (int i = 0; i < worker_list.length; i++) {
        pthread_join(worker_list.workers[i], NULL);
    }
}

void init_workers(worker_list_t* worker_list, LifeBoard* next_board, LifeBoard *state, int workers_num) {
    int board_size = state->width * state->height;
    int range_length = board_size / workers_num;
    int i = 0;

    worker_list->workers = (pthread_t*)malloc(sizeof(pthread_t) * workers_num);
    worker_list->states = (worker_state_t*)malloc(sizeof(worker_state_t) * workers_num);
    worker_list->length = 0;

    for (; i < workers_num - 1; i++) {
        // TODO: calculate start of the range, respecting the borders (borders must be 0 always)
        create_worker(worker_list, next_board, state, i * range_length, i * range_length + range_length);
    }
    create_worker(worker_list, next_board, state, i * range_length, board_size);
}

void simulate_life_parallel(int threads, LifeBoard *state, int steps) {
    // TODO: implement this with multithreading
    if (steps == 0) return;
    int step = 0;
    LifeBoard *next_board = create_life_board(state->width, state->height);
    if (next_board == NULL) {
        fprintf(stderr, "Failed to allocate memory for next state.\n");
        return;
    }

    //pthread_mutex_init(&term_lock, NULL);
    barrier_init(&start_barrier, threads + 1, 'S');
    barrier_init(&finish_barrier, threads + 1, 'F');
    init_workers(&workers, next_board, state, threads);

    while (step < steps) {
        // TODO: perform main logic of synchronization with barriers here

        // barrier 1
        barrier_wait(&start_barrier);

        // barrier 2
        barrier_wait(&finish_barrier);

        swap(next_board, state);
        step++;
    }

    terminate_workers(workers);

    // Let workers wake up from start_barrier
    barrier_wait(&start_barrier);

    // Wait for workers to exit their loop
    //barrier_wait(&finish_barrier);

    // Now safely join threads
    join_workers(workers);
}
