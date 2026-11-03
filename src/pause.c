/* Pause / turtle selection screen (B3 $A91C-$AD8A, $B695, $B8C9) and pause (B7 $CDD6 CheckPause, $CE0E, $CE23),
 * game over and "continue" (B7 $CD1A-$CDBA). This screen serves both for the pause (Start) and after a turtle's death
 * ($0617 = 1): the level is then reloaded (StageState = 3 - $0617).
 * Paths not ported: levels with special counters ($A7, $BF), manual pause (messages of $ADF4), turtle change. */
#include "game.h"
#include "hal.h"
#include "intro.h"
#include "nmi.h"
#include "pause.h"
#include "player.h"
#include "sound.h"
#define TODO_AREA "pause" /* prefix of the TODO traces of this file */

#define HEALTH(i) M(TurtleHealth0 + (i))

static void app(Game *g, uint8_t v) {
    ppu_buffer_append(g, v);
}

/* B7 $C742: A (< 100) -> Tmp1 = tens, Tmp0 = units; a zero digit becomes $0A (the "0" tile) */
static void to_digits(Game *g, uint8_t a) {
    M(Tmp0) = 0;
    M(Tmp0 + 1) = 0;
    while (a >= 10) {
        a = (uint8_t)(a - 10);
        M(Tmp0 + 1)++;
    }
    M(Tmp0) = a ? a : 0x0A;
    if (!M(Tmp0 + 1)) {
        M(Tmp0 + 1) = 0x0A;
    }
}

/* B3 $AC77: score / counter rows, every frame (levels with counters: $A7, $BF) */
static void draw_counters(Game *g) {
    M(Tmp8) = M(Tmp9) = M(TmpA) = M(TmpB) = 0;
    M(Tmp0) = M(Tmp0 + 1) = 0;
    uint8_t lvl = M(LevelNumber);
    if (R3(0xAD8B + lvl)) {
        M(Tmp8) = 0x34;
        M(Tmp9) = 0x36;
        M(TmpA) = 0x35;
        M(TmpB) = 0x37;
        to_digits(g, M(RopeCount));
        if (!M(RopeCount)) {
            goto second;
        }
    }
    app(g, 2);
    app(g, R3(0xADA1));
    app(g, R3(0xADA2));
    app(g, M(Tmp8));
    app(g, M(Tmp9));
    app(g, 2);
    app(g, R3(0xADA3));
    app(g, R3(0xADA4));
    app(g, M(TmpA));
    app(g, M(TmpB));
    app(g, 2);
    app(g, R3(0xADA5));
    app(g, R3(0xADA6));
    app(g, M(Tmp0 + 1));
    app(g, M(Tmp0));
second:
    M(Tmp0) = M(Tmp0 + 1) = 0;
    if (R3(0xAD92 + lvl)) {
        to_digits(g, M(SpecialShotCount));
        if (!M(SpecialShotCount)) {
            return;
        }
    }
    app(g, 2);
    app(g, R3(0xADA7));
    app(g, R3(0xADA8));
    app(g, M(Tmp0 + 1));
    app(g, M(Tmp0));
    if (R3(0xAD92 + lvl)) { /* $AD59: two sprites (counter icon) in place of the two zeros */
        uint8_t y = M(SideViewFlag) ? 4 : 0;
        M(OamBuffer + 0x32 + y) = R3(0xAD99 + y);
        M(OamBuffer + 0x31 + y) = R3(0xAD9A + y);
        M(OamBuffer + 0x36 + y) = R3(0xAD9B + y);
        M(OamBuffer + 0x35 + y) = R3(0xAD9C + y);
        M(OamBuffer + 0x30 + y) = M(OamBuffer + 0x34 + y) = 0xAA;
        M(OamBuffer + 0x33 + y) = 0xD0;
        M(OamBuffer + 0x37 + y) = 0xD8;
        return;
    }
    app(g, 2);
    app(g, R3(0xADA9));
    app(g, R3(0xADAA));
    app(g, 0);
    app(g, 0); /* $CABC: two zeros */
    app(g, 2);
    app(g, R3(0xADAB));
    app(g, R3(0xADAC));
    app(g, 0);
    app(g, 0);
}

