/* Type $4D: satellite orbiting a column; $4E: its shot. (functions shared with the other enemy files) */
#ifndef ENEMIES_SATELLITE_H
#define ENEMIES_SATELLITE_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_orbiting_satellite(Game *g, int x);
void ai_satellite_shot(Game *g, int x);

#endif
