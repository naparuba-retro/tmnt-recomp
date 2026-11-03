/* Type $AB: scripted trajectories then charge. (functions shared with the other enemy files) */
#ifndef ENEMIES_SWOOPER_H
#define ENEMIES_SWOOPER_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_swooper(Game *g, int x);
void swooper_trajectory_state(Game *g, int x);

#endif
