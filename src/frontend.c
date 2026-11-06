/* Player front-end (the executable `tmnt`): finds the ROM, runs the game at 60 frames per second and relays keys, picture and sound
 * through platform.h (Windows or SDL2). The pace comes from the sound card: 735 samples at 44100 Hz per frame.
 * Keys: arrows = d-pad, Z or W = B, X = A, Enter = Start, Tab or right Shift = Select, M = sound, P = pause, I = invincible,
 * H = hit boxes, J = the fixes of FIXES.md, F = no sprite flicker, L = large view, F5 = restart, F9 = rewind, F10 = capture, Esc = quit. */
#include <stdlib.h>
#include <string.h>
#include "fixes.h"
#include "game.h"
#include "help_screen.h"
#include "hitbox_view.h"
#include "large_view.h"
#include "nmi.h"
#include "platform.h"
#include "ppu.h"

#define TITLE "TMNT (Europe) - port C"

/* default Mesen palette (NES indices $00-$3F as 0xRRGGBB) */
static const uint32_t PAL[64] = {
    0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00, 0x333500, 0x0B4800, 0x005200, 0x004F08, 0x00404D,
    0x000000, 0x000000, 0x000000, 0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00, 0x6B6D00, 0x388700,
    0x0D9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000, 0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170,
    0xEA9E22, 0xBCBE00, 0x88D800, 0x5CE430, 0x45E082, 0x48CDDE, 0x4F4F4F, 0x000000, 0x000000, 0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF,
    0xFBC2FF, 0xFEC4EA, 0xFECCC5, 0xF7D8A5, 0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000,
};

static uint32_t screen[SCREEN_W * SCREEN_H];
static uint8_t idx[SCREEN_W * SCREEN_H];
static int paused, muted, frozen;
static int invincible; /* key I or --invincible: the turtles' health is set back to the maximum (cheat) */

/* snapshots: a full copy of the state (Game holds only RAM and registers) every 10 s of play, F9 goes back */
#define NSNAP 12
#define SNAP_EVERY 600
static Game snap[NSNAP];
static int snap_n, snap_head, snap_idx; /* number of snapshots, next slot, snapshot targeted by the next F9 */
static FILE *rewind_log;                /* tmnt_rewind_n3.txt: "frame snapshot-number" at each F9, to replay the game identically (tmnt_replay, argument 14) */

static void snap_save(Game *g) {
    snap[snap_head] = *g;
    snap_head = (snap_head + 1) % NSNAP;
    if (snap_n < NSNAP) {
        snap_n++;
    }
    snap_idx = 0;
}

static int snap_restore(Game *g) /* 1st press: the last snapshot; the following ones go back further */
{
    if (snap_idx >= snap_n) {
        return 0;
    }
    const Game *s = &snap[(snap_head - 1 - snap_idx + 2 * NSNAP) % NSNAP];
    if (rewind_log) {
        fprintf(rewind_log, "%u %d\n", g->frame, snap_idx);
        fflush(rewind_log);
    }
    Rom *rom = g->rom;
    FILE *tr = g->trace;
    uint32_t fr = g->frame;
    *g = *s;
    g->rom = rom;
    g->trace = tr;
    g->frame = fr;
    hitbox_clear(); /* the boxes recorded belong to the state that was left */
    snap_idx++;
    return 1;
}

static void render(Game *g) {
    large_view_prepare(g); /* key L: what the lines at the top of the screen show (display only) */
    ppu_render(g, idx);
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        screen[i] = PAL[idx[i] & 63];
    }
    hitbox_draw(g, screen); /* key H: drawn over the picture only, the game state is not touched */
}

static void put16(FILE *f, unsigned v) {
    fputc((int)(v & 0xFF), f);
    fputc((int)((v >> 8) & 0xFF), f);
}

static void put32(FILE *f, uint32_t v) {
    put16(f, v & 0xFFFF);
    put16(f, v >> 16);
}

