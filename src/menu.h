/* The menu: a comic-book page of panels drawn with ui.c, with tabs (PLAY, SOUND, GRAPHICS, CONTROLS). It shows and changes the options of options.c. The front-end
 * (frontend.c) feeds it the pad and mouse events, draws it over the frozen game, and acts on what it returns. */
#ifndef MENU_H
#define MENU_H
#include <stdint.h>
#include "ui.h"

typedef struct {
    int up, down, left, right; /* pressed since the last call (one event per press) */
    int confirm, back;         /* A / Enter, B */
    int tab_prev, tab_next;    /* LB / RB of a pad: the previous and the next page */
    int escape;                /* the Esc key */
    int mouse_x, mouse_y;      /* in the window, pixels; -1 when unknown */
    int clicked;               /* left button pressed since the last call */
} MenuInput;

typedef enum {
    MENU_NONE,
    MENU_NEW_GAME,
    MENU_RESUME,
    MENU_QUIT
} MenuAction;

/* Loads the fonts and the logo from `data_dir` (the folder "data" of the game); `ini_path` is where the options are saved (may be NULL). */
void menu_init(const char *data_dir, const char *ini_path);
void menu_open(int game_running); /* game_running: a game is under the menu, RESUME is possible */
void menu_close(void);
int menu_is_open(void);
MenuAction menu_update(const MenuInput *in);
/* Draws the page on `ui` (the whole window); `picture` is the game's last 256x240 picture shown darkened behind (NULL at the launch). */
void menu_draw(Ui *ui, const uint32_t *picture);
/* Tests: where an item is (its centre, in pixels of the last drawn frame). kind: one of the MENU_ITEM_ values; arg: the page, the edition, the option or the level. */
enum {
    MENU_ITEM_TAB,
    MENU_ITEM_EDITION,
    MENU_ITEM_OPTION,
    MENU_ITEM_LEVEL,
    MENU_ITEM_PLAY,
    MENU_ITEM_RESUME,
    MENU_ITEM_QUIT,
    MENU_ITEM_YES,
    MENU_ITEM_NO
};
int menu_item_centre(int kind, int arg, int *x, int *y); /* 1 if the item is on the page */
void menu_select_page(int page);                         /* tests: 0 PLAY, 1 SOUND, 2 GRAPHICS, 3 CONTROLS */
void menu_set_focus_option(int opt_id);

#endif
