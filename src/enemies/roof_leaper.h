/* Type $86 (roof_leaper / divebombs). (functions shared with the other enemy files) */
#ifndef ENEMIES_ROOF_LEAPER_H
#define ENEMIES_ROOF_LEAPER_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_roof_leaper(Game *g, int x);
void go_left(Game *g, int x);
void go_right(Game *g, int x);
void spider_climb(Game *g, int x);
void spider_drop(Game *g, int x);
void spider_walk(Game *g, int x);

#endif
