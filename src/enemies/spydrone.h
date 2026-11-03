/* Type $AA: spydrone. (functions shared with the other enemy files) */
#ifndef ENEMIES_SPYDRONE_H
#define ENEMIES_SPYDRONE_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_dam_scenery_swap(Game *g, int x);
void ai_rotbar(Game *g, int x);
void ai_scenery_swap(Game *g, int x);
void ai_spydrone(Game *g, int x);
void rotbar_visibility(Game *g, int x);
void spydrone_chase_state(Game *g, int x);
void vector_to(Game *g, int x);

#endif
