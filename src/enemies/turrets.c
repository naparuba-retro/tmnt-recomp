/* Turrets ($36-$3D), their projectile and types $41 / $48 / $35. */
#include "game.h"
#include "anim.h"
#include "enemy.h"
#include "player.h"
#include "enemies/chansawiac.h"
#include "enemies/common.h"
#include "enemies/foot_soldier.h"
#include "enemies/patrollers.h"
#include "enemies/purple_ninja.h"
#include "enemies/sewer.h"
#include "enemies/walker_dropper.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- turrets ($36-$3D, B5 $8EBD-$8F45), their projectile ($3E, $8F64), types $41/$48 ($8FE0) and $35 ($90D7) --- */
static int turret_tick(Game *g, int x, uint8_t *phase) /* B5 $8F46: carry = one frame out of 16, phase = $0630 & $30 */
{
    uint8_t px = M(EntX + x);
    if (px >= 0xEC || px < 0x18) {
        return 0;
    }
    M(EntVar0 + x)++;
    if (M(EntVar0 + x) & 0x0F) {
        return 0;
    }
    *phase = M(EntVar0 + x) & 0x30;
    return 1;
}

static void turret_fire(Game *g, int x, uint8_t st) /* B5 $8EFD: firing state, the turret blinks */
{
    set_state(g, x, st);
    M(EntFlags + x) |= 0x20;
    M(EntHurtTimer + x) = 0x30;
}

void ai_turret(Game *g, int x, uint8_t type) /* B5 $8EBD-$8F45: each type fires in two phases at most */
{
    uint8_t ph;
    if (!turret_tick(g, x, &ph)) {
        return;
    }
    static const uint8_t shots[8][2][2] = {/* {phase, state} per type $36-$3D; state $13-$16: routines $8EC6, $8ED3, $8EE0, $8EF4 */
                                           {{0x00, 0x13}, {0xFF, 0}},    {{0x10, 0x14}, {0xFF, 0}},    {{0x20, 0x15}, {0xFF, 0}},
                                           {{0x30, 0x16}, {0xFF, 0}},    {{0x00, 0x13}, {0x20, 0x15}}, {{0x00, 0x13}, {0x30, 0x16}},
                                           {{0x10, 0x14}, {0x20, 0x15}}, {{0x10, 0x14}, {0x20, 0x16}}};
    for (int i = 0; i < 2; i++) {
        if (shots[type - 0x36][i][0] != ph) {
            continue;
        }
        uint8_t st = shots[type - 0x36][i][1];
        if (st == 0x15 && M(EntX + x) < 0x10) {
            return; /* $8EE0 */
        }
        if (st == 0x16 && M(EntX + x) >= 0xF0) {
            return; /* $8EF4 */
        }
        turret_fire(g, x, st);
        return;
    }
}

static int turret_state_end(Game *g, int x) /* B5 $902A: carry = the blinking is over */
{
    hurt_blink_tick(g, x);
    if (M(EntHurtTimer + x)) {
        return 0;
    }
    M(EntFlags + x) &= 0xDF;
    M(EntAttr + x) = 3;
    M(EntState + x) = 0;
    M(EntRoutine + x) = 0;
    M(EntAnimFrame + x) = 0;
    return 1;
}

void turret_state(Game *g, int x, uint8_t shot) /* B5 $9006-$9021 then $904E: fires a projectile (child 9 to 12) */
{
    if (!turret_state_end(g, x)) {
        return;
    }
    int n = spawn_child_slot(g, x, shot);
    if (n < 0) {
        return;
    }
    uint16_t t = (uint16_t)(0x9078 + (shot - 9) * 3);
    M(EntVelX + n) = R5(t);
    M(EntVelY + n) = R5(t + 1);
    M(EntAnimId + n) = R5(t + 2);
}

void ai_turret_shot(Game *g, int x) /* B5 $8F64: the projectile disappears when hitting a turret */
{
    uint8_t ox, oy;
    if (M(EntVelY + x) | M(EntVelYFrac + x)) {
        ox = 0;
        oy = (M(EntVelY + x) & 0x80) ? 0xEA : 0x06;
    } else {
        ox = (M(EntVelX + x) & 0x80) ? 0xF4 : 0x0C;
        oy = 0;
    }
    uint8_t px = (uint8_t)(M(EntX + x) + ox), py = (uint8_t)(M(EntY + x) + oy);
    for (int y = 5; y < 16; y++) {
        uint8_t t = M(EntType + y);
        if (t < 0x36 || t >= 0x3E) {
            continue;
        }
        uint8_t dx = (uint8_t)(M(EntX + y) - px), dy;
        if (M(EntX + y) < px) {
            dx = (uint8_t)(0 - dx); /* $8FD8 */
        }
        if (dx >= 0x0C) {
            continue;
        }
        dy = (uint8_t)(M(EntY + y) - py);
        if (M(EntY + y) < py) {
            dy = (uint8_t)(0 - dy);
        }
        if (dy >= 0x0A) {
            continue;
        }
        clear_entity(g, x);
    }
}

