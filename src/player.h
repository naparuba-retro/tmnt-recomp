/* Player, entities of bank 7 and tile lookups. */
#ifndef PLAYER_H
#define PLAYER_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void clear_entity(Game *g, int x); /* B7 $EF67 */
void player_spawn(Game *g);        /* B4 $9662 */
void player_update(Game *g);       /* B4 $9745 */
void weapon_update(Game *g);
void clear_vel_x(Game *g, int x);
uint8_t tile_at_xy(Game *g, uint8_t px, uint8_t py, uint8_t yreg); /* B7 $C7E4: tile at the point, index in $0598 */
void hurt_blink_tick(Game *g, int x);                              /* B7 $F199 */
void clear_vel_y(Game *g, int x);
void move_entity_x(Game *g, int x);
void move_entity_y(Game *g, int x);
void move_entity(Game *g, int x);                               /* B7 $EF22 */
uint8_t tile_at_offset(Game *g, int x, uint8_t dx, uint8_t dy); /* B7 $F091 GetTileAtOffset */
int player_hurt_state(Game *g);
void health_display_update(Game *g); /* B6 $ABF9 */ /* B4 $97A3: the player is hit */
void player_side_hurt(Game *g, int x);              /* B4 $A66B */
void player_attack_end(Game *g);                    /* B4 $9F35 */
void attack_sound(Game *g);                         /* B4 $9F5C */
void overhead_weapon_start(Game *g);                /* B7 $E132 */
void player_set_pose(Game *g, uint8_t anim);        /* B4 $A634 */
void door_transition_entry(Game *g);                /* B7 $E6A9 */

#endif
