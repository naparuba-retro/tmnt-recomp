<img align="left" src="logo.jpg" alt="tmnt-recomp logo" width="128" height="140" hspace="16">

<h1>tmnt-recomp</h1>

A C port of the NES game **Teenage Mutant Ninja Turtles** (Konami, 1989), written function by function from the original 6502 code. It is not an
emulator: the game logic is real C (no 6502 interpreter); only the NES hardware (PPU, APU, mapper) is emulated, as much as the game needs.

<br clear="left">

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

## Download

The releases page has, for each version, `tmnt-<version>-windows-x64.zip` (contains `tmnt.exe`) and `tmnt-<version>-linux-x86_64.tar.gz` (contains the
executable `tmnt`, which needs the SDL2 library: `sudo apt-get install libsdl2-2.0-0`). Unpack, put your ROM in the `rom` folder next to the executable
(see below), start it. Every other build of the CI (`tmnt-dev-<commit>-...`) is only in the Actions tab, as an artifact of the workflow run.

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
| Esc | quit |

The first game controller found works too: d-pad or left stick, A = jump, X or B = attack, Start, Back = Select, left trigger = rewind.

Options: `--level2` ... `--level6` start directly in a level, `--invincible`, `--dump N name` (windowless: writes `name.bmp` and `name.wav` after N frames).

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
    tools/                 formatting and tests

Comments in the code give the address of the original 6502 routine each function comes from (for instance `B5 $8907` = bank 5, address `$8907`).

## License

MIT, see `LICENSE`. This license covers the code of this repository only.

## Disclaimer

This is an unofficial, non-commercial fan project. It is not affiliated with, endorsed by or sponsored by Konami, Nickelodeon, Paramount or any owner of the
Teenage Mutant Ninja Turtles trademarks and characters; all names and trademarks belong to their respective owners. The game's code, graphics, sounds and
music are not distributed here: the port needs a copy of the game that you own, and it contains no data extracted from it. Please do not post links to ROMs,
nor screenshots of the game, in issues or pull requests.