void ai_boss74_shot(Game *g, int x, uint8_t vx, uint8_t vy) /* B5 $B3FF-$B429 (types $44-$47), $B434: projectile of boss $74, state 7 */
{
    set_vel_x(g, x, 0, vx);
    set_vel_y_abs(g, x, 0, vy);
    M(EntRoutine + x) = 7;
    M(EntState + x)++;
    if (M(EntX) & 0x80) {
        M(EntFlags + x) ^= 1; /* $B4F4: $0480 is the player's position */
    } else {
        M(EntVelX + x) = (uint8_t)(0 - M(EntVelX + x));
    }
}

void ai_slow_advancer(Game *g, int x) /* B5 $8FE0: advances slowly to the edge of the screen */
{
    if (M(EntFlags + x) & 1) {
        if (M(EntX + x) < 0x18) {
            clear_vel_x(g, x);
        } else {
            set_vel_x(g, x, 0x7C, 0xFF);
        }
    } else {
        if (M(EntX + x) >= 0xE4) {
            clear_vel_x(g, x);
        } else {
            set_vel_x(g, x, 0x84, 0);
        }
    }
}

void ai_close_attacker(Game *g, int x) /* B5 $90D7: jumps, attacks the player at close range */
{
    if (M(EntFlags + x) & 4) {
        clear_velocity(g, x);
        return;
    }
    if (!front_tile_set(g, x, 0x0E, 2)) {
        hop_start(g, x, 7);
        return;
    }
    face_player_get_distance(g, x);
    int attack = 0;
    if (M(Tmp0) < 0x20) {
        if (!(M(EntStatus) & M(Tmp4))) {
            attack = 1;
        } else if (++M(EntVar0 + x) < 3) {
            attack = 1;
        }
    }
    if (attack) {
        set_animation(g, x, 0x3F);
        clear_velocity(g, x);
        set_state(g, x, 3);
        return;
    }
    hop_start(g, x, 5);
    walk_and_stop(g, x, 2, 0);
}

void update_hurt_flash(Game *g, int x) /* B5 $8049 */
{
    if (!(M(EntFlags + x) & 4)) {
        return;
    }
    if (--M(EntHurtTimer + x) == 0) {
        M(EntFlags + x) ^= 4;
        M(EntAttr + x) = 0;
        return;
    }
    M(EntAttr + x) = M(EntHurtTimer + x) & 3;
}

void ai_van_shot(Game *g, int x) /* B7 $FD6A: shot of the van, destroyed by a wall or when leaving the screen */
{
    uint8_t t = tile_at_offset(g, x, 0, 0xF8); /* $F091 */
    if (t != 0 && t != 2 && t != 0x0C) {
        clear_entity(g, x);
        return;
    }
    if ((uint8_t)(M(EntX + x) + 0x0A) < 0x11) {
        clear_entity(g, x);
        return;
    } /* $FD9A */
    if (M(SideViewFlag)) { /* the carry is 1 after the CMP: +$40 and +8 */
        if ((uint8_t)(M(EntY + x) + 0x40) < 0x44) {
            clear_entity(g, x);
        }
    } else if ((uint8_t)(M(EntY + x) + 0x08) < 0x10) {
        clear_entity(g, x);
    }
}

void clamp_entity_position(Game *g, int x) /* B5 $8077 */
{
    uint8_t t = M(EntType + x);
    if (t >= 0x75 && t < 0x79) {
        return;
    }
    if (t == 0x4D || t == 0x4F) { /* $806C -> B6 $BD64: only the screen column counts */
        uint8_t xa = t == 0x4F ? (uint8_t)(M(EntX + x) + 0x0A) : (uint8_t)(M(EntX + x) - R6(0xBCE9 + ((M(EntVar0 + x) & 0x0F) << 1)) + 8);
        uint8_t ey = M(EntY + x);
        if (xa >= 3 && xa < 0xFB && ey >= R5(0x80FB) && ey < R5(0x80FC)) {
            M(Tmp0) = 0;
            return;
        }
        if (M(EntFlags + x) & 8) {
            M(Tmp0) = 1;
            return;
        }
        clear_entity(g, x);
        return;
    }
    if (x < 8 && M(WaveActive)) { /* $8091: during a wave the enemy stays on the screen */
        uint8_t ex = M(EntX + x);
        if (ex < 6) {
            M(EntX + x) = 6;
        } else if (ex >= 0xF6) {
            M(EntX + x) = 0xF6;
        }
    }
    uint8_t y = M(SideViewFlag) ? 0 : 4;
    uint8_t ex = M(EntX + x), ey = M(EntY + x);
    if (ex >= R5(0x80F9 + y) && ex < R5(0x80FA + y) && ey >= R5(0x80FB + y) && ey < R5(0x80FC + y)) {
        M(Tmp0) = 0;
        return;
    }
    if (M(EntFlags + x) & 8) {
        M(Tmp0) = 1;
        return;
    } /* $80D1: out of the zone -> removed */
    if (t == 0x0E) {
        TODO("clamp type $0E");
        return;
    }
    clear_entity(g, x);
}
