/* Large view (key L of the game): the lines at the top of the screen that show the row above the level's map. In a level the game scrolls the picture
 * 4 lines down; the first 4 lines of the screen then read the attribute table of the video memory as if it were tiles (rows 30 and 31), a band of
 * random tiles that a TV of the time hid in its overscan. With the large view on, these lines show the row of the level that is really above the picture
 * (rebuilt with the game's own row drawing, run on a copy of the state), or the backdrop colour when the map has no row there (the top of the map, the
 * rooms seen from the side, the title screens). Display only: the game's state, the replays and the validation do not change. */
#ifndef LARGE_VIEW_H
#define LARGE_VIEW_H

typedef struct Game Game;

void large_view_set_enabled(int on);
int large_view_enabled(void);

/* Before ppu_render(): tells the PPU what to show on those lines. */
void large_view_prepare(Game *g);

/* Tests (replay tool, TMNT_LARGE=1): at every frame where the lines are there, rebuilds the row and checks that the game's state is untouched and that the
 * row is the one the video memory holds when it holds one. Prints a summary at the end of the replay. */
void large_view_check(Game *g);
void large_view_report(void);

#endif