/* B3 $AC14: draws the crossed-out portrait of a dead turtle (3 rows of 3 tiles); carry = done or already done */
static int draw_dead_portrait(Game *g, uint8_t lo, uint8_t hi, uint8_t mask) {
    M(Tmp0 + 7) = mask;
    M(Tmp0 + 1) = lo;
    M(Tmp0 + 2) = hi;
    if (M(0x03DA) & mask) {
        return 1;
    }
    if (M(PpuBufferIndex) >= 0x2D) {
        return 0;
    }
    uint8_t x = M(PpuBufferIndex), y = 0;
    M(Tmp0 + 3) = 3;
    while (M(Tmp0 + 3)) {
        M(PpuBuffer + x) = 3;
        M(PpuBuffer + (uint8_t)(x + 1)) = M(Tmp0 + 2);
        M(PpuBuffer + (uint8_t)(x + 2)) = M(Tmp0 + 1);
        for (int k = 0; k < 3; k++) {
            M(PpuBuffer + (uint8_t)(x + 3 + k)) = R3(0xAC6E + y++);
        }
        x = (uint8_t)(x + 6);
        M(Tmp0 + 1) = (uint8_t)(M(Tmp0 + 1) + 0x20);
        M(Tmp0 + 3)--;
    }
    M(PpuBufferIndex) = x;
    M(0x03DA) |= mask;
    return 1;
}

static int draw_dead_portraits(Game *g) /* B3 $ABC6: carry = all drawn */
{
    if (!HEALTH(0)) {
        if (!draw_dead_portrait(g, 0x95, 0x20, 1)) {
            return 0;
        }
    }
    if (!HEALTH(1)) {
        if (!draw_dead_portrait(g, 0x15, 0x21, 2)) {
            return 0;
        }
    }
    if (!HEALTH(2)) {
        if (!draw_dead_portrait(g, 0x95, 0x21, 4)) {
            return 0;
        }
    }
    if (!HEALTH(3)) { /* $AC03: the last one is drawn without a test (falls through) */
        if (!draw_dead_portrait(g, 0x15, 0x22, 8)) {
            return 0;
        }
    }
    return 1;
}

static void draw_health_bar(Game *g) /* B3 $AB03: 4 health segments of turtle $08 */
{
    uint8_t t = M(Tmp8);
    app(g, 6);
    app(g, R3(0xADAE + t * 2));
    app(g, R3(0xADAD + t * 2));
    M(Tmp0 + 1) = 4;
    uint8_t h = (uint8_t)(HEALTH(t) >> 1);
    M(Tmp0 + 3) = h;
    M(Tmp0 + 2) = h >> 4;
    uint8_t lo = M(Tmp0 + 3) & 0x0F;
    if (lo >= 9) {
        M(Tmp0 + 2)++;
        lo = 0;
    }
    M(Tmp0 + 3) = lo;
    do {
        uint8_t tile;
        if (M(Tmp0 + 2)) {
            M(Tmp0 + 2)--;
            tile = 0x30;
        } else if (M(Tmp0 + 3)) {
            M(Tmp0 + 3) = 0;
            tile = 0x31;
        } else {
            tile = 0x32;
        }
        app(g, tile);
    } while (--M(Tmp0 + 1));
}

static void draw_ammo(Game *g) /* B3 $AB5B */
{
    M(Tmp0) = M(Tmp0 + 1) = 0x0F;
    uint8_t n = M(SubWeaponAmmo0 + M(Tmp8));
    if (n) {
        to_digits(g, n);
    }
    app(g, M(Tmp0 + 1));
    app(g, M(Tmp0));
}

static void draw_weapon_icon(Game *g) /* B3 $AB75 */
{
    uint8_t t = M(Tmp8);
    app(g, 2);
    app(g, R3(0xADB6 + t * 2));
    app(g, R3(0xADB5 + t * 2));
    M(TmpA) = (uint8_t)(M(SubWeaponType0 + t) << 2);
    app(g, R3(0xADBD + M(TmpA)));
    app(g, R3(0xADBE + M(TmpA)));
    app(g, 2);
    app(g, R3(0xADB6 + t * 2));
    app(g, (uint8_t)(R3(0xADB5 + t * 2) + 0x20));
    app(g, R3(0xADBF + M(TmpA)));
    app(g, R3(0xADC0 + M(TmpA)));
}

static void set_turtle_text(Game *g, uint8_t t) /* B3 $AAC2 */
{
    load_palette_preset(g, R7(0xD1DF + t * 2));
    print_text(g, R7(0xD1DE + t * 2));
}

/* B3 $B91E: Tmp1:Tmp0 = (x:y) * Tmp3 (repeated addition) */
static void mul16(Game *g, uint8_t y, uint8_t x) {
    M(Tmp0) = y;
    M(Tmp0 + 1) = x;
    M(Tmp0 + 3)--;
    do {
        unsigned s = y + M(Tmp0);
        M(Tmp0) = (uint8_t)s;
        M(Tmp0 + 1) = (uint8_t)(x + M(Tmp0 + 1) + (s > 0xFF));
        M(Tmp0 + 3)--;
    } while (M(Tmp0 + 3));
}

