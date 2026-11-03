/* Cutscene actors of bank 2 (B2 $B8F9 / $B933): initialisation and per-frame behaviour. */
#include "game.h"
#include "sound.h"
#include "scenes.h"
#include "scenes/actors.h"
#include "scenes/actors_bank3.h"
#include "scenes/scene_common.h"
#define TODO_AREA "scene" /* prefix of the TODO traces of this file */

static void actor_special(Game *g, int x);

static void actor_place_pose_a(Game *g, int x) /* B2 $BBCA */
{
    if (M(EntVar0 + x)) {
        if (--M(EntVar0 + x) == 0) {
            M(EntWait + x)++;
            M(SceneCounter)++;
        }
    }
    M(EntX + x) = 0x30;
    M(EntY + x) = 0x8C;
    zero_frac_vel(g, x);
}

static void actor_place_pose_b(Game *g, int x) /* B2 $BC3A */
{
    if (M(EntVar0 + x)) {
        if (--M(EntVar0 + x) == 0) {
            M(EntWait + x)++;
            M(SceneCounter)++;
        }
    }
    M(EntX + x) = 0x38;
    M(EntY + x) = 0x88;
    zero_frac_vel(g, x);
}

static void actor_timer_then_alternate_pose_b(Game *g, int x) /* B2 $BBC0 */
{
    M(EntTimer + x) = M(EntVar0 + x);
    if (M(EntVar0 + x) & 1) {
        actor_place_pose_b(g, x);
    } else {
        actor_place_pose_a(g, x);
    }
}

static void actor_timer_then_alternate_pose_a(Game *g, int x) /* B2 $BC30 */
{
    M(EntTimer + x) = M(EntVar0 + x);
    if (M(EntVar0 + x) & 1) {
        actor_place_pose_a(g, x);
    } else {
        actor_place_pose_b(g, x);
    }
}

void actor_init(Game *g, int x, uint8_t k) {
    switch (k) {
        case 0:
        case 1: /* $B970 / $B9C2 */
            M(EntX + x) = k ? 0xB4 : 0xAC;
            M(EntXFrac + x) = 0;
            M(EntY + x) = 0x6B;
            M(EntYFrac + x) = 0;
            M(EntTimer + x) = R2(0xB5AF);
            zero_vel(g, x);
            break;
        case 2: /* $BA04 */
            M(EntX + x) = 0x20;
            M(EntXFrac + x) = 0;
            M(EntY + x) = 0x70;
            M(EntYFrac + x) = 0;
            M(EntVelX + x) = 2;
            M(EntVelXFrac + x) = M(EntVelY + x) = M(EntVelYFrac + x) = 0;
            M(EntTimer + x) = 0;
            break;
        case 3: /* $BA77 */
            M(EntX + x) = 0xC0;
            M(EntXFrac + x) = 0;
            M(EntY + x) = 0x4A;
            M(EntYFrac + x) = 0;
            zero_vel(g, x);
            M(EntFlags + x) &= 0xFB;
            M(EntTimer + x) = R2(0xB5B0);
            break;
        case 4: /* $BACD, then the continuation ($BB01) */
            M(EntFlags + x) |= 1;
            M(EntTimer + x) = 0x10;
            M(EntX + x) = 0xC0;
            M(EntY + x) = 0x70;
            M(EntVar0 + x) = 0;
            M(EntXFrac + x) = 0;
            M(EntYFrac + x) = 0;
            M(EntVelX + x) = 0xFF;
            M(EntVelY + x) = 0;
            M(EntVelXFrac + x) = 0x80;
            M(EntVelYFrac + x) = 0x80;
            actor_special(g, x);
            break;
        case 5: /* $BB44 */
            M(EntTimer + x) = 0x30;
            flags_clear_bit1(g, x);
            M(EntX + x) = 0x90;
            M(EntY + x) = 0x60;
            zero_frac_vel(g, x);
            break;
        case 6: /* $BB7E */
            M(EntWait + x) = 0;
            M(EntVar1 + x) = 0x60;
            M(EntVar0 + x) = 8;
            M(EntTimer + x) = 0;
            actor_timer_then_alternate_pose_b(g, x);
            break;
        case 7: /* $BBED */
            M(EntWait + x) = 0;
            M(EntVar1 + x) = 0x60;
            M(EntVar0 + x) = 8;
            M(EntTimer + x) = 0;
            actor_timer_then_alternate_pose_a(g, x);
            break;
        case 8:
        case 9:
        case 10: /* $BC63 / $BC78 / $BC92 */
            M(EntX + x) = k == 10 ? 0x40 : 0x70;
            M(EntY + x) = k == 10 ? 0x98 : 0x74;
            flags_clear_bit1(g, x);
            if (k == 9) {
                M(EntVar0 + x) = 0xFF;
            }
            if (k == 10) {
                M(EntVar0 + x) = 0xFC;
            }
            zero_vel(g, x);
            break;
        default:
            bank3_init(g, x, k - 11);
            break;
    }
}

