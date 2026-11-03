/* Helpers shared by the cutscenes: animation, clearing, entity creation. (functions shared with the other player files) */
#ifndef SCENES_SCENE_COMMON_H
#define SCENES_SCENE_COMMON_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void animate0(Game *g);
void clear_all(Game *g);
void entity_from_type(Game *g, int x);
void flags_clear_bit1(Game *g, int x);
void flags_set_bit1(Game *g, int x);
void zero_frac_vel(Game *g, int x);
void zero_vel(Game *g, int x);

#endif
