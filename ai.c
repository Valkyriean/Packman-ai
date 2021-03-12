// Modified by Jiachen Li (1068299)

#include <time.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include "ai.h"
#include "utils.h"
#include "priority_queue.h"

struct heap h;

float get_reward( node_t* n );
bool lost_life(node_t* n);
// int last_move[2];
// float last_score[2];

/**
 * Function called by pacman.c
*/
void initialize_ai(){
	// last_move[0] = -1;
	// last_move[1] = -1;
	// last_score[0] = INT_MIN;
	// last_score[1] = INT_MIN;
	heap_init(&h);
}

/**
 * function to copy a src into a dst state
*/
void copy_state(state_t* dst, state_t* src){
	//Location of Ghosts and Pacman
	memcpy( dst->Loc, src->Loc, 5*2*sizeof(int) );

    //Direction of Ghosts and Pacman
	memcpy( dst->Dir, src->Dir, 5*2*sizeof(int) );

    //Default location in case Pacman/Ghosts die
	memcpy( dst->StartingPoints, src->StartingPoints, 5*2*sizeof(int) );

    //Check for invincibility
    dst->Invincible = src->Invincible;

    //Number of pellets left in level
    dst->Food = src->Food;

    //Main level array
	memcpy( dst->Level, src->Level, 29*28*sizeof(int) );

    //What level number are we on?
    dst->LevelNumber = src->LevelNumber;

    //Keep track of how many points to give for eating ghosts
    dst->GhostsInARow = src->GhostsInARow;

    //How long left for invincibility
    dst->tleft = src->tleft;

    //Initial points
    dst->Points = src->Points;

    //Remiaining Lives
    dst->Lives = src->Lives;

}

node_t* create_init_node( state_t* init_state ){
	node_t * new_n = (node_t *) malloc(sizeof(node_t));
	new_n->parent = NULL;
	new_n->priority = 0;
	new_n->depth = 0;
	new_n->num_childs = 0;
	copy_state(&(new_n->state), init_state);
	new_n->acc_reward =  get_reward( new_n );
	return new_n;

}

float heuristic( node_t* n ){
	float h = 0;
	// Pac-Man has eaten a fruit and becomes invincible in that state
	if (n->state.Invincible == 1) {
		h += 10;
	}

	// A life has been lost in that state
	if (lost_life(n) == true) {
		h -= 10;
	}

	// The game is over
	if (n->state.Lives == -1) {
		h -= 100;
	}

	return h;
}

float get_reward (node_t* n) {
	float reward = 0;
	reward += heuristic(n);
	reward += n->state.Points;
	// Take the diffence from parent score
	if (n->parent != NULL) {
		reward -= n->parent->state.Points;
	}
	// Depth discount
	float discount = pow(0.99, n->depth);
	return discount * reward;
}

/**
 * Apply an action to node n and return a new node resulting from executing the action
*/
bool applyAction (node_t* n, node_t* new_node, move_t action) {
	bool changed_dir = false;
	// Like node node to its parent
	new_node->parent = n;
	copy_state(&(new_node->state), &(n->state));
	// Take the move
    changed_dir = execute_move_t( &((new_node)->state), action );

	new_node->depth = n->depth + 1;
	new_node->priority = -1 * new_node->depth;
	new_node->acc_reward=n->acc_reward + get_reward(new_node);
	n->num_childs++;
	new_node->num_childs=0;
	// Update move to recored which first move was take for this node
	if (n->depth == 0) {
		// If this is the first move node
		new_node->move = action;
	} else {
		new_node->move = n->move;
	}
	return changed_dir;
}


/**
 * Find best action by building all possible paths up to budget
 * and back propagate using either max or avg
 */

