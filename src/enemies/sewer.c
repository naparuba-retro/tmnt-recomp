/* Sewer enemies: types $4B and $85. */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "enemy.h"
#include "enemies/chansawiac.h"
#include "enemies/common.h"
#include "enemies/foot_soldier.h"
#include "enemies/purple_ninja.h"
#include "enemies/roof_leaper.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* ------------------------------------------------------------ sewer: types $4B ($9122) and $85 ($94A5, state $9726) --- */
void walk_and_stop(Game *g, int x, uint8_t whole, uint8_t frac) /* B5 $8584 */
{
    walk_speed(g, x, whole, frac);
    if (move_blocked(g, x, 0x0A, 0xF0)) {
        clear_vel_x(g, x);
    }
}

void ai_sewer_thrower(Game *g, int x) /* B5 $9122 */
{
    if (!front_tile_set(g, x, 0x0C, 0xFF)) { /* no more ground: it jumps */
        hop_start(g, x, 8);
        clear_vel_x(g, x);
        return;
    }
    M(EntVar0 + x)++;
    if ((M(EntVar0 + x) & 0x1F) == 0) { /* $9184: throws a projectile */
        set_animation(g, x, 0x7C);
        int c = spawn_child_slot(g, x, 0x17);
        M(EntVar0 + (c >= 0 ? c : 0x17 * 3)) = 0x10; /* failure: Y stays at idx * 3 ($8101) */
        clear_vel_x(g, x);
        M(EntRoutine + x) = 3;
        M(EntState + x)++;
        return;
    }
    if (M(EntAnimId + x) != 0x7B) { /* $913D */
        if (!(M(EntFlags + x) & 1)) {
            if (M(EntX + x) >= 0xE8) {
                M(EntFlags + x) |= 1;
            }
        } else if (M(EntX + x) < 0x18) {
            M(EntFlags + x) &= 0xFE;
        }
        walk_and_stop(g, x, 1, 0x28);
        set_animation(g, x, 0x7B);
    }
    if (move_blocked(g, x, 0x14, 0xDC)) {
        turn_around(g, x); /* $9178 */
    }
}

void ai_sewer_walker(Game *g, int x) /* B5 $94A5: starts walking in the direction of the scrolling */
{
    M(EntState + x)++; /* $97FE */
    if (!(M(CameraDeltaX) & 0x80)) {
        go_left(g, x);
    } else {
        go_right(g, x);
    }
    M(EntRoutine + x) = M(EntType + x);
}

void sewer_walker_leap(Game *g, int x) /* B5 $9726: leaps onto the player when it is at the right distance */
{
    ROM_TABLE(band_dy, 7, 5, 0x9775); /* $9775 */
    ROM_TABLE(band_hi, 8, 5, 0x977C); /* $977C */
    if (M(EntVelY + x) != 0) {
        return;
    }
    distance_to_player(g, x);
    int y = 0;
    while (y < 7 && M(Tmp0 + 1) < band_dy[y]) {
        y++;
    }
    if (y == 7) {
        return;
    }
    if (M(Tmp0) >= band_hi[y] || M(Tmp0) < band_hi[y + 1]) {
        return;
    }
    uint8_t side = M(Tmp0 + 2);
    static const uint8_t vy[2] = {0x05, 0xF9}, anim[2] = {0x0F, 0x0E}, vx[2] = {0x01, 0xFE}, vxf[2] = {0xC0, 0x40};
    M(EntVelY + x) = vy[side];
    M(EntVelYFrac + x) = 0x80;
    set_animation(g, x, anim[side]);
    int d = (M(EntFlags + x) & 1) ? 1 : 0;
    M(EntVelX + x) = vx[d];
    M(EntVelXFrac + x) = vxf[d];
}

void ai_sewer_thrower_shot(Game *g, int x) /* B5 $919B: projectile of $4B, then it becomes $83 */
{
    if (!M(EntVar0 + x)) { /* $91AD */
        if (M(EntAnimFrame + x) != 0xFF) {
            return;
        }
        set_animation(g, x, 0x23);
        M(EntType + x) = 0x83;
        return;
    }
    if (--M(EntVar0 + x) != 0) {
        walk_speed(g, x, 3, 0x40);
        return;
    } /* $91BF */
    set_animation(g, x, 0x7E);
    clear_vel_x(g, x);
}

void ai_leaper(Game *g, int x) /* B5 $979B: leaps towards the player */
{
    M(EntRoutine + x) = M(EntType + x);
    M(EntState + x)++;
    M(EntVelY + x) = 0xFE;
    if (M(EntX) >= M(EntX + x)) {
        M(EntFlags + x) &= 0xFE;
        M(EntVelX + x) = 0x01;
        M(EntVelXFrac + x) = 0xA0;
    } else {
        M(EntFlags + x) |= 1;
        M(EntVelX + x) = 0xFE;
        M(EntVelXFrac + x) = 0x60;
    }
}

void ai_fast_walker(Game *g, int x) /* B5 $9977 */
{
    walk_speed(g, x, 3, 0);
    M(EntRoutine + x) = 0x8D; /* $8644 */
    M(EntState + x)++;
}

void ai_defensive_hopper(Game *g, int x) /* B5 $8A94 */
{
    if (M(EntFlags + x) & 4) {
        clear_vel_x(g, x);
        clear_vel_y(g, x);
        return;
    }
    if (!front_tile_set(g, x, 0, 2)) { /* no more ground: it falls */
        M(EntVelY + x) = 3;
        clear_vel_x(g, x);
        set_animation(g, x, 0x9E);
        M(EntRoutine + x) = 6;
        M(EntState + x)++;
        return;
    }
    face_player_get_distance(g, x);
    if (M(Tmp0) >= 0x20) {
        walk_speed(g, x, 2, 0);
        hop_start(g, x, 2);
        return;
    } /* $8AF9 */
    if (M(EntStatus) & M(Tmp4)) { /* $8AC2 */
        if (M(SubWeaponSelected)) {
            walk_speed(g, x, 2, 0);
            hop_start(g, x, 3);
            return;
        }
        if (M(FrameCounter) >= 0xC0) {
            walk_speed(g, x, 2, 0);
            hop_start(g, x, 2);
            turn_around(g, x);
            return;
        }
    }
    if (M(FrameCounter) < 0x60) {
        return; /* $8AE5 */
    }
    set_animation(g, x, 0x9F);
    clear_vel_x(g, x);
    clear_vel_y(g, x);
    M(EntRoutine + x) = 3;
    M(EntState + x)++;
}
