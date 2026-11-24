/* Comic-book sound effects: see comic.h. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "comic.h"
#include "game.h"
#include "options.h"
#include "platform.h"
#include "ram_map.h"
#include "ui.h"

#define MAX_EVENTS 16
#define LIFE 14    /* frames that a word stays on the screen */
#define MIN_GAP 10 /* frames before the same entity gets another word */
#define ROW_LIMIT 192

typedef struct {
    uint32_t born;
    int wx, wy; /* the world position of the place that was hit */
    const char *word;
    float tilt; /* the angle of the star behind the word */
    int hurt;   /* the word of the player (a different colour) */
} Event;

static Event events[MAX_EVENTS];
static uint32_t last_word[16]; /* per entity: the frame of its last word (+1: 0 = none) */
static uint32_t last_hurt;
static UiFont *font;

void comic_init(const char *data_dir) {
    char path[1200];
    snprintf(path, sizeof path, "%s/fonts/KOMIKAHB.ttf", data_dir);
    font = ui_font_load(path);
}

static unsigned mix(unsigned h) {
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    h *= 3266489917u;
    h ^= h >> 16;
    return h;
}

static void add(Game *g, int wx, int wy, const char *word, int hurt) {
    unsigned h = mix((unsigned)wx * 73856093u ^ (unsigned)wy * 19349663u ^ g->frame * 2654435761u);
    Event *slot = &events[0];
    for (int i = 0; i < MAX_EVENTS; i++) { /* a free place, else the oldest word */
        if (events[i].word == NULL || g->frame - events[i].born >= LIFE) {
            slot = &events[i];
            break;
        }
        if (events[i].born < slot->born) {
            slot = &events[i];
        }
    }
    slot->born = g->frame;
    slot->wx = wx;
    slot->wy = wy;
    slot->word = word;
    slot->tilt = (float)(h & 255) * 0.0245f;
    slot->hurt = hurt;
}

static int camera_x(Game *g) {
    return g->ram[CameraXLo] | (g->ram[CameraXHi] << 8);
}

static int camera_y(Game *g) {
    return g->ram[CameraYLo] | (g->ram[CameraYHi] << 8);
}

static int may_speak(Game *g, int x) {
    if (!opt_get(OPT_ATMOSPHERE) || !font || x < 0 || x > 15) {
        return 0;
    }
    if (last_word[x] && g->frame - (last_word[x] - 1) < MIN_GAP) {
        return 0;
    }
    if (mix(g->frame * 40503u + (unsigned)x * 9973u + 17u) % 5u != 0) { /* only now and then: one blow in five */
        return 0;
    }
    last_word[x] = g->frame + 1;
    return 1;
}

void comic_hit(Game *g, int x) {
    if (!may_speak(g, x)) {
        return;
    }
    static const char *normal[4] = {"POW!", "BAM!", "WHAP!", "SMACK!"};
    const char *word = (g->ram[EntStatus] & 0x10) ? "WHAM!" : normal[mix(g->frame + (unsigned)x) & 3u]; /* a blow from the air is the big one */
    add(g, camera_x(g) + g->ram[EntX + x], camera_y(g) + g->ram[EntY + x] - 12, word, 0);
}

void comic_kill(Game *g, int x) {
    if (!may_speak(g, x)) {
        return;
    }
    static const char *big[3] = {"KO!", "CRASH!", "BOOM!"};
    add(g, camera_x(g) + g->ram[EntX + x], camera_y(g) + g->ram[EntY + x] - 12, big[mix(g->frame + (unsigned)x * 7u) % 3u], 0);
}

void comic_hurt(Game *g) {
    if (!opt_get(OPT_ATMOSPHERE) || !font || (last_hurt && g->frame - (last_hurt - 1) < MIN_GAP)) {
        return;
    }
    if (mix(g->frame * 31337u + 5u) % 5u != 0) {
        return;
    }
    last_hurt = g->frame + 1;
    static const char *ouch[3] = {"OUCH!", "UGH!", "ARGH!"};
    add(g, camera_x(g) + g->ram[EntX], camera_y(g) + g->ram[EntY] - 24, ouch[mix(g->frame) % 3u], 1);
}

