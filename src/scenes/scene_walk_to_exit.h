/* Scene 14: after the boss of the vertical zone, the turtle walks to the exit. (functions shared with the other stage_play files) */
#ifndef STAGE_PLAY_SCENE_WALK_TO_EXIT_H
#define STAGE_PLAY_SCENE_WALK_TO_EXIT_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void scene_walk_to_exit_continue(Game *g);
void scene_walk_to_exit_start(Game *g);
void walk_exit_simulated_pad(Game *g, uint8_t y);

#endif
