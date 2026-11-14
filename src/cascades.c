/* Waterfalls of the sewers: see cascades.h. */
#include <math.h>
#include <string.h>
#include "cascades.h"
#include "game.h"
#include "options.h"
#include "platform.h"
#include "ram_map.h"

#define ROW_LIMIT 192 /* the playfield: below it is the status bar */

typedef struct {
    int level, area;
    int x, y, w, h; /* in the world of the area: left, top, width, height (the water stops at y + h, where the foam is) */
} Cascade;

/* The first sewer (level 1, area 0, a map of 1024 pixels): the room of the mid boss (the camera between 508 and 764) has two waterfalls. They start at the bottom of the
 * ceiling (the last row of its bricks is the row 19 of the picture) and fall to the floor of the gallery. */
static const Cascade cascades[] = {
    {0, 0, 610, 20, 16, 156}, {0, 0, 700, 20, 8, 156},
    {0, 1, 544, 20, 16, 156},                           /* the second sewer (area 1): the room of the fight against Bebop, the camera locked at 508 */
    {0, 1, 616, 20, 8, 156},  {0, 3, 400, 20, 16, 156}, /* the third sewer (area 3): the room of the boss, the camera locked at 251 */
};

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static unsigned mix(unsigned h) {
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    h *= 3266489917u;
    h ^= h >> 16;
    return h;
}

static void blend(uint32_t *px, float r, float g, float b, float alpha) {
    uint32_t c = *px;
    float cr = (float)((c >> 16) & 255), cg = (float)((c >> 8) & 255), cb = (float)(c & 255);
    cr += (r - cr) * alpha;
    cg += (g - cg) * alpha;
    cb += (b - cb) * alpha;
    *px = ((uint32_t)clampf(cr, 0.0f, 255.0f) << 16) | ((uint32_t)clampf(cg, 0.0f, 255.0f) << 8) | (uint32_t)clampf(cb, 0.0f, 255.0f);
}

/* One column: the water is the picture behind it, tinted blue and read with a vertical shift that shakes with the time (this is what makes it fall); the edges are teeth,
 * the top is wavy, and the foam is where the swell of sines is low. */
static void draw_column(uint32_t *screen, const uint32_t *behind, int sx, int sy, int w, int h, float t) {
    for (int j = 0; j < h; j++) {
        int py = sy + j;
        if (py < 0 || py >= ROW_LIMIT) {
            continue;
        }
        for (int i = 0; i < w; i++) {
            int px = sx + i;
            if (px < 0 || px >= SCREEN_W) {
                continue;
            }
            float fx = (float)px, fy = (float)py, u = ((float)i + 0.5f) / (float)w;
            float w1 = (sinf(23.0f * t + fx - 0.3f * fy) + sinf(13.0f * t + 2.7f * fx - 0.2f * fy)) * 0.5f + 0.5f;
            if (u < w1 * 0.125f || u > 0.875f + w1 * 0.125f) { /* the edges are teeth */
                continue;
            }
            float w2 = (sinf(5.0f * t - 0.5f * fx) + sinf(-10.0f * t)) * 0.5f + 0.5f;
            if ((float)j < w2 * 5.0f) { /* the top is wavy */
                continue;
            }
            float d = sinf(fx) + sinf(2.8f * fx + 0.25f * fy - 13.7f * t) + sinf(2.1f * fx + 0.0625f * fy - 11.0f * t) + 0.8f * sinf(0.25f * fy - 3.0f * t);
            int foam = (float)j < w2 * 7.0f || d <= -1.5f;
            float shake = 0.5f * (sinf(7.0f * t + 10.0f * fx) + sinf(19.2f * t + 3.0f * fy) + sinf(7.0f * fx + 24.3f * t));
            int by = py + (int)lroundf(shake * 1.5f);
            by = by < 0 ? 0 : by >= ROW_LIMIT ? ROW_LIMIT - 1 : by;
            uint32_t s = behind[by * SCREEN_W + px];
            float sr = (float)((s >> 16) & 255) / 255.0f, sg = (float)((s >> 8) & 255) / 255.0f, sb = (float)(s & 255) / 255.0f;
            float r, g, b;
            if (i == 0 || i == w - 1) { /* the edge: the picture pulled a little towards the blue */
                r = sr + 0.25f * (0.4f - sr);
                g = sg + 0.25f * (0.4f - sg);
                b = sb + 0.25f * (1.0f - sb);
            } else { /* the water: the picture with less red and green, and a little blue */
                r = 0.7f * sr + 0.02f;
                g = 0.78f * sg + 0.05f;
                b = sb + 0.12f * (1.0f - sb) + 0.05f;
                if (foam) {
                    r += 0.30f;
                    g += 0.38f;
                    b += 0.38f;
                }
            }
            screen[py * SCREEN_W + px] = ((uint32_t)(clampf(r, 0.0f, 1.0f) * 255.0f) << 16) | ((uint32_t)(clampf(g, 0.0f, 1.0f) * 255.0f) << 8) |
                                         (uint32_t)(clampf(b, 0.0f, 1.0f) * 255.0f);
        }
    }
}

