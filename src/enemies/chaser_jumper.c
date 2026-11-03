/* Type $A6: walks, jumps and chases the player. */
#include "game.h"
#include "enemy.h"
#include "enemies/bouncer.h"
#include "enemies/foot_soldier.h"
#include "enemies/purple_ninja.h"
#include "enemies/walker_dropper.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- type $A6 (B5 $9DAB-$A07F): walks and jumps, changes direction at obstacles, chases the player --- */
static int chaser_scenery_blocks_left(Game *g, int x) /* B5 $A072: carry if the scenery blocks on the left ($B0) */
{
    return M(WaveActive) && M(EntX + x) < 8;
}

static int chaser_scenery_blocks_right(Game *g, int x) /* B5 $A07F */
{
    return M(WaveActive) && M(EntX + x) >= 0xF0;
}

static void chaser_face_velocity(Game *g, int x) /* B5 $9FAA: the sprite faces the side of the velocity */
{
    M(EntFlags + x) &= 0xFE;
    if (M(EntVelX + x) & 0x80) {
        M(EntFlags + x) |= 0x01;
    }
}

static void chaser_set_vel_x(Game *g, int x, uint16_t table,
                             uint8_t y) /* $9F7D ($A0B6) / $9F8D ($A0B2) then $9F9A: X velocity from the table, no more Y velocity */
{
    M(EntVelX + x) = R5(table + y);
    M(EntVelXFrac + x) = R5(table + y + 1);
    M(EntVelY + x) = 0;
    M(EntVelYFrac + x) = 0;
    if (M(EntVar0 + x) != 3) {
        chaser_face_velocity(g, x);
    }
}

static void chaser_start_jump(Game *g, int x) /* B5 $9EC0: hit, the jump begins (sub-state 3) */
{
    if (M(EntFlags + x) & 0x04) {
        M(EntHealth + x) = 0xFF;
        M(EntFlags + x) &= 0xFB;
        M(EntVar0 + x) = 3;
        M(EntAnimId + x)++;
        M(EntVelX + x) = 0;
        M(EntVelXFrac + x) = 0;
    }
    M(EntTimer + x) = 0;
}

static uint8_t chaser_wall_ahead(Game *g, int x) /* B5 $9E81: 0 = continues, 2 = turns around; returned in $00 and Y */
{
    uint8_t r;
    if (M(EntVelX + x) & 0x80) {
        r = (chaser_scenery_blocks_left(g, x) || tile_in_wall_set(g, x, 0xFC, 0xF0)) ? 0 : 2;
    } else {
        r = (chaser_scenery_blocks_right(g, x) || tile_in_wall_set(g, x, 0x04, 0xF0)) ? 2 : 0;
    }
    M(Tmp0) = r;
    return r;
}

static void chaser_gravity_step(Game *g, int x) /* B5 $A063 */
{
    if (!tile_in_wall_set(g, x, 0, 0xFC)) {
        M(EntVelY + x) = 2;
    }
}

static void chaser_jump(Game *g, int x) /* B5 $9EEA: sub-state 3, the jump (counter $0550 from 1 to $30) */
{
    M(EntFlags + x) &= 0xFB;
    uint8_t n = ++M(EntTimer + x);
    int player_left = M(EntX + x) < M(EntX);
    if (n == 0x18) { /* $9F46: throws a projectile */
        anim_if_changed(g, x, (uint8_t)(2 + M(EntAnimId + x)));
        int c = spawn_child_slot(g, x, 0x1C);
        if (c >= 0) {
            M(EntTimer + c) = M(EntTimer + x);
            M(EntVar0 + c) = M(EntVar0 + x);
            M(EntVar1 + c) = M(EntVar1 + x);
            M(EntFlags + c) = M(EntFlags + x);
            M(EntAnimId + c) = M(EntAnimId + x);
        }
        M(EntVelX + x) = 0;
        M(EntVelXFrac + x) = 0;
    } else if (n < 0x18) { /* $9F32 */
        chaser_set_vel_x(g, x, 0xA0B2, player_left ? 2 : 0);
    } else if (n == 0x30) { /* $9F02: end of the jump */
        M(EntHealth + x) = 0x20;
        M(EntTimer + x) = 0;
        M(EntAnimId + x)--;
        M(EntVar0 + x) = (uint8_t)((uint8_t)(M(EntAnimId + x) - 0xC1) >> 1);
        M(EntVar1 + x) = 0;
        chaser_set_vel_x(g, x, 0xA0B6, player_left ? 0 : 2);
    } else { /* $9F74 */
        M(EntVelX + x) = 0;
        M(EntVelXFrac + x) = 0;
    }
}