static void draw_position_marker(Game *g, uint8_t level) /* B3 $B8C9: the blinking "you are here" marker (sprite 63) */
{
    if (level >= 5) {
        return;
    }
    uint8_t i = (uint8_t)(level << 2);
    M(Tmp0 + 3) = R3(0xB934 + i);
    unsigned lo = M(SavedPlayerX) + M(SavedCameraXLo);
    mul16(g, (uint8_t)lo, (uint8_t)(M(SavedCameraXHi) + (lo > 0xFF)));
    M(OamBuffer + 0xFF) = (uint8_t)(R3(0xB935 + i) + M(Tmp0 + 1));
    M(Tmp0 + 3) = R3(0xB936 + i);
    lo = M(SavedPlayerY) + M(SavedCameraYLo);
    mul16(g, (uint8_t)lo, (uint8_t)(M(SavedCameraYHi) + (lo > 0xFF)));
    M(OamBuffer + 0xFC) = (uint8_t)(R3(0xB937 + i) + M(Tmp0 + 1));
    M(OamBuffer + 0xFD) = 0xAF;
    M(OamBuffer + 0xFE) = M(FrameCounter) & 3;
}

static void draw_level_picture(Game *g, uint8_t level) /* B3 $B695: draws the level picture, one part per frame */
{
    if (M(PpuBufferIndex) >= 0x20) {
        return;
    }
    uint8_t x = (uint8_t)((M(0x03D4) ? 2 : level) << 1);
    if (M(PauseDrawIndex) >= 3) {
        return;
    }
    uint8_t y = (uint8_t)(M(PauseDrawIndex) << 2);
    uint16_t src = rom_word(g->rom, B3, (uint16_t)(0xB641 + x));
    M(Tmp10) = x;
    for (int k = 0; k < 4; k++) {
        M(Tmp12 + k) = rdmem(g, (uint16_t)(src + y + k));
    }
    uint8_t py = M(0x0596);
    M(PauseDrawIndex)++;
    uint16_t data = rom_word(g->rom, B3, (uint16_t)(0xB717 + x));
    do {
        M(Tmp16) = M(Tmp15);
        app(g, M(Tmp15));
        app(g, M(Tmp13));
        app(g, M(Tmp14));
        do {
            app(g, R3(data + py++));
        } while (--M(Tmp16));
        unsigned s = M(Tmp14) + 0x20;
        M(Tmp14) = (uint8_t)s;
        M(Tmp13) = (uint8_t)(M(Tmp13) + (s > 0xFF));
    } while (--M(Tmp12));
    if (M(PauseDrawIndex) == 3) {
        py = 0;
    }
    M(0x0596) = py;
}

/* --------------------------------------------------------------- state machine ($D0) --- */
static int all_turtles_dead(Game *g) {
    return !(HEALTH(0) | HEALTH(1) | HEALTH(2) | HEALTH(3));
}

