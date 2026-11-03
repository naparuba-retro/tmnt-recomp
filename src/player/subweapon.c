/* Sub-weapons (shuriken, boomerang) and ammo. */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "sound.h"
#include "player/pose.h"
#include "player/subweapon.h"
#include "player/tiles.h"
#define TODO_AREA "player" /* prefix of the TODO traces of this file */

/* --- sub-weapons (shuriken...): B4 $9E97-$9F1C, B7 $E083-$E124 --- */
void subweapon_sound(Game *g) /* B4 $9F6E: sound effect according to the turtle's weapon */
{
    sound_play(g, R4(0x9F7F + M(SubWeaponType0 + M(CurrentTurtle))));
}

int free_projectile_slot(Game *g) /* B7 $EEDA: free entity among 2 to 4, -1 if none */
{
    for (int x = 2; x < 5; x++) {
        if (!M(EntType + x)) {
            return x;
        }
    }
    return -1;
}

static void projectile_init(Game *g, int x) /* B7 $E0C7 */
{
    uint8_t w = M(SubWeaponType0 + M(CurrentTurtle));
    clear_entity(g, x);
    M(EntFlags + x) = (uint8_t)((M(EntFlags) & 1) | 0xC2);
    M(EntAnimId + x) = R7(0xE11C + w);
    M(EntType + x) = R7(0xE120 + w);
    M(EntVar0 + x) = 0x0A;
    M(EntAnimTimer + x) = 3;
    M(EntX + x) = M(EntX);
    M(EntY + x) = M(EntY);
    if (M(EntAnimId) == 0x84) {
        M(EntY + x) = (uint8_t)(M(EntY + x) + 0x0C);
    }
    M(EntVelX + x) = (M(EntFlags) & 1) ? 0xFB : 0x05;
}

void subweapon_throw(Game *g) /* B7 $E083 */
{
    uint8_t t = M(CurrentTurtle);
    int x = free_projectile_slot(g);
    if (x < 0) {
        return;
    }
    projectile_init(g, x);
    if (M(SubWeaponType0 + t) == 2) { /* $E0A2: three projectiles: straight, upward, downward */
        x = free_projectile_slot(g);
        if (x >= 0) {
            projectile_init(g, x);
            M(EntVelY + x) = 0xFF;
            x = free_projectile_slot(g);
            if (x >= 0) {
                projectile_init(g, x);
                M(EntVelY + x) = 0x01;
            }
        }
    }
    if (--M(SubWeaponAmmo0 + t) == 0) {
        M(SubWeaponType0 + t) = 0;
        M(SubWeaponSelected) = 0;
    } /* $E095: no more ammo */
}

void player_attack(Game *g) /* B4 $9E7E */
{
    if (M(EntStatus) & 0x04) {
        return;
    }
    if (M(EntStatus) & 0x10) {
        return; /* $9F22: no attack during the jump */
    }
    M(EntStatus) |= 0x04;
    uint8_t t = M(CurrentTurtle), anim;
    if (M(SubWeaponSelected) && M(SubWeaponType0 + t)) {
        subweapon_sound(g); /* $9E97 */
    } else {
        if (M(SubWeaponSelected)) {
            M(SubWeaponSelected) = 0; /* no more weapon: back to the normal attack */
        }
        attack_sound(g);
    }
    if (M(JoyHeld1) & 0x80) { /* $9EEE : A + B */
        M(0xD4) = 0;
        if (M(SubWeaponSelected)) { /* $9F0A */
            subweapon_sound(g);
            set_animation(g, 0, 0x86);
            subweapon_throw(g);
            return;
        }
        attack_sound(g);
        anim = R4(0x9F50 + (uint8_t)((M(JoyHeld1) & 0x0C) + t));
    } else if (M(SubWeaponSelected)) { /* $9ED8 */
        M(0xD4) = (M(JoyHeld1) & 0x04) ? 1 : 0;
        set_animation(g, 0, (M(JoyHeld1) & 0x04) ? 0x84 : 0x82);
        subweapon_throw(g);
        return;
    } else if (!(M(JoyHeld1) & 0x0C)) {
        M(0xD4) = 0;
        anim = R4(0x9F50 + t);
    } else if (!(M(JoyHeld1) & 0x08)) {
        M(0xD4) = 1;
        anim = R4(0x9F4C + t);
    } else {
        M(0xD4) = 0;
        anim = R4(0x9F48 + t);
    }
    set_animation(g, 0, anim);
    weapon_effect_start(g);
}

void player_walk_up_down(Game *g) /* B4 $9DF0 */
{
    clear_vel_x(g, 0);
    if (M(JoyHeld1) & 0x04) { /* down: $9E14 */
        if (!(M(EntStatus) & 0x04) && probe_tile(g, 0x08, 0x01)) {
            if (M(EntState) != 0x0B || M(WaveState) == 0) {
                return;
            }
        }
        M(EntState) = 0x02; /* $9E2F */
        clear_vel_y(g, 0);
        probe_tile(g, 0x08, 0x0A);
        return;
    }
    if (!(M(EntStatus) & 0x04)) { /* up */
        if (probe_tile(g, 0x09, 0x01)) {
            return;
        }
    }
    probe_tile(g, 0x08, 0x0A); /* $9E0D */
}
