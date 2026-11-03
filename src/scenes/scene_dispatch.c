/* Scene start and per-frame dispatch (B6 $9F47) and the scenes 1-9. */
#include "game.h"
#include "anim.h"
#include "enemy.h"
#include "hal.h"
#include "pause.h"
#include "player.h"
#include "rope.h"
#include "scenes.h"
#include "sound.h"
#include "stage.h"
#include "stage_play.h"
#include "scenes/scene_boss74_death.h"
#include "scenes/scene_dispatch.h"
#include "scenes/scene_game_end.h"
#include "scenes/scene_runner.h"
#include "scenes/scene_turtle_killed.h"
#include "scenes/scene_walk_to_exit.h"
#include "stage_play/shared.h"
#define TODO_AREA "stage_play" /* prefix of the TODO traces of this file */

static void scene_begin_run_away(Game *g) /* $A019: rocksteady runs away with april */
{
    for (int x = 1; x < 5; x++) {
        M(EntType + x) = 0;
        M(EntAnimId + x) = 0;
        M(EntSprite + x) = 0;
    }
    M(EntAttr) = 0;
    set_chr_bank0(g, 8);
    M(EntVelX + 9) = 0xFF;
    M(DelayTimerLo) = 0x20;
    M(DelayTimerHi) = 0;
    scene_continue(g);
}

static void scene_begin_join_hostage(Game *g) /* $A070: the player joins the hostage */
{
    clear_weapon_entities(g);
    set_chr_bank0(g, 8);
    clear_weapon_entities(g);
    M(EntFlags) = 0xC3;
    M(EntStatus) = 0;
    M(EntAttr) = 0;
    M(EntState) = 0;
    M(EntY) = 0x8C;
    if (M(EntX) >= M(EntX + 15) && (uint8_t)(M(EntX) - M(EntX + 15)) >= 0x10) {
        M(EntX)--;
        M(EntSprite) = rom_prg(g->rom, 6, (uint16_t)(0xA85C + ((M(FrameCounter) & 0x18) >> 3))); /* $A84D: walks */
        return;
    }
    M(EntSprite) = 9;
    M(EntX) = 0x32;
    M(EntSprite + 15) = 0x91;
    M(EntY + 15)--;
    scene_continue(g);
}

static void scene_begin_info_screen(Game *g) /* $A0C8: information screen, hostage's message */
{
    sound_play(g, 0x52);
    M(CameraYLo) = 0xD8;
    M(CameraYHi) = 0x01;
    scene_info_init(g);
    M(SceneState) = 1;
    for (int x = 0; x < 16; x++) {
        clear_entity(g, x);
    }
    scene_continue(g);
}

static void scene_begin_enter_building(Game *g) /* $A64F: the player enters the building */
{
    scene_decor(g);
    M(EntY) = (uint8_t)(M(EntY) - 2);
    if (M(EntY) >= 0x36) {
        return;
    }
    for (int x = 1; x < 16; x++) {
        clear_entity(g, x);
    }
    M(DelayTimerLo) = 0x38;
    M(EntSprite) = 0;
    M(EntAnimId) = 0;
    M(SceneStep) = 0;
    load_palette_preset(g, 0x2D);
    sound_play(g, 0x29);
    scene_continue(g);
}

static void scene_begin_join_bomb(Game *g) /* $A167: the turtle joins the bomb */
{
    int x = -1;
    do {
        x++;
    } while (M(ObjType + x) < 0x17 || M(ObjType + x) >= 0x1F);
    uint8_t target = (uint8_t)(M(ObjY + x) - 0x19);
    if (M(EntY) == target) {
        M(0xA5) = M(EntX);
        M(EntX) = M(ObjX + x);
        M(DelayTimerLo) = 0x40;
        scene_continue(g);
    } else if (M(EntY) > target) {
        M(EntY)--;
    } else {
        M(EntY)++;
    }
}

