/* Generic entities of bank 7: velocities, movement, clearing, hurt blinking. */
#include "game.h"
#include "player.h"
#define TODO_AREA "player" /* prefix of the TODO traces of this file */

#define Tmp0_ Tmp0
#define OFF_X(i) (rom_prg(g->rom, B7, (uint16_t)(0xC83E + (i) * 2)))
#define OFF_Y(i) (rom_prg(g->rom, B7, (uint16_t)(0xC83F + (i) * 2)))

/* --- entities (bank 7) --- */
void clear_vel_x(Game *g, int x) {
    M(EntVelXFrac + x) = 0;
    M(EntVelX + x) = 0;
} /* $EFB2 */
void clear_vel_y(Game *g, int x) {
    M(EntVelYFrac + x) = 0;
    M(EntVelY + x) = 0;
} /* $EFAB */

void hurt_blink_tick(Game *g, int x) /* B7 $F199: blinking (flag $20) */
{
    if (!(M(EntFlags + x) & 0x20)) {
        return;
    }
    M(EntHurtTimer + x)--;
    M(EntAttr + x) = M(EntHurtTimer + x) & 3;
}

void clear_entity(Game *g, int x) /* $EF67 */
{
    static const uint16_t arrays[] = {EntSprite,    EntType, EntFlags, EntState,  EntAttr, EntY,   EntYFrac, EntX,   EntXFrac,  EntAnimFrame, EntAnimTimer,
                                      EntHurtTimer, 0x0520,  0x0530,   EntStatus, 0x0550,  0x0630, 0x0640,   0x0560, EntAnimId, 0x0580};
    for (unsigned i = 0; i < sizeof arrays / sizeof *arrays; i++) {
        M(arrays[i] + x) = 0;
    }
    clear_vel_x(g, x);
    clear_vel_y(g, x);
}

void move_entity_x(Game *g, int x) /* $EF2C */
{
    unsigned s = M(EntXFrac + x) + M(EntVelXFrac + x);
    M(EntXFrac + x) = (uint8_t)s;
    M(EntX + x) = (uint8_t)(M(EntX + x) + M(EntVelX + x) + (s > 0xFF));
}

void move_entity_y(Game *g, int x) /* $EF40 */
{
    unsigned s = M(EntYFrac + x) + M(EntVelYFrac + x);
    M(EntYFrac + x) = (uint8_t)s;
    M(EntY + x) = (uint8_t)(M(EntY + x) + M(EntVelY + x) + (s > 0xFF));
}

void move_entity(Game *g, int x) /* $EF22: if EntFlags bit 6, position += velocity */
{
    if (!(M(EntFlags + x) & 0x40)) {
        return;
    }
    move_entity_y(g, x);
    move_entity_x(g, x);
}
