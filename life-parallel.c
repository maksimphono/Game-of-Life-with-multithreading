#include "life.h"
#include "cyclic_barrier.h"
#include "workers.h"
#include "split_board.h"

// reference: https://github.com/dogmelons/Threaded-Game-Of-Life

int rows = 0;
int columns = 0;
int generations = 0;
int size = 0;
LifeBoard* primary_board;
LifeBoard* secondary_board;

// processes a single step, used in the workers
void process_generation(worker_param_t* params) {
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

            // main condition for being alive on the next iteration
            secondary_board->cells[index] = ((neighbors == 3) || (neighbors == 4 && primary_board->cells[index] == 1)) ? 1 : 0;
        }
    }
}

void* worker_function(void* args) {
    worker_param_t* params = (worker_param_t*)args;

    for (int step = 0; step < generations; step++) {
        process_generation(params);

        cyclic_barrier_await(params->barrier);
    }

    return NULL;
}

void sync_board() {
    swap(primary_board, secondary_board);
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

void simulate_life_parallel(int number_of_threads, LifeBoard* initial_state, int steps) {
    if (initial_state->width - 2 < number_of_threads && initial_state->height - 2 < number_of_threads) {
        // the board is too small, thread creation will introduce overhead -> just do it with serial
        my_simulate_life_serial(initial_state, steps);
        return;
    }

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

    workers_init(
        workers, 
        parameters, 
        &barrier, 
        worker_function, 
        number_of_threads
    );

    for (int i = 0; i < number_of_threads; i++) {
        pthread_join(workers[i], NULL);
    }

    cyclic_barrier_destroy(&barrier);

    destroy_life_board(secondary_board);
    destroy_workers(workers, parameters);
}
