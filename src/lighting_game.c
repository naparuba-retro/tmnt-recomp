/* See lighting_game.h. Two ambiances: the sewer (side view of the first level, bluish bricks) and the street (every view from above: the streets, the river). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lighting.h"
#include "lighting_game.h"
#include "options.h"
#include "platform.h"
#include "player/tiles.h"
#include "ram_map.h"

#define PLAY_ROWS 192 /* the playfield: below it is the status bar */

enum {
    KIND_NONE,
    KIND_SEWER,
    KIND_STREET,
    KIND_WAREHOUSE
};

/* Is the picture a sewer? The side views of the first level are the sewer (bluish bricks) and the warehouses (brown). The answer is a steady one: a score that goes up and down
 * with what the picture looks like, and starts again when the level or the area changes, so that the effect does not blink when the scroll changes the share of blue. */
static int sewer_now(Game *g, const uint32_t *screen, int instant) {
    static int score, last_scene = -1;
    int scene = g->ram[LevelNumber] * 256 + g->ram[AreaNumber] + (g->ram[SideViewFlag] ? 65536 : 0);
    if (scene != last_scene) {
        last_scene = scene;
        score = 0;
    }
    if (g->ram[GameMode] != 4 || !g->ram[SideViewFlag] || g->ram[LevelNumber] != 0) {
        return 0;
    }
    int blue = 0, total = 0;
    for (int y = 16; y < PLAY_ROWS; y += 4) {
        for (int x = 0; x < SCREEN_W; x += 4) {
            uint32_t c = screen[y * SCREEN_W + x];
            int r = (int)((c >> 16) & 255), b = (int)(c & 255);
            total += (c & 0xFFFFFF) != 0;
            blue += b > r + 20;
        }
    }
    if (total > 0) {
        score += blue * 25 > total ? 1 : -1;
        score = score > 40 ? 40 : score < -40 ? -40 : score;
    }
    return instant ? blue * 25 > total : score >= 10;
}

static int scene_kind(Game *g, const uint32_t *screen, int instant) {
    if (g->ram[GameMode] != 4) {
        return KIND_NONE;
    }
    if (!g->ram[SideViewFlag]) {
        return KIND_STREET;
    }
    if (getenv("TMNT_WAREHOUSE")) { /* a test: every side view as a warehouse */
        return KIND_WAREHOUSE;
    }
    return sewer_now(g, screen, instant) ? KIND_SEWER : KIND_WAREHOUSE;
}

static const LightParams sewer_base = {
    {0.82f, 0.86f, 0.94f}, /* ambient: bluish, only a little darker than the original */
    0.9f,                  /* the bright parts of the picture light their surroundings */
    0.45f,                 /* bloom */
    {0.02f, 0.05f, 0.09f}, /* haze */
    0.20f,                 /* haze amount */
    {1.0f, 1.0f, 1.0f},    /* grade */
    1.0f,                  /* saturation */
    0.0f,                  /* noise */
    0.3f,                  /* violet glows */
    0.35f,                 /* the light that floors, walls and platforms get */
    1.35f,                 /* the lights reach farther over the background */
    0.30f,                 /* the waves of light */
    12,                    /* the share of the columns that drip */
    18.0f,                 /* no glow around the turtle */
    0.35f,                 /* vignette */
    0.04f,                 /* flicker */
    PLAY_ROWS,
    0,    /* scroll (set at every frame) */
    0.0f, /* mist */
    0.0f, /* mist over the water */
    1.0f, /* mist speed */
    0,    /* waves everywhere */
    1.0f, /* wave speed */
    0,    /* no dust */
};