/* The foot: a row of puffs of foam that overlap (every 4 pixels), whose pixels change every 4 frames, and a splash that jumps around each of them. */
static void draw_foot(uint32_t *screen, int sx, int sy, int w, unsigned frame) {
    for (int i = 0; i <= w / 4; i++) {
        int x = sx - 4 + 4 * i;
        unsigned step = (frame / 4u + (unsigned)i) % 8u;
        for (int n = 0; n < 14; n++) { /* a puff: scattered pale pixels in a box of 8 x 6 */
            unsigned h = mix((unsigned)(i * 131 + n * 17) + step * 977u);
            int px = x + (int)(h % 8u), py = sy - 4 + (int)((h >> 8) % 6u);
            if (px >= 0 && px < SCREEN_W && py >= 0 && py < ROW_LIMIT) {
                blend(&screen[py * SCREEN_W + px], 215.0f, 255.0f, 235.0f, 0.75f);
            }
        }
        unsigned r = mix((unsigned)(i * 7919) + frame * 2654435761u); /* the splash: a few pixels of foam that jump to another place every frame */
        int jx = x + 4 + (int)(r % 9u) - 4, jy = sy - 6 + (int)((r >> 8) % 9u) - 4;
        for (int n = 0; n < 3; n++) {
            int px = jx + n % 2, py = jy + n / 2;
            if (px >= 0 && px < SCREEN_W && py >= 0 && py < ROW_LIMIT) {
                blend(&screen[py * SCREEN_W + px], 240.0f, 255.0f, 255.0f, 0.85f);
            }
        }
    }
}

void cascades_draw(Game *g, uint32_t *screen) {
    if (!opt_get(OPT_ATMOSPHERE) || g->ram[GameMode] != 4 || g->ram[StageState] != 6 || !g->ram[SideViewFlag]) {
        return;
    }
    int camera_x = g->ram[CameraXLo] | (g->ram[CameraXHi] << 8), camera_y = g->ram[CameraYLo] | (g->ram[CameraYHi] << 8);
    static uint32_t behind[SCREEN_W * SCREEN_H];
    int copied = 0;
    float t = (float)g->frame / 60.0f;
    for (size_t k = 0; k < sizeof cascades / sizeof *cascades; k++) {
        const Cascade *c = &cascades[k];
        if (c->level != g->ram[LevelNumber] || c->area != g->ram[AreaNumber]) {
            continue;
        }
        int sx = c->x - camera_x, sy = c->y - camera_y;
        if (sx + c->w < 0 || sx > SCREEN_W) {
            continue;
        }
        if (!copied) { /* the picture without the water: what the water is drawn from */
            memcpy(behind, screen, sizeof behind);
            copied = 1;
        }
        draw_column(screen, behind, sx, sy, c->w, c->h, t);
        draw_foot(screen, sx, sy + c->h, c->w, g->frame);
    }
}
