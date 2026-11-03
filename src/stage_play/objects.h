/* Objects (bonuses, scenery, crates, currents): B4 $B554 UpdateObjectSlots. (functions shared with the other stage_play files) */
#ifndef STAGE_PLAY_OBJECTS_H
#define STAGE_PLAY_OBJECTS_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void update_object_slots(Game *g);

#endif
