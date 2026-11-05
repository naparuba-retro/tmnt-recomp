/* Side view: walking, jumping, falling. */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "sound.h"
#include "player/pose.h"
#include "player/side_move.h"
#include "player/subweapon.h"
#include "player/tiles.h"
#define TODO_AREA "player" /* prefix of the TODO traces of this file */

/* --- walking, jumping and falling in side view (B4 $9D49-$A1B0) --- */
void walk_velocity(Game *g, uint8_t y) /* B4 $A764: y = 1 (right) or 2 (left) */
{
    if (M(EntStatus) & 0x10) {
        y = (uint8_t)(y + 2);
    }
    uint8_t i = (uint8_t)(y << 1);
    M(EntVelX) = R4(0xA77D + i);
    M(EntVelXFrac) = R4(0xA77E + i);
}

void player_gravity(Game *g) /* B4 $A5F0 */
{
    unsigned s = M(EntVelYFrac) + 0x54;
    M(EntVelYFrac) = (uint8_t)s;
    uint8_t a = (uint8_t)(M(EntVelY) + (s > 0xFF));
    if (a & 0x80) {
        if (a < 0xF9) {
            a = 0xF9;
        }
    } else if (a >= 9) {
        a = 9;
    }
    M(EntVelY) = a;
}

static int attack_allowed(Game *g) /* B4 $9F22: carry = can attack */
{
    if (M(EntStatus) & 0x10) {
        return 0;
    }
    M(EntStatus) |= 0x04;
    return 1;
}

static int air_attack(Game *g) /* B4 $9EEE: attack during a jump; gives back the X register on return (1 after a melee attack: the weapon, measured with BizHawk ; the turtle number after a throw) */
{
    M(0xD4) = 0;
    if (M(SubWeaponSelected) && !M(SubWeaponType0 + M(CurrentTurtle))) {
        M(SubWeaponSelected) = 0; /* $9F0A */
    }
    if (M(SubWeaponSelected)) {
        subweapon_sound(g);
        set_animation(g, 0, 0x86);
        int thrown = free_projectile_slot(g) >= 0;
        subweapon_throw(g);
        return thrown ? M(CurrentTurtle) : 0; /* $E095: ldx $67 after a throw ; the shuriken of slot X then loses its speed at $A004 (seen with turtle 3 on the last boss) */
    }
    attack_sound(g);
    set_animation(g, 0, R4(0x9F50 + (uint8_t)((M(JoyHeld1) & 0x0C) + M(CurrentTurtle))));
    weapon_effect_start(g);
    return 1;
}

static int air_attack_check(Game *g) /* $9FB5 / $A0A8; gives back the X register on return (0: player, 1: the weapon) */
{
    if (!(M(EntStatus) & 0x04) && (M(JoyPressed1) & 0x40) && attack_allowed(g)) {
        return air_attack(g);
    }
    return 0;
}

static int probes_any(Game *g, const uint8_t *ys, int n, uint8_t a) /* carry of a chain of PlayerProbeTile */
{
    for (int i = 0; i < n; i++) {
        if (probe_tile(g, ys[i], a)) {
            return 1;
        }
    }
    return 0;
}

static int side_blocked_right(Game *g) {
    static const uint8_t y[3] = {0, 0x0A, 0x17};
    return probes_any(g, y, 3, 5);
} /* $A0EA */
static int side_blocked_left(Game *g) {
    static const uint8_t y[3] = {1, 0x0B, 0x18};
    return probes_any(g, y, 3, 5);
} /* $A106 */

static void landing_sound_and_state(Game *g) /* $A18A */
{
    clear_vel_x(g, 0);
    clear_vel_y(g, 0);
    M(EntState) = ((M(EntStatus) & 0x10) && (M(EntStatus) & 0x04)) ? 5 : 0;
    M(EntFlags) &= 0xEF;
    sound_play(g, 8);
}

static int landing_check(Game *g) /* $A122: carry = tile found */
{
    if (!probe_tile(g, 7, 2)) {
        return 0;
    }
    uint8_t t = M(Tmp0);
    if (t == 0x0F) {
        clear_vel_y(g, 0);
        return 1;
    }
    if (t == 9 && tile_near_player(g, 0x0D) == 9) {
        return 1;
    }
    for (unsigned iterations = 0;; iterations++) { /* $A140: goes up to the ground */
        LOOP_GUARD(iterations)
        M(EntY)--;
        if (!probe_tile(g, 6, 2)) {
            break;
        }
        t = M(Tmp0);
        if (t == 9 || t == 0x0A) {
            uint8_t t2 = tile_near_player(g, 0x0D);
            if (t2 == 9 || t2 == 0x0A) {
                break;
            }
        }
    }
    if (!(M(EntVelX) & 0x80)) { /* $A163 */
        while (tile_near_player(g, 0x14) == 4) {
            M(EntX) -= 2;
            if (!M(EntX)) {
                break;
            }
        }
    } else {
        while (tile_near_player(g, 0x15) == 4) {
            M(EntX) += 2;
            if (!M(EntX)) {
                break;
            }
        }
    }
    landing_sound_and_state(g);
    return 1;
}

static int side_horizontal_and_landing(Game *g) /* $A0CE: side collisions then ground; carry */
{
    int c;
    if (!(M(EntVelX) & 0x80)) {
        c = side_blocked_right(g);
    } else {
        c = side_blocked_left(g);
    }
    if (c) {
        clear_vel_x(g, 0);
    }
    if (M(EntVelY) & 0x80) {
        return c;
    }
    return landing_check(g);
}

