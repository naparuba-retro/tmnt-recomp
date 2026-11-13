/* Enemy waves and wave triggers (B6 $9C1D-$9E10). */
#include "game.h"
#include "anim.h"
#include "enemy.h"
#include "hal.h"
#include "persist.h"
#include "player.h"
#include "sound.h"
#include "stage.h"
#include "stage_play.h"
#include "stage_play/spawn_list.h"
#include "stage_play/waves.h"
#define TODO_AREA "stage_play" /* prefix of the TODO traces of this file */

static void wave_state3_end(Game *g);

static void wave_end(Game *g) /* $9DC8 / $9E10: the wave fight is over */
{
    M(WaveState) = 0;
    M(WaveActive) = 0;
}

static void wave_state2(Game *g) /* B6 $9DA8: waits for the end of the enemies, then frees the camera */
{
    if (enemy_present(g)) {
        return;
    }
    if (!M(WaveTimer)) {
        sound_play(g, 0x77);
        M(WaveTimer) = 0x40;
    }
    if (--M(WaveTimer)) {
        return;
    }
    M(WaveTriggersUsed) |= R7(0xF137 + M(0xAF));
    persist_note_wave(g, R7(0xF137 + M(0xAF)));
    wave_end(g);
    restart_level_music(g);
}

static void wave_state3(Game *g) /* B6 $9DD1: end of a boss fight */
{
    if (enemy_present(g)) {
        return;
    }
    if (!M(WaveTimer)) {
        if (M(LevelNumber) == 4) {
            wave_state3_end(g);
            return;
        } /* $9DDF -> $9E10: no wait in level 5 */
        int init = 1;
        if (M(LevelNumber) == 2) {
            init = (--M(0x03E8) == 0);
        }
        if (init) {
            sound_init(g);
        }
        sound_play(g, 0x42);
        M(WaveTimer) = 0x20;
    }
    for (unsigned iterations = 0;; iterations++) { /* $9DF9 */
        LOOP_GUARD(iterations)
        if (--M(WaveTimer)) {
            if (M(WaveTimer) != 0x80) {
                return;
            }
            break;
        }
        if (M(WaveTriggerId) != 0) {
            break;
        }
        sound_play(g, 0x77);
        M(WaveTimer) = 0xC0;
    }
    wave_state3_end(g);
}

static void wave_state3_end(Game *g) /* B6 $9E10 */
{
    wave_end(g);
    if (M(TurtleHealth0 + M(CurrentTurtle))) {
        M(WaveDoneMask) |= R7(0xF137 + M(WaveTriggerId));
        ROM_TABLE(b5, 8, 6, 0x9E58); /* $9E58 */
        M(SceneId) = b5[M(WaveTriggerId)];
        if (M(SceneId) == 9) {
            return;
        }
        if (M(SceneId) == 8) {
            clear_entity(g, 13);
        }
        if (M(WaveTriggerId) == 0 || M(WaveTriggerId) == 2) {
            restart_level_music(g);
            return;
        }
        M(0x56) = 0;
        if (M(LevelNumber) < 4) {
            sound_play(g, 0x7F);
        }
    }
}

static void wave_f5(Game *g) /* B6 $9D32: the six elements of the boss of the vertical zone, table $9D84 (Y, X, type, life, animation, flags) */
{
    uint8_t status = rdmem(g, (uint16_t)(ptr(g, SpawnListPtrLo) + M(SavedY) + 2));
    uint8_t y = 0;
    for (int x = 5; y < 0x24; x++, y = (uint8_t)(y + 6)) {
        M(EntStatus + x) = status;
        M(EntY + x) = R6(0x9D84 + y);
        M(EntX + x) = R6(0x9D85 + y);
        if (x == 5) {
            M(CameraSpeedX) = R6(0x9D85 + y);
        }
        M(EntType + x) = R6(0x9D86 + y);
        M(EntHealth + x) = R6(0x9D87 + y);
        if (R6(0x9D88 + y)) {
            set_animation(g, x, R6(0x9D88 + y));
        }
        M(EntFlags + x) = R6(0x9D89 + y);
    }
    M(WaveState) += 2;
}

