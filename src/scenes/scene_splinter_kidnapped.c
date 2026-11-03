/* Scene 7 (end of level 2): Splinter is kidnapped, after the last bomb is defused (state machine $03C2 of bank 2). */
#include "game.h"
#include "hal.h"
#include "pause.h"
#include "player.h"
#include "scenes.h"
#include "sound.h"
#include "scenes/actor_groups.h"
#include "scenes/ppu_text.h"
#include "scenes/scene_common.h"
#define TODO_AREA "scene" /* prefix of the TODO traces of this file */

/* --- state machine $03C2 (B2 $B5BE) --- */
static void splinter_kidnapped_show_title(Game *g) /* $B5EA */
{
    set_chr_bank0(g, 0x11);
    M(0x36) = 1;
    M(EntType) = 0xAD;
    entity_from_type(g, 0);
    M(SceneTimerA) = 2;
    M(SceneTimerB) = 0x20;
    animate0(g);
    M(SceneState)++;
    M(0x03E9) = 1;
}

static void splinter_kidnapped_flash_wait(Game *g) /* $B613 */
{
    if (--M(SceneTimerB) == 0) {
        if (--M(SceneTimerA)) {
            sound_play(g, 0x7F);
            return;
        }
        M(SceneState)++;
        clear_all(g);
        animate0(g);
        return;
    }
    animate0(g);
    if (M(EntAnimFrame) == 0xFF) {
        M(EntAnimFrame) = 1;
        M(EntAnimTimer) = 0xFF;
    }
}

static void splinter_kidnapped_info_init(Game *g) /* $B644 */
{
    set_chr_bank1(g, 0x1B);
    M(CameraXLo) = 0;
    M(CameraYHi) = 1;
    M(CameraYLo) = 0xD8;
    M(0x03D4) = 0;
    scene_info_init(g);
    M(SceneTimerB) = 0x10;
    M(SceneTimerA) = 0x80;
    M(SceneState)++;
    sound_play(g, 0x4E);
    M(0x03E9) = 0;
}

static void splinter_kidnapped_info_run(Game *g) /* $B675 */
{
    if (M(SceneTimerA)) {
        scene_info_update(g);
        if (M(TextState) == 3) {
            M(SceneTimerA) = 0;
            clear_all(g);
        }
        animate0(g);
        return;
    }
    if (--M(SceneTimerB)) {
        M(SceneState)++;
    }
}

static void splinter_kidnapped_group1_setup(Game *g) /* $B699 */
{
    set_chr_bank0(g, 1);
    set_chr_bank1(g, 0x1C);
    load_level_palette(g, 0x17);
    M(CameraYLo) = 0;
    M(CameraYHi) = 0;
    load_screen(g, 2, 0x0E);
    M(SceneState)++;
    M(SceneTimerB) = 0x40;
    M(SceneTimerA) = 1;
    M(SceneIndex) = 0;
    group_setup(g, 0);
    animate0(g);
}

static void splinter_kidnapped_group1_run(Game *g) /* $B6CE */
{
    if (M(SceneTimerA)) {
        group_frame(g);
        if (M(SceneIndex)) {
            M(SceneTimerA) = 0;
        }
        animate0(g);
        return;
    }
    if (--M(SceneTimerB)) {
        return;
    }
    M(SceneState)++;
    clear_all(g);
    animate0(g);
}

static void splinter_kidnapped_group2_setup(Game *g) /* $B6F4 */
{
    load_screen(g, 2, 0x10);
    load_level_palette(g, 0x17);
    M(SceneState)++;
    M(SceneTimerA) = 1;
    M(SceneTimerB) = 0x10;
    M(SceneIndex) = 0;
    M(SceneCounter) = 0;
    group_setup(g, 1);
    animate0(g);
}

static void splinter_kidnapped_group2_run(Game *g) /* $B71E */
{
    if (M(SceneTimerA)) {
        group_frame(g);
        if (M(SceneIndex)) {
            M(SceneTimerA) = 0;
            fill_palette_backdrop(g, 0x0F);
        } else if (M(SceneCounter)) {
            fill_palette_backdrop(g, (M(FrameCounter) & 4) ? 0x0F : 0x16);
        }
        animate0(g);
        return;
    }
    if (--M(SceneTimerB) == 0) {
        M(SceneState)++;
        animate0(g);
    }
}

static void splinter_kidnapped_clear(Game *g) /* $B75B */
{
    M(SceneState)++;
    M(SceneTimerB) = 2;
    clear_all(g);
    animate0(g);
}

static void splinter_kidnapped_wait(Game *g) /* $B76A */
{
    if (--M(SceneTimerB) == 0) {
        M(SceneState)++;
    }
}

static void splinter_kidnapped_group3_setup(Game *g) /* $B774 */
{
    set_chr_bank1(g, 0x1D);
    load_level_palette(g, 0x18);
    load_screen(g, 2, 0x12);
    M(SceneState)++;
    M(SceneTimerB) = 0x4F;
    group_setup(g, 2);
    animate0(g);
    load_palette_preset(g, 0x37);
}

static void splinter_kidnapped_palette_fade_in(Game *g) /* $B79B */
{
    if (--M(SceneTimerB) == 0) {
        M(SceneState)++;
        return;
    }
    load_palette_preset(g, R2(0xB7B3 + (M(SceneTimerB) >> 4)));
}

