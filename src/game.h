/* C port of TMNT (Europe) - game state: the 2 KB RAM of the NES, as it is, so that it can be compared
 * byte by byte with the emulator. The names come from symbols.txt (ram_map.h). */
#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include <stdio.h>
#include "ram_map.h"
#include "rom.h"
#include "ppu.h"
#include "apu.h"

/* NES PPU: only what is needed to display a frame (no per-scanline timing) */
typedef struct Ppu {
    uint8_t vram[0x800]; /* 2 KB of name tables; horizontal mirroring (MMC1 control = $1F) */
    uint8_t pal[32];
    uint8_t oam[256];
    uint8_t ctrl, mask, w, x; /* registers $2000, $2001, write toggle, fine X */
    uint16_t v, t;            /* internal "loopy" addresses */
    uint8_t status_read;
    uint8_t split_on; /* scroll split after sprite 0 (status bar of the side view) */
    uint8_t split_x;
    uint8_t mirror_v; /* MMC1 control $1E: vertical mirroring (NT0=NT2, NT1=NT3); otherwise horizontal ($1F) */
    uint16_t split_v;
} Ppu;

typedef struct Apu {
    uint8_t enabled, five_step, cycle_odd;
    uint8_t duty[2], len_halt[4], const_vol[4], env_period[4], env_start[4], env_decay[4], env_div[4];
    uint8_t sweep_on[2], sweep_period[2], sweep_neg[2], sweep_shift[2], sweep_reload[2], sweep_div[2];
    uint8_t pulse_cnt_dummy;
    uint16_t pulse_timer[2], pulse_cnt[2];
    uint8_t pulse_phase[2], len[4];
    uint8_t tri_ctrl, tri_reload_val, tri_reload, tri_lin, tri_phase, noise_mode;
    uint16_t tri_timer, tri_cnt, noise_per, noise_cnt, lfsr;
    int fs_cycle;
    double cyc_acc, hp;
} Apu;

typedef struct Game {
    uint8_t ram[0x800];
    Rom *rom;
    Ppu ppu;
    Apu apu;
    unsigned frame_vram_bytes, frame_lag_updates; /* per logical frame: see load_screen() */
    unsigned ppu_writes_acc;                      /* bytes written to $2007 since the last LoadScreen (measures the duration) */
    uint8_t pad;                                  /* state of the buttons of controller 1 (A=80 B=40 Select=20 Start=10 up=08 down=04 left=02 right=01) */
    uint8_t prg_bank;                             /* PRG bank visible at $8000 (MMC1 register): used by the level data pointers */
    uint8_t chr_bank0, chr_bank1;                 /* MMC1: 4 KB CHR banks ($0000 and $1000) */
    /* observation output */
    FILE *trace;
    uint32_t frame;
    int left_attract; /* 1 when the port leaves the title/intro scope (GameMode >= 4) */
} Game;

