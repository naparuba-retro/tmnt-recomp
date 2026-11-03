/* NES APU (sound output). */
#ifndef APU_H
#define APU_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void apu_write(Game *g, int reg, uint8_t v);
uint8_t apu_read_status(Game *g);
void apu_mix(Game *g, int16_t *out, int n);

#endif
