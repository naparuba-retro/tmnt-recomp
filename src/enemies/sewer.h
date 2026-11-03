/* Sewer enemies: types $4B and $85. (functions shared with the other enemy files) */
#ifndef ENEMIES_SEWER_H
#define ENEMIES_SEWER_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_defensive_hopper(Game *g, int x);
void ai_fast_walker(Game *g, int x);
void ai_leaper(Game *g, int x);
void ai_sewer_thrower(Game *g, int x);
void ai_sewer_thrower_shot(Game *g, int x);
void ai_sewer_walker(Game *g, int x);
void sewer_walker_leap(Game *g, int x);
void walk_and_stop(Game *g, int x, uint8_t whole, uint8_t frac);

#endif
