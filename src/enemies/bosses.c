/* Boss dispatcher ($70-$7F), bebop ($70) and the boss holding the hostage ($71). */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "enemy.h"
#include "enemies/common.h"
#include "enemies/purple_ninja.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* $B3AD leaves X = element placed: the rest of the $8001 loop ($8049, $EF22, $8077, INX) uses it */

void boss_state(Game *g, int x, uint8_t st) /* B5 $B3A4: new state, counter reset to zero */
{
    M(EntState + x) = st;
    M(EntTimer + x) = 0;
}

void boss_place(Game *g, int x, uint8_t i) /* B5 $B3AD: flags, position and sprite of a scenery element of the boss */
{
    ROM_TABLE(flags, 7, 5, 0xB3C6);
    ROM_TABLE(px, 7, 5, 0xB3CD);
    ROM_TABLE(py, 7, 5, 0xB3D4);
    ROM_TABLE(spr, 7, 5, 0xB3DB);
    M(EntFlags + x) = flags[i];
    M(EntX + x) = px[i];
    M(EntY + x) = py[i];
    M(EntSprite + x) = spr[i];
}

static void boss_face_player(Game *g, int x) /* B5 $B249 / $B24F according to the side */
{
    if (M(EntX + x) >= M(EntX)) {
        M(EntFlags + x) |= 1;
    } else {
        M(EntFlags + x) &= 0xFE;
    }
    walk_speed(g, x, 2, 0);
    if (move_blocked(g, x, 0x0A, 0xF0)) {
        clear_vel_x(g, x); /* $8584 */
    }
}

static void boss_wall_stop(Game *g, int x) /* B5 $ABF8 */
{
    uint8_t a = (M(EntVelX + x) & 0x80) ? 0xF4 : 0x0C;
    if (tile_at_offset(g, x, a, 0xDB) != 0) {
        clear_vel_x(g, x);
    }
}

void boss_bebop(Game *g, int x) /* B5 $AADB */
{
    switch (M(EntState + x)) {
        case 0: /* $AAF5 */
            M(EntState + x)++;
            boss_place(g, 8, 1);
            M(EntAnimId + 9) = 0x46;
            M(EntHealth + 9) = 0xFF;
            M(EntType + 9) = 0x11;
            boss_place(g, 9, 2);
            ai_leak_x = 9;
            break;
        case 1: /* $AB15: turns towards the player */
            boss_face_player(g, x);
            set_animation(g, x, 0x40);
            boss_state(g, x, 2);
            break;
        case 2: { /* $AB30: advances */
            boss_wall_stop(g, x);
            M(EntTimer + x)++;
            uint8_t ex = M(EntX + x);
            if (ex < 0x19 || ex >= 0xE8) {
                turn_around(g, x); /* $B259 */
            }
            uint8_t d = (M(EntX + x) >= M(EntX)) ? (uint8_t)(M(EntX + x) - M(EntX)) : (uint8_t)(M(EntX) - M(EntX + x));
            if (d < 0x10) {
                boss_state(g, x, 5);
                break;
            }
            if (d < 0x24) {
                if (M(FrameCounter) & 0x30) {
                    break;
                }
                boss_state(g, x, 3);
                break;
            }
            if (M(EntTimer + x) >= 0x30) {
                boss_state(g, x, (M(EntFlags) & 0x10) ? 3 : 1);
            }
            break;
        }
        case 3: /* $AB6E: jump */
            set_vel_y_abs(g, x, 0, 0xFB);
            M(EntState + x)++;
            set_animation(g, x, 0x43);
            break;
        case 4: /* $AB7D */
            ballistic(g, x, 6, 0);
            if (land_check(g, x, 0, 0xFB)) {
                M(EntState + x) = 7;
            }
            break;
        case 5: /* $AB91: blow */
            set_animation(g, x, 0x42);
            M(EntState + x)++;
            clear_vel_x(g, x);
            break;
        case 6: /* $AB9C */
            if ((uint8_t)(M(EntAnimFrame + x) + 1) != 0) {
                break;
            }
            M(EntState + x) = 9;
            set_animation(g, x, 0x41);
            break;
        case 7: /* $ABAC: steps back */
            if (M(EntX + x) >= M(EntX)) {
                M(EntFlags + x) &= 0xFE;
                walk_speed(g, x, 2, 0);
                if (move_blocked(g, x, 0x0A, 0xF0)) {
                    clear_vel_x(g, x);
                }
            } else {
                M(EntFlags + x) |= 1;
                walk_speed(g, x, 2, 0);
                if (move_blocked(g, x, 0x0A, 0xF0)) {
                    clear_vel_x(g, x);
                }
            }
            set_animation(g, x, 0x40);
            boss_state(g, x, 2);
            break;
        case 8: /* $ABBA */
            boss_wall_stop(g, x);
            M(EntTimer + x)++;
            if (M(EntTimer + x) >= 8 || M(EntX) == 0x10 || M(EntX) == 0xF0) {
                M(EntState + x) = 9;
            }
            break;
        case 9: /* $ABD8 : pause */
            set_animation(g, x, 0x41);
            clear_vel_x(g, x);
            clear_vel_y(g, x);
            if (((M(FrameCounter) >> 1) & 0x0F) != 0) {
                break;
            }
            M(EntState + x) = 1;
            if (!(M(FrameCounter) & 0x40)) {
                M(EntState + x) = 7;
            }
            break;
        default:
            break;
    }
}

