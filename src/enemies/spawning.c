/* Creation of enemies from the spawn list (B6 $8146-$82C8). */
#include "game.h"
#include "anim.h"
#include "enemy.h"
#include "hal.h"
#include "player.h"
#include "stage.h"
#include "spawn_mask.h"
#include "stage_play.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

static int spawn_at(Game *g, uint8_t y0, int mode);
static void wave_switch_bank(Game *g);

/* B6 $8277-$82C7: hit points, animation and orientation of an entity whose EntType has just been set; carry = cancelled (obstacle) */
int init_entity_type_tail(Game *g, int x, uint8_t t) {
    if (t & 0x80) { /* $8288 */
        uint8_t i = (uint8_t)((t & 0x7F) << 1);
        M(EntHealth + x) = R6(0x883D + i);
        set_animation(g, x, R6(0x883E + i));
    } else { /* $8277 */
        uint8_t i = (uint8_t)(t << 1);
        M(EntHealth + x) = R6(0x8749 + i);
        set_animation(g, x, R6(0x874A + i));
    }
    M(EntFlags + x) = (M(EntX + x) & 0x80) ? 0xC3 : 0xC2; /* $8298 */
    if (!M(WaveActive) && !M(0x03D7)) {
        uint8_t keep = M(SavedY);
        if (tile_at_offset(g, x, 0, 0xF8) == 4) { /* on an obstacle: cancelled */
            clear_entity(g, x);
            M(SavedY) = keep;
            return 1;
        }
        M(SavedY) = keep;
    }
    return 0;
}

/* ---------------------------------------------------------------- creation (B6 $8146) --- */
/* y = position of the entry in the spawn list ($42); top view only */
int spawn_enemy(Game *g, uint8_t y0) {
    uint8_t y = (uint8_t)(y0 + 2);
    uint8_t code = rdmem(g, (uint16_t)(ptr(g, SpawnListPtrLo) + y));
    if (code < 0x10) {
        return spawn_at(g, y0, 1); /* $8150: object (pizza...), entities 14 and 15 ($EEE0) */
    }
    if (!M(SideViewFlag)) {
        M(WaveSpriteSet) = 0x10;
    } else if (M(WaveSpriteSet) != 7) { /* $816A: waves according to the score ($C5/$C6) */
        if (M(WaveBankChangePending)) { /* $81EF: the wave changes the bank when the enemies have disappeared */
            int present = 0;
            for (int i = 5; i < 16; i++) {
                if (M(EntType + i) >= 0x10) {
                    present = 1;
                }
            }
            if (!present) {
                wave_switch_bank(g);
            }
        } else { /* $8174: score thresholds */
            int n = -1;
            for (uint8_t i = 0; i < 0x10; i += 2) {
                if (M(0xC6) < R6(0x838B + i)) {
                    break;
                }
                if (M(0xC6) == R6(0x838B + i) && M(LevelScoreBcd) < R6(0x838C + i)) {
                    break;
                }
                n++;
            }
            if (n >= 0) { /* $819D */
                uint8_t cnt = 0, yy = (uint8_t)(n * 8);
                for (int k = 0; k < 4; k++, yy = (uint8_t)(yy + 2)) {
                    uint8_t hi = R6(0x839B + yy), lo = R6(0x839C + yy);
                    if (M(ElapsedTimeHi) < hi || (M(ElapsedTimeHi) == hi && M(ElapsedTimeLo) < lo)) {
                        cnt++;
                    } else {
                        break;
                    }
                }
                if (cnt != M(0x0614)) {
                    M(0x0614) = cnt;
                    M(WaveBankChangePending) = 1;
                    uint8_t a = M(EntHealth);
                    if (a < 0x40) {
                        M(0x0614)--;
                        if (!(M(0x0614) & 0x80)) {
                            if (a < 0x20) {
                                M(0x0614)--;
                                if (M(0x0614) & 0x80) {
                                    M(0x0614)++;
                                }
                            }
                        } else {
                            M(0x0614)++;
                        }
                    }
                }
            }
        }
    }
    return spawn_at(g, y0, 0);
}

/* B6 $81F4-$8222: sprite bank and palette of the current wave */
static void wave_switch_bank(Game *g) {
    uint16_t t = rom_word(g->rom, 6, (uint16_t)(0x82EB + M(LevelNumber) * 2));
    uint16_t p = rom_word(g->rom, 6, (uint16_t)(t + M(AreaNumber) * 2));
    M(WaveSpriteSet) = R6(p + (M(0x0614) & 3));
    set_chr_bank0(g, M(WaveSpriteSet));
    wave_palette(g);
    M(WaveBankChangePending) = 0;
}

int spawn_wave_enemy(Game *g) {
    wave_switch_bank(g);
    return spawn_at(g, M(SavedY), 0);
}

int spawn_wave_boss(Game *g) /* B6 $82C9 */
{
    return spawn_at(g, M(SavedY), 2);
}

