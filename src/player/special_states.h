/* Other player states: roof flight, run state, push zones, conveyor. (functions shared with the other player files) */
#ifndef PLAYER_SPECIAL_STATES_H
#define PLAYER_SPECIAL_STATES_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void player_animation_state(Game *g);
void player_pose_anim(Game *g);
int player_run_state(Game *g, uint8_t s);
void push_zone(Game *g);

#endif