void boss_face(Game *g, int x) /* B5 $B27C / $B285: turns towards the player */
{
    if (M(EntX + x) >= M(EntX)) {
        M(EntFlags + x) |= 1;
    } else {
        M(EntFlags + x) &= 0xFE;
    }
}

void boss_hostage_holder(Game *g, int x) /* B5 $AC0B */
{
    switch (M(EntState + x)) {
        case 0: /* $AC21: the hostage (entity 15) appears */
            M(EntState + x)++;
            boss_place(g, 15, 0);
            ai_leak_x = 15;
            break;
        case 1: /* $AC2B */
            boss_face(g, x);
            walk_speed(g, x, 2, 0);
            if (move_blocked(g, x, 0x0A, 0xF0)) {
                clear_vel_x(g, x);
            }
            set_animation(g, x, 0x45);
            boss_state(g, x, 2);
            break;
        case 2: { /* $AC4D */
            boss_wall_stop(g, x);
            M(EntTimer + x)++;
            if (tile_at_offset(g, x, 0, 0xFC) == 0) {
                boss_state(g, x, 4);
                break;
            }
            uint8_t ex = M(EntX + x);
            if (ex < 0x19 || ex >= 0xE8) {
                turn_around(g, x); /* $B259 */
            }
            uint8_t d = (M(EntX + x) >= M(EntX)) ? (uint8_t)(M(EntX + x) - M(EntX)) : (uint8_t)(M(EntX) - M(EntX + x));
            if (d < 0x20) {
                boss_state(g, x, 3);
            } else if (M(EntTimer + x) >= 0x1C) {
                boss_state(g, x, 5);
            }
            break;
        }
        case 3: /* $AC7C */
            M(EntVelYFrac + x) = 0;
            M(EntVelY + x) = 0xF9;
            M(EntState + x)++;
            set_animation(g, x, 0x48);
            break;
        case 4: /* $AB7D: jump */
            ballistic(g, x, 6, 0);
            if (land_check(g, x, 0, 0xFB)) {
                M(EntState + x) = 7;
            }
            break;
        case 5: /* $AC8B: throws a projectile */
            boss_face(g, x);
            clear_vel_x(g, x);
            spawn_child(g, x, 2);
            M(EntState + x)++;
            set_animation(g, x, 0x47);
            break;
        case 6: /* $ACB2 */
            if (M(EntAnimFrame + x) != 0xFF) {
                break;
            }
            set_animation(g, x, 0x46);
            boss_state(g, x, 1);
            break;
        case 7: /* $ACC2 */
            set_animation(g, x, 0x46);
            clear_vel_x(g, x);
            clear_vel_y(g, x);
            if (((M(FrameCounter) >> 1) & 7) != 0) {
                break;
            }
            M(EntState + x) = 1;
            if (!(M(FrameCounter) & 0x40)) {
                M(EntState + x) = 5;
            }
            break;
        default:
            break;
    }
}
