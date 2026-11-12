/* Player front-end (the executable `tmnt`): finds the ROM, runs the game at 60 frames per second and relays keys, picture and sound
 * through platform.h (Windows or SDL2). The pace comes from the sound card: 735 samples at 44100 Hz per frame.
 * Keys: arrows = d-pad, Z or W = B, X = A, Enter = Start, Tab or right Shift = Select, M = sound, P = pause, I = invincible,
 * H = hit boxes, J = the fixes of FIXES.md, E = the water fixes, C = the camera fix, S = the spawn mask, R = the balance, F = no sprite flicker, L = large view, F5 = restart, F9 = rewind, F10 = capture, Esc = quit. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "crt.h"
#include "fixes.h"
#include "game.h"
#include "help_screen.h"
#include "hitbox_view.h"
#include "large_view.h"
#include "menu.h"
#include "nmi.h"
#include "options.h"
#include "platform.h"
#include "ppu.h"
#include "spawn_mask.h"
#include "ui.h"

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
static int paused, frozen;
static Ui menu_ui;                       /* the menu is drawn here, the size of the window */
static int game_started, quit_requested; /* a game is running under the menu / Quit game was chosen */
static uint8_t menu_prev_pad;
static uint8_t
    pad_swallow; /* buttons that were held when the menu closed: the game does not see them until they are let go (Start that resumes must not pause) */
static char ini_path[1100];
/* the cheat "invincible" (key I, --invincible or the menu) is the option OPT_INVINCIBLE: the turtles' health is set back to the maximum */

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
static int start_level2, level2_done, no_menu;
static int random_seed;       /* --random-seed: randomises RandomSeed when --levelN jumps to the level */
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

/* The line of the range that is still open is written at once and rewritten every 30 frames (in place, padded with spaces), so that what has been played is in the
 * file even if the game is killed while a button is held. */
static long input_open_pos = -1;

static void write_input_range(uint32_t first, uint32_t last, uint32_t mask) {
    char line[64];
    snprintf(line, sizeof line, "%u %u %u", first, last, mask);
    fprintf(input_log, "%-40s\n", line);
}

static void record_input(Game *g) {
    if (!input_log) {
        return;
    }
    uint32_t f = g->frame - 1;
    if (g->pad == input_mask && f != 0) {
        if (input_mask && f % 30 == 0 && input_open_pos >= 0) {
            fseek(input_log, input_open_pos, SEEK_SET);
            write_input_range(input_start, f, input_mask);
            fflush(input_log);
        }
        return;
    }
    if (input_mask && input_open_pos >= 0) {
        fseek(input_log, input_open_pos, SEEK_SET);
        write_input_range(input_start, f - 1, input_mask);
    }
    input_mask = g->pad;
    input_start = f;
    input_open_pos = -1;
    if (input_mask) {
        input_open_pos = ftell(input_log);
        write_input_range(f, f, input_mask);
    }
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
    int shot_from =
        getenv("TMNT_DUMP_FROM") ? atoi(getenv("TMNT_DUMP_FROM")) : 0; /* TMNT_DUMP_FROM / TMNT_DUMP_TO: a picture of every frame of that range (name_N.bmp) */
    int shot_to = getenv("TMNT_DUMP_TO") ? atoi(getenv("TMNT_DUMP_TO")) : 0;
    FILE *hash_log = getenv("TMNT_DUMP_HASH") ? fopen(getenv("TMNT_DUMP_HASH"), "w") : NULL;
    for (int f = 1; f <= frames; f++) {
        logic_frame(g, 0);
        if (hash_log) { /* TMNT_DUMP_HASH=<file>: for every frame, a hash of the top 12 rows and of the rest of the picture (to look for blinking) */
            render(g);
            unsigned long long ht = 14695981039346656037ull, hr = ht;
            for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
                unsigned long long *h = i < SCREEN_W * 12 ? &ht : &hr;
                *h = (*h ^ screen[i]) * 1099511628211ull;
            }
            fprintf(hash_log, "%d %llx %llx%c", f, ht, hr, 10);
        }
        if (shot_from && f >= shot_from && f <= shot_to) {
            render(g);
            snprintf(name, sizeof name, "%s_%d.bmp", base, f);
            write_bmp(name);
        }
        apu_mix(g, pcm, SAMPLES_PER_FRAME);
        for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
            put16(w, (uint16_t)pcm[i]);
        }
    }
    fclose(w);
    if (hash_log) {
        fclose(hash_log);
    }
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

