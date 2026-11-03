/* Type $AC: palette band that follows the scrolling. */
#include "game.h"
#include "player.h"
#include "enemy.h"
#include "enemies/bouncer.h"
#include "enemies/purple_ninja.h"
#include "enemies/walker_dropper.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- type $AC (B5 $A59F-$A850): palette band (background attributes) that follows the scrolling --- */
static void palette_band_add_wrap(Game *g) /* $A825: $16/$17 + $15, the height of a screen is $F0 */
{
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        unsigned s = M(Tmp16) + M(Tmp15);
        M(TmpE) = (uint8_t)s;
        if (s > 0xFF) {
            M(Tmp16) = (uint8_t)s;
            M(Tmp17)++;
            M(Tmp15) = 0x10;
            continue;
        }
        if ((uint8_t)s >= 0xF0) {
            M(TmpE) = (uint8_t)(s - 0xF0);
            M(TmpF) = (uint8_t)(M(Tmp17) + 1);
            return;
        }
        M(TmpF) = M(Tmp17);
        return;
    }
}

static void palette_band_sub_wrap(Game *g) /* $A7E1 : $16/$17 - $15 */
{
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        uint8_t a = M(Tmp16), b = M(Tmp15), d = (uint8_t)(a - b);
        M(TmpE) = d;
        if (a < b) {
            M(Tmp16) = d;
            M(Tmp17)--;
            M(Tmp15) = 0x10;
            continue;
        }
        if (d >= 0xF0) {
            M(TmpE) = (uint8_t)(d - 0x10);
            M(TmpF) = M(Tmp17);
            return;
        }
        M(TmpF) = M(Tmp17);
        return;
    }
}

static void palette_band_to_screen(Game *g) /* $A80B */
{
    M(Tmp16) = M(TmpE);
    M(Tmp15) = M(Tmp6);
    M(Tmp17) = M(Tmp7);
    palette_band_add_wrap(g);
    M(Tmp0) = M(TmpE);
    M(Tmp8) = M(TmpF) & 1;
}

static void palette_band_position(Game *g, int x) /* $A7CA */
{
    M(Tmp16) = M(EntY + x);
    M(Tmp15) = M(Tmp11);
    M(Tmp17) = M(Tmp7);
    palette_band_sub_wrap(g);
    M(Tmp7) = M(TmpF);
    palette_band_to_screen(g);
}

static void palette_band_buffer_write(Game *g, int x) /* $A776: writes "count, PPU address high, low, data" into the buffer $0300 (background attributes) */
{
    M(TmpD) = M(Tmp8);
    uint8_t a = (uint8_t)(((M(Tmp0) & 0xE0) >> 2) + M(EntTimer + x) + 0xC0);
    M(Tmp1) = M(TmpC) = a;
    if (M(PpuBufferIndex) >= 0x6C) {
        return;
    }
    M(Tmp2) = M(EntVar0 + x);
    uint8_t y = M(PpuBufferIndex);
    M(PpuBuffer + y) = M(Tmp2);
    M(PpuBuffer + ++y) = (uint8_t)((M(Tmp8) ? 8 : 0) + 0x23);
    M(PpuBuffer + ++y) = M(Tmp1);
    do {
        M(PpuBuffer + ++y) = M(Tmp3);
    } while (--M(Tmp2) != 0);
    M(PpuBufferIndex) = (uint8_t)(y + 1);
}

static void palette_band_strip(Game *g, int x) /* $A7BF */
{
    M(Tmp14) = 0;
    palette_band_position(g, x);
    palette_band_buffer_write(g, x);
}

static void palette_band_draw(Game *g, int x, uint8_t y) /* $A734: four attribute bands from the table $A851 */
{
    M(Tmp12) = R5(0xA851 + y);
    M(Tmp13) = R5(0xA852 + y);
    uint16_t t = (uint16_t)(M(Tmp12) | M(Tmp13) << 8);
    M(Tmp3) = 0;
    M(Tmp11) = 0x50;
    palette_band_strip(g, x);
    M(Tmp3) = R5(t);
    M(Tmp11) = 0x40;
    palette_band_strip(g, x);
    M(Tmp3) = R5(t + 1);
    M(Tmp11) = 0x20;
    palette_band_strip(g, x);
    uint8_t a = R5(t + 2);
    if (a != 0xFF) {
        M(Tmp3) = a;
        M(Tmp11) = 0;
        palette_band_strip(g, x);
    }
}

static void palette_band_fall(Game *g, int x) /* $A61A */
{
    M(EntVelY + x) = 2;
    M(EntVelYFrac + x) = 0x80;
    M(EntVelX + x) = 0;
    M(EntVelXFrac + x) = 0;
}

