/* Screen shown in the game window when no ROM can be found: the comic-book page of the menu (menu.c) says where to put it, any key or click quits. */
#include <stdio.h>
#include <string.h>
#include "font8x8.h"
#include "help_screen.h"
#include "menu.h"
#include "platform.h"
#include "ui.h"

/* Without the fonts of the menu (the "data" folder is missing): the plain screen with the 8x8 font of the console. */
#define COLUMNS (SCREEN_W / 8 - 2) /* one blank column on each side */
static uint32_t picture[SCREEN_W * SCREEN_H];

static void draw_char(int x, int y, char c, uint32_t color) {
    if (c < 32 || c > 126) {
        return;
    }
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (font8x8[c - 32][row] & (1 << col)) {
                int px = x + col, py = y + row;
                if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                    picture[py * SCREEN_W + px] = color;
                }
            }
        }
    }
}

/* Writes `text` from line `line` on, wrapped at COLUMNS characters (words are kept whole when they fit); returns the next free line. */
static int draw_text(int line, const char *text, uint32_t color) {
    int col = 0;
    for (const char *p = text; *p;) {
        if (*p == '\n') {
            line++;
            col = 0;
            p++;
            continue;
        }
        if (*p == ' ') {
            if (col > 0 && col < COLUMNS) {
                col++;
            }
            p++;
            continue;
        }
        int len = 0;
        while (p[len] && p[len] != ' ' && p[len] != '\n') {
            len++;
        }
        if (col > 0 && col + len > COLUMNS) {
            line++;
            col = 0;
        }
        for (int i = 0; i < len;) {
            int n = len - i < COLUMNS - col ? len - i : COLUMNS - col;
            if (i + n < len) { /* a word longer than a line, such as a path: cut it after a folder separator if there is one */
                for (int k = n; k > 1; k--) {
                    if (p[i + k - 1] == '\\' || p[i + k - 1] == '/') {
                        n = k;
                        break;
                    }
                }
            }
            for (int k = 0; k < n; k++) {
                draw_char(8 + (col + k) * 8, line * 8, p[i + k], color);
            }
            col += n;
            i += n;
            if (i < len) {
                line++;
                col = 0;
            }
        }
        p += len;
    }
    return line + 1;
}

static void plain_screen(const char *rom_dir) {
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        picture[i] = 0x000060;
    }
    int line = 3;
    line = draw_text(line, "TMNT - ROM needed", 0xFFD800) + 1;
    line = draw_text(line, "Put your ROM (.nes file, European or American version) in the \"rom\" folder next to the game.", 0xFFFFFF) + 1;
    draw_text(line, rom_dir, 0xFFFFFF);
    draw_text(SCREEN_H / 8 - 3, "Press any key to quit.", 0xFFD800);
    for (;;) {
        PlatformEvents ev;
        platform_poll(&ev);
        if (ev.quit || ev.any_key) {
            break;
        }
        platform_present(picture);
        platform_wait_frame();
    }
}

void help_screen_rom_missing(const char *rom_dir, const char *data_dir) {
    fprintf(stderr, "TMNT - ROM needed\nPut your ROM (.nes file, European or American version) in the \"rom\" folder next to the game.\n(%s)\n", rom_dir);
    platform_make_dir(rom_dir); /* so that the person only has to drop the file in it */
    if (platform_init("TMNT - ROM needed")) {
        platform_error_box("ROM needed: put your .nes file (European or American version) in the \"rom\" folder next to the game, then start it again.");
        return;
    }
    menu_init(data_dir, NULL);
    if (!menu_has_fonts()) {
        plain_screen(rom_dir);
        platform_shutdown();
        return;
    }
    Ui ui = {0};
    for (;;) {
        PlatformEvents ev;
        platform_poll(&ev);
        int mx, my, clicked, down;
        platform_mouse(&mx, &my, &clicked, &down);
        if (ev.quit || ev.any_key || clicked) {
            break;
        }
        int w, h;
        platform_window_size(&w, &h);
        ui_resize(&ui, w, h);
        menu_draw_rom_missing(&ui, rom_dir);
        platform_present_hd(ui.px, w, h);
        platform_wait_frame();
    }
    ui_free(&ui);
    platform_shutdown();
}