static int find_data_dir(char *out, size_t size);

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
        char data_dir[1200];
        find_data_dir(data_dir, sizeof data_dir);
        help_screen_rom_missing(dirs[0], data_dir); /* creates the folder and explains what to put in it */
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
        if (!strcmp(argv[i], "--no-menu")) {
            no_menu = 1; /* straight into the game, as before the menu existed */
        }
        if (!strcmp(argv[i], "--random-seed")) {
            random_seed = 1;
        }
        if (!strcmp(argv[i], "--invincible")) {
            opt_set(OPT_INVINCIBLE, 1);
        }
        if (!strcmp(argv[i], "--hitbox")) {
            hitbox_set_enabled(1);
        }
        if (!strcmp(argv[i], "--fixes")) {
            fix_set_all(1);
        }
        if (!strcmp(argv[i], "--water-fixes")) {
            fix_set_water(1);
        }
        if (!strcmp(argv[i], "--camera-fix")) {
            fix_set_camera(1);
        }
        if (!strcmp(argv[i], "--balance")) {
            fix_set_balance(1);
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
    if (random_seed) { /* --random-seed: a different layout of enemies at each launch (logged like the other pokes, so the replay stays exact) */
        uint8_t s = (uint8_t)(time(NULL) ^ clock());
        g->ram[RandomSeed] = s;
        if (poke_log) {
            fprintf(poke_log, "%u %u %u\n", g->frame, RandomSeed, s);
            fflush(poke_log);
        }
    }
}

static void invincible_cheat(Game *g) {
    if (!(opt_get(OPT_INVINCIBLE) && (g->frame & 3) == 0 && g->ram[GameMode] == 4)) {
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
    uint8_t raw = keyboard ? platform_pad() : 0;
    pad_swallow &= raw;
    g->pad = pad_script_n ? pad_script_at(g->frame) : (uint8_t)(raw & ~pad_swallow);
    g->frame++;
    record_input(g);
    rng_idle(g);
    nmi_frame(g);
    hitbox_end_frame(g);
    apply_start_level(g);
    invincible_cheat(g);
}

/* "Show FPS": the real number of pictures shown per second, in the bottom right corner (digits 3x5 drawn at twice the size, on a black patch). */
static void draw_fps(int fps) {
    static const uint16_t digit[10] = {0x7B6F, 0x2C97, 0x73E7, 0x73CF, 0x5BC9,
                                       0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF}; /* 15 bits, row by row, top left first */
    char text[8];
    int n = snprintf(text, sizeof text, "%d", fps > 999 ? 999 : fps);
    int w = n * 8 + 2, x0 = SCREEN_W - 4 - w, y0 = SCREEN_H - 16;
    for (int y = 0; y < 12; y++) {
        for (int x = 0; x < w; x++) {
            screen[(y0 + y) * SCREEN_W + x0 + x] = 0x000000;
        }
    }
    for (int i = 0; i < n; i++) {
        uint16_t bits = digit[text[i] - '0'];
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 3; x++) {
                if (bits & (1 << (14 - y * 3 - x))) {
                    for (int d = 0; d < 4; d++) {
                        screen[(y0 + 1 + y * 2 + d / 2) * SCREEN_W + x0 + 1 + i * 8 + x * 2 + d % 2] = 0xFFFFFF;
                    }
                }
            }
        }
    }
}

/* TMNT_FRAMELOG=<file>: the frames whose parts took long (poll of the keyboard and the pad, wait for the sound card, logic and drawing), to find a stall. */
static double now_ms(void) {
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}

