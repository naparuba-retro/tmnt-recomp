/* The menu, see menu.h. Immediate-mode: the draw functions also record the items (what can be focused or clicked) that the next update uses. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "menu.h"
#include "options.h"

#define PAPER 0xFFF3D6
#define INK 0x101018
#define YELLOW 0xFFD21F
#define RED 0xE3262B
#define GREEN 0x2FB84F
#define BLUE 0x2F6FD6
#define ORANGE 0xF28C28
#define NAVY 0x1B2650
#define GREY 0x9A937F
#define WHITE 0xFFFFFF

#define DESIGN_W 1280.0f
#define DESIGN_H 720.0f

typedef enum {
    K_TAB,
    K_EDITION,
    K_OPT,
    K_LEVEL,
    K_PLAY,
    K_RESUME,
    K_QUIT,
    K_YES,
    K_NO,
    K_STATIC
} Kind;

typedef struct {
    Kind kind;
    int arg;
    UiQuad q;
    int enabled;
} Item;

#define MAX_ITEMS 96
static Item items[MAX_ITEMS], last_items[MAX_ITEMS];
static int n_items, n_last;

typedef struct {
    Ui *ui;
    float s, ox, oy;
} Env;

static UiFont *font_title, *font_body;
static UiImage *logo;
static char ini_file[1100];
static int is_open, game_running, page, quit_modal, tick;
static Kind focus_kind = K_PLAY;
static int focus_arg;
static int last_mx = -1, last_my = -1;
static char page_names[4][12] = {"PLAY", "SOUND", "GRAPHICS", "CONTROLS"};

/* ------------------------------------------------------------------ setup --- */
static int file_exists(const char *p) {
    FILE *f = fopen(p, "rb");
    if (f) {
        fclose(f);
    }
    return f != NULL;
}

void menu_init(const char *data_dir, const char *ini_path) {
    char p[1200];
    snprintf(p, sizeof p, "%s/fonts/KOMIKAHB.ttf", data_dir);
    font_title = ui_font_load(p);
    snprintf(p, sizeof p, "%s/fonts/KOMIKAH_.ttf", data_dir);
    font_body = ui_font_load(p);
    snprintf(p, sizeof p, "%s/ui/logo-shell.png", data_dir);
    logo = file_exists(p) ? ui_image_load(p) : NULL;
    if (!font_title || !font_body) {
        fprintf(stderr, "menu: fonts not found in %s/fonts (the menu has no text)\n", data_dir);
    }
    if (!font_body) {
        font_body = font_title;
    }
    if (!font_title) {
        font_title = font_body;
    }
    snprintf(ini_file, sizeof ini_file, "%s", ini_path ? ini_path : "");
}

void menu_open(int running) {
    is_open = 1;
    game_running = running;
    quit_modal = 0;
    page = 0;
    focus_kind = K_PLAY;
    focus_arg = 0;
}

void menu_close(void) {
    is_open = 0;
}

int menu_is_open(void) {
    return is_open;
}

void menu_select_page(int p) {
    page = p;
}

void menu_set_focus_option(int opt_id) {
    focus_kind = K_OPT;
    focus_arg = opt_id;
}

/* ------------------------------------------------------------------ geometry helpers --- */
static float X(const Env *e, float u) {
    return e->ox + u * e->s;
}

static float Y(const Env *e, float u) {
    return e->oy + u * e->s;
}

static UiQuad quad(const Env *e, float x, float y, float w, float h, float skew) {
    return ui_quad_skew(X(e, x), Y(e, y), w * e->s, h * e->s, skew * e->s);
}

static void reg(Kind k, int arg, UiQuad q, int enabled) {
    if (n_items < MAX_ITEMS) {
        items[n_items].kind = k;
        items[n_items].arg = arg;
        items[n_items].q = q;
        items[n_items].enabled = enabled;
        n_items++;
    }
}

static void centre(const UiQuad *q, float *cx, float *cy) {
    *cx = (q->x[0] + q->x[1] + q->x[2] + q->x[3]) * 0.25f;
    *cy = (q->y[0] + q->y[1] + q->y[2] + q->y[3]) * 0.25f;
}

