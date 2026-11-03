/* Creation of enemies from the spawn list (B6 $8146-$82C8). (functions shared with the other enemy files) */
#ifndef ENEMIES_SPAWNING_H
#define ENEMIES_SPAWNING_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

int init_entity_type_tail(Game *g, int x, uint8_t t);

#endif