static void splinter_kidnapped_script_init(Game *g) /* $B7B8 */
{
    M(SceneTimerA) = 1;
    M(SceneState)++;
    M(SceneIndex) = 0;
    M(SceneTimerB) = 0;
}

static void splinter_kidnapped_typewriter(Game *g) /* $B7C9 */
{
    uint8_t ra;
    if (!M(SceneTimerA)) {
        M(SceneState)++;
        return;
    }
    group_frame(g);
    animate0(g);
    if (M(SceneTimerB)) {
        M(SceneTimerB)--;
        return;
    }
    uint8_t a = R2(0xB815 + M(SceneIndex));
    int carry = ppu_script(g, a, &ra);
    a = ra;
    if (carry) {
        M(SceneIndex)++;
        a = R2(0xB815 + M(SceneIndex));
        if (a == 0xFF) {
            M(SceneTimerA) = 0;
            a = 0;
        }
    }
    if (a == 0xFE) {
        M(SceneIndex)++;
        a = R2(0xB815 + M(SceneIndex));
        if (a == 0xFF) {
            M(SceneTimerA) = 0;
            return;
        }
        M(SceneTimerB) = 0x40;
    }
}

static void splinter_kidnapped_clear_for_fade(Game *g) /* $B821 */
{
    M(SceneTimerB) = 0;
    M(SceneState)++;
    clear_all(g);
    animate0(g);
}

static void splinter_kidnapped_palette_fade_out(Game *g) /* $B830 */
{
    M(SceneTimerB)++;
    if (M(SceneTimerB) == 0x4F) {
        M(SceneState)++;
        return;
    }
    load_palette_preset(g, R2(0xB7B3 + (M(SceneTimerB) >> 4)));
}

static void splinter_kidnapped_info2_init(Game *g) /* $B84D */
{
    set_chr_bank1(g, 0x1B);
    M(CameraXLo) = 0;
    M(CameraYHi) = 1;
    M(CameraYLo) = 0xD8;
    M(0x03D4) = 1;
    scene_info_init(g);
    M(SceneTimerB) = 0x80;
    M(SceneTimerA) = 0x80;
    M(SceneState)++;
}

static void splinter_kidnapped_info2_run(Game *g) /* $B874 */
{
    if (M(SceneTimerA)) {
        scene_info_update(g);
        if (M(TextState) == 3) {
            M(SceneTimerA) = 0;
            clear_all(g);
            M(MusicFadeSteps) = 7;
        }
        animate0(g);
        return;
    }
    if (--M(SceneTimerB) == 0) {
        M(SceneState)++;
    }
}

static void splinter_kidnapped_finish(Game *g) /* $B89D */
{
    M(0x03D4) = 0;
    for (int i = 1; i <= 7; i++) {
        M(SceneState + i) = 0;
    }
    M(SceneState) = 0x14;
}

static void state_machine(Game *g) {
    switch (M(SceneState)) {
        case 0:
            splinter_kidnapped_show_title(g);
            break;
        case 1:
            splinter_kidnapped_flash_wait(g);
            break;
        case 2:
            splinter_kidnapped_info_init(g);
            break;
        case 3:
            splinter_kidnapped_info_run(g);
            break;
        case 4:
            splinter_kidnapped_group1_setup(g);
            break;
        case 5:
            splinter_kidnapped_group1_run(g);
            break;
        case 6:
            splinter_kidnapped_group2_setup(g);
            break;
        case 7:
            splinter_kidnapped_group2_run(g);
            break;
        case 8:
            splinter_kidnapped_clear(g);
            break;
        case 9:
            splinter_kidnapped_wait(g);
            break;
        case 10:
            splinter_kidnapped_group3_setup(g);
            break;
        case 11:
            splinter_kidnapped_palette_fade_in(g);
            break;
        case 12:
            splinter_kidnapped_script_init(g);
            break;
        case 13:
            splinter_kidnapped_typewriter(g);
            break;
        case 14:
            splinter_kidnapped_clear_for_fade(g);
            break;
        case 15:
            splinter_kidnapped_palette_fade_out(g);
            break;
        case 16:
            splinter_kidnapped_info2_init(g);
            break;
        case 17:
            splinter_kidnapped_info2_run(g);
            break;
        case 18:
            splinter_kidnapped_finish(g);
            break;
        default:
            break;
    }
}

void scene_splinter_kidnapped_start(Game *g) /* B6 $A1ED */
{
    M(SceneId) |= 0x80; /* $A83B */
    M(SceneStep) = 0;
    M(SceneState) = 0; /* D15E -> B2 $B5B2 (X stays at 0) */
}

void scene_splinter_kidnapped_continue(Game *g) /* B6 $A1F3 */
{
    M(0x03ED) = 0;
    if (M(SceneState) < 0x13) {
        state_machine(g);
        return;
    }
    M(SceneState) = 0; /* $A202 */
    M(0x9F) = 0;
    M(LevelNumber) = 2; /* $A0F3: level 3 */
    M(AreaNumber) = 2;
    M(SideViewFlag) = 0;
    M(DelayTimerLo) = M(DelayTimerHi) = 0;
    M(StageState) = 2;
    clear_entity(g, 0);
    M(SceneStep) = 0;
    M(SceneId) = 0; /* $A846 */
}
