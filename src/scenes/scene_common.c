/* Helpers shared by the cutscenes: animation, clearing, entity creation. */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "scenes.h"
#include "scenes/scene_common.h"
#define TODO_AREA "scene" /* prefix of the TODO traces of this file */

void animate0(Game *g) {
    animate_entities_from(g, 0);
} /* B7 $CFFA */
void clear_all(Game *g) {
    for (int x = 0; x < 16; x++) {
        clear_entity(g, x);
    }
} /* B7 $EF5C */

void zero_vel(Game *g, int x) /* B2 $B98A */
{
    M(EntVelX + x) = M(EntVelXFrac + x) = M(EntVelY + x) = M(EntVelYFrac + x) = 0;
}

void zero_frac_vel(Game *g, int x) /* B2 $BB5B */
{
    M(EntXFrac + x) = M(EntYFrac + x) = 0;
    zero_vel(g, x);
}

void flags_clear_bit1(Game *g, int x) {
    M(EntFlags + x) &= 0xFD;
}
void flags_set_bit1(Game *g, int x) {
    M(EntFlags + x) |= 0x02;
} /* B7 $F301 */

/* --- entity created from its type (B7 $FE2D, with bank 6) --- */
void entity_from_type(Game *g, int x) {
    uint8_t i = (uint8_t)(M(EntType + x) << 1);
    M(EntHealth + x) = R6(0x883D + i);
    uint8_t a = R6(0x883E + i);
    M(EntAnimId + x) = a;
    uint16_t p = rom_word(g->rom, 6, (uint16_t)(0x8E11 + (uint8_t)(a << 1)));
    M(EntAnimFrame + x) = 0;
    M(EntSprite + x) = R6(p);
    M(EntAnimTimer + x) = 1;
    M(EntFlags + x) = 0xC2;
}