/* The street: seen from above, in the light of the end of the day, with a mist that drifts over the land and gathers over the river, where the light moves in waves. */
static const LightParams street_base = {
    {0.80f, 0.84f, 0.95f}, /* ambient: dusk, cool: the lights of the characters can be seen */
    0.2f,                  /* the bright parts of the picture light their surroundings */
    0.25f,                 /* bloom */
    {0.60f, 0.68f, 0.86f}, /* the mist: pale blue-grey */
    0.0f,                  /* no haze from the bottom */
    {1.0f, 1.0f, 1.0f},    /* grade */
    0.95f,                 /* saturation */
    0.0f,                  /* noise */
    0.0f,                  /* glows */
    0.35f,                 /* the light that the buildings get */
    1.0f,                  /* the lights reach as far everywhere */
    0.75f,                 /* the waves of light: over the water */
    0,                     /* no drops */
    18.0f,                 /* no glow around the turtle */
    0.06f,                 /* vignette */
    0.03f,                 /* flicker */
    PLAY_ROWS,
    0,     /* scroll */
    0.05f, /* a faint mist over the land */
    0.30f, /* and more over the water */
    1.0f,  /* mist speed */
    1,     /* the waves of light are over the water only */
    0.5f,  /* and they move slowly */
    0,     /* no dust */
};

/* The warehouse: the other rooms seen from the side, with brown walls: a warm dim light from the lamps, and dust that floats in it. */
static const LightParams warehouse_base = {
    {0.66f, 0.60f, 0.58f}, /* ambient: warm and dim */
    0.7f,                  /* the bright parts of the picture light their surroundings */
    0.35f,                 /* bloom */
    {0.10f, 0.06f, 0.03f}, /* haze: warm */
    0.15f,                 /* haze amount */
    {1.0f, 1.0f, 1.0f},    /* grade */
    1.0f,                  /* saturation */
    0.0f,                  /* noise */
    0.0f,                  /* glows */
    0.35f,                 /* the light that floors, walls and platforms get */
    1.3f,                  /* the lights reach farther over the background */
    0.0f,                  /* no waves of light */
    0,                     /* no drops */
    18.0f,                 /* no glow around the turtle */
    0.30f,                 /* vignette */
    0.05f,                 /* flicker: the lamps waver */
    PLAY_ROWS,
    0,    /* scroll */
    0.0f, /* mist */
    0.0f, /* mist over the water */
    1.0f, /* mist speed */
    0,    /* waves everywhere */
    1.0f, /* wave speed */
    60,   /* dust */
};

