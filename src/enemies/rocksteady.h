/* Types $92 / $93 / $94: rocksteady, its projectile and the captive. (functions shared with the other enemy files) */
#ifndef ENEMIES_ROCKSTEADY_H
#define ENEMIES_ROCKSTEADY_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void add_vel_x_a(Game *g, int x, uint8_t a);
void ai_april(Game *g, int x);
void ai_april_shot(Game *g, int x);
void ai_rocksteady(Game *g, int x);
void ai_rocksteady_shot(Game *g, int x);
void sub_vel_x_a(Game *g, int x, uint8_t a);

#endif
