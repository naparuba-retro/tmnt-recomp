/* Cutscenes (Splinter kidnapped, end of level 4) and the PPU text engine. */
#ifndef SCENES_H
#define SCENES_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void scene_splinter_kidnapped_start(Game *g);    /* B6 $A1ED */
void scene_splinter_kidnapped_continue(Game *g); /* B6 $A1F3 */
int ppu_fill_script(Game *g, uint16_t p);        /* B7 $F259: carry = block finished */
int ppu_text_script(Game *g, uint16_t p);        /* B7 $F301 */
void scene_level4_end_start(Game *g);            /* B6 $A33E: end of level 4 */
void scene_level4_end_continue(Game *g);         /* B6 $A344 */

#endif
