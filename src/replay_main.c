/* Command-line replay tool (no window, no keyboard, no sound output). Runs the port from the power-on screens for a number of frames,
 * feeding it recorded inputs, and writes a trace (same format as tools/intro_states.lua) plus optional dumps for comparison with an
 * emulator (RAM, name tables, one 256x240 picture of NES palette indices per frame).
 *
 * Usage : tmnt_replay <rom.nes> <trace.txt> [frames] [dir] [step] [start_at] [ram_frames] [seed.txt] [script.txt] [pokes.txt] [upd.txt]
 *                    [pad.txt] [seedrg.txt] [rewind.txt] [--allow-unknown-rom]
 *   frames      number of frames to run (default 2800)
 *   dir         output folder for the dumps ("" : none) ; step : one picture every `step` frames
 *   start_at    presses Start for 3 frames from that frame (0 : never)
 *   ram_frames  frames whose RAM is dumped, "435,450,470-6000" (numbers or ranges)
 *   seed.txt    "frame value" : random seed seen at the end of each frame by Mesen (the seed comes from the idle loop)
 *   script.txt  "first last mask" : buttons held on logical frames first..last (Mesen frame numbers : the key read on frame L acts on L + 1)
 *   pokes.txt   "frame address value" : RAM writes at the end of a frame (cheats, forced situations)
 *   upd.txt     "frame count" : frames where the console updated the sound more than once (lost frames)
 *   pad.txt     "number mask" : controller reads of Mesen ($C4DE) ; replaces script.txt and reproduces lost frames exactly
 *   seedrg.txt  "number value" : $45 seen by the game logic at each call of RunGameMode
 *   rewind.txt  "frame snapshot" : F9 rewinds of a played game (see win_main.c)
 * Environment : WAVOUT=<file.wav> (audio of the replay, one frame of sound per NMI, 44100 Hz mono), SNDCOUNT=<file> ("logical frame updates so far"), PIXPIPE=1 (pictures on stdout, PIXFROM / PIXTO : frame window), TRACE_UNBUF, REWINDMAP, SEEDOUT=<file> (writes the seeds of the port), RAMBIN=1 (one ram.bin file), NTDUMP=1 (name tables). */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include "game.h"
#include "apu.h"
#include "platform.h"
#include "hitbox_view.h"
#include "large_view.h"
#include "fixes.h"
#include "enemy.h"
#include "nmi.h"
#include "sound.h"
#include "stage_play.h"
#include "diag.h"

enum {
    NSNAP = 12,
    SNAP_EVERY = 600,
    MAX_INDEX = 1 << 26
};

/* ------------------------------------------------------------------------------------------------ input files */
typedef struct {
    unsigned *a, *b, *c;
    size_t n, cap;
} Triples; /* rows of three numbers */
typedef struct {
    int *val;
    size_t n;
    int def;
} IndexedInts; /* value by index, `def` where nothing was recorded */
typedef struct {
    unsigned frame, address, value;
    size_t order;
} Poke;
typedef struct {
    unsigned frame;
    int snapshot;
} Rewind;

static void die(const char *msg) {
    fprintf(stderr, "tmnt_replay: %s\n", msg);
    exit(2);
}

static void *grow(void *p, size_t bytes) {
    p = realloc(p, bytes);
    if (!p) {
        die("out of memory");
    }
    return p;
}

static FILE *open_input(const char *path, const char *what) {
    if (!path || !path[0]) {
        return NULL;
    }
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "tmnt_replay: warning: cannot open %s file %s (%s) : ignored\n", what, path, strerror(errno));
    }
    return f;
}

static unsigned parse_number(const char *s, const char *what) {
    char *end;
    errno = 0;
    unsigned long v = strtoul(s, &end, 10);
    if (errno || end == s || *end) {
        fprintf(stderr, "tmnt_replay: invalid %s \"%s\"\n", what, s);
        exit(2);
    }
    return (unsigned)v;
}

static void triples_add(Triples *t, unsigned a, unsigned b, unsigned c) {
    if (t->n == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 1024;
        t->a = grow(t->a, t->cap * sizeof *t->a);
        t->b = grow(t->b, t->cap * sizeof *t->b);
        t->c = grow(t->c, t->cap * sizeof *t->c);
    }
    t->a[t->n] = a;
    t->b[t->n] = b;
    t->c[t->n] = c;
    t->n++;
}

