/* Drawing and scrolling of a level map. */
#ifndef STAGE_MAP_H
#define STAGE_MAP_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void split_c0(Game *g); /* B7 $DF40 */
void stage_draw_map(Game *g);

/* helpers shared with stage_scroll.c */
uint8_t map_rol(uint8_t v, int *c);
uint8_t map_ror(uint8_t v, int *c);
void map_append(Game *g, uint8_t v);
void split_y(Game *g);
void ppu_row_address(Game *g, uint8_t mask);
void update_attribute_shadow(Game *g);
void record_tile_terrain(Game *g, uint8_t tile, uint8_t saved_y, int column);
void select_palette_bits(Game *g, uint8_t y, int column);
void draw_map_row(Game *g);

#endif
