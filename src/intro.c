/* Intro scene (GameMode 1): state machine SceneState ($03C2), bank 3 $BCFB-$BF83.
 * The function names follow labels.txt (Intro_StateN_...); the original address is in a comment. */
#include "game.h"
#include "anim.h"
#include "hal.h"
#include "intro.h"
#include "player.h"
#include "sound.h"

#define E5(base) ((base) + 5) /* field of entity 5 (the portrait) */

#define flash_colors(i) ROM_AT(3, 0xBEF6, (i))     /* B3 $BEF6 */
#define demo_inputs(i) ROM_AT(3, 0xBEFA, (i))      /* B3 $BEFA */
#define portrait_sprites(i) ROM_AT(3, 0xBEEA, (i)) /* B3 $BEEA */
#define group_palettes(i) ROM_AT(3, 0xBEEE, (i))   /* B3 $BEEE */
#define group_stop_x(i) ROM_AT(3, 0xBEF2, (i))     /* B3 $BEF2 */
#define final_box_data(i) ROM_AT(3, 0xBF84, (i))   /* B3 $BF84: 14 bytes */

/* --- text typed letter by letter (B3 $ADD1 / $AF22): name and weapon of the turtle, 1 letter every 4 frames --- */
static void text_char(Game *g, uint8_t ch) /* $AF58 */
{
    uint8_t x = M(PpuBufferIndex);
    M(PpuBuffer + x) = 1;
    M(PpuBuffer + (uint8_t)(x + 1)) = M(TextPpuAddrHi);
    M(PpuBuffer + (uint8_t)(x + 2)) = M(TextPpuAddrLo);
    M(PpuBuffer + (uint8_t)(x + 3)) = ch;
    M(PpuBufferIndex) = (uint8_t)(x + 4);
    sound_play(g, M(DemoModeBits) ? 0x36 : 0x39);
    M(TextState)++;
    M(TextPpuAddrLo)++;
}

static void text_typewriter(Game *g) /* $AF22 */
{
    M(ZP_8F)++;
    if (M(ZP_8F) & 3) {
        return;
    }
    uint8_t y = M(TextState) & 0x7F;
    uint16_t script = (uint16_t)(M(TextPtrLo) | M(TextPtrLo + 1) << 8);
    uint8_t v = rom_prg(g->rom, B3, (uint16_t)(script + y));
    if (v == 0) {
        M(TextState)++;
        M(TextPpuAddrLo)++;
        return;
    } /* space */
    if (v < 0xF7) {
        text_char(g, v);
        return;
    }
    switch ((uint8_t)(v + 9)) {
        case 2:
            print_text_keep_tmp3(g, 2);
            M(TextState)++;
            break; /* $F9: label "WEAPON:" */
        case 7:    /* $FE: new PPU address */
            M(TextPpuAddrHi) = rom_prg(g->rom, B3, (uint16_t)(script + y + 1));
            M(TextPpuAddrLo) = rom_prg(g->rom, B3, (uint16_t)(script + y + 2));
            M(TextState) += 3;
            break;
        case 8:
            M(TextState) = 3;
            break; /* $FF: end of the text */
        case 6: {  /* $FD: $AF8B, next pause text */
            uint16_t nxt = rom_word(g->rom, B3, (uint16_t)(0xB412 + M(0x0597)));
            M(TextPtrLo) = (uint8_t)nxt;
            M(TextPtrLo + 1) = (uint8_t)(nxt >> 8);
            M(TextPpuAddrLo) = 0xE7;
            M(TextState) = 0x80;
            break;
        }
        case 0:
            M(TextPpuAddrLo) = 0x63;
            M(TextState)++;
            break; /* $F7 : $AFB3 */
        case 5: {  /* $FC: $AFC1, number of zero bits of $7D (bombs left) */
            uint8_t n = 0, b = (uint8_t)~M(BombsDefusedMask);
            for (int k = 0; k < 8; k++) {
                n = (uint8_t)(n + (b & 1));
                b >>= 1;
            }
            text_char(g, n ? n : 0x0A);
            break;
        }
        case 1:
            M(TextPpuAddrLo) = 0xA3;
            M(TextState)++;
            break; /* $F8 : $AFBA */
        case 4: {  /* $FB: $AFCC, continuation of the turtle's text ($A0) */
            M(TextPpuAddrLo)++;
            uint8_t i = 0;
            if (M(PausedTurtle) == 2) {
                M(TextPpuAddrLo) = 0x63;
                M(TextPpuAddrHi) = 0x22;
                i = 2;
            }
            uint16_t nxt = rom_word(g->rom, B3, (uint16_t)(0xB4AE + i));
            M(TextPtrLo) = (uint8_t)nxt;
            M(TextPtrLo + 1) = (uint8_t)(nxt >> 8);
            M(TextState) = 0x80;
            break;
        }
        default:
            TRACE("f=%u TODO text control %02X\n", g->frame, v);
            M(TextState)++;
            break;
    }
}

