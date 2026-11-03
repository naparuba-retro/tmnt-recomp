/* Type $30: monster that drops a projectile, and its transformation. */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "enemy.h"
#include "enemies/common.h"
#include "enemies/foot_soldier.h"
#include "enemies/purple_ninja.h"
#include "enemies/sewer.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- type $30 (B5 $8C12): monster that advances towards the player, drops a projectile ($0E), then turns into type $32 ($10) --- */
void set_state(Game *g, int x, uint8_t st) {
    M(EntRoutine + x) = st;
    M(EntState + x)++;
} /* B5 $8644 / $86D3 */

void ai_walker_dropper(Game *g, int x) {
    if (!front_tile_set(g, x, 7, 0xFF)) { /* no more ground: it falls */
        M(EntVelY + x) = 3;
        clear_vel_x(g, x);
        set_animation(g, x, 0x0A);
        set_state(g, x, 6);
        return;
    }
    face_player_get_distance(g, x); /* $8C2D */
    if (M(EntHealth + x) < 0x10) {  /* low health */
        clear_vel_x(g, x);
        clear_vel_y(g, x);
        set_state(g, x, 0x10);
        M(EntAttr + x) = 0;
        set_animation(g, x, 3);
        return;
    }
    uint8_t d = M(Tmp0), fc = M(FrameCounter);
    int attack;
    if (d < 0x20) {
        attack = fc >= 0x40;
    } else if (d < 0x38) {
        attack = (fc & 8) != 0;
    } else {
        attack = 0;
    }
    if (attack) { /* $8C6E: it stops and gets ready */
        clear_vel_x(g, x);
        clear_vel_y(g, x);
        set_state(g, x, 0x0E);
        set_animation(g, x, 0x0A);
        return;
    }
    if (M(EntAnimId + x) != 9) {
        set_animation(g, x, 9);
        return;
    } /* $8C4F: walks */
    walk_and_stop(g, x, 1, 0x80);
    if (move_blocked(g, x, 0x0A, 0xDC)) {
        clear_vel_x(g, x);
    }
}

void ai_flyer(Game *g, int x) /* B5 $8C8D: flies from one edge of the screen to the other following the player */
{
    M(Tmp0) = (uint8_t)(M(EntX) >= M(EntX + x) ? M(EntX) - M(EntX + x) : M(EntX + x) - M(EntX)); /* $8361 */
    if (M(EntY) >= M(EntY + x)) {
        M(Tmp0 + 1) = (uint8_t)(M(EntY) - M(EntY + x));
        M(Tmp0 + 2) = 0;
    } /* $83B3 */
    else {
        M(Tmp0 + 1) = (uint8_t)(M(EntY + x) - M(EntY));
        M(Tmp0 + 2) = 1;
    }
    if (M(EntFlags + x) & 1) {
        if (M(EntX + x) >= 0x10) {
            return;
        }
    } else if (M(EntX + x) < 0xF0) {
        return;
    }
    M(EntFlags + x) ^= 1; /* $8CA7: U-turn */
    walk_speed(g, x, 5, 0);
    if (M(Tmp0 + 2)) {
        set_vel_y(g, x, 0x78, 0xFF);
        return;
    }
    if (M(Tmp0 + 1) >= 0x28) {
        set_vel_y(g, x, 0x98, 0);
    }
}

void walker_dropper_drop_state(Game *g, int x) /* B5 $8D7F */
{
    if (M(EntAnimTimer + x) >= 2) {
        if (M(EntAttr + x) != 2) {
            M(EntAttr + x)++;
        } else {
            M(EntAttr + x)--;
        }
        return;
    }
    M(EntAttr + x) = 0;
    spawn_child(g, x, 4);
    M(EntRoutine + x) = 3;
    set_animation(g, x, 0x0D);
}

void walker_dropper_transform_state(Game *g, int x) /* B5 $8DA9: becomes type $32 */
{
    if (M(EntAnimFrame + x) != 0xFF) {
        return;
    }
    walk_speed(g, x, 5, 0);
    M(EntY + x) = (uint8_t)(M(EntY + x) - 0x18);
    M(EntType + x) = 0x32;
    M(EntHealth + x) = 0x0C;
    M(EntFlags + x) &= 0xC3;
    M(EntAttr + x) = 0;
    M(EntState + x) = 0;
    M(EntRoutine + x) = 0;
    set_animation(g, x, 0x0C);
}
