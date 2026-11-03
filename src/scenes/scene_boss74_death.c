/* Death of boss $74. */
#include "game.h"
#include "anim.h"
#include "enemy.h"
#include "hal.h"
#include "player.h"
#include "sound.h"
#include "stage_play.h"
#include "scenes/scene_boss74_death.h"
#include "scenes/scene_runner.h"
#define TODO_AREA "stage_play" /* prefix of the TODO traces of this file */

static void scene_boss74_explosions(Game *g) /* B6 $A7FD: three explosions around the boss */
{
    M(EntFlags + 6) = M(EntFlags + 7) = M(EntFlags + 8) = 0x42;
    M(EntAnimId + 6) = M(EntAnimId + 7) = M(EntAnimId + 8) = 0xD3;
    M(EntAnimTimer + 6) = M(EntAnimTimer + 7) = M(EntAnimTimer + 8) = 2;
    M(EntX + 6) = 0x70;
    M(EntX + 7) = 0x80;
    M(EntX + 8) = 0x90;
    M(EntY + 7) = 0x38;
    M(EntY + 6) = M(EntY + 8) = 0x48;
}

void scene_boss74_death_continue(Game *g) /* B6 $A6E7: death of boss $74 */
{
    if (M(SceneStep) != 4) {
        scene_run_frame(g);
    }
    switch (M(SceneStep)) {
        case 0: /* $A6FF */
            if (M(EntAnimFrame + 5) != 0xFF) {
                return;
            }
            M(EntAnimId + 5) = 0;
            M(DelayTimerLo) = 0x88;
            M(SceneStep)++;
            sound_play(g, 0x48);
            return;
        case 1: /* $A718 */
            if (--M(DelayTimerLo) == 0) {
                ppu_buffer_append(g, 3);
                ppu_buffer_append(g, 0xA0);
                ppu_buffer_append(g, 0xED);
                for (int i = 0; i < 3; i++) {
                    ppu_buffer_append(g, 0);
                }
                ppu_buffer_append(g, 3);
                ppu_buffer_append(g, 0xA0);
                ppu_buffer_append(g, 0xF2);
                for (int i = 0; i < 3; i++) {
                    ppu_buffer_append(g, 0);
                }
                M(EntFlags + 6) = M(EntFlags + 7) = M(EntFlags + 8) = M(EntFlags + 5) = M(EntFlags + 15) = 0;
                M(SceneStep)++;
            } else if (!(M(DelayTimerLo) & 0x0F)) {
                scene_boss74_explosions(g);
            }
            return;
        case 2: /* $A76C */
            boss74_picture(g, 3);
            M(DelayTimerLo) = 0x15;
            M(SceneStep)++;
            sound_play(g, 0x31);
            return;
        case 3: /* $A77C: the background is erased line by line */
            M(DelayTimerLo)--;
            if (M(DelayTimerLo) & 3) {
                return;
            }
            ppu_buffer_append(g, 1);
            ppu_buffer_append(g, 0x22);
            ppu_buffer_append(g, (uint8_t)((M(DelayTimerLo) << 3) + 0x1E));
            ppu_buffer_append(g, 0);
            if (M(DelayTimerLo)) {
                return;
            }
            M(0x074F) = 0;
            M(0x0757) = 0;
            M(EntFlags) &= 0xFE;
            for (int x = 1; x < 16; x++) {
                clear_entity(g, x);
            }
            M(SceneStep)++;
            return;
        default: /* $A7BB: the turtle goes down then exits on the right */
            animate_entities_from(g, 0);
            if (M(EntY) < 0x9C) {
                set_animation(g, 0, 0x85);
                M(EntY) += 3;
                return;
            }
            if (M(EntAnimId) != 0x81) {
                set_animation(g, 0, 0x81);
            }
            M(EntX) += 2;
            if (M(EntX) >= 0xEF) {
                M(SceneId) = 0x0A;
                M(SceneStep) = 0;
            }
            return;
    }
}
