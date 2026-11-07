# Fixes ("Whole Pizza" edition)

The port plays the original game exactly: it replays the reference TAS 1:1. The fixes below are corrections of the original game, **off by default**. Each one
has a flag in the code (`src/fixes.h`), a sheet here with its evidence, and its effect on the TAS. With every flag off the port is bit-exact with the original
(the reference game and the TAS replay without any difference); that is checked by `tools/verify_all.sh`.

How to switch them on, for tests and demonstrations:

- in the game, the **J** key switches the fixes on or off (the window title shows `FIXES ON`), or start with `--fixes`;
- the fixes of the water level (dam) are a group of their own: the **E** key (title: `WATER FIXES`), or `--water-fixes`; J does not touch them;
- the fix of the camera is a third group: the **C** key (title: `CAMERA FIX`), or `--camera-fix`;
- in the replay tool, the environment variable `TMNT_FIXES=1` (all of them; `TMNT_FIXES=1 bash tools/verify_all.sh` shows where the replays leave the original's) `TMNT_WATER_FIXES=1` (the water ones only) or `TMNT_CAMERA_FIX=1` (the camera one).

| Id | Fix | Evidence | Effect on the TAS |
|---|---|---|---|
| PIZZA-001 | Hitting a ceiling in a jump no longer stops the horizontal speed of the turtle | the code, and the original game itself (BizHawk) | the TAS leaves the original's at image 3921 |
| PIZZA-002 (water, key E) | The current of the water no longer leaves a speed that pushes the swimming turtle for ever | the code, the TAS (BizHawk), and Upsilandre's blog | the TAS leaves the original's at image 10854 |
| PIZZA-003 (water, key E) | The rotating bar of the dam has a box that matches its sprite | the code, a measure in the TAS, and Upsilandre's blog | none by itself (the TAS already leaves the original's at 10854) |
| PIZZA-004 (camera, key C) | The camera catches up with a turtle that is far to one side of the screen | the code, and a recorded game (`sessions/camera_midboss`) | none (the TAS never gets there with the fix off; the fix is off in the replays) |

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
walking. Nothing changes when the flag is off, nor between `$58` and `$A8`.

**Intention assumed.** A camera that follows the turtle is meant to show what is ahead; the stuck view is a consequence of the speed-only follow, not a rule.

**Evidence.** `bash tools/camera_check.sh` replays the recorded game `sessions/camera_midboss` (sewer of level 1, mid-boss killed, then the turtle stands at the left of
the screen) and counts the frames where the turtle is below `$40` (or above `$C0`) although the screen could scroll that way (`camera_far_from_player()`):

| Replay | Frames in the longest run | Total |
|---|---|---|
| fix off (the original) | 9485 (until the end of the record, frame 14120) | 9485 |
| fix on (`TMNT_CAMERA_FIX=1`) | 35 (the turtle is brought back, frame 4670) | 35 |

In the reference game (EU, played by a person) the longest run is 155 frames with the fix off.
