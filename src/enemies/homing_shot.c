/* Type $33: projectile that realigns on the player. */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "enemy.h"
#include "enemies/boss_brawler.h"
#include "enemies/foot_soldier.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- type $33 (B5 $B442, state $11 at $B45F): projectile of boss $73, realigns on the player every 8 frames --- */
void ai_homing_shot(Game *g, int x) {
    M(EntRoutine + x) = 0x11;
    M(EntState + x)++;
    if (!(M(EntFlags + x) & 1)) {
        set_vel_x(g, x, 0, 3);
    } else {
        set_vel_x(g, x, 0, 0xFD);
    }
}

static void homing_velocity(Game *g, int x) /* B5 $B4A9 */
{
    clear_vel_x(g, x);
    clear_vel_y(g, x);
    if (boss_dist_y(g, x) >= 0x10) {
        M(EntVelY + x) = 2;
    }
    if (boss_dist_x(g, x) >= 0x0A) {
        M(EntVelX + x) = 2;
    }
    if (!(M(EntVelX + x) | M(EntVelY + x))) {
        M(EntVelX + x) = 2;
    }
    if (M(EntX) < M(EntX + x)) {
        M(EntVelX + x) = (uint8_t)(~M(EntVelX + x) + 1);
    }
    if (M(EntY) < M(EntY + x)) {
        M(EntVelY + x) = (uint8_t)(~M(EntVelY + x) + 1);
    }
}

void homing_shot_state(Game *g, int x) /* B5 $B45F */
{
    M(EntTimer + x)++;
    M(EntFlags + x) = (M(EntVelX + x) & 0x80) ? 0xC3 : 0xC2;
    uint8_t n = M(EntTimer + x);
    if ((n & 7) || n >= 0x40) {
        return;
    }
    homing_velocity(g, x);
    uint8_t vy = M(EntVelY + x), vx = M(EntVelX + x);
    if (vy == 0) {
        set_animation(g, x, 0x6A);
    } else if (vy & 0x80) {
        set_animation(g, x, vx ? 0x6B : 0x68);
    } else {
        set_animation(g, x, vx ? 0x6C : 0x69);
    }
}
