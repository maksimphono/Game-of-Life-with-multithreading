#include "life.h"
#include <pthread.h>
#include <semaphore.h>

typedef enum {False, True} boolean;
typedef enum {Horizontal, Vertical} axis_t;

#define WORKERS_AND_MAIN(worker_num) (worker_num + 1)

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
} worker_state_t;

/*
* Special abstract data staructure, that represents list of worker threads and 
* their states for easy access and manipulation
*/
typedef struct {
    pthread_t* workers; // threads themselves
    worker_state_t* states; // workers states (will be passed to werker function as arguments)
    int length;
    boolean terminated;
    pthread_mutex_t term_mutex;
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

void init_gate(gate_t *b, int total_threads, char name) {
    pthread_mutex_init(&b->m, NULL);
    pthread_cond_init(&b->cv, NULL);
    b->arrived = 0;
    b->total = total_threads;
    b->generation = 0;
    b->name = name;
}

void destroy_gate(gate_t* gate){
    pthread_cond_destroy(&gate->cv);
    pthread_mutex_destroy(&gate->m);
}

gate_t start_gate;
gate_t finish_gate;
worker_list_t workers_list;
axis_t processing_axis = Horizontal;

LifeCell create_next_cell(LifeBoard *state, int x, int y) {
    int live_neighbors = count_live_neighbors(state, x, y);
    LifeCell current_cell = at(state, x, y);
    return (live_neighbors == 3) || (live_neighbors == 4 && current_cell == 1) ? 1 : 0;
    
}

void calculate_slice(LifeBoard* next_board, LifeBoard* old_board, int start_row, int end_row) {
    // will be used by each thread to perform calculations on it's assigned slice
    LifeCell next_cell = 0;

    switch (processing_axis) {
    case Horizontal:
        for (int y = start_row; y < end_row; y++) {
            for (int x = 1; x < old_board->width - 1; x++) {
                next_cell = create_next_cell(old_board, x, y);
                set_at(next_board, x, y, next_cell);
            }
        }
        return;
    case Vertical:
        for (int x = start_row; x < end_row; x++) {
            for (int y = 1; y < old_board->height - 1; y++) {
                next_cell = create_next_cell(old_board, x, y);
                set_at(next_board, x, y, next_cell);
            }
        }
        return;
    }
}

void* worker_function(void* state) {
    // this function is a start for each thread, each thread will run infinite loop here
    worker_state_t* worker_state = (worker_state_t*)state;
    LifeBoard* next_board = worker_state->next_board;
    LifeBoard* old_board = worker_state->old_board;

    // this loop will run forever, gate will control it, 
    // when basically on each step of the game, 
    // gate will wait for all threads to come and wait 
    // for all of them to finish their respective slice of the job
    while (1) { 

        gate_wait(&start_gate);

        if (workers_list.terminated) {
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
    worker_state_t worker_state = {
        next_board, 
        state, 
        start_row, 
        end_row,
    };
    worker_list->states[worker_list->length] = worker_state;
    pthread_create(&worker_list->workers[worker_list->length], NULL, worker_function, (void*)(worker_list->states + worker_list->length));
    worker_list->length += 1;
    return worker_state;
}

void destroy_workers(worker_list_t* worker_list) {
    free(worker_list->workers);
    free(worker_list->states);
}

void terminate_workers(worker_list_t* worker_list) {
    pthread_mutex_lock(&worker_list->term_mutex);
    worker_list->terminated = True;
    pthread_mutex_unlock(&worker_list->term_mutex);
}

void join_workers(worker_list_t* worker_list) {
    for (int i = 0; i < worker_list->length; i++) {
        pthread_join(worker_list->workers[i], NULL);
    }
}


// initializes worker list
void init_workers(worker_list_t* worker_list, int workers_num) {
    worker_list->workers = (pthread_t*)malloc(sizeof(pthread_t) * workers_num);
    worker_list->states = (worker_state_t*)malloc(sizeof(worker_state_t) * workers_num);
    worker_list->length = 0;
    worker_list->terminated = False;
}

// Assigns slices to workers and creates threads
// returns minimum slice size assigned to each worker
int distribute_work(worker_list_t* worker_list, LifeBoard* next_board, LifeBoard *state, int workers_num) {
    int rows_number = (state->height - 2);
    int columns_number = (state->width - 2);
    int rows_per_worker = 1;
    int reminder = 0;

    if (rows_number >= WORKERS_AND_MAIN(workers_num) && (rows_number % WORKERS_AND_MAIN(workers_num) == 0 || rows_number > columns_number)){
        // process horizontally (each worker processes set of rows)
        processing_axis = Horizontal;
        rows_per_worker = rows_number / WORKERS_AND_MAIN(workers_num);
        reminder = rows_number % WORKERS_AND_MAIN(workers_num);
    } else 
    if (columns_number >= WORKERS_AND_MAIN(workers_num) && (columns_number % WORKERS_AND_MAIN(workers_num) == 0 || columns_number > rows_number)){
        // process vertically (each worker processes set of columns)
        processing_axis = Vertical;
        rows_per_worker = columns_number / WORKERS_AND_MAIN(workers_num);
        reminder = columns_number % WORKERS_AND_MAIN(workers_num);
    }

    int i = 0;
    int last_row = 1 + rows_per_worker; // the first slice will be managed by main

    for (; i < workers_num; i++) {
        if (i < reminder) {
            create_worker(
                worker_list, 
                next_board, 
                state,
                last_row,
                last_row + rows_per_worker + 1
            );
            last_row += rows_per_worker + 1;
        } else {
            create_worker(
                worker_list, 
                next_board, 
                state, 
                last_row, 
                last_row + rows_per_worker
            );
            last_row += rows_per_worker;
        }
    }
    return rows_per_worker;
}

void my_simulate_life_serial(LifeBoard *state, int steps) {
    if (steps == 0) return;
    LifeBoard *next_state = create_life_board(state->width, state->height);
    if (next_state == NULL) {
        fprintf(stderr, "Failed to allocate memory for next state.\n");
        return;
    }
    for (int step = 0; step < steps; step++) {
        for (int y = 1; y < state->height - 1; y++) {
            for (int x = 1; x < state->width - 1; x++) {
                int live_neighbors = count_live_neighbors(state, x, y);
                LifeCell current_cell = at(state, x, y);
                LifeCell new_cell = (live_neighbors == 3) || (live_neighbors == 4 && current_cell == 1) ? 1 : 0;
                set_at(next_state, x, y, new_cell);
            }
        }
        swap(state, next_state);
    }
    destroy_life_board(next_state);
}

void simulate_life_parallel(int threads, LifeBoard *state, int steps) {
    // TODO: implement this with multithreading
    if (steps == 0) return;
    if (state->width - 2 < threads && state->height - 2 < threads) {
        // the board is too small, thread creation will introduce overhead -> just do it with serial
        my_simulate_life_serial(state, steps);
        return;
    }

    int step = 0;
    LifeBoard *next_board = create_life_board(state->width, state->height);
    if (next_board == NULL) {
        fprintf(stderr, "Failed to allocate memory for next state.\n");
        return;
    }

    init_gate(&start_gate, threads + 1, 'S');
    init_gate(&finish_gate, threads + 1, 'F');
    init_workers(&workers_list, threads);
    int rows_for_main = distribute_work(&workers_list, next_board, state, threads);
    worker_state_t main_worker_state = {next_board, state, 1, 1 + rows_for_main};

    while (step < steps) {

        // gate 1
        gate_wait(&start_gate);

        calculate_slice(
            next_board, 
            state, 
            main_worker_state.start_row, 
            main_worker_state.end_row
        );

        // gate 2
        gate_wait(&finish_gate);

        swap(next_board, state);
        step++;
    }

    terminate_workers(&workers_list);

    // Let workers wake up from start_gate
    gate_wait(&start_gate);

    join_workers(&workers_list);

    destroy_gate(&start_gate);
    destroy_gate(&finish_gate);
    destroy_workers(&workers_list);
    destroy_life_board(next_board);
}