/* B6 $9C9E: the wave trigger has just been activated; y = index of the entry in the list ($42) */
static int wave_trigger(Game *g, uint8_t y) {
    uint8_t n = y + 5;
    uint8_t b = rdmem(g, ptr(g, SpawnListPtrLo) + n);
    int special = 0;
    if (b != 0xFF && (b & 0xF0) == 0xF0) {
        uint8_t x = b & 0x0F;
        if (R7(0xF137 + x) & M(WaveDoneMask)) {
            M(SavedY) = y;
            return 1;
        }
        special = x != 0;
    }
    if (!M(WaveState)) {
        sound_init(g);
        sound_play(g, special ? 0x7B : 0x73);
        M(WaveState) = 1;
        M(WaveTimer) = 8;
    }
    M(SavedY) = (uint8_t)(M(SavedY) + 2);
    if (enemy_present(g)) {
        return 0;
    }
    M(WaveActive) = 1;
    if (--M(WaveTimer)) {
        return 0;
    }
    uint8_t ny = (uint8_t)(M(SavedY) + 3);
    b = rdmem(g, ptr(g, SpawnListPtrLo) + ny);
    if (b == 0xFF || (b & 0xF0) != 0xF0) { /* $9D2A */
        spawn_wave_enemy(g);
        M(WaveState) = 2;
        return 0;
    }
    M(WaveTriggerId) = b & 7;
    uint8_t i = (uint8_t)(M(WaveTriggerId) << 1);
    M(WaveSpriteSet) = R6(0x9E5F + i);
    set_chr_bank0(g, R6(0x9E60 + i));
    if (b == 0xF5) {
        wave_f5(g);
        return 0;
    }
    wave_palette(g);
    spawn_wave_boss(g); /* $82C9 */
    M(WaveState) = 3;
    return 0;
}

/* B6 $9C1D: enemy wave triggers ($AE = triggers already used) */
void side_wave_triggers(Game *g) {
    if (M(WaveState) == 2) {
        wave_state2(g);
        return;
    }
    if (M(WaveState) == 3) {
        wave_state3(g);
        return;
    }
    uint16_t t = rom_word(g->rom, 6, (uint16_t)(0x9E6D + M(LevelNumber) * 2));
    uint16_t p = rom_word(g->rom, 6, (uint16_t)(t + M(AreaNumber) * 2));
    M(SpawnListPtrLo) = (uint8_t)p;
    M(SpawnListPtrHi) = (uint8_t)(p >> 8);
    M(0xAF) = 0xFF;
    uint8_t y = 0xFB;
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        y = (uint8_t)(y + 5);
        M(0xAF)++;
        if (M(WaveTriggersUsed) & R7(0xF137 + M(0xAF))) {
            continue;
        }
        uint8_t a = rdmem(g, ptr(g, SpawnListPtrLo) + y);
        if (a != 0xFF && (a & 0xF0) == 0xF0) {
            y++;
        }
        M(SavedY) = y;
        a = rdmem(g, ptr(g, SpawnListPtrLo) + y);
        if (a == 0xFF) {
            return;
        }
        if (a != M(CameraYScreen)) {
            continue;
        }
        uint8_t h = rdmem(g, ptr(g, SpawnListPtrLo) + y + 1);
        if (h == 0) {
            if (M(CameraXHi) != 0 || M(CameraXLo) != 0) {
                continue;
            }
        } else if ((uint8_t)(h - 1) != M(CameraXHi) || M(CameraXLo) < 0xFB || M(CameraXLo) >= 0xFE) {
            continue;
        }
        if (wave_trigger(g, y)) {
            continue; /* 1 : the scan resumes with the next entry */
        }
        return;
    }
}
