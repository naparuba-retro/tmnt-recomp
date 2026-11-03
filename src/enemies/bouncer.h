/* Types $A3 / $A4: bounces while advancing. (functions shared with the other enemy files) */
#ifndef ENEMIES_BOUNCER_H
#define ENEMIES_BOUNCER_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_bouncer(Game *g, int x);
void ai_bouncer_shot(Game *g, int x);
uint8_t anim_if_changed(Game *g, int x, uint8_t a);
void bouncer_state(Game *g, int x);
int tile_in_wall_set(Game *g, int x, uint8_t dx, uint8_t dy);

#endif
