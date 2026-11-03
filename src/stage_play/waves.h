/* Enemy waves and wave triggers (B6 $9C1D-$9E10). (functions shared with the other stage_play files) */
#ifndef STAGE_PLAY_WAVES_H
#define STAGE_PLAY_WAVES_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void side_wave_triggers(Game *g);

#endif
