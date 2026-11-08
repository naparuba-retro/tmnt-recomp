/* The list of options and tmnt.ini, see options.h. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fixes.h"
#include "hitbox_view.h"
#include "large_view.h"
#include "options.h"
#include "ppu.h"
#include "spawn_mask.h"

static const OptInfo info[OPT_COUNT] = {
    [OPT_EDITION] = {"edition", "play", "Edition", "The preset of options that the menu highlights.", 0, 0, 1, 1, 1},
    [OPT_FIX_CEILING] = {"ceiling_slide", "whole_pizza", "Ceiling slide", "Hitting a ceiling in a jump no longer stops the turtle's horizontal speed.", 0, 0, 1,
                         1, 1},
    [OPT_FIX_WATER] = {"water_current", "whole_pizza", "Water current", "The current of the water no longer pushes the swimming turtle for ever.", 0, 0, 1, 1,
                       1},
    [OPT_FIX_HITBOX] = {"hitbox_fixes", "whole_pizza", "Hitbox fixes", "The rotating bar of the dam has a box that matches its sprite.", 0, 0, 1, 1, 1},
    [OPT_FIX_CAMERA] = {"smart_camera", "whole_pizza", "Smart camera", "The camera catches up with a turtle that is far to one side of the screen.", 0, 0, 1, 1,
                        1},
    [OPT_SPAWN_MASK] = {"spawn_mask", "whole_pizza", "Spawn mask",
                        "Some enemy spawns are cut (data/spawn_mask.txt). Only the spawns to come, not the enemies already there.", 0, 0, 1, 1, 1},
    [OPT_BAL_DAMAGE] = {"damage_balance", "whole_pizza", "Damage balance", "More damage per hit: Leonardo 14, Raphael 20, Michelangelo 14, Donatello 15.", 0, 0,
                        1, 1, 1},
    [OPT_BAL_REACH] = {"raph_reach", "whole_pizza", "Raph's reach", "The sai of Raphael reaches 35 pixels in front of him instead of 28.", 0, 0, 1, 1, 1},
    [OPT_LARGE_VIEW] = {"large_view", "whole_pizza", "Large view", "No random tiles at the top and the edges of the screen: they show the map.", 0, 0, 1, 1, 1},
    [OPT_NO_FLICKER] = {"no_flicker", "whole_pizza", "No flicker", "All the sprites of a line are drawn, not only 8: no more blinking sprites.", 0, 0, 1, 1, 1},
    [OPT_HITBOX_VIEW] = {"hitbox_view", "extras", "Hitbox view", "Shows the boxes that the game tests (key H).", 0, 0, 1, 1, 1},
    [OPT_INVINCIBLE] = {"invincible", "extras", "Invincible", "The turtles' health is always full (cheat).", 0, 0, 1, 0, 1},
    [OPT_START_LEVEL] = {"start_level", "extras", "Start level", "The level where a new game starts.", 1, 1, 6, 0, 1},
    [OPT_VOLUME] = {"volume", "sound", "Volume", "The sound level of the game.", 100, 0, 100, 1, 1},
    [OPT_MUTE] = {"mute", "sound", "Mute", "No sound at all (key M).", 0, 0, 1, 1, 1},
    [OPT_FULLSCREEN] = {"fullscreen", "graphics", "Fullscreen", "The game on the whole screen.", 0, 0, 1, 1, 0},
    [OPT_INTEGER_SCALE] = {"integer_scale", "graphics", "Integer scaling", "Pixels all the same size (black bars where it does not fit).", 0, 0, 1, 1, 0},
    [OPT_SHOW_FPS] = {"show_fps", "graphics", "Show FPS", "The number of images per second in a corner of the screen.", 0, 0, 1, 1, 0},
};

static int local[OPT_COUNT]; /* the values that no engine holds */
static int dirty[OPT_COUNT]; /* changed from the menu: to be written to tmnt.ini */
static int local_ready;

const OptInfo *opt_info(OptId id) {
    return &info[id];
}

static void init_local(void) {
    if (local_ready) {
        return;
    }
    for (int i = 0; i < OPT_COUNT; i++) {
        local[i] = info[i].def;
    }
    local_ready = 1;
}

