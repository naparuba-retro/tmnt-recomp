/* Bosses $72 and $73: jumps, walks towards the player, strikes (two phases). */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "enemy.h"
#include "enemies/bosses.h"
#include "enemies/common.h"
#include "enemies/purple_ninja.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

static void brawler2_rise(Game *g, int x);

/* --- boss $72 (B5 $ACE2-$AE7E): jumps, walks towards the player, strikes; a shadow ($2F) follows it --- */
uint8_t boss_dist_x(Game *g, int x) /* B7 $F065: |X of the boss - X of the player| */
{
    return M(EntX + x) >= M(EntX) ? (uint8_t)(M(EntX + x) - M(EntX)) : (uint8_t)(M(EntX) - M(EntX + x));
}

uint8_t boss_dist_y(Game *g, int x) /* B7 $F057 */
{
    return M(EntY + x) >= M(EntY) ? (uint8_t)(M(EntY + x) - M(EntY)) : (uint8_t)(M(EntY) - M(EntY + x));
}

static void brawler_face_velocity(Game *g, int x) /* B5 $B2EA: X velocity of 2 to the right, -2 to the left */
{
    M(EntVelX + x) = (M(EntFlags + x) & 1) ? 0xFE : 0x02;
}

static void brawler_stop_at_wall(Game *g, int x) /* B5 $B297: stops against a wall */
{
    uint8_t a = (M(EntVelX + x) & 0x80) ? 0xF3 : 0x09;
    if (tile_at_offset(g, x, a, 0x12) != 0) {
        clear_vel_x(g, x);
    }
}

static void brawler_rise_from_ground(Game *g, int x) /* B5 $B2B4: comes back up out of the ground, Y velocity zero */
{
    do {
        clear_vel_y(g, x);
    } while (tile_at_offset(g, x, 0, 0x13) != 0 && (M(EntY + x)--, 1));
    M(EntFlags + x) &= 0xEF;
}

static void brawler_spawn_shadow(Game *g, int x) /* B5 $B2DF / $B2F9: the shadow takes the slot $0B */
{
    M(EntStatus + x) |= 0x04;
    M(CurrentEntity) = (uint8_t)x;
    M(Tmp0) = M(EntX + x);
    M(Tmp1) = M(EntY + x);
    clear_entity(g, 0x0B);
    M(EntX + 0x0B) = M(Tmp0);
    M(EntY + 0x0B) = M(Tmp1);
    M(EntHealth + 0x0B) = 0xFF;
    M(EntType + 0x0B) = 0x2F;
    M(EntFlags + 0x0B) = 0xC2;
    M(EntAnimId + 0x0B) = 0x9D;
    M(EntAnimTimer + 0x0B) = M(EntAnimTimer + 5);
}

static void brawler_idle(Game *g, int x) /* $AE23 */
{
    clear_vel_x(g, x);
    clear_vel_y(g, x);
    boss_state(g, x, 1);
    set_animation(g, x, 0x80);
}

static void brawler_walk(Game *g, int x) /* $AE30 */
{
    boss_face(g, x);
    boss_state(g, x, 2);
    set_animation(g, x, 0x81);
}

static void brawler_jump_start(Game *g, int x) /* $AE4C */
{
    boss_face(g, x);
    boss_state(g, x, 3);
    M(EntFlags + x) |= 0x10;
    set_animation(g, x, 0x85);
}

static void brawler_jump(Game *g, int x) /* $AE3D: stronger jump if the player is far */
{
    M(EntVelY + x) = boss_dist_x(g, x) >= 0x30 ? 0xFB : 0xFA;
    brawler_jump_start(g, x);
}

static void brawler_attack(Game *g, int x) /* $AE5C */
{
    boss_face(g, x);
    brawler_spawn_shadow(g, x);
    boss_state(g, x, 4);
    set_animation(g, x, 0x91);
}