static FILE *frame_log_file(void) {
    static FILE *f;
    static int tried;
    if (!tried) {
        tried = 1;
        if (getenv("TMNT_FRAMELOG")) {
            f = fopen(getenv("TMNT_FRAMELOG"), "w");
        }
    }
    return f;
}

static void frame_log(unsigned frame, double poll, double wait, double run, int pad) {
    FILE *f = frame_log_file();
    static int last_pad = -1;
    if (f && pad != last_pad) { /* the pad that the game sees, with the clock, to compare with the moment of a key press */
        fprintf(f, "t=%.0f frame %u pad %02X\n", now_ms(), frame, pad);
        last_pad = pad;
    }
    if (f && (poll > 8.0 || wait > 40.0 || run > 12.0)) {
        fprintf(f, "frame %u: poll %.1f ms, wait %.1f ms, logic+draw %.1f ms\n", frame, poll, wait, run);
        fflush(f);
    }
}

/* TMNT_FRAMELOG: every 5 seconds, where a frame's time goes (logic of the game, drawing of the picture, showing it in the window). */
static void frame_parts(double logic, double render_ms, double present) {
    static int frames;
    static double sum_l, sum_r, sum_p, max_l, max_r, max_p;
    if (!getenv("TMNT_FRAMELOG")) {
        return;
    }
    frames++;
    sum_l += logic;
    sum_r += render_ms;
    sum_p += present;
    max_l = logic > max_l ? logic : max_l;
    max_r = render_ms > max_r ? render_ms : max_r;
    max_p = present > max_p ? present : max_p;
    if (frames >= 300) {
        FILE *f = frame_log_file();
        if (f) {
            fprintf(f, "300 frames: logic %.1f ms (max %.1f), drawing %.1f ms (max %.1f), showing %.1f ms (max %.1f)%c", sum_l / 300, max_l, sum_r / 300, max_r,
                    sum_p / 300, max_p, 10);
            fflush(f);
        }
        frames = 0;
        sum_l = sum_r = sum_p = max_l = max_r = max_p = 0;
    }
}

static void count_fps(void) {
    static struct timespec t0;
    static int frames, shown;
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    if (!t0.tv_sec && !t0.tv_nsec) {
        t0 = now;
    }
    frames++;
    long ms = (long)((now.tv_sec - t0.tv_sec) * 1000 + (now.tv_nsec - t0.tv_nsec) / 1000000);
    if (ms >= 500) {
        shown = (int)(frames * 1000L / ms);
        frames = 0;
        t0 = now;
    }
    draw_fps(shown);
}

/* What the menu needs to show and choose the keys. */
static const char *keys_name(int code) {
    return platform_key_name(code);
}

static int keys_default(int row, int slot) {
    return platform_default_key(7 - row, slot);
}

static const MenuKeys menu_keys = {keys_name, keys_default};

/* The picture of the game: with the CRT look, or stretched by crt.c with a sharp bilinear filter (without Integer scaling, the window is rarely a whole multiple of 256x240,
 * and the window's own stretch gives pixels that are 4 or 5 wide at random: lines), or by the window itself when it is a whole multiple. The picture of the game, plain or with the CRT look (drawn by the processor at the size of the window, with the proportions kept). */
