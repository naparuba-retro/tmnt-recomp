/* Sound wrappers around the bank 0 driver. */
#ifndef SOUND_H
#define SOUND_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void sound_update(Game *g);
void sound_init(Game *g);
void sound_play(Game *g, uint8_t id);

#endif
