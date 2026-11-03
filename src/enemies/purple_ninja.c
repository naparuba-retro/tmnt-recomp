/* purple_ninja ($98) and its projectile ($99). */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "enemy.h"
#include "enemies/common.h"
#include "enemies/foot_soldier.h"
#include "enemies/spawning.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* ------------------------------------------------ purple_ninja ($98) and its projectile ($99), B5 $864B-$9784 --- */
void distance_to_player(Game *g, int x) /* B5 $8361: Tmp0 = |dx| (without changing the orientation) */
{
    M(Tmp0) = (M(EntX) >= M(EntX + x)) ? (uint8_t)(M(EntX) - M(EntX + x)) : (uint8_t)(M(EntX + x) - M(EntX));
    if (M(EntY) >= M(EntY + x)) {
        M(Tmp0 + 1) = (uint8_t)(M(EntY) - M(EntY + x));
        M(Tmp0 + 2) = 0;
    } else {
        M(Tmp0 + 1) = (uint8_t)(M(EntY + x) - M(EntY));
        M(Tmp0 + 2) = 1;
    }
}

void set_vel_y_abs(Game *g, int x, uint8_t frac, uint8_t whole) {
    M(EntVelYFrac + x) = frac;
    M(EntVelY + x) = whole;
} /* B7 $F089 */

int spawn_child_slot(Game *g, int x, uint8_t idx) /* B5 $8101: creates a projectile; returns its entity or -1 (carry) */
{
    M(CurrentEntity) = (uint8_t)x;
    uint16_t o = (uint16_t)(0x816D + idx * 3);
    uint8_t dx = R5(o + 1), dy = R5(o + 2);
    uint8_t px = (M(EntFlags + x) & 1) ? (uint8_t)(M(EntX + x) - dx) : (uint8_t)(M(EntX + x) + dx);
    uint8_t py = (uint8_t)(dy + M(EntY + x));
    int n = 0x0B; /* B7 $EEE6: entities 11 to 14 */
    while (n < 0x0F && M(EntType + n)) {
        n++;
    }
    if (n == 0x0F) {
        return -1;
    }
    M(EntX + n) = px;
    M(EntY + n) = py;
    M(EntStatus + n) = 0x80;
    M(EntType + n) = R5(o); /* B7 $D12E -> B6 $8272 */
    if (init_entity_type_tail(g, n, M(EntType + n))) {
        return -1;
    }
    M(EntFlags + n) = (uint8_t)((M(EntFlags + x) & 1) | 0xC2);
    return n;
}

int spawn_child(Game *g, int x, uint8_t idx) {
    return spawn_child_slot(g, x, idx) < 0;
} /* carry = failure */

/* B5 $84B1 / $84A4: ballistic movement with collisions; a = index in the tables $855A-$857D; sub = 0 gravity, 1 upward force */
void ballistic(Game *g, int x, uint8_t idx, int sub) {
    uint8_t g8 = R5(0x857D + idx);
    if (!sub) {
        add_vel_y(g, x, g8);
    } else { /* B7 $F023 */
        unsigned s = M(EntVelYFrac + x) + (uint8_t)(((uint8_t)~g8) + 1);
        M(EntVelYFrac + x) = (uint8_t)s;
        M(EntVelY + x) = (uint8_t)(M(EntVelY + x) + 0xFF + (s > 0xFF));
    }
    uint8_t b08 = R5(0x855A + idx), b09 = R5(0x8561 + idx), b0a = R5(0x8568 + idx), b0b = R5(0x856F + idx), b0c = R5(0x8576 + idx);
    if (M(EntVelX + x) & 0x80) {
        b08 = (uint8_t)(0 - b08);
        b0b = (uint8_t)(0 - b0b);
    }
    int blocked = 0;
    if (M(EntVelY + x) & 0x80) {
        uint8_t ny = (uint8_t)(b09 + M(EntVelY + x));
        blocked = in_block_list(tile_at_offset(g, x, (uint8_t)(b08 + M(EntVelX + x)), b09));
        b09 = ny;
    } else {
        blocked = in_block_list(tile_at_offset(g, x, b0b, 0));
    }
    if (!blocked) {
        uint8_t ny = (uint8_t)(b0a + M(EntVelY + x));
        uint8_t nx = (uint8_t)(b0b + M(EntVelX + x));
        blocked = in_block_list(tile_at_offset(g, x, nx, b0a));
        b0a = ny;
        b0b = nx;
    }
    if (blocked) { /* $8530 */
        if (M(EntVelY + x) & 0x80) {
            clear_vel_y(g, x);
        }
        clear_vel_x(g, x);
        return;
    }
    if (M(EntVelY + x) & 0x80) { /* $853E: head */
        uint8_t ny = (uint8_t)(b0c + M(EntVelY + x));
        if (in_block_list(tile_at_offset(g, x, 0, b0c))) {
            clear_vel_y(g, x);
        }
        (void)ny;
    }
}

void ninja_set_anim_state(Game *g, int x, uint8_t anim) /* B5 $86CE: animation, state 3 */
{
    set_animation(g, x, anim);
    M(EntRoutine + x) = 3;
    M(EntState + x)++;
}