void boss_brawler(Game *g, int x) {
    M(SceneCounter) = M(EntX + x); /* $ACF3 */
    M(SceneDelay) = M(EntY + x);
    M(EntAttr + x) = 2;
    M(EntTimer + x)++;
    switch (M(EntState + x)) {
        case 0: /* $AD1B */
            M(0x03E8) = 2;
            M(EntState + x)++;
            M(CurrentEntity) = (uint8_t)x;
            boss_place(g, 0x0F, 3);
            M(0x03E7) = 0x0B;
            break;
        case 1: /* $AD34 : decide */
            set_animation(g, x, 0x80);
            clear_vel_x(g, x);
            clear_vel_y(g, x);
            if (M(EntTimer + x) < 0x10) {
                M(0x03E7) = 0x0B;
                break;
            }
            if (boss_dist_y(g, x) >= 0x28) {
                brawler_jump(g, x);
            } else if (boss_dist_x(g, x) >= 0x14) {
                brawler_walk(g, x);
            } else if (M(FrameCounter) & 0x0C) {
                brawler_attack(g, x);
            } else {
                brawler_jump(g, x);
            }
            break;
        case 2: /* $AD63: walks */
            brawler_face_velocity(g, x);
            brawler_stop_at_wall(g, x);
            if (tile_at_offset(g, x, 0, 0x14) == 0) {
                brawler_jump_start(g, x);
                break;
            }
            if (M(EntTimer + x) < 0x14) {
                break;
            }
            if (!(M(FrameCounter) & 0x60)) {
                brawler_jump(g, x);
            } else if (boss_dist_x(g, x) < 0x24) {
                brawler_attack(g, x);
                M(EntState + x)++;
            } else if (boss_dist_y(g, x) >= 0x20) {
                brawler_jump(g, x);
            } else if (M(EntTimer + x) >= 0x20) {
                brawler_walk(g, x);
            }
            break;
        case 3: /* $ADA4: jump */
            brawler_face_velocity(g, x);
            add_vel_y(g, x, 0x4E);
            brawler_stop_at_wall(g, x);
            if (!(M(EntVelY + x) & 0x80)) {
                if (tile_at_offset(g, x, 0, 0x14) == 0) {
                    break;
                }
                brawler_rise_from_ground(g, x);
                brawler_idle(g, x);
            } else if (M(FrameCounter) & 0x48) {
                brawler_attack(g, x);
                M(EntState + x) += 2;
                M(EntFlags + x) |= 0x10;
            }
            break;
        case 4: /* $ADCC */
            if (M(EntAnimFrame + x) != 0xFF) {
                break;
            }
            M(EntStatus + x) &= 0xFB;
            brawler_idle(g, x);
            break;
        case 5: /* $ADD8 */
            brawler_stop_at_wall(g, x);
            if (M(EntAnimFrame + x) != 0xFF) {
                break;
            }
            M(EntStatus + x) &= 0xFB;
            brawler_walk(g, x);
            break;
        case 6: /* $ADE7: strikes in mid-air */
            if (M(EntFlags + x) & 0x10) {
                add_vel_y(g, x, 0x4E);
            }
            brawler_stop_at_wall(g, x);
            if (!(M(EntVelY + x) & 0x80) && tile_at_offset(g, x, 0, 0x14) != 0) {
                brawler_rise_from_ground(g, x);
            }
            M(EntStatus + x) &= 0xFB;
            if (M(EntAnimFrame + x) == 0xFF) {
                if (M(EntFlags + x) & 0x10) {
                    set_animation(g, x, 0x85);
                    brawler_jump_start(g, x);
                } else {
                    brawler_idle(g, x);
                }
            }
            break;
        default:
            break;
    }
    if (M(EntX + x) < 0x26) {
        M(EntX + x) = 0x26; /* $B267 */
    }
    M(0xB1) = M(EntX + x);
    M(0xB2) = M(EntY + x);
}

/* --- boss $73 (B5 $AE7F-$AFF7): appears when boss $72 is beaten (scene 9); jumps, blows, projectiles --- */
static void brawler2_start(Game *g, int x, uint8_t anim) /* B5 $B2AB: animation, next state, counter to zero */
{
    set_animation(g, x, anim);
    M(EntState + x)++;
    M(EntTimer + x) = 0;
}

static void brawler2_set_state(Game *g, int x, uint8_t st) {
    M(EntState + x) = st;
}

