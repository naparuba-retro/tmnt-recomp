/* Cutscene actors of bank 2 (B2 $B8F9 / $B933): initialisation and per-frame behaviour. (functions shared with the other player files) */
#ifndef SCENES_ACTORS_H
#define SCENES_ACTORS_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void actor_frame(Game *g, int x, uint8_t k);
void actor_init(Game *g, int x, uint8_t k);

#endif
