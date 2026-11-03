/* Cutscene actors of bank 3. */
#include "game.h"
#include "hal.h"
#include "scenes.h"
#include "scenes/actors_bank3.h"
#include "scenes/scene_common.h"
#define TODO_AREA "scene" /* prefix of the TODO traces of this file */

/* --- bank 3 actors (D1FF..D218 -> $BBC8, $BBF5, $BC78, $BC1E, $BC1E, $BC88) --- */
static void bank3_common(Game *g, int x) /* B3 $BC04 */
{
    M(EntVar1 + x) = 0;
    M(EntX + x) = 0xB0;
    M(EntY + x) = 0xB5;
    M(EntFlags + x) = (uint8_t)((M(EntFlags + x) & 0xFD) | 1);
}

void bank3_init(Game *g, int x, int k) {
    switch (k) {
        case 0:
            M(EntVar0 + x) = 0x10;
            M(EntWait + x) = 0;
            M(EntTimer + x) = 0;
            bank3_common(g, x);
            break; /* $BBC8 */
        case 1:
            M(EntVar0 + x) = 0x50;
            M(EntWait + x) = 1;
            M(EntTimer + x) = 0;
            bank3_common(g, x);
            break; /* $BBF5 */
        case 2:
            M(EntX + x) = 0x50;
            M(EntY + x) = 0xB4;
            M(EntTimer + x) = 0x20;
            break; /* $BC78 */
        default:
            break;
    }
}

void bank3_frame(Game *g, int x, int k) {
    if (k == 2) { /* $BC88 */
        if (M(SceneCounter) != 2) {
            return;
        }
        if (M(EntTimer + x)) {
            M(EntTimer + x)--;
            return;
        }
        M(EntVar0 + x)++;
        uint8_t a = M(EntVar0 + x);
        if (a >= 3) { /* $BCB6 */
            if (a < 4) {
                M(EntVelY + x) = 0xFF;
                M(EntTimer + x) = 0x40;
                M(MusicFadeSteps) = 7;
                return;
            }
            if (a < 5) {
                M(EntVelY + x) = 0;
                M(EntVelX + x) = 2;
                M(SceneDelay) = (uint8_t)x; /* $BCDA */
                load_palette_preset(g, 0x34);
                M(SceneFrameCounter) = 1;
                M(EntTimer + x) = 0x4E;
                M(EntAnimId + x)++; /* $BCA4 */
                M(EntAnimFrame + x) = 0;
                M(EntAnimTimer + x) = 0x10;
                return;
            }
            M(SceneTimerA) = 0;
            M(EntVelX + x) = 0;
            return;
        }
        M(EntTimer + x) = 0x10;
        M(EntAnimId + x)++;
        M(EntAnimFrame + x) = 0;
        M(EntAnimTimer + x) = 0x10;
        return;
    }
    /* $BC1E */
    if (!M(EntVar1 + x)) {
        if (M(EntVar0 + x)) {
            M(EntVar0 + x)--;
            return;
        }
        uint8_t y = M(EntWait + x);
        if (M(TurtleHealth0 + y)) {
            flags_set_bit1(g, x);
        }
        M(EntVelX + x) = 0xFF;
        M(EntVar1 + x) = 1;
        return;
    }
    if (M(EntVar1 + x) == 2) {
        return;
    }
    if (M(EntX + x) >= 0x50) {
        return;
    }
    M(EntVelX + x) = 0;
    M(EntTimer + x)++;
    if (M(EntTimer + x) != 2) { /* $BBD5 */
        M(EntWait + x) += 2;
        M(SceneIndex)++;
        uint8_t col = (uint8_t)(M(SceneIndex) + 0x31);
        M(SceneDelay) = (uint8_t)x; /* $BBE4 */
        load_palette_preset(g, col);
        M(EntVar0 + x) = 1;
        bank3_common(g, x);
        return;
    }
    flags_clear_bit1(g, x);
    M(EntVar1 + x) = 2;
    M(SceneCounter)++;
}
