# Fixes ("Whole Pizza" edition)

The port plays the original game exactly: it replays the reference TAS 1:1. The fixes below are corrections of the original game, **off by default**. Each one
has a flag in the code (`src/fixes.h`), a sheet here with its evidence, and its effect on the TAS. With every flag off the port is bit-exact with the original
(the reference game and the TAS replay without any difference); that is checked by `tools/verify_all.sh`.

How to switch them on, for tests and demonstrations:

- in the game, the **J** key switches all the fixes on or off (the window title shows `FIXES ON`), or start with `--fixes`;
- in the replay tool, the environment variable `TMNT_FIXES=1` (`TMNT_FIXES=1 bash tools/verify_all.sh` shows where the replays leave the original's).

| Id | Fix | Evidence | Effect on the TAS |
|---|---|---|---|
| PIZZA-001 | Hitting a ceiling in a jump no longer stops the horizontal speed of the turtle | the code, and the original game itself (BizHawk) | the TAS leaves the original's at image 3921 |

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
