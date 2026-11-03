/* Boss dispatcher ($70-$7F), bebop ($70) and the boss holding the hostage ($71). (functions shared with the other enemy files) */
#ifndef ENEMIES_BOSSES_H
#define ENEMIES_BOSSES_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void boss_bebop(Game *g, int x);
void boss_face(Game *g, int x);
void boss_hostage_holder(Game *g, int x);
void boss_place(Game *g, int x, uint8_t i);
void boss_state(Game *g, int x, uint8_t st);

#endif
