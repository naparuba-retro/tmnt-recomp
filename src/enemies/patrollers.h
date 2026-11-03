/* Types $9A / $9B: patrol, and the defeated enemy that disappears. (functions shared with the other enemy files) */
#ifndef ENEMIES_PATROLLERS_H
#define ENEMIES_PATROLLERS_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_defeated_enemy(Game *g, int x);
void ai_patroller_a(Game *g, int x);
void ai_patroller_b(Game *g, int x);
void clear_velocity(Game *g, int x);
int patrol_down(Game *g, int x);
int patrol_left(Game *g, int x);
int patrol_right(Game *g, int x);
int patrol_up(Game *g, int x);

#endif