static void load_triples(const char *path, const char *what, Triples *t) {
    FILE *f = open_input(path, what);
    unsigned a, b, c;
    while (f && fscanf(f, "%u %u %u", &a, &b, &c) == 3) {
        triples_add(t, a, b, c);
    }
    if (f) {
        fclose(f);
    }
}

static void indexed_init(IndexedInts *x, int def) {
    x->val = NULL;
    x->n = 0;
    x->def = def;
}

static void indexed_set(IndexedInts *x, long idx, int value) {
    if (idx < 0 || idx >= MAX_INDEX) {
        fprintf(stderr, "tmnt_replay: warning: index %ld out of range, line ignored\n", idx);
        return;
    }
    if ((size_t)idx >= x->n) {
        x->val = grow(x->val, ((size_t)idx + 1) * sizeof *x->val);
        for (size_t i = x->n; i <= (size_t)idx; i++) {
            x->val[i] = x->def;
        }
        x->n = (size_t)idx + 1;
    }
    x->val[idx] = value;
}

static int indexed_get(const IndexedInts *x, unsigned idx) {
    return idx < x->n ? x->val[idx] : x->def;
}

/* "index value" rows ; with min_value > 0 only the rows whose value is above it are kept, stored as value - min_value (lag file) */
static void load_indexed(const char *path, const char *what, IndexedInts *x, int min_value) {
    FILE *f = open_input(path, what);
    long idx;
    int v;
    while (f && fscanf(f, "%ld %d", &idx, &v) == 2) {
        if (min_value) {
            if (v > min_value) {
                indexed_set(x, idx, v - min_value);
            }
        } else {
            indexed_set(x, idx, v);
        }
    }
    if (f) {
        fclose(f);
    }
}

static int compare_pokes(const void *pa, const void *pb) /* by frame, then by position in the file : stable */
{
    const Poke *a = pa, *b = pb;
    if (a->frame != b->frame) {
        return a->frame < b->frame ? -1 : 1;
    }
    return a->order < b->order ? -1 : a->order > b->order;
}

/* ------------------------------------------------------------------------------------------------ the replay */
typedef struct Replay {
    Game *g;
    unsigned frames, step, start_at;
    int pixpipe; /* PIXPIPE=1 : pictures on stdout, from frame PIXFROM to PIXTO (see tools/video/) */
    unsigned pix_first, pix_last;
    const char *dir, *ram_frames;
    Triples script;
    Poke *pokes;
    size_t npokes, next_poke;
    IndexedInts seeds, seedrg, lag_extra, padlog;
    Rewind *rewinds;
    size_t nrewinds, next_rewind;
    Game *snap;
    uint32_t snap_frame[NSNAP];
    int snap_count, snap_head;
    FILE *rewind_map, *seed_out, *ram_bin;
    int ram_bin_opened, last_mode, last_scene;
    uint8_t picture[256 * 240];
} Replay;

static void load_pokes(Replay *r, const char *path) {
    Triples t = {0};
    load_triples(path, "pokes", &t);
    r->pokes = grow(NULL, (t.n ? t.n : 1) * sizeof *r->pokes);
    for (size_t i = 0; i < t.n; i++) {
        r->pokes[i] = (Poke){t.a[i], t.b[i], t.c[i], i};
    }
    r->npokes = t.n;
    qsort(r->pokes, r->npokes, sizeof *r->pokes, compare_pokes);
    free(t.a);
    free(t.b);
    free(t.c);
}

static void load_rewinds(Replay *r, const char *path) {
    FILE *f = open_input(path, "rewind");
    unsigned frame;
    int snapshot;
    size_t cap = 0;
    while (f && fscanf(f, "%u %d", &frame, &snapshot) == 2) {
        if (r->nrewinds == cap) {
            cap = cap ? cap * 2 : 64;
            r->rewinds = grow(r->rewinds, cap * sizeof *r->rewinds);
        }
        r->rewinds[r->nrewinds++] = (Rewind){frame, snapshot};
    }
    if (f) {
        fclose(f);
    }
}