static void scene_begin_master_hangs(Game *g) /* $A20E: the boss is beaten, the master hangs from the rope */
{
    load_palette_preset(g, 0x12);
    M(EntAttr) = 0;
    M(SpecialShotCount) = 0;
    clear_entity(g, 0x0D);
    for (int x = 1; x < 5; x++) {
        M(EntType + x) = 0;
        M(EntAnimId + x) = 0;
        M(EntSprite + x) = 0;
    } /* $EFC8 */
    M(EntFlags + 15) = 0xC2;
    M(EntX + 15) = 0xBE;
    M(EntY + 15) = 0x4C;
    M(EntSprite + 15) = 0x98;
    M(EntVelY + 15) = 1;
    M(EntX) = 0xA8;
    M(EntY) = 0x8C;
    M(EntFlags) = 0xC2;
    M(EntSprite) = 0x08;
    scene_continue(g);
}

static void scene_begin_boss72_beaten(Game *g) /* $A35A: boss $72 is beaten, it becomes boss $73 (next phase) in entity 5 */
{
    load_palette_preset(g, 0x11);
    M(EntX + 5) = M(SceneCounter);
    M(EntY + 5) = M(SceneDelay);
    M(WaveState) = 3;
    M(WaveTriggerId) = 3;
    M(WaveDoneMask) = 2;
    M(WaveActive) = 1;
    M(EntType + 5) = 0x73;
    M(EntState + 5) = 0x0E;
    M(EntHealth + 5) = 0x60;
    M(EntFlags + 5) = 0xC2;
    M(EntAnimId + 5) = 0x64;
    M(DelayTimerLo) = 0x30;
    scene_end(g);
}

static void scene_begin_boss74_beaten(Game *g) /* $A6BC: boss $74 is beaten */
{
    boss74_picture(g, 1); /* B7 $D075 */
    M(EntFlags + 5) = 0xC2;
    M(EntX + 5) = 0x80;
    M(EntY + 5) = 0x34;
    M(EntAnimId + 5) = 0x79;
    M(EntAnimTimer + 5) = 0x18;
    sound_init(g);
    sound_play(g, 0x44);
    scene_continue(g);
}

static void scene_begin(Game *g, uint8_t b) /* start of the scene b */
{
    if (b == 1 || b == 2) {
        scene_begin_run_away(g);
    } else if (b == 3) {
        scene_begin_join_hostage(g);
    } else if (b == 4) {
        scene_begin_info_screen(g);
    } else if (b == 0x0D) {
        scene_begin_enter_building(g);
    } else if (b == 5) { /* $A110: the timer is at zero, explosion */
        M(DelayTimerLo) = 0x32;
        scene_continue(g);
    } else if (b == 6) {
        scene_begin_join_bomb(g);
    } else if (b == 0x0A) {
        scene_level4_end_start(g); /* $A33E: end of level 4 */
    } else if (b == 0x0B) {
        scene_rope_start(g); /* $A39B: rope between the rooftops */
    } else if (b == 7) {
        scene_splinter_kidnapped_start(g); /* $A1ED: the last bomb is defused, Splinter is kidnapped */
    } else if (b == 8) {
        scene_begin_master_hangs(g);
    } else if (b == 9) {
        scene_begin_boss72_beaten(g);
    } else if (b == 0x0E || b == 0x0F) {
        scene_walk_to_exit_start(g);
    } else if (b == 0x10) {
        scene_turtle_killed_start(g); /* $9F99 */ /* $D2CD */
    } else if (b == 0x0C) {
        scene_begin_boss74_beaten(g);
    } else {
        TRACE("f=%u TODO scene %d (start)\n", g->frame, b);
    }
}

static void scene_step_run_away(Game *g) /* $A037 */
{
    move_entity_y(g, 9);
    move_entity_x(g, 9);
    if (M(DelayTimerLo) | M(DelayTimerHi)) {
        if (!M(DelayTimerLo)) {
            M(DelayTimerHi)--;
        }
        M(DelayTimerLo)--;
    } /* $C6C5 */
    uint8_t t = M(DelayTimerLo);
    if (!t) {
        M(WaveSpriteSet) = 0;
        set_chr_bank0(g, 0);
        wave_palette(g);
        scene_end(g);
    } else if (t == 8) {
        clear_entity(g, 9);
    } else if (t == 0x10) {
        clear_entity(g, 8);
        M(EntSprite + 9) = 0x80;
        set_chr_bank0(g, 5);
    }
}