static void ninja_to_state1(Game *g, int x) /* B5 $86F9: jumps towards the player */
{
    M(EntRoutine + x) = 1;
    M(EntState + x)++;
    set_vel_y_abs(g, x, 0, 0xFA);
    walk_speed(g, x, 1, 0x40); /* $86E6 */
    if (move_blocked(g, x, 0x0A, 0xF0)) {
        clear_vel_x(g, x);
    }
    if (move_blocked(g, x, 0x18, 0xF0)) {
        turn_around(g, x); /* $86ED */
    }
}

void ai_purple_ninja(Game *g, int x) /* B5 $864B */
{
    if (M(EntFlags + x) & 4) { /* hit */
        if (M(EntAnimId + x) != 0x98) {
            M(EntVar0 + x) = 0x10;
            set_animation(g, x, 0x98);
            clear_vel_x(g, x);
            clear_vel_y(g, x);
        } else if (--M(EntVar0 + x) == 0) {
            ninja_to_state1(g, x);
        }
        return;
    }
    if (!front_tile_set(g, x, 0x0C, 0xFF)) { /* no more ground: fall */
        M(EntVelY + x) = 2;
        clear_vel_x(g, x);
        M(EntRoutine + x) = 2;
        M(EntState + x)++;
        return;
    }
    face_player_get_distance(g, x);
    uint8_t d = M(Tmp0);
    if (d >= 0x60) {
        ninja_to_state1(g, x);
        return;
    }
    if (d >= 0x40) {
        if (M(FrameCounter) >= 0xC0) {
            goto walk;
        }
        ninja_set_anim_state(g, x, (uint8_t)(0x95 + (M(SubWeaponSelected) ? 1 : 0)));
        spawn_child(g, x, 0);
        return;
    }
    if (d >= 0x18) {
        goto walk;
    }
    {
        uint8_t y;
        if (M(SubWeaponSelected)) {
            y = (M(FrameCounter) & 8) ? 0x91 : 0x92;
        } else {
            y = (M(FrameCounter) >= 0x60) ? 0x93 : 0x94;
        }
        ninja_set_anim_state(g, x, y);
        clear_vel_x(g, x);
        clear_vel_y(g, x);
        return;
    }
walk:
    if (M(EntAnimId + x) != 0x90) {
        set_animation(g, x, 0x90);
        walk_speed(g, x, 1, 0x40); /* $8584 */
        if (move_blocked(g, x, 0x0A, 0xF0)) {
            clear_vel_x(g, x);
        }
    }
    if (move_blocked(g, x, 0x18, 0xF0)) {
        turn_around(g, x); /* $86ED */
    }
}

void ninja_state_fall(Game *g, int x) /* B5 $969F: state 2 */
{
    distance_to_player(g, x);
    if (M(Tmp0) < 0x20) {
        M(EntRoutine + x) = 0x96;
        set_animation(g, x, 0x11);
        clear_vel_x(g, x);
        clear_vel_y(g, x);
        return;
    }
    if (!M(EntWait + x)) {
        ballistic(g, x, 5, 0);
        if (!(M(EntVelY + x) & 0x80) && M(EntY + x) >= M(EntVar1 + x)) {
            M(EntVar1 + x) = M(EntY + x);
            M(EntWait + x)++;
        }
    } else {
        ballistic(g, x, 5, 1);
        if ((M(EntVelY + x) & 0x80) && M(EntY + x) < M(EntVar1 + x)) {
            M(EntWait + x)--;
            M(EntVar1 + x) = M(EntY + x);
        }
    }
}

void ninja_state_jump(Game *g, int x) /* B5 $983F: state 3 (jump of the throw) */
{
    ballistic(g, x, 3, 0);
    if (M(EntVelY + x) & 0x80) {
        return;
    }
    land_check(g, x, 0, 0xFC);
}

void ai_purple_ninja_shot(Game *g, int x) /* B5 $8708 : projectile */
{
    M(EntRoutine + x) = 4;
    M(EntState + x)++;
    if (M(EntX) >= M(EntX + x)) {
        M(EntFlags + x) &= 0xFE;
        set_vel_x(g, x, 0, 3);
    } else {
        M(EntFlags + x) |= 1;
        set_vel_x(g, x, 0, 0xFD);
    }
}

void wait_anim_end(Game *g, int x) /* B5 $832F: end of animation -> state 0 */
{
    if (M(EntAnimFrame + x) != 0xFF) {
        return;
    }
    M(EntState + x) = 0;
    M(EntRoutine + x) = 0;
    M(EntAnimFrame + x) = 0;
}

/* B5 $82D1: states of the enemies whose animation has bit 7 (attacks, jumps); table $82E3 */
void anim_state_arc(Game *g, int x) /* $8A3B: ballistic jump */
{
    ballistic(g, x, 0, 0);
    uint8_t vy = M(EntVelY + x);
    if (!(vy & 0x80)) {
        if (vy >= 2) {
            set_animation(g, x, 0x90);
        }
        land_check(g, x, 0, 0xFC);
    } else if (vy < 0xFD) {
        set_animation(g, x, 0xAB);
    } else if (M(EntAnimId + x) != 0x17) {
        set_animation(g, x, 0x97);
    }
}
