/* Types $A7 / $A8: goes back and forth on a platform and fires. */
#include "game.h"
#include "enemy.h"
#include "enemies/bouncer.h"
#include "enemies/purple_ninja.h"
#include "enemies/walker_dropper.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- types $A7 / $A8 (B5 $A0BA-$A31E): goes back and forth on the platform, fires a projectile when the player is facing it --- */
static int platform_shooter_reverse(Game *g, int x) /* B5 $A2D8: reverses the X velocity and the direction; returns the 6502 carry */
{
    uint8_t lo = M(EntVelXFrac + x), hi = M(EntVelX + x);
    int c1 = lo == 0;
    M(EntVelXFrac + x) = (uint8_t)(~lo + 1);
    M(EntVelX + x) = (uint8_t)(~hi + c1);
    int cf = hi == 0 && c1;
    M(EntTimer + x) ^= 0x02;
    M(EntFlags + x) &= 0xFE; /* $A307 */
    if (M(EntTimer + x) & 0x02) {
        M(EntFlags + x) |= 0x01;
    }
    return cf;
}

void ai_platform_shooter(Game *g, int x, uint8_t first) /* B5 $A0BF ($A7, $0550 = 0) / $A0BA ($A8, $0550 = 1) */
{
    M(EntTimer + x) = first;
    M(EntVelX + x) = 3;
    M(EntVelXFrac + x) = 0;
    if (M(EntX) < M(EntX + x)) {
        M(EntTimer + x) |= 0x02;
        M(EntVelX + x) = 0xFD;
    }
    M(EntVelY + x) = 0;
    M(EntVelYFrac + x) = 0;
    M(EntFlags + x) &= 0xFE; /* $A307 */
    if (M(EntTimer + x) & 0x02) {
        M(EntFlags + x) |= 0x01;
    }
    set_state(g, x, 0x21);
}

void platform_shooter_state(Game *g, int x) /* B5 $A0F5: state $21 (the 6502 carry is used further down: it is followed step by step) */
{
    int cf = 0;
    if (!(M(EntTimer + x) & 1)) {
        cf = 0xC7 >= M(EntAnimId + x);
        anim_if_changed(g, x, 0xC7);
        uint8_t nib = M(EntVar0 + x) & 0x0F;
        cf = nib >= 1;
        if (nib == 1) {
            cf = 0xC8 >= M(EntAnimId + x);
            anim_if_changed(g, x, 0xC8);
        }
    }
    M(Tmp2) = 0;
    int keep = 1;
    if ((M(EntTimer + x) & 0x20) && --M(EntWait + x) != 0) {
        keep = 0;
    } else if (M(EntTimer + x) & 0x20) {
        M(EntTimer + x) &= 0xDF;
    }
    if (keep) { /* $A127 */
        uint8_t dy = R5(0xA347 + (M(EntVar0 + x) & 0x0F));
        uint8_t dx = (M(EntTimer + x) & 0x02) ? 0xF4 : 0x0C;
        M(TmpE) = dx;
        M(TmpF) = dy;
        cf = tile_in_wall_set(g, x, dx, dy);
        if (!cf) {
            keep = 0;
        } else {
            cf = platform_shooter_reverse(g, x);
            M(Tmp2) = 1;
            if (M(EntTimer + x) & 0x40) {
                cf = tile_in_wall_set(g, x, (uint8_t)(0 - dx), dy);
                if (cf) {
                    M(EntWait + x) = 8;
                    M(EntTimer + x) |= 0x20;
                }
            }
            M(EntTimer + x) |= 0x40;
        }
    }
    if (!keep) {
        M(EntTimer + x) &= 0xBF; /* $A17C */
    }
    /* $A184: distance to the player, M0 = |difference|, M1 = $80 if the player is on the left */
    int r = (int)M(EntX) - (int)M(EntX + x) - (cf ? 0 : 1);
    M(Tmp1) = 0;
    if (r >= 0) {
        M(Tmp0) = (uint8_t)r;
        cf = 1;
    } else {
        M(Tmp0) = (uint8_t)(-r);
        M(Tmp1) = 0x80;
        cf = 0;
    }
    uint8_t nib = M(EntVar0 + x) & 0x0F;
    if (nib == 2) {
        M(Tmp8) = 1;
    } else if (nib == 1) {
        M(Tmp8) = 0;
    } else { /* $A28A: shot */
        if (M(EntTimer + x) & 1) {
            if (M(Tmp0) >= 8) {
                return;
            }
            spawn_child_slot(g, x, 0x25);
            M(EntTimer + x) &= 0xFE;
        }
        if (M(EntTimer + x) & 4) {
            return;
        }
        uint8_t d = M(Tmp0) >= 0x40 ? (uint8_t)(M(Tmp0) - 0x40) : (uint8_t)(0x40 - M(Tmp0));
        if (d >= 8) {
            return;
        }
        M(EntTimer + x) |= 4;
        M(EntVar0 + x)++;
        M(EntVar1 + x) = 0;
        if ((M(EntVelX + x) & 0x80) != M(Tmp1)) {
            platform_shooter_reverse(g, x);
        }
        return;
    }
    if (M(Tmp2)) { /* $A1C1 */
        M(EntVar0 + x)++;
        if (M(Tmp8)) {
            M(EntVar0 + x) = M(Tmp8);
        }
        M(EntVelY + x) = 0;
        M(EntVelYFrac + x) = 0;
        return;
    }
    if (!(M(EntTimer + x) & 8)) { /* $A1D4 */
        M(EntTimer + x) |= 8;
        uint8_t dy = (uint8_t)(M(EntY) - M(EntY + x));
        M(Tmp4) = dy;
        if (M(EntY) < M(EntY + x)) {
            M(EntVar0 + x) = 0;
            return;
        }
        M(EntTimer + x) &= 0xEF;
        if (dy < 0x40) {
            M(EntTimer + x) |= 0x10;
        }
    }
    uint8_t i = (uint8_t)(M(EntVar1 + x) << 1); /* $A20B */
    if (M(EntTimer + x) & 0x10) {
        M(EntVelY + x) = R5(0xA31F + i);
        M(EntVelYFrac + x) = R5(0xA320 + i);
    } else {
        M(EntVelY + x) = R5(0xA333 + i);
        M(EntVelYFrac + x) = R5(0xA334 + i);
    }
    if (M(Tmp8)) { /* $A234: goes up */
        unsigned f = (uint8_t)~M(EntVelYFrac + x) + 1u;
        M(EntVelYFrac + x) = (uint8_t)f;
        M(EntVelY + x) = (uint8_t)((uint8_t)~M(EntVelY + x) + (f > 0xFF));
    }
    uint8_t s = (uint8_t)(M(EntVar0 + x) + 0x10); /* $A24D */
    if (s < 0x20) {
        M(EntVar0 + x) = s;
        return;
    }
    M(Tmp9) = s & 0x0F;
    if (M(Tmp8)) {
        if (--M(EntVar1 + x) == 0) {
            M(EntVar0 + x) = 0;
            M(EntVelY + x) = 0;
            M(EntVelYFrac + x) = 0;
            return;
        }
    } else if (++M(EntVar1 + x) == 9) {
        M(EntVar0 + x)++;
        return;
    }
    M(EntVar0 + x) = M(Tmp9);
}