/* --- actors, picture (B2 $B933: A = type - $AE) --- */
static void actor_special(Game *g, int x) /* $BB01 */
{
    if (M(EntVar0 + x)) {
        if (M(EntAnimFrame + x) == 0xFF) {
            M(EntAnimFrame + x) = 1;
            M(EntAnimTimer + x) = 0xFF;
        }
        zero_vel(g, x);
        M(EntTimer + x)--;
        return;
    }
    if (M(EntTimer + x)) {
        M(EntTimer + x)--;
        return;
    }
    M(EntAnimId + x)++;
    M(EntAnimFrame + x) = 0;
    M(EntAnimTimer + x) = 0x20;
    M(EntVar0 + x) = 0x20;
    M(EntTimer + x) = 0x90;
}

static int past_cutscene_step(Game *g) {
    return M(SceneState) >= 0x0D;
} /* B2 $BD4E: carry = yes */

static void actor_walk_left(Game *g, int x) /* $B999 */
{
    if (M(EntTimer + x)) {
        M(EntTimer + x)--;
        return;
    }
    M(EntAttr + x) |= 0x20;
    if (M(EntX + x) < 0xA0) {
        zero_vel(g, x);
        return;
    }
    M(EntVelX + x) = 0xFF;
    M(EntVelXFrac + x) = M(EntVelY + x) = M(EntVelYFrac + x) = 0;
}

static void actor_walk_right_with_sound(Game *g, int x) /* $B9CF */
{
    if (M(EntTimer + x)) {
        M(EntTimer + x)--;
        if (M(EntTimer + x) == 1) {
            sound_play(g, 0x31);
        }
        return;
    }
    M(EntAttr + x) |= 0x20;
    if (M(EntX + x) >= 0xC0) {
        zero_vel(g, x);
        return;
    }
    M(EntVelX + x) = 1;
    M(EntVelXFrac + x) = M(EntVelY + x) = M(EntVelYFrac + x) = 0;
}

static void actor_pose_sequence(Game *g, int x) /* $BA2E */
{
    if (M(EntX + x) < 0xB0) {
        return;
    }
    zero_vel(g, x);
    if (M(EntAnimFrame + x) == 0xFF || !M(EntTimer + x)) {
        M(EntAnimId + x)++;
        M(EntVar0 + x)++;
        M(EntAnimFrame + x) = 0;
        M(EntAnimTimer + x) = 1;
        uint8_t a = R2(0xBAC8 + M(EntVar0 + x));
        if (a == 0xFF) {
            M(EntAnimId + x)--;
            flags_clear_bit1(g, x);
            M(SceneIndex) = 1;
            return;
        }
        M(EntTimer + x) = a;
    }
    M(EntTimer + x)--;
}

static void actor_blink(Game *g, int x) /* $BAA6 */
{
    uint8_t a = M(EntTimer + x);
    if (a && a < R2(0xB5B1)) {
        M(EntTimer + x)--;
        flags_set_bit1(g, x);
        return;
    }
    M(EntTimer + x)--;
    flags_clear_bit1(g, x);
}

