/* Type $30: monster that drops a projectile, and its transformation. (functions shared with the other enemy files) */
#ifndef ENEMIES_WALKER_DROPPER_H
#define ENEMIES_WALKER_DROPPER_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_flyer(Game *g, int x);
void ai_walker_dropper(Game *g, int x);
void set_state(Game *g, int x, uint8_t st);
void walker_dropper_drop_state(Game *g, int x);
void walker_dropper_transform_state(Game *g, int x);

#endif
