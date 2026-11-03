/* Intro scene (GameMode 1) and the typed texts. */
#ifndef INTRO_H
#define INTRO_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void text_update(Game *g); /* B3 $ADD1 */
void intro_update(Game *g);

#endif
