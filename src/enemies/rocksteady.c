/* Types $92 / $93 / $94: rocksteady, its projectile and the captive. */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "enemy.h"
#include "enemies/chansawiac.h"
#include "enemies/common.h"
#include "enemies/foot_soldier.h"
#include "enemies/purple_ninja.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* ------------------------------------------------------------ rocksteady ($92) and its projectile ($93), type $94: B5 $9288, $92F3, $9983 --- */
void add_vel_x_a(Game *g, int x, uint8_t a) /* B7 $F035: horizontal velocity += a */
{
    unsigned sum = M(EntVelXFrac + x) + a;
    M(EntVelXFrac + x) = (uint8_t)sum;
    M(EntVelX + x) = (uint8_t)(M(EntVelX + x) + (sum > 0xFF));
}

void sub_vel_x_a(Game *g, int x, uint8_t a) /* B7 $F045: horizontal velocity -= a */
{
    unsigned sum = M(EntVelXFrac + x) + (uint8_t)(((uint8_t)~a) + 1);
    M(EntVelXFrac + x) = (uint8_t)sum;
    M(EntVelX + x) = (uint8_t)(M(EntVelX + x) + 0xFF + (sum > 0xFF));
}

void ai_rocksteady(Game *g, int x) /* B5 $9288 */
{
    if (!front_tile_set(g, x, 0x00, 0xFF)) { /* no more ground: it jumps */
        hop_start(g, x, 0x0A);
        clear_vel_x(g, x);
        return;
    }
    face_player_get_distance(g, x);
    if (M(Tmp0) < 0x28) {
        hop_start(g, x, 6);
        return;
    } /* $92AA: the player is close, it leaps */
    if ((M(RandomSeed) & 0x3F) == 0) { /* $92AF: throws its projectile */
        set_animation(g, x, 0x86);
        clear_vel_x(g, x);
        clear_vel_y(g, x);
        int child = spawn_child_slot(g, x, 0x18);
        if (child >= 0) {
            M(EntVar1 + child) = (uint8_t)x;
            walk_speed(g, child, 4, 0);
            M(EntVar0 + child) = 6;
            return;
        }
    }
    walk_speed(g, x, 1, 0xC0); /* $92D4 */
    if (move_blocked(g, x, 0x0A, 0xF0)) {
        clear_vel_x(g, x);
    }
    if (M(EntAnimId + x) != 0x85) {
        set_animation(g, x, 0x85);
    }
    if (move_blocked(g, x, 0x14, 0xDC)) {
        clear_vel_x(g, x);
    }
}

void ai_rocksteady_shot(Game *g, int x) /* B5 $92F3: rocksteady's projectile, comes back towards it */
{
    if (M(EntFlags + x) & 1) {
        add_vel_x_a(g, x, 0x10);
    } else {
        sub_vel_x_a(g, x, 0x10);
    }
    if (M(EntVar0 + x)) {
        M(EntVar0 + x)--;
        return;
    }
    for (int y = 5; y < 16; y++) {
        if (M(EntType + y) != 0x92) {
            continue;
        }
        uint8_t d = (M(EntX + y) >= M(EntX + x)) ? (uint8_t)(M(EntX + y) - M(EntX + x)) : (uint8_t)(M(EntX + x) - M(EntX + y));
        if (d >= 4) {
            continue;
        }
        if (M(EntVar1 + x) != y) {
            continue;
        }
        clear_entity(g, x);
        return;
    }
}

void ai_april(Game *g, int x) /* B5 $9983 */
{
    if (!front_tile_set(g, x, 0x0C, 0xFF)) {
        hop_start(g, x, 0x0B);
        return;
    }
    uint8_t dy = (M(EntY) >= M(EntY + x)) ? (uint8_t)(M(EntY) - M(EntY + x)) : (uint8_t)(M(EntY + x) - M(EntY));
    if (dy < 0x20 && !(M(FrameCounter) & 0x20)) { /* $99A7: fires */
        set_animation(g, x, 0x1F);
        clear_vel_x(g, x);
        clear_vel_y(g, x);
        spawn_child(g, x, 0x19);
        return;
    }
    walk_speed(g, x, 1, 0x60); /* $99B9 */
    if (move_blocked(g, x, 0x0A, 0xF0)) {
        clear_vel_x(g, x);
    }
    if (M(EntAnimId + x) != 0x1E) {
        set_animation(g, x, 0x1E);
    }
    if (move_blocked(g, x, 0x0C, 0xF4)) {
        turn_around(g, x);
    }
}

void ai_april_shot(Game *g, int x) /* B5 $99D9: projectile of type $94 */
{
    walk_speed(g, x, 2, 0x80);
    M(EntRoutine + x) = 0x80;
    M(EntState + x)++;
}
