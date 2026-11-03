/* Player: update, state dispatch and spawn. The rest is in player/. */
#include "game.h"
#include "anim.h"
#include "hal.h"
#include "player.h"
#include "sound.h"
#include "player/overhead.h"
#include "player/pose.h"
#include "player/scroll.h"
#include "player/shared.h"
#include "player/special_states.h"
#define TODO_AREA "player" /* prefix of the TODO traces of this file */

void player_state_dispatch(Game *g) {
    player_run_state(g, M(EntState));
}

void player_spawn(Game *g) /* B4 $9662 */
{
    clear_entity(g, 0);
    if (!M(SideViewFlag)) { /* $96B8: start in top view */
        M(EntSprite) = 0;
        M(EntFlags) = 0xC2;
        uint8_t y = (uint8_t)(M(AreaNumber) << 1);
        M(EntX) = rom_prg(g->rom, B7, (uint16_t)(0xD75A + y));
        M(EntY) = rom_prg(g->rom, B7, (uint16_t)(0xD75B + y));
        if (M(AreaNumber) == 2) { /* $96D2: the van waits, the player is not in it */
            M(EntX + 5) = rom_prg(g->rom, B7, (uint16_t)(0xD7C8 + y));
            M(EntY + 5) = rom_prg(g->rom, B7, (uint16_t)(0xD7C9 + y));
            M(EntAnimId + 5) = 0xCF;
            M(InVanFlag) = 0;
        }
        return;
    }
    if (M(LevelNumber) == 5) { /* $9671: level 6, the turtle arrives from the final scene */
        M(EntX) = 0xB0;
        M(EntY) = 0x8C;
        M(EntSprite) = 9;
        M(EntAnimId) = 0x80;
        M(EntFlags) = 0xC2;
        return;
    }
    uint16_t p = rom_word(g->rom, B7, (uint16_t)(0xD764 + M(LevelNumber) * 2));
    uint8_t y = (uint8_t)(M(AreaNumber) << 1);
    M(EntX) = rom_prg(g->rom, B7, (uint16_t)(p + y));
    M(EntY) = rom_prg(g->rom, B7, (uint16_t)(p + y + 1));
    M(EntSprite) = 0;
    if (M(LevelNumber) == 2) { /* $96AB -> $96D8: the van too, at the position of the table for this zone */
        M(EntX + 5) = rom_prg(g->rom, B7, (uint16_t)(0xD7C8 + y));
        M(EntY + 5) = rom_prg(g->rom, B7, (uint16_t)(0xD7C9 + y));
        M(EntAnimId + 5) = 0xCF;
        M(InVanFlag) = 0;
    }
    M(EntFlags) = 0xC2;
}

void player_update(Game *g) /* B4 $9745 */
{
    M(ScrollDirY) = 0;
    M(ScrollDirX) = 0;
    M(CameraDeltaX) = 0;
    M(CameraDeltaY) = 0;
    M(CameraXLoSaved) = M(CameraXLo);
    M(CameraYLoSaved) = M(CameraYLo);
    if (M(0xD2)) {
        M(JoyPressed1) = 0;
        M(JoyHeld1) = 0;
    }
    if (M(ZoneChangeDelay)) { /* $9761: delay after a zone change */
        M(ZoneChangeDelay)--;
        if (M(SideViewFlag)) {
            player_animation_state(g);
            player_pose_anim(g);
        }
        return;
    }
    is_special_area(g);
    if (!(M(EntFlags) & 0x80)) { /* $9770: death in progress */
        if (M(SceneIndex)) {
            M(EntFlags) = 0xC2;
            set_animation(g, 0, 0x80);
            return;
        } /* $977A: jmp $D121, during a scene the player is restarted alive */
        if (M(EntAnimFrame) != 0xFF) {
            return; /* $9789: waits for the end of the animation */
        }
        M(EntAnimId) = 0;
        M(EntSprite) = 0;
        if (!M(JingleDone)) {
            return; /* sound driver flag: jingle finished */
        }
        M(StageState) = 7;
        load_screen_and_clear(g);
        return;
    }
    if ((M(EntFlags) & 0x04) && player_hurt_state(g)) {
        return;
    }
    uint8_t h = M(TurtleHealth0 + M(CurrentTurtle)); /* $9989: low health warning */
    if (h && h < 0x18) {
        if (!M(0x03D6)) {
            M(0x03D6) = 1;
            sound_play(g, 0x33);
        } /* low health warning */
    } else if (h) {
        M(0x03D6) = 0;
    }
    if (!M(SideViewFlag)) {
        overhead_player(g);
        return;
    }
    if (M(VerticalScrollMode) || M(ScreenChangeDir)) {
        side_vertical_scroll(g);
        return;
    } /* $981D */
    if (M(JoyPressed1) & 0x20) { /* $982C: Select changes weapon */
        M(SubWeaponSelected) ^= 1;
        if (!M(SubWeaponAmmo0 + M(CurrentTurtle))) {
            M(SubWeaponSelected) = 0;
        } else {
            sound_play(g, 0x3C);
        }
    }
    if (M(LevelNumber) == 2) {
        level3_pit_blink(g); /* $984E */
    }
    player_scroll_trigger(g);
    if (!M(ScrollDirX)) {
        move_entity_x(g, 0); /* $98A0: the camera carries the player when it scrolls */
    }
    move_entity_y(g, 0);
    if (M(EntX) < 0x14) {
        M(EntX) = 0x14;
    }
    if (M(EntX) >= 0xEC) {
        M(EntX) = 0xEC;
    }
    if (M(SwimZoneFlag)) { /* $98C7: screen edges while swimming */
        uint8_t y = M(EntY);
        if (y < 0x10) {
            clear_vel_y(g, 0);
            y = 0x10;
        }
        if (y >= 0xB5) {
            clear_vel_y(g, 0);
            y = 0xB5;
        }
        M(EntY) = y;
        uint8_t dir = 0;
        if (y == 0x10 && (M(EntFlags) & 0x80)) {
            dir = 1;
        } else if (y == 0xB5 && (M(EntFlags) & 0x80)) {
            dir = 2;
        }
        if (dir && !M(WaveState)) { /* $98F7: vertical screen change */
            M(VerticalScrollMode) = dir;
            M(VerticalScrollTimer) = 0x60;
            for (int x = 1; x < 16; x++) {
                clear_entity(g, x);
            }
            return;
        }
        player_animation_state(g); /* $9966 */
        player_pose_anim(g);
        return;
    }
    if (side_screen_edge(g)) {
        return; /* $9906 */
    }
    push_zone(g);              /* $9BE4 */
    player_animation_state(g); /* $9969 */
    player_pose_anim(g);
}
