# Fixes ("Whole Pizza" edition)

The port plays the original game: it replays the reference TAS 1:1 (see TECHNICAL.md for what is and is not covered by that). The entries below are in two groups:

- **Fixes** (PIZZA-001 to 004): corrections of flaws of the original, accepted only with evidence: the code, an inconsistency inside the game, or a serious outside source.
- **Fairness and balance** (PIZZA-005 to 007): suggestions of the author. They change the difficulty, they do not claim to restore what the designers meant, and they are kept apart on purpose.

All of them are **off by default**. Each one
has a flag in the code (`src/fixes.h`), a sheet here with its evidence, and its effect on the TAS. With every flag off the port is bit-exact with the original
(the reference game and the TAS replay without any difference); that is checked by `tools/verify_all.sh`.

How to switch them on, for tests and demonstrations:

- in the game, the **J** key switches the fixes on or off (the window title shows `FIXES ON`), or start with `--fixes`;
- the fixes of the water level (dam) are a group of their own: the **E** key (title: `WATER FIXES`), or `--water-fixes`; J does not touch them;
- the fix of the camera is a third group: the **C** key (title: `CAMERA FIX`), or `--camera-fix`;
- the **spawn mask** (PIZZA-005) is loaded at start from `data/spawn_mask.txt` (an absent file is an empty mask): the **S** key switches it on or off (title: `SPAWN MASK (S)`), or start with `--spawn-mask` (on from the start; with a file name, that file instead of the shipped one), or `TMNT_SPAWN_MASK=<file>` in the replay tool;
- the balance options (PIZZA-006 and PIZZA-007) are a fourth group: the **R** key (title: `BALANCE (R)`), or `--balance`, or `TMNT_BALANCE=1` in the replay tool;
- in the replay tool, the environment variable `TMNT_FIXES=1` (all of them; `TMNT_FIXES=1 bash tools/verify_all.sh` shows where the replays leave the original's) `TMNT_WATER_FIXES=1` (the water ones only) or `TMNT_CAMERA_FIX=1` (the camera one).

### Fixes

| Id | Fix | Evidence | Effect on the TAS |
|---|---|---|---|
| PIZZA-001 | Hitting a ceiling in a jump no longer stops the horizontal speed of the turtle | the code, and the original game itself (BizHawk) | the TAS leaves the original's at image 3921 |
| PIZZA-002 (water, key E) | The current of the water no longer leaves a speed that pushes the swimming turtle for ever | the code, the TAS (BizHawk), and Upsilandre's blog | the TAS leaves the original's at image 10854 |
| PIZZA-003 (water, key E) | The rotating bar of the dam has a box that matches its sprite | the code, a measure in the TAS, and Upsilandre's blog | none by itself (the TAS already leaves the original's at 10854) |
| PIZZA-004 (camera, key C) | The camera catches up with a turtle that is far to one side of the screen | the code, and a recorded game (`sessions/camera_midboss`) | none (the TAS never gets there with the fix off; the fix is off in the replays) |
### Fairness and balance (suggestions of the author)

