/* Comic-book sound effects ("POW!", "BAM!", "OUCH!") that pop up where a blow lands, on the picture of the game (display only). The hooks in the game's logic only
 * read the RAM and record an event here, outside `Game`; the words are drawn on the 256x240 picture after the rendering and the lighting. */
#ifndef COMIC_H
#define COMIC_H
#include <stdint.h>

typedef struct Game Game;

void comic_init(const char *data_dir); /* loads the font of the menu (fonts/KOMIKAHB.ttf): without it nothing is drawn */
void comic_hit(Game *g, int x);        /* the player's blow hurt enemy x (it is still alive) */
void comic_kill(Game *g, int x);       /* the player's blow defeated enemy x */
void comic_hurt(Game *g);              /* the player is hit */
void comic_draw(Game *g, uint32_t *screen);

#endif
