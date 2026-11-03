/* Turrets ($36-$3D), their projectile and types $41 / $48 / $35. (functions shared with the other enemy files) */
#ifndef ENEMIES_TURRETS_H
#define ENEMIES_TURRETS_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_boss74_shot(Game *g, int x, uint8_t vx, uint8_t vy);
void ai_close_attacker(Game *g, int x);
void ai_slow_advancer(Game *g, int x);
void ai_turret(Game *g, int x, uint8_t type);
void ai_turret_shot(Game *g, int x);
void ai_van_shot(Game *g, int x);
void turret_state(Game *g, int x, uint8_t shot);
void update_hurt_flash(Game *g, int x);

#endif