void text_update(Game *g) /* $ADD1: "portrait" states ($8E = 2) and "death of a turtle" ($0617) */
{
    if (M(PpuBufferIndex) >= 0x20) {
        return;
    }
    if (M(TextState) & 0x80) {
        text_typewriter(g);
        return;
    }
    M(TextPpuAddrHi) = 0x22;
    M(TextPpuAddrLo) = 0x23;
    if (M(TextState) == 2) { /* $AE8F: CurrentTurtle script (table $B5D9) */
        uint16_t p = rom_word(g->rom, B3, (uint16_t)(0xB5D9 + M(CurrentTurtle) * 2));
        M(TextPtrLo) = (uint8_t)p;
        M(TextPtrLo + 1) = (uint8_t)(p >> 8);
        M(TextPpuAddrHi) = 0x21;
        M(TextPpuAddrLo) = 0xEF;
        M(TextState) = 0x80;
    } else if (M(TextState) == 0 && M(PauseAfterDeath)) { /* $ADF4 -> $AEAB: message after the death of a turtle */
        uint16_t p = rom_word(g->rom, B3, (uint16_t)(0xB47D + (uint8_t)(M(PausedTurtle) << 1)));
        M(TextPtrLo) = (uint8_t)p;
        M(TextPtrLo + 1) = (uint8_t)(p >> 8);
        uint8_t y = rom_prg(g->rom, B3, (uint16_t)(0xAEC6 + M(LevelNumber)));
        M(TextState) = 0x7F;
        M(0x0597) = y; /* $AF04: copies the introduction text into the PPU buffer */
        uint16_t t = rom_word(g->rom, B3, (uint16_t)(0xB43F + y));
        for (uint8_t i = 0;; i++) {
            uint8_t v = rom_prg(g->rom, B3, (uint16_t)(t + i));
            if (v == 0xFF) {
                break;
            }
            ppu_buffer_append(g, v);
        }
        M(TextState)++;
    } else if (M(TextState) == 0 && !M(PauseAfterDeath)) { /* $ADFC: message of the manual pause */
        int special = -1;                                  /* $AEDF: message of the $B026 table */
        if (M(WaveState) == 3) {
            special = 6;                                                                                           /* $AEEE: during a boss */
        } else if (M(SideViewFlag) && M(AreaNumber) == rom_prg(g->rom, B3, (uint16_t)(0xB020 + M(LevelNumber)))) { /* $AECC */
            if (M(LevelNumber) == 1) {
                uint8_t n = 0, v = (uint8_t)~M(BombsDefusedMask); /* B3 $B00D: number of zero bits of $7D */
                for (int b = 0; b < 8; b++) {
                    n = (uint8_t)(n + (v & 1));
                    v >>= 1;
                }
                special = (n == 1) ? 4 : 1;
            } else {
                special = M(LevelNumber);
            }
        }
        if (special >= 0) {
            uint16_t sc = rom_word(g->rom, B3, (uint16_t)(0xB026 + special * 2));
            M(TextPtrLo) = (uint8_t)sc;
            M(TextPtrLo + 1) = (uint8_t)(sc >> 8);
            uint8_t y = rom_prg(g->rom, B3, (uint16_t)(0xAEC6 + M(LevelNumber))); /* $AEB9 */
            M(TextState) = 0x7F;
            M(0x0597) = y; /* $AF04 */
            uint16_t t = rom_word(g->rom, B3, (uint16_t)(0xB43F + y));
            for (uint8_t i = 0;; i++) {
                uint8_t v = rom_prg(g->rom, B3, (uint16_t)(t + i));
                if (v == 0xFF) {
                    break;
                }
                ppu_buffer_append(g, v);
            }
            M(TextState)++;
            return;
        }
        uint8_t sel = (M(FrameCounter) >> 1) & 3; /* $AE16: message chosen by the frame counter */
        if (sel == 0) {
            int x = 3;
            while (x >= 0 && M(TurtleHealth0 + x)) {
                x--;
            }
            if (x < 0) {
                sel = 1; /* all the turtles are alive */
            }
        }
        uint16_t tbl = rom_word(g->rom, B3, (uint16_t)(0xB3D6 + (uint8_t)(M(LevelNumber) << 1)));
        uint16_t sc = rom_word(g->rom, B3, (uint16_t)(tbl + sel * 2));
        M(TextPtrLo) = (uint8_t)sc;
        M(TextPtrLo + 1) = (uint8_t)(sc >> 8);
        uint8_t y;
        if (sel == 0) { /* $AE49 */
            y = rom_prg(g->rom, B3, (uint16_t)(0xAE55 + M(LevelNumber)));
            M(TextState) = 0x7F;
        } else { /* $AEF2 */
            M(TextState) = 0x80;
            y = (uint8_t)(rom_prg(g->rom, B3, sc) << 1);
        }
        M(0x0597) = y; /* $AF04 */
        uint16_t t = rom_word(g->rom, B3, (uint16_t)(0xB43F + y));
        for (uint8_t i = 0;; i++) {
            uint8_t v = rom_prg(g->rom, B3, (uint16_t)(t + i));
            if (v == 0xFF) {
                break;
            }
            ppu_buffer_append(g, v);
        }
        M(TextState)++;
    } else if (M(TextState) == 1) { /* $AE5B: end-of-level message */
        uint8_t x = M(0x03D4) ? 0x0A : (uint8_t)(M(LevelNumber) << 1);
        uint16_t sc = rom_word(g->rom, B3, (uint16_t)(0xB4ED + x));
        M(TextPtrLo) = (uint8_t)sc;
        M(TextPtrLo + 1) = (uint8_t)(sc >> 8);
        uint8_t y = (M(0x03D4) || M(LevelNumber) != 2) ? 2 : 0;
        M(TextState) = 0x7F;
        M(0x0597) = y; /* $AF04 */
        uint16_t t = rom_word(g->rom, B3, (uint16_t)(0xB43F + y));
        for (uint8_t i = 0;; i++) {
            uint8_t v = rom_prg(g->rom, B3, (uint16_t)(t + i));
            if (v == 0xFF) {
                break;
            }
            ppu_buffer_append(g, v);
        }
        M(TextState)++;
    } else if (M(TextState) != 3) {
        TRACE("f=%u TODO text state %02X\n", g->frame, M(TextState));
    }
}

