/* Fixes of the "Whole Pizza" edition (see FIXES.md): corrections of the original game, one flag per fix.
 * Every flag is off by default, and off in the replay tool: the port then plays exactly like the original (the reference game and the TAS replay
 * without any difference). A flag is a global, not part of `Game`, so the snapshots (F9) and the replays do not carry it. */
#ifndef FIXES_H
#define FIXES_H

typedef enum {
    FIX_CEILING_SLIDE, /* PIZZA-001: hitting a ceiling in a jump no longer stops the turtle's horizontal speed */
    FIX_COUNT
} FixId;

int fix_enabled(FixId id);
void fix_set(FixId id, int on);
void fix_set_all(int on);
void fix_toggle_all(void); /* key J of the game: all the fixes on, or all off */
int fix_any_enabled(void);

#endif
