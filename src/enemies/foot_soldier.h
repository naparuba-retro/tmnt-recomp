/* Type $40: foot soldier of the top-view streets. (functions shared with the other enemy files) */
#ifndef ENEMIES_FOOT_SOLDIER_H
#define ENEMIES_FOOT_SOLDIER_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_foot_soldier(Game *g, int x);
int blocked(Game *g, int x, uint8_t dx, uint8_t dy);
void face_player_get_distance(Game *g, int x);
void set_vel_x(Game *g, int x, uint8_t frac, uint8_t whole);
void set_vel_y(Game *g, int x, uint8_t frac, uint8_t whole);

#endif
