/* Camera spawn list (B6 $80F6): enemies at the screen edge. (functions shared with the other stage_play files) */
#ifndef STAGE_PLAY_SPAWN_LIST_H
#define STAGE_PLAY_SPAWN_LIST_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

int enemy_present(Game *g);
void process_spawn_list(Game *g);
void update_enemies(Game *g);

#endif
