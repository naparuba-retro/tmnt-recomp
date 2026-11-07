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

/* Reads the file; returns the number of lines kept (0 with a warning if the file is missing). */
int spawn_mask_load(const char *path, const Rom *rom);
int spawn_mask_active(void); /* at least one line kept */
int spawn_masked(uint16_t address, uint8_t sprite_set);

#endif