move_t get_next_move(state_t init_state, int budget, propagation_t propagation,
		char* stats, int* total_max_depth, int* total_generated,
		int* total_expanded, clock_t* process_time) {
	clock_t t = clock();
	move_t best_action;
	float best_action_score[4];
	for(unsigned i = 0; i < 4; i++)
	    best_action_score[i] = INT_MIN;
	float total_action_score[4];
	int num_childs[4];
	for (int i = 0; i < 4; i++) {
		num_childs[i] = 0;
		total_action_score[i] = 0;
	}
	unsigned generated_nodes = 0;
	unsigned expanded_nodes = 0;
	unsigned max_depth = 0;
	//Add the initial node
	node_t* n = create_init_node( &init_state );
	node_t* new_node;
	node_t* root_node = n;
	//Use the max heap API provided in priority_queue.h
	heap_push(&h,n);
	//Make space for array of nodes used for free all nodes
	node_t** explored = (node_t**)malloc(sizeof(node_t*) * (budget*10));
	assert(explored != NULL);
	//Keep running before reach budget
	while(expanded_nodes < budget && h.count > 0){
		//pop node out from pq
		n = heap_delete(&h);
		explored[expanded_nodes] = n;
		expanded_nodes++;
		//Try all for moves form current node
		for (move_t move = left; move <= down; move++) {
			//If this move would not move into the wall
			if (applicable(n->state, move)) {
				new_node = malloc(sizeof(node_t));
				assert(new_node != NULL);
				// Simulate the action done to current node
				applyAction(n, new_node, move);
				generated_nodes++;
				// Check if this is the max score for its first move
				if (propagation == max && new_node->acc_reward >
					best_action_score[new_node->move]) {
					best_action_score[new_node->move] = new_node->acc_reward;
				}
				// Update the max depth
				if (new_node->depth > max_depth) {
					max_depth = new_node->depth;
				}
				if (lost_life(new_node)){
					// recored the score of node before delete if choosing
					// average propagation
					if (propagation == avg) {
						num_childs[new_node->move]+=1;
						total_action_score[new_node->move] +=
						new_node->acc_reward;
					}
					free(new_node);
				} else {
					heap_push(&h,new_node);
				}
			}
		}
	}

	// Calculate the average score of all nodes after same first move
	if(propagation == avg){
		for (int i = 1; i < expanded_nodes; i++) {
			n = explored[i];
			move_t move = n->move;
			// Count this node regarding to its first move
			(num_childs[move])+= 1;
			(total_action_score[move]) += (n->acc_reward);
		}
		// Take the average
		for (int i = 0; i < 4; i++) {
			if (num_childs[i] != 0) {
				best_action_score[i] = total_action_score[i] / num_childs[i];
			}
		}
	}

	// Choose Best move based on score
	int tie = 0;
	float max = INT_MIN;
	for(int i = 0; i < 4; i++){
		if (best_action_score[i] == max) {
			// Randomly break tie if two move have same score
			tie = 1;
			if (rand()%2 == 1) {
				best_action = i;
			}
		} else if (best_action_score[i] > max) {
			best_action = i;
			max = best_action_score[i];
			// Fix if tie is not the highest score
			tie = 0;
		}
	}

	/**Solver for buridans ass situation
	 * Will excuted if the score of current move is same as the last move and
	 * encounter a tie in current move, usually happened when the pacman cannot
	 * see any score-changing object in sight.
	 * Or current move have same move and score as the move before last move,
	 * usually happened when pacman is oscillating
	 * The pacman will repeat last step if it will not make pacman die or
	 * move into wall
	 */

	// REMOVED to pass test basicRightTwo
	//if ((last_move[1] == best_action && last_score[1] == max) ||
	// if (
	// (max == last_score[0] && tie == 1)) {
	// 	// Check next move
	// 	new_node = malloc(sizeof(node_t));
	// 	bool changed_dir = applyAction(root_node, new_node, last_move[0]);
	// 	if (!lost_life(new_node) && changed_dir) {
	// 		// If canbe done, replace best_action with it
	// 		best_action = last_move[0];
	// 		max = last_score[0];
	// 	}
	// 	free(new_node);
	// }

	//Free the memory allocated
	emptyPQ(&h);

	for (int i = 0; i < expanded_nodes; i++) {
		n = explored[i];
		free(n);
	}
	free(explored);

	//Update informations needed for output.txt
	if ((*total_max_depth)<max_depth) {
		(*total_max_depth) = max_depth;
	}
	(*total_generated) += generated_nodes;
	(*total_expanded) += expanded_nodes;

	//Push decided best move to the history
	// last_move[1] = last_move[0];
	// last_move[0] = best_action;
	// last_score[1] = last_score[0];
	// last_score[0] = max;

	// Print informations to screen
	sprintf(stats, "Max Depth: %d Expanded nodes: %d  Generated nodes: %d\n",max_depth,expanded_nodes,generated_nodes);
	if(best_action == left)
		sprintf(stats, "%sSelected action: Left\n",stats);
	if(best_action == right)
		sprintf(stats, "%sSelected action: Right\n",stats);
	if(best_action == up)
		sprintf(stats, "%sSelected action: Up\n",stats);
	if(best_action == down)
		sprintf(stats, "%sSelected action: Down\n",stats);
	sprintf(stats, "%sScore Left %f Right %f Up %f Down %f",stats,best_action_score[left],best_action_score[right],best_action_score[up],best_action_score[down]);
	t = clock() - t;
	(*process_time) += t;
	return best_action;
}

/**
 * Check if has lost life in current state
 */
bool lost_life(node_t* n){
	// The root node would not be lost life
	if (n->parent == NULL) {
		return false;
	} else {
		// Did not lost life if the number of lives of current
		// step is same as the previous step
		if (n->parent->state.Lives == n->state.Lives) {
			return false;
		} else {
			return true;
		}
	}
}