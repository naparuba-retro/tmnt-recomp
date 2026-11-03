/* Bosses $72 and $73: jumps, walks towards the player, strikes (two phases). (functions shared with the other enemy files) */
#ifndef ENEMIES_BOSS_BRAWLER_H
#define ENEMIES_BOSS_BRAWLER_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void boss_brawler(Game *g, int x);
void boss_brawler_phase2(Game *g, int x);
uint8_t boss_dist_x(Game *g, int x);
uint8_t boss_dist_y(Game *g, int x);

#endif