/* B6 $8225: creates the enemy described by entry y0 of the list ($42) */
static int spawn_at(Game *g, uint8_t y0, int mode) /* mode 0: normal, 2: $82C9 (X read from the list, relative to the camera) */
{
    uint8_t y = (uint8_t)(y0 + 2);
    uint8_t code = rdmem(g, (uint16_t)(ptr(g, SpawnListPtrLo) + y));
    if (mode != 2 && spawn_mask_active() && code < 0xF0 && spawn_masked((uint16_t)(ptr(g, SpawnListPtrLo) + y0), M(WaveSpriteSet))) {
        /* Whole Pizza, spawn mask: this spawn is cut. After the wave logic of spawn_enemy (which ran as before), before anything is created. The bosses
         * ($70-$7F once the table of the sprite set is read) are never cut. */
        uint8_t type = code < 0x10 ? code : rdmem(g, (uint16_t)(rom_word(g->rom, 6, (uint16_t)(0x86E9 + M(WaveSpriteSet) * 2)) + (code & 7)));
        if ((type & 0xF0) != 0x70) {
            return SPAWN_SKIPPED;
        }
    }
    uint8_t x = mode == 1 ? 0x0E : 5; /* B7 $EEF2 FindFreeEntitySlot: entities 5 to 10 ($EEE0: 14 and 15) */
    uint8_t end = mode == 1 ? 0x10 : 0x0B;
    while (M(EntType + x)) {
        if (++x == end) {
            return -1;
        }
    }
    if (mode != 2) {
        clear_entity(g, x); /* $82C9 skips the $EF67 of $822B: the entity keeps its old values */
    }
    if (mode == 2) {
        M(EntX + x) = (uint8_t)(rdmem(g, (uint16_t)(ptr(g, SpawnListPtrLo) + y0)) - M(CameraXLo));
        M(EntY + x) = (uint8_t)(rdmem(g, (uint16_t)(ptr(g, SpawnListPtrLo) + y0 + 1)) - M(CameraYInScreen));
    } else if (M(WaveActive)) { /* $82DB: during a wave, absolute positions */
        M(EntX + x) = rdmem(g, (uint16_t)(ptr(g, SpawnListPtrLo) + y0));
        M(EntY + x) = rdmem(g, (uint16_t)(ptr(g, SpawnListPtrLo) + y0 + 1));
    } else {
        M(EntX + x) = (uint8_t)(M(SpawnLimitX) - M(CameraXLo));
        M(EntY + x) = (uint8_t)(rdmem(g, (uint16_t)(ptr(g, SpawnListPtrLo) + y0 + 1)) - M(CameraYInScreen));
    }
    uint8_t t = code;
    if (t < 0x10) {
        M(EntType + x) = t;
    } else {
        uint16_t tbl = rom_word(g->rom, 6, (uint16_t)(0x86E9 + M(WaveSpriteSet) * 2));
        M(EntStatus + x) = t;
        t = rdmem(g, (uint16_t)(tbl + (t & 7)));
        M(EntType + x) = t;
    }
    return init_entity_type_tail(g, x, t) ? -1 : x;
}

/* B6 $85CC: enemies of the start screen of a side-view zone */
void spawn_initial_side(Game *g) {
    uint8_t saved = g->prg_bank;
    g->prg_bank = 6;
    M(0x03D7) = 1;
    camera_screen_split(g);
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 1 && M(SpawnLimitX) == 7) {
            break; /* $85FE */
        }
        uint16_t t = rom_word(g->rom, 6, (uint16_t)(0x8681 + M(LevelNumber) * 2)); /* $8672 */
        M(Tmp0) = (uint8_t)t;
        M(Tmp0 + 1) = (uint8_t)(t >> 8);
        M(Tmp0 + 4) = pass ? 0xF8 : 0x07;
        M(Tmp0 + 5) = 0x17;
        spawn_position(g);
        M(SavedY) = 0;
        for (unsigned iterations = 0;; iterations++) {
            LOOP_GUARD(iterations)
            uint8_t c = rdmem(g, ptr(g, SpawnListPtrLo) + M(SavedY));
            if (c == 0xFF) {
                break;
            }
            if (pass == 0 ? c >= M(SpawnLimitX) : c < M(SpawnLimitX)) {
                int free_slot = spawn_enemy(g, M(SavedY));
                if (free_slot == SPAWN_SKIPPED) { /* cut by the spawn mask: the next entry */
                    M(SavedY) = (uint8_t)(M(SavedY) + 3);
                    continue;
                }
                if (free_slot < 0) {
                    if (pass == 0) {
                        break;
                    }
                    goto done;
                } /* carry: no room left */
                M(EntX + free_slot) = rdmem(g, ptr(g, SpawnListPtrLo) + M(SavedY)); /* $8634 */
                M(EntY + free_slot) = rdmem(g, ptr(g, SpawnListPtrLo) + M(SavedY) + 1);
            }
            M(SavedY) = (uint8_t)(M(SavedY) + 3);
        }
    }
done:
    M(0x03D7) = 0;
    g->prg_bank = saved;
}
