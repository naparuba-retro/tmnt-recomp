/* End of the game (GameMode 5): B7 $C6B6 -> $D2A6 -> B4 $BB69, state machine $03C2 (11 states): picture, flashing palettes, "typewriter" texts. */
#include "game.h"
#include "anim.h"
#include "ending.h"
#include "hal.h"
#include "player.h"
#include "scenes.h"

static void clear_screen_buffers(Game *g) /* B7 $C97F: empty background, buffers $05C0 and $06C0 cleared */
{
    load_screen(g, 1, 0);
    for (int y = 0x3F; y >= 0; y--) {
        M(AttrBufferA + y) = 0;
        M(AttrBufferB + y) = 0;
    }
}

static int copy_block(Game *g, uint8_t a) /* B4 $BEA7: copies a block of the $BED5 table into the PPU buffer; carry = copied */
{
    uint8_t y = (uint8_t)(a << 1);
    if (M(SceneDelay)) {
        return 0;
    }
    uint16_t p = rom_word(g->rom, 4, (uint16_t)(0xBED5 + y)); /* pointer read in the ROM : US address converted back to EU */
    if (M(PpuBufferIndex) >= 0x30) {
        return 0;
    }
    uint8_t x = M(PpuBufferIndex);
    for (uint8_t v; (v = R4(p)) != 0xFF; p++) {
        M(PpuBuffer + x++) = v;
    }
    M(PpuBufferIndex) = x;
    return 1;
}

static int script_step(Game *g, uint8_t a) /* B4 $BCAC: list of texts and blocks ($BD03); carry = list finished */
{
    uint8_t y = (uint8_t)(a << 1);
    M(SceneDelay) = 0;
    if (!M(SceneFrameCounter)) {
        uint16_t list = rom_word(g->rom, 4, (uint16_t)(0xBD03 + y));
        uint8_t v = R4(list + M(SceneListIndex));
        if (v == 0xFF) {
            M(SceneListIndex) = 0;
            return 1;
        }
        if (v != 0xFE) {
            int done;
            if (v & 0x80) { /* $BD1B: fill block */
                uint8_t k = (uint8_t)(v << 1);
                done = ppu_fill_script(g, rom_word(g->rom, 4, (uint16_t)(0xBD2A + k)));
            } else { /* $BD42: text */
                uint8_t k = (uint8_t)(v << 1);
                done = ppu_text_script(g, rom_word(g->rom, 4, (uint16_t)(0xBD51 + k)));
            }
            if (done) {
                M(SceneListIndex)++;
            }
            return 0;
        }
    }
    M(SceneDelay) = 1; /* $BCEC : pause */
    if (++M(SceneFrameCounter) >= 0x60) {
        M(SceneFrameCounter) = 0;
        M(SceneListIndex)++;
    }
    return 0;
}

static void next_after_script(Game *g, uint8_t a) /* B4 $BBCD */
{
    if (script_step(g, a)) {
        M(SceneState)++;
        M(SceneTimerB) = 0x40;
    }
}

void ending_run(Game *g) /* B4 $BB69 */
{
    g->prg_bank = 4; /* D2A6: bank 4 is visible at $8000 */
    switch (M(SceneState)) {
        case 0: /* $BB85 */
            for (int y = 7; y > 0; y--) {
                M(SceneState + y) = 0;
            }
            M(PpuTextBusy) = M(SceneTimerB) = M(SceneTimerA) = 0;
            M(SceneState) = 1;
            load_palette_preset(g, 0x10);
            set_chr_bank1_raw(g, 0x1F);
            load_screen(g, 2, 0x20);
            for (int x = 0; x < 16; x++) {
                clear_entity(g, x);
            }
            animate_entities_from(g, 0);
            break;
        case 1: /* $BBB5 */
            M(SceneTimerB)++;
            if (M(SceneTimerB) & 0x0F) {
                copy_block(g, (M(SceneTimerB) & 0x10) ? 1 : 0);
            }
            next_after_script(g, 0);
            break;
        case 2: /* $BBDB */
            if (M(SceneTimerB)) {
                M(SceneTimerB)--;
                fill_palette_backdrop(g, (M(SceneTimerB) & 4) ? 0x16 : 0x0F);
            } else {
                M(SceneTimerB) = 0x70;
                M(PpuTextBusy) = 0;
                M(SceneState)++;
                fill_palette_backdrop(g, 0x0F);
            }
            break;
        case 3: {                                                 /* $BC00 */
            static const uint8_t t[8] = {7, 6, 5, 4, 3, 2, 0, 0}; /* $BC1F */
            if (--M(SceneTimerB) == 0) {
                M(SceneState)++;
                M(PpuTextBusy) = 0;
                M(SceneTimerB) = 0;
                break;
            }
            copy_block(g, t[M(SceneTimerB) >> 4]);
            break;
        }
        case 4: /* $BC26 */
            M(SceneTimerA)++;
            if (M(SceneTimerA) & 0x0F) {
                copy_block(g, (uint8_t)(((M(SceneTimerA) & 0x10) ? 1 : 0) + 7));
            }
            next_after_script(g, 1);
            break;
        case 5: /* $BC44 */
            M(SceneState)++;
            clear_screen_buffers(g);
            break;
        case 6: /* $BC4A */
            M(SceneState)++;
            load_screen(g, 2, 0x22);
            M(SceneTimerB) = 0;
            M(SceneTimerA) = 0;
            break;
        case 7: { /* $BC5D */
            M(SceneTimerA)++;
            copy_block(g, R4(0xBC9C + (M(SceneTimerA) >> 4)));
            next_after_script(g, 2);
            break;
        }
        case 8: /* $BC73 */
            clear_screen_buffers(g);
            M(SceneState)++;
            M(SceneTimerB) = 0xF0;
            M(SceneTimerA) = 2;
            load_palette_preset(g, 0x21);
            break;
        case 9: /* $BC88 */
            next_after_script(g, 3);
            break;
        case 10: /* $BC8D */
            if (--M(SceneTimerB)) {
                break;
            }
            if (--M(SceneTimerA)) {
                break;
            }
            M(GameMode) = 0;
            break;
        default:
            break;
    }
}
