/* Entity animation engine (bank 6 $8001-$80F5).
 * Animation script: a list of sprite indices; $FF = end (loop), $FE = stop, $FD = jump to another script. */
#include "game.h"
#include "anim.h"

static uint16_t script_ptr(Game *g, int x) /* B6 $8081 GetAnimScriptPtr */
{
    uint8_t id = M(EntAnimId + x), y = (uint8_t)(id << 1);
    if (M(EntType + x) < 0x80) {
        if (id & 0x80) {
            return rom_word(g->rom, B6, (uint16_t)(0x8BC5 + y));
        }
        return rom_word(g->rom, B6, (uint16_t)(0x8CC5 + y));
    }
    if (id & 0x80) {
        return rom_word(g->rom, B6, (uint16_t)(0x8E11 + y));
    }
    return rom_word(g->rom, B6, (uint16_t)(0x8DC5 + y));
}

static uint16_t duration_base(Game *g, int x, uint8_t id) {
    if (M(EntType + x) < 0x80) {
        return (id & 0x80) ? 0x9305 : 0x9405;
    }
    return (id & 0x80) ? 0x9445 : 0x9485;
}

void animate_entity(Game *g, int x) /* B6 $8010 */
{
    if (M(EntAnimId + x) == 0) {
        return;
    }
    if (--M(EntAnimTimer + x) == 0) {
        uint8_t id = M(EntAnimId + x); /* GetAnimDuration: index = full id */
        M(EntAnimTimer + x) = rom_prg(g->rom, B6, (uint16_t)(duration_base(g, x, id) + id));
        M(EntAnimFrame + x)++;
    }
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        uint16_t p = script_ptr(g, x);
        uint8_t y = M(EntAnimFrame + x);
        if (y & 0x80) {
            M(EntAnimFrame + x) = 0;
            y = 0;
        }
        uint8_t v = rom_prg(g->rom, B6, (uint16_t)(p + y));
        if (v == 0xFE) {
            M(EntAnimFrame + x) = 0;
            return;
        }
        if (v == 0xFF) {
            M(EntAnimFrame + x) = 0xFF;
            return;
        }
        if (v != 0xFD) {
            M(EntSprite + x) = v;
            return;
        }
        /* $FD: new script = next byte; duration read with (id & $7F) as index (code at $8047),
         * then resume at $8021 with the same frame index. */
        uint8_t id = rom_prg(g->rom, B6, (uint16_t)(p + y + 1));
        M(EntAnimId + x) = id;
        M(EntAnimTimer + x) = rom_prg(g->rom, B6, (uint16_t)(duration_base(g, x, id) + (id & 0x7F)));
    }
}

void set_animation(Game *g, int x, uint8_t id) /* B6 $8081 SetAnimation */
{
    M(EntAnimId + x) = id;
    M(EntAnimFrame + x) = 0;
    M(EntAnimTimer + x) = rom_prg(g->rom, B6, (uint16_t)(duration_base(g, x, id) + id));
}

void animate_entities_from(Game *g, int first) /* B6 $8001 (5) / $8005 (0) */
{
    for (int x = first; x < 16; x++) {
        animate_entity(g, x);
    }
}