static void present_game(void) {
    static uint32_t *crt_buf;
    static int crt_w, crt_h, crt_div = 1, calm;
    static double crt_avg;
    if (!opt_get(OPT_CRT) && opt_get(OPT_INTEGER_SCALE)) { /* a whole multiple: the window can stretch it block by block */
        platform_present(screen);
        return;
    }
    int w, h;
    platform_window_size(&w, &h);
    /* on a huge window the filter may not fit in a frame (1/60 s): it is then drawn at half the size, which the window stretches, until it fits again */
    int rw = w / crt_div, rh = h / crt_div;
    if (rw != crt_w || rh != crt_h || !crt_buf) {
        free(crt_buf);
        crt_buf = (uint32_t *)malloc((size_t)rw * (size_t)rh * sizeof *crt_buf);
        crt_w = rw;
        crt_h = rh;
    }
    if (!crt_buf) {
        platform_present(screen);
        return;
    }
    double t0 = now_ms();
    CrtParams p = {opt_get(OPT_CRT_SCAN), opt_get(OPT_CRT_MASK), opt_get(OPT_CRT_CURVE), opt_get(OPT_INTEGER_SCALE), !opt_get(OPT_CRT)};
    crt_render(crt_buf, rw, rh, screen, &p);
    platform_present_hd(crt_buf, rw, rh);
    double dt = now_ms() - t0;
    crt_avg = crt_avg < 0.5 ? dt : crt_avg * 0.9 + dt * 0.1;
    if (crt_div == 1 && crt_avg > 13.0 && w % 2 == 0 && h % 2 == 0) {
        crt_div = 2;
        crt_avg = 0;
        calm = 0;
    } else if (crt_div == 2) {
        calm = crt_avg * 4.0 < 9.0 ? calm + 1 : 0; /* what the full size would cost, about four times as much */
        if (calm > 180) {
            crt_div = 1;
            crt_avg = 0;
            calm = 0;
        }
    }
}

/* The display options (menu or F11) reach the window, and the keys of the pad (menu) the keyboard. */
static void apply_display_options(void) {
    for (int bit = 0; bit < 8; bit++) {
        for (int slot = 0; slot < 2; slot++) {
            int v = opt_get(OPT_KEY(bit, slot));
            platform_set_key(bit, slot, v > 0 ? v : v < 0 ? 0 : platform_default_key(bit, slot));
        }
    }
    static int fullscreen = -1, integer = -1;
    if (fullscreen != opt_get(OPT_FULLSCREEN)) {
        fullscreen = opt_get(OPT_FULLSCREEN);
        platform_set_fullscreen(fullscreen);
    }
    if (integer != opt_get(OPT_INTEGER_SCALE)) {
        integer = opt_get(OPT_INTEGER_SCALE);
        platform_set_integer_scale(integer);
    }
}

/* One game frame: keys, logic, rendering, window refresh. */
static void run_game_frame(Game *g, int audio) {
    static int last_title_frame = -1;
    if (g->ram[GameMode] == 4 && g->frame % SNAP_EVERY == SNAP_EVERY - 1) {
        snap_save(g); /* end of the 1st frame of the series: stable state between two frames */
    }
    double t_a = now_ms();
    logic_frame(g, 1);
    double t_b = now_ms();
    if (g->left_attract && g->ram[GameMode] == 4) { /* level loaded, but the rest is not ported: the frame is kept */
        frozen = 1;
        apu_write(g, 0x15, 0);
        platform_set_title("TMNT (Europe) - C port: level 1 loaded, the rest is not ported (F5 = restart)");
    } else if (g->left_attract) { /* GameMode 5 and later: back to the licence screen */
        platform_set_title("TMNT (Europe) - C port: the game is not ported, back to the licence screen");
        reset_game(g);
        g->frame = 0;
    }
    double t_c = now_ms();
    render(g);
    double t_d = now_ms();
    if (opt_get(OPT_SHOW_FPS)) {
        count_fps();
    }
    present_game();
    frame_parts(t_b - t_a, t_d - t_c, now_ms() - t_d);
    if ((int)g->frame / 60 != last_title_frame && g->frame % 60 == 0) {
        char t[240];
        last_title_frame = (int)g->frame / 60;
        snprintf(t, sizeof t, "TMNT (Europe) - port C | mode %d.%d%s%s%s%s%s%s%s%s%s", g->ram[GameMode], g->ram[GameSubMode],
                 opt_get(OPT_MUTE) ? " | muted" : "", audio ? "" : " | no sound card", fix_any_enabled() ? " | FIXES ON (J)" : "",
                 fix_water_enabled() ? " | WATER FIXES (E)" : "", fix_camera_enabled() ? " | CAMERA FIX (C)" : "",
                 spawn_mask_active() ? " | SPAWN MASK (S)" : "", fix_balance_enabled() ? " | BALANCE (R)" : "",
                 ppu_sprite_limit() > 8 ? " | NO FLICKER (F)" : "", large_view_enabled() ? " | LARGE VIEW (L)" : "");
        platform_set_title(t);
    }
}

