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

#endif
