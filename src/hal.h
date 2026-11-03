/* Hardware services: sound requests, PPU buffer, screens, palettes, mapper banks. */
#ifndef HAL_H
#define HAL_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void set_chr_bank0(Game *g, uint8_t b);
void set_chr_bank1(Game *g, uint8_t b);
void load_screen(Game *g, int bank, uint8_t index);
void load_screen_and_clear(Game *g);
void load_palette_preset(Game *g, uint8_t index);
void load_level_palette(Game *g, uint8_t index);
void set_turtle_sprite_palette(Game *g);
void fill_palette_backdrop(Game *g, uint8_t color);
void ppu_buffer_append(Game *g, uint8_t v);
void print_text(Game *g, uint8_t index);
void print_text_keep_tmp3(Game *g, uint8_t index);
void clear_game_ram30(Game *g);
void clear_game_ram48(Game *g);
void set_delay_timer(Game *g);
void set_delay_timer_value(Game *g, uint8_t lo, uint8_t hi);
int tick_delay_timer(Game *g);
void load_screen_alt(Game *g, uint8_t x);   /* B7 $C992 */
void set_chr_bank0_raw(Game *g, uint8_t b); /* B7 $CE8E: without remembering ChrBank0 */
void set_chr_bank1_raw(Game *g, uint8_t b); /* B7 $CEA9 */

#endif
