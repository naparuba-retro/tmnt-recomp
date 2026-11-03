/* Helpers shared by the enemy AIs (side-view movement, collisions, vectors). (functions shared with the other enemy files) */
#ifndef ENEMIES_COMMON_H
#define ENEMIES_COMMON_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void add_vel_y(Game *g, int x, uint8_t a);
void ai_charger(Game *g, int x);
extern int ai_leak_x; /* $B3AD leaves X = element placed: the rest of the $8001 loop uses it */
void ai_walker_start(Game *g, int x);
void aim_at(Game *g, int x);
void aim_index(Game *g, int x);
int front_tile_set(Game *g, int x, uint8_t a, uint8_t dy);
int in_block_list(uint8_t t);
int land_check(Game *g, int x, uint8_t dx, uint8_t dy);
int move_blocked(Game *g, int x, uint8_t a, uint8_t dy);
void run_entity_ai(Game *g, int x);
void set_velocity_from_m(Game *g, int x);
void turn_around(Game *g, int x);
void walk_speed(Game *g, int x, uint8_t whole, uint8_t frac);
void walker_patrol_step(Game *g, int x);

#endif
