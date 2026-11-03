/* Hardware services. For now they record what would be sent to the hardware (trace);
 * PPU rendering and audio come next. Whatever touches RAM is reproduced exactly. */
#include "game.h"
#include "hal.h"
#include "nmi.h"
#include "sound.h"

void set_chr_bank0(Game *g, uint8_t b) {
    M(ChrBank0) = b;
    g->chr_bank0 = b & 31;
}
void set_chr_bank1(Game *g, uint8_t b) {
    M(ChrBank1) = b;
    g->chr_bank1 = b & 31;
}

/* LoadScreen (B7 $C99F): writes a background directly into the PPU (display off).
 * x = offset in the pointer table $E3EB; the data is in bank 7 if the address is >= $C000,
 * otherwise in the given bank (-1: the current bank, not handled -> trace). */
static void load_screen_impl(Game *g, int bank, uint8_t x, uint8_t fc, uint8_t df);

void load_screen(Game *g, int bank, uint8_t x) {
    load_screen_impl(g, bank, x, 0, 0);
}

/* B7 $C992: same with the vertical scroll shifted ($FC = $E8, $03DF = 2): the lower screen page */
void load_screen_alt(Game *g, uint8_t x) {
    load_screen_impl(g, -1, x, 0xE8, 2);
}

void set_chr_bank0_raw(Game *g, uint8_t b) {
    g->chr_bank0 = b & 31;
}
void set_chr_bank1_raw(Game *g, uint8_t b) {
    g->chr_bank1 = b & 31;
}

static void load_screen_impl(Game *g, int bank, uint8_t x, uint8_t fc, uint8_t df) {
    M(ScrollYShadow) = fc;
    M(NametableBit) = df;
    uint16_t pn = rom_word_native(g->rom, 7, (uint16_t)(0xE3EB + x)); /* address in the loaded ROM; converted back to an EU address once the bank is known */
    int us = rom_is_us(g->rom);                                       /* US ROM: the screen data is read as it is (its sizes differ from the EU ones) */
    int b = pn >= 0xC000 ? 7 : (bank < 0 ? g->prg_bank : bank);       /* bank < 0: the one visible at $8000 */
    uint16_t p = pn, p0 = us ? rom_ptr_to_eu(g->rom, b, pn) : pn;
    TRACE("f=%u LoadScreen bank=%d x=%02X ptr=%04X\n", g->frame, bank, x, p);
    blank_ppu(g);
    M(ScrollXShadow) = 0;
#define DATA(o) (us ? rom_prg_native(g->rom, b, (uint16_t)(p + (o))) : rom_prg(g->rom, b, (uint16_t)(p + (o))))
    for (unsigned iterations = 0;; iterations++) { /* $C9B7 */
        LOOP_GUARD(iterations)
        ppu_read_status(g);
        ppu_write(g, 6, DATA(1));
        ppu_write(g, 6, DATA(0));
        p = (uint16_t)(p + 2);
        for (unsigned iterations = 0;; iterations++) { /* $C9CE */
            LOOP_GUARD(iterations)
            uint8_t cmd = DATA(0);
            if (cmd == 0xFF) {
                goto done;
            }
            if (cmd == 0x7F) {
                p = (uint16_t)(p + 1);
                break;
            }
            if (cmd & 0x80) { /* n literal bytes */
                int n = cmd & 0x7F;
                for (int i = 1; i <= n; i++) {
                    ppu_write(g, 7, DATA(i));
                }
                p = (uint16_t)(p + 1 + n);
            } else { /* one byte repeated cmd times (0 = 256) */
                int n = cmd ? cmd : 256;
                for (int i = 0; i < n; i++) {
                    ppu_write(g, 7, DATA(1));
                }
                p = (uint16_t)(p + 2);
            }
        }
    }
done:
#undef DATA
    /* A screen load runs past the end of the frame: the NMI fires during the logic ("lost frame", path
     * $C3A8) and only does the scroll and the sound update. Model measured on Mesen: one more update
     * every ~2600 $2007 writes in the logical frame (true for 11 frames out of 12 in the title and the intro). */
    /* Measured exception: the group screen ($BB8D, Intro_State6) does not lose a frame, although its weight predicts it. */
    if (p0 != 0xBB8D) {
        g->frame_vram_bytes += g->ppu_writes_acc;
    }
    g->ppu_writes_acc = 0;
    while (g->frame_vram_bytes >= 2600u * (g->frame_lag_updates + 1)) {
        g->frame_lag_updates++;
        sound_update(g);
    }
    M(DisplayDelay) = 3;
    M(PpuCtrlShadow) |= 0x80; /* $C49C */
    ppu_write(g, 0, M(PpuCtrlShadow));
}

void load_screen_and_clear(Game *g) {
    load_screen(g, -1, 0);
    for (int y = 0x3F; y >= 0; y--) {
        M(AttrBufferA + y) = 0;
        M(AttrBufferB + y) = 0;
    }
}