/* The folder "data" of the game (fonts, logo, spawn mask): next to the executable, one folder up, or in the current folder. */
static int find_data_dir(char *out, size_t size) {
    char exe_dir[1024];
    platform_exe_dir(exe_dir, sizeof exe_dir);
    const char *fmt[3] = {"%s/data", "%s/../data", "data"};
    for (int i = 0; i < 3; i++) {
        char probe[1300];
        snprintf(out, size, fmt[i], exe_dir);
        snprintf(probe, sizeof probe, "%s/fonts/KOMIKAHB.ttf", out);
        FILE *f = fopen(probe, "rb");
        if (f) {
            fclose(f);
            return 1;
        }
    }
    snprintf(out, size, "%s/data", exe_dir);
    return 0;
}

/* Where tmnt.ini lives: next to the executable when that folder can be written (or the file is already there), else in the user's own folder. */
static void choose_ini_path(char *out, size_t size) {
    char exe_dir[1024], probe[1200];
    platform_exe_dir(exe_dir, sizeof exe_dir);
    snprintf(out, size, "%s/tmnt.ini", exe_dir);
    FILE *f = fopen(out, "r");
    if (f) {
        fclose(f);
        return;
    }
    snprintf(probe, sizeof probe, "%s/tmnt.ini.probe", exe_dir);
    f = fopen(probe, "w");
    if (f) {
        fclose(f);
        remove(probe);
        return;
    }
    const char *base = getenv("APPDATA");
    char dir[1000];
    if (base) {
        snprintf(dir, sizeof dir, "%s/tmnt", base);
    } else if (getenv("XDG_CONFIG_HOME")) {
        snprintf(dir, sizeof dir, "%s/tmnt", getenv("XDG_CONFIG_HOME"));
    } else if (getenv("HOME")) {
        snprintf(dir, sizeof dir, "%s/.config/tmnt", getenv("HOME"));
    } else {
        return; /* nowhere: the file next to the executable, which will not be written */
    }
    platform_make_dir(dir);
    snprintf(out, size, "%s/tmnt.ini", dir);
}

/* The logo of the Original panel: the game is run without a window up to its title screen, which gives the menu its logo (the game restarts after). */
static void cut_title_logo(Game *g) {
    FILE *saved_input = input_log;
    input_log = NULL;
    reset_game(g);
    g->frame = 0;
    for (int i = 0; i < 400; i++) {
        logic_frame(g, 0);
    }
    render(g);
    menu_set_logo_picture(screen);
    input_log = saved_input;
    reset_game(g);
    g->frame = 0;
}

/* "PLAY" in the menu: a new game with the options as they are, at the level chosen. */
static void start_new_game(Game *g) {
    reset_game(g);
    g->frame = 0;
    frozen = 0;
    paused = 0;
    level2_done = 0;
    start_level2 = opt_get(OPT_START_LEVEL) > 1;
    start_level_n = opt_get(OPT_START_LEVEL) - 1;
    game_started = 1;
    platform_set_title(TITLE);
}

/* One frame of the menu: the pad and the mouse go to it, it is drawn over the frozen game and shown at the size of the window. */
#define MENU_MAX_PIXELS 1500000.0