void boss_brawler_phase2(Game *g, int x) {
    M(0x03E7) = 0;
    M(EntTimer + x)++;
    switch (M(EntState + x)) {
        case 0: /* $AEB2 */
            brawler2_start(g, x, 0x62);
            clear_vel_x(g, x);
            clear_vel_y(g, x);
            break;
        case 1: /* $AF24: waits, then chooses */
            if (M(EntTimer + x) != 8) {
                break;
            }
            if (!(M(FrameCounter) & 0x80)) {
                brawler2_set_state(g, x, 4);
            } else {
                brawler2_set_state(g, x, (M(EntX) & 4) ? 0x0C : 2);
            }
            break;
        case 2: /* $AEBA: walks */
            boss_face(g, x);
            brawler_face_velocity(g, x);
            brawler2_start(g, x, 0x63);
            break;
        case 3: /* $AF44: checks the ground and the wall */
            if (tile_at_offset(g, x, 0, 1) == 0) {
                brawler2_set_state(g, x, 4);
                break;
            }
            if (tile_at_offset(g, x, (M(EntFlags + x) & 1) ? 0xF1 : 0x0A, 0xFC) == 0) {
                brawler2_set_state(g, x, 4);
                break;
            }
            clear_vel_x(g, x);
            clear_vel_y(g, x);
            brawler2_set_state(g, x, 0);
            break;
        case 4: { /* $AEC5: jumps towards the player */
            boss_face(g, x);
            uint8_t d = boss_dist_x(g, x), y = 0;
            if (d >= 0x18) {
                y++;
            }
            if (d >= 0x30) {
                y++;
            }
            M(Tmp1) = y;
            M(EntVelX + x) = (uint8_t)(~y + 1); /* the 6502 tests the result of ROR here (never zero): always negative */
            M(EntVelY + x) = 0xFD;
            brawler2_start(g, x, 0x64);
            break;
        }
        case 5: /* $AF6F */
            if (M(EntTimer + x) >= 0x15) {
                brawler2_set_state(g, x, 8);
            }
            break;
        case 6: /* $AEF2 */
            boss_face(g, x);
            brawler_face_velocity(g, x);
            M(EntVelY + x) = 3;
            brawler2_start(g, x, 0x64);
            break;
        case 7:
            brawler2_rise(g, x);
            break;
        case 8: /* $AF02 */
            boss_face(g, x);
            clear_vel_x(g, x);
            clear_vel_y(g, x);
            brawler2_start(g, x, 0x65);
            break;
        case 9: /* $AF99 */
            if (M(EntTimer + x) != 0x10) {
                break;
            }
            if (M(FrameCounter) & 4) {
                brawler2_set_state(g, x, 0x0A);
            } else {
                set_animation(g, x, 0x64);
                brawler2_set_state(g, x, 6);
            }
            break;
        case 10: /* $AF0D : projectile */
            spawn_child_slot(g, x, 5);
            brawler2_start(g, x, 0x66);
            break;
        case 11: /* $AFAA */
            if (M(EntAnimFrame + x) != 0xFF) {
                break;
            }
            set_animation(g, x, 0x64);
            brawler2_set_state(g, x, 6);
            break;
        case 12: /* $AF17: other projectile */
            boss_face(g, x);
            spawn_child_slot(g, x, 6);
            brawler2_start(g, x, 0x67);
            break;
        case 13: /* $AFB9 */
            if (M(EntAnimFrame + x) != 0xFF) {
                break;
            }
            set_animation(g, x, 0x62);
            brawler2_set_state(g, x, 0);
            break;
        case 14: /* $AFCA: entrance, blinks then goes to state 6 */
            if (--M(DelayTimerLo) == 0) {
                M(0x03E7) = 0;
                brawler2_set_state(g, x, 6);
                M(EntAnimId + x) = 0x64;
                M(EntFlags + x) |= 0x02;
            } else if (M(DelayTimerLo) & 2) {
                M(EntFlags + x) &= 0xFD;
            } else {
                M(EntFlags + x) |= 0x02;
            }
            break;
        default:
            break;
    }
    if (M(EntX + x) < 0x26) {
        M(EntX + x) = 0x26; /* $B267 */
    }
}

static void brawler2_rise(Game *g, int x) /* B5 $AF7A: state 7, comes back up out of the ground */
{
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        if (!(M(EntTimer + x) & 0x0F)) {
            boss_face(g, x);
            brawler_face_velocity(g, x);
        }
        if (tile_at_offset(g, x, 0, 0) == 0) {
            return;
        }
        brawler2_set_state(g, x, 0);
        if (--M(EntY + x) != 0) {
            continue;
        }
        if (M(EntTimer + x) != 0x10) {
            return; /* $AF99: state 9 chains on */
        }
        if (M(FrameCounter) & 4) {
            brawler2_set_state(g, x, 0x0A);
        } else {
            set_animation(g, x, 0x64);
            brawler2_set_state(g, x, 6);
        }
        return;
    }
}
