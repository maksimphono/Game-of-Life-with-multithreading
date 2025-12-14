# Changes 

*** Dec 15 ***

*** Changes to the Lab 4, best score 70.00 ***

## Terminology

* `worker` - wrapper for a thread, created with specific `worker_function` function and includes specific state, that is passed as argument to this function
* `worker list` - collection of workers and their respective states, as well as the state of this collection itself like `terminated`, etc. Used to manipulate all workers easily

## Basic principal
Implemented parallel computation of "Game of life" using multiple threads. Created N threads and 2 "gates": start gate and finish gate, which are responsible for synchronization, when thread arrive at the gate, it waits for all other threads to arrive as well. Thus on each iteration all threads first are stopping at the starting gate, then performing calculations and waiting on the finish gate for all other threads to finish calculations as well, then main thread performs board swap while other threads wait for it on the starting gate, ready to start next iteration.

##
## Main functional
##

### Board calculations
    Each worker is assign it's slice (set of rows) of the board, worker processes only single slice, assigned to it. The worker doesn't perform calculations on the borders, only on the board itself. 

### Load distribution
    Board slices are distributed as evenly as possible across workers. Normally, workers will process the board horizontally (so each worker works on it's own set of rows), but if processing the board vertically is more efficient (for example for very wide boards) - workers will switch to vertical board processing. If the board is too small (thread creation will introduce overhead) - serial approach is used instead.

### Termination
    Each worker runs infinite loop, where it waits on 2 gates and perform calculations, when `termination` flag becomes set on the worker list, each workers detect it in it's loop and exits. After that main thread will join all workers.
    
    
##
## Important fucntions
##

* `calculate_slice()`
Is used by each worker to process it's assigned slice, implementation is very similar to the serial approach

* `worker_function()`
Main function of each worker, here it's logic with waiting on 2 gates and slice pocessing is realized

* `init_workers()`
Creates all workers including underlying threads and their states, assign slices to workers

* `simulate_life_parallel()`
Main logic
