/* Screen shown in the game window when no ROM can be found: the comic-book page of the menu (menu.c) says where to put it, any key or click quits. */
#include <stdio.h>
#include <string.h>
#include "help_screen.h"
#include "menu.h"
#include "platform.h"
#include "ui.h"

void help_screen_rom_missing(const char *rom_dir, const char *data_dir) {
    fprintf(stderr, "TMNT - ROM needed\nPut your ROM (.nes file, European or American version) in the \"rom\" folder next to the game.\n(%s)\n", rom_dir);
    platform_make_dir(rom_dir); /* so that the person only has to drop the file in it */
    if (platform_init("TMNT - ROM needed")) {
        platform_error_box("ROM needed: put your .nes file (European or American version) in the \"rom\" folder next to the game, then start it again.");
        return;
    }
    menu_init(data_dir, NULL);
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
