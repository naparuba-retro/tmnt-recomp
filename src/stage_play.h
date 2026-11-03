/* Level play: spawns, objects, scenes after a boss. */
#ifndef STAGE_PLAY_H
#define STAGE_PLAY_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void clear_object(Game *g, int x);                /* B7 $EFE6 */
void ai_van(Game *g, int x);                      /* B7 $CFD5 -> B4 $AA9A */
void level3_van_spawn(Game *g);                   /* B7 $CFB4 */
void camera_screen_split(Game *g); /* B7 $F1AC */ /* B6 $8146 */
void spawn_position(Game *g);                     /* B7 $F0A8 */
void stage_play(Game *g);
void stage_check_pause(Game *g);
void object_routes_report(Game *g, FILE *out);

#endif
