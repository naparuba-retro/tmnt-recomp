/* Top-view player actions (B4 $A9D9). */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "sound.h"
#include "stage_play.h"
#include "stage_play/player_actions.h"
#include "stage_play/spawn_top_view.h"
#define TODO_AREA "stage_play" /* prefix of the TODO traces of this file */

/* --- top-view player (B4 $A9D9 PlayerActions) --- */
void player_actions(Game *g) {
    if (M(SideViewFlag)) {
        return;
    }
    if (!(M(EntFlags) & 0x80)) {
        return;
    }
    if ((M(JoyPressed1) & 0x20) && M(InVanFlag)) { /* $A9E9: gets out of the van, which stays there */
        M(InVanFlag) = 0;
        M(EntState) = 0;
        M(0x03E5) = 1;
        clear_entity(g, 5);
        M(EntX + 5) = M(EntX);
        M(EntY + 5) = M(EntY);
        M(EntAnimId + 5) = M(EntAnimId);
        level3_van_spawn(g); /* $AA11 : B4 $9712 */
        sound_play(g, 0x32);
    }
    switch (M(EntState)) {
        case 0:
            if (M(JoyPressed1) & 0x40) { /* $AA25: attack */
                M(EntState) = 1;
                if (!(M(EntStatus) & 0x10)) {
                    M(EntStatus) |= 0x04; /* $9F22 */
                }
                attack_sound(g);
                set_animation(g, 0, R4(0xAA8E + M(EntRoutine)));
                overhead_weapon_start(g);
                return;
            }
            if (M(JoyHeld1) & 0x0F) {
                player_set_pose(g, R4(0xAA8A + M(EntRoutine)));
                return;
            } /* $AA55 */
            set_animation(g, 0, R4(0xAA86 + M(EntRoutine))); /* $AA44: still pose according to the direction */
            break;
        case 1: /* $AA5B: the blow lasts until the end of the animation */
            if (M(EntAnimFrame) != 0xFF) {
                break;
            }
            clear_entity(g, 1);
            player_attack_end(g); /* $9F35 */
            M(EntState) = 0;
            break;
        case 2: /* $AA6F */
            if (M(JoyHeld1) & 0x0F) {
                player_set_pose(g, R4(0xAA96 + M(EntRoutine)));
            } else {
                set_animation(g, 0, R4(0xAA92 + M(EntRoutine)));
            }
            break;
        default:
            TODO("top-view player state");
            break;
    }
}