static void chaser_walk_toward(Game *g, int x) /* B5 $9FC2: walks and decides to jump or to charge the player */
{
    uint8_t hi = M(EntVar1 + x) >> 4;
    uint8_t y;
    if (hi == 0) { /* $9FCB: counter low, it sets off again towards the player */
        M(EntVar1 + x) = (uint8_t)(M(EntVar1 + x) + 0x10);
        y = M(EntX) < M(EntX + x) ? 2 : 0;
    } else {
        int blocked;
        M(Tmp0) = hi;
        if (M(EntVelX + x) & 0x80) {
            blocked = chaser_scenery_blocks_left(g, x) || tile_in_wall_set(g, x, 0xFC, 0xF0);
        } else {
            blocked = chaser_scenery_blocks_right(g, x) || tile_in_wall_set(g, x, 0x04, 0xF0);
        }
        if (blocked) {
            y = (M(EntVelX + x) & 0x80) ? 0 : 2; /* $A03B / $A032 */
        } else if (hi >= 4 || M(EntTimer + x) || (M(EntVar1 + x) & 0x0F)) {
            chaser_face_velocity(g, x);
            return;
        } /* $A00D -> $A04A */
        else {
            int d = (int)M(EntX) - (int)M(EntX + x) - 1; /* SBC without carry ($A01F) */
            if (d >= 0) {
                if (d < 0x40) {
                    chaser_face_velocity(g, x);
                    return;
                }
                y = 0;
            } else {
                if ((uint8_t)(M(EntX + x) - M(EntX) + 1) < 0x40) {
                    chaser_face_velocity(g, x);
                    return;
                }
                y = 2;
            }
        }
    }
    M(EntVelX + x) = R5(0xA0B6 + y); /* $A03D */
    M(EntVelXFrac + x) = R5(0xA0B7 + y);
    chaser_face_velocity(g, x); /* $A04A */
}

void chaser_substate_dispatch(Game *g, int x) /* B5 $9E03: state $20 according to the sub-state $0630 */
{
    switch (M(EntVar0 + x)) {
        case 0: /* $9E75 */
            chaser_start_jump(g, x);
            chaser_set_vel_x(g, x, 0xA0B2, chaser_wall_ahead(g, x));
            chaser_gravity_step(g, x);
            break;
        case 1: /* $9E69 */
            chaser_start_jump(g, x);
            chaser_set_vel_x(g, x, 0xA0B6, chaser_wall_ahead(g, x));
            chaser_gravity_step(g, x);
            break;
        case 3:
            chaser_jump(g, x);
            break;
        case 4: /* $A04E: landing */
            if (tile_in_wall_set(g, x, 0, 0xFC)) {
                M(EntVar0 + x) = 0;
                M(EntVelY + x) = 0;
                M(EntVelYFrac + x) = 0;
            }
            break;
        default: /* $9E1A */
            chaser_walk_toward(g, x);
            if (M(EntTimer + x) != 0x11) {
                uint8_t i = (uint8_t)(M(EntTimer + x) << 1);
                M(EntVelY + x) = R5(0xA08E + i);
                M(EntVelYFrac + x) = R5(0xA08F + i);
                uint8_t t = M(EntVar1 + x)++;
                if ((t & 0x0F) >= 4) {
                    M(EntVar1 + x) &= 0xF0;
                    M(EntTimer + x)++;
                }
            }
            if (M(EntTimer + x) >= 3 && tile_in_wall_set(g, x, 0, 0xF8)) {
                M(EntVar1 + x) = (uint8_t)(M(EntVar1 + x) + 0x10);
                M(EntTimer + x) = 0;
            }
            break;
    }
}

void ai_chaser_jumper(Game *g, int x) /* B5 $9DAB: start */
{
    if (!M(EntTimer + x)) {
        M(EntVar0 + x) = 0;
        M(EntVelY + x) = 0;
        M(EntVelYFrac + x) = 0;
        if (!tile_in_wall_set(g, x, 0, 0xFC)) {
            M(EntVar0 + x) = 4;
            M(EntVelY + x) = 2;
            M(EntVelYFrac + x) = 0;
        }
        M(EntFlags + x) &= 0xFE;
        uint8_t y = 0;
        if (M(EntX) < M(EntX + x)) {
            y = 2;
            M(EntFlags + x) |= 1;
        }
        M(EntVelX + x) = R5(0xA0B2 + y);
        M(EntVelXFrac + x) = R5(0xA0B3 + y);
    }
    set_state(g, x, 0x20);
}