static void actor_delayed_show(Game *g, int x) /* $BB70 */
{
    if (M(EntTimer + x)) {
        M(EntTimer + x)--;
        return;
    }
    flags_set_bit1(g, x);
}

static void actor_hit_sequence_a(Game *g, int x) /* $BB95 */
{
    if (M(EntWait + x)) {
        M(EntWait + x)++;
        if (M(EntWait + x) >= 0x40) {
            M(SceneIndex) = 1;
            return;
        }
        if (M(EntWait + x) & 1) {
            actor_place_pose_b(g, x);
        } else {
            actor_place_pose_a(g, x);
        }
        return;
    }
    if (M(EntVar1 + x)) {
        M(EntVar1 + x)--;
        flags_clear_bit1(g, x);
        return;
    }
    M(EntFlags + x) &= 0x02;
    if (M(EntTimer + x)) {
        M(EntTimer + x)--;
        return;
    }
    actor_timer_then_alternate_pose_b(g, x);
}

static void actor_hit_sequence_b(Game *g, int x) /* $BC04 */
{
    if (M(EntWait + x)) {
        M(EntWait + x)++;
        if (M(EntWait + x) >= 0x10) {
            M(SceneIndex) = 1;
            return;
        }
        if (M(EntWait + x) & 1) {
            actor_place_pose_a(g, x);
        } else {
            actor_place_pose_b(g, x);
        }
        return;
    }
    if (M(EntVar1 + x)) {
        M(EntVar1 + x)--;
        flags_clear_bit1(g, x);
        return;
    }
    flags_set_bit1(g, x);
    if (M(EntTimer + x)) {
        M(EntTimer + x)--;
        return;
    }
    actor_timer_then_alternate_pose_a(g, x);
}

static void actor_toggle_pose(Game *g, int x) /* $BCB8 */
{
    if (!past_cutscene_step(g)) {
        return;
    }
    if (!M(EntTimer + x)) {
        if (M(EntVar0 + x)) {
            flags_clear_bit1(g, x);
            M(EntTimer + x) = 0x30;
            M(EntVar0 + x) = 0;
            return;
        }
        flags_set_bit1(g, x);
        M(EntTimer + x) = 0x10;
        M(EntVar0 + x) = 1;
    }
    M(EntTimer + x)--;
}

static void actor_bob_vertical(Game *g, int x) /* $BCF3 */
{
    if (!past_cutscene_step(g)) {
        return;
    }
    flags_set_bit1(g, x);
    if (!M(EntTimer + x)) {
        M(EntTimer + x) = 8;
        M(EntVar0 + x) = (uint8_t)(-(int8_t)M(EntVar0 + x));
        M(EntY + x) = (uint8_t)(M(EntY + x) + M(EntVar0 + x));
    }
    M(EntTimer + x)--;
}

static void actor_bob_horizontal(Game *g, int x) /* $BD20 */
{
    if (!past_cutscene_step(g)) {
        return;
    }
    flags_set_bit1(g, x);
    if (!M(EntTimer + x)) {
        M(EntTimer + x) = 0x20;
        M(EntVar0 + x) = (uint8_t)(-(int8_t)M(EntVar0 + x));
        M(EntX + x) = (uint8_t)(M(EntX + x) + M(EntVar0 + x));
    }
    M(EntTimer + x)--;
}

void actor_frame(Game *g, int x, uint8_t k) {
    switch (k) {
        case 0:
            actor_walk_left(g, x);
            break;
        case 1:
            actor_walk_right_with_sound(g, x);
            break;
        case 2:
            actor_pose_sequence(g, x);
            break;
        case 3:
            actor_blink(g, x);
            break;
        case 4:
            actor_special(g, x);
            break;
        case 5:
            actor_delayed_show(g, x);
            break;
        case 6:
            actor_hit_sequence_a(g, x);
            break;
        case 7:
            actor_hit_sequence_b(g, x);
            break;
        case 8:
            actor_toggle_pose(g, x);
            break;
        case 9:
            actor_bob_vertical(g, x);
            break;
        case 10:
            actor_bob_horizontal(g, x);
            break;
        default:
            bank3_frame(g, x, k - 11);
            break;
    }
}
