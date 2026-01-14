#ifndef SPLIT_BOARD_H_
#define SPLIT_BOARD_H_

#include "workers.h"

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

#endif