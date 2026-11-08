/* Screen shown when no ROM is found. */
#ifndef HELP_SCREEN_H
#define HELP_SCREEN_H

/* Creates `rom_dir` if it does not exist, then shows in a window where to put the ROM; returns when a key is pressed or the window is closed. */
void help_screen_rom_missing(const char *rom_dir, const char *data_dir); /* data_dir: the "data" folder (fonts) */

#endif
