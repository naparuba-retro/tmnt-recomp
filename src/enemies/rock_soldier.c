/* Types $49 / $4A: rock_soldier and its crescent. */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "enemy.h"
#include "enemies/common.h"
#include "enemies/foot_soldier.h"
#include "enemies/purple_ninja.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* ------------------------------------------------------------ types $49 and $4A: B5 $9249-$93D3 --- */
void ai_rock_soldier(Game *g, int x) /* B5 $9249 */
{
    face_player_get_distance(g, x);
    uint8_t d = M(Tmp0);
    if (d < 0x20 || d >= 0x50) {
        return;
    }
    int facing_left = (M(EntFlags + x) & 1) != 0;
    if (M(EntX) >= M(EntX + x)) {
        if (facing_left) {
            return;
        }
    } /* $9274 */
    else if (!facing_left) {
        return;
    }
    set_animation(g, x, 0x51);
    M(EntAttr + x) = 1;
    M(EntRoutine + x) = 0x17;
    M(EntState + x)++;
}

void ai_rock_soldier_crescent(Game *g, int x) /* B5 $927C : projectile */
{
    M(EntRoutine + x) = 0;
    M(EntState + x)++;
    walk_speed(g, x, 3, 0x40);
}

int landing_step(Game *g, int x) /* B5 $8CE7: gravity then landing; carry = set on the ground */
{
    add_vel_y(g, x, 0x24);
    if (!front_tile_set(g, x, 0x0C, 0x04)) {
        return 0;
    }
    return land_check(g, x, 0, 0xFC);
}

void rock_soldier_walk_state(Game *g, int x) /* $9359: state $18 */
{
    if (!front_tile_set(g, x, 0x0A, 0xFF)) { /* no more ground: it falls */
        M(EntVelY + x) = 3;
        clear_vel_x(g, x);
        M(EntRoutine + x) = 0x19;
        return;
    }
    face_player_get_distance(g, x);
    if (M(Tmp0) < 0x10) { /* on contact: blow */
        clear_vel_x(g, x);
        clear_vel_y(g, x);
        set_animation(g, x, 0x52);
        M(EntRoutine + x) = 0x1B;
        return;
    }
    M(EntVar0 + x)++;
    if ((M(EntVar0 + x) & 0x1F) == 0) { /* $93AC: throws a projectile */
        clear_vel_x(g, x);
        clear_vel_y(g, x);
        spawn_child(g, x, 0x16);
        set_animation(g, x, 0x54);
        M(EntRoutine + x) = 0x1A;
        return;
    }
    if (M(EntAnimId + x) != 0x53) {
        set_animation(g, x, 0x53);
    }
    walk_speed(g, x, 1, 0);
    if (move_blocked(g, x, 0x0A, 0xF0)) {
        clear_vel_x(g, x);
    }
    if (move_blocked(g, x, 0x16, 0xDC)) {
        clear_vel_x(g, x);
    }
}