/* B7 $F75F: copies a palette script (bank 7) into PaletteBuffer; x = start index, y = position in the script.
 * Entries that are multiples of 4 (background colour) take ZP $2C and do not consume a script byte. */
static void palette_script(Game *g, uint16_t p, uint8_t x, uint8_t y) {
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        uint8_t v = rom_prg_native(g->rom, 7, (uint16_t)(p + y)); /* script read as it is: in the US ROM its address is the one of the loaded ROM */
        if (v == 0xFE) {
            break;
        }
        if ((x & 3) == 0) {
            v = M(BackdropColor);
        } else {
            y++;
        }
        M(PaletteBuffer + x) = v;
        if (++x == 0) {
            break;
        }
    }
    M(PaletteDirty) = 1;
}

/* B7 $F74D : A = preset (table en $FA55) -> PaletteBuffer */
void load_palette_preset(Game *g, uint8_t a) {
    uint16_t p = rom_word_native(g->rom, 7, (uint16_t)(0xFA55 + a * 2));
    palette_script(g, p, rom_prg_native(g->rom, 7, p), 1);
    TRACE("f=%u LoadPalettePreset %02X\n", g->frame, a);
}

/* B7 $F737: A = index in the $F7B5 table (palette scripts from x = 0) */
void load_level_palette(Game *g, uint8_t a) {
    palette_script(g, rom_word_native(g->rom, 7, (uint16_t)(0xF7B5 + (uint8_t)(a << 1))), 0, 0);
}

/* B7 $F7A3 */
void fill_palette_backdrop(Game *g, uint8_t color) {
    M(BackdropColor) = color;
    for (int y = 0; y != 0x20; y += 4) {
        M(PaletteBuffer + y) = color;
    }
    M(PaletteDirty) = 1;
}

/* B7 $CAB2: PpuBuffer = $0300 (256 bytes, the index is a byte) */
void ppu_buffer_append(Game *g, uint8_t v) {
    M(PpuBuffer + M(PpuBufferIndex)) = v;
    M(PpuBufferIndex)++;
}

/* B7 $CA7B / $CA75: A = text index (table at $CAD4), copies up to $FF/$FE */
void print_text_keep_tmp3(Game *g, uint8_t a) {
    M(Tmp2_) = a;
    uint16_t p = rom_word_visible(g->rom, 7, (uint16_t)(0xCAD4 + (uint8_t)(a << 1)), g->prg_bank);
    uint8_t x = M(PpuBufferIndex), y = 0;
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        uint8_t v = rdmem(g, (uint16_t)(p + y++)) /* the text may be in the bank visible at $8000 */;
        if (v == 0xFF || v == 0xFE) {
            break;
        }
        M(PpuBuffer + x) = v;
        if (a & 0x80) {
            if (M(Tmp3_) == 0) {
                M(PpuBuffer + x) = 0; /* erases the first letters (fade) */
            } else {
                M(Tmp3_)--;
            }
        }
        if (++x == 0) {
            M(PpuBuffer + 0) = 0xFF; /* overflow: the routine adds $FF */
            x = 1;
            break;
        }
    }
    M(PpuBufferIndex) = x;
}

void print_text(Game *g, uint8_t a) {
    M(Tmp3_) = 3;
    print_text_keep_tmp3(g, a);
}

void clear_game_ram_from(Game *g, int x) {
    for (; x != 0xE0; x++) {
        M(Tmp0 + x) = 0;
        if (x == 0xC7) {
            x += 3; /* skips $C8-$CA */
        }
    }
    for (int i = 0; i < 0x100; i++) {
        M(EntSprite + i) = 0;
        M(EntHurtTimer + i) = 0;
        M(0x0600 + i) = 0;
        M(TerrainMap + i) = 0;
    }
    for (int i = 0xC0; i < 0x100; i++) {
        M(PpuBuffer + i) = 0;
    }
}

void clear_game_ram30(Game *g) {
    clear_game_ram_from(g, 0x30);
}
void clear_game_ram48(Game *g) {
    clear_game_ram_from(g, 0x48);
}

void set_delay_timer_value(Game *g, uint8_t lo, uint8_t hi) {
    M(DelayTimerLo) = lo;
    M(DelayTimerHi) = hi;
}
void set_delay_timer(Game *g) {
    set_delay_timer_value(g, 0, 1);
}

/* B7 $C6C8: returns 1 while the delay is running (16 bits) */
int tick_delay_timer(Game *g) {
    if ((M(DelayTimerLo) | M(DelayTimerHi)) == 0) {
        return 0;
    }
    if (M(DelayTimerLo) == 0) {
        M(DelayTimerHi)--;
    }
    M(DelayTimerLo)--;
    return 1;
}

/* B7 $D565: $05B1 = colour specific to CurrentTurtle */
void set_turtle_sprite_palette(Game *g) {
    M(0x05B1) = rom_prg(g->rom, 7, (uint16_t)(0xD692 + M(CurrentTurtle)));
}
