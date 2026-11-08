/* The options of the player, in one list: the ones the game engine reads (the fixes of FIXES.md, the spawn mask, the large view, the sprite flicker...), the
 * cheats, and the settings of the front-end (volume, window...). The menu (menu.c) shows this list; the keys of the front-end are shortcuts to some of it.
 * The list also tells how each option is saved in tmnt.ini: only a value that differs from its default is written, and what the file already holds (other
 * keys, comments, keys of other versions) is kept. The replay tool never reads that file: its options come from the environment. */
#ifndef OPTIONS_H
#define OPTIONS_H

typedef enum {
    OPT_EDITION, /* 0 = Original, 1 = Whole Pizza: only the one that the menu highlights */
    /* what the Whole Pizza edition switches on (the engine reads these) */
    OPT_FIX_CEILING, /* PIZZA-001 */
    OPT_FIX_WATER,   /* PIZZA-002 */
    OPT_FIX_HITBOX,  /* PIZZA-003: the rotating bar of the dam */
    OPT_FIX_CAMERA,  /* PIZZA-004 */
    OPT_SPAWN_MASK,  /* PIZZA-005 */
    OPT_BAL_DAMAGE,  /* PIZZA-006 */
    OPT_BAL_REACH,   /* PIZZA-007 */
    OPT_LARGE_VIEW,
    OPT_NO_FLICKER,
    /* extras: third-party options */
    OPT_HITBOX_VIEW,
    OPT_INVINCIBLE,
    OPT_START_LEVEL, /* 1 to 6: the level where a new game starts */
    /* sound */
    OPT_VOLUME, /* 0 to 100 */
    OPT_MUTE,
    /* graphics (not wired yet: they are kept in the file and shown in the menu) */
    OPT_FULLSCREEN,
    OPT_INTEGER_SCALE,
    OPT_SHOW_FPS,
    /* the keyboard: two keys for each pad button, in the order A, B, Select, Start, Up, Down, Left, Right (OPT_KEY). 0 = the default key of the platform,
     * -1 = no key, otherwise the key code of the platform layer (platform.h) */
    OPT_KEY_FIRST,
    OPT_KEY_LAST = OPT_KEY_FIRST + 15,
    OPT_COUNT
} OptId;

/* The option of the key `slot` (0 or 1) of a pad button given by its NES bit (A = 7 ... Right = 0). */
#define OPT_KEY(bit, slot) ((OptId)(OPT_KEY_FIRST + (7 - (bit)) * 2 + (slot)))

typedef struct {
    const char *key, *section, *label, *description;
    int def, min, max;
    int persist; /* written to tmnt.ini */
    int wired;   /* 0 = "soon": the option is shown but does nothing yet */
} OptInfo;

const OptInfo *opt_info(OptId id);
int opt_get(OptId id);
void opt_set(OptId id, int value);      /* applies the value (engine or front-end) */
void opt_set_user(OptId id, int value); /* the same, from the menu: the value will be written to tmnt.ini */

/* The editions are presets of the Whole Pizza options (OPT_FIX_CEILING to OPT_NO_FLICKER). */
#define EDITION_COUNT 2
void options_apply_edition(int edition);
int options_edition_matches(int edition); /* the options are exactly the preset */

/* tmnt.ini: lines "key=value" in sections, ';' for comments. */
void options_load(const char *path);
int options_save(const char *path); /* writes what the menu changed; 1 = written */

#endif
