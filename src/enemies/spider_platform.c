/* Types $82 / $88 / $89: spider platform along its anchor. */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "enemy.h"
#include "enemies/chansawiac.h"
#include "enemies/common.h"
#include "enemies/purple_ninja.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- types $82, $88 and $89: B5 $94AF-$96A0 ($88 is an anchor, $89 the spider platform that goes up and down along it) --- */
void ai_hopper(Game *g, int x) /* B5 $94AF */
{
    M(EntVar1 + x) = M(EntY + x); /* B7 $F07A */
    walk_speed(g, x, 1, 0);
    hop_start(g, x, 4);
}

void ai_spider_anchor(Game *g, int x) /* B5 $94BE: a single platform per column */
{
    uint8_t ox = M(EntX + x);
    for (int i = 5; i < 0x0B; i++) {
        if (i == x) {
            continue;
        }
        uint8_t t = M(EntType + i);
        if ((t == 0x88 || t == 0x89) && M(EntX + i) == ox) {
            clear_entity(g, x);
            return;
        }
    }
    int n = x;
    while (n < 0x0B && M(EntType + n)) {
        n++; /* B7 $EEF4: from x to entity 10 */
    }
    if (n >= 0x0B) {
        return;
    }
    M(EntType + n) = 0x89;
    M(EntHealth + n) = 0xFF;
    M(EntAnimId + n) = 0x16;
    M(EntX + n) = M(EntX + x); /* $9518 */
    M(EntXFrac + n) = M(EntXFrac + x);
    M(EntY + n) = (uint8_t)(M(EntY + x) - 7);
    M(EntYFrac + n) = M(EntYFrac + x);
    M(EntFlags + n) = 0xC2;
    M(EntVar0 + n) = (uint8_t)x;
    M(EntState + x) = 1;
    M(EntRoutine + x) = 0;
}

static int platform_blocked(Game *g, int p) /* B5 $9682: the tile under the anchor is hard */
{
    return in_block_list(tile_at_offset(g, p, 0, 0x10));
}

static void platform_vel(Game *g, int p, uint8_t v) {
    M(EntVelY + p) = v;
} /* B5 $9675 */

static void platform_stop(Game *g, int p) /* B5 $9663 */
{
    clear_vel_y(g, p);
    set_animation(g, p, 0x13);
}

void ai_spider_platform(Game *g, int x) /* B5 $9539 */
{
    int p = M(EntVar0 + x);
    uint8_t pt = M(EntType + p);
    if (pt == 0 || pt == 0x15) {
        clear_entity(g, x);
        return;
    }
    if (M(EntStatus + x)) { /* $9591: the shot is over */
        if (M(EntAnimFrame + p) == 0xFF) {
            set_animation(g, p, 0x14);
            M(EntStatus + x) = 0;
        }
        return;
    }
    if (M(EntAnimTimer + x) == 2 && !(M(FrameCounter) & 7)) { /* $9561: fires */
        M(EntStatus + x) = 1;
        int n = spawn_child_slot(g, p, 8);
        if (n >= 0) {
            clear_vel_y(g, p);
            set_animation(g, p, 0x15);
            M(EntVelY + n) = 4;
            M(EntAttr + n) = 1;
            set_animation(g, n, 0x12);
            return;
        }
        M(EntStatus + x) = 0;
    }
    uint8_t v = M(EntVar1 + x);
    if (!(v & 0x7F)) { /* $95BC */
        if (!(v & 0x80)) {
            if (M(EntAnimId + x) >= 0x1B || platform_blocked(g, p)) {
                M(EntVar1 + x) = 0x80;
                return;
            } /* $95C8 */
            M(EntVar1 + x) = 8;
            set_animation(g, p, 0x14);
            return;
        }
        if (M(EntAnimId + x) == 0x16) {
            M(EntVar1 + x) = 0;
            return;
        } /* $95E8 */
        M(EntAnimId + x)--;
        set_animation(g, x, M(EntAnimId + x));
        M(EntY + x) = (uint8_t)(M(EntY + x) - 8);
        M(EntVar1 + x) = 0x88;
        set_animation(g, p, 0x14);
        return;
    }
    if (!(v & 0x80)) { /* $9612: goes down */
        if (platform_blocked(g, p)) {
            M(EntVar1 + x) = 0x80;
            platform_stop(g, p);
            return;
        }
        platform_vel(g, p, 2);
        if (--M(EntVar1 + x)) {
            return;
        }
        M(EntAnimId + x)++;
        if (M(EntAnimId + x) >= 0x1C) {
            M(EntAnimId + x)--;
            platform_stop(g, p);
            return;
        }
        set_animation(g, x, M(EntAnimId + x));
        M(EntY + x) = (uint8_t)(M(EntY + x) + 8);
        M(EntVar1 + x) = 8;
        return;
    }
    platform_vel(g, p, 0xFE); /* $964C: goes back up */
    M(EntVar1 + x)--;
    if (M(EntVar1 + x) & 0x7F) {
        return;
    }
    if (M(EntAnimId + x) == 0x16) {
        platform_stop(g, p);
    }
}