void lighting_game_apply(Game *g, uint32_t *screen, int instant) {
    static float fade;
    static int kind;
    if (!opt_get(OPT_ATMOSPHERE)) {
        fade = 0.0f;
        return;
    }
    int now = scene_kind(g, screen, instant);
    if (now != kind) { /* another kind of scene: the old one goes out (over 20 frames) before the new one comes in */
        if (fade > 0.0f && !instant) {
            fade = fade - 0.05f < 0.0f ? 0.0f : fade - 0.05f;
            if (fade <= 0.0f) {
                kind = now;
            }
        } else {
            kind = now;
            fade = 0.0f;
        }
    } else if (kind != KIND_NONE) {
        fade = instant ? 1.0f : (fade + 0.05f > 1.0f ? 1.0f : fade + 0.05f); /* it comes in over 20 frames */
    }
    if (instant) {
        fade = kind == KIND_NONE ? 0.0f : 1.0f;
    }
    if (fade <= 0.0f || kind == KIND_NONE) {
        return;
    }
    int lit_rows = kind == KIND_STREET ? SCREEN_H : PLAY_ROWS; /* from above the picture is all playfield; from the side the status bar is below it */
    static uint32_t before[SCREEN_W * SCREEN_H];
    memcpy(before, screen, (size_t)SCREEN_W * (size_t)lit_rows * sizeof before[0]);
    LightParams params = kind == KIND_SEWER ? sewer_base : kind == KIND_WAREHOUSE ? warehouse_base : street_base;
    params.rows = lit_rows;
    params.scroll_x = g->ram[CameraXLo] | (g->ram[CameraXHi] << 8);
    if (getenv("TMNT_DRIP")) { /* a test: the share of the columns that drip */
        params.particles = atoi(getenv("TMNT_DRIP"));
    }
    uint8_t solid8[(SCREEN_W / 8) * (SCREEN_H / 8)];
    {
        for (int cy = 0; cy < PLAY_ROWS / 8;
             cy++) { /* the collision map under the middle of every square of 8x8 pixels (in the side view the solid class is 4) */
            for (int cx = 0; cx < SCREEN_W / 8; cx++) {
                uint8_t c = terrain_class_at(&g->ram[TerrainMap], g->ram[CameraXLo], g->ram[CameraXHi], g->ram[CameraYLo], g->ram[CameraYHi],
                                             (g->ram[VerticalLevelFlag] & 0x80) != 0, (uint8_t)(cx * 8 + 4), (uint8_t)(cy * 8 + 4));
                uint32_t px = screen[(cy * 8 + 4) * SCREEN_W + cx * 8 + 4];
                int water = ((px >> 16) & 255) < 64 && (px & 255) > 190; /* the river is blue: it cannot be crossed, but the light goes over it */
                solid8[cy * (SCREEN_W / 8) + cx] =
                    kind != KIND_STREET
                        ? c == 4
                        : ((c == 4 || c == 0x0C) && !water); /* from the side the solid class is 4; from above the walls and the buildings are 4 and $0C */
            }
        }
    }
    float lamp = kind == KIND_STREET ? 0.6f : 1.0f; /* in the street the characters need more light to show against the sky */
    if (getenv("TMNT_MASK_DEBUG")) {                /* the solid squares, as text */
        for (int cy = 0; cy < lit_rows / 8; cy++) {
            for (int cx = 0; cx < SCREEN_W / 8; cx++) {
                fputc(solid8[cy * (SCREEN_W / 8) + cx] ? 35 : 46, stderr);
            }
            fputc(10, stderr);
        }
    }
    LightSource lights[12];
    int count = 0;
    if ((g->ram[EntFlags] & 2) && (g->ram[EntAnimId] | g->ram[EntSprite])) { /* the active turtle: entity 0 */
        lights[count].x = (float)g->ram[EntX];
        lights[count].y = (float)g->ram[EntY] - 10.0f;
        lights[count].radius = kind == KIND_STREET ? 280.0f : 70.0f; /* in the street it carries far, but the buildings stop it */
        lights[count].glow = 0.0f;
        lights[count].intensity = 0.30f * lamp;
        lights[count].r = 1.0f;
        lights[count].g = 0.93f;
        lights[count].b = 0.78f;
        count++;
    }
    for (int x = 5; x < 16 && count < 12 && kind != KIND_STREET; x++) {
        /* (in the street their light is so faint that it is not worth its cost) */ /* the enemies and their shots: a small cold light, so that they are not lost in the dark */
        if (g->ram[EntType + x] != 0 && (g->ram[EntFlags + x] & 2)) {
            lights[count].x = (float)g->ram[EntX + x];
            lights[count].y = (float)g->ram[EntY + x] - 8.0f;
            lights[count].radius = kind == KIND_STREET ? 150.0f : 64.0f;
            lights[count].intensity = kind == KIND_STREET ? 0.07f : 0.25f;
            lights[count].glow = kind == KIND_STREET ? 0.0f : 0.11f;
            lights[count].r = 0.75f;
            lights[count].g = 0.85f;
            lights[count].b = 1.0f;
            count++;
        }
    }
    if (getenv("TMNT_LIGHT_DEBUG")) {
        fprintf(stderr, "kind %d lights: %d", kind, count);
        for (int k = 0; k < count; k++) {
            fprintf(stderr, " (%.0f,%.0f)", lights[k].x, lights[k].y);
        }
        fputc(10, stderr);
    }
    lighting_apply(screen, SCREEN_W, SCREEN_H, lights, count, &params, g->frame, solid8);
    if (fade < 1.0f) { /* a fade between the picture and the lit picture */
        for (int i = 0; i < SCREEN_W * lit_rows; i++) {
            uint32_t a = before[i], b = screen[i], o = 0;
            for (int sh = 16; sh >= 0; sh -= 8) {
                float va = (float)((a >> sh) & 255), vb = (float)((b >> sh) & 255);
                o |= (uint32_t)(va + (vb - va) * fade + 0.5f) << sh;
            }
            screen[i] = o;
        }
    }
}