int opt_get(OptId id) {
    init_local();
    switch (id) {
        case OPT_FIX_CEILING:
            return fix_enabled(FIX_CEILING_SLIDE);
        case OPT_FIX_WATER:
            return fix_enabled(FIX_WATER_DRIFT);
        case OPT_FIX_HITBOX:
            return fix_enabled(FIX_WATER_SPINNER);
        case OPT_FIX_CAMERA:
            return fix_enabled(FIX_CAMERA_FOLLOW);
        case OPT_SPAWN_MASK:
            return spawn_mask_enabled();
        case OPT_BAL_DAMAGE:
            return fix_enabled(FIX_BALANCE_DAMAGE);
        case OPT_BAL_REACH:
            return fix_enabled(FIX_RAPH_REACH);
        case OPT_LARGE_VIEW:
            return large_view_enabled();
        case OPT_NO_FLICKER:
            return ppu_sprite_limit() > 8;
        case OPT_HITBOX_VIEW:
            return hitbox_enabled();
        default:
            return local[id];
    }
}

void opt_set(OptId id, int value) {
    init_local();
    if (value < info[id].min) {
        value = info[id].min;
    }
    if (value > info[id].max) {
        value = info[id].max;
    }
    switch (id) {
        case OPT_FIX_CEILING:
            fix_set(FIX_CEILING_SLIDE, value);
            break;
        case OPT_FIX_WATER:
            fix_set(FIX_WATER_DRIFT, value);
            break;
        case OPT_FIX_HITBOX:
            fix_set(FIX_WATER_SPINNER, value);
            break;
        case OPT_FIX_CAMERA:
            fix_set(FIX_CAMERA_FOLLOW, value);
            break;
        case OPT_SPAWN_MASK:
            spawn_mask_set_enabled(value);
            break;
        case OPT_BAL_DAMAGE:
            fix_set(FIX_BALANCE_DAMAGE, value);
            break;
        case OPT_BAL_REACH:
            fix_set(FIX_RAPH_REACH, value);
            break;
        case OPT_LARGE_VIEW:
            large_view_set_enabled(value);
            break;
        case OPT_NO_FLICKER:
            ppu_set_sprite_limit(value ? 64 : 8);
            break;
        case OPT_HITBOX_VIEW:
            hitbox_set_enabled(value);
            break;
        default:
            local[id] = value;
            break;
    }
}

void opt_set_user(OptId id, int value) {
    opt_set(id, value);
    dirty[id] = 1;
}

/* ------------------------------------------------------------------ editions --- */
void options_apply_edition(int edition) {
    for (int id = OPT_FIX_CEILING; id <= OPT_NO_FLICKER; id++) {
        opt_set_user((OptId)id, edition == 1);
    }
    opt_set_user(OPT_EDITION, edition);
}

int options_edition_matches(int edition) {
    for (int id = OPT_FIX_CEILING; id <= OPT_NO_FLICKER; id++) {
        if (opt_get((OptId)id) != (edition == 1)) {
            return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ tmnt.ini --- */
static char *dupstr(const char *s) {
    size_t n = strlen(s) + 1;
    char *d = (char *)malloc(n);
    if (d) {
        memcpy(d, s, n);
    }
    return d;
}

typedef struct {
    char **line;
    int n, cap;
} Lines;

static void lines_add(Lines *l, const char *s, int at) {
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 32;
        l->line = (char **)realloc(l->line, (size_t)l->cap * sizeof *l->line);
    }
    if (at < 0 || at > l->n) {
        at = l->n;
    }
    memmove(&l->line[at + 1], &l->line[at], (size_t)(l->n - at) * sizeof *l->line);
    l->line[at] = dupstr(s);
    l->n++;
}

static void lines_remove(Lines *l, int at) {
    free(l->line[at]);
    memmove(&l->line[at], &l->line[at + 1], (size_t)(l->n - at - 1) * sizeof *l->line);
    l->n--;
}

static void lines_free(Lines *l) {
    for (int i = 0; i < l->n; i++) {
        free(l->line[i]);
    }
    free(l->line);
    l->line = NULL;
    l->n = l->cap = 0;
}

static void lines_read(Lines *l, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return;
    }
    char buf[512];
    while (fgets(buf, sizeof buf, f)) {
        size_t n = strlen(buf);
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
            buf[--n] = 0;
        }
        lines_add(l, buf, -1);
    }
    fclose(f);
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t')) {
        *--e = 0;
    }
    return s;
}