void comic_draw(Game *g, uint32_t *screen) {
    if (!opt_get(OPT_ATMOSPHERE) || !font || g->ram[GameMode] != 4) {
        return;
    }
    Ui view = {screen, SCREEN_W, SCREEN_H}; /* the picture of the game as a surface of the user interface layer (nothing is allocated or freed) */
    int cx = camera_x(g), cy = camera_y(g);
    for (int i = 0; i < MAX_EVENTS; i++) {
        Event *e = &events[i];
        if (!e->word) {
            continue;
        }
        if (e->born > g->frame + 1u || g->frame - e->born >= LIFE) { /* gone (or from a time that a rewind took back) */
            e->word = NULL;
            continue;
        }
        float age = (float)(g->frame - e->born);
        float s = age < 2.0f ? 0.6f + 0.2f * age : 1.0f; /* it pops... */
        if (age > 9.0f) {
            s *= ((float)LIFE - age) / 5.0f; /* ...and is gone soon */
        }
        float x = (float)(e->wx - cx), y = (float)(e->wy - cy) - age * 0.3f;
        if (y < 14.0f || y > (float)ROW_LIMIT + 8.0f || x < -30.0f || x > (float)SCREEN_W + 30.0f) {
            continue;
        }
        float size = 8.5f * s, w = ui_text_width(font, size, e->word);
        uint32_t star = e->hurt ? 0xFF8A3D : 0xFFE03A, edge = e->hurt ? 0x7A1E00 : 0xC4141C, ink = e->hurt ? 0xFFFFFF : 0xD7191F;
        /* see-through: draw the word, then mix it with what the picture was */
        int x0 = (int)(x - w * 0.6f - 8.0f), x1 = (int)(x + w * 0.6f + 8.0f), y0 = (int)(y - 22.0f), y1 = (int)(y + 8.0f);
        x0 = x0 < 0 ? 0 : x0;
        x1 = x1 > SCREEN_W ? SCREEN_W : x1;
        y0 = y0 < 0 ? 0 : y0;
        y1 = y1 > SCREEN_H ? SCREEN_H : y1;
        if (x0 >= x1 || y0 >= y1) {
            continue;
        }
        static uint32_t before[SCREEN_W * SCREEN_H];
        for (int yy = y0; yy < y1; yy++) {
            memcpy(&before[yy * SCREEN_W + x0], &screen[yy * SCREEN_W + x0], (size_t)(x1 - x0) * sizeof(uint32_t));
        }
        ui_fill_burst(&view, x, y - 4.0f * s, w * 0.56f + 3.5f * s, w * 0.34f + 1.0f * s, 10, e->tilt, edge);
        ui_fill_burst(&view, x, y - 4.0f * s, w * 0.56f + 2.0f * s, w * 0.34f, 10, e->tilt, star);
        ui_text_outlined(&view, font, size, x - w * 0.5f, y, ink, 0x000000, 1.0f * s, e->word);
        int a256 = (int)(150.0f * (s < 1.0f ? s : 1.0f)); /* about 60% ink at full size */
        for (int yy = y0; yy < y1; yy++) {
            for (int xx = x0; xx < x1; xx++) {
                uint32_t n = screen[yy * SCREEN_W + xx], o = before[yy * SCREEN_W + xx];
                if (n == o) {
                    continue;
                }
                uint32_t r = 0;
                for (int sh = 0; sh <= 16; sh += 8) {
                    r |= (((((n >> sh) & 255u) * (unsigned)a256 + ((o >> sh) & 255u) * (unsigned)(256 - a256)) >> 8) & 255u) << sh;
                }
                screen[yy * SCREEN_W + xx] = (n & 0xFF000000u) | r;
            }
        }
    }
}
