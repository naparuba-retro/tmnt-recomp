/* Scene start and per-frame dispatch (B6 $9F47) and the scenes 1-9. (functions shared with the other stage_play files) */
#ifndef STAGE_PLAY_SCENE_DISPATCH_H
#define STAGE_PLAY_SCENE_DISPATCH_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void scene_update(Game *g);

#endif
