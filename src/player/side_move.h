/* Side view: walking, jumping, falling. (functions shared with the other player files) */
#ifndef PLAYER_SIDE_MOVE_H
#define PLAYER_SIDE_MOVE_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void player_airborne(Game *g);
void player_falling(Game *g);
void player_gravity(Game *g);
void player_on_ground(Game *g);
void walk_velocity(Game *g, uint8_t y);

#endif
