/* Bosses $75-$78 of the vertical zone of level 5. (functions shared with the other enemy files) */
#ifndef ENEMIES_BOSS_CORE_H
#define ENEMIES_BOSS_CORE_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void boss_core(Game *g, int x);
void boss_fragment(Game *g, int x);
void boss_turret(Game *g, int x);
void boss_turret_shot(Game *g, int x);
uint8_t core_aim(Game *g, int x);
void core_palette_swap(Game *g, int x);

#endif