void pause_screen(Game *g) /* B3 $A91C */
{
    M(CurPrgBank) = 3; /* Call_Entry_B3_A91C : SetPrgBankSaved(3) */
    g->prg_bank = 3;
    draw_counters(g);
    switch (M(PauseState)) {
        case 0:
            M(PauseState)++;
            break;
        case 1:                        /* $A936 */
            if (all_turtles_dead(g)) { /* nobody left: game over */
                M(PauseState)++;
                for (int x = 0; x < 16; x++) {
                    clear_entity(g, x);
                }
                load_screen_and_clear(g);
                load_level_palette(g, 3);
                set_chr_bank1_raw(g, 0x0F);
                set_chr_bank0_raw(g, 0x00);
                M(PauseFlag) = 0;
                M(ElapsedTimeLo) = 0xF0;
                M(ElapsedTimeHi) = 0xFE;
                M(0x03D9) = 1;
                M(StageState) = 0x0B;
                return;
            }
            if (!M(0x03DA)) { /* $A972 */
                load_screen_alt(g, 8);
                set_chr_bank1_raw(g, 0x1B);
                M(PausedTurtle) = M(CurrentTurtle);
            }
            if (draw_dead_portraits(g)) {
                M(PauseState)++;
                M(0x03DA) = 0;
                M(PpuCtrlShadow) = (uint8_t)((M(PpuCtrlShadow) & 0xFE) | 0x02);
            }
            break;
        case 2: /* $A99A: health and ammo of the 4 turtles */
            M(PauseState)++;
            for (int t = 3; t >= 0; t--) {
                M(Tmp8) = (uint8_t)t;
                draw_health_bar(g);
                draw_ammo(g);
            }
            break;
        case 3:
            load_level_palette(g, 3);
            M(PauseState)++;
            break; /* $A9AB */
        case 4: {  /* $A9B3: level number, weapons */
            M(PauseState)++;
            app(g, 1);
            app(g, 0x20);
            app(g, 0x35);
            app(g, (uint8_t)(M(LevelNumber) + 1));
            uint8_t y = 0;
            if (R3(0xAD8B + M(LevelNumber)) && M(RopeCount)) {
                y++;
            }
            if (R3(0xAD92 + M(LevelNumber)) && M(SpecialShotCount)) {
                y += 2;
            }
            app(g, 3);
            app(g, 0x23);
            app(g, 0xED);
            M(Tmp0) = y;
            uint8_t i = (uint8_t)(y * 3);
            app(g, R3(0xAA10 + i));
            app(g, R3(0xAA11 + i));
            app(g, R3(0xAA12 + i));
            break;
        }
        case 5: /* $AA1C */
            for (int t = 3; t >= 0; t--) {
                M(Tmp8) = (uint8_t)t;
                draw_weapon_icon(g);
            }
            M(PauseState)++;
            break;
        case 6: { /* $AA2A: first turtle still alive */
            uint8_t y = M(CurrentTurtle);
            while (!HEALTH(y)) {
                if (++y == 4) {
                    y = 0;
                }
            }
            M(CurrentTurtle) = y;
            set_turtle_text(g, y);
            M(PauseDrawIndex) = 0;
            M(0x0596) = 0;
            M(PauseState)++;
            break;
        }
        case 7: /* $AA4A */
            draw_level_picture(g, M(LevelNumber));
            if (M(PauseDrawIndex) >= 3) {
                M(TextState) = 0;
                M(PauseState)++;
            }
            break;
        case 8: /* $AA5C: the menu waits */
            draw_position_marker(g, M(LevelNumber));
            text_update(g);
            if (M(JoyPressed2) & 0x0C) { /* $AA6B: up / down changes the turtle */
                M(0x03D6) = 0;
                int alive = 0;
                for (int t = 0; t < 4; t++) {
                    if (HEALTH(t)) {
                        alive++;
                    }
                }
                if (alive > 1) {
                    sound_play(g, 0x3C);
                }
                print_text_keep_tmp3(g, (uint8_t)(M(CurrentTurtle) + 8));
                if (!(M(JoyPressed2) & 0x04)) {
                    do {
                        M(CurrentTurtle) = (uint8_t)(M(CurrentTurtle) - 1);
                        if (M(CurrentTurtle) & 0x80) {
                            M(CurrentTurtle) = 3;
                        }
                    } while (!HEALTH(M(CurrentTurtle)));
                } else {
                    do {
                        M(CurrentTurtle)++;
                        if (M(CurrentTurtle) >= 4) {
                            M(CurrentTurtle) = 0;
                        }
                    } while (!HEALTH(M(CurrentTurtle)));
                }
                uint8_t x = (uint8_t)(M(CurrentTurtle) << 1);
                load_palette_preset(g, R7(0xD1DF + x));
                print_text_keep_tmp3(g, R7(0xD1DE + x));
            }
            break;
        default:
            break;
    }
}

/* --- information screen after a boss (scene $B5 = 4): B7 $D179 / $D1CA --- */
void scene_info_init(Game *g) /* B7 $D179: the pause screen is built in one go, buffer emptied at each pass */
{
    uint8_t saved = M(CurPrgBank);
    M(PauseState) = 0;
    for (int guard = 0; guard < 64; guard++) {
        pause_screen(g);
        if (M(PauseState) >= 6) {
            ppu_buffer_append(g, 0);
            M(PpuCtrlShadow) &= 0x7F;
            flush_ppu_buffer(g);
            break;
        }
        ppu_buffer_append(g, 0);  /* $CAB8 */
        M(PpuCtrlShadow) &= 0x7F; /* $C4CB: NMI off during the construction */
        flush_ppu_buffer(g);      /* $C26C */
    }
    uint8_t t = (uint8_t)(M(CurrentTurtle) << 1);
    print_text_keep_tmp3(g, R7(0xD1DE + t));
    load_palette_preset(g, R7(0xD1DF + t));
    M(PauseState) = 0;
    M(PauseDrawIndex) = 0;
    M(0x0596) = 0;
    M(0x9F) = 1;
    M(TextState) = 1;
    draw_level_picture(g, M(LevelNumber));
    text_update(g);
    M(CurPrgBank) = saved;
    g->prg_bank = saved;
}

void scene_info_update(Game *g) /* B7 $D1CA */
{
    uint8_t saved = M(CurPrgBank);
    M(CurPrgBank) = 3; /* $D1CC : SetPrgBank(3) */
    g->prg_bank = 3;
    if (M(PauseDrawIndex) < 3) {
        draw_level_picture(g, M(LevelNumber));
    }
    text_update(g);
    M(CurPrgBank) = saved;
    g->prg_bank = saved;
}
