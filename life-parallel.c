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
} gate_t;

typedef struct {
    LifeBoard* next_board;
    LifeBoard* old_board;
    int start_row;
    int end_row;
    boolean terminated;
} worker_state_t;

/*
* Special abstract data staructure, that represents list of worker threads and 
* their states for easy access and manipulation
*/
typedef struct {
    pthread_t* workers; // threads themselves
    worker_state_t* states; // workers states (will be passed to werker function as arguments)
    int length;
} worker_list_t;

void gate_wait(gate_t *b) {
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

void gate_init(gate_t *b, int total_threads, char name) {
    pthread_mutex_init(&b->m, NULL);
    pthread_cond_init(&b->cv, NULL);
    b->arrived = 0;
    b->total = total_threads;
    b->generation = 0;
    b->name = name;
}

gate_t start_gate;
gate_t finish_gate;
worker_list_t workers_list;

LifeCell create_next_cell(LifeBoard *state, int x, int y) {
    int live_neighbors = count_live_neighbors(state, x, y);
    LifeCell current_cell = at(state, x, y);
    return (live_neighbors == 3) || (live_neighbors == 4 && current_cell == 1) ? 1 : 0;
    
}

boolean is_border(LifeBoard* board, int x, int y) {
    return (x <= 0) || (x >= board->width - 1) || (y <= 0) || (y >= board->height - 1);
}

void calculate_slice(LifeBoard* next_board, LifeBoard* old_board, int start_row, int end_row) {
    // will be used by each thread to perform calculations on it's assigned slice
    LifeCell next_cell = 0;

    for (int y = start_row; y < end_row; y++) {
        for (int x = 1; x < old_board->width - 1; x++) {
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

    while (1) { // this loop will run forever, gate will control it, when basically on each step of the game, gate will wait for all threads to come and wait for all of them to finish their respective slice of the job
        // TODO: use 2 gates here to wait for all threads to get ready first, then for all threads to finish

        gate_wait(&start_gate);

        if (worker_state->terminated) {
            break; // stop worker
        }

        calculate_slice(
            next_board, 
            old_board, 
            worker_state->start_row, 
            worker_state->end_row
        );

        gate_wait(&finish_gate);
    }
    return NULL;
}

worker_state_t create_worker(worker_list_t* worker_list, LifeBoard* next_board, LifeBoard *state, int start_row, int end_row){
    worker_state_t worker_state = {next_board, state, start_row, end_row, False};
    worker_list->states[worker_list->length] = worker_state;
    pthread_create(&worker_list->workers[worker_list->length], NULL, worker_function, (void*)(worker_list->states + worker_list->length));
    worker_list->length += 1;
    return worker_state;
}

void terminate_workers(worker_list_t* worker_list) {
    for (int i = 0; i < worker_list->length; i++)
        worker_list->states[i].terminated = True;
}

void join_workers(worker_list_t* worker_list) {
    for (int i = 0; i < worker_list->length; i++) {
        pthread_join(worker_list->workers[i], NULL);
    }
}

void init_workers(worker_list_t* worker_list, LifeBoard* next_board, LifeBoard *state, int workers_num) {
    int rows_per_worker = (state->height - 2) / workers_num;

    worker_list->workers = (pthread_t*)malloc(sizeof(pthread_t) * workers_num);
    worker_list->states = (worker_state_t*)malloc(sizeof(worker_state_t) * workers_num);
    worker_list->length = 0;

    int i = 0;
    for (; i < workers_num - 1; i++) {
        // TODO: calculate start of the range, respecting the borders (borders must be 0 always)
        create_worker(
            worker_list, 
            next_board, 
            state, 
            1 + i * rows_per_worker, 
            1 + i * rows_per_worker + rows_per_worker
        );
    }
    // last worker is responsible for the last slice, that could be bigger than others
    create_worker(
        worker_list, 
        next_board, 
        state, 
        1 + i * rows_per_worker, 
        state->height - 1
    );
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
    gate_init(&start_gate, threads + 1, 'S');
    gate_init(&finish_gate, threads + 1, 'F');
    init_workers(&workers_list, next_board, state, threads);

    while (step < steps) {
        // TODO: perform main logic of synchronization with gates here

        // gate 1
        gate_wait(&start_gate);

        // gate 2
        gate_wait(&finish_gate);

        swap(next_board, state);
        step++;
    }

    terminate_workers(&workers_list);

    // Let workers wake up from start_gate
    gate_wait(&start_gate);

    join_workers(&workers_list);
}