static void menu_frame(Game *g, const PlatformEvents *ev) {
    int w, h;
    platform_window_size(&w, &h);
    /* the menu is drawn by the processor: on a big window it is drawn at a smaller size (about 1.5 million pixels at most) and the window stretches it */
    int uw = w, uh = h;
    if ((double)w * (double)h > MENU_MAX_PIXELS) {
        double k = sqrt(MENU_MAX_PIXELS / ((double)w * (double)h));
        uw = (int)((double)w * k);
        uh = (int)((double)h * k);
    }
    ui_resize(&menu_ui, uw, uh);
    uint8_t pad = platform_pad(), edge = (uint8_t)(pad & ~menu_prev_pad);
    menu_prev_pad = pad;
    MenuInput in = {0};
    in.up = (edge & 0x08) != 0;
    in.down = (edge & 0x04) != 0;
    in.left = (edge & 0x02) != 0;
    in.right = (edge & 0x01) != 0;
    in.confirm = (edge & 0x90) != 0; /* A or Start */
    in.back = (edge & 0x40) != 0;
    in.escape = ev->menu;
    in.start = ev->pad_start;
    in.tab_prev = ev->tab_prev;
    in.tab_next = ev->tab_next;
    platform_mouse(&in.mouse_x, &in.mouse_y, &in.clicked, &in.mouse_down);
    if (in.mouse_x >= 0 && (uw != w || uh != h)) { /* the cursor in the pixels of the menu's picture */
        in.mouse_x = (int)((long long)in.mouse_x * uw / w);
        in.mouse_y = (int)((long long)in.mouse_y * uh / h);
    }
    in.key = platform_capture_key();
    MenuAction a = menu_update(&in);
    menu_draw(&menu_ui, game_started ? screen : NULL);
    platform_present_hd(menu_ui.px, uw, uh);
    if (a == MENU_NEW_GAME || a == MENU_RESUME) {
        pad_swallow = platform_pad();
    }
    if (a == MENU_NEW_GAME) {
        start_new_game(g);
    } else if (a == MENU_QUIT) {
        quit_requested = 1;
    }
}