/* 32-bit top-down BMP of the current picture (same bytes on every system) */
static void write_bmp(const char *name) {
    FILE *b = fopen(name, "wb");
    if (!b) {
        return;
    }
    uint32_t size = sizeof screen;
    fputc('B', b);
    fputc('M', b);
    put32(b, 54 + size);
    put32(b, 0);
    put32(b, 54);
    put32(b, 40);
    put32(b, SCREEN_W);
    put32(b, (uint32_t)-SCREEN_H);
    put16(b, 1);
    put16(b, 32);
    put32(b, 0);
    put32(b, size);
    put32(b, 0);
    put32(b, 0);
    put32(b, 0);
    put32(b, 0);
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        put32(b, screen[i]);
    }
    fclose(b);
}

/* F10: capture for debugging (BMP image + full game state) in tmnt_capture_N.* */
static void capture(Game *g) {
    static int n;
    char name[64];
    render(g);
    snprintf(name, sizeof name, "tmnt_capture_%d.bmp", ++n);
    write_bmp(name);
    snprintf(name, sizeof name, "tmnt_capture_%d.state", n);
    FILE *s = fopen(name, "wb");
    if (s) {
        fwrite(g, sizeof *g, 1, s);
        fclose(s);
    }
}

/* tmnt_input.txt: pad by ranges "first last mask" (same format as build/sc_*.txt, frames counted from the launch):
 * allows replaying the game in the port without a window (tmnt_replay) */
static FILE *input_log;
static FILE *poke_log; /* tmnt_pokes.txt: "frame address value", format of build/pk_*.txt (replay in the port) */
static int start_level2, level2_done;
static int start_level_n = 1; /* --level2: 1; --level3: 2; --level4: 3 (value of LevelNumber and AreaNumber of the street) */
static uint8_t input_mask;

/* --input file: plays the pad from a file of ranges "first last mask" (the format of tmnt_input.txt), frames counted from the launch, instead of the
 * keyboard. Handy to watch a recorded game, with the hit boxes for instance. */
typedef struct {
    uint32_t first, last;
    uint8_t mask;
} PadRange;
static PadRange *pad_script;
static int pad_script_n;

static void load_pad_script(const char *fn) {
    FILE *f = fopen(fn, "r");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", fn);
        exit(1);
    }
    unsigned a, b, m;
    while (fscanf(f, "%u %u %u", &a, &b, &m) == 3) {
        PadRange *p = realloc(pad_script, (size_t)(pad_script_n + 1) * sizeof *p);
        if (!p) {
            exit(1);
        }
        pad_script = p;
        pad_script[pad_script_n++] = (PadRange){a, b, (uint8_t)m};
    }
    fclose(f);
}

static uint8_t pad_script_at(uint32_t frame) {
    for (int i = 0; i < pad_script_n; i++) {
        if (frame >= pad_script[i].first && frame <= pad_script[i].last) {
            return pad_script[i].mask;
        }
    }
    return 0;
}
static uint32_t input_start;

static void record_input(Game *g) {
    if (!input_log) {
        return;
    }
    uint32_t f = g->frame - 1;
    if (g->pad == input_mask && f != 0) {
        return;
    }
    if (input_mask) {
        fprintf(input_log, "%u %u %u\n", input_start, f - 1, input_mask);
    }
    input_mask = g->pad;
    input_start = f;
    fflush(input_log);
}

static void reset_game(Game *g) {
    Rom *r = g->rom;
    FILE *tr = g->trace;
    memset(g, 0, sizeof *g);
    g->rom = r;
    g->trace = tr;
    game_init(g);
    hitbox_clear();
}

static void logic_frame(Game *g, int keyboard);

/* windowless test mode: tmnt --dump N output  -> output.bmp (last frame) and output.wav (N frames) */
static int dump_mode(Game *g, int frames, const char *base) {
    char name[512];
    static int16_t pcm[SAMPLES_PER_FRAME];
    snprintf(name, sizeof name, "%s.wav", base);
    FILE *w = fopen(name, "wb");
    if (!w) {
        return 1;
    }
    uint32_t bytes = (uint32_t)frames * SAMPLES_PER_FRAME * 2;
    fwrite("RIFF", 1, 4, w);
    put32(w, 36 + bytes);
    fwrite("WAVEfmt ", 1, 8, w);
    put32(w, 16);
    put16(w, 1);
    put16(w, 1);
    put32(w, SAMPLE_RATE);
    put32(w, SAMPLE_RATE * 2);
    put16(w, 2);
    put16(w, 16);
    fwrite("data", 1, 4, w);
    put32(w, bytes);
    g->frame = 0;
    for (int f = 1; f <= frames; f++) {
        logic_frame(g, 0);
        apu_mix(g, pcm, SAMPLES_PER_FRAME);
        for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
            put16(w, (uint16_t)pcm[i]);
        }
    }
    fclose(w);
    render(g);
    snprintf(name, sizeof name, "%s.bmp", base);
    write_bmp(name);
    return 0;
}

