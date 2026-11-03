/* Type $4D: satellite orbiting a column; $4E: its shot. */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "enemy.h"
#include "enemies/boss_core.h"
#include "enemies/foot_soldier.h"
#include "enemies/purple_ninja.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- type $4D: satellite that orbits its column and fires $4E (B6 $BBC5 via B7 $CF7A); $4E: the shot (B6 $BD29 via B7 $CF87) --- */
void ai_orbiting_satellite(Game *g, int x) {
    M(EntWait + x)++;
    if (M(EntHealth + x) < 0xC0) { /* $BBCF: hit, the column collapses */
        if (!M(EntVar1 + x)) {
            M(ColumnsDestroyedMask) |= R6(0xBB7B + M(EntTimer + x));
            set_animation(g, x, 0xD3);
            M(EntVar1 + x) = 1;
            return;
        }
        if (M(EntAnimFrame + x) != 0xFF) {
            return;
        }
        uint8_t y = (uint8_t)((M(EntVar0 + x) & 0x0F) << 1); /* $BBF2 */
        M(EntX + x) = (uint8_t)(M(EntX + x) - R6(0xBCE9 + y) - 2);
        M(EntY + x) = (uint8_t)(M(EntY + x) - R6(0xBCEA + y) + 7);
        set_animation(g, x, 0xE9);
        M(EntType + x) = 0x4F;
        return;
    }
    uint8_t y = (uint8_t)((M(EntVar0 + x) & 0x0F) << 1); /* $BC21: the orbit offset is removed */
    M(EntX + x) = (uint8_t)(M(EntX + x) - R6(0xBCE9 + y));
    M(EntY + x) = (uint8_t)(M(EntY + x) - R6(0xBCEA + y));
    M(0x9C) = core_aim(g, x); /* B7 $CF92 -> B5 $BAE6 */
    uint8_t a = M(EntVar0 + x) & 0x0F;
    M(0x9C) = (uint8_t)(M(0x9C) << 1);
    uint8_t t = M(0x9C);
    if (a == t) { /* $BC62: facing, it fires */
        if (!(M(FrameCounter) & 0x80) && !(M(EntWait + x) & 0x0F)) {
            int n = 3, blocked = 0;
            for (int e = 0x10; e > 0; e--) {
                if (M(EntType + e) == 0x4E && --n == 0) {
                    blocked = 1;
                    break;
                }
            }
            if (!blocked) {
                int c = spawn_child_slot(g, x, (uint8_t)(((M(EntVar0 + x) & 0x0F) >> 1) + 0x26));
                if (c >= 0) {
                    uint8_t k = (uint8_t)(M(0x9C) << 1);
                    M(EntVelX + c) = R6(0xBD09 + k);
                    M(EntVelXFrac + c) = R6(0xBD0A + k);
                    M(EntVelY + c) = R6(0xBD0B + k);
                    M(EntVelYFrac + c) = R6(0xBD0C + k);
                }
            }
        }
    } else {
        int diff = a > t ? a - t : t - a;
        int up = a > t ? diff >= 8 : diff < 8; /* dec ($BCC4) or inc ($BCCA) */
        if (up) {
            M(EntVar0 + x)++;
        } else {
            M(EntVar0 + x)--;
        }
    }
    y = (uint8_t)((M(EntVar0 + x) & 0x0F) << 1); /* $BCCD */
    M(EntX + x) = (uint8_t)(M(EntX + x) + R6(0xBCE9 + y));
    M(EntY + x) = (uint8_t)(M(EntY + x) + R6(0xBCEA + y));
}

void ai_satellite_shot(Game *g, int x) {
    unsigned fy = M(EntVelYFrac + x) + M(EntYFrac + x);
    M(EntYFrac + x) = (uint8_t)fy;
    M(EntY + x) = (uint8_t)(M(EntVelY + x) + M(EntY + x) + (fy > 0xFF));
    uint8_t py = (uint8_t)(M(EntY + x) + 0xF8);
    unsigned fx = M(EntVelXFrac + x) + M(EntXFrac + x);
    M(EntXFrac + x) = (uint8_t)fx;
    M(EntX + x) = (uint8_t)(M(EntVelX + x) + M(EntX + x) + (fx > 0xFF));
    M(Tmp0) = M(EntX + x);
    M(Tmp1) = py;
    if (tile_at_xy(g, M(EntX + x), py, M(SavedY))) {
        clear_entity(g, x);
    }
}
