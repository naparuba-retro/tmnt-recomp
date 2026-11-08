<img align="left" src="logo.jpg" alt="tmnt-recomp logo" width="128" height="140" hspace="16">

<h1>tmnt-recomp</h1>

A C port of the NES game **Teenage Mutant Ninja Turtles** (Konami, 1989), written function by function from the original 6502 code.

<br clear="left">

It is not an emulator: the game logic is real C, only the NES hardware (picture, sound) is emulated. It plays the original game **exactly** (checked frame by frame against emulators, including a 15 minute speed run), and adds an in-game menu, a few optional fixes and display options.

> **Note on how this was made.** I was heavily helped by Claude (an AI assistant from Anthropic) for the decompilation part: it analyzed the game and
> wrote the specifications that made a reimplementation possible afterwards. I then did a manual pass to reorganize the whole thing a bit. So this is not
> 100% human-made code.

**The ROM is not included.** You need your own copy of the game, the European (PAL) or the American (USA) release, as a `.nes` file.

<p><img src="images/menu_play_whole_pizza.png" alt="The menu" width="70%"></p>

## Play

1. Download the archive of the latest release (the **Releases** page): `tmnt-<version>-windows-x64.zip` or `tmnt-<version>-linux-x86_64.tar.gz`
   (Linux needs SDL2: `sudo apt-get install libsdl2-2.0-0`).
2. Unpack it. Put your ROM (any file name) in the `rom` folder next to the game.
3. Start `tmnt` (`tmnt.exe` on Windows). If the ROM is missing, the game tells you where to put it.
4. In the menu, pick an edition and a level, then **PLAY**.

### Controls

| Keyboard | Game pad |
|---|---|
| Arrows | d-pad or left stick |
| X = jump (A), Z or W = attack (B) | A = jump, X or B = attack |
| Enter = Start | Start |
| Tab or right Shift = Select (weapon, van) | Y |
| Esc = menu | View / Back |
| P pause, M mute, F11 fullscreen, F5 restart, F9 rewind | left trigger = rewind |

The keys can be changed in the menu (CONTROLS page).

### The menu

Esc opens it (it is also the launcher at start). Mouse, keyboard and pad all work.

- **PLAY**: choose the edition, the extras and the level, then play or resume.
  **Original** is the game exactly as it was in 1989. **Whole Pizza** switches on a set of fixes and options (see [FIXES.md](FIXES.md)); click a tick to switch one off. The edition that is not chosen is shown in grey (in colour again when you point at it).
- **SOUND**: volume, mute.
- **GRAPHICS**: large view, no flicker, hitbox view, fullscreen, integer scaling, show FPS, and a **CRT filter** (scanlines, phosphor mask, curved screen).
- **CONTROLS**: change the keyboard keys, see the pad.

<p>
<img src="images/menu_graphics.png" alt="The GRAPHICS page" width="49%">
<img src="images/menu_controls.png" alt="The CONTROLS page" width="49%">
</p>

What you change in the menu is saved in `tmnt.ini`, next to the game. Delete it to get the defaults back.

### Command line (optional)

`tmnt [rom.nes] [options]`: `--no-menu` (start the game at once), `--level2` ... `--level6` (start in a level), `--invincible`, `--fixes` (the fixes on from the start), `--blur-logo` (blur the game's logo shown in the menu, for screenshots).
The other options are in [TECHNICAL.md](TECHNICAL.md).

## Steam Deck

The Linux build runs on a Steam Deck: unpack it (for instance in `/home/deck/Games/TMNT`), put your ROM in `rom`, add `tmnt` as a Non-Steam Game and start it from **Game Mode**.
(In Desktop Mode, Steam turns the controller into a keyboard, so B opens the menu: use the keyboard there.)

## Build

    python build.py               # Windows (Visual Studio Build Tools, Python 3)
    python3 build.py              # Linux (gcc, SDL2: sudo apt-get install build-essential libsdl2-dev pkg-config)

It produces `build/tmnt` (the game) and `build/tmnt_replay` (a windowless tool used by the tests). More in [TECHNICAL.md](TECHNICAL.md): how it is checked, the tests, the sources.

## License

MIT, see `LICENSE`. It covers the code of this repository only. Third-party files of the menu: `stb_truetype.h` and `stb_image.h` (Sean Barrett, public domain / MIT) in `src/third_party/`,
and the font Komika by Apostrophic Labs in `data/fonts/` (freeware, its licence is next to it).

## Disclaimer

This is an unofficial, non-commercial fan project. It is not affiliated with, endorsed by or sponsored by Konami, Nickelodeon, Paramount or any owner of the
Teenage Mutant Ninja Turtles trademarks and characters; all names and trademarks belong to their respective owners. The game's code, graphics, sounds and
music are not distributed here: the port needs a copy of the game that you own, and it contains no data extracted from it. Please do not post links to ROMs,
nor screenshots of the game, in issues or pull requests. The screenshots of the menu in `images/` show none of it (the logo that the menu cuts from the ROM is blurred).
