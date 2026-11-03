/* The player's weapon (entity 1). */
#include "game.h"
#include "player.h"
#include "player/tiles.h"
#define TODO_AREA "player" /* prefix of the TODO traces of this file */

static void weapon_side(Game *g);

/* --- player's weapon (entity 1): B7 $E162 --- */
static void weapon_follow_player(Game *g) /* $E235: the weapon starts at the player's position */
{
    M(EntY + 1) = M(EntY);
    M(EntYFrac + 1) = M(EntYFrac);
    M(EntX + 1) = M(EntX);
    M(EntXFrac + 1) = M(EntXFrac);
    M(EntFlags + 1) = (uint8_t)((M(EntFlags + 1) & 0xFE) + (M(EntFlags) & 1));
}

/* B7 $E1BC: weapon offset (X according to the orientation, Y) read from the table p at index y */
static void weapon_apply_offsets(Game *g, uint16_t p, uint8_t y) {
    uint8_t off = rdmem_off(g, p, y); /* the table may point to RAM (mirror) */
    if (!(M(EntFlags + 1) & 1)) {
        M(EntX + 1) = (uint8_t)(M(EntX + 1) + off);
    } else {
        M(EntX + 1) = (uint8_t)(M(EntX + 1) - off);
    }
    M(EntY + 1) = (uint8_t)(M(EntY + 1) + rdmem_off(g, p, (uint16_t)(y + 1)));
    int clear_visible;
    if (M(EntX) < 0x1D) {
        clear_visible = (M(EntFlags) & 1) != 0;
    } else if (M(EntX) < 0xE4) {
        clear_visible = 0;
    } else {
        clear_visible = (M(EntFlags) & 1) == 0;
    }
    if (clear_visible) {
        M(EntFlags + 1) &= 0xFD;
    }
}

static void boss73_part(Game *g) /* B7 $E342: the element of entity 13 follows boss $73 (entity 5) */
{
    if (M(EntType + 5) != 0x73) {
        return;
    }
    { /* the element of entity 13 follows boss $73 */
        uint8_t i = (uint8_t)(M(EntSprite + 5) - 0x82);
        if (i >= 8) {
            clear_entity(g, 0x0D);
            return;
        }
        uint8_t dx = R7(0xE392 + 2 * i), dy = R7(0xE393 + 2 * i);
        if (M(EntFlags + 5) & 1) {
            dx = (uint8_t)(~dx + 1);
        }
        M(Tmp0) = dx;
        M(Tmp1) = dy;
        M(EntX + 13) = (uint8_t)(M(EntX + 5) + dx);
        M(EntY + 13) = (uint8_t)(M(EntY + 5) + dy);
        M(EntSprite + 13) = 0x94;
        M(EntFlags + 13) = M(EntFlags + 5);
    }
}

void weapon_update(Game *g) {
    if (!M(SideViewFlag)) { /* $E202: top view */
        weapon_follow_player(g);
        if (!M(EntSprite + 1)) {
            return;
        }
        if ((uint8_t)(M(EntAnimFrame + 1) + 1) == 0) {
            M(EntSprite + 1) = 0;
            M(EntAnimId + 1) = 0;
            return;
        }
        uint16_t p = rom_word(g->rom, B7, (uint16_t)(0xE2FA + (uint8_t)((M(EntAnimId + 1) - 0xB0) << 1)));
        weapon_apply_offsets(g, p, (uint8_t)(M(EntAnimFrame + 1) << 1));
        return;
    }
    weapon_side(g); /* $E169 */
    boss73_part(g); /* $E16C -> $E342: every frame, even when $E16F returns early */
}

static void weapon_side(Game *g) /* B7 $E16F: weapon in side view */
{
    if (M(EntAnimId + 1) == 0) {
        return; /* $E16F */
    }
    if (M(EntAnimId + 1) == 0xD8) { /* $E3A2: the swimmer's bubble */
        move_entity_y(g, 1);
        move_entity_x(g, 1);
        M(EntX + 1) = (uint8_t)(M(EntX + 1) - M(CameraDeltaX));
        uint8_t idx = (uint8_t)((M(WaterCurrent) >> 3) & 0x1E);
        M(EntX + 1) = (uint8_t)(M(EntX + 1) + R7(0xE3E1 + idx));
        uint8_t ny = (uint8_t)(M(EntY + 1) + R7(0xE3E2 + idx));
        if (ny >= 0xF0) {
            clear_entity(g, 1);
            return;
        }
        M(EntY + 1) = ny;
        M(Tmp0) = M(EntX + 1);
        M(Tmp0 + 1) = ny;
        if (tile_at(g, M(EntX + 1), ny, idx) != 0) {
            clear_entity(g, 1);
        }
        return;
    }
    if (!(M(EntFlags) & 0x80) || M(EntAnimFrame + 1) == 0xFF || (M(EntStatus) & 0x40)) {
        clear_entity(g, 1);
        return;
    }
    weapon_follow_player(g);
    if (!M(EntSprite + 1)) {
        return;
    }
    uint16_t p = rom_word(g->rom, B7, (uint16_t)(0xE25C + (uint8_t)((M(EntAnimId + 1) - 0x9C) << 1)));
    weapon_apply_offsets(g, p, (uint8_t)((M(EntAnimFrame + 1) << 1) - 2));
    /* $E342: only for an entity 5 of type $73 (not in the intro) */
}
