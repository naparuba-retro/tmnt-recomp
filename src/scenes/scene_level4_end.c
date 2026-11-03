/* Scene 10: end of level 4 (same actors as scene 7). */
#include "game.h"
#include "hal.h"
#include "pause.h"
#include "player.h"
#include "scenes.h"
#include "sound.h"
#include "scenes/actor_groups.h"
#include "scenes/scene_common.h"
#define TODO_AREA "scene" /* prefix of the TODO traces of this file */

/* --- scene 10: end of level 4 (B6 $A33E / $A344, state machine $03C2 of bank 3 $BAF7, same actors as scene 7) --- */
void scene_level4_end_start(Game *g) {
    M(SceneId) |= 0x80; /* $A83B */
    M(SceneStep) = 0;
    sound_play(g, 0x4E); /* B3 $BAE2 */
    M(SceneState) = 0;
}

static void scene_level4_end_step(Game *g) /* B3 $BAF7 */
{
    switch (M(SceneState)) {
        case 0: /* $BB09: information screen */
            M(CameraXLo) = 0;
            M(CameraYLo) = 0xD8;
            M(CameraYHi) = 1;
            set_chr_bank1(g, 0x27);
            M(0x03D4) = 0;
            scene_info_init(g);
            M(SceneState)++;
            M(SceneTimerB) = 0x40;
            M(SceneTimerA) = 0;
            clear_all(g);
            animate0(g);
            break;
        case 1: /* $BB36 */
            if (!M(SceneTimerA)) {
                scene_info_update(g);
                if (M(TextState) == 3) {
                    M(SceneTimerA) = 1;
                }
                break;
            }
            if (M(SceneTimerB)) {
                M(SceneTimerB)--;
            }
            M(SceneState)++;
            break;
        case 2: /* $BB56 */
            set_chr_bank0(g, 0x13);
            set_chr_bank1(g, 0x1E);
            load_level_palette(g, 0x0D);
            M(CameraYLo) = 0;
            M(CameraYHi) = 0;
            load_screen(g, 1, 0x1A);
            M(SceneState)++;
            break;
        case 3: /* $BB76 */
            group_setup(g, 3);
            animate0(g);
            M(SceneState)++;
            M(SceneTimerA) = 1;
            M(SceneIndex) = 0;
            M(SceneFrameCounter) = 0;
            break;
        case 4: /* $BB91 */
            if (!M(SceneTimerA)) {
                M(SceneState)++;
                break;
            }
            if (M(SceneFrameCounter) && M(SceneFrameCounter) < 0x3F) {
                uint8_t a = M(SceneFrameCounter)++;
                load_palette_preset(g, R3(0xBBC4 + (a >> 4)));
            }
            group_frame(g);
            animate0(g);
            break;
        default: /* $BBB8 */
            M(SceneState) = 9;
            clear_all(g);
            animate0(g);
            break;
    }
}

void scene_level4_end_continue(Game *g) /* B6 $A344 */
{
    if (M(SceneState) < 8) {
        scene_level4_end_step(g);
        return;
    }
    M(SceneState) = 0;
    M(0x9F) = 0;
    M(LevelNumber) = 4; /* $A0F3: level 5 */
    M(AreaNumber) = 4;
    M(SideViewFlag) = 0;
    M(DelayTimerLo) = M(DelayTimerHi) = 0;
    M(StageState) = 2;
    clear_entity(g, 0);
    M(SceneStep) = 0;
    M(SceneId) = 0; /* $A846 */
}
