/* Boss $79: the last boss and its sword ($51). */
#include "game.h"
#include "anim.h"
#include "hal.h"
#include "player.h"
#include "sound.h"
#include "enemy.h"
#include "enemies/boss_brawler.h"
#include "enemies/boss_core.h"
#include "enemies/boss_eyes_machine.h"
#include "enemies/bosses.h"
#include "enemies/purple_ninja.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- boss $79 (B5 $BB00-$BF88): the last boss; $5A is a shift register of 8 conditions, rules ($BD3E) choose the animation, hence the attack --- */
static void final_speed_x(Game *g, int x, uint8_t y) /* B5 $BE77 */
{
    M(EntVelXFrac + x) = R5(0xBE84 + y);
    M(EntVelX + x) = R5(0xBE85 + y);
}

static void final_speed_y(Game *g, int x, uint8_t y) /* B5 $BE92 */
{
    M(EntVelYFrac + x) = R5(0xBE9F + y);
    M(EntVelY + x) = R5(0xBEA0 + y);
}

static uint8_t final_probe_tile(Game *g, int x, uint8_t y) /* B5 $BEA5: tile at the offset point */
{
    uint8_t px = (uint8_t)(M(EntX + x) + R5(0xBEBA + y));
    uint8_t py = (uint8_t)(M(EntY + x) + R5(0xBEBB + y));
    M(Tmp0) = px;
    M(Tmp1) = py;
    return tile_at_xy(g, px, py, M(SavedY));
}

static void final_face_player(Game *g, int x) /* B5 $BE61: turns towards the player */
{
    M(EntFlags + x) = (uint8_t)((M(EntFlags + x) & 0xFE) | (M(EntX + x) >= M(EntX) ? 1 : 0));
}

static void final_idle(Game *g, int x) /* B5 $BBD7: back to waiting */
{
    M(EntVar0 + x) = M(RandomSeed) & 0x0F;
    final_speed_x(g, x, 0);
    set_animation(g, x, 0x20);
    if (!(M(EntWait + x) & 0x80)) {
        M(EntWait + x) = 0;
    }
}

static int final_match_rule(Game *g, int x, uint8_t y) /* B5 $BDCE: carry = no rule fits; otherwise the animation changes */
{
    uint8_t anim = R5(0xBD54 + y), end = R5(0xBD53 + y), k = R5(0xBD52 + y);
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        if ((M(FinalBossConditions) & R5(0xBD3E + k)) == R5(0xBD3F + k)) {
            M(SavedY) = k;
            set_animation(g, x, anim);
            return 0;
        }
        k = (uint8_t)(k + 2);
        if (k >= end) {
            return 1;
        }
    }
    return 1; /* loop guard tripped : no rule fits */
}

static void final_wait_time(Game *g, int x) /* B5 $BBB3 */
{
    M(EntVar0 + x) = (uint8_t)((M(FrameCounter) & 0x38) + 7);
}

static void final_walk_speed(Game *g, int x) /* B5 $BBA6 */
{
    final_speed_x(g, x, (M(EntFlags + x) & 1) ? 4 : 2);
}

static void final_condition_flags(Game *g, int x) /* B5 $BF11: the register $5A */
{
#define PUSH(c) (M(FinalBossConditions) = (uint8_t)((M(FinalBossConditions) << 1) | ((c) ? 1 : 0)))
    PUSH((M(EntStatus) & 4) >= 4);
    int y = 4;
    while (y > 0 && !M(EntType + y)) {
        y--;
    }
    int found = 0;
    if (y > 0) {
        uint8_t d = (uint8_t)(M(EntX + x) - M(EntX + y));
        if (M(EntX + x) >= M(EntX + y)) {
            found = 0;
        } else if (!(((d ^ M(EntVelX + y)) ^ 0x80) & 0x80)) {
            found = 0;
        } else {
            uint8_t dist = (uint8_t)(0 - d);
            M(Tmp0) = d;
            M(Tmp1) = dist;
            PUSH(0x40 >= dist);
            found = 1;
        }
    }
    if (!found) {
        PUSH(0);
    }
    PUSH(M(EntY + y) >= 0x70);
    PUSH(M(EntY) >= 0x70);
    PUSH(M(EntY + x) >= 0x70);
    uint8_t dx = M(EntX) >= M(EntX + x) ? (uint8_t)(M(EntX) - M(EntX + x)) : (uint8_t)(M(EntX + x) - M(EntX));
    M(Tmp2) = dx;
    PUSH(0x30 >= dx);
    PUSH(0x40 >= M(EntHealth + x));
    uint8_t a = (uint8_t)((M(EntX + x) << 1) | (M(EntX + x) >= M(EntX)));
    PUSH((a ^ M(EntFlags + x)) & 1);
#undef PUSH
}

