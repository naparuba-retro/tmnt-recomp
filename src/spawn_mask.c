/* Spawn mask: see spawn_mask.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spawn_mask.h"
#include "rom.h"

typedef struct {
    uint16_t address;
    int all;   /* * : every sprite set */
    int count; /* the sprite sets, when not all */
    uint8_t sets[8];
} MaskEntry;

static MaskEntry *entries;
static int entry_count;
static int enabled;

static int compare(const void *a, const void *b) {
    return (int)((const MaskEntry *)a)->address - (int)((const MaskEntry *)b)->address;
}

void spawn_mask_set_enabled(int on) {
    enabled = on;
}

int spawn_mask_enabled(void) {
    return enabled;
}

int spawn_mask_count(void) {
    return entry_count;
}

int spawn_mask_active(void) {
    return enabled && entry_count > 0;
}

const char *spawn_mask_default_path(const char *exe_dir, char *out, unsigned size) {
    const char *fmt[4] = {"%s/data/spawn_mask.txt", "%s/spawn_mask.txt", "%s/../data/spawn_mask.txt", "data/spawn_mask.txt"};
    for (int i = 0; i < 4; i++) {
        snprintf(out, size, fmt[i], exe_dir);
        FILE *f = fopen(out, "r");
        if (f) {
            fclose(f);
            return out;
        }
    }
    return NULL;
}

int spawn_masked(uint16_t address, uint8_t sprite_set) {
    int lo = 0, hi = entry_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (entries[mid].address == address) {
            const MaskEntry *e = &entries[mid];
            if (e->all) {
                return 1;
            }
            for (int i = 0; i < e->count; i++) {
                if (e->sets[i] == sprite_set) {
                    return 1;
                }
            }
            return 0;
        }
        if (entries[mid].address < address) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return 0;
}

/* Adds the sprite sets of a line to an entry (the same address may come on several lines). */
static void add_sets(MaskEntry *e, char *sets) {
    if (!strcmp(sets, "*")) {
        e->all = 1;
        return;
    }
    for (char *tok = strtok(sets, ","); tok; tok = strtok(NULL, ",")) {
        if (e->count < 8) {
            e->sets[e->count++] = (uint8_t)strtoul(tok, NULL, 16);
        }
    }
}

int spawn_mask_load(const char *path, const Rom *rom, int quiet) {
    free(entries);
    entries = NULL;
    entry_count = 0;
    FILE *f = fopen(path, "r");
    if (!f) {
        if (!quiet) {
            fprintf(stderr, "spawn mask: cannot read %s\n", path);
        }
        return 0;
    }
    int capacity = 0, lineno = 0;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        lineno++;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        char bank[8], sets[64];
        unsigned address, x, y, code;
        if (sscanf(line, "%7s %x %x %x %x %63s", bank, &address, &x, &y, &code, sets) != 6 || strcmp(bank, "B6")) {
            fprintf(stderr, "spawn mask: line %d ignored (format)\n", lineno);
            continue;
        }
        uint16_t a = (uint16_t)address;
        if (rom_prg(rom, 6, a) != x || rom_prg(rom, 6, (uint16_t)(a + 1)) != y || rom_prg(rom, 6, (uint16_t)(a + 2)) != code) {
            fprintf(stderr, "spawn mask: line %d ignored (the ROM has other values at B6 %04X)\n", lineno, a);
            continue;
        }
        MaskEntry *e = NULL;
        for (int i = 0; i < entry_count; i++) {
            if (entries[i].address == a) {
                e = &entries[i];
            }
        }
        if (!e) {
            if (entry_count == capacity) {
                capacity = capacity ? capacity * 2 : 64;
                MaskEntry *grown = realloc(entries, (size_t)capacity * sizeof *entries);
                if (!grown) {
                    fclose(f);
                    return entry_count;
                }
                entries = grown;
            }
            e = &entries[entry_count++];
            memset(e, 0, sizeof *e);
            e->address = a;
        }
        add_sets(e, sets);
    }
    fclose(f);
    qsort(entries, (size_t)entry_count, sizeof *entries, compare);
    fprintf(stderr, "spawn mask: %d spawn(s) cut (%s)\n", entry_count, path);
    return entry_count;
}
