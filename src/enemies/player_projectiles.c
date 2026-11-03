/* Player projectiles: shuriken, sub-weapons, boomerang. */
#include "game.h"
#include "player.h"
#include "sound.h"
#include "enemy.h"
#include "enemies/rocksteady.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* ------------------------------------------------------------ player projectiles: B7 $FD7E (shuriken), $FD9A, $FDBE (boomerang) --- */
void subweapon_offscreen(Game *g, int x) /* B7 $FD9A: disappears when leaving the screen through the top or the left */
{
    if ((uint8_t)(M(EntX + x) + 0x0A) < 0x11) {
        clear_entity(g, x);
        return;
    }
    uint8_t lim = M(SideViewFlag) ? 0x44 : 0x10;
    uint8_t add = M(SideViewFlag) ? 0x40 : 0x08;
    if ((uint8_t)(M(EntY + x) + add) < lim) {
        clear_entity(g, x);
    }
}

void ai_shuriken(Game *g, int x) /* B7 $FD7E */
{
    uint8_t t = tile_at_offset(g, x, 0, 0xF8);
    if (t == 0 || t == 2 || t == 9 || t == 0x0A || t == 7) {
        subweapon_offscreen(g, x);
        return;
    }
    clear_entity(g, x); /* wall */
}

static uint8_t abs_diff(uint8_t a, uint8_t b) {
    return a >= b ? (uint8_t)(a - b) : (uint8_t)(b - a);
}

void ai_boomerang(Game *g, int x) /* B7 $FDBE: comes back towards the player */
{
    if (M(EntAnimFrame + x) != 1) {
        sound_play(g, 0x16);
    }
    if (M(EntFlags + x) & 1) {
        add_vel_x_a(g, x, 0x28);
    } else {
        sub_vel_x_a(g, x, 0x28);
    }
    if (M(EntVar0 + x)) {
        M(EntVar0 + x)--;
        return;
    }
    uint8_t dx = (M(EntX) >= M(EntX + x)) ? (uint8_t)(M(EntX) - M(EntX + x)) : (uint8_t)(M(EntX + x) - M(EntX));
    uint8_t dy = (M(EntY) >= M(EntY + x)) ? (uint8_t)(M(EntY) - M(EntY + x)) : (uint8_t)(M(EntY + x) - M(EntY));
    (void)abs_diff;
    if (dx >= 0x0C || dy >= 0x18) {
        subweapon_offscreen(g, x);
        return;
    }
    uint8_t t = M(CurrentTurtle), w = M(SubWeaponType0 + t); /* caught: one more ammo */
    if (w == 0) {
        M(SubWeaponType0 + t) = 3;
        M(SubWeaponSelected) = 1;
        M(SubWeaponAmmo0 + t)++;
    } else if (w == 3) {
        M(SubWeaponAmmo0 + t)++;
    }
    clear_entity(g, x);
}

void ai_hostage_boss_shot(Game *g, int x) /* B5 $B3E2: projectile of boss $71 */
{
    M(EntRoutine + x) = 7;
    M(EntState + x)++;
    if (!(M(EntFlags + x) & 1)) {
        M(EntVelXFrac + x) = 0;
        M(EntVelX + x) = 0x05;
    } /* B7 $F081 */
    else {
        M(EntVelXFrac + x) = 0;
        M(EntVelX + x) = 0xFB;
    }
}
