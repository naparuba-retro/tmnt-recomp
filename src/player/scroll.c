/* Scrolling triggered by the player and vertical screen changes (pits). */
#include "game.h"
#include "enemy.h"
#include "hal.h"
#include "player.h"
#include "sound.h"
#include "stage.h"
#include "stage_map.h"
#include "player/scroll.h"
#include "player/tiles.h"
#define TODO_AREA "player" /* prefix of the TODO traces of this file */

/* --- horizontal scrolling triggered by the player (B4 $9851-$98A0) --- */
static uint8_t camera_next_hi(Game *g, uint8_t add_hi) /* $9B6B / $9BAD: high byte of the camera after the step */
{
    unsigned s1 = M(CameraXFrac) + M(EntVelXFrac);
    unsigned s2 =
        M(CameraXLo) + M(EntVelX) + (add_hi == 1 ? 0 : (s1 > 0xFF)); /* $9B6B (right): a CLC before the addition, the carry of the fractions is lost */
    return (uint8_t)(M(CameraXHi) + add_hi + (s2 > 0xFF));
}

static uint8_t next_screen_column(Game *g) /* B4 $9B82: byte of the level map at the next column */
{
    M(TmpA) = M(CameraYLo);
    M(TmpB) = M(CameraYHi);
    split_c0(g);
    uint8_t width = rdmem(g, ptr(g, 0x49)), sum = 0;
    for (int n = M(MapYScreen); n > 0; n--) {
        sum = (uint8_t)(sum + width);
        if (!sum) {
            break;
        }
    }
    M(Tmp0) = sum;
    uint8_t y = (uint8_t)(sum + M(Tmp10) + 2);
    return rdmem(g, (uint16_t)(ptr(g, 0x49) + y));
}

static void scroll_speed_limit(Game *g) /* B4 $9BBF: scroll speed ($C0/$C1) */
{
    uint8_t a = M(EntVelX);
    if (a & 0x80) {
        if (a < 0xFC) {
            M(EntVelXFrac) = 0;
            a = 0xFC;
        }
    } else if (a >= 4) {
        M(EntVelXFrac) = 0;
        a = 4;
    }
    M(CameraSpeedX) = a;
    M(CameraSpeedXFrac) = M(EntVelXFrac);
}

void player_scroll_trigger(Game *g) /* B4 $9851 */
{
    if (!(M(EntVelX) | M(EntVelXFrac))) {
        return;
    }
    if (M(EntVelX) & 0x80) {
        if (M(EntX) >= 0x60) {
            return;
        }
        M(Tmp10) = camera_next_hi(g, 0xFF);
        if ((M(Tmp10) & 0x80) && !M(CameraXLo)) {
            return;
        }
        if (next_screen_column(g) == 0xFF || M(WaveState)) {
            return;
        }
        scroll_speed_limit(g);
        M(ScrollDirX) = 1;
    } else {
        if (M(EntX) < 0xA0) {
            return;
        }
        M(Tmp10) = camera_next_hi(g, 1);
        if (M(Tmp10) >= rdmem(g, ptr(g, 0x49))) {
            return;
        }
        if (next_screen_column(g) == 0xFF || M(WaveState)) {
            return;
        }
        scroll_speed_limit(g);
        M(ScrollDirX) = 2;
    }
}

/* --- vertical screen change in side view (ladder at the top or bottom of the screen): B4 $9A73-$9B13 --- */
static void camera_y_step(Game *g, int up) /* B4 $9AF3 / $9B07: 2 pixels, twice in a row for 4 */
{
    for (int n = 0; n < 2; n++) {
        if (up) {
            M(CameraDeltaY) = (uint8_t)(M(CameraDeltaY) - 2);
            M(CameraYLo) = (uint8_t)(M(CameraYLo) - 2);
            if ((uint8_t)(M(CameraYLo) + 2) == 0) {
                M(CameraYHi)--;
            }
        } else {
            M(CameraDeltaY) = (uint8_t)(M(CameraDeltaY) + 2);
            M(CameraYLo) = (uint8_t)(M(CameraYLo) + 2);
            if (M(CameraYLo) == 0) {
                M(CameraYHi)++;
            }
        }
    }
}

/* B4 $99A7: exit through the bottom in the buildings of levels 3 and 4; carry = a fall state ($16 / $17) took over */
static int pit_fall(Game *g, int water) {
    for (int x = 1; x < 16; x++) {
        clear_entity(g, x); /* $99E8 / $9A28 : EF5E */
    }
    clear_vel_x(g, 0);
    clear_vel_y(g, 0);
    if (!water) { /* $99F2: fall into the void */
        M(EntVelX) = 1;
        M(EntVelXFrac) = 0x80;
        M(WaveState) = 1;
        M(EntState) = 0x16;
        M(DelayTimerLo) = 1;
        sound_play(g, 0x2C);
        return 1;
    }
    M(EntState) = 0x17; /* $9A2D: fall into the water */
    M(EntSprite) = 0;
    M(EntAnimId) = 0;
    M(DelayTimerLo) = 0x38;
    sound_play(g, 0x29);
    return 1;
}

static int pit_tile_open(Game *g) /* $9A4A: the tile under the player is empty or of type 2 */
{
    uint8_t t = tile_near_player(g, 0x16);
    return t == 0 || t == 2;
}