/* --- player: see player.c --- */
static void run_player_frame(Game *g) /* B7 $D0AA */
{
    player_update(g);
    animate_entities_from(g, 0);
    weapon_update(g);
}

static void state0_init_scene(Game *g) /* $BD20 */
{
    load_screen_and_clear(g);
    set_chr_bank0(g, 0x0A);
    set_chr_bank1(g, 0x0F);
    load_palette_preset(g, 0x1E);
    fill_palette_backdrop(g, 0x0F);
    load_screen(g, 1, 0x1C);
    M(0x0744) = 0x44;
    M(0x0745) = 0x44;
    M(E5(EntSprite)) = 0x90;
    M(E5(EntFlags)) = 0xC2;
    M(E5(EntX)) = 0x80;
    M(E5(EntY)) = 0x78;
    M(E5(EntAttr)) = 0x00;
    M(SceneTimerA) = 0x40;
    M(SceneCounter) = 0x08;
    M(SceneState)++;
}

static void state1_flash_wait(Game *g) /* $BD69 */
{
    if (M(SceneTimerA)) {
        M(SceneTimerA)--;
        return;
    }
    if (--M(SceneCounter) != 0) { /* $BD72: blinks */
        uint8_t y = (uint8_t)((M(SceneCounter) & 1) << 1);
        M(Tmp8) = y;
        fill_palette_backdrop(g, flash_colors(y));
        M(PaletteBuffer) =
            rom_prg(g->rom, B3, 0xBEF7 + 0x20); /* Y is $20 after FillPaletteBackdrop: reads $BF17 (= $EE, the opcode of "inc $03C4"), not the table */
        return;
    }
    M(E5(EntType)) = 0x50; /* $BD8F: starts the turtle's entrance */
    M(E5(EntAnimTimer)) = 0x01;
    M(E5(EntAnimFrame)) = 0x00;
    M(E5(EntAnimId)) = 0xF8;
    load_palette_preset(g, 0x1E);
    fill_palette_backdrop(g, 0x0F);
    set_turtle_sprite_palette(g);
    M(SceneState)++;
}

static void state2_entrance_anim(Game *g) /* $BDB2 */
{
    if (M(E5(EntAnimFrame)) != 0xFF) {
        animate_entities_from(g, 5); /* $D265 */
        return;
    }
    M(SideViewFlag) = 1; /* $BDBE */
    player_spawn(g);
    M(EntX) = 0x80;
    M(EntY) = 0x78;
    M(E5(EntFlags)) = 0x00;
    M(SceneIndex) = 0;
    M(SceneTimerB) = 0x20;
    M(SceneState)++;
}

