/* Score (BCD points). */
#include "game.h"
#include "player.h"
#include "sound.h"
#include "stage_play.h"
#include "enemy.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- score (B7 $C89E-$C93A), BCD points on 3 bytes at $C2 (score) and $C5 --- */
static void bcd_split_digits(Game *g, uint8_t a) /* B7 $C928: Tmp0 = units, Tmp1 = tens */
{
    M(Tmp0) = 0;
    M(Tmp0 + 1) = 0;
    while (a >= 10) {
        a -= 10;
        M(Tmp0 + 1)++;
    }
    M(Tmp0) = a;
}

static void score_add_at(Game *g, uint8_t lo, uint8_t hi, uint8_t y) /* B7 $C8CC */
{
    M(Tmp10) = lo;
    M(Tmp11) = hi;
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        M(Tmp12) = M(ScoreBcd + y) & 0x0F;
        bcd_split_digits(g, (uint8_t)((M(Tmp10) & 0x0F) + M(Tmp12)));
        M(Tmp14) = M(Tmp0);
        M(Tmp12) = M(ScoreBcd + y) >> 4;
        bcd_split_digits(g, (uint8_t)((M(Tmp10) >> 4) + M(Tmp0 + 1) + M(Tmp12)));
        M(ScoreBcd + y) = (uint8_t)(M(Tmp0) << 4 | M(Tmp14));
        uint8_t a;
        if (M(Tmp0 + 1)) {
            if (y == 1) {
                a = M(Tmp0 + 1);
            } else {
                a = (uint8_t)(M(Tmp0 + 1) + M(Tmp11));
            }
        } else {
            if (y == 1) {
                return;
            }
            a = M(Tmp11);
        }
        M(Tmp10) = a;
        y++;
        if (y == 3 || y == 6) {
            return;
        }
    }
}

static void score_add_player(Game *g, uint8_t lo, uint8_t hi) /* B7 $C8A6: adds, then best score at $C8 */
{
    score_add_at(g, lo, hi, 0);
    for (int y = 2; y >= 0; y--) {
        if (M(ScoreBcd + y) == M(BestScoreBcd + y)) {
            continue;
        }
        if (M(ScoreBcd + y) > M(BestScoreBcd + y)) {
            for (int i = 2; i >= 0; i--) {
                M(BestScoreBcd + i) = M(ScoreBcd + i);
            }
        }
        return;
    }
}

void enemy_score(Game *g, int x) /* B6 $9BDF: points according to the EntStatus of the enemy */
{
    if (!M(EntStatus + x)) {
        return;
    }
    uint8_t i = (uint8_t)((M(EntStatus + x) & 0x78) >> 2);
    score_add_player(g, R6(0x9C03 + i), R6(0x9C04 + i));
    score_add_at(g, R6(0x9C03 + i), R6(0x9C04 + i), 3);
}

void crate_contact(Game *g, int x) /* B6 $BB83: the crate stops the player */
{
    if (!(M(EntSprite + x) & 1)) {
        return;
    }
    if (!M(0x8D)) {
        uint8_t right = M(EntX) >= M(EntX + x);
        M(Tmp10) = right;
        M(0x8D) = (uint8_t)(0x80 | right);
        M(EntX) = (uint8_t)(M(EntX) + (right ? 4 : 0xFC));
        M(EntVelX) = 0;
        return;
    }
    if (!M(EntVelX)) {
        return;
    }
    uint8_t dir = (M(EntVelX) & 0x80) ? 1 : 0;
    M(Tmp10) = dir;
    if ((M(0x8D) ^ dir) & 1) {
        return;
    }
    M(EntVelX) = 0;
}

void crate_hit(Game *g, int x) /* B6 $BB13: the blow destroys the crate and frees the tile */
{
    M(ColumnsDestroyedMask) |= R6(0xBB7B + M(EntHealth + x));
    M(EntType + x) = 0x15;
    M(EntAnimId + x) = 0xD3;
    sound_play(g, 0x27);
    uint8_t o = M(EntVar1 + x);
    M(ObjType + o) = 0;
    M(Tmp0) = M(EntX + x);
    M(Tmp1) = M(EntY + x);
    tile_at_xy(g, M(EntX + x), M(EntY + x), o);
    if (!(M(EntSprite + x) & 1)) {
        uint8_t idx = M(TileIndex);
        M(0x06F8 + idx) = (M(EntY + x) & 0x80) ? M(TerrainMap + idx) : M(0x06F0 + idx);
        clear_object(g, o);
    }
}