/* The index of the line "key=..." inside the section, or -1; `section_end` receives where a new key of the section goes (-1 if it has no section yet). */
static int find_key(const Lines *l, const char *section, const char *key, int *section_end) {
    char cur[64] = "";
    int in_section = 0, found = -1, end = -1;
    for (int i = 0; i < l->n; i++) {
        char tmp[512];
        snprintf(tmp, sizeof tmp, "%s", l->line[i]);
        char *s = trim(tmp);
        if (s[0] == '[') {
            char *close = strchr(s, ']');
            if (close) {
                *close = 0;
                snprintf(cur, sizeof cur, "%s", s + 1);
                in_section = !strcmp(cur, section);
                if (in_section) {
                    end = i + 1;
                }
            }
            continue;
        }
        if (!in_section) {
            continue;
        }
        if (s[0] && s[0] != ';') {
            end = i + 1; /* after the last line that holds something */
            char *eq = strchr(s, '=');
            if (eq) {
                *eq = 0;
                if (!strcmp(trim(s), key)) {
                    found = i;
                }
            }
        }
    }
    *section_end = end;
    return found;
}

void options_load(const char *path) {
    init_local();
    Lines l = {0};
    lines_read(&l, path);
    char section[64] = "";
    for (int i = 0; i < l.n; i++) {
        char tmp[512];
        snprintf(tmp, sizeof tmp, "%s", l.line[i]);
        char *s = trim(tmp);
        if (s[0] == '[') {
            char *close = strchr(s, ']');
            if (close) {
                *close = 0;
                snprintf(section, sizeof section, "%s", s + 1);
            }
            continue;
        }
        char *eq = strchr(s, '=');
        if (s[0] == ';' || !eq) {
            continue;
        }
        *eq = 0;
        char *key = trim(s), *value = trim(eq + 1);
        for (int id = 0; id < OPT_COUNT; id++) {
            if (info[id].persist && !strcmp(info[id].section, section) && !strcmp(info[id].key, key)) {
                char *end;
                long v = strtol(value, &end, 10);
                if (end != value && v >= info[id].min && v <= info[id].max) {
                    opt_set((OptId)id, (int)v);
                } else {
                    fprintf(stderr, "tmnt.ini: %s=%s ignored (not a value from %d to %d)\n", key, value, info[id].min, info[id].max);
                }
            }
        }
    }
    lines_free(&l);
}

int options_save(const char *path) {
    int any = 0;
    for (int id = 0; id < OPT_COUNT; id++) {
        any |= dirty[id] && info[id].persist;
    }
    if (!any) {
        return 0;
    }
    Lines l = {0};
    lines_read(&l, path);
    if (l.n == 0) {
        lines_add(&l, "; tmnt.ini: only the values that differ from the defaults are written here (the game menu does it).", -1);
        lines_add(&l, "; Delete a line to get the default back. Keys that this version does not know are left alone.", -1);
    }
    for (int id = 0; id < OPT_COUNT; id++) {
        if (!dirty[id] || !info[id].persist) {
            continue;
        }
        int section_end;
        int at = find_key(&l, info[id].section, info[id].key, &section_end);
        char text[128];
        snprintf(text, sizeof text, "%s=%d", info[id].key, opt_get((OptId)id));
        if (opt_get((OptId)id) == info[id].def) {
            if (at >= 0) {
                lines_remove(&l, at);
            }
        } else if (at >= 0) {
            free(l.line[at]);
            l.line[at] = dupstr(text);
        } else if (section_end >= 0) {
            lines_add(&l, text, section_end);
        } else {
            char head[80];
            snprintf(head, sizeof head, "[%s]", info[id].section);
            lines_add(&l, "", -1);
            lines_add(&l, head, -1);
            lines_add(&l, text, -1);
        }
        dirty[id] = 0;
    }
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) {
        lines_free(&l);
        return 0;
    }
    for (int i = 0; i < l.n; i++) {
        fprintf(f, "%s\n", l.line[i]);
    }
    int ok = fclose(f) == 0;
    lines_free(&l);
    if (ok) {
        remove(path);
        ok = rename(tmp, path) == 0;
    }
    return ok;
}
