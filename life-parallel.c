#include "life.h"
#include <pthread.h>
#include <semaphore.h>

typedef enum {DEAD, ALIVE} Cell_state_t;

#define START 0
#define END 1

LifeCell create_next_cell(LifeBoard *state, int x, int y) {
    int live_neighbors = count_live_neighbors(state, x, y);
    LifeCell current_cell = at(state, x, y);
    return (live_neighbors == 3) || (live_neighbors == 4 && current_cell == 1) ? 1 : 0;
    
}

void calculate_slice(LifeBoard* next_state, LifeBoard* old_state, int range[2]) {
    // will be used by each thread to perform calculations on it's assigned slice
    int y = 0;
    int x = 0;
    LifeCell next_cell = 0;

    for (int i = range[START]; i < range[END]; i++) {
        y = i / old_state->width;
        x = i % old_state->width;
        next_cell = create_next_cell(old_state, x, y);
        set_at(next_state, x, y, next_cell);
    }
}

void* worker_function(LifeBoard* next_state, LifeBoard *state, int range[2]) {
    // this function is a start for each thread, each thread will run infinite loop here

forever: // this loop will run forever, barrier will control it, when basically on each step of the game, barrier will wait for all threads to come and wait for all of them to finish their respective slice of the job
    

    goto forever;
}

void simulate_life_parallel(int threads, LifeBoard *state, int steps) {
    // TODO: implement this with multithreading
    pthread_t workers[threads];
    if (steps == 0) return;
    LifeBoard *next_state = create_life_board(state->width, state->height);
    if (next_state == NULL) {
        fprintf(stderr, "Failed to allocate memory for next state.\n");
        return;
    }

    for (int step = 0; step < steps; step++) {

    }
}
