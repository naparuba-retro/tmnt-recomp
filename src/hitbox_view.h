/* Hit box display (key H of the game): shows the boxes that the collision routine (stage_play/collisions.c, B6 $9535) works with.
 * Nothing here changes the game: the hooks only read the RAM, the journal is outside `Game`, and the drawing is done on the 256x240 picture of the
 * front-end after the PPU rendering. */
#ifndef HITBOX_VIEW_H
#define HITBOX_VIEW_H
#include <stdint.h>

typedef struct Game Game;

void hitbox_set_enabled(int on); /* the journal is only filled while this is on */
int hitbox_enabled(void);
void hitbox_clear(void); /* forgets the journal (restart, rewind) */

/* Hooks of check_entity_collisions() (it runs every other frame): read-only. They tell which entities the game tested and which pairs touch. */
void hitbox_begin(Game *g);                                 /* start of one logic frame: every active entity, from the formula */
void hitbox_note_tested(Game *g, int x, int yi);            /* entity_hitbox(g, x, yi) just ran: its box is in Tmp8+yi .. TmpB+yi */
void hitbox_note_pair(Game *g, int x, int y, int touching); /* hitboxes_overlap(g, x, y) returned `touching` */

/* End of every logic frame: records the boxes of the entities (same formula as the game's, checked against it by the hooks above). */
void hitbox_end_frame(Game *g);

/* The box of entity x (left, right, top, bottom, in screen pixels), as the game computes it; nothing is written. */
void hitbox_entity_rect(Game *g, int x, uint8_t rect[4]);

/* Draws the boxes on the 256x240 picture (0xRRGGBB). The picture of frame N shows the sprites built from the state after logic frame N-2. */
void hitbox_draw(Game *g, uint32_t *screen);

unsigned hitbox_checked(void);    /* boxes read from the game so far */
unsigned hitbox_mismatches(void); /* boxes read from the game that differ from the formula of this file (should stay 0) */

#endif