static void final_clamp(Game *g, int x) /* B5 $BEC4 */
{
    if (M(EntY + x) <= 0x20) {
        M(EntY + x) = 0x20;
    } else if (M(EntY + x) > 0xC0) {
        M(EntY + x) = 0xC0;
    }
    if (M(EntX + x) <= 0x24 || M(EntX + x) > 0xDC) {
        M(EntX + x) = M(EntX + x) <= 0x24 ? 0x24 : 0xDC;
        if (M(EntAnimId + x) == 0x21) {
            final_idle(g, x);
        }
    }
}

static void final_hurt(Game *g, int x) /* B5 $BDF6: hit or defeated */
{
    uint8_t a = M(EntAnimId + x);
    if (a == 0x27 || a == 0x28) {
        return;
    }
    if (!(M(EntFlags + x) & 0x10) && M(EntWait + x) == 0xFF) { /* $BE34: defeated */
        if (!M(TurtleHealth0 + M(CurrentTurtle))) {            /* the turtle is KO: it gets up */
            M(EntHealth + x) = 4;
            M(SceneId) = 0;
            M(EntWait + x) = 0;
            return;
        }
        load_palette_preset(g, 0x22);
        M(SceneIndex) = 1;
        sound_init(g);
        sound_play(g, 0x46);
        set_animation(g, x, 0x28);
        return;
    }
    if (!(M(SceneTimerB) & 4)) {
        return; /* $BE0F: a blow has just hit it */
    }
    set_animation(g, x, 0x27);
    final_speed_x(g, x, M(EntX + x) < M(EntX) ? 8 : 6);
    M(EntVar0 + x) = 0;
    final_speed_y(g, x, 4);
}

static void final_gravity(Game *g, int x) /* B5 $BC52: fall, bounce against the walls, landing */
{
    unsigned f = 0x40u + M(EntVelYFrac + x);
    M(EntVelYFrac + x) = (uint8_t)f;
    M(EntVelY + x) = (uint8_t)(M(EntVelY + x) + (f > 0xFF));
    uint8_t y = (M(EntVelX + x) & 0x80) ? 4 : 2;
    if (final_probe_tile(g, x, y)) {
        final_speed_x(g, x, 0);
    }
    if (M(EntVelY + x) & 0x80) {
        return;
    }
    if (!final_probe_tile(g, x, 0)) {
        return;
    }
    M(EntFlags + x) &= 0xEF;
    final_speed_y(g, x, 0);
    final_speed_x(g, x, 0);
    M(EntY + x) = (uint8_t)((M(EntY + x) & 0xF0) + 4);
    final_idle(g, x);
}

static void final_walk(Game *g, int x) /* B5 $BBBE */
{
    int cont;
    if (M(EntAnimId + x) == 0x24) {
        cont = 1;
    } else if (final_match_rule(g, x, 3)) {
        cont = 1;
    } else {
        final_speed_x(g, x, 0);
        M(EntVar1 + x) = M(EntX + x);
        cont = 0;
    }
    if (!cont) {
        return;
    }
    if (!M(EntVar0 + x)) {
        final_idle(g, x);
        return;
    }
    M(EntVar0 + x)--;
    if (final_probe_tile(g, x, 0)) {
        return;
    }
    set_animation(g, x, 0x23);
}

