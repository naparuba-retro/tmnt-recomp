/* Scrolling triggered by the player and vertical screen changes (pits). (functions shared with the other player files) */
#ifndef PLAYER_SCROLL_H
#define PLAYER_SCROLL_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void level3_pit_blink(Game *g);
void player_scroll_trigger(Game *g);
int side_screen_edge(Game *g);
void side_vertical_scroll(Game *g);

#endif
