/* Scene 16: the turtle is killed outright. */
#include "game.h"
#include "anim.h"
#include "hal.h"
#include "player.h"
#include "stage_play.h"
#include "scenes/scene_runner.h"
#include "scenes/scene_turtle_killed.h"
#define TODO_AREA "stage_play" /* prefix of the TODO traces of this file */

/* --- scene 16: the turtle is killed outright (B6 $9F99 / $9FB2) --- */
void scene_turtle_killed_start(Game *g) {
    clear_entity(g, 1);
    M(EntType) = 0x80;
    M(EntAttr) = 0;
    set_animation(g, 0, 0x24);
    scene_continue(g);
}

void scene_turtle_killed_continue(Game *g) {
    switch (M(SceneStep)) {
        case 0: /* $9FBB */
            if (M(EntAnimFrame) == 0xFF) {
                set_animation(g, 0, 0x25);
                M(DelayTimerLo) = 0x30;
                M(SceneStep)++;
            }
            return;
        default: /* $9FCF */
            if (M(EntY) < 0x9C) {
                M(EntY) += 3;
                return;
            }
            if (--M(DelayTimerLo) == 0) { /* $A008 */
                M(TurtleHealth0 + M(CurrentTurtle)) = 0;
                M(StageState) = 7;
                load_screen(g, 1, 0); /* B7 $C97F */
                for (int y = 0x3F; y >= 0; y--) {
                    M(AttrBufferA + y) = 0;
                    M(AttrBufferB + y) = 0;
                }
                M(SceneStep) = 0;
                M(SceneId) = 0; /* $A846 */
                return;
            }
            {
                int neg = (M(EntX) & 0x80) != 0;
                int left = M(DelayTimerLo) < 0x18 ? !neg : neg; /* $9FEC / $9FFA */
                if (left) {
                    M(EntX)--;
                    M(EntFlags) = 0xC3;
                } else {
                    M(EntX)++;
                    M(EntFlags) = 0xC2;
                }
            }
            return;
    }
}
