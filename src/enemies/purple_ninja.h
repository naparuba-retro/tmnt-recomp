/* purple_ninja ($98) and its projectile ($99). (functions shared with the other enemy files) */
#ifndef ENEMIES_PURPLE_NINJA_H
#define ENEMIES_PURPLE_NINJA_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_purple_ninja(Game *g, int x);
void ai_purple_ninja_shot(Game *g, int x);
void anim_state_arc(Game *g, int x);
void ballistic(Game *g, int x, uint8_t idx, int sub);
void distance_to_player(Game *g, int x);
void ninja_set_anim_state(Game *g, int x, uint8_t anim);
void ninja_state_fall(Game *g, int x);
void ninja_state_jump(Game *g, int x);
void set_vel_y_abs(Game *g, int x, uint8_t frac, uint8_t whole);
int spawn_child(Game *g, int x, uint8_t idx);
int spawn_child_slot(Game *g, int x, uint8_t idx);
void wait_anim_end(Game *g, int x);

#endif
