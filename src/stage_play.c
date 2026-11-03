/* __root__ */
#include "game.h"
#include "anim.h"
#include "hal.h"
#include "hud.h"
#include "player.h"
#include "rope.h"
#include "sound.h"
#include "stage.h"
#include "stage_map.h"
#include "stage_scroll.h"
#include "stage_play.h"
#include "stage_play/camera_split.h"
#include "stage_play/collisions.h"
#include "stage_play/objects.h"
#include "stage_play/player_actions.h"
#include "scenes/scene_runner.h"
#include "stage_play/shared.h"
#include "stage_play/spawn_list.h"
#include "stage_play/spawn_top_view.h"
#define TODO_AREA "stage_play" /* prefix of the TODO traces of this file */

/* --- Stage_Play --- */
static void enter_pause(Game *g) /* B7 $CE0E */
{
    M(PauseState) = 0;
    M(EntSprite) = 0;
    M(PauseFlag) = 1;
    M(PpuCtrlSaved) = M(PpuCtrlShadow);
    M(EntTimer) = M(EntSprite);
}

static void leave_pause(Game *g) /* B7 $CE23: Start in the pause screen -> the level is reloaded */
{
    if (M(SubWeaponSelected) && !M(SubWeaponType0 + M(CurrentTurtle))) {
        M(SubWeaponSelected) = 0;
    }
    M(PauseFlag) = 0;
    M(PauseDrawIndex) = 0;
    M(EntSprite) = M(EntTimer);
    load_screen_and_clear(g);
    set_chr_bank0(g, M(ChrBank0));
    set_chr_bank1(g, M(ChrBank1));
    if (M(StageState) != 7) {
        M(0x0609) = 1;
        M(0x03D5) = 1;
    }
    M(StageState) = (uint8_t)(3 - M(PauseAfterDeath));
    M(PauseAfterDeath) = 0;
    M(PpuCtrlShadow) = M(PpuCtrlSaved);
    M(ScrollXShadow) = M(CameraXLo);
}

static void stage_pause_check(Game *g) /* B7 $CDD6 CheckPause */
{
    if (M(FrameCounter) & 1) {
        M(JoyPressedRawLast) = M(JoyPressedRaw1);
        return;
    }
    if (M(DemoModeBits) | M(0x03ED) | M(0x36) | M(DisplayDelay) | M(VerticalScrollMode) | M(StageIntroTimer) | M(SceneId) | M(0xD2)) {
        return;
    }
    if (M(EntState) == 0x16) {
        return;
    }
    if (M(TurtleHealth0 + M(CurrentTurtle)) == 0) {
        return;
    }
    uint8_t a = M(JoyPressedRaw1) | M(JoyPressedRawLast);
    if (!(a & 0x10)) {
        return;
    }
    if (!M(PauseFlag)) {
        sound_play(g, 0x3E);
        enter_pause(g);
    } else {
        leave_pause(g);
    }
}

void clear_weapon_entities(Game *g) /* B7 $EFC8: entities 1 to 4 */
{
    for (int x = 1; x < 5; x++) {
        M(EntType + x) = 0;
        M(EntAnimId + x) = 0;
        M(EntSprite + x) = 0;
    }
}

void stage_play(Game *g) /* B7 $CC35 */
{
    if (M(StageIntroTimer)) {
        if (--M(StageIntroTimer) == 0) {
            M(0x36) = 0;
        }
        set_scroll_from_camera(g);
        status_bar_split(g);
        return;
    }
    if (M(SceneId)) {
        scene_frame(g);
        return;
    }
    if ((M(FrameCounter) & 0x7F) == 0 && M(ElapsedTimeHi) != 0xFF) { /* play time */
        unsigned s = M(ElapsedTimeLo) + 1;
        M(ElapsedTimeLo) = (uint8_t)s;
        M(ElapsedTimeHi) = (uint8_t)(M(ElapsedTimeHi) + (s > 0xFF));
    }
    if (M(FrameCounter) & 1) { /* odd frames: player and camera */
        set_prg_saved(g, 4);
        player_update(g);
        if (M(VerticalLevelFlag) & 0x80) { /* $CC86: vertical level, the camera is handled elsewhere */
            set_scroll_from_camera(g);
            status_bar_split(g);
            return;
        }
        if (!M(VerticalScrollMode)) { /* CallScrollCameraX : ScrollCameraX ($D7D6) */
            set_prg_saved(g, 1);
            scroll_camera_x(g);
        }
        set_prg_saved(g, 1); /* CallScrollCameraY : ScrollCameraY ($DC8B) */
        scroll_camera_y(g);
        rope_redraw(g); /* $E415 */
        status_bar_split(g);
        update_scroll_edge_flag(g);
        set_prg_saved(g, 4); /* Call_PlayerActions */
        player_actions(g);
        return;
    }
    if (!M(VerticalScrollMode) && !M(0x03FC)) { /* even frames: enemies and collisions */
        set_prg_saved(g, 6);
        process_spawn_list(g);
        set_prg_saved(g, 4);
        spawn_enemies_near_camera(g);
    }
    set_prg_saved(g, 5);
    update_enemies(g);
    set_prg_saved(g, 4);
    update_object_slots(g);
    set_prg_saved(g, 6);
    animate_entities_from(g, 0);
    check_entity_collisions(g);
    status_bar_split(g);
    set_scroll_from_camera(g);
    g->prg_bank = 0; /* L_D06A: B0 $8055 (status bar) */
    hud_update(g);
    g->prg_bank = M(CurPrgBank);
    weapon_update(g);
    M(0x03D5) = 0;
}

void stage_check_pause(Game *g) {
    if (!M(0x03EA) && M(StageState) >= 6) {
        stage_pause_check(g);
    }
}
