/* Family of type $30: $9E, $9F, $A0, $A1 and their states. (functions shared with the other enemy files) */
#ifndef ENEMIES_WALKER_FAMILY_H
#define ENEMIES_WALKER_FAMILY_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_walker_charge_left(Game *g, int x);
void ai_walker_charge_right(Game *g, int x);
void ai_walker_hopper(Game *g, int x);
void ai_walker_stop_and_throw(Game *g, int x);
void walker_approach_state(Game *g, int x);
void walker_hop_state(Game *g, int x);
void walker_recover_state(Game *g, int x);
void walker_start_walk_state(Game *g, int x);
void walker_toss_state(Game *g, int x);

#endif
