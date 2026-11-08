<img align="left" src="logo.jpg" alt="tmnt-recomp logo" width="128" height="140" hspace="16">

<h1>tmnt-recomp</h1>

A C port of the NES game **Teenage Mutant Ninja Turtles** (Konami, 1989), written function by function from the original 6502 code.

<br clear="left">

It is not an emulator: the game logic is real C (no 6502 interpreter); only the NES hardware (PPU, APU, mapper) is emulated, as much as the game needs.

> **Note on how this was made.** I was heavily helped by Claude (an AI assistant from Anthropic) for the decompilation part: it analyzed the game and
> wrote the specifications that made a reimplementation possible afterwards. I then did a manual pass to reorganize the whole thing a bit. So this is not
> 100% human-made code.

The port reads the original ROM at run time and reproduces it exactly: the whole game is checked frame by frame against emulators (Mesen, BizHawk),
for the European and the American releases.

**The ROM is not included.** You need your own copy of the game, the European (PAL) or the American (USA) release, as a `.nes` file.

## Status

- European and American ROMs are both supported (the American code is read through an address translation, see `src/rom.c`).
- **Replays the reference TAS 1:1.** The port plays the tool-assisted speedrun
  [NES Teenage Mutant Ninja Turtles by DreamYao in 15:36.19](https://tasvideos.org/4543M) (TASVideos movie #4543, American ROM, levels 1 to 6) with the
  inputs recorded by BizHawk: 55 530 logical frames, and the console RAM matches the BizHawk run frame by frame, with no divergence. This comparison is
  made against memory dumps of BizHawk that are not part of this repository (the TAS itself is not distributed here, only linked).
- Also verified identical to the original: a complete game played from the title screen to the end screen (127 500 frames, European ROM), see `tests/`.
- Not everything is ported: the paths that none of these runs take still print `TODO ...` lines in the debug log instead of doing anything.
- Randomness is a model (same law as the console, different sequence): a game does not unfold exactly as the same game on the console.

## TODO

- One of the next steps is a "Definitive Edition" of the port, with the fixes reported by the brilliant Upsilandre on his blog:
  [Tous les secrets de Teenage Mutant Ninja Turtles](https://upsilandre.over-blog.com/2022/06/tous-les-secrets-de-teenage-mutant-ninja-turtles.html).

## Download

The releases page has, for each version, `tmnt-<version>-windows-x64.zip` (contains `tmnt.exe` and the `data` folder with `spawn_mask.txt`) and `tmnt-<version>-linux-x86_64.tar.gz` (contains the
executable `tmnt`, which needs the SDL2 library: `sudo apt-get install libsdl2-2.0-0`). Unpack, put your ROM in the `rom` folder that is already next to the
executable (see below), start it. Every other build of the CI (`tmnt-dev-<commit>-...`) is only in the Actions tab, as an artifact of the workflow run.

A release is made by tagging a commit: `git tag v1.0.0 && git push origin v1.0.0`.

## Build

Windows (Visual Studio Build Tools, Python 3):

    python build.py

Linux (gcc, SDL2, Python 3):

    sudo apt-get install build-essential libsdl2-dev pkg-config
    python3 build.py

This produces `build/tmnt` (the game) and `build/tmnt_replay` (a windowless replay tool used by the tests).
`python build.py --replay-only` builds the replay tool without SDL2, `--clean` / `--rebuild` clean the build folder.

## Run

Put your ROM (any file name) in a folder named `rom` next to the executable and start `tmnt`. If no ROM is found, the game creates the `rom` folder and
tells you where to put the file. The ROM can also be given as an argument: `tmnt path/to/rom.nes`. The European release is preferred when both are in `rom`.

| Key | Action |
|---|---|
| Arrows | d-pad |
| Z or W | B (attack) |
| X | A (jump) |
| Enter | Start |
| Tab / right Shift | Select (change weapon, leave the van) |
| M / P | sound off / pause |
| F5 / F9 / F10 | restart / rewind (a snapshot every 10 s) / debug capture |
| I | invincibility cheat |
| H | show / hide the hit boxes (see below) |
| J | switch the fixes on / off (see `FIXES.md`) |
| E | switch the water fixes (dam level) on / off (see `FIXES.md`) |
| C | switch the camera fix on / off: the camera catches up with the turtle (see `FIXES.md`) |
| S | switch the spawn mask on / off: the enemy spawns cut by `data/spawn_mask.txt` (the Whole Pizza edition; only the spawns to come, see `FIXES.md`) |
| R | switch the balance options on / off: more damage for Leonardo, Raphael and Michelangelo and a longer reach for Raphael (see `FIXES.md`) |
| F | remove the sprite flicker (see below) |
| L | large view: no random tiles at the top of the screen (see below) |
| Esc | the menu: PLAY (edition, extras, level), SOUND, GRAPHICS, CONTROLS, and Quit game. Esc again goes back to the game. At start the menu is the launcher (`--no-menu` skips it) |

The first game controller found works too: d-pad or left stick, A = jump, X or B = attack, Start, Back = Select, left trigger = rewind.

Options: `--level2` ... `--level6` start directly in a level, `--invincible`, `--hitbox` (hit boxes shown from the start), `--fixes` (the fixes of `FIXES.md` on from the start), `--water-fixes` (the water ones), `--camera-fix` (the camera one), `--balance` (the balance options on from the start), `--spawn-mask [file]` (the spawn mask on from the start; alone, it uses the `data/spawn_mask.txt` that comes with the game, in a `data` folder next to the executable like the `rom` folder; see `FIXES.md`), `--no-flicker` (no sprite flicker from the start), `--large` (large view from the start), `--input file` (plays the pad
from a file of `first last mask` ranges instead of the keyboard), `--dump N name` (windowless: writes `name.bmp` and `name.wav` after N frames).

### Fixes

The port plays the original game. `FIXES.md` lists the corrections of the original that exist, each with its evidence and its effect on the TAS. They are off
by default; the J key (or `--fixes`) switches them on, and the window title shows when they are on.

The spawn mask (key S, `data/spawn_mask.txt`) is edited with the spawn editor, a small local web tool (`tools/spawn_editor/`, `python run.py`, needs Python 3 and Pillow, and
your own ROM in `rom/`): it draws the levels and the enemies from your ROM, so what it generates (`build/`) must never be published.

### Large view

In a level the game scrolls the picture 4 lines down, so the first 4 lines of the screen show the row above the map, which is not there: the PPU reads the
attribute table as if it were tiles (rows 30 and 31), and a band of random tiles appears at the top (grey blocks on the water, little boxes in a sewer). A
TV of the time hid those lines in its overscan, and the port shows them like Mesen does. The L key (or `--large`) replaces them, without cutting the
picture: with the row of the level that is really above the screen when the map has one (the game's own row drawing is run on a copy of the state to get
it, at the vertical scrolls of the buildings for instance), and with the backdrop colour (black in the sewers) when it has not (the top of the map, the
rooms seen from the side, the title screens). It changes only what is displayed: the game's memory, the replays and the validation are the same, and the
replay tool keeps the NES picture. About 98 % of the frames that have this band are of the second kind.
The same rebuild (the game's row drawing on a copy, read from the PPU scroll registers) also replaces what the video memory holds out of date: the three
first rows of the picture (the game scrolls first and draws the new row later; palettes arrive late), and the columns at the left and right edges, which the
game draws only from 8 pixels beyond the left edge: the mask of the 8 left pixels and the column of black sprites at the right, which hide them on a TV, are
not drawn in the large view.

### Sprite flicker

The NES draws at most 8 sprites per line, and the game changes at every frame which ones are kept, so that no character vanishes for good: on a crowded line,
parts of the characters blink. The port does the same, as the original does. The F key (or `--no-flicker`) draws all the sprites of a line instead. It
changes only what is displayed: the game's memory and the replays are the same, and the replay tool keeps the 8 per line. Over the reference game and the TAS,
the line limit drops sprites in about a quarter of the frames (counting the transparent parts of the sprites); the budget of 64 sprites per frame, which this
does not remove, is reached in less than 0.5 % of them.

### Hit boxes

The H key draws the boxes that the game's collision routine works with, over the picture (nothing in the game changes): green = the turtle's body,
yellow = its weapon, red = enemies and their shots, grey = an active entity that the game did not test this frame, white (thicker) = a pair that touches,
magenta = a box that wraps around the edge of the screen (the game compares such boxes in an odd way). The boxes are computed with the game's own formula
(and checked against the values the game computes each time it tests a collision, which it does every other frame) and drawn with the sprites they belong to.

The seaweed of the dam level is not an entity: it is read from the collision map of the level (one class per 16x16 tile) at a few points around the swimming
turtle. So the same display also tints the tiles of the map that hurt (orange: seaweed, -1 health block) or, in the swimming zone, kill (red: deadly seaweed),
and marks the points of the turtle where the map is read (cyan crosses): the game does not test the turtle against the scenery with a box but with such
points, so its "scenery box" is not its hit box (the green one, the box enemies hit). Seen from the side, there are points at the feet (a few pixels below
the bottom of the green box), the head and the sides; seen from above (the streets), only one point in the direction of the walk (two on each side for the
van). As the map is made of 16x16 tiles, the tinted area is a little larger than the drawing.
The same map gives the scenery: the outline of the solid parts (floors, walls, platforms, blocks) is drawn in a sober grey, and in a lighter grey the ladders
and the conveyor belts (the doors in the streets). Only the classes of the map whose meaning is known are drawn.

## Steam Deck

The Linux build runs on a Steam Deck (SteamOS already has SDL2, there is nothing to install):

1. In Desktop Mode, download the `linux-x86_64.tar.gz` of the release and unpack it, for instance in `/home/deck/Games/TMNT`.
2. Put your ROM in the `rom` folder that is next to the `tmnt` executable.
3. In Steam, "Add a Non-Steam Game" and pick the `tmnt` executable.
4. Start it from **Game Mode**: the Deck's controls then work as a game controller (d-pad or left stick, A = jump, X or B = attack, Y = Select (weapon, van), Start, View = the menu, LB and RB = the menu's pages,
   left trigger = rewind).

For now it works in **Game Mode only**. When it is started from Desktop Mode, Steam's desktop layout turns the controller into a keyboard (A becomes
Enter, B becomes Escape), so B opens the menu instead of attacking. Playing in Desktop Mode with the controller is not supported yet; the keyboard works there.

## Tests

- `bash tools/smoke_test.sh` : tests that need no ROM (usage, refusal of a wrong ROM, help screen when the ROM is missing). They run in the CI.
- `bash tools/ref_run_check.sh` : replays the reference game in `tests/reference_game` (inputs recorded from a complete game) and compares the screens, the
  palettes, the sound and the APU writes with the fingerprints (hashes) of the original trace. Needs the European ROM as `rom/tmnt_eu.nes`.
- `bash tools/format_c.sh --check` : formatting (clang-format, style in `.clang-format`; `bash tools/format_c.sh` fixes it).

## Layout

    build.py               builds everything (MSVC on Windows, gcc + SDL2 on Linux)
    src/                   the port (C11)
      frontend.c           the game executable: ROM search, 60 frames per second loop, snapshots, cheats
      platform_win.c       window, sound and pad on Windows (GDI, waveOut, XInput)
      platform_sdl.c       the same with SDL2 (Linux)
      replay_main.c        the windowless replay tool
      rom.c                the only way the game data is read (European or American release)
      ppu.c apu.c nmi.c    the NES hardware and the console's frame loop
      player/ enemies/     the player, the enemies and the bosses (one file per enemy or family)
      scenes/ stage_play/  cutscenes, level play, spawns, objects
      ram_map.h            names of the RAM variables
    tests/reference_game/  recorded inputs of the reference game and fingerprints of its original trace
    data/                  data that comes with the game: spawn_mask.txt, fonts/ and ui/ of the menu (read from the folder next to the executable)
    tools/                 formatting and tests; spawn_editor/ is the editor of the spawn mask

Comments in the code give the address of the original 6502 routine each function comes from (for instance `B5 $8907` = bank 5, address `$8907`).

## License

MIT, see `LICENSE`. This license covers the code of this repository only.

Third-party files that are part of the game's menu: `src/third_party/stb_truetype.h` and `stb_image.h` (Sean Barrett, public domain / MIT, the licence is at the end of each
file), and the font Komika by Apostrophic Labs in `data/fonts/` (freeware, used unmodified, its licence is `KOMIKA-LICENSE.txt` next to it). The menu's settings are kept in
`tmnt.ini`, next to the game (or in your user folder): only the values that differ from the defaults are written, and you can keep the file from one version to the next.

## Disclaimer

This is an unofficial, non-commercial fan project. It is not affiliated with, endorsed by or sponsored by Konami, Nickelodeon, Paramount or any owner of the
Teenage Mutant Ninja Turtles trademarks and characters; all names and trademarks belong to their respective owners. The game's code, graphics, sounds and
music are not distributed here: the port needs a copy of the game that you own, and it contains no data extracted from it. Please do not post links to ROMs,
nor screenshots of the game, in issues or pull requests.
