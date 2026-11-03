/* Scenes after a boss: common steps and the per-frame runner. */
#include "game.h"
#include "anim.h"
#include "hal.h"
#include "hud.h"
#include "pause.h"
#include "player.h"
#include "sound.h"
#include "stage.h"
#include "stage_play.h"
#include "stage_play/camera_split.h"
#include "scenes/scene_dispatch.h"
#include "scenes/scene_runner.h"
#include "stage_play/spawn_list.h"
#define TODO_AREA "stage_play" /* prefix of the TODO traces of this file */

/* --- scenes after a boss ($B5 non-zero): B7 $CCB9, B6 $9F47 --- */
void scene_decor(Game *g) /* B6 $ABF9: scenery of the scene (top view only), entity 15 = health icon */
{
    if (M(SideViewFlag)) {
        return;
    }
    if (M(AreaNumber) == 2) {
        TRACE("f=%u TODO scene scenery zone 2 ($AC04)\n", g->frame);
        return;
    }
    uint8_t h = M(TurtleHealth0 + M(CurrentTurtle));
    M(EntVar0 + 15) = (uint8_t)(h >> 4);
    if (h & 0x0F) {
        if (h & 0x08) {
            M(EntVar0 + 15)++;
        } else {
            M(EntVar0 + 15) |= 0x80;
        }
    }
    M(EntFlags + 15) = 0xC2;
    M(EntAttr + 15) = 0x03;
    M(EntY + 15) = 0xD3;
    M(EntX + 15) = 0x68;
}

void scene_end(Game *g) {
    M(SceneStep) = 0;
    M(SceneId) = 0;
} /* B6 $A846 */
void scene_continue(Game *g) {
    M(SceneId) |= 0x80;
    M(SceneStep) = 0;
} /* B6 $A83B */

void scene_master_on_rope_continue(Game *g) /* B6 $A257 */
{
    switch (M(SceneStep)) {
        case 0: /* $A266: the master comes down */
            move_entity_y(g, 0x0F);
            move_entity_x(g, 0x0F);
            if (M(EntY + 15) < 0xA4) {
                return;
            }
            M(SceneStep)++;
            M(DelayTimerLo) = 0x38;
            break;
        case 1: /* $A27A */
            if (--M(DelayTimerLo) != 0) {
                return;
            }
            M(SceneStep)++;
            M(EntX + 14) = 0x10;
            M(EntY + 14) = 0x88;
            M(EntSprite + 14) = 0x81;
            M(EntFlags + 14) = 0xC2;
            M(EntVelX + 14) = 2;
            M(EntVelY + 14) = 0xFF;
            M(EntSprite + 15) = 0x83;
            M(DelayTimerLo) = 0x80;
            sound_play(g, 0x3F);
            set_chr_bank0(g, 5);
            break;
        case 2: { /* $A2B2: the scenery turtle jumps and salutes */
            if (--M(DelayTimerLo) == 0) {
                M(SceneStep)++;
                return;
            }
            move_entity_y(g, 0x0E);
            move_entity_x(g, 0x0E);
            M(EntSprite + 14) = 0x81;
            if (M(FrameCounter) & 4) {
                M(EntSprite + 14)++;
            }
            switch (M(DelayTimerLo)) {
                case 0x40:
                    M(EntVelX + 14) = 0;
                    M(EntVelY + 14) = 0;
                    break;
                case 0x38:
                    M(EntFlags + 14) = 0xC3;
                    break;
                case 0x30:
                    M(EntVelX + 14) = 0xFE;
                    M(EntVelY + 14) = 0xFF;
                    break;
                case 0x08:
                    M(EntFlags + 14) = 0;
                    break;
                default:
                    break;
            }
            if (M(DelayTimerLo) == 0x6E) {
                M(EntSprite) = 0x21;
                M(EntFlags) |= 1;
            }
            break;
        }
        case 3: /* $A315: message screen */
            M(SceneStep)++;
            M(SceneState) = 1;
            M(CameraYLo) = 0xD8;
            M(CameraYHi) = 0x01;
            scene_info_init(g);
            sound_play(g, 0x52);
            for (int x = 0; x < 16; x++) {
                clear_entity(g, x);
            }
            break;
        case 4: /* $A32A: the message is typed, then next level */
            scene_info_update(g);
            if (M(TextState) == 3) {
                M(SceneState) = 0;
                M(LevelNumber) = 3;
                M(AreaNumber) = 3;
                M(SideViewFlag) = 0;
                M(DelayTimerLo) = M(DelayTimerHi) = M(0x9F) = 0;
                M(StageState) = 2;
                clear_entity(g, 0);
                scene_end(g);
            }
            break;
        default:
            break;
    }
}

void scene_run_frame(Game *g) /* B7 $D08F: player, enemies, animations, weapon */
{
    player_update(g);
    update_enemies(g);
    animate_entities_from(g, 0);
    weapon_update(g);
}

#define S14_ENTRY_Y 0 /* 6502 Y register at the entry of $BE77 in states 1 and 2 (left over by the caller) : always 0 in the captured runs */

void scene_frame(Game *g) /* B7 $CCB9 */
{
    if (M(FrameCounter) & 1) {
        set_prg_saved(g, 6);
        scene_update(g);
        status_bar_split(g);
        set_scroll_from_camera(g);
        return;
    }
    uint8_t id = M(SceneId) & 0x7F;
    if (id == 0x0D || id == 0x10) {
        animate_entities_from(g, 0); /* $CFEA */
    } else if (id == 6) {
        animate_entities_from(g, 5); /* $CFF2 */
    }
    status_bar_split(g);
    g->prg_bank = 0;
    hud_update(g);
    g->prg_bank = M(CurPrgBank);
    set_scroll_from_camera(g);
}
