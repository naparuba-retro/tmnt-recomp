/* Spawn mask ("Whole Pizza" edition, see FIXES.md): a file written by the spawn editor (spawn_editor/data/spawn_mask.txt) lists enemy spawns of the game's
 * lists that are not to be created. Off by default, and off in the validation replays: without a file nothing changes (no RAM byte). Like the fixes, it is a
 * global, not part of `Game`, so the snapshots (F9) and the replays do not carry it.
 *
 * A line is "B6 <address> <x> <y> <code> <sprite sets>" (hex): the address, in the bank 6 in European addresses, of an entry (x, y, code) of a spawn list or of
 * a wave record, and the sprite sets (`WaveSpriteSet`, the value that picks the enemy table) it is cut for, or * for all. The signature (x, y, code) is checked
 * against the ROM that is loaded: a line that does not match is ignored with a warning. The bosses are never masked. */
#ifndef SPAWN_MASK_H
#define SPAWN_MASK_H

#include <stdint.h>
#include "game.h"

#define SPAWN_SKIPPED (-2) /* what spawn_enemy returns for a masked spawn (-1 = no room left) */

/* Reads the file; returns the number of lines kept (0 if the file is missing: an absent mask is an empty one; `quiet` = no message for that). */
int spawn_mask_load(const char *path, const Rom *rom, int quiet);
/* The mask that comes with the game (data/spawn_mask.txt next to the executable, or in the current folder, or one folder up): its path in `out`, or NULL. */
const char *spawn_mask_default_path(const char *exe_dir, char *out, unsigned size);
/* The mask is loaded at start and switched on or off while playing (key S): off, the game is the original; on, the spawns of the file are not created.
 * It only acts on the spawns to come, it does not remove an enemy that is already there. */
void spawn_mask_set_enabled(int on);
int spawn_mask_enabled(void);
int spawn_mask_count(void);  /* lines kept */
int spawn_mask_active(void); /* enabled and at least one line kept */
int spawn_masked(uint16_t address, uint8_t sprite_set);

#endif