/* F9 pressed before this frame in the played game: go back to a snapshot (same snapshots every 600 game frames) */
static void apply_rewinds(Replay *r) {
    Game *g = r->g;
    while (r->next_rewind < r->nrewinds && r->rewinds[r->next_rewind].frame == g->frame - 1) {
        int which = r->rewinds[r->next_rewind].snapshot;
        if (which < r->snap_count) {
            int slot = (r->snap_head - 1 - which + 2 * NSNAP) % NSNAP;
            if (r->rewind_map) {
                fprintf(r->rewind_map, "R %u %u\n", r->rewinds[r->next_rewind].frame, r->snap_frame[slot]);
            }
            Rom *rom = g->rom;
            FILE *trace = g->trace;
            uint32_t frame = g->frame;
            *g = r->snap[slot];
            g->rom = rom;
            g->trace = trace;
            g->frame = frame;
        }
        r->next_rewind++;
    }
    if (M(GameMode) == 4 && (g->frame - 1) % SNAP_EVERY == SNAP_EVERY - 1) {
        if (r->rewind_map) {
            fprintf(r->rewind_map, "S %u\n", g->frame);
        }
        r->snap_frame[r->snap_head] = g->frame;
        r->snap[r->snap_head] = *g;
        r->snap_head = (r->snap_head + 1) % NSNAP;
        if (r->snap_count < NSNAP) {
            r->snap_count++;
        }
    }
}

static void apply_inputs(Replay *r) {
    Game *g = r->g;
    g->pad = (r->start_at && g->frame >= r->start_at && g->frame < r->start_at + 3) ? 0x10 : 0;
    for (size_t i = 0; i < r->script.n; i++) { /* the key read by Mesen on logical frame L acts on frame L + 1 */
        if (g->frame >= r->script.a[i] + 1 && g->frame <= r->script.b[i] + 1) {
            g->pad |= (uint8_t)r->script.c[i];
        }
    }
    if (r->padlog.n && g->frame < r->padlog.n) {
        g->pad = (uint8_t)r->padlog.val[g->frame];
    }
}

static void apply_random_seed(Replay *r) {
    Game *g = r->g;
    int logic = indexed_get(&r->seedrg, g->frame);
    int idle = indexed_get(&r->seeds, g->frame - 1);
    if (logic >= 0) {
        M(RandomSeed) = (uint8_t)logic;
    } else if (idle >= 0) {
        M(RandomSeed) = (uint8_t)idle; /* the seed comes from the idle loop */
    } else {                           /* no measured seed : the modelled random numbers (like win_main) */
        rng_idle(g);
        if (r->seed_out) {
            fprintf(r->seed_out, "%u %u\n", g->frame, M(RandomSeed)); /* SEEDOUT : seeds of the port, to impose on Mesen */
        }
    }
}

static void apply_pokes(Replay *r) {
    Game *g = r->g;
    while (r->next_poke < r->npokes && r->pokes[r->next_poke].frame <= g->frame) {
        const Poke *p = &r->pokes[r->next_poke++];
        if (p->frame == g->frame && p->address < 0x800) {
            M(p->address) = (uint8_t)p->value;
        }
    }
}

static int in_frame_list(const char *list, unsigned frame) /* "435,450,470-6000" : numbers or ranges a-b */
{
    for (const char *q = list; *q;) {
        char *e;
        unsigned a = (unsigned)strtoul(q, &e, 10), b = a;
        if (e == q) {
            break;
        }
        if (*e == '-') {
            b = (unsigned)strtoul(e + 1, &e, 10);
        }
        if (frame >= a && frame <= b) {
            return 1;
        }
        q = (*e == ',') ? e + 1 : e;
    }
    return 0;
}

static FILE *create_output(const char *dir, const char *name_fmt, unsigned frame) {
    char name[512];
    snprintf(name, sizeof name, name_fmt, dir, frame);
    FILE *f = fopen(name, "wb");
    if (!f) {
        fprintf(stderr, "tmnt_replay: warning: cannot create %s (%s)\n", name, strerror(errno));
    }
    return f;
}

