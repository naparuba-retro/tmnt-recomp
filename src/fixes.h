/* Fixes of the "Whole Pizza" edition (see FIXES.md): corrections of the original game, one flag per fix.
 * Every flag is off by default, and off in the replay tool: the port then plays exactly like the original (the reference game and the TAS replay
 * without any difference). A flag is a global, not part of `Game`, so the snapshots (F9) and the replays do not carry it. */
#ifndef FIXES_H
#define FIXES_H

typedef enum {
    FIX_CEILING_SLIDE, /* PIZZA-001: hitting a ceiling in a jump no longer stops the turtle's horizontal speed */
    FIX_WATER_DRIFT,   /* PIZZA-002: the current of the water no longer leaves a speed that pushes the swimming turtle for ever (group water, key E) */
    FIX_WATER_SPINNER, /* PIZZA-003: the box of the rotating bar of the dam is a square a little smaller than its sprite, centred on it (it was a tall box hanging above it) (group water, key E) */
    FIX_COUNT
} FixId;

int fix_enabled(FixId id);
void fix_set(FixId id, int on);
void fix_set_all(int on);
void fix_toggle_all(void); /* key J of the game: all the fixes (but those of the water) on, or all off */
int fix_any_enabled(void);
/* The fixes of the water level are a group of their own, with their own key (E) and option (--water-fixes). */
int fix_is_water(FixId id);
void fix_toggle_water(void);
int fix_water_enabled(void);
void fix_set_water(int on);

#endif
