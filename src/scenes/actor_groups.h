/* Groups of cutscene actors. (functions shared with the other player files) */
#ifndef SCENES_ACTOR_GROUPS_H
#define SCENES_ACTOR_GROUPS_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void group_frame(Game *g);
void group_setup(Game *g, uint8_t grp);

#endif