static void dump_outputs(Replay *r) {
    Game *g = r->g;
    const char *dir = r->dir;
    if (r->ram_frames && dir && in_frame_list(r->ram_frames, g->frame)) {
        if (!r->ram_bin_opened) { /* RAMBIN=1 : a single ram.bin file (records : 4 bytes of frame number + 2048 bytes of RAM) */
            r->ram_bin_opened = 1;
            if (getenv("RAMBIN")) {
                char fn[512];
                snprintf(fn, sizeof fn, "%s/ram.bin", dir);
                r->ram_bin = fopen(fn, "wb");
                if (!r->ram_bin) {
                    fprintf(stderr, "tmnt_replay: warning: cannot create %s (%s)\n", fn, strerror(errno));
                }
            }
        }
        if (r->ram_bin) {
            uint32_t frame = g->frame;
            fwrite(&frame, 4, 1, r->ram_bin);
            fwrite(g->ram, 1, 0x800, r->ram_bin);
        } else if (!getenv("RAMBIN")) {
            FILE *f = create_output(dir, "%s/ram_%05u.bin", g->frame);
            if (f) {
                fwrite(g->ram, 1, 0x800, f);
                fclose(f);
            }
        }
    }
    if (dir && getenv("NTDUMP") && g->frame % 100 == 0) { /* the four name tables (4 KB) according to the mirroring */
        FILE *f = create_output(dir, "%s/nv_%05u.bin", g->frame);
        if (f) {
            for (int n = 0; n < 4; n++) {
                fwrite(g->ppu.vram + (g->ppu.mirror_v ? (n & 1) : (n >> 1)) * 0x400, 1, 0x400, f);
            }
            fclose(f);
        }
    }
    if (dir && getenv("NTDUMP") && M(SideViewFlag) == 1 && g->frame % 25 == 0) { /* NTDUMP : name tables 0 and 2 during the rope (see tools/lt_fuzz.lua) */
        FILE *f = create_output(dir, "%s/nt_%05u.bin", g->frame);
        if (f) {
            fwrite(g->ppu.vram, 1, 0x800, f);
            fclose(f);
        }
    }
    if (r->pixpipe && g->frame >= r->pix_first &&
        g->frame <= r->pix_last) { /* PIXPIPE=1 : records on stdout (4 bytes of frame number + 61440 palette indices) */
        uint32_t frame = g->frame;
        ppu_render(g, r->picture);
        fwrite(&frame, 4, 1, stdout);
        fwrite(r->picture, 1, sizeof r->picture, stdout);
    }
    if (dir && r->step && g->frame % r->step == 0) {
        if (getenv("TMNT_LARGE_PIX")) { /* tests only: the pictures are drawn with the large view */
            large_view_set_enabled(1);
            large_view_prepare(g);
        }
        ppu_render(g, r->picture);
        FILE *f = create_output(dir, "%s/p%05u.pix", g->frame);
        if (f) {
            fwrite(r->picture, 1, sizeof r->picture, f);
            fclose(f);
        }
    }
}

static void log_events(Replay *r) {
    Game *g = r->g;
    int mode = M(GameMode);
    if (mode != r->last_mode) {
        fprintf(g->trace, "frame=%u GameMode=%02X\n", g->frame, mode);
        r->last_mode = mode;
    }
    if (mode == 1 && M(GameSubMode) == 1) {
        int key = M(SceneState) * 16 + M(CurrentTurtle);
        if (key != r->last_scene) {
            fprintf(g->trace, "frame=%u IntroState=%d turtle=%d ent5=%02X,%02X\n", g->frame, M(SceneState), M(CurrentTurtle), M(EntSprite + 5),
                    M(EntAnimFrame + 5));
            r->last_scene = key;
        }
    }
}

/* ------------------------------------------------------------------------------------------------ audio (WAVOUT, SNDCOUNT) */
static FILE *wav_file, *snd_count_file;
static unsigned snd_updates; /* sound driver updates so far = NMIs = console frames */

static void wav_put32(FILE *f, uint32_t v) {
    for (int i = 0; i < 4; i++) {
        fputc((int)((v >> (8 * i)) & 0xFF), f);
    }
}

