/* Map: tile under a point and collision probes. (functions shared with the other player files) */
#ifndef PLAYER_TILES_H
#define PLAYER_TILES_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

int probe_tile(Game *g, uint8_t y, uint8_t a);
int probe_tile_vel(Game *g, uint8_t y, uint8_t a);
uint8_t tile_at(Game *g, uint8_t px, uint8_t py, uint8_t yreg);
uint8_t tile_near_player(Game *g, uint8_t i);

/* The class (0-15) of the collision map under the screen point (px, py), computed as GetTileAt (B7 $C76B) does but without writing anything: from the
 * 256 bytes of the map and the camera. tile_class_near_player() is the same at the probe point i around the player. */
uint8_t terrain_class_at(const uint8_t *terrain, uint8_t cam_x_lo, uint8_t cam_x_hi, uint8_t cam_y_lo, uint8_t cam_y_hi, int vertical, uint8_t px, uint8_t py);
uint8_t tile_class_near_player(Game *g, uint8_t i);

#endif
