# Threaded Game of life

* Implementation inspired by [Threaded-Game-Of-Life by dogmelons](https://github.com/dogmelons/Threaded-Game-Of-Life)

## Introduction

This project is my implementation of Game of Life with multithreading. Initially is my university project for OS class. This implementation utilizes 2 GoL boards, that are swapped on each iteration, multiple threads (can be specified) and cyclic barriers to sinchronize the threads.

The board is bounded, the boarders are always dead, width, height and initial state can be specified in the input file.

* Rules for this specific implementation are:
1. If the cell has exactly 3 alive neighbors (including itself) - it will live to the next generation
2. If the cell is alive and has 4 alive neighbors and is alive - it will live to the next generation
3. Otherwise cell dies in the generation

## Core Features and Their Implementation

### Multithreading Framework

The core of the parallelization relies on pthreads to create and manage worker threads. Each worker is assigned a portion of the board to process. Implementation involves initializing an array of thread handles and parameter structures, where each parameter encapsulates the starting row, column, and length of the slice for that thread. Threads are created in a loop, passing these parameters to a worker function. The main thread waits for all workers to complete via joins after simulation steps. Cleanup frees allocated resources for threads and parameters.

### Cyclic Barrier Synchronization

Synchronization is implemented through a custom cyclic barrier, which acts as a reusable synchronization point for threads. The barrier is initialized with the number of workers, a mutex for mutual exclusion, and a condition variable for waiting. Each thread awaits the barrier after processing its slice, decrementing an outstanding count. When the count reaches zero, a barrier function is invoked to synchronize the board state, and the condition is broadcast to release waiting threads. This implementation allows all threads to reach the barrier before proceeding to the next generation, ensuring consistent board updates. The barrier is destroyed at the end, releasing its resources.

### Workload Distribution and Slicing

Workload distribution uses a board splitter function (`split_board`) to divide the grid among threads. The splitter handles cases where threads are fewer than rows (assigning row blocks), more than cells (assigning single cells or zero-length for extras), or in between (mixing row and cell assignments). Parameters are populated with start row, start column, and length in row-major order. To exclude borders, the splitter operates on the full board dimensions, but processing logic clips to inner regions (rows 1 to height-2, columns 1 to width-2). This ensures no time is wasted on border iterations, with threads computing only valid inner cells. The approach balances load by distributing evenly, with extras handling remainders, promoting fair computation across threads.

### Generation Processing Logic

Each worker processes its assigned slice over multiple generations in a loop. For each step, it computes the next state for cells in its range, counting neighbors from the primary board and updating the secondary board. Neighbor counts exclude the cell itself and handle edge cases implicitly by inner clipping. Borders are enforced dead by skipping computations and setting to zero during synchronization. This per-thread processing minimizes data races, as writes are to a separate buffer.

### Board Synchronization and Swapping

After all threads complete a generation (via barrier), the synchronization function swaps the secondary and primary board and enforces dead borders by zeroing perimeter cells. This double-buffering prevents read-write conflicts, ensuring threads always read from a consistent state. The barrier's callback mechanism integrates this seamlessly, executing only when all participants arrive.

### Fallback to Serial Execution

For efficiency, a check determines if the board is too small (inner dimensions less than threads), falling back to a serial simulation. The serial version loops over inner cells, computes next states, and swaps buffers without threading. This avoids overhead from thread creation and synchronization on tiny boards, where parallelism yields diminishing returns.

### Conclusion

The implementation effectively parallelizes GoL using pthreads and cyclic barriers, focusing on inner cell computations while enforcing dead borders. Core features like workload slicing, synchronization, and fallback ensure scalability and efficiency. The logic prioritizes simplicity and correctness, adapting repository ideas to bounded grids. Potential improvements could include dynamic axis selection or work-stealing for better balance, but the current design suits moderate-sized boards well. Overall, it demonstrates sound multithreading principles in C.

