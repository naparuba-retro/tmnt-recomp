/* Type $2F: shadow of the van. */
#include "game.h"
#include "player.h"
#include "enemy.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- type $2F (B5 $B331): shadow of entity 5 (the van) --- */
void ai_van_shadow(Game *g, int x) {
    if (!(M(EntFlags + 5) & 0x80)) {
        clear_entity(g, x);
        return;
    }
    M(EntY + x) = M(EntY + 5);
    M(EntX + x) = M(EntX + 5);
    M(EntAnimTimer + x) = M(EntAnimTimer + 5);
    M(EntFlags + x) = (uint8_t)(0xC2 + (M(EntFlags + 5) & 1));
    if (M(EntAnimFrame + x) == 0xFF) {
        clear_entity(g, x);
        return;
    }
    uint8_t y = (uint8_t)(M(EntAnimFrame + x) << 1);
    if (M(EntAnimTimer + x) != 1) {
        y = (uint8_t)(y - 2);
    }
    if (y >= 6) {
        M(EntFlags + x) &= 0xFD;
        return;
    }
    M(Tmp2) = R7(0xE27C + y);
    if (M(EntFlags + x) & 1) {
        M(EntX + x) = (uint8_t)(M(EntX + x) - M(Tmp2));
    } else {
        M(EntX + x) = (uint8_t)(M(EntX + x) + M(Tmp2));
    }
    M(EntY + x) = (uint8_t)(M(EntY + x) + R7(0xE27D + y));
}
