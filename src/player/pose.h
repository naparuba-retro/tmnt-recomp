/* Player animation and poses. (functions shared with the other player files) */
#ifndef PLAYER_POSE_H
#define PLAYER_POSE_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

int is_special_area(Game *g);
void player_idle(Game *g);
void weapon_effect_start(Game *g);

#endif