int menu_item_centre(int kind, int arg, int *x, int *y) {
    for (int i = 0; i < n_last; i++) {
        if ((int)last_items[i].kind == kind && last_items[i].arg == arg) {
            float cx, cy;
            centre(&last_items[i].q, &cx, &cy);
            *x = (int)cx;
            *y = (int)cy;
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ drawing helpers --- */
static void text_centred(const Env *e, UiFont *f, float size, const UiQuad *q, uint32_t colour, uint32_t outline, const char *s) {
    float cx, cy;
    centre(q, &cx, &cy);
    float px = size * e->s, w = ui_text_width(f, px, s);
    if (outline != 0xFFFFFFFF) {
        ui_text_outlined(e->ui, f, px, cx - w * 0.5f, cy + px * 0.34f, colour, outline, 2.2f * e->s, s);
    } else {
        ui_text(e->ui, f, px, cx - w * 0.5f, cy + px * 0.34f, colour, s);
    }
}

/* A comic panel: a hard black shadow, the fill, the thick outline. */
static void panel(const Env *e, UiQuad q, uint32_t fill, float shadow) {
    UiQuad sh = ui_quad_offset(q, shadow * e->s, shadow * e->s);
    ui_fill_quad(e->ui, &sh, INK, 255);
    ui_fill_quad(e->ui, &q, fill, 255);
}

static void outline(const Env *e, const UiQuad *q, float thickness, uint32_t colour) {
    ui_stroke_quad(e->ui, q, thickness * e->s, colour);
}

static void thick_line(Ui *ui, float x0, float y0, float x1, float y1, float w, uint32_t colour) {
    float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
    if (len < 0.01f) {
        return;
    }
    float nx = -dy / len * w * 0.5f, ny = dx / len * w * 0.5f;
    UiQuad q = {{x0 + nx, x1 + nx, x1 - nx, x0 - nx}, {y0 + ny, y1 + ny, y1 - ny, y0 - ny}};
    float area = 0.0f;
    for (int i = 0; i < 4; i++) {
        int j = (i + 1) & 3;
        area += q.x[i] * q.y[j] - q.x[j] * q.y[i];
    }
    if (area < 0.0f) { /* the quads must be clockwise */
        UiQuad r = {{q.x[3], q.x[2], q.x[1], q.x[0]}, {q.y[3], q.y[2], q.y[1], q.y[0]}};
        q = r;
    }
    ui_fill_quad(ui, &q, colour, 255);
}

static void checkbox(const Env *e, float x, float y, float size, int on, int enabled) {
    float s = size * e->s, px = X(e, x), py = Y(e, y);
    UiQuad q = ui_quad_rect(px, py, s, s);
    UiQuad sh = ui_quad_offset(q, 3 * e->s, 3 * e->s);
    ui_fill_quad(e->ui, &sh, INK, 255);
    ui_fill_quad(e->ui, &q, enabled ? WHITE : 0xD8D2C0, 255);
    ui_stroke_quad(e->ui, &q, 3.0f * e->s, INK);
    if (on) {
        thick_line(e->ui, px + s * 0.18f, py + s * 0.55f, px + s * 0.42f, py + s * 0.80f, s * 0.2f, enabled ? GREEN : GREY);
        thick_line(e->ui, px + s * 0.42f, py + s * 0.80f, px + s * 0.88f, py + s * 0.14f, s * 0.2f, enabled ? GREEN : GREY);
    }
}

static void soon_tag(const Env *e, float x, float y) {
    UiQuad q = quad(e, x, y, 86, 28, 6);
    panel(e, q, YELLOW, 3);
    outline(e, &q, 2.5f, INK);
    text_centred(e, font_title, 17, &q, INK, 0xFFFFFFFF, "SOON");
}

static UiQuad caption(const Env *e, float x, float y, float w, const char *title) {
    UiQuad q = quad(e, x, y, w, 46, 12);
    panel(e, q, YELLOW, 5);
    outline(e, &q, 4.0f, INK);
    text_centred(e, font_title, 30, &q, INK, 0xFFFFFFFF, title);
    return q;
}

static void button(const Env *e, Kind kind, int arg, float x, float y, float w, float h, uint32_t colour, const char *label, int enabled) {
    UiQuad q = quad(e, x, y, w, h, 14);
    panel(e, q, enabled ? colour : 0x8E8A7E, 7);
    ui_halftone_quad(e->ui, &q, 0xFFFFFF, 9.0f * e->s, 1.0f, 0.0f, 90);
    outline(e, &q, 5.0f, INK);
    float size = h * 0.55f, width = ui_text_width(font_title, size * e->s, label), room = (w - 40.0f) * e->s; /* the label is made smaller if it does not fit */
    if (width > room) {
        size *= room / width;
    }
    text_centred(e, font_title, size, &q, WHITE, INK, label);
    reg(kind, arg, q, enabled);
}

/* The description of the option that has the focus. */
static const char *focus_description(void) {
    if (focus_kind == K_OPT) {
        return opt_info((OptId)focus_arg)->description;
    }
    if (focus_kind == K_EDITION) {
        return focus_arg == 0 ? "Everything switched off: the game exactly as it was in 1990, checked against the original."
                              : "Every fix and option of the Whole Pizza edition switched on. Click a line to switch it off.";
    }
    if (focus_kind == K_LEVEL) {
        return "The level where a new game starts.";
    }
    if (focus_kind == K_TAB) {
        static const char *const tab_text[4] = {"The edition, the extras and the level of a new game.", "The sound level of the game.",
                                                "How the picture is drawn.", "The keys and the pad."};
        return tab_text[focus_arg & 3];
    }
    if (focus_kind == K_QUIT) {
        return "Leaves the game.";
    }
    if (focus_kind == K_PLAY) {
        return game_running ? "Goes back to the game where you left it." : "Starts a new game with the options above.";
    }
    return "";
}

static int focus_in_whole_pizza(void) {
    return (focus_kind == K_OPT && focus_arg >= OPT_FIX_CEILING && focus_arg <= OPT_NO_FLICKER) || (focus_kind == K_EDITION && focus_arg == 1);
}

static void narration(const Env *e, float x, float y, float w, float h) {
    UiQuad q = quad(e, x, y, w, h, 0);
    panel(e, q, 0xFFF9B0, 5);
    outline(e, &q, 4.0f, INK);
    ui_text_wrapped(e->ui, font_body, 20.0f * e->s, X(e, x + 16), Y(e, y + 30), (w - 32) * e->s, 25.0f * e->s, INK, focus_description());
}

/* ------------------------------------------------------------------ pages --- */
static void draw_background(const Env *e, const uint32_t *picture) {
    Ui *ui = e->ui;
    if (picture && game_running) {
        float k = fminf((float)ui->w / 256.0f, (float)ui->h / 240.0f);
        int w = (int)(256.0f * k), h = (int)(240.0f * k);
        ui_fill_rect(ui, 0, 0, ui->w, ui->h, NAVY, 255);
        ui_picture(ui, picture, 256, 240, (ui->w - w) / 2, (ui->h - h) / 2, w, h, 150);
    } else {
        ui_fill_rect(ui, 0, 0, ui->w, ui->h, NAVY, 255);
    }
    UiQuad full = ui_quad_rect(0, 0, (float)ui->w, (float)ui->h);
    ui_halftone_quad(ui, &full, 0x34509E, 14.0f * e->s, 0.0f, 1.0f, picture && game_running ? 120 : 255);
}

static void draw_tabs(const Env *e) {
    for (int i = 0; i < 4; i++) {
        float x = 40 + (float)i * 212.0f, y = i == page ? 14.0f : 22.0f;
        UiQuad q = quad(e, x, y, 200.0f, i == page ? 66.0f : 56.0f, 14.0f);
        panel(e, q, i == page ? YELLOW : PAPER, 5);
        if (i != page) {
            ui_halftone_quad(e->ui, &q, 0xC9BFA0, 8.0f * e->s, 1.0f, 0.0f, 255);
        }
        outline(e, &q, i == page ? 5.0f : 4.0f, INK);
        text_centred(e, font_title, 30, &q, i == page ? INK : 0x5A5346, 0xFFFFFFFF, page_names[i]);
        reg(K_TAB, i, q, 1);
    }
    button(e, K_QUIT, 0, 1010, 22, 230, 56, RED, "QUIT GAME", 1);
}

static void option_row(const Env *e, OptId id, float x, float y, float w, float h, int tick_only) {
    const OptInfo *oi = opt_info(id);
    UiQuad q = quad(e, x, y, w, h, 0);
    if (tick_only && opt_info(id)->max == 1) { /* only the tick answers to the mouse: the label and the rest are not the item */
        q = quad(e, x + w - 54, y + (h - 38) * 0.5f - 2, 42, 42, 0);
    }
    reg(K_OPT, (int)id, q, oi->wired);
    float px = 30.0f * e->s;
    ui_text(e->ui, font_title, px, X(e, x + 8), Y(e, y + h * 0.5f) + px * 0.34f, oi->wired ? INK : 0x7A7464, oi->label);
    if (oi->max == 1) {
        checkbox(e, x + w - 52, y + (h - 38) * 0.5f, 38, opt_get(id), oi->wired);
    } else { /* a slider */
        float tx = x + w - 380, ty = y + h * 0.5f, tw = 320;
        UiQuad track = quad(e, tx, ty - 7, tw, 14, 0);
        ui_fill_quad(e->ui, &track, WHITE, 255);
        outline(e, &track, 3.0f, INK);
        float frac = (float)(opt_get(id) - oi->min) / (float)(oi->max - oi->min);
        UiQuad fillq = quad(e, tx, ty - 7, tw * frac, 14, 0);
        ui_fill_quad(e->ui, &fillq, GREEN, 255);
        UiQuad knob = quad(e, tx + tw * frac - 11, ty - 20, 22, 40, 4);
        panel(e, knob, YELLOW, 3);
        outline(e, &knob, 3.0f, INK);
        char v[8];
        snprintf(v, sizeof v, "%d", opt_get(id));
        ui_text(e->ui, font_title, 28.0f * e->s, X(e, tx + tw + 30), Y(e, ty) + 10.0f * e->s, INK, v);
    }
    if (!oi->wired) {
        soon_tag(e, x + w - 160, y + (h - 28) * 0.5f);
    }
}

static void draw_options_page(const Env *e, const OptId *ids, int count, const char *title) {
    UiQuad q = quad(e, 40, 112, 1200, 500, 10);
    panel(e, q, PAPER, 10);
    ui_halftone_quad(e->ui, &q, 0xF3D9A0, 12.0f * e->s, 1.0f, 0.0f, 255);
    outline(e, &q, 6.0f, INK);
    caption(e, 70, 94, 280, title);
    for (int i = 0; i < count; i++) {
        option_row(e, ids[i], 90, 170 + (float)i * 70.0f, 1060, 60, 0);
    }
    narration(e, 40, 628, 1200, 76);
}

static const char *const keys_list[][2] = {
    {"MOVE", "ARROW KEYS"}, {"ATTACK (B)", "Z OR W"}, {"JUMP (A)", "X"}, {"START", "ENTER"}, {"SELECT", "TAB OR RIGHT SHIFT"}, {"MENU", "ESC"}, {"PAUSE", "P"},
    {"MUTE", "M"},          {"RESTART", "F5"},        {"REWIND", "F9"},  {"CAPTURE", "F10"},
};

static const char *const pad_list[][2] = {
    {"MOVE", "DPAD OR LEFT STICK"}, {"JUMP (A)", "A"},       {"ATTACK (B)", "X OR B"},   {"START", "START"},
    {"SELECT", "Y (WEAPON / VAN)"}, {"MENU", "VIEW / BACK"}, {"MENU TABS", "LB AND RB"}, {"REWIND", "LEFT TRIGGER"},
};

static void controls_list(const Env *e, const char *const (*list)[2], int count, float x, float y, float step, float value_x) {
    for (int i = 0; i < count; i++) {
        float px = 25.0f * e->s, yy = y + (float)i * step;
        ui_text(e->ui, font_title, px, X(e, x), Y(e, yy) + px * 0.35f, INK, list[i][0]);
        ui_text(e->ui, font_body, px, X(e, x + value_x), Y(e, yy) + px * 0.35f, 0x2F6FD6, list[i][1]);
    }
}

static void draw_controls_page(const Env *e) {
    UiQuad left = quad(e, 40, 112, 580, 500, 10), right = quad(e, 660, 112, 580, 500, 10);
    UiQuad *sides[2] = {&left, &right};
    for (int k = 0; k < 2; k++) {
        panel(e, *sides[k], PAPER, 10);
        ui_halftone_quad(e->ui, sides[k], k == 0 ? 0xF3D9A0 : 0xA9E0B0, 12.0f * e->s, 1.0f, 0.0f, 255);
        outline(e, sides[k], 6.0f, INK);
    }
    caption(e, 70, 94, 280, "KEYBOARD");
    caption(e, 690, 94, 280, "GAMEPAD");
    controls_list(e, keys_list, 11, 70, 176, 34.0f, 210);
    controls_list(e, pad_list, 8, 690, 176, 40.0f, 205);
    UiQuad note = quad(e, 690, 506, 520, 70, 0);
    panel(e, note, 0xFFF9B0, 4);
    outline(e, &note, 3.5f, INK);
    ui_text_wrapped(e->ui, font_body, 18.0f * e->s, X(e, 704), Y(e, 534), 492 * e->s, 23.0f * e->s, INK,
                    "The shortcuts H J E C S R F L I still work. Changing the keys comes soon.");
    UiQuad soon = quad(e, 70, 560, 260, 40, 8);
    panel(e, soon, 0xD8D2C0, 3);
    outline(e, &soon, 3.5f, INK);
    text_centred(e, font_title, 22, &soon, 0x7A7464, 0xFFFFFFFF, "CHANGE KEYS");
    soon_tag(e, 346, 566);
    reg(K_STATIC, 0, soon, 0);
}

static const char *const original_text = "THE GAME EXACTLY AS IT WAS IN 1990.";
static const char *const original_text2 = "CHECKED BIT FOR BIT AGAINST THE ORIGINAL (A 15 MINUTE SPEED RUN REPLAYS WITHOUT ONE DIFFERENCE).";

static void draw_edition(const Env *e, int ed, float x, float y, float w, float h) {
    UiQuad q = quad(e, x, y, w, h, 12);
    int selected = opt_get(OPT_EDITION) == ed;
    panel(e, q, PAPER, 10);
    ui_halftone_quad(e->ui, &q, ed == 0 ? 0x9DB8F0 : 0xF7B67A, 11.0f * e->s, 1.0f, 0.0f, 255);
    outline(e, &q, 6.0f, INK);
    if (selected) {
        UiQuad glow = ui_quad_offset(q, 0, 0);
        ui_stroke_quad(e->ui, &glow, 15.0f * e->s, YELLOW);
        outline(e, &q, 6.0f, INK);
    }
    if (focus_kind == K_EDITION && focus_arg == ed) { /* the focus, under the title and the tag (drawn next), not over them */
        float pulse = 1.0f + 0.25f * sinf((float)tick * 0.18f);
        ui_stroke_quad(e->ui, &q, 9.0f * e->s * pulse, WHITE);
        ui_stroke_quad(e->ui, &q, 4.5f * e->s, ORANGE);
    }
    UiQuad cap = caption(e, x + 30, y - 24, 270, ed == 0 ? "ORIGINAL" : "WHOLE PIZZA");
    reg(K_EDITION, ed, cap, 1); /* the title selects the edition... */
    reg(K_EDITION, ed, q, 1);   /* ...and so does the panel, under its lines (they are registered after it) */
    if (selected) {
        const char *tag = options_edition_matches(ed) ? "SELECTED" : "MODIFIED *";
        UiQuad tq = quad(e, x + w - 200, y - 18, 170, 38, 10);
        panel(e, tq, options_edition_matches(ed) ? GREEN : ORANGE, 4);
        outline(e, &tq, 3.5f, INK);
        text_centred(e, font_title, 22, &tq, WHITE, INK, tag);
    }
    if (ed == 0) {
        ui_text(e->ui, font_title, 64.0f * e->s, X(e, x + 40), Y(e, y + 130), 0x2F6FD6, "TMNT");
        ui_text_outlined(e->ui, font_title, 64.0f * e->s, X(e, x + 40), Y(e, y + 130), YELLOW, INK, 3.0f * e->s, "TMNT");
        ui_text_wrapped(e->ui, font_body, 25.0f * e->s, X(e, x + 40), Y(e, y + 190), (w - 100) * e->s, 31.0f * e->s, INK, original_text);
        ui_text_wrapped(e->ui, font_body, 21.0f * e->s, X(e, x + 40), Y(e, y + 250), (w - 100) * e->s, 27.0f * e->s, INK, original_text2);
    } else {
        static const OptId rows[9] = {OPT_FIX_CEILING, OPT_FIX_WATER, OPT_FIX_HITBOX, OPT_FIX_CAMERA, OPT_SPAWN_MASK,
                                      OPT_BAL_DAMAGE,  OPT_BAL_REACH, OPT_LARGE_VIEW, OPT_NO_FLICKER};
        for (int i = 0; i < 9; i++) {
            float ry = y + 34 + (float)i * 31.0f;
            UiQuad rq = quad(e, x + 28, ry + 1, 28, 28, 0); /* only the tick answers to a click: the label and the rest of the panel select the edition */
            reg(K_OPT, (int)rows[i], rq, 1);
            checkbox(e, x + 30, ry + 2, 24, opt_get(rows[i]), 1);
            ui_text(e->ui, font_body, 22.0f * e->s, X(e, x + 68), Y(e, ry + 22), INK, opt_info(rows[i])->label);
        }
        if (logo) {
            float lh = 118.0f, lw = lh * (float)ui_image_w(logo) / (float)ui_image_h(logo);
            ui_image_draw(e->ui, logo, X(e, x + w - 50 - lw), Y(e, y + 30), lw * e->s, lh * e->s, 255, 1);
        }
        /* what the line under the focus (or the mouse) does, at the bottom right of the panel */
        UiQuad bubble = quad(e, x + w - 292, y + h - 168, 270, 148, 0);
        panel(e, bubble, 0xFFF9B0, 4);
        outline(e, &bubble, 3.5f, INK);
        const char *text = focus_in_whole_pizza() && focus_kind == K_OPT
                               ? opt_info((OptId)focus_arg)->description
                               : "Click a tick to switch one line on or off. Click anywhere else in the panel for the whole edition.";
        ui_text_wrapped(e->ui, font_body, 18.0f * e->s, X(e, x + w - 278), Y(e, y + h - 142), 242 * e->s, 23.0f * e->s, INK, text);
    }
}

static void draw_play_page(const Env *e) {
    draw_edition(e, 0, 40, 124, 580, 330);
    draw_edition(e, 1, 660, 124, 580, 330);

    /* extras */
    UiQuad ex = quad(e, 40, 486, 390, 214, 8);
    panel(e, ex, PAPER, 8);
    ui_halftone_quad(e->ui, &ex, 0xF3D9A0, 10.0f * e->s, 1.0f, 0.0f, 255);
    outline(e, &ex, 5.0f, INK);
    caption(e, 66, 468, 200, "EXTRAS");
    option_row(e, OPT_INVINCIBLE, 56, 524, 350, 40, 1);
    option_row(e, OPT_HITBOX_VIEW, 56, 566, 350, 40, 1);
    if (!focus_in_whole_pizza()) { /* the description of what has the focus (the Whole Pizza lines show theirs in their own panel) */
        ui_text_wrapped(e->ui, font_body, 18.0f * e->s, X(e, 62), Y(e, 634), 330 * e->s, 22.0f * e->s, INK, focus_description());
    }

    /* level strip */
    UiQuad lv = quad(e, 450, 486, 500, 214, 8);
    panel(e, lv, PAPER, 8);
    ui_halftone_quad(e->ui, &lv, 0xA9E0B0, 10.0f * e->s, 1.0f, 0.0f, 255);
    outline(e, &lv, 5.0f, INK);
    caption(e, 476, 468, 200, "LEVEL");
    for (int i = 1; i <= 6; i++) {
        float x = 476.0f + (float)(i - 1) * 78.0f;
        int sel = opt_get(OPT_START_LEVEL) == i;
        UiQuad cq = quad(e, x, sel ? 530.0f : 540.0f, 66.0f, sel ? 130.0f : 120.0f, 8.0f);
        panel(e, cq, sel ? YELLOW : WHITE, 4);
        ui_halftone_quad(e->ui, &cq, sel ? 0xF0A800 : 0xC9C2AE, 7.0f * e->s, 1.0f, 0.0f, 255);
        outline(e, &cq, sel ? 5.0f : 3.5f, INK);
        char n[4];
        snprintf(n, sizeof n, "%d", i);
        text_centred(e, font_title, 54, &cq, sel ? RED : INK, WHITE, n);
        reg(K_LEVEL, i, cq, 1);
    }

    /* buttons */
    button(e, K_PLAY, 0, 990, 520, 250, 150, GREEN, game_running ? "RESUME" : "PLAY", 1); /* one button: it starts the game, or goes back to it */
}

static void draw_modal(const Env *e) {
    ui_fill_rect(e->ui, 0, 0, e->ui->w, e->ui->h, 0x000000, 150);
    UiQuad q = quad(e, 340, 240, 600, 250, 12);
    panel(e, q, PAPER, 12);
    ui_halftone_quad(e->ui, &q, 0xF3D9A0, 11.0f * e->s, 1.0f, 0.0f, 255);
    outline(e, &q, 6.0f, INK);
    text_centred(e, font_title, 46, &(UiQuad){{X(e, 340), X(e, 940), X(e, 940), X(e, 340)}, {Y(e, 262), Y(e, 262), Y(e, 362), Y(e, 362)}}, INK, WHITE,
                 "QUIT THE GAME?");
    button(e, K_YES, 0, 390, 380, 200, 76, RED, "YES", 1);
    button(e, K_NO, 0, 690, 380, 200, 76, GREEN, "NO", 1);
}

/* ------------------------------------------------------------------ drawing: a whole frame --- */
void menu_draw(Ui *ui, const uint32_t *picture) {
    Env e;
    e.ui = ui;
    e.s = fminf((float)ui->w / DESIGN_W, (float)ui->h / DESIGN_H);
    e.ox = ((float)ui->w - DESIGN_W * e.s) * 0.5f;
    e.oy = ((float)ui->h - DESIGN_H * e.s) * 0.5f;
    n_items = 0;
    draw_background(&e, picture);
    if (quit_modal) {
        /* the page is drawn dimmed behind: its items are not recorded */
    }
    draw_tabs(&e);
    static const OptId sound[] = {OPT_VOLUME, OPT_MUTE};
    static const OptId graphics[] = {OPT_LARGE_VIEW, OPT_NO_FLICKER, OPT_HITBOX_VIEW, OPT_FULLSCREEN, OPT_INTEGER_SCALE, OPT_SHOW_FPS};
    switch (page) {
        case 0:
            draw_play_page(&e);
            break;
        case 1:
            draw_options_page(&e, sound, 2, "SOUND");
            break;
        case 2:
            draw_options_page(&e, graphics, 6, "GRAPHICS");
            break;
        default:
            draw_controls_page(&e);
            break;
    }
    if (quit_modal) {
        n_items = 0; /* only the two buttons count */
        draw_modal(&e);
    }
    /* the focus: a thick white outline that pulses, with a yellow one inside */
    for (int i = n_items - 1; i >= 0; i--) {
        if (items[i].kind == focus_kind && items[i].arg == focus_arg && items[i].enabled && items[i].kind != K_EDITION) { /* (an edition draws its own) */
            float pulse = 1.0f + 0.25f * sinf((float)tick * 0.18f);
            ui_stroke_quad(ui, &items[i].q, 9.0f * e.s * pulse, WHITE);
            ui_stroke_quad(ui, &items[i].q, 4.5f * e.s, ORANGE);
            break;
        }
    }
    memcpy(last_items, items, sizeof items);
    n_last = n_items;
}

/* ------------------------------------------------------------------ input --- */
static int find_focus(void) {
    for (int i = n_last - 1; i >= 0; i--) { /* the last quad: the panel of an edition rather than its title */
        if (last_items[i].kind == focus_kind && last_items[i].arg == focus_arg) {
            return i;
        }
    }
    return -1;
}

static void set_focus(int i) {
    focus_kind = last_items[i].kind;
    focus_arg = last_items[i].arg;
}

/* The ticks of the Whole Pizza lines: reached from the edition (down), not from the other items. */
static int is_pizza_tick(Kind kind, int arg) {
    return page == 0 && kind == K_OPT && arg >= OPT_FIX_CEILING && arg <= OPT_NO_FLICKER;
}

static void focus_on(Kind kind, int arg) {
    focus_kind = kind;
    focus_arg = arg;
}

static void move_focus(int dx, int dy) {
    if (focus_kind == K_EDITION && focus_arg == 1 && dy > 0 && page == 0) { /* Whole Pizza: down enters its list of ticks */
        focus_on(K_OPT, OPT_FIX_CEILING);
        return;
    }
    if (is_pizza_tick(focus_kind, focus_arg)) { /* inside the list: up and down walk it, left goes back to the other edition */
        if (dy < 0) {
            if (focus_arg == OPT_FIX_CEILING) {
                focus_on(K_EDITION, 1);
            } else {
                focus_on(K_OPT, focus_arg - 1);
            }
            return;
        }
        if (dy > 0 && focus_arg < OPT_NO_FLICKER) {
            focus_on(K_OPT, focus_arg + 1);
            return;
        }
        if (dx < 0) {
            focus_on(K_EDITION, 0);
            return;
        }
        if (dx > 0) {
            return;
        }
    }
    int cur = find_focus();
    if (cur < 0) {
        for (int i = 0; i < n_last; i++) {
            if (last_items[i].enabled) {
                set_focus(i);
                return;
            }
        }
        return;
    }
    float cx, cy;
    centre(&last_items[cur].q, &cx, &cy);
    int best = -1;
    for (int pass = 0; pass < 2 && best < 0; pass++) { /* first the items inside a cone of 45 degrees around the direction, then any item that lies that way */
        float best_score = 1e18f;
        for (int i = 0; i < n_last; i++) {
            if (!last_items[i].enabled || (last_items[i].kind == focus_kind && last_items[i].arg == focus_arg) ||
                is_pizza_tick(last_items[i].kind, last_items[i].arg)) {
                continue; /* (an edition has several quads: its title and its panel; the ticks of Whole Pizza are reached from the edition) */
            }
            float ix, iy;
            centre(&last_items[i].q, &ix, &iy);
            float along = dx ? (ix - cx) * (float)dx : (iy - cy) * (float)dy, across = dx ? fabsf(iy - cy) : fabsf(ix - cx);
            if (along < 8.0f || (pass == 0 && across > along)) {
                continue;
            }
            float score = along + 2.5f * across;
            if (score < best_score) {
                best_score = score;
                best = i;
            }
        }
    }
    if (best >= 0) {
        set_focus(best);
    }
}

static void save_options(void) {
    if (ini_file[0]) {
        options_save(ini_file);
    }
}

static int drag_opt = -1; /* the slider that the mouse is dragging */

/* The value of a slider for a mouse position (the track is the right part of the row, see option_row). */
static void slider_from_x(int arg, int mouse_x) {
    const OptInfo *oi = opt_info((OptId)arg);
    for (int i = 0; i < n_last; i++) {
        if (last_items[i].kind == K_OPT && last_items[i].arg == arg) {
            float x0 = last_items[i].q.x[3], x1 = last_items[i].q.x[2];
            float tx0 = x1 - 380.0f * (x1 - x0) / 1060.0f, tw = 320.0f * (x1 - x0) / 1060.0f;
            float frac = ((float)mouse_x - tx0) / tw;
            frac = frac < 0 ? 0 : frac > 1 ? 1 : frac;
            opt_set_user((OptId)arg, oi->min + (int)(frac * (float)(oi->max - oi->min) + 0.5f));
        }
    }
}

static MenuAction activate(Kind k, int arg, int mouse_x) {
    switch (k) {
        case K_TAB:
            page = arg;
            break;
        case K_EDITION:
            options_apply_edition(arg);
            save_options();
            break;
        case K_OPT: {
            const OptInfo *oi = opt_info((OptId)arg);
            if (!oi->wired) {
                break;
            }
            if (oi->max == 1) {
                opt_set_user((OptId)arg, !opt_get((OptId)arg));
                if (arg >= OPT_FIX_CEILING &&
                    arg <= OPT_NO_FLICKER) { /* the edition is the one whose preset is nearest: it stays as chosen, "modified" shows */
                    if (options_edition_matches(0)) {
                        opt_set_user(OPT_EDITION, 0);
                    } else if (options_edition_matches(1)) {
                        opt_set_user(OPT_EDITION, 1);
                    }
                }
            } else if (mouse_x >= 0) { /* a click on the slider: the value follows the mouse while the button is held */
                slider_from_x(arg, mouse_x);
                drag_opt = arg;
            }
            save_options();
            break;
        }
        case K_LEVEL:
            opt_set(OPT_START_LEVEL, arg);
            break;
        case K_PLAY: /* one button: a new game at the launcher, back to the game when one is running */
            is_open = 0;
            return game_running ? MENU_RESUME : MENU_NEW_GAME;
        case K_QUIT:
            quit_modal = 1;
            focus_kind = K_NO;
            focus_arg = 0;
            break;
        case K_YES:
            return MENU_QUIT;
        case K_NO:
            quit_modal = 0;
            focus_kind = K_QUIT;
            focus_arg = 0;
            break;
        default:
            break;
    }
    return MENU_NONE;
}

MenuAction menu_update(const MenuInput *in) {
    tick++;
    if (find_focus() < 0 && n_last > 0) { /* the focused item is not on this page: the page's own tab */
        focus_kind = K_TAB;
        focus_arg = page;
    }
    if (drag_opt >= 0) { /* a slider is being dragged: it follows the mouse until the button is let go */
        if (in->mouse_down && in->mouse_x >= 0) {
            slider_from_x(drag_opt, in->mouse_x);
            return MENU_NONE;
        }
        drag_opt = -1;
        save_options();
    }
    if (in->escape) {
        if (quit_modal) {
            quit_modal = 0;
            focus_kind = K_QUIT;
            focus_arg = 0;
            return MENU_NONE;
        }
        if (game_running) {
            is_open = 0;
            return MENU_RESUME;
        }
        return MENU_NONE;
    }
    if (in->start && !quit_modal) { /* Start of a pad: play, or resume the game, wherever the focus is */
        is_open = 0;
        return game_running ? MENU_RESUME : MENU_NEW_GAME;
    }
    if (in->back) { /* B: back out of the question, or out of the menu if a game is under it */
        if (quit_modal) {
            quit_modal = 0;
            focus_kind = K_QUIT;
            focus_arg = 0;
            return MENU_NONE;
        }
        if (game_running) {
            is_open = 0;
            return MENU_RESUME;
        }
    }
    if ((in->tab_prev || in->tab_next) && !quit_modal) { /* LB and RB turn the pages */
        page = (page + 4 + in->tab_next - in->tab_prev) % 4;
        focus_kind = K_TAB;
        focus_arg = page;
    }
    /* the mouse: the focus follows it, a click activates */
    if (in->mouse_x >= 0 &&
        ((last_mx >= 0 && (in->mouse_x != last_mx || in->mouse_y != last_my)) || in->clicked)) { /* (the first sample only says where the cursor rests) */
        for (int i = n_last - 1; i >= 0; i--) {
            if (last_items[i].enabled && ui_quad_contains(&last_items[i].q, (float)in->mouse_x, (float)in->mouse_y)) {
                set_focus(i);
                if (in->clicked) {
                    return activate(last_items[i].kind, last_items[i].arg, in->mouse_x);
                }
                break;
            }
        }
    }
    last_mx = in->mouse_x;
    last_my = in->mouse_y;
    int cur = find_focus();
    if (in->left || in->right) { /* a slider takes left and right, otherwise they move the focus */
        if (focus_kind == K_OPT && opt_info((OptId)focus_arg)->max > 1) {
            const OptInfo *oi = opt_info((OptId)focus_arg);
            opt_set_user((OptId)focus_arg, opt_get((OptId)focus_arg) + (in->right - in->left) * 5);
            (void)oi;
            save_options();
        } else {
            move_focus(in->right - in->left, 0);
        }
    }
    if (in->up || in->down) {
        move_focus(0, in->down - in->up);
    }
    if (in->confirm && cur >= 0 && last_items[cur].enabled) {
        return activate(last_items[cur].kind, last_items[cur].arg, -1);
    }
    return MENU_NONE;
}
