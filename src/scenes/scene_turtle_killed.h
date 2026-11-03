/* Scene 16: the turtle is killed outright. (functions shared with the other stage_play files) */
#ifndef STAGE_PLAY_SCENE_TURTLE_KILLED_H
#define STAGE_PLAY_SCENE_TURTLE_KILLED_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void scene_turtle_killed_continue(Game *g);
void scene_turtle_killed_start(Game *g);

#endif