/* ROM search: the European (PAL) release wins over the American one, whatever the file names are. */
typedef struct {
    Rom *eu, *us;
} RomSearch;

static int on_nes_file(const char *path, void *user) {
    RomSearch *s = user;
    Rom *r = rom_load(path);
    if (r && rom_is_us(r)) {
        s->us = s->us ? s->us : r;
    } else if (r) {
        s->eu = s->eu ? s->eu : r;
    }
    return s->eu != NULL;
}

static Rom *find_rom_in(const char *dir) {
    RomSearch s = {0};
    platform_for_each_nes(dir, on_nes_file, &s);
    return s.eu ? s.eu : s.us;
}

/* ROM: the first argument that is not an option if there is one (no fallback, the reason of a refusal is shown); otherwise the first of
 * "rom" next to the executable, "rom" in the current folder, "../rom" next to the executable: the first that holds a known release.
 * NULL = error already shown. */
static Game *load_game(int argc, char **argv) {
    const char *rom = NULL;
    for (int i = 1; i < argc && !rom; i++) {
        if (strncmp(argv[i], "--", 2) && !(i >= 2 && !strcmp(argv[i - 1], "--dump")) && !(i >= 3 && !strcmp(argv[i - 2], "--dump")) &&
            !(i >= 2 && !strcmp(argv[i - 1], "--input"))) {
            rom = argv[i];
        }
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--allow-unknown-rom")) {
            rom_allow_unknown(1);
        }
    }
    Game *g = calloc(1, sizeof *g);
    if (!g) {
        return NULL;
    }
    if (rom) {
        g->rom = rom_load(rom);
        if (!g->rom) {
            platform_error_box(rom_error());
            return NULL;
        }
        return g;
    }
    char exe_dir[1024];
    platform_exe_dir(exe_dir, sizeof exe_dir);
    char dirs[3][1100];
    snprintf(dirs[0], sizeof dirs[0], "%s" PATH_SEP "rom", exe_dir);
    snprintf(dirs[1], sizeof dirs[1], "rom");
    snprintf(dirs[2], sizeof dirs[2], "%s" PATH_SEP ".." PATH_SEP "rom", exe_dir);
    for (int i = 0; i < 3 && !g->rom; i++) {
        g->rom = find_rom_in(dirs[i]);
    }
    if (!g->rom) {
        help_screen_rom_missing(dirs[0]); /* creates the folder and explains what to put in it */
        return NULL;
    }
    return g;
}

static void parse_options(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--level2")) {
            start_level2 = 1;
        }
        if (!strcmp(argv[i], "--level3")) {
            start_level2 = 1;
            start_level_n = 2;
        }
        if (!strcmp(argv[i], "--level4")) {
            start_level2 = 1;
            start_level_n = 3;
        }
        if (!strcmp(argv[i], "--level5")) {
            start_level2 = 1;
            start_level_n = 4;
        }
        if (!strcmp(argv[i], "--level6")) {
            start_level2 = 1;
            start_level_n = 5;
        }
        if (!strcmp(argv[i], "--invincible")) {
            invincible = 1;
        }
        if (!strcmp(argv[i], "--hitbox")) {
            hitbox_set_enabled(1);
        }
        if (!strcmp(argv[i], "--fixes")) {
            fix_set_all(1);
        }
        if (!strcmp(argv[i], "--no-flicker")) {
            ppu_set_sprite_limit(64);
        }
        if (!strcmp(argv[i], "--large")) {
            large_view_set_enabled(1);
        }
        if (!strcmp(argv[i], "--input") && i + 1 < argc) {
            load_pad_script(argv[i + 1]);
        }
    }
}