#ifdef CHECKED /* diagnostic build (tmnt_replay_checked.exe) : every invariant is verified, any failure stops the program */
#include <stdlib.h>
#define ASSERT(c)                                                                                                                                              \
    do {                                                                                                                                                       \
        if (!(c)) {                                                                                                                                            \
            fprintf(stderr, "ASSERT %s:%d: %s\n", __FILE__, __LINE__, #c);                                                                                     \
            fflush(stderr);                                                                                                                                    \
            abort();                                                                                                                                           \
        }                                                                                                                                                      \
    } while (0)
static inline uint8_t *checked_ram(Game *g, unsigned a, const char *f, int l) {
    if (a >= 0x800) {
        fprintf(stderr, "ASSERT %s:%d: RAM index $%X out of range\n", f, l, a);
        fflush(stderr);
        abort();
    }
    return &g->ram[a];
}
#define M(a) (*checked_ram(g, (unsigned)(a), __FILE__, __LINE__))
#else
#define ASSERT(c) ((void)0)
#ifdef WATCHZP
extern uint8_t *dbg_ptr(Game *g, unsigned a, const char *f, int l);
#define M(a) (*dbg_ptr(g, (unsigned)(a), __FILE__, __LINE__))
#else
#define M(a) (g->ram[(a)])
#endif
#endif
#define ZP_2E 0x2E
#define ZP_37 0x37
#define ZP_8F 0x8F
#define Tmp1_ (Tmp0 + 1)
#define Tmp2_ (Tmp0 + 2)
#define Tmp3_ (Tmp0 + 3)

/* read through a 6502 pointer: RAM, fixed bank 7, or the current bank at $8000 (g->prg_bank) */
static inline uint8_t rdmem(const Game *g, uint16_t a) {
    if (a < 0x2000) {
        return g->ram[a & 0x7FF]; /* RAM and its mirrors ($0800-$1FFF) */
    }
    if (a >= 0xC000) {
        return rom_prg(g->rom, 7, a);
    }
    if (a >= 0x8000) {
        return rom_prg(g->rom, g->prg_bank, a);
    }
    return (uint8_t)(a >> 8); /* $4020-$7FFF: nothing on the bus, it keeps the high byte of the pointer read just before */
}

/* read at base + offset: in the US ROM we start from the real address of the base and advance byte by byte, like the 6502 (an offset that leaves the table
 * reads the neighbouring data of the US ROM, not what the EU -> US address translation would put there). */
static inline uint8_t rdmem_off(const Game *g, uint16_t base, uint16_t off) {
    if (base >= 0x8000 && rom_is_us(g->rom)) {
        int bank = base >= 0xC000 ? 7 : g->prg_bank;
        uint16_t nb = rom_eu_to_native(g->rom, bank, base);
        return rom_prg_native(g->rom, bank, (uint16_t)(nb + off));
    }
    return rdmem(g, (uint16_t)(base + off));
}
static inline uint16_t ptr(const Game *g, uint8_t zp) {
    return (uint16_t)(g->ram[zp] | g->ram[zp + 1] << 8);
}

#define TRACE(...)                                                                                                                                             \
    do {                                                                                                                                                       \
        if (g->trace)                                                                                                                                          \
            fprintf(g->trace, __VA_ARGS__);                                                                                                                    \
    } while (0)

/* Loops whose end depends on data read from the ROM: LOOP_GUARD(counter) leaves the loop (with a TODO trace) instead of hanging if the data never ends.
 * The counter is a local `unsigned` declared in the `for` header; the limit is far above anything a valid ROM needs. */
/* ROM bank numbers and byte readers : R<n>(address) reads the byte at CPU address `address` of PRG bank n (needs `g` in scope). */
#define B0 0
#define B1 1
#define B2 2
#define B3 3
#define B4 4
#define B5 5
#define B6 6
#define B7 7
#define R0(a) rom_prg(g->rom, 0, (uint16_t)(a))
#define R1(a) rom_prg(g->rom, 1, (uint16_t)(a))
#define R2(a) rom_prg(g->rom, 2, (uint16_t)(a))
#define R3(a) rom_prg(g->rom, 3, (uint16_t)(a))
#define R4(a) rom_prg(g->rom, 4, (uint16_t)(a))
#define R5(a) rom_prg(g->rom, 5, (uint16_t)(a))
#define R6(a) rom_prg(g->rom, 6, (uint16_t)(a))
#define R7(a) rom_prg(g->rom, 7, (uint16_t)(a))
/* Trace of a code path that is not ported yet: every file defines TODO_AREA (a string) before using it. */
#define TODO(what) TRACE("f=%u TODO " TODO_AREA " %s\n", g->frame, what)

#define LOOP_LIMIT 1000000u
#define LOOP_GUARD(n)                                                                                                                                          \
    if ((n) > LOOP_LIMIT) {                                                                                                                                    \
        TRACE("f=%u TODO loop does not end at %s:%d\n", g->frame, __FILE__, __LINE__);                                                                         \
        break;                                                                                                                                                 \
    }

/* Small game data tables: read from the ROM at launch (at their disassembly address, given bank), not copied into the C.
 * ROM_TABLE / ROM_TABLE2 declare a local array filled from the ROM; ROM_AT(bank, address, i) reads byte i of a table. Need `g` in scope. */
#define ROM_AT(bank, addr, i) rom_prg(g->rom, (bank), (uint16_t)((addr) + (i)))
#define ROM_TABLE(name, n, bank, addr)                                                                                                                         \
    uint8_t name[n];                                                                                                                                           \
    for (int i_ = 0; i_ < (n); i_++)                                                                                                                           \
    name[i_] = ROM_AT(bank, addr, i_)
#define ROM_TABLE2(name, a, b, bank, addr)                                                                                                                     \
    uint8_t name[a][b];                                                                                                                                        \
    for (int i_ = 0; i_ < (a) * (b); i_++)                                                                                                                     \
    ((uint8_t *)name)[i_] = ROM_AT(bank, addr, i_)

/* Dispatch on a ROM routine address: the original jumps to the address read from a ROM table ; the port keeps a table of the routines it knows.
 * ROUTE_RUN returns 0 when the address is not in the table (routine not ported : the caller traces a TODO). route_nop = known routine that does nothing. */
typedef struct {
    uint16_t rom_addr;
    void (*run)(Game *g, int x);
} Route;
static inline void route_nop(Game *g, int x) {
    (void)g;
    (void)x;
}
static inline int route_run(const Route *table, int n, uint16_t addr, Game *g, int x) {
    for (int i = 0; i < n; i++) {
        if (table[i].rom_addr == addr) {
            table[i].run(g, x);
            return 1;
        }
    }
    return 0;
}
#define ROUTE_RUN(table, addr, g, x) route_run(table, (int)(sizeof(table) / sizeof((table)[0])), (addr), (g), (x))

#endif
