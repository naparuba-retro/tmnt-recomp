/* Type $91: chansawiac. (functions shared with the other enemy files) */
#ifndef ENEMIES_CHANSAWIAC_H
#define ENEMIES_CHANSAWIAC_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_chansawiac(Game *g, int x);
void hop_start(Game *g, int x, uint8_t a);

#endif