static void portrait_setup(Game *g) /* $BE1E (state 7 too) */
{
    uint8_t t = M(CurrentTurtle);
    M(E5(EntAttr)) = t;
    M(E5(EntSprite)) = portrait_sprites(t);
    M(E5(EntFlags)) = 0xC2;
    M(E5(EntX)) = 0xF7;
    M(E5(EntY)) = 0x74;
    M(SceneState)++;
}

static void state3_scripted_demo(Game *g) /* $BDE2 */
{
    if (--M(SceneTimerB) != 0) { /* $BE00 */
        M(JoyHeld1) = 0;
        M(JoyPressed1) = 0;
        run_player_frame(g);
        return;
    }
    if (M(SceneIndex) != 3) {
        uint8_t k = demo_inputs(M(SceneIndex));
        M(JoyHeld1) = k;
        M(JoyPressed1) = k;
        M(SceneIndex)++;
        M(SceneTimerB) = 0x20;
        run_player_frame(g);
        return;
    }
    M(EntFlags) = 0; /* $BE09: end of the demo */
    M(EntFlags + 1) = 0;
    set_chr_bank0(g, 0x0B);
    load_palette_preset(g, 0x23);
    load_screen_and_clear(g);
    portrait_setup(g);
}

static void state4_portrait_slide_in(Game *g) /* $BE3C */
{
    M(E5(EntX)) -= 8;
    if (M(E5(EntX)) >= 0x60) {
        return;
    }
    M(TextState) = 2;
    text_update(g);
    print_text_keep_tmp3(g, 1);
    sound_play(g, 0x3A);
    M(SceneTimerA) = 0xA0;
    M(SceneDelay) = 0x20;
    M(SceneState)++;
}

static void state5_portrait_text(Game *g) /* $BE68 */
{
    if (M(SceneDelay)) {
        M(SceneDelay)--;
        return;
    }
    if (--M(SceneTimerA) != 0) {
        text_update(g);
        return;
    }
    M(CurrentTurtle)++;
    if (M(CurrentTurtle) == 4) {
        M(SceneState)++;
        return;
    }
    M(SceneState) = 0;
}

static void state6_group_init(Game *g) /* $BE8B */
{
    M(CurrentTurtle) = 0;
    load_screen_and_clear(g);
    set_chr_bank1(g, 0x0B);
    load_palette_preset(g, 0x36);
    load_screen(g, 1, 0x1E);
    M(SceneState)++;
}

static void state8_group_slide_in(Game *g) /* $BEA5 */
{
    uint8_t t = M(CurrentTurtle);
    M(E5(EntX)) -= 8;
    if (M(E5(EntX)) >= group_stop_x(t)) {
        return;
    }
    sound_play(g, 0x3A);
    load_palette_preset(g, group_palettes(t));
    M(CurrentTurtle)++;
    if (M(CurrentTurtle) == 4) {
        M(SceneTimerA) = 0x40;
        M(SceneState)++;
        return;
    }
    M(SceneState) = 7;
}

static void state9_group_hold(Game *g) /* $BEDA */
{
    if (--M(SceneTimerA) != 0) {
        return;
    }
    M(E5(EntFlags)) = 0;
    M(CurrentTurtle) = 0;
    M(SceneState)++;
}

static void state10_final_init(Game *g) /* $BEFD */
{
    load_screen_and_clear(g);
    set_chr_bank1(g, 0x0B);
    load_screen(g, 1, 0x24);
    M(SceneTimerA) = 0x40;
    M(SceneState)++;
    load_palette_preset(g, 0x1C);
}

static void state11_final_box(Game *g) /* $BF17 */
{
    if (++M(SceneTimerA) != 0) {
        return; /* 192 frames of waiting */
    }
    for (int i = 0; i < 14; i++) {
        ppu_buffer_append(g, final_box_data(i));
    }
    M(SceneDelay) = 6;
    M(SceneIndex) = 0x22;
    M(SceneCounter) = 0x24;
    M(SceneTimerA) = 0x40;
    M(SceneState)++;
}

/* --- frame of the last picture (bank 1 $BE83-$BF2A): (SceneIndex:SceneCounter) = current PPU address;
 * each step pushes into the PPU buffer a column of 9 tiles and a row of 10, then moves the address back up. */
#define box_tile_offsets(i) ROM_AT(B1, 0xBFC3, (i)) /* B1 $BFC3 */