static void wav_header(FILE *f, uint32_t frames) {
    uint32_t bytes = frames * SAMPLES_PER_FRAME * 2;
    fwrite("RIFF", 1, 4, f);
    wav_put32(f, 36 + bytes);
    fwrite("WAVEfmt ", 1, 8, f);
    wav_put32(f, 16);
    wav_put32(f, 1 | (1u << 16)); /* PCM, mono */
    wav_put32(f, SAMPLE_RATE);
    wav_put32(f, SAMPLE_RATE * 2);
    wav_put32(f, 2 | (16u << 16)); /* block align, bits per sample */
    fwrite("data", 1, 4, f);
    wav_put32(f, bytes);
}

static void sound_tap(Game *g) { /* one sound update = one NMI = 1/60.0988 s of audio (the video tools correct the speed) */
    snd_updates++;
    if (wav_file) {
        static int16_t pcm[SAMPLES_PER_FRAME];
        apu_mix(g, pcm, SAMPLES_PER_FRAME);
        for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
            fputc(pcm[i] & 0xFF, wav_file);
            fputc((pcm[i] >> 8) & 0xFF, wav_file);
        }
    }
}

static void audio_finish(void) {
    if (wav_file) {
        fseek(wav_file, 0, SEEK_SET);
        wav_header(wav_file, snd_updates);
        fclose(wav_file);
    }
    if (snd_count_file) {
        fclose(snd_count_file);
    }
}

static int check_large; /* TMNT_LARGE=1 */

static void run_frame(Replay *r) {
    Game *g = r->g;
    if (snd_count_file) {
        fprintf(snd_count_file, "%u %u\n", g->frame, snd_updates); /* updates done before this logical frame */
    }
    apply_rewinds(r);
    apply_inputs(r);
    apply_random_seed(r);
    nmi_frame(g);
    if (check_large) { /* tests only: rebuilds the row above the picture at every frame where it is needed and checks it (see large_view.h) */
        large_view_check(g);
    }
    for (int extra = indexed_get(&r->lag_extra, g->frame) - (int)g->frame_lag_updates; extra > 0; extra--) {
        sound_update(g); /* measured "lost" frames */
    }
    apply_pokes(r);
    dump_outputs(r);
    log_events(r);
}

#ifdef SNDDIFF
static void play_every_sound(Game *g) /* SNDALL=1 (with SNDDIFF=1) : plays each sound $00-$FF for 400 frames */
{
    for (int id = 0; id < 256; id++) {
        g->frame = (uint32_t)id * 1000 + 1;
        sound_init(g);
        sound_play(g, (uint8_t)id);
        for (int i = 0; i < 400; i++) {
            g->frame++;
            sound_update(g);
        }
    }
}
#endif