| Id | Option | Evidence | Effect on the TAS |
|---|---|---|---|
| PIZZA-005 (spawn mask, key S) | Enemy spawns listed in a file written by the spawn editor (`spawn_editor/`) are not created, one by one and per sprite set; the bosses never | the editor and `sessions/spawn_mask` (`bash tools/spawn_mask_check.sh`) | none (the option is off in the replays; with an empty or wrong mask the RAM is identical to the original's on 1916 frames) |
| PIZZA-006 (balance, key R) | The damage of a normal attack is raised for Leonardo (7 to 14), Raphael (9 to 20) and Michelangelo (7 to 14); Donatello stays at 15 | the damage table of the game (bank 6 `$9BCB`) and a measure on the four turtles (`bash tools/balance_check.sh`) | none (the option is off in the replays) |
| PIZZA-007 (balance, key R) | The hit box of Raphael's sai is 7 pixels wider each side: 35 pixels of reach in front of him instead of 28 | the hit box tables of the game (bank 6 `$8935`, `$8A43`) and a measure with dummy enemies (`bash tools/balance_check.sh`) | none (off in the replays) |
| PIZZA-008 (stay dead) | An enemy that the player defeats does not come back: not when leaving and re-entering a building or a sewer, not when the camera passes over its place again; a wave that is over stays over; a new game brings everything back | a recorded game (`bash tools/stay_dead_check.sh`) | none (off in the replays) |

## PIZZA-001: a ceiling no longer stops the turtle's horizontal speed

**Problem.** When a turtle jumps and hits a ceiling with its head, it loses its horizontal speed for a few frames, and so goes much less far sideways than
the player expects.

**Cause (B4 `$9FB0`-`$A0BD`, `src/player/side_move.c`).** The turtle is tested against the collision map at points around it, not with a box. After the
ceiling probe at (0, -12) has stopped the vertical speed, the lateral test ($A0EA to the right, $A106 to the left) uses three probes; the first one, at
(+6, -12) to the right or (-6, -12) to the left, is **at the same height as the ceiling probe**. When the head is inside a blocking tile, that probe reads
the same row of tiles, takes it for a wall and stops the horizontal speed. The test is done again at every frame, so the speed stays stopped as long as the
head is in the row of the ceiling.

**Fix.** In the air, when the head probe (0, -12) is inside a blocking tile, the lateral head probe that is inside a blocking tile as well is left out of
the lateral test. A wall beside the head with nothing above it still blocks, and so do the probes at the body and the feet. Nothing changes when the flag
is off, and nothing changes on the ground.

**Intention assumed.** The head-height lateral probes are there to stop the turtle against a wall that reaches its head; that they also fire under a ceiling
looks like an accident of two probes sharing a height, not a rule. No external source says so yet.

**Evidence.** The behaviour exists in the original: in the reference TAS (US ROM, BizHawk), at images 3921 to 3924 the turtle, moving left in a jump, hits the
ceiling (vertical speed 250 then 0) and its horizontal speed (`$04E0`) is 0 for four frames, then 253 (-3) again. The port without the fix reproduces it
exactly; with the fix the speed stays 253:

| Image | BizHawk (original) | port, fix off | port, fix on |
|---|---|---|---|
| 3920 | vy 250, vx 0 | vy 250, vx 0 | vy 250, vx 0 |
| 3921 | vy 0, vx 0 | vy 0, vx 0 | vy 0, **vx 253** |
| 3922-3924 | vy 0, vx 0 | vy 0, vx 0 | vy 0, **vx 253** |
| 3925 | vy 0, vx 253 | vy 0, vx 253 | vy 0, vx 253 |

(`vy` = `$04B0`, `vx` = `$04E0`; in this part the screen scrolls, so the movement shows in the camera and not in the turtle's X.)

**Measure.** The jump of the TAS at image 3921 was replayed 11 times with the same inputs, the trajectory moved by -5 to +5 pixels (a poke of the turtle's Y
at image 3919), so that the head enters the ceiling by 1 to 6 pixels, which is what a jump does. Frames without horizontal speed in the air, and the
position (camera + X) at image 3950, the turtle starting at 322 and moving left:

| Trajectory shift | Frames stopped, original | Frames stopped, fixed | Position, original | Position, fixed |
|---|---|---|---|---|
| -5 | 22 | 0 | 320 | 293 |
| -3 | 12 | 0 | 306 | 293 |
| 0 (the TAS) | 4 | 0 | 297 | 293 |
| +2 | 10 | 0 | 304 | 293 |
| +5 | 6 | 2 (1) | 300 | 295 |
| mean of the 11 | 11.3 | 0.4 (1) | 305.6 | 293.4 |

With the fix the turtle goes **12 pixels farther on average** in these 29 frames (28.6 pixels travelled instead of 16.4), up to 27 pixels farther. (1) The 2
frames of the +4 and +5 lines are the first frames of the jump, before the horizontal speed is set; they are the same for every variant. The script is
`tools/ceiling_measure.py` of the development repository (it needs the TAS files, which are not in this repository).

**Why not simply move the lateral head probes down by a few pixels?** It was measured too. The ceiling is reached by 1 to 6 pixels, so a probe moved down by
`s` pixels only helps when the head is in by `s` pixels or less: with `s` = 1, 1 of the 11 trajectories is cured (the one of the TAS, by chance); `s` = 2, 2
of them; `s` = 4, 4 of them; it takes 6 pixels or more to cure all. But then the probe also decides differently where there is no ceiling, which the fix
never does. Counting, over the whole TAS (6103 lateral tests in the air, 173 with the head in a ceiling tile) and over the whole European reference game
(14760 tests, 311 under a ceiling), the frames where the wall decision differs from the original's away from a ceiling:

| Variant | TAS: under a ceiling / elsewhere | Reference game: under a ceiling / elsewhere |
|---|---|---|
| the fix | 142 / **0** | 286 / **0** |
| probe 1 pixel lower | 101 / 6 | 149 / 26 |
| 2 pixels | 107 / 15 | 217 / 44 |
| 4 pixels | 118 / 33 | 286 / 102 |
| 6 pixels | 136 / 47 | 286 / 134 |
| 8 pixels | 138 / 63 | 286 / 178 |

A moved probe changes walls beside the head with nothing above them (tens to hundreds of frames), and it still misses some ceilings; the fix only touches the
frames where the head is inside a blocking tile.

**Effect on the TAS.** With the fix on, the replay leaves the original's at image 3921 (US ROM) and image 3402 (European reference game), and the runs differ
from there on. With the fix off, nothing changes.

## PIZZA-002: the current of the water no longer pushes the turtle for ever (water group, key E)

**Problem.** In the swimming zone of the dam, a water current that the turtle has left goes on pushing it sideways, even with the d-pad released, until it
touches the ground. This is what makes the first seaweed field so hard: the corridors are 32 pixels wide, the turtle's box is wide, and the current that is
set on the screen before (when the bomb in the right corner is approached) is still pushing in a field that has no current (its bubbles rise straight).

**Cause (B4 `$A78B`, `src/player/swim.c`).** Every frame the horizontal speed `$04E0` is set from the d-pad (a table, whole pixels only). A current adds its
strength to the **fraction** of the speed, `$04F0`, which the d-pad never touches. When the turtle leaves the current, the fraction stays as it was, and as
it is added to the position at every frame the turtle keeps drifting (a fraction of $F0 is almost one pixel per frame).

**Fix.** When there is no current (`$7E` is 0), the fraction `$04F0` is set to 0 in the swimming state. Inside a current nothing changes.

**Intention assumed.** The zone of the seaweed is marked "no current", and nothing in the code resets the fraction: the drift looks like an oversight of
the same kind as the one that Upsilandre describes on his blog ("Tous les secrets de Teenage Mutant Ninja Turtles", 2022), where the lateral current is a
side effect of the bomb, not a rule.

**Evidence.** In the reference TAS (US ROM), sampled while swimming (two frames out of thirty), with no current (`$7E` = 0) the fraction `$04F0` was not 0 in
26 of the 98 samples (it stays at `$F0`), and with the current 41 it goes through `$F0`, `$E0`, `$C0`, `$B0`, `$A0`, `$80`, `$70`: the speed left by the current. With
the fix (`TMNT_WATER_FIXES=1`) the TAS leaves the original's at image 10854, in the dam.

## PIZZA-003: the rotating bar of the dam has a box that matches its sprite (water group, key E)

**Problem.** The rotating bar of the dam (an anneau at the centre and small discs turning around it) is dangerous by contact, but its hit boxes hang above the
sprites: about ten pixels too high, as Upsilandre notes on his blog.

**Cause (B6 `$9B90`-`$9BC6`, `src/stage_play/collisions.c`).** The Y of an entity is taken as its feet: the box goes from Y - h to Y - 2, except for the types of
a short table ($9BC6: `$23`-`$26` and `$89`) where the box goes to Y + h. The bar's elements (types `$1B` to `$1E`, animations `$57` and `$58`) are not in it, but their
Y is the centre of the sprite. Measured in the TAS (US ROM), at image 10940: the ring at (213, 88) has the box lines 75 to 86 while its sprite covers about 80 to 96; a disc at (200, 72) has
the lines 60 to 70 for a sprite centred on 72. The boxes are tall, narrow and above the sprites (see the H display).

**Fix.** For these elements, the box is a square with a half side of the half width minus 1 (12 pixels for the ring, 6 for the discs), a little
smaller than the sprite. It is centred on Y for the ring; the discs are drawn in the lower half of their 8x16 sprite, so their box is centred 4 pixels below Y, so that a touch must be a real touch. The H display draws the same box (checked against the game's: 0 difference on 16 673 boxes
of the TAS).

**Intention assumed.** The Y of these elements is their centre (their sprites are centred on it), and no box was meant to hang 8 pixels above them.

## PIZZA-004: the camera catches up with the turtle (camera group, key C)

**Problem.** After a mid-boss of a sewer (side view) has been killed and the screen is released, the camera does not come back to the turtle: if the turtle has
gone far to one side while the screen was held, it stays there, the camera only follows at the turtle's pace, and the view ahead is nearly empty.

**Cause (B4 `$9851`-`$98A0`, `src/player/scroll.c`).** In side view the screen scrolls only when the turtle walks: to the left once it is below `$60`, to the right once it
is above `$A0`, and then at the speed of the turtle (4 pixels per frame at most): the camera keeps the distance, it never reduces it. While a wave or a mid-boss
holds the screen (`$5E` is not 0) there is no scroll at all, so the turtle can walk anywhere in the screen; once released, nothing brings it back, and a turtle that
stands still never makes the screen move.

**Fix.** When the turtle is below `$58` (or above `$A8`), the screen could scroll that way, and the camera is not held, the camera is moved 2 pixels per frame more than
the turtle walks, towards it, and the turtle is moved on the screen by what the camera has moved: it comes back to `$58` (or `$A8`) in a few frames, standing still or
walking, at 4 pixels per frame at most (the original's own limit: above it the scroll's column drawing, B7 `$D7D6`, skips a column and leaves a stale tile in the
floor; `bash tools/scroll_columns_check.sh` checks it on `sessions/camera_tunnel`). Nothing changes when the flag is off, nor between `$58` and `$A8`.

**Intention assumed.** A camera that follows the turtle is meant to show what is ahead; the stuck view is a consequence of the speed-only follow, not a rule.

**Evidence.** `bash tools/camera_check.sh` replays the recorded game `sessions/camera_midboss` (sewer of level 1, mid-boss killed, then the turtle stands at the left of
the screen) and counts the frames where the turtle is below `$40` (or above `$C0`) although the screen could scroll that way (`camera_far_from_player()`):

| Replay | Frames in the longest run | Total |
|---|---|---|
| fix off (the original) | 9485 (until the end of the record, frame 14120) | 9485 |
| fix on (`TMNT_CAMERA_FIX=1`) | 35 (the turtle is brought back, frame 4670) | 35 |

In the reference game (EU, played by a person) the longest run is 155 frames with the fix off.

## PIZZA-005: the spawn mask (option `--spawn-mask`)

**What.** A fairer game: the enemies that are placed in a sneaky way are not created. They are listed in `data/spawn_mask.txt`, which the spawn editor (`tools/spawn_editor/`, `python run.py`) writes: one cuts spawns of the enemy lists there, variant by variant.
The mask is on with the **S** key of the game or the menu's line (or `--spawn-mask <file>`, or `TMNT_SPAWN_MASK=<file>` in the replay tool); the window title shows `SPAWN MASK`.

**How (`src/spawn_mask.c`, hook at the top of `spawn_at`, `src/enemies/spawning.c`).** A line gives the address of an entry of a list (bank 6, European
addresses, the same in the US ROM), its signature (x, y, code) and the sprite sets it is cut for (`*` = all). The signature is checked against the ROM at load:
a line that does not match is ignored with a warning. The test is after the wave logic of `spawn_enemy`, which runs as before: only the enemy is missing. The
bosses (types `$70`-`$7F`) are never cut. `spawn_initial_side` goes on to the next entry when a spawn is cut (`SPAWN_SKIPPED`, distinct from "no room left").

**Delivery.** `port/data/spawn_mask.txt` is the mask of the Whole Pizza edition: the folder `data/` goes with the executable (like `rom/`). The spawn editor (`tools/spawn_editor/`, `python run.py`) writes it, with its source `data/spawn_mask.json`, at each save.

**Effect.** Off by default (key S or `--spawn-mask` to switch it on) and off in the replays. Switching it on only acts on the spawns to come: an enemy already on the screen stays and in `tools/verify_all.sh`. A wave whose enemies are all cut ends at once (tested on the three waves of level 1).

## PIZZA-006: the damage of the normal attacks (balance group, key R)

**Problem.** The four turtles do not hit alike: a normal attack does 7 (Leonardo), 9 (Raphael), 7 (Michelangelo) or 15 (Donatello) damage, read in the table at bank 6
`$9BCB` (two bytes per turtle: normal, and a little more when the turtle's health is below half). Donatello kills most enemies in half the hits, the others in
about twice as many, and the jump attack (240 for all) is far above any of them.

**Fix.** `enemy_hit_by_weapon` (`src/enemies/contact.c`) reads 14 / 17 (Leonardo), 20 / 20 (Raphael), 14 / 19 (Michelangelo) and the original 15 / 16 (Donatello); the second
value is the one at low health, with the bonus of the original (+3, 0, +5, +1). The jump attack, the sub-weapons and the special weapons are not changed.

**Intention assumed.** This one is a balance choice, not a correction: it is a suggestion of the project (the author), which is why it is in a group of its own.

**Evidence.** `bash tools/balance_check.sh` hits a dummy enemy with each turtle and measures 7 / 9 / 7 / 15 without the option and 14 / 20 / 14 / 15 with it.

## PIZZA-007: Raphael's reach (balance group, key R)

**Problem.** Raphael's sai reaches 28 pixels in front of him (the right side of its hit box, 5 pixels from its centre, at the best frame of the thrust), against 39 to 54
for the others. Its box is 11 by 17 pixels.

**Cause.** The half width of the weapon box comes from the entry 9 of the hit box table (bank 6 `$8A43`), shared by the three attacks of Raphael (animations `$A5`, `$A6`, `$A7`:
up, straight, down).

**Fix.** The box is computed with a half width of 12 instead of 5 for these three animations when Raphael is the turtle (`entity_hitbox_raw`, `src/stage_play/collisions.c`, and the hit
box view, `src/hitbox_view.c`): 35 pixels of reach straight (28 up, 35 down instead of 21 and 28), a box of 25 by 17 pixels, which also reaches 7 pixels more behind him.

**Evidence.** `bash tools/balance_check.sh`: the farthest dummy enemy that is hit is at 31 pixels without the option and 38 with it; the hit box view stays equal to the game's formula.

## PIZZA-008: the enemies that are defeated stay dead (menu option "Stay dead")

**Problem.** The game forgets everything at every door: going into a building or a sewer empties all the entities and resets the masks of the zone, and the spawn lists of the ROM have no
memory (an entry spawns again each time the camera passes over it). A building that was cleaned is full again as soon as the player comes back.

**Fix.** `src/persist.c`. One bit per entry of the spawn lists (bank 6) remembers the entries whose enemy was defeated by the player (a blow, or the van), and `spawn_at` skips them
(the same "skipped" answer as the spawn mask). A table tells for every entity the entry that created it (filled at the spawn, cleared by `clear_entity`). The waves are kept per level and
zone (`waves_done`), and restored at the door. The bosses keep their own memory (`WaveDoneMask`). The data is in `Game`, outside the NES RAM: the F9 snapshots restore it and the comparisons
with the emulators do not see it. A new game (the title screen, or PLAY in the menu) brings every enemy back.

**Not kept.** An enemy that is hurt but alive, one that only went out of the screen, the projectiles, the pickups (they come back as before).

**Evidence.** `bash tools/stay_dead_check.sh` replays the reference game with the option on: enemies are remembered and later spawns are skipped; the replays without the option are unchanged.