static uint8_t add16_to_scene_addr(Game *g, uint8_t a, uint8_t *y) /* B7 $C94D: (Y:A) += address */
{
    unsigned lo = a + M(SceneCounter);
    *y = (uint8_t)(*y + M(SceneIndex) + (lo > 0xFF));
    return (uint8_t)lo;
}

static void scene_addr_up_a_row(Game *g) /* B7 $C93D: address -= $1F */
{
    uint8_t y = 0xFF, a = add16_to_scene_addr(g, 0xE1, &y);
    M(SceneCounter) = a;
    M(SceneIndex) = y;
}

static void box_column_header(Game *g) /* B1 $BF0F: 9 bytes, vertical address */
{
    M(Tmp11) = 9;
    ppu_buffer_append(g, 9);
    ppu_buffer_append(g, M(SceneIndex) | 0x80);
}

static void box_row_header(Game *g) /* B1 $BF1F */
{
    ppu_buffer_append(g, M(SceneIndex));
    ppu_buffer_append(g, M(SceneCounter));
}

static void box_step_c(Game *g) /* B1 $BE83 */
{
    uint8_t y = 0;
    M(Tmp10) = box_tile_offsets(M(SceneDelay) & 7);
    box_column_header(g);
    ppu_buffer_append(g, add16_to_scene_addr(g, 0x0A, &y));
    uint8_t t = M(Tmp10);
    do {
        ppu_buffer_append(g, rom_prg(g->rom, B1, (uint16_t)(0xBF2B + t++)));
    } while (--M(Tmp11) != 0);
    M(Tmp10) = t;
    scene_addr_up_a_row(g);
    M(Tmp11) = 0x0A;
    ppu_buffer_append(g, 0x0A);
    box_row_header(g);
    t = M(Tmp10);
    do {
        ppu_buffer_append(g, rom_prg(g->rom, B1, (uint16_t)(0xBF2B + t++)));
    } while (--M(Tmp11) != 0);
}

static void box_step_a(Game *g) /* B1 $BEC0 */
{
    uint8_t y = 1;
    box_column_header(g);
    ppu_buffer_append(g, M(SceneCounter));
    for (int i = 0; i < 9; i++) {
        ppu_buffer_append(g, 0);
    }
    ppu_buffer_append(g, 0x8A);
    uint8_t a = add16_to_scene_addr(g, 0x20, &y);
    ppu_buffer_append(g, y);
    ppu_buffer_append(g, a);
    ppu_buffer_append(g, 0);
}

static void box_step_b(Game *g) /* B1 $BEEB */
{
    uint8_t y = 0;
    box_column_header(g);
    ppu_buffer_append(g, add16_to_scene_addr(g, 0x0A, &y));
    do {
        ppu_buffer_append(g, 2);
    } while (--M(Tmp11) != 0);
    scene_addr_up_a_row(g);
    ppu_buffer_append(g, 0x8A);
    box_row_header(g);
    ppu_buffer_append(g, 2);
}

static void state12_final_open(Game *g) /* $BF41 */
{
    if (M(SceneTimerA)) {
        M(SceneTimerA)--;
        return;
    }
    if (M(FrameCounter) & 1) {
        return;
    }
    box_step_a(g);
    box_step_b(g);
    if (--M(SceneDelay) != 0) {
        return;
    }
    load_palette_preset(g, 0x1D);
    M(SceneDelay) = 7;
    M(SceneState)++;
}

static void state13_final_end(Game *g) /* $BF67 */
{
    if (M(FrameCounter) & 1) {
        return;
    }
    box_step_a(g);
    box_step_c(g);
    if ((int8_t)--M(SceneDelay) >= 0) {
        return;
    }
    M(GameMode) = 0; /* back to the title, without reloading the screen */
    M(GameSubMode) = 1;
    M(ZP_2E) = 1;
    set_delay_timer(g);
}

void intro_update(Game *g) /* $BCFB */
{
    switch (M(SceneState)) {
        case 0:
            state0_init_scene(g);
            break;
        case 1:
            state1_flash_wait(g);
            break;
        case 2:
            state2_entrance_anim(g);
            break;
        case 3:
            state3_scripted_demo(g);
            break;
        case 4:
            state4_portrait_slide_in(g);
            break;
        case 5:
            state5_portrait_text(g);
            break;
        case 6:
            state6_group_init(g);
            break;
        case 7:
            portrait_setup(g);
            break;
        case 8:
            state8_group_slide_in(g);
            break;
        case 9:
            state9_group_hold(g);
            break;
        case 10:
            state10_final_init(g);
            break;
        case 11:
            state11_final_box(g);
            break;
        case 12:
            state12_final_open(g);
            break;
        case 13:
            state13_final_end(g);
            break;
    }
}
