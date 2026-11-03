/* Main state machine (B7 $C569 RunGameMode): GameMode 0 = licence/title, 1 = intro, 2 = menu confirmed,
 * 3 = new game. The port stops at GameMode 3 (the game proper is not ported). */
#include "game.h"
#include "ending.h"
#include "hal.h"
#include "intro.h"
#include "sound.h"
#include "stage.h"
#include "title.h"

#define menu_cursor_y(i) ROM_AT(7, 0xC642, (i)) /* B7 $C642 */

static void next_mode(Game *g) {
    M(GameMode)++;
    M(GameSubMode) = 0;
} /* $C6B9 */

/* B7 $C959: title background */
static void build_title_screen(Game *g) {
    load_screen_and_clear(g);
    M(ScrollXShadow) = 0;
    M(ScrollYShadow) = 0;
    M(PpuCtrlShadow) &= 0xFC;
    load_screen(g, 1, 0x02);
    load_palette_preset(g, 0x21);
    fill_palette_backdrop(g, 0x0F);
    print_text(g, 0x00);
}

static void mode0_licence_init(Game *g) /* $C589 */
{
    M(DemoModeBits) = 0;
    M(PpuCtrlShadow) &= 0xFC;
    sound_init(g);
    clear_game_ram30(g);
    load_screen_and_clear(g);
    set_chr_bank1(g, 0x0F);
    load_palette_preset(g, 0x21);
    fill_palette_backdrop(g, 0x0F);
    load_screen(g, 0, 0x26);
    set_delay_timer(g);
    M(GameSubMode)++;
}

static void mode0_licence_wait(Game *g) /* $C5B4 */
{
    if (!(M(JoyPressed1) & 0x30)) {
        if (tick_delay_timer(g)) {
            return;
        }
    }
    if (!M(ZP_2E)) {
        sound_init(g); /* $C5BF: goes to the title */
    }
    M(DemoModeBits) = 0;
    M(PpuCtrlShadow) &= 0xFC;
    clear_game_ram30(g);
    load_screen_and_clear(g);
    build_title_screen(g);
    if (!rom_is_us(g->rom)) {
        load_screen(g, 0, 0x28); /* $C5D9: absent from the US ROM */
    }
    set_chr_bank1(g, 0x0F);
    set_chr_bank0(g, 0x00);
    set_delay_timer(g);
    M(GameSubMode)++;
    if (!M(ZP_2E)) {
        sound_play(g, 0x4E); /* title music */
    }
    M(MenuSelection) = 0;
}

static void mode0_title_menu(Game *g) /* $C5FE */
{
    if (!tick_delay_timer(g)) {
        next_mode(g);
        return;
    } /* delay elapsed: the intro starts */
    M(EntX) = 0x40;
    M(EntFlags) = 0x02;
    M(EntSprite) = 0x00;
    M(EntY) = menu_cursor_y(M(MenuSelection) & 1);
    if (!(M(JoyPressedRaw1) & 0x10)) {
        return;
    }
    sound_play(g, 0x3C);
    M(DelayTimerLo) = 0x80;
    M(GameSubMode)++;
}

static void mode0_title_confirm(Game *g) /* $C62C: blinks, then GameMode 2 */
{
    uint8_t blink = (uint8_t)((M(DelayTimerLo) & 0x08) << 4);
    print_text(g, (uint8_t)(blink + M(MenuSelection)));
    if (--M(DelayTimerLo) != 0) {
        return;
    }
    M(GameMode) = 2;
    M(GameSubMode) = 0;
}

static void mode1_intro(Game *g) /* $C642 */
{
    if (M(GameSubMode) == 0) {
        clear_game_ram30(g);
        set_delay_timer_value(g, 0x00, 0x40); /* safeguard: 16384 frames */
        M(DemoModeBits) = 1;
        M(0x39) = 1;
        M(SideViewFlag) = 1;
        M(GameSubMode)++;
        return;
    }
    if (!(M(JoyPressed1) & 0x30)) {
        intro_update(g); /* B7 $D25A -> bank 3 */
        if (M(ZP_37) == 0 && tick_delay_timer(g)) {
            return;
        }
    }
    M(ZP_2E) = 0; /* $C66F: back to the title */
    M(GameMode) = 0;
    M(GameSubMode) = 1;
}

void run_game_mode(Game *g) /* $C569 */
{
    M(FrameCounter)++;
    switch (M(GameMode)) {
        case 0:
            switch (M(GameSubMode)) {
                case 0:
                    mode0_licence_init(g);
                    break;
                case 1:
                    mode0_licence_wait(g);
                    break;
                case 2:
                    mode0_title_menu(g);
                    break;
                case 3:
                    mode0_title_confirm(g);
                    break;
            }
            break;
        case 1:
            mode1_intro(g);
            break;
        case 2: /* $C67C */
            clear_game_ram30(g);
            M(DemoModeBits) = 0;
            M(MenuSelectionPrev) = M(MenuSelection);
            next_mode(g);
            break;
        case 3:
            mode3_new_game(g);
            break; /* $C686 */
        case 4:
            stage_run(g);
            break; /* $CB24 */
        case 5:
            ending_run(g);
            break; /* $C6B6: end of the game */
        default:
            g->left_attract = 1; /* GameMode 6 and later: not ported */
            break;
    }
}