static void scene_step_message(Game *g) /* $A0E7: the message is typed, then next level */
{
    scene_info_update(g);
    if (M(TextState) == 3) {
        M(LevelNumber) = 1;
        M(AreaNumber) = 1;
        M(SceneState) = 0;
        M(SideViewFlag) = 0;
        M(DelayTimerLo) = M(DelayTimerHi) = M(0x9F) = 0;
        M(StageState) = 2;
        clear_entity(g, 0);
        scene_end(g);
    }
}

static void scene_step_enter_door(Game *g) /* $A680 */
{
    scene_decor(g);
    switch (M(SceneStep)) {
        case 0:
            if (--M(DelayTimerLo) != 0) {
                break;
            }
            set_animation(g, 0, 0xE5);
            sound_init(g);
            sound_play(g, 0x2C);
            M(SceneStep)++;
            break;
        case 1:
            if (M(EntAnimFrame) == 0xFF) {
                M(DelayTimerLo) = 0x18;
                M(SceneStep)++;
            }
            break;
        case 2:
            if (--M(DelayTimerLo) != 0) {
                break;
            }
            scene_end(g);
            door_transition_entry(g); /* jmp $E6A9 */
            break;
        default:
            break;
    }
}

static void scene_step_explosion(Game *g) /* $A117: explosion, the palette blinks then the game is lost */
{
    if (--M(DelayTimerLo) == 0) {
        for (int t = 0; t < 4; t++) {
            M(TurtleHealth0 + t) = 0;
        }
        M(StageState) = 7;
        scene_end(g);
    } else {
        uint8_t y = (uint8_t)((M(DelayTimerLo) & 4) << 2);
        for (int i = 0; i < 16; i++) {
            M(PaletteBuffer + i) = rom_prg(g->rom, 6, (uint16_t)(0xA147 + y + i));
        }
        fill_palette_backdrop(g, M(PaletteBuffer));
    }
}

static void scene_step_defuse(Game *g) /* $A19F: defusing */
{
    if (--M(DelayTimerLo) == 0) { /* $A1C3 */
        set_animation(g, M(DefusedBombEntity), 0x39);
        load_palette_preset(g, 0x0B);
        M(ScrollDirX) = M(ScrollDirY) = 0;
        clear_vel_x(g, 0);
        clear_vel_y(g, 0);
        sound_play(g, 0x32);
        if (M(BombsDefusedMask) == 0xFF) {
            M(SceneId) = 7;
        } else {
            scene_end(g);
        }
    } else {
        if (M(DelayTimerLo) == 2 && M(BombsDefusedMask) == 0xFF) {
            sound_init(g);
            M(0x03ED) = 1;
        }
        M(EntSprite) = (uint8_t)(0x8A + ((M(DelayTimerLo) & 4) >> 2));
    }
}

static void scene_step(Game *g, uint8_t id) /* one frame of the scene id + 1 */
{
    if (id <= 1) {
        scene_step_run_away(g);
    } else if (id == 2) { /* $A0BE: the music is finished, next scene */
        if (M(JingleDone)) {
            M(SceneId) = 4;
        }
    } else if (id == 3) {
        scene_step_message(g);
    } else if (id == 0x0C) {
        scene_step_enter_door(g);
    } else if (id == 4) {
        scene_step_explosion(g);
    } else if (id == 7) {
        scene_master_on_rope_continue(g); /* $A257: scene of the master tied to the rope (sub-state $B6) */
    } else if (id == 10) {
        scene_rope_continue(g); /* $A422 */
    } else if (id == 6) {
        scene_splinter_kidnapped_continue(g); /* $A1F3 */
    } else if (id == 5) {
        scene_step_defuse(g);
    } else if (id == 9) {
        scene_level4_end_continue(g);
    } else if (id == 13) {
        scene_walk_to_exit_continue(g);
    } else if (id == 14) {
        scene_game_end_continue(g);
    } else if (id == 15) {
        scene_turtle_killed_continue(g);
    } else if (id == 11) {
        scene_boss74_death_continue(g);
    } else {
        TRACE("f=%u TODO scene %d (continuation)\n", g->frame, id + 1);
    }
}

void scene_update(Game *g) /* B6 $9F47 */
{
    uint8_t b = M(SceneId);
    if (!(b & 0x80)) {
        scene_begin(g, b);
    } else {
        scene_step(g, (uint8_t)((b - 1) & 0x7F));
    }
}
