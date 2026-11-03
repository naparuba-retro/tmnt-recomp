/* Types $9A / $9B: patrol, and the defeated enemy that disappears. */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "enemy.h"
#include "enemies/foot_soldier.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* ------------------------------------------------- AI of types $9A / $9B: patrol (B5 $877E / $87C0) --- */
static int patrol_anim_done(Game *g, int x, uint8_t anim, int clear_x) /* $888A / $88EB: animation then crossed velocity to zero */
{
    if (M(EntAnimId + x) != anim) {
        set_animation(g, x, anim);
    }
    if (clear_x) {
        clear_vel_x(g, x);
    } else {
        clear_vel_y(g, x);
    }
    return 1;
}

int patrol_right(Game *g, int x) /* B5 $882D */
{
    if (blocked(g, x, 0x10, 0xE8) || blocked(g, x, 0x10, 0xFC)) {
        return 0;
    }
    set_vel_x(g, x, M(InVanFlag) ? 0x60 : 0x20, 2);
    M(EntFlags + x) &= 0xFE;
    return patrol_anim_done(g, x, 0xB2, 0);
}

int patrol_left(Game *g, int x) /* B5 $885D */
{
    if (blocked(g, x, 0xF0, 0xE8) || blocked(g, x, 0xF0, 0xFC)) {
        return 0;
    }
    set_vel_x(g, x, M(InVanFlag) ? 0xA0 : 0xE0, 0xFD);
    M(EntFlags + x) |= 0x01;
    return patrol_anim_done(g, x, 0xB2, 0);
}

int patrol_up(Game *g, int x) /* B5 $889B */
{
    if (blocked(g, x, 0x08, 0xE4) || blocked(g, x, 0xF8, 0xE4)) {
        return 0;
    }
    set_vel_y(g, x, M(InVanFlag) ? 0x80 : 0xC0, 0xFD);
    return patrol_anim_done(g, x, 0xB0, 1);
}

int patrol_down(Game *g, int x) /* B5 $88C4 */
{
    if (blocked(g, x, 0x08, 0x02) || blocked(g, x, 0xF8, 0x02)) {
        return 0;
    }
    set_vel_y(g, x, M(InVanFlag) ? 0x80 : 0x40, 2);
    return patrol_anim_done(g, x, 0xB1, 1);
}

void clear_velocity(Game *g, int x) {
    clear_vel_x(g, x);
    clear_vel_y(g, x);
}

void ai_patroller_a(Game *g, int x) /* B5 $877E */
{
    if (!(M(EntVelY + x) | M(EntVelYFrac + x))) {
        if (!(M(EntFlags + x) & 1)) { /* $878D */
            if (patrol_right(g, x)) {
                return;
            }
            goto up;
        }
        if (patrol_left(g, x)) {
            return; /* $8794 */
        }
        goto down;
    }
    if (!(M(EntVelY + x) & 0x80)) {
        goto down; /* $879B */
    }
up: /* $87A0 */
    if (patrol_up(g, x)) {
        return;
    }
    M(EntFlags + x) |= 1;
    clear_velocity(g, x);
    return;
down: /* $87B0 */
    if (patrol_down(g, x)) {
        return;
    }
    M(EntFlags + x) &= 0xFE;
    if (patrol_right(g, x)) {
        return; /* $878D */
    }
    goto up;
}

void ai_patroller_b(Game *g, int x) /* B5 $87C0 */
{
    if (!(M(EntVelY + x) | M(EntVelYFrac + x))) {
        if (!(M(EntFlags + x) & 1)) {
            if (patrol_right(g, x)) {
                return;
            }
            goto down; /* $87F2 */
        }
        goto left; /* $87D6 */
    }
    if (!(M(EntVelY + x) & 0x80)) {
        goto down; /* $87DD */
    }
    goto up;
down:
    if (patrol_down(g, x)) {
        return;
    }
left:
    if (patrol_left(g, x)) {
        return; /* $87D6 */
    }
up: /* $87E2 */
    if (patrol_up(g, x)) {
        return;
    }
    M(EntFlags + x) &= 0xFE;
    clear_velocity(g, x);
}

void ai_defeated_enemy(Game *g, int x) /* B5 $8733: types $15/$2C/$52, the defeated enemy disappears */
{
    ROM_TABLE(drop_type, 4, 5, 0x8776);
    ROM_TABLE(drop_anim, 4, 5, 0x877A); /* $8776 */
    if (M(EntAnimFrame + x) != 0xFF) {
        return;
    }
    if (M(SideViewFlag)) {
        M(0x03D8)++;
        if (M(0x03D8) == 0x19) {
            M(0x03D8) = 0;
            if (!(M(RandomSeed) & 1)) { /* object left by the enemy */
                uint8_t i = (M(RandomSeed) >> 1) & 3;
                M(EntType + x) = drop_type[i];
                set_animation(g, x, drop_anim[i]);
                M(EntVar0 + x) = 0x80;
                return;
            }
        }
    }
    clear_entity(g, x);
}
