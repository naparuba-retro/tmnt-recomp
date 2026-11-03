/* NMI: what the console does around the game logic, controllers, random numbers. */
#ifndef NMI_H
#define NMI_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void read_controllers(Game *g);
void nmi_frame(Game *g);
void game_init(Game *g);
void rng_idle(Game *g); /* B7 $C395 MainLoop: the console's randomness */
void blank_ppu(Game *g);
void flush_ppu_buffer(Game *g);

#endif
