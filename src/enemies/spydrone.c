/* Type $AA: spydrone. */
#include "game.h"
#include "anim.h"
#include "enemy.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* ------------------------------------------------------------ spydrone ($AA): B5 $A34F, state $22 ($A357), vectors $A944/$A9B0 --- */
static void negate16(Game *g, uint8_t hi_addr, uint8_t lo_addr) /* eor #$FF / clc / adc #1, then eor #$FF / adc #0 */
{
    uint8_t lo = M(lo_addr), c = (lo == 0);
    M(lo_addr) = (uint8_t)(~lo + 1);
    M(hi_addr) = (uint8_t)(~M(hi_addr) + c);
}

static void vector_from_angle(Game *g, uint8_t n) /* B5 $A9B0: $00:$01 = X velocity, $02:$03 = Y velocity (quadrant in $08) */
{
    uint8_t y = (uint8_t)(n << 1);
    M(Tmp1) = R5(0xAA76 + y);
    M(Tmp0) = R5(0xAA86 + y);
    if (M(Tmp8) & 2) {
        negate16(g, 0x00, 0x01);
    }
    y++;
    M(Tmp3) = R5(0xAA76 + y);
    M(Tmp2) = R5(0xAA86 + y);
    if (!(M(Tmp8) & 1)) {
        negate16(g, 0x02, 0x03);
    }
}

void vector_to(Game *g, int x) /* B5 $A944: direction of entity x towards ($00, $01) */
{
    M(Tmp8) = 0;
    if (M(Tmp0) >= M(EntX + x)) {
        M(Tmp0) = (uint8_t)(M(Tmp0) - M(EntX + x));
    } else {
        M(Tmp0) = (uint8_t)(M(EntX + x) - M(Tmp0));
        M(Tmp8) += 2;
    }
    M(Tmp4) = M(Tmp0);
    if (M(Tmp1) >= M(EntY + x)) {
        M(Tmp1) = (uint8_t)(M(Tmp1) - M(EntY + x));
        M(Tmp8) += 1;
    } else {
        M(Tmp1) = (uint8_t)(M(EntY + x) - M(Tmp1));
    }
    M(Tmp5) = M(Tmp1);
    M(Tmp0) = (uint8_t)(M(Tmp0) >> 4);
    M(Tmp9) = (uint8_t)((M(Tmp1) & 0xF0) + M(Tmp0));
    uint8_t b = R5(0xA9F6 + (M(Tmp9) >> 1));
    M(Tmp9) = (M(Tmp9) & 1) ? (uint8_t)(b >> 4) : (uint8_t)(b & 0x0F);
    vector_from_angle(g, M(Tmp9));
}

static void spydrone_bob(Game *g, int x) /* B5 $9CF8: vertical oscillation */
{
    if (--M(EntVar0 + x) == 0) {
        M(EntVar0 + x) = 2;
        M(EntVar1 + x) = (uint8_t)(M(EntVar1 + x) + M(EntWait + x));
        uint8_t a = M(EntVar1 + x) & 0x7F;
        int flip = 0;
        if (a == 0) {
            if (M(EntVar1 + x) & 0x80) {
                M(EntVar1 + x) &= 0x7F;
            } else {
                M(EntVar1 + x) |= 0x80;
            }
            flip = 1;
        } else if (a == 4) {
            flip = 1;
        }
        if (flip) {
            M(EntWait + x) = (uint8_t)(0 - M(EntWait + x)); /* $9D37 */
        }
    }
    uint8_t y = (uint8_t)((M(EntVar1 + x) & 0x7F) << 1); /* $9D42 / $9D69 */
    M(EntVelY + x) = R5(0x9D7E + y);
    M(EntVelYFrac + x) = R5(0x9D7F + y);
    if (M(EntVar1 + x) & 0x80) {
        uint8_t lo = M(EntVelYFrac + x), c = (lo == 0);
        M(EntVelYFrac + x) = (uint8_t)(~lo + 1);
        M(EntVelY + x) = (uint8_t)(~M(EntVelY + x) + c);
    }
}

