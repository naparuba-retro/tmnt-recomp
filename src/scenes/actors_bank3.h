/* Cutscene actors of bank 3. (functions shared with the other player files) */
#ifndef SCENES_ACTORS_BANK3_H
#define SCENES_ACTORS_BANK3_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void bank3_frame(Game *g, int x, int k);
void bank3_init(Game *g, int x, int k);

#endif
