/* Enemies, bosses and their contact with the player. */
#ifndef ENEMY_H
#define ENEMY_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void boss74_picture(Game *g, uint8_t n);         /* B5 $B170 (called by B7 $D075) */
int spawn_enemy(Game *g, uint8_t list_pos);      /* returns the created entity, -1 (no room) or SPAWN_SKIPPED (spawn_mask.h: cut by the spawn mask) */
void spawn_initial_side(Game *g);                /* B6 $85CC */
void clamp_entity_position(Game *g, int x);      /* B5 $8077 */
void enemy_update_all(Game *g);                  /* B5 $8001 */
void player_hit_by_enemy(Game *g, int x);        /* B6 $9784 */
void enemy_hit_by_weapon(Game *g, int x, int y); /* B6 $9913 */
int spawn_wave_boss(Game *g);                    /* B6 $82C9 */
int spawn_wave_enemy(Game *g);                   /* B6 $81F4: wave bank then enemy of the list */
void enemy_routes_report(Game *g, FILE *out);    /* routes still missing (see ROM tables), printed by the replay tool with --routes */

#endif
