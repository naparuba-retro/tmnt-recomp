/* Entity animation engine (bank 6). */
#ifndef ANIM_H
#define ANIM_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void animate_entities_from(Game *g, int first);
void animate_entity(Game *g, int x);
void set_animation(Game *g, int x, uint8_t id); /* B6 $8081 */

#endif
