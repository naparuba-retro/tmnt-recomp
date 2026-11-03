/* Types $42 / $43: jumps towards the player and throws a projectile. */
#include "game.h"
#include "player.h"
#include "sound.h"
#include "enemy.h"
#include "enemies/foot_soldier.h"
#include "enemies/purple_ninja.h"
#include "enemies/walker_dropper.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- types $42 and $43 (B5 $89CC, $8A0F) --- */
void ai_jumping_thrower(Game *g, int x) /* B5 $89CC: jumps towards the player and throws a projectile */
{
    face_player_get_distance(g, x);
    set_vel_y(g, x, 0, 0xFD);
    if (M(Tmp0) >= 0x0A) {
        if (M(EntX + x) >= M(EntX)) {
            set_vel_x(g, x, 0, 0xFD);
        } else {
            set_vel_x(g, x, 0, 3);
        }
    } else {
        clear_vel_x(g, x);
    }
    if (M(Tmp1) >= 0x18) {
        return;
    }
    if (spawn_child_slot(g, x, 0x15) >= 0) {
        sound_play(g, 0x24);
    }
    set_state(g, x, 0);
}

void ai_thrower_recover(Game *g, int x) /* B5 $8A0F: at the end of the animation, becomes type $52 */
{
    if (M(EntAnimFrame + x) != 0xFF) {
        return;
    }
    sound_play(g, 0x28);
    M(EntType + x) = 0x52;
    M(EntAnimId + x) = 0xAE;
    M(EntFlags + x) = 0x82;
    M(EntAnimTimer + x) = 2;
    M(EntState + x) = 0;
    M(EntAnimFrame + x) = 0;
    M(EntHealth + x) = 0;
}
