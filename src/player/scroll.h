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

/* The turtle is further than these from the middle of the screen while the screen could scroll that way (see camera_far_from_player()). */
#define CAMERA_FAR_LEFT 0x40
#define CAMERA_FAR_RIGHT 0xC0
int camera_far_from_player(const Game *g);

/* PIZZA-004 (FIXES.md): after the camera has scrolled, the turtle follows when the camera was moved to bring it back towards the middle. */
void camera_recentre_finish(Game *g);

#endif