static void player_fall_tail(Game *g) /* $A0BD */
{
    if (M(EntVelY) & 0x80) {
        if (probe_tile(g, 0x0C, 5)) {
            M(EntRoutine) = 8;
            clear_vel_y(g, 0);
        } /* $A1B8: ceiling */
    }
    side_horizontal_and_landing(g);
}

void player_airborne(Game *g) /* B4 $9FB0: states 7 and 8 (jump) */
{
    player_gravity(g);
    int xr = air_attack_check(g); /* X stays at 1 after the attack: the jsr $EFB2 of $A004 then clears the weapon's velocity, not the player's */
    uint8_t h = M(JoyHeld1);
    if (h & 3) {
        int c;
        if (!(h & 1)) {
            walk_velocity(g, 2);
            M(EntFlags) |= 1;
            c = probe_tile(g, 0x0B, 5);
        } else {
            walk_velocity(g, 1);
            M(EntFlags) &= 0xFE;
            c = probe_tile(g, 0x0A, 5);
        }
        if (c) {
            clear_vel_x(g, xr);
        }
    }
    M(EntActionFlags) = (uint8_t)((h & 0x80) & M(EntActionFlags));
    uint8_t y = M(EntRoutine);
    if (y < 8) {
        M(EntRoutine)++;
        y++;
        if (y == 4 && !M(EntActionFlags)) {
            M(EntVelY) = 0xFD;
            M(EntVelYFrac) = 0x90;
        } else if (y == 8 && M(EntActionFlags)) {
            if (!(M(EntStatus) & 0x04)) {
                M(EntState) = 8;
            }
            M(EntVelY) = 0xFA;
            M(EntVelYFrac) = 0;
        }
    }
    if (M(EntY) >= 0x0C) {
        player_fall_tail(g);
        return;
    }
    if (!(M(CameraYLo) | M(CameraYHi))) { /* $A05C: exits through the top */
        for (int i = 0; R4(0xA087 + i) != 0xFF; i += 2) {
            if (R4(0xA087 + i) == M(LevelNumber) && R4(0xA088 + i) == M(AreaNumber)) {
                M(EntRoutine) = 8;
                M(EntState) = 9;
                M(EntFlags) &= 0xFD;
                return;
            }
        }
    }
    M(EntRoutine) = 8; /* $A090 */
    M(EntVelY) = 0;
    M(EntVelYFrac) = 0;
    M(EntY) = 0x0C;
}

void player_side_hurt(Game *g, int x) /* B4 $A66B: the player is hit in side view */
{
    if (M(EntState) == 0x0F) {
        clear_vel_x(g, x);
        clear_vel_y(g, x);
        return;
    }
    if (M(EntStatus) & 0x80) {
        M(EntVelX) = 0xFE;
        M(EntVelXFrac) = 0x80;
    } else {
        M(EntVelX) = 1;
        M(EntVelXFrac) = 0x50;
    }
    M(EntVelY) = 0xFE;
    M(EntVelYFrac) = 0xFE;
    M(EntFlags) |= 0x10;
    M(EntState) = 0x0A;
    player_fall_tail(g);
}

void player_falling(Game *g) /* B4 $A0A3: state 10 (fall) */
{
    player_gravity(g);
    air_attack_check(g);
    player_fall_tail(g);
}

static void player_jump_start(Game *g) /* B4 $9E48: jump from the ground */
{
    int c = (M(EntFlags) & 1) ? side_blocked_left(g) : side_blocked_right(g); /* $A0DA / $A0D3 */
    if (c) {
        clear_vel_x(g, 0);
    }
    if (!(M(EntVelY) & 0x80)) {
        c = landing_check(g); /* $A0E4 */
    }
    if (c) {
        clear_vel_x(g, 0); /* $9E53 */
    }
    M(EntFlags) |= 0x10;
    M(EntVelY) = 0xFA;
    M(EntVelYFrac) = 0;
    M(EntRoutine) = 0;
    M(EntActionFlags) = 0x80;
    M(EntState) = 7;
    sound_play(g, 7);
}

static void player_walk(Game *g) /* B4 $9D49: walking left / right */
{
    if ((M(EntStatus) & 0x04) && M(0xD4)) {
        clear_vel_x(g, 0);
        return;
    }
    M(EntState) = 1;
    int right = !(M(JoyHeld1) & 2);
    int c;
    walk_velocity(g, right ? 1 : 2);
    if (right) {
        M(EntFlags) &= 0xFE;
    } else {
        M(EntFlags) |= 1;
    }
    c = probe_tile(g, right ? 0x12 : 0x13, 3);
    if (!c) {
        c = probe_tile(g, right ? 4 : 5, 3);
    }
    if (c) {
        if (M(Tmp0) == 0x0A) { /* $9D91 / $9DDB */
            if (!probe_tile(g, right ? 3 : 2, 0)) {
                return;
            }
            if (M(Tmp0) == 0x0A) {
                M(EntState) = 0x11;
            }
            return;
        }
        clear_vel_x(g, 0);
    }
    if (!probe_tile(g, 2, 8)) {
        probe_tile(g, 3, 0); /* $9DC6 */
    }
    clear_vel_y(g, 0);
}

void player_on_ground(Game *g) /* B4 $9D0C, states 0-3 */
{
    if (is_special_area(g)) {
        M(EntState) = 0x0F;
        return;
    }
    if (M(ZoneChangeDelay) || !M(JoyHeld1)) {
        player_idle(g);
        return;
    }
    if (M(JoyPressed1) & 0x80) {
        player_jump_start(g);
        return;
    }
    if (M(JoyPressed1) & 0x40) {
        player_attack(g);
    }
    if (M(JoyHeld1) & 0x03) {
        player_walk(g);
        return;
    }
    if (M(JoyHeld1) & 0x0C) {
        player_walk_up_down(g);
    } else {
        player_idle(g);
    }
}
