/* Type $AB: scripted trajectories then charge. */
#include "game.h"
#include "enemy.h"
#include "enemies/bouncer.h"
#include "enemies/common.h"
#include "enemies/walker_dropper.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- type $AB (B5 $A3C3-$A52C): follows scripted trajectories, then charges the nearby player --- */
void ai_swooper(Game *g, int x) {
    M(EntVelX + x) = 0;
    M(EntVelXFrac + x) = 0;
    M(EntVelY + x) = 0;
    M(EntVelYFrac + x) = 0;
    if (M(EntX) >= M(EntX + x)) {
        M(EntTimer + x) = 1;
    }
    set_state(g, x, 0x23);
}

void swooper_trajectory_state(Game *g, int x) /* B5 $A3E5 */
{
    if (M(EntVar0 + x) & 0x80) { /* scripted trajectory in progress or to be read */
        anim_if_changed(g, x, 0xCD);
        if (M(EntVar1 + x)) { /* $A448 : pause */
            M(EntVar1 + x)--;
            M(EntVelX + x) = 0;
            M(EntVelXFrac + x) = 0;
            M(EntVelY + x) = 0;
            M(EntVelYFrac + x) = 0;
            return;
        }
        M(EntVar0 + x) &= 0x7F;
        for (unsigned iterations = 0;; iterations++) { /* $A3FE */
            LOOP_GUARD(iterations)
            uint8_t i = (uint8_t)((M(EntTimer + x) & 0x7F) << 1);
            if (R5(0xA542 + i) == 0xFF) {
                M(EntTimer + x) = 0;
                i = 0;
            }
            M(TmpC) = R5(0xA542 + i);
            M(TmpD) = R5(0xA543 + i);
            uint16_t script = (uint16_t)(M(TmpC) | M(TmpD) << 8);
            uint8_t j = (uint8_t)((M(EntVar0 + x) & 0x7F) << 1);
            uint8_t b = R5(script + j);
            if (b == 0xFF) {
                M(EntVar0 + x) = 0;
                M(EntTimer + x)++;
                continue;
            }
            M(EntWait + x) = b;
            M(Tmp0) = b;
            M(EntVar1 + x) = R5(script + j + 1);
            M(EntVar0 + x)++;
            aim_index(g, x);
            return;
        }
    }
    uint8_t idx = (uint8_t)(R5(0xA56F + M(EntWait + x)) << 1); /* $A45A: does the scenery block this direction? */
    if (tile_in_wall_set(g, x, R5(0xA590 + idx), R5(0xA58F + idx))) {
        M(EntWait + x) = (uint8_t)((M(EntWait + x) + 8) & 0x1F);
        M(Tmp0) = M(EntWait + x);
        M(EntVar1 + x) = 0x40;
        M(EntTimer + x) &= 0x7F;
        M(EntVar0 + x) &= 0x7F;
        aim_index(g, x);
        return;
    }
    anim_if_changed(g, x, 0xCC);
    if (!(M(EntTimer + x) & 0x80)) {
        uint8_t dx = M(EntX) >= M(EntX + x) ? (uint8_t)(M(EntX) - M(EntX + x)) : (uint8_t)(M(EntX + x) - M(EntX));
        uint8_t dy = M(EntY) >= M(EntY + x) ? (uint8_t)(M(EntY) - M(EntY + x)) : (uint8_t)(M(EntY + x) - M(EntY));
        M(Tmp4) = dx;
        M(Tmp5) = dy;
        if (dx < 0x40 && dy < 0x40) { /* $A4D5: the player is close, it charges */
            M(EntTimer + x) |= 0x80;
            M(Tmp0) = M(EntX);
            M(Tmp1) = M(EntY);
            aim_at(g, x);
            set_velocity_from_m(g, x);
            M(EntWait + x) = R5(0xA8D6 + M(Tmp8) * 8 + M(Tmp9)); /* $A8C9 */
            M(Tmp4) >>= 1;
            M(EntVar1 + x) = (uint8_t)((M(Tmp5) >> 1) + M(Tmp4));
            return;
        }
        if (M(EntVar1 + x)) {
            M(EntVar1 + x)--;
            return;
        } /* $A502 */
    } else { /* $A50E */
        if (M(EntVar1 + x)) {
            M(EntVar1 + x)--;
            return;
        }
        M(EntTimer + x) &= 0x7F;
    }
    M(EntVar0 + x) |= 0x80; /* $A51B */
    M(EntVar1 + x) = 0x20;
}
