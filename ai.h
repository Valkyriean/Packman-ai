// Modified by Jiachen Li (1068299)

#ifndef __AI__
#define __AI__

#include <stdint.h>
#include <unistd.h>
#include "node.h"
#include "priority_queue.h"


void initialize_ai();

move_t get_next_move( state_t init_state, int budget, propagation_t propagation,
        char* stats, int* total_max_depth, int* total_generated,
        int* total_expanded, clock_t *process_time);

#endif