int main(int argc, char **argv) {
    if (getenv("WATCHDOG")) {
        diag_start_watchdog((int)parse_number(getenv("WATCHDOG"), "WATCHDOG"));
    }
    if (argc > 1 && !strcmp(argv[argc - 1], "--allow-unknown-rom")) {
        rom_allow_unknown(1);
        argc--;
    } /* must be the last argument */
    if (argc == 3 && !strcmp(argv[2], "--routes")) { /* lists the routines of the ROM dispatch tables that the port does not know */
        Game *g = calloc(1, sizeof *g);
        if (!g) {
            die("out of memory");
        }
        g->rom = rom_load(argv[1]);
        if (!g->rom) {
            fprintf(stderr, "ROM error: %s\n", rom_error());
            return 1;
        }
        enemy_routes_report(g, stdout);
        object_routes_report(g, stdout);
        printf("(nothing listed above = every routine of the ROM dispatch tables has a C function)\n");
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr,
                "usage: %s rom.nes trace.txt [frames] [dir] [step] [start_at] [ram_frames] [seed.txt] [script.txt] [pokes.txt] [upd.txt] [pad.txt] "
                "[seedrg.txt] [rewind.txt] [--allow-unknown-rom]\n",
                argv[0]);
        return 2;
    }
    Game *g = calloc(1, sizeof *g);
    Replay *r = calloc(1, sizeof *r);
    if (!g || !r) {
        die("out of memory");
    }
    r->g = g;
    g->rom = rom_load(argv[1]);
    if (!g->rom) {
        fprintf(stderr, "ROM error: %s\n", rom_error());
        return 1;
    }
    g->trace = fopen(argv[2], "w");
    if (!g->trace) {
        fprintf(stderr, "tmnt_replay: cannot create trace file %s (%s)\n", argv[2], strerror(errno));
        return 1;
    }
    if (getenv("TRACE_UNBUF")) {
        setvbuf(g->trace, NULL, _IONBF, 0); /* to see where the port hangs */
    }

    r->frames = argc > 3 ? parse_number(argv[3], "frame count") : 2800;
    r->dir = argc > 4 && argv[4][0] ? argv[4] : NULL;
    r->step = argc > 5 ? parse_number(argv[5], "picture step") : 1;
    r->start_at = argc > 6 ? parse_number(argv[6], "Start frame") : 0;
    r->ram_frames = argc > 7 && argv[7][0] ? argv[7] : NULL;
    indexed_init(&r->seeds, -1);
    indexed_init(&r->seedrg, -1);
    indexed_init(&r->lag_extra, 0);
    indexed_init(&r->padlog, 0);
    if (argc > 8) {
        load_indexed(argv[8], "seed", &r->seeds, 0);
    }
    if (argc > 9) {
        load_triples(argv[9], "script", &r->script);
    }
    if (argc > 10) {
        load_pokes(r, argv[10]);
    }
    if (argc > 11) {
        load_indexed(argv[11], "update", &r->lag_extra, 1); /* count 2 -> one extra sound update */
    }
    if (argc > 12) {
        load_indexed(argv[12], "pad", &r->padlog, 0);
    }
    if (argc > 13) {
        load_indexed(argv[13], "seedrg", &r->seedrg, 0);
    }
    if (argc > 14) {
        load_rewinds(r, argv[14]);
    }
    r->snap = calloc(NSNAP, sizeof *r->snap);
    if (!r->snap) {
        die("out of memory");
    }
    r->rewind_map = getenv("REWINDMAP") ? fopen(getenv("REWINDMAP"), "w")
                                        : NULL; /* "S frame" at each snapshot, "R frame snapshot-frame" at each rewind : for tools/linearise_partie.py */
    r->seed_out = getenv("SEEDOUT") ? fopen(getenv("SEEDOUT"), "w") : NULL;
    r->last_mode = r->last_scene = -1;

    if (getenv("PIXPIPE")) {
        const char *from = getenv("PIXFROM"), *to = getenv("PIXTO");
        r->pixpipe = 1;
        r->pix_first = from ? (unsigned)strtoul(from, NULL, 10) : 1;
        r->pix_last = to ? (unsigned)strtoul(to, NULL, 10) : 0xFFFFFFFFu;
#ifdef _WIN32
        _setmode(_fileno(stdout), _O_BINARY); /* the pictures are binary */
#endif
    }
    if (getenv("WAVOUT") || getenv("SNDCOUNT")) { /* audio of the replay : one frame of sound per NMI */
        if (getenv("WAVOUT")) {
            wav_file = fopen(getenv("WAVOUT"), "wb");
            if (!wav_file) {
                fprintf(stderr, "tmnt_replay: cannot create %s (%s)\n", getenv("WAVOUT"), strerror(errno));
                return 1;
            }
            wav_header(wav_file, 0); /* sizes patched at the end */
        }
        if (getenv("SNDCOUNT")) {
            snd_count_file = fopen(getenv("SNDCOUNT"), "w");
        }
        g->sound_tap = sound_tap;
    }
    game_init(g);
#ifdef SNDDIFF
    if (getenv("SNDALL")) {
        play_every_sound(g);
        fclose(g->trace);
        return 0;
    }
#endif
    if (getenv("TMNT_LARGE")) {
        check_large = 1;
        atexit(large_view_report);
    }
    if (getenv("TMNT_FIXES")) { /* tests only: the fixes of FIXES.md on (they are off otherwise, and then the replays are the original's) */
        fix_set_all(1);
    }
    if (getenv("TMNT_HITBOX")) { /* fills the hit box journal (tests: the game must not change, and the formula must match the game's boxes) */
        hitbox_set_enabled(1);
    }
    for (g->frame = 1; g->frame <= r->frames && !g->left_attract; g->frame++) {
        run_frame(r);
    }
    if (hitbox_enabled()) {
        fprintf(stderr, "hitbox: %u boxes read from the game, %u differ from the formula\n", hitbox_checked(), hitbox_mismatches());
    }
    audio_finish();
    fclose(g->trace);
    return 0;
}
