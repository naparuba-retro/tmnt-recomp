/* End of the game scene. */
#include "game.h"
#include "player.h"
#include "sound.h"
#include "stage_play.h"
#include "scenes/scene_game_end.h"
#include "scenes/scene_runner.h"
#include "scenes/scene_walk_to_exit.h"
#define TODO_AREA "stage_play" /* prefix of the TODO traces of this file */

void scene_game_end_continue(Game *g) /* B2 $BD6C: after the last boss, the end of the game */
{
    switch (M(SceneSubStep)) { /* $BDC1 */
        case 0:
            M(SceneSubTimer) = 0x12;
            M(SceneSubStep)++;
            break;
        case 1:
            if (--M(SceneSubTimer) == 0) {
                sound_play(g, 0x7F);
                M(SceneSubStep)++;
            }
            break;
        case 2:
            if (M(JingleDone)) {
                sound_play(g, 0x87);
                M(SceneSubStep)++;
                M(SceneSubTimer) = 0x0E;
            }
            break;
        case 3:
            if (--M(SceneSubTimer) == 0) {
                M(SceneSubTimer)++;
                M(GameMode)++; /* B7 $C6B9: next game mode */
                M(GameSubMode) = 0;
            }
            break;
        default:
            break;
    }
    M(JoyHeld1) = 0;
    M(JoyPressed1) = 0;
    switch (M(SceneIndex)) {
        case 0: /* $BD78 */
            walk_exit_simulated_pad(g, 6);
            if (!(M(EntFlags) & 0x10) && !M(JoyHeld1)) {
                M(SceneIndex)++;
            }
            break;
        case 1: /* $BD8C */
            M(JoyHeld1) = M(JoyPressed1) = 0x82;
            scene_run_frame(g);
            if (!(M(EntFlags) & 0x10)) {
                M(SceneIndex)++;
            }
            break;
        case 2: /* $BDA6 */
            M(JoyHeld1) = M(JoyPressed1) = 0x80;
            scene_run_frame(g);
            if (++M(SceneListIndex) >= 0x0C) {
                M(SceneIndex)++;
                clear_entity(g, 0);
            }
            break;
        default:
            break;
    }
}
