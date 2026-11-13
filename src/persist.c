/* Enemies that stay dead: see persist.h. */
#include <stdio.h>
#include <string.h>
#include "persist.h"

static int enabled;
static unsigned stat_marked, stat_skipped; /* tests: how many enemies were remembered, and how many spawns were skipped */

void persist_set_enabled(int on) {
    enabled = on != 0;
}

int persist_enabled(void) {
    return enabled;
}

void persist_reset(Game *g) {
    memset(g->dead_spawns, 0, sizeof g->dead_spawns);
    memset(g->spawn_origin, 0, sizeof g->spawn_origin);
    memset(g->waves_done, 0, sizeof g->waves_done);
}

int persist_is_dead(const Game *g, uint16_t entry) {
    if (!enabled || entry < 0x8000 || entry >= 0xC000) {
        return 0;
    }
    unsigned bit = (unsigned)(entry - 0x8000);
    int dead = (g->dead_spawns[bit >> 3] >> (bit & 7)) & 1;
    stat_skipped += (unsigned)dead;
    return dead;
}

void persist_note_spawn(Game *g, int x, uint16_t entry) {
    if (x >= 0 && x < 16) {
        g->spawn_origin[x] = entry;
    }
}

void persist_note_death(Game *g, int x) {
    if (x < 0 || x >= 16 || !g->spawn_origin[x]) {
        return;
    }
    uint16_t entry = g->spawn_origin[x];
    g->spawn_origin[x] = 0;
    uint8_t type = g->ram[EntType + x];
    if (!enabled || entry < 0x8000 || entry >= 0xC000 || (type >= 0x70 && type < 0x80)) { /* the bosses have their own memory ($7C) */
        return;
    }
    unsigned bit = (unsigned)(entry - 0x8000);
    stat_marked += !((g->dead_spawns[bit >> 3] >> (bit & 7)) & 1);
    g->dead_spawns[bit >> 3] |= (uint8_t)(1u << (bit & 7));
}

void persist_report(void) {
    fprintf(stderr, "stay dead: %u enemies remembered, %u spawns skipped\n", stat_marked, stat_skipped);
}

void persist_note_wave(Game *g, uint8_t mask) {
    if (enabled && g->ram[LevelNumber] < 8 && g->ram[AreaNumber] < 32) {
        g->waves_done[g->ram[LevelNumber]][g->ram[AreaNumber]] |= mask;
    }
}

uint8_t persist_waves_over(const Game *g) {
    return enabled && g->ram[LevelNumber] < 8 && g->ram[AreaNumber] < 32 ? g->waves_done[g->ram[LevelNumber]][g->ram[AreaNumber]] : 0;
}
