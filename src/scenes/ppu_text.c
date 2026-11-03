/* PPU text engine (B7 $F23F): fill blocks and typewriter texts. */
#include "game.h"
#include "scenes.h"
#include "sound.h"
#include "scenes/ppu_text.h"
#define TODO_AREA "scene" /* prefix of the TODO traces of this file */

/* --- PPU text engine (B7 $F23F): A (return) is only used for the comparison with $FE by the caller --- */
static int ppu_script_ptr(Game *g, uint16_t p, int fill, uint8_t *ra) /* $F259 (fill) / $F301 (text): the script pointer is already chosen */
{
    if (fill) { /* fill block */
        if (!M(PpuTextBusy)) {
            if (M(PpuBufferIndex) >= 0x38) {
                *ra = M(PpuBufferIndex);
                return 0;
            }
            uint8_t x = M(PpuBufferIndex);
            M(PpuTextBusy) = 1;
            M(PpuTextIndex) = 0;
            for (int i = 0; i < 4; i++) {
                uint8_t v = rdmem(g, (uint16_t)(p + i));
                M(PpuBuffer + x++) = v;
                if (i == 1) {
                    M(PpuTextAddrHi) = v;
                }
                if (i == 2) {
                    M(PpuTextAddrLo) = v;
                }
                if (i == 3) {
                    M(PpuTextStep) = v;
                    *ra = v;
                }
            }
            M(PpuBufferIndex) = x;
            M(PpuTextIndex) = 4;
            return 0;
        }
        if (M(PpuBufferIndex) >= 0x38) {
            *ra = M(PpuBufferIndex);
            return 0;
        }
        uint8_t y = M(PpuTextIndex), b = rdmem(g, (uint16_t)(p + y)), c = rdmem(g, (uint16_t)(p + y + 1));
        if (!(M(PpuTextAddrLo) < c) && M(PpuTextAddrHi) >= b) {
            M(PpuTextBusy) = 0;
            *ra = 0;
            return 1;
        }
        uint8_t x = M(PpuBufferIndex);
        M(PpuBuffer + x++) = rdmem(g, p);
        M(PpuBuffer + x++) = M(PpuTextAddrHi);
        M(PpuBuffer + x++) = M(PpuTextAddrLo);
        M(PpuBuffer + x++) = M(PpuTextStep);
        M(PpuBufferIndex) = x;
        unsigned lo = M(PpuTextAddrLo) + 0x20; /* $F28E */
        M(PpuTextAddrLo) = (uint8_t)lo;
        M(PpuTextAddrHi) = (uint8_t)(M(PpuTextAddrHi) + (lo > 0xFF));
        *ra = M(PpuTextAddrHi);
        return 0;
    }
    if (!M(PpuTextBusy)) { /* "typewriter" text script */
        M(PpuTextBusy) = 1;
        M(PpuTextAddrHi) = rdmem(g, p);
        M(PpuTextAddrLo) = rdmem(g, (uint16_t)(p + 1));
        M(PpuTextIndex) = M(PpuTextStep) = 2;
        M(PpuTextDelay) = 1;
    }
    if (--M(PpuTextDelay)) {
        *ra = 1;
        return 0;
    }
    M(PpuTextDelay) = 4;
    uint8_t y = M(PpuTextIndex), b = rdmem(g, (uint16_t)(p + y));
    if (b == 0xFF) {
        M(PpuTextBusy) = 0;
        *ra = 0;
        return 1;
    }
    if (b == 0xFE) {
        M(PpuTextIndex)++;
        y = M(PpuTextIndex);
        M(PpuTextAddrHi) = rdmem(g, (uint16_t)(p + y));
        M(PpuTextAddrLo) = rdmem(g, (uint16_t)(p + y + 1));
        M(PpuTextIndex) = (uint8_t)(y + 2);
        M(PpuTextStep) = 2;
        M(PpuTextDelay) = 1;
        *ra = 1;
        return 0;
    }
    y = M(PpuBufferIndex);
    if (y >= 0x38) {
        M(PpuTextDelay) = 1;
        *ra = 1;
        return 0;
    }
    M(PpuBuffer + y++) = 1;
    M(PpuBuffer + y++) = M(PpuTextAddrHi);
    M(PpuBuffer + y++) = (uint8_t)(M(PpuTextAddrLo) + M(PpuTextStep));
    M(PpuBuffer + y++) = b & 0x3F;
    if (b & 0x80) {
        M(PpuBuffer + y++) = 1;
        M(PpuBuffer + y++) = M(PpuTextAddrHi);
        uint8_t v = (uint8_t)(M(PpuTextAddrLo) + M(PpuTextStep));
        M(PpuBuffer + y) = (uint8_t)(v - 0x20);
        if (v < 0x20) {
            M(PpuBuffer + y - 1)--;
        }
        y++;
        M(PpuBuffer + y++) = (b & 0x40) ? 0x3E : 0x30;
    }
    M(PpuTextIndex)++;
    M(PpuTextStep)++;
    M(PpuBufferIndex) = y;
    *ra = 0;
    if (b) {
        sound_play(g, 0x38);
        *ra = 0x38;
    }
    return 0;
}

int ppu_script(Game *g, uint8_t in, uint8_t *ra) {
    if (in & 0x80) {
        return ppu_script_ptr(g, rom_word(g->rom, 7, (uint16_t)(0xF3EF + ((in & 0x7F) << 1))), 1, ra);
    }
    return ppu_script_ptr(g, rom_word(g->rom, 7, (uint16_t)(0xF3F7 + (in << 1))), 0, ra);
}

int ppu_fill_script(Game *g, uint16_t p) {
    uint8_t ra;
    return ppu_script_ptr(g, p, 1, &ra);
} /* B7 $F259 */
int ppu_text_script(Game *g, uint16_t p) {
    uint8_t ra;
    return ppu_script_ptr(g, p, 0, &ra);
}