/* suffix of the log files: level 3 writes its own files, those of level 2 serve as the reference */
static const char *log_suffix(void) {
    return start_level_n == 2 ? "_n3" : start_level_n == 3 ? "_n4" : start_level_n == 4 ? "_n5" : start_level_n == 5 ? "_n6" : "";
}

static void open_logs(Game *g) {
    const char *sfx = log_suffix();
    char fn[64];
    snprintf(fn, sizeof fn, "tmnt_todo%s.log", sfx);
    g->trace = fopen(fn, "w"); /* what is left to port, reported by the "TODO ..." TRACEs */
    snprintf(fn, sizeof fn, "tmnt_input%s.txt", sfx);
    input_log = fopen(fn, "w");
    if (start_level2) {
        snprintf(fn, sizeof fn, "tmnt_pokes%s.txt", sfx);
        poke_log = fopen(fn, "w");
    }
    snprintf(fn, sizeof fn, "tmnt_rewind%s.txt", sfx);
    rewind_log = fopen(fn, "w");
    if (g->trace) {
        setvbuf(g->trace, NULL, _IOLBF, 4096);
    }
}

/* --level2 ... --level6: at the first frame of the street (level 1), pokes the RAM to jump to the wanted level */
static void apply_start_level(Game *g) {
    if (!(start_level2 && !level2_done && g->ram[GameMode] == 4 && g->ram[StageState] == 6 && !g->ram[SideViewFlag] && g->ram[LevelNumber] == 0)) {
        return;
    }
    level2_done = 1; /* level 2: LevelNumber 1, AreaNumber 1, top view, StageState 2 */
    const struct {
        uint16_t a;
        uint8_t v;
    } pk[4] = {{0x48, (uint8_t)start_level_n}, {0x5D, (uint8_t)(start_level_n == 5 ? 0 : start_level_n)}, {0x5C, (uint8_t)(start_level_n == 5)}, {0x34, 2}};
    for (int i = 0; i < 4; i++) {
        g->ram[pk[i].a] = pk[i].v;
        if (poke_log) {
            fprintf(poke_log, "%u %u %u\n", g->frame, pk[i].a, pk[i].v);
            fflush(poke_log);
        }
    }
}

static void invincible_cheat(Game *g) {
    if (!(invincible && (g->frame & 3) == 0 && g->ram[GameMode] == 4)) {
        return; /* cheat: health at the maximum every 4 frames, logged for the replay in Mesen */
    }
    if (!poke_log) {
        char pf[64];
        snprintf(pf, sizeof pf, "tmnt_pokes%s.txt", log_suffix());
        poke_log = fopen(pf, "a");
    }
    for (int t = 0; t < 4; t++) {
        g->ram[0x77 + t] = 0x80;
        if (poke_log) {
            fprintf(poke_log, "%u %u %u\n", g->frame, 0x77 + t, 0x80);
        }
    }
    if (poke_log) {
        fflush(poke_log);
    }
}

/* The logic of one frame, with the pad from the keyboard (or from --input), as in the game window; without a keyboard (--dump) the pad is only
 * the --input script. */
static void logic_frame(Game *g, int keyboard) {
    g->pad = pad_script_n ? pad_script_at(g->frame) : keyboard ? platform_pad() : 0;
    g->frame++;
    record_input(g);
    rng_idle(g);
    nmi_frame(g);
    hitbox_end_frame(g);
    apply_start_level(g);
    invincible_cheat(g);
}

