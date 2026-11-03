/* Scenes after a boss: common steps and the per-frame runner. (functions shared with the other stage_play files) */
#ifndef STAGE_PLAY_SCENE_RUNNER_H
#define STAGE_PLAY_SCENE_RUNNER_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void scene_continue(Game *g);
void scene_decor(Game *g);
void scene_end(Game *g);
void scene_frame(Game *g);
void scene_master_on_rope_continue(Game *g);
void scene_run_frame(Game *g);

#endif