static void palette_band_row(Game *g, int x) /* $A60E: $0550 = the 3 high bits of $01 */
{
    M(EntTimer + x) = (M(Tmp1) >> 5) & 7;
}

void ai_palette_band(Game *g, int x) /* B5 $A59F */
{
    if (M(0x03EB) >= 2) {
        clear_entity(g, x);
        return;
    }
    M(0x03EB)++;
    M(EntAttr + x) = 2;
    if (M(EntY + x) >= 0x80) {
        M(EntY + x) = 8;
    }
    palette_band_fall(g, x);
    M(EntVar0 + x) = 1;
    M(Tmp0) = (uint8_t)((uint8_t)(M(EntX + x) + M(ScrollXShadow)) + M(CameraDeltaX));
    M(Tmp1) = M(Tmp0) & 0xE0;
    int path_a = 0;
    if (M(Tmp0) & 0x10) {
        path_a = 1;
    } else {
        palette_band_row(g, x);
        M(Tmp2) = (uint8_t)(M(Tmp0) - M(Tmp1));
        if (M(EntX + x) < M(Tmp2)) {
            path_a = 1;
        } else {
            M(EntX + x) = (uint8_t)(M(EntX + x) - M(Tmp2) + 0x10);
        }
    }
    if (path_a) { /* $A5DB */
        M(Tmp1) = (uint8_t)(M(Tmp1) + 0x20);
        palette_band_row(g, x);
        M(EntX + x) = (uint8_t)(M(Tmp1) - M(Tmp0) + M(EntX + x) + 0x10);
    }
    set_state(g, x, 0x24);
}

void palette_band_state(Game *g, int x) /* B5 $A62D */
{
    M(Tmp17) = M(NametableBit) >> 1;
    M(Tmp16) = M(ScrollYShadow);
    uint8_t sc = M(CameraDeltaY);
    if (!(sc & 0x80)) {
        M(Tmp15) = sc;
        palette_band_add_wrap(g);
    } else {
        M(Tmp15) = (uint8_t)(~sc + 1);
        palette_band_sub_wrap(g);
    }
    M(Tmp6) = M(TmpE);
    M(Tmp7) = M(TmpF);
    enum {
        SPAWN,
        FAIL,
        END5,
        EDGE
    } next = SPAWN;
    if (M(EntWait + x)) {
        if ((M(EntWait + x) & 0x0F) >= 5) {
            palette_band_fall(g, x);
            next = EDGE;
        }
    } else {
        uint8_t dxp = (uint8_t)(M(EntX) - M(EntX + x));
        uint8_t dyp = (uint8_t)(M(EntY + x) - M(EntY));
        if ((dxp >= 0x10 && dxp < 0xF0) || dyp >= 0x30) {
            next = EDGE;
        } else {
            M(Tmp0) = 1;
            M(EntVelY + x) = 0;
            M(EntVelYFrac + x) = 0;
        }
    }
    if (next == SPAWN) { /* $A694: one more element */
        int c = spawn_child_slot(g, x, 0x36);
        if (c < 0) {
            next = FAIL;
        } else {
            M(Tmp10) = (uint8_t)c;
            uint8_t y = (uint8_t)((M(EntWait + x) & 0xF0) >> 3);
            M(Tmp11) = y;
            uint8_t dx = R5(0xA86A + y);
            if (dx == 0xFF) {
                next = END5;
            } else {
                M(Tmp12) = dx;
                if (tile_in_wall_set(g, x, dx, R5(0xA86B + y))) {
                    M(EntType + c) = 0;
                    next = FAIL;
                } else {
                    M(EntX + c) = (uint8_t)(dx + M(EntX + x));
                    M(EntY + c) = (uint8_t)(R5(0xA86B + y) + M(EntY + x));
                    M(EntWait + x)++;
                    next = EDGE;
                }
            }
        }
    }
    if (next == FAIL) { /* $A6F1 */
        M(EntWait + x) = (uint8_t)(M(EntWait + x) + 0x10);
        next = M(EntWait + x) >= 0x90 ? END5 : EDGE;
    }
    if (next == END5) {
        M(EntWait + x) = 5; /* $A6C5 */
    }
    uint8_t ey = M(EntY + x), ex = M(EntX + x); /* $A6FE */
    if (ey >= 0xF8 || ey < 0x08 || ex >= 0xF8 || ex < 0x08) {
        if (--M(0x03EB) & 0x80) {
            M(0x03EB) = 0;
        }
        palette_band_draw(g, x, 8);
        clear_entity(g, x);
        return;
    }
    M(Tmp11) = 0;
    palette_band_position(g, x);
    palette_band_draw(g, x, (uint8_t)((M(Tmp0) & 0x18) >> 2));
}
