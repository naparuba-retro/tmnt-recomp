/* Scene 14: after the boss of the vertical zone, the turtle walks to the exit. */
#include "game.h"
#include "hal.h"
#include "nmi.h"
#include "pause.h"
#include "player.h"
#include "sound.h"
#include "stage_play.h"
#include "scenes/scene_runner.h"
#include "scenes/scene_walk_to_exit.h"
#define TODO_AREA "stage_play" /* prefix of the TODO traces of this file */
#define S14_ENTRY_Y 0

/* --- scene 14: after the boss of the vertical zone, the turtle walks to the exit (B6 $D2CD -> B2 $BD59; continuation B2 $BE20), then level 6 begins --- */

static uint8_t walk_exit_target(Game *g, uint8_t y) /* B2 $BEAE: 0 = arrived, 1 = target on the right, 2 = target on the left */
{
    M(Tmp2) = R2(0xBEA2 + y);
    M(Tmp3) = R2(0xBEA3 + y);
    M(Tmp4) = R2(0xBEA4 + y);
    unsigned lo = M(CameraXLo) + M(EntX);
    M(Tmp0) = (uint8_t)lo;
    M(Tmp1) = (uint8_t)(M(CameraXHi) + (lo > 0xFF));
    uint8_t r = 1;
    unsigned d = (unsigned)(M(Tmp3) << 8 | M(Tmp2)) - (unsigned)(M(Tmp1) << 8 | M(Tmp0));
    M(Tmp2) = (uint8_t)d;
    M(Tmp3) = (uint8_t)(d >> 8);
    if (M(Tmp3) & 0x80) {
        unsigned n = (0x10000 - (M(Tmp3) << 8 | M(Tmp2))) & 0xFFFF;
        M(Tmp2) = (uint8_t)n;
        M(Tmp3) = (uint8_t)(n >> 8);
        r = 2;
    }
    if (!M(Tmp3) && M(Tmp2) < M(Tmp4)) {
        r = 0;
    }
    return r;
}

void walk_exit_simulated_pad(Game *g, uint8_t y) /* B2 $BE77: simulated keys ($30 / $32), then one game frame */
{
    if (M(SceneIndex) != 2) {
        M(Tmp8) = y;
        if (!walk_exit_target(g, y)) {
            M(JoyPressed1) = 0x80;
            M(JoyHeld1) |= 0x80;
        }
        uint8_t r = walk_exit_target(g, (uint8_t)(M(Tmp8) + 3));
        M(JoyHeld1) |= r;
    }
    M(ScrollDirX) = 0;
    scene_run_frame(g);
}

static uint8_t walk_exit_camera_follow(Game *g) /* B2 $BEF6: the camera follows the turtle; returns 1 if it moves */
{
    uint8_t r = 0, hi = 0;
    if (!M(ScrollDirX) || !(M(EntVelX) | M(EntVelXFrac))) {
        return 0;
    }
    r = 1;
    if (M(EntVelX) & 0x80) {
        hi--;
    }
    unsigned f = M(SceneDelay) + M(EntVelXFrac);
    M(SceneDelay) = (uint8_t)f;
    unsigned s = M(CameraXLo) + M(EntVelX) + (f > 0xFF);
    M(CameraXLo) = (uint8_t)s;
    M(CameraXHi) = (uint8_t)(M(CameraXHi) + hi + (s > 0xFF));
    return r;
}

static void walk_exit_palette(Game *g) /* B2 $BE3D */
{
    if (!M(SceneCounter)) {
        return;
    }
    uint8_t a = M(SceneCounter);
    M(SceneCounter) = (uint8_t)(a - 1);
    uint8_t y = 0x24;
    if (a != 0x20) {
        y = 0x25;
        if (a != 1) {
            return;
        }
    }
    load_palette_preset(g, y);
}

void scene_walk_to_exit_start(Game *g) /* B2 $BD59 */
{
    M(SceneCounter) = 0x40;
    M(SceneId) |= 0x80;
    M(SceneIndex) = 0;
    M(SceneSubStep) = 0;
}

void scene_walk_to_exit_continue(Game *g) /* B2 $BE20 */
{
    switch (M(SceneSubStep)) { /* $BDFD */
        case 0:
            if (--M(SceneSubTimer) == 0) {
                M(SceneSubStep)++;
                sound_play(g, 0x7F);
            }
            break;
        case 1:
            if (M(JingleDone)) {
                M(SceneIndex) = 4;
                M(SceneSubStep)++;
            }
            break;
        default:
            break;
    }
    M(JoyHeld1) = 0;
    M(JoyPressed1) = 0;
    switch (M(SceneIndex)) {
        case 0: /* $BE54 */
            for (int e = 5; e < 16; e++) {
                clear_entity(g, e);
            }
            walk_exit_palette(g);
            walk_exit_simulated_pad(g, 0);
            if (!walk_exit_camera_follow(g) && !M(JoyHeld1) && !(M(EntFlags) & 0x10)) {
                M(SceneListIndex) = 0x24;
                M(SceneIndex)++;
            }
            break;
        case 1: /* $BF26 */
            walk_exit_palette(g);
            M(JoyPressed1) = M(JoyHeld1) = 0x81;
            M(EntAttr) = 0x20;
            walk_exit_simulated_pad(g, S14_ENTRY_Y);
            walk_exit_camera_follow(g);
            M(SceneListIndex) = 0x24;
            M(SceneIndex)++;
            break;
        case 2: /* $BF43 */
            walk_exit_palette(g);
            M(JoyHeld1) = M(JoyPressed1) = 0x81;
            walk_exit_simulated_pad(g, S14_ENTRY_Y);
            walk_exit_camera_follow(g);
            if (--M(SceneListIndex)) {
                break;
            }
            M(SceneListIndex) = 0x80;
            M(SceneIndex)++;
            clear_entity(g, 0);
            break;
        case 4: /* $BF64: information screen */
            M(SceneIndex)++;
            blank_ppu(g);
            g->ppu.mirror_v = 0; /* B7 $CE73: MMC1 control $1F */
            M(CameraXLo) = 0;
            M(CameraXHi) = 0;
            M(CameraYHi) = 0;
            M(CameraYLo) = 0xE8;
            M(PpuCtrlShadow) |= 2;
            for (int e = 0; e < 16; e++) {
                clear_entity(g, e);
            }
            sound_init(g);
            scene_info_init(g);
            M(SceneState) = 1;
            sound_play(g, 0x52);
            break;
        case 5: /* $BF94 */
            scene_info_update(g);
            if (M(TextState) == 3) {
                M(MusicFadeSteps) = 7;
                M(SceneIndex)++;
                M(SceneFrameCounter) = 0x40;
                M(0x9F) = 0;
            }
            break;
        case 6: /* $BFAF: level 6 */
            if (--M(SceneFrameCounter)) {
                break;
            }
            M(LevelNumber) = 5;
            M(AreaNumber) = 0;
            M(SceneIndex) = 0;
            M(SceneId) = 0;
            M(SideViewFlag) = 1;
            M(StageState) = 2;
            for (int i = 0; i < 8; i++) {
                M(SceneState + i) = 0; /* B7 $C6E4 */
            }
            break;
        default:
            break;
    }
}