static void final_act(Game *g, int x) /* B5 $BB48 */
{
    if (!(M(EntFlags + x) & 0x30) && M(EntAnimId + x) != 0x28) {
        final_match_rule(g, x, 0x0C);
    }
    if (!M(EntAnimId + x)) {
        return;
    }
    switch (M(EntAnimId + x) - 0x20) {
        case 0: { /* $BB7A: waiting, choice of the attack */
            if (M(SceneId)) {
                return;
            }
            if (M(EntVar0 + x)) {
                M(EntVar0 + x)--;
                return;
            }
            final_face_player(g, x);
            if (M(EntWait + x)) {
                return; /* $BDA7 */
            }
            if (!final_match_rule(g, x, 6)) {
                M(EntWait + x)++;
                return;
            }
            if (M(EntAnimId + x) != 0x24) { /* $BD8C */
                if (!final_match_rule(g, x, 3)) {
                    final_speed_x(g, x, 0);
                    M(EntVar1 + x) = M(EntX + x);
                    return;
                }
            }
            if (!final_match_rule(g, x, 9)) { /* $BDB7 */
                final_speed_x(g, x, (M(EntFlags + x) & 1) ? 2 : 4);
                final_wait_time(g, x);
                return;
            }
            if (!(M(EntFlags + x) & 0x30) && !(M(EntWait + x) & 0x80)) { /* $BD64 */
                if (!final_match_rule(g, x, 0)) {
                    M(EntFlags + x) |= 0x10;
                    final_speed_y(g, x, 2);
                    final_face_player(g, x);
                    final_walk_speed(g, x);
                    return;
                }
            }
            set_animation(g, x, 0x21);
            final_wait_time(g, x);
            final_walk_speed(g, x);
            return;
        }
        case 1: /* $BBBE: walks towards the player */
            final_walk(g, x);
            return;
        case 2: /* $BD1B: same, then turns */
            final_walk(g, x);
            final_face_player(g, x);
            return;
        case 3: /* $BC33: jump */
            if (M(EntAnimId + x) == 0x25) {
                return;
            }
            {
                int c = final_match_rule(g, x, 0x0F);
                final_gravity(g, x);
                if (c) {
                    return;
                }
                if (M(EntAnimId + x) == 0x20) {
                    set_animation(g, x, 0x24);
                }
            }
            return;
        case 4: { /* $BCA2: blow, it advances */
            uint8_t f = M(EntAnimFrame + x);
            if (f == 0xFF) {
                final_idle(g, x);
                return;
            }
            uint8_t idx = (uint8_t)((f << 1) | (M(EntFlags + x) & 1));
            M(EntX + x) = (uint8_t)(R5(0xBCCA + idx) + M(EntVar1 + x));
            if (!final_probe_tile(g, x, 0)) {
                set_animation(g, x, 0x25);
            }
            return;
        }
        case 5:
        case 7: /* $BC52: the blow falls back / it is hit */
            final_gravity(g, x);
            return;
        case 6: { /* $BCD2: throws the sword */
            if (M(EntAnimFrame + x) == 0xFF) {
                final_idle(g, x);
                return;
            }
            if (M(EntAnimFrame + x) != 4 || M(EntWait + x) != 1) {
                return;
            }
            M(EntWait + x)++;
            int c = spawn_child_slot(g, x, 0x35);
            if (c < 0) {
                return;
            }
            M(EntFlags + c) = M(EntFlags + x);
            final_speed_x(g, c, (M(EntFlags + x) & 1) ? 0x0C : 0x0A);
            sound_play(g, 0x2D);
            return;
        }
        case 8: /* $BD0A: dies */
            core_palette_swap(g, 0x14);
            if (M(EntAnimFrame + x) == 0xFF) {
                clear_entity(g, x);
            }
            return;
        case 9: /* $BBF3: appearance */
            if (!(M(EntVar1 + x) & 0x80)) {
                M(EntVar1 + x) += 2;
                return;
            }
            M(EntFlags + x) |= 2;
            core_palette_swap(g, 0x18);
            if (M(EntAnimFrame + x) != 0xFF) {
                return;
            }
            set_chr_bank0(g, 0x0A);
            load_level_palette(g, 0x16);
            final_idle(g, x);
            M(EntY + x) = 0x54;
            M(0x36) = 0;
            M(EntHealth + x) = 0xA0;
            return;
        default:
            return;
    }
}

static void boss_final(Game *g, int x) /* B5 $BB00 */
{
    if (!M(EntState + x)) {
        for (int i = 0; i < 8; i++) {
            M(SceneState + i) = 0; /* B7 $C6E4 */
        }
        M(EntFlags + x) &= 0xFD;
        M(0x36) = 1;
        M(EntVar1 + x) = 1;
        M(EntState + x) = 1;
        M(SceneTimerA) = M(EntFlags + x);
        set_animation(g, x, 0x29);
        return;
    }
    uint8_t f = M(EntFlags + x);
    M(SceneTimerB) = (uint8_t)((f ^ M(SceneTimerA)) & M(EntFlags + x));
    M(SceneTimerA) = f;
    final_condition_flags(g, x);
    final_clamp(g, x);
    final_hurt(g, x);
    final_act(g, x);
}

void ai_final_boss_sword(Game *g, int x) /* B5 $BEF0: the sword of boss $79 */
{
    if (final_probe_tile(g, x, (M(EntFlags + x) & 1) ? 8 : 6)) {
        clear_entity(g, x);
        M(EntWait + 5) = 0;
        return;
    }
    if (M(EntAnimFrame + x) > 4) {
        M(EntAnimFrame + x) = 4;
    }
}

void boss_ai(Game *g, int x) /* B5 $AA96 */
{
    uint8_t t = M(EntType + x);
    if (t == 0x70) {
        boss_bebop(g, x);
    } else if (t == 0x71) {
        boss_hostage_holder(g, x);
    } else if (t == 0x72) {
        boss_brawler(g, x);
    } else if (t == 0x73) {
        boss_brawler_phase2(g, x);
    } else if (t == 0x74) {
        boss_eyes_machine(g, x);
    } else if (t == 0x75) {
        boss_core(g, x);
    } else if (t == 0x76) {
        boss_turret(g, x);
    } else if (t == 0x77) {
        boss_fragment(g, x);
    } else if (t == 0x78) {
        boss_turret_shot(g, x);
    } else if (t == 0x79) {
        boss_final(g, x);
    } else {
        TRACE("f=%u TODO boss type %02X\n", g->frame, t);
        return;
    }
    if (M(VerticalLevelFlag) & 0x80) {
        return; /* $AA99 */
    }
    uint8_t ex = M(EntX + x);
    if (ex < 0x18) {
        M(EntX + x) = 0x18;
    } else if (ex >= 0xE8) {
        M(EntX + x) = 0xE8;
    }
    if (M(EntY + x) < 0x20) {
        M(EntY + x) = 0x20;
    }
}
