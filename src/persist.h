/* Enemies that stay dead ("Whole Pizza" edition, see FIXES.md PIZZA-008): an enemy defeated by the player does not come back, neither when the player leaves a building or a
 * sewer and comes in again, nor when the camera passes over its place again. The memory is one bit per entry of the spawn lists of the ROM (bank 6, $8000-$BFFF), and a
 * table that tells, for every entity, the entry that created it. Both live in `Game` (outside the NES RAM): the snapshots (F9) restore them, and the comparisons of the RAM
 * with the emulators do not see them. Off by default and in the replay tools: the game then plays exactly like the original. */
#ifndef PERSIST_H
#define PERSIST_H

#include <stdint.h>
#include "game.h"

void persist_set_enabled(int on);
int persist_enabled(void);
void persist_reset(Game *g); /* a new game: every enemy comes back */
int persist_is_dead(const Game *g, uint16_t entry);
void persist_note_spawn(Game *g, int x, uint16_t entry); /* entity x was created by this entry of a list (0 = by none) */
void persist_note_death(Game *g, int x);                 /* the player defeated entity x (call it before the enemy turns into a corpse) */

void persist_note_wave(Game *g, uint8_t mask); /* a wave of the current zone is over (the bit of WaveTriggersUsed) */
uint8_t persist_waves_over(const Game *g);     /* the waves of the current zone that are over (0 when the option is off) */
void persist_report(void);                     /* tests: prints the counts to stderr */

#endif
