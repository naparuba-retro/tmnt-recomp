/* Side-view status bar. */
#ifndef HUD_H
#define HUD_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void hud_update(Game *g); /* B0 $8055 */

#endif