void spydrone_chase_state(Game *g, int x) /* B5 $A357 */
{
    spydrone_bob(g, x);
    if (!M(EntTimer + x)) {
        uint8_t dx = (M(EntX + x) >= M(EntX)) ? (uint8_t)(M(EntX + x) - M(EntX)) : (uint8_t)(M(EntX) - M(EntX + x));
        if (dx >= 0x40) {
            return;
        }
        M(Tmp0) = M(EntX);
        M(Tmp1) = M(EntY);
        vector_to(g, x);
        M(EntTimer + x) = (uint8_t)((M(Tmp8) << 4) | M(Tmp9) | 0x80);
    } else {
        M(Tmp8) = (uint8_t)((M(EntTimer + x) & 0x7F) >> 4);
        M(Tmp9) = M(EntTimer + x) & 0x0F;
        vector_from_angle(g, M(Tmp9));
    }
    M(EntVelX + x) = M(Tmp0);
    M(EntVelXFrac + x) = M(Tmp1);
    unsigned sum = M(Tmp3) + M(EntVelYFrac + x);
    M(EntVelYFrac + x) = (uint8_t)sum;
    M(EntVelY + x) = (uint8_t)(M(Tmp2) + M(EntVelY + x) + (sum > 0xFF));
}

void ai_spydrone(Game *g, int x) /* B5 $A34F */
{
    M(EntWait + x) = 0xFF; /* $9C95 */
    M(EntVar0 + x) = 2;
    M(EntVar1 + x) = 4;
    M(EntRoutine + x) = 0x22; /* $8644 */
    M(EntState + x)++;
}

void ai_dam_scenery_swap(Game *g, int x) /* B5 $8E79: dam scenery, changes type after $50 frames */
{
    if (M(FrameCounter) < 0x50) {
        return;
    }
    uint8_t i = (uint8_t)((M(EntType + x) - 0x1F) << 1);
    M(EntType + x) = R5(0x8E93 + i);
    set_animation(g, x, R5(0x8E94 + i));
}

void ai_scenery_swap(Game *g, int x) /* B5 $8E9B */
{
    if (M(FrameCounter) >= 0x50) {
        return;
    }
    uint8_t i = (uint8_t)((M(EntType + x) - 0x23) << 1);
    M(EntType + x) = R5(0x8EB5 + i);
    set_animation(g, x, R5(0x8EB6 + i));
}

void rotbar_visibility(Game *g, int x) /* B5 $8E2E: the "on screen" flag ($02) follows the position relative to the pivot (entity 5) */
{
    uint8_t cx = M(EntX + 5), px = M(EntX + x), on;
    if (cx < 0x80) {
        if (px < 0x08) {
            on = 0;
        } else {
            uint8_t d = (uint8_t)(cx - 0x38);
            on = (cx >= 0x38) || px < d;
        }
    } else {
        if (px >= 0xF8) {
            on = 0;
        } else {
            unsigned d = cx + 0x38;
            on = d <= 0xFF || px >= (uint8_t)d;
        }
    }
    if (on) {
        M(EntFlags + x) |= 0x02;
    } else {
        M(EntFlags + x) &= 0xFD;
    }
}

void ai_rotbar(Game *g, int x) /* B5 $8DE5: rotating bar, the segments rotate around entity 5 ($9084, offset 4) */
{
    uint16_t lx = (uint16_t)((uint16_t)(M(EntX + x) << 8 | M(EntXFrac + x)) - (uint16_t)(M(EntX + 5) << 8 | M(EntXFrac + 5)));
    uint16_t ly = (uint16_t)((uint16_t)(M(EntY + x) << 8 | M(EntYFrac + x)) - (uint16_t)(M(EntY + 5) << 8 | M(EntYFrac + 5)));
    ly = (uint16_t)(ly - (uint16_t)((int16_t)lx >> 4));
    lx = (uint16_t)(lx + (uint16_t)((int16_t)ly >> 4));
    uint16_t nx = (uint16_t)(lx + (uint16_t)(M(EntX + 5) << 8 | M(EntXFrac + 5)));
    uint16_t ny = (uint16_t)(ly + (uint16_t)(M(EntY + 5) << 8 | M(EntYFrac + 5)));
    M(EntX + x) = (uint8_t)(nx >> 8);
    M(EntXFrac + x) = (uint8_t)nx;
    M(EntY + x) = (uint8_t)(ny >> 8);
    M(EntYFrac + x) = (uint8_t)ny;
    rotbar_visibility(g, x);
}