int main(int argc, char **argv) {
    Game *g = load_game(argc, argv);
    if (!g) {
        return 1;
    }
    reset_game(g);
    choose_ini_path(ini_path, sizeof ini_path);
    options_load(ini_path); /* what the player chose in the menu; the command line below comes after and wins */
    parse_options(argc, argv);
    if (start_level2) {
        opt_set(OPT_START_LEVEL, start_level_n + 1);
    }
    { /* the spawn mask that comes with the game is always loaded (an absent one is an empty one); the key S, or --spawn-mask, switches it on */
        char exe_dir[1024], found[1100];
        platform_exe_dir(exe_dir, sizeof exe_dir);
        const char *shipped = spawn_mask_default_path(exe_dir, found, sizeof found);
        if (shipped) {
            spawn_mask_load(shipped, g->rom, 1);
        }
        for (int i = 1; i < argc; i++) {
            if (!strcmp(argv[i], "--spawn-mask")) { /* alone: the shipped mask; with a file: that one */
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    spawn_mask_load(argv[i + 1], g->rom, 0);
                }
                spawn_mask_set_enabled(1);
            }
        }
    }
    open_logs(g);
    for (int i = 1; i + 2 < argc; i++) {
        if (!strcmp(argv[i], "--dump")) {
            return dump_mode(g, atoi(argv[i + 1]), argv[i + 2]);
        }
    }

    if (platform_init(opt_get(OPT_INVINCIBLE) ? "TMNT (Europe) - port C - INVINCIBLE (I to disable)" : TITLE)) {
        return 1;
    }
    int audio = platform_audio_open();
    static int16_t pcm[SAMPLES_PER_FRAME];
    g->frame = 0;
    {
        char data_dir[1200];
        find_data_dir(data_dir, sizeof data_dir);
        menu_init(data_dir, ini_path);
        menu_set_keys(&menu_keys);
    }
    game_started = no_menu || start_level2 || pad_script_n > 0;
    if (!game_started) { /* the launcher: the menu on its first page, no game under it yet */
        for (int i = 1; i < argc; i++) {
            if (!strcmp(argv[i], "--blur-logo")) { /* screenshots: the game's logo, blurred beyond recognition */
                menu_set_logo_blur(1);
            }
        }
        cut_title_logo(g);
        menu_open(0);
    }
    for (;;) {
        PlatformEvents ev;
        double t_start = now_ms();
        platform_poll(&ev);
        double t_poll = now_ms() - t_start;
        if (ev.quit) {
            break;
        }
        if (ev.fullscreen) { /* F11: like the menu's option, and saved as it is */
            opt_set_user(OPT_FULLSCREEN, !opt_get(OPT_FULLSCREEN));
            options_save(ini_path);
        }
        apply_display_options();
        if (menu_is_open()) {
            menu_frame(g, &ev);
            if (quit_requested) {
                break;
            }
            platform_wait_frame();
            if (audio) {
                memset(pcm, 0, sizeof pcm);
                platform_audio_submit(pcm);
            }
            continue;
        }
        if (ev.menu) { /* Esc: the menu over the frozen game */
            menu_open(game_started);
            menu_prev_pad = platform_pad();
            int mx, my, clicked, down;
            platform_mouse(&mx, &my, &clicked, &down); /* a click made while playing must not reach the menu */
            continue;
        }
        if (ev.mute) {
            opt_set(OPT_MUTE, !opt_get(OPT_MUTE));
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
        if (ev.water) { /* the fixes of the water level (key E), apart from the others */
            fix_toggle_water();
            platform_set_title(fix_water_enabled() ? "TMNT (Europe) - port C | WATER FIXES ON (E to switch off)"
                                                   : "TMNT (Europe) - port C | water fixes off (E to switch on)");
        }
        if (ev.camera) { /* the camera catches up with the turtle (key C), apart from the other fixes */
            fix_toggle_camera();
            platform_set_title(fix_camera_enabled() ? "TMNT (Europe) - port C | CAMERA FIX ON (C to switch off)"
                                                    : "TMNT (Europe) - port C | camera fix off (C to switch on)");
        }
        if (ev.balance) { /* the damage of the normal attacks and the reach of Raphael's sai (key R, group of its own) */
            fix_toggle_balance();
            platform_set_title(fix_balance_enabled() ? "TMNT (Europe) - port C | BALANCE ON (R to switch off)"
                                                     : "TMNT (Europe) - port C | balance off (R to switch on)");
        }
        if (ev.spawn) { /* the spawn mask of the Whole Pizza edition (key S): the spawns to come, not the enemies already there */
            spawn_mask_set_enabled(!spawn_mask_enabled());
            platform_set_title(!spawn_mask_count()    ? "TMNT (Europe) - port C | no spawn mask file (data/spawn_mask.txt)"
                               : spawn_mask_enabled() ? "TMNT (Europe) - port C | SPAWN MASK ON (S to switch off)"
                                                      : "TMNT (Europe) - port C | spawn mask off (S to switch on)");
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
            opt_set(OPT_INVINCIBLE, !opt_get(OPT_INVINCIBLE));
            platform_set_title(opt_get(OPT_INVINCIBLE) ? "TMNT (Europe) - port C - INVINCIBLE (I to disable)" : TITLE);
        }
        if (ev.restart) {
            reset_game(g);
            g->frame = 0;
            frozen = 0;
        }
        double t_wait0 = now_ms();
        platform_wait_frame();
        double t_wait = now_ms() - t_wait0;
        if (ev.rewind && g->ram[GameMode] == 4 && snap_restore(g)) {
            paused = 0;
            frozen = 0;
        }
        if (ev.capture) {
            capture(g);
        }
        double t_run0 = now_ms();
        if (!paused && !frozen) {
            run_game_frame(g, audio);
        }
        frame_log(g->frame, t_poll, t_wait, now_ms() - t_run0, g->pad);
        if (audio) {
            if (paused) {
                memset(pcm, 0, sizeof pcm);
            } else {
                apu_mix(g, pcm, SAMPLES_PER_FRAME);
            }
            if (opt_get(OPT_MUTE)) {
                memset(pcm, 0, sizeof pcm);
            } else if (opt_get(OPT_VOLUME) < 100) {
                for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
                    pcm[i] = (int16_t)(pcm[i] * opt_get(OPT_VOLUME) / 100);
                }
            }
            platform_audio_submit(pcm);
        }
    }
    menu_flush(); /* the last change of the menu, if the program ends before it was written */
    platform_shutdown();
    return 0;
}