static int level_pit(Game *g) {
    if (M(LevelNumber) == 3) { /* $99C6 */
        if (M(AreaNumber) | M(CameraYLo) | M(CameraYHi)) {
            return 0;
        }
        if (M(CameraYHi) || !pit_tile_open(g)) {
            return 0;
        }
        return pit_fall(g, 1);
    }
    if (M(LevelNumber) != 2) {
        return 0;
    }
    switch (M(AreaNumber)) {
        case 1:
            return M(CameraYHi) == 1 ? pit_fall(g, 0) : 0; /* $99D3 */
        case 2:
            return M(CameraYHi) == 2 ? pit_fall(g, 0) : 0; /* $99DA */
        case 5:
            return M(CameraYLo) == 0xC0 ? pit_fall(g, 0) : 0; /* $99E2 */
        case 3:
        case 4: /* $9A1F */
            if (M(CameraYHi) || !pit_tile_open(g)) {
                return 0;
            }
            return pit_fall(g, 1);
        case 6: /* $9A10 */
            if (M(CameraYHi) || !pit_tile_open(g)) {
                return 0;
            }
            if (M(WaveState)) { /* $9A2D: the entities are not cleared */
                clear_vel_x(g, 0);
                clear_vel_y(g, 0);
                M(EntState) = 0x17;
                M(EntSprite) = 0;
                M(EntAnimId) = 0;
                M(DelayTimerLo) = 0x38;
                sound_play(g, 0x29);
                return 1;
            }
            return pit_fall(g, 1);
        default:
            return 0;
    }
}

/* B4 $9B14: level 3, the arrow palette blinks in the hole screen */
void level3_pit_blink(Game *g) {
    ROM_TABLE2(cam, 3, 2, 4, 0x9B65); /* $9B65 */
    int i;
    switch (M(AreaNumber)) {
        case 1:
            i = 0;
            break;
        case 2:
            i = 1;
            break;
        case 5:
            i = 2;
            break;
        default:
            return;
    }
    if (M(CameraYHi) == cam[i][0] && M(CameraYLo) == cam[i][1] && (M(FrameCounter) & 8)) {
        M(PaletteBuffer + 10) = 0x31;
        M(PaletteBuffer + 11) = 0x11;
    } else {
        M(PaletteBuffer + 10) = 0x11;
        M(PaletteBuffer + 11) = 0x31;
    }
    if (M(FrameCounter) & 6) {
        return;
    }
    M(PaletteDirty) = 1; /* $F778 */
}

/* B4 $9906-$9963: the player leaves through the top or bottom of the screen (ladder); carry = the vertical screen change starts */
int side_screen_edge(Game *g) {
    uint8_t y = M(EntY);
    if (y < 0x0B) {
        M(EntY) = y = 0x0B;
        if ((M(JoyHeld1) & 0x08) && !(M(EntFlags) & 0x10)) { /* $9918: lda $0450 / and #$10 (entity flags, not its status) */
            if (M(WaveState)) {
                return 0;
            }
            if (M(CameraYHi) | M(CameraYLo)) {
                if (!(M(EntFlags) & 0x80)) {
                    return 0;
                }
                M(ScreenChangeDir) = 1;
                for (int x = 1; x < 16; x++) {
                    clear_entity(g, x);
                }
                return 1;
            }
        }
    }
    if (y < 0xAC) {
        return 0;
    }
    M(EntY) = 0xAC;
    uint8_t s = M(EntState);
    if (s == 0x16 || s == 0x17 || s == 0x18 || s == 0x09) {
        return 0;
    }
    if (level_pit(g)) {
        return 0; /* $994E: carry = fall, no screen change */
    }
    if (M(WaveState) || !(M(EntFlags) & 0x80)) {
        return 0;
    }
    M(ScreenChangeDir) = 2;
    for (int x = 1; x < 16; x++) {
        clear_entity(g, x);
    }
    return 1;
}

void side_vertical_scroll(Game *g) {
    if (!M(VerticalScrollMode)) { /* $9A73: start, $0607 = 1 (upward) or 2 (downward) */
        M(VerticalScrollMode) = M(ScreenChangeDir);
        M(VerticalScrollTimer) = 0x60;
        M(EntFlags) &= 0xEF;
        M(ScreenChangeDir) = 0;
        return;
    }
    if (M(0x58)) { /* $9A87 */
        M(0x58) = 0;
        set_chr_bank0(g, M(WaveSpriteSet));
        wave_palette(g);
    }
    M(EntAnimTimer)++;
    M(VerticalScrollTimer) = (uint8_t)(M(VerticalScrollTimer) - 2);
    if (M(VerticalScrollTimer) == 0xFE) { /* $9AA3: end of the scroll */
        if (M(VerticalScrollMode) == 1) {
            M(EntY) = (uint8_t)(M(EntY) - 2);
        }
        M(VerticalScrollMode) = M(WaterCurrent) = 0;
        M(0x65) = 1;
        M(StageIntroTimer) = 4;
        clear_object_slots(g);
        spawn_initial_side(g); /* $CF3D -> B6 $85CC */
        return;
    }
    int up = M(VerticalScrollMode) == 1;
    camera_y_step(g, up);
    if (M(VerticalScrollTimer) < 0x50) {
        M(EntY) = (uint8_t)(M(EntY) + (up ? 4 : -4));
    }
}
