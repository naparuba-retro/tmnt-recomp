/* Boss $74: large background picture with eyes. */
#include "game.h"
#include "enemy.h"
#include "hal.h"
#include "sound.h"
#include "enemies/bosses.h"
#include "enemies/purple_ninja.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- boss $74 (B5 $AFF8-$B248): the boss is a large background picture (columns written into the name table), entity 5; entity 15 carries the eyes --- */
static void eyes_next_state(Game *g, int x) /* B5 $B138: next state, counter and picture reset to zero */
{
    M(EntState + x)++;
    M(EntTimer + x) = 0;
    M(EntAnimFrame + x) = 0;
}

void boss74_picture(Game *g, uint8_t n) /* B5 $B170: boss picture n, 4 columns of 8 tiles in the name table ($208E-$2091) */
{
    uint16_t t = (uint16_t)(0xB1A5 + n * 32);
    for (uint8_t col = 0x8E; col != 0x92; col++) {
        ppu_buffer_append(g, 8);
        ppu_buffer_append(g, 0xA0);
        ppu_buffer_append(g, col);
        for (int i = 0; i < 8; i++) {
            ppu_buffer_append(g, R5(t++));
        }
    }
}

static void eyes_picture(Game *g, int x, uint8_t n) {
    (void)x;
    boss74_picture(g, n);
}

static void eyes_follow_player(Game *g, int x) /* B5 $B144: the eyes (sprite of entity 15) follow the player */
{
    M(EntSprite + 15) = R5(0xB239 + (M(EntX) >> 5));
    (void)x;
}

static void eyes_fire(Game *g, int x) /* B5 $B154: two projectiles according to the player's position */
{
    sound_play(g, 0x2D);
    uint8_t a = R5(0xB241 + (M(EntX) >> 5));
    spawn_child_slot(g, x, a);
    spawn_child_slot(g, x, (uint8_t)(a + 1));
}

void boss_eyes_machine(Game *g, int x) {
    if (M(0x03D5)) {
        M(EntState + x) = 1;
    }
    M(EntTimer + x)++;
    switch (M(EntState + x)) {
        case 0: /* $B01C */
            boss_place(g, 0x0F, 4);
            boss_place(g, 5, 5);
            M(EntAnimTimer + x) = 0x0A;
            M(EntState + x)++;
            return;
        case 1: /* $B034: the eyes turn */
            if (--M(EntAnimTimer + x) == 0) {
                M(EntAnimTimer + x) = 0x0A;
                M(EntAnimFrame + x) = (uint8_t)((M(EntAnimFrame + x) + 1) & 7);
                M(EntSprite + 15) = R5(0xB225 + M(EntAnimFrame + x));
            }
            if (M(EntTimer + x) < 0x20) {
                return;
            }
            M(EntAnimTimer + x) = 4;
            eyes_next_state(g, x);
            return;
        case 2: /* $B05F */
            if (--M(EntAnimTimer + x) == 0) {
                M(EntAnimTimer + x) = 4;
                M(EntAnimFrame + x)++;
                M(EntSprite + 15) = R5(0xB22D + M(EntAnimFrame + x));
            }
            if (M(EntAnimFrame + x) != 0x0C) {
                return;
            }
            eyes_next_state(g, x);
            return;
        case 3: /* $B07F */
            eyes_follow_player(g, x);
            if (M(EntTimer + x) < 0x10) {
                return;
            }
            eyes_next_state(g, x);
            return;
        case 4: { /* $B08C: shot, twice at frame $10 */
            M(EntAttr + 15) = (uint8_t)(((M(FrameCounter) >> 1) & 1) | 2);
            uint8_t c = M(EntTimer + x);
            if (c < 0x28) {
                if (c != 0x10) {
                    return;
                }
                eyes_fire(g, x);
                M(EntAttr + x) = 0;
            }
            eyes_fire(g, x);
            eyes_next_state(g, x);
            return;
        }
        case 5: /* $B0AF */
            eyes_follow_player(g, x);
            M(EntSprite + 15) = 0xA3;
            eyes_picture(g, x, 1);
            eyes_next_state(g, x);
            return;
        case 6: /* $B0C6 */
            if (M(EntTimer + x) < 0x20) {
                return;
            }
            M(EntSprite + 15) = 0xB0;
            eyes_picture(g, x, 2);
            M(EntFlags + x) = 0xC2;
            eyes_next_state(g, x);
            return;
        case 7: { /* $B0DF: an enemy $8D every $20 frames */
            uint8_t c = M(EntTimer + x);
            if (!(c & 0x1F)) {
                M(CurrentEntity) = (uint8_t)x;
                int n = 5;
                while (n < 0x0B && M(EntType + n)) {
                    n++; /* B7 $EEF2 */
                }
                if (n < 0x0B) {
                    boss_place(g, n, 6);
                    M(EntHealth + n) = 4;
                    M(EntAnimId + n) = 1;
                    M(EntType + n) = 0x8D;
                    M(EntAnimTimer + n) = 2;
                }
            }
            if (c < 0x60) {
                return;
            }
            M(EntSprite + 15) = 0xA3;
            eyes_picture(g, x, 1);
            eyes_next_state(g, x);
            return;
        }
        case 8: /* $B11B: back to state 1 */
            if (M(EntTimer + x) < 0x10) {
                return;
            }
            M(EntAnimTimer + x) = 0x0A;
            eyes_picture(g, x, 0);
            M(EntState + x) = 1;
            M(EntFlags + x) = 0xC0;
            M(EntTimer + x) = 0;
            M(EntAnimFrame + x) = 0;
            return;
        default:
            return;
    }
}
