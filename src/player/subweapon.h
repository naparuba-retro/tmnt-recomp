/* Sub-weapons (shuriken, boomerang) and ammo. (functions shared with the other player files) */
#ifndef PLAYER_SUBWEAPON_H
#define PLAYER_SUBWEAPON_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

int free_projectile_slot(Game *g);
void player_attack(Game *g);
void player_walk_up_down(Game *g);
void subweapon_sound(Game *g);
void subweapon_throw(Game *g);

#endif