/* One game frame: keys, logic, rendering, window refresh. */
static void run_game_frame(Game *g, int audio) {
    static int last_title_frame = -1;
    if (g->ram[GameMode] == 4 && g->frame % SNAP_EVERY == SNAP_EVERY - 1) {
        snap_save(g); /* end of the 1st frame of the series: stable state between two frames */
    }
    logic_frame(g, 1);
    if (g->left_attract && g->ram[GameMode] == 4) { /* level loaded, but the rest is not ported: the frame is kept */
        frozen = 1;
        apu_write(g, 0x15, 0);
        platform_set_title("TMNT (Europe) - C port: level 1 loaded, the rest is not ported (F5 = restart)");
    } else if (g->left_attract) { /* GameMode 5 and later: back to the licence screen */
        platform_set_title("TMNT (Europe) - C port: the game is not ported, back to the licence screen");
        reset_game(g);
        g->frame = 0;
    }
    render(g);
    platform_present(screen);
    if ((int)g->frame / 60 != last_title_frame && g->frame % 60 == 0) {
        char t[160];
        last_title_frame = (int)g->frame / 60;
        snprintf(t, sizeof t, "TMNT (Europe) - port C | mode %d.%d%s%s%s%s%s", g->ram[GameMode], g->ram[GameSubMode], muted ? " | muted" : "",
                 audio ? "" : " | no sound card", fix_any_enabled() ? " | FIXES ON (J)" : "", ppu_sprite_limit() > 8 ? " | NO FLICKER (F)" : "",
                 large_view_enabled() ? " | LARGE VIEW (L)" : "");
        platform_set_title(t);
    }
}

int main(int argc, char **argv) {
    Game *g = load_game(argc, argv);
    if (!g) {
        return 1;
    }
    reset_game(g);
    parse_options(argc, argv);
    open_logs(g);
    for (int i = 1; i + 2 < argc; i++) {
        if (!strcmp(argv[i], "--dump")) {
            return dump_mode(g, atoi(argv[i + 1]), argv[i + 2]);
        }
    }

    if (platform_init(invincible ? "TMNT (Europe) - port C - INVINCIBLE (I to disable)" : TITLE)) {
        return 1;
    }
    int audio = platform_audio_open();
    static int16_t pcm[SAMPLES_PER_FRAME];
    g->frame = 0;
    for (;;) {
        PlatformEvents ev;
        platform_poll(&ev);
        if (ev.quit) {
            break;
        }
        if (ev.mute) {
            muted ^= 1;
        }
        if (ev.pause) {
            paused ^= 1;
        }
        if (ev.hitbox) {
            hitbox_set_enabled(!hitbox_enabled());
        }
        if (ev.fix) { /* the fixes of FIXES.md, all together; the title shows the state at once */
            fix_toggle_all();
            platform_set_title(fix_any_enabled() ? "TMNT (Europe) - port C | FIXES ON (J to switch off)"
                                                 : "TMNT (Europe) - port C | fixes off (J to switch on)");
        }
        if (ev.large) { /* the lines at the top of the screen show the map's row above, or the backdrop, not random tiles */
            large_view_set_enabled(!large_view_enabled());
            platform_set_title(large_view_enabled() ? "TMNT (Europe) - port C | LARGE VIEW (L to switch off)"
                                                    : "TMNT (Europe) - port C | top of the screen as on the NES (L for the large view)");
        }
        if (ev.flicker) { /* all the sprites of a line are drawn, not only 8 (display only) */
            ppu_set_sprite_limit(ppu_sprite_limit() > 8 ? 8 : 64);
            platform_set_title(ppu_sprite_limit() > 8 ? "TMNT (Europe) - port C | NO FLICKER (F to switch off)"
                                                      : "TMNT (Europe) - port C | flicker as on the NES (F to remove it)");
        }
        if (ev.invincible) {
            invincible ^= 1;
            platform_set_title(invincible ? "TMNT (Europe) - port C - INVINCIBLE (I to disable)" : TITLE);
        }
        if (ev.restart) {
            reset_game(g);
            g->frame = 0;
            frozen = 0;
        }
        platform_wait_frame();
        if (ev.rewind && g->ram[GameMode] == 4 && snap_restore(g)) {
            paused = 0;
            frozen = 0;
        }
        if (ev.capture) {
            capture(g);
        }
        if (!paused && !frozen) {
            run_game_frame(g, audio);
        }
        if (audio) {
            if (paused) {
                memset(pcm, 0, sizeof pcm);
            } else {
                apu_mix(g, pcm, SAMPLES_PER_FRAME);
            }
            if (muted) {
                memset(pcm, 0, sizeof pcm);
            }
            platform_audio_submit(pcm);
        }
    }
    platform_shutdown();
    return 0;
}
