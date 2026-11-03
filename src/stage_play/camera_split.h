/* Status bar split and camera/screen flags of the top view. (functions shared with the other stage_play files) */
#ifndef STAGE_PLAY_CAMERA_SPLIT_H
#define STAGE_PLAY_CAMERA_SPLIT_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void set_prg_saved(Game *g, uint8_t b);
void status_bar_split(Game *g);
void update_scroll_edge_flag(Game *g);

#endif
