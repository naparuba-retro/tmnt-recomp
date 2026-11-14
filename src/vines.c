/* Vines of the dam: see vines.h. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "hitbox_view.h"
#include "options.h"
#include "platform.h"
#include "player/tiles.h"
#include "ram_map.h"
#include "vines.h"

#define MAX_VINES 96
#define MAX_ROPES 3
#define MAX_NODES 12
#define ROPE_SPACING 3.0f      /* pixels between the ropes of a vine */
#define DAMPING 0.97f          /* the speed is multiplied by this at every frame */
#define GRAVITY 0.06f          /* weak: the vines are in water (Animal Well: 0.172 in the air) */
#define CAMERA_WIND (-0.0125f) /* what the scroll of the camera pushes the nodes with */
#define PUSH_RADIUS 1.0f       /* a node stays this far from the box of the turtle */
#define SOLVER_ITERATIONS 6
#define ROW_LIMIT 192        /* the playfield: below it is the status bar */
#define COLOUR_ROPE 0x2BA865 /* a darker green than the one of Animal Well (0x45FF91) */
#define COLOUR_BUD 0xFF8CC8

typedef struct {
    float x[2], w[2], q[2], v[2]; /* validated position, working position, rest position, speed */
    int flag;                     /* horizontal shift of the segment when it is drawn: 0, -1, +1 (the zigzag of the pixels) */
    float length;                 /* the distance to the node above, 1 to 7 pixels */
} Node;

typedef struct {
    int used, seen;
    int tile_x, tile_y; /* the world tile under the ceiling where it hangs */
    int nodes;
    Node rope[MAX_ROPES][MAX_NODES];
} Vine;

static Vine pool[MAX_VINES];
static int prev_camera_x = -1;

/* The same hash as the vines of Animal Well (sinf of a product, scaled, fractional part): the shifts and the lengths of the segments look alike. */
static float hash_unit(float a) {
    return fabsf(remainderf(sinf(a) * 43758.5469f, 1.0f));
}

static unsigned tile_hash(int tx, int ty) {
    unsigned h = (unsigned)tx * 73856093u ^ (unsigned)ty * 19349663u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return h;
}

static void make_vine(Vine *v, int tx, int ty, unsigned h) {
    memset(v, 0, sizeof *v);
    v->used = v->seen = 1;
    v->tile_x = tx;
    v->tile_y = ty;
    v->nodes = 4 + 2 * (int)((h >> 8) % 4u); /* 4, 6, 8 or 10 nodes: 12 to 40 pixels or so */
    float y0 = (float)(ty * 8);
    for (int r = 0; r < MAX_ROPES; r++) {
        float x = (float)(tx * 8) + (float)r * ROPE_SPACING;
        Node *n = v->rope[r];
        n[0].x[0] = n[0].w[0] = n[0].q[0] = x;
        n[0].x[1] = n[0].w[1] = n[0].q[1] = y0;
        int phase = (int)(hash_unit(x * 17.53623f + 105.614555f * y0) * 4.0f);
        float yy = y0;
        for (int k = 1; k < v->nodes; k++) {
            static const int shift[4] = {0, -1, 0, 1};
            n[k].flag = shift[(phase + k) % 4];
            int c = (int)(hash_unit(yy * 3911.65015f + x * 649.490051f) * 6.0f);
            int length = (k & 1) ? c + 1 : 7 - c;
            n[k].length = (float)length;
            yy += (float)length;
            n[k].x[0] = n[k].w[0] = n[k].q[0] = x;
            n[k].x[1] = n[k].w[1] = n[k].q[1] = yy;
        }
    }
}

/* Moves the point p out of the rectangle (x, y, w, h) when it is nearer than PUSH_RADIUS: a projection of the position, no force. */
static void push_point(const float rect[4], float p[2]) {
    float x0 = rect[0], y0 = rect[1], x1 = rect[0] + rect[2], y1 = rect[1] + rect[3], cx, cy, dx, dy;
    if (x1 < p[0] || p[0] < x0 || p[1] < y0 || y1 < p[1]) { /* outside: the nearest point of the rectangle */
        cx = p[0] < x0 ? x0 : p[0] > x1 ? x1 : p[0];
        cy = p[1] < y0 ? y0 : p[1] > y1 ? y1 : p[1];
        dx = p[0] - cx;
        dy = p[1] - cy;
    } else { /* inside: the nearest edge */
        float ex = fabsf(x1 - p[0]) < fabsf(x0 - p[0]) ? x1 : x0, ey = fabsf(y1 - p[1]) < fabsf(y0 - p[1]) ? y1 : y0;
        if (fabsf(ey - p[1]) <= fabsf(ex - p[0])) {
            cx = p[0];
            cy = ey;
        } else {
            cx = ex;
            cy = p[1];
        }
        dx = cx - p[0];
        dy = cy - p[1];
    }
    float d2 = dx * dx + dy * dy;
    if (d2 <= PUSH_RADIUS * PUSH_RADIUS) {
        if (d2 > 1.1754944e-38f) {
            float inv = 1.0f / sqrtf(d2);
            dx *= inv;
            dy *= inv;
        }
        p[0] = cx + dx * PUSH_RADIUS;
        p[1] = cy + dy * PUSH_RADIUS;
    }
}

/* Puts a at the distance `length` of b, in the direction from b to a (the chain does not stretch). */
static void keep_distance(float a[2], const float b[2], float length) {
    float dx = a[0] - b[0], dy = a[1] - b[1], d2 = dx * dx + dy * dy;
    if (d2 > 1.1754944e-38f) {
        float inv = 1.0f / sqrtf(d2);
        dx *= inv;
        dy *= inv;
    }
    a[0] = dx * length + b[0];
    a[1] = dy * length + b[1];
}

/* One frame of a rope (node 0 is the anchor): the nodes move on with their speed, are pushed out of the box, and come back to their lengths. */
static void rope_step(Node *n, int count, float wind_x, float sway, const float *rect) {
    for (int k = count - 1; k >= 1; k--) {
        n[k].w[0] = n[k].x[0];
        n[k].w[1] = n[k].x[1];
        n[k].v[0] *= DAMPING;
        n[k].v[1] = n[k].v[1] * DAMPING + GRAVITY;
        n[k].w[0] += wind_x + sway + n[k].v[0];
        n[k].w[1] += n[k].v[1];
    }
    for (int k = 1; k < count; k++) {
        for (int it = 0; it < SOLVER_ITERATIONS; it++) {
            if (rect) {
                push_point(rect, n[k].w);
            }
            if (k < count - 1) {
                keep_distance(n[k].w, n[k + 1].w, n[k + 1].length);
            }
            keep_distance(n[k].w, n[k - 1].w, n[k].length);
        }
    }
    for (int k = count - 1; k >= 1; k--) {
        n[k].v[0] = n[k].w[0] - n[k].x[0];
        n[k].v[1] = n[k].w[1] - n[k].x[1];
        n[k].x[0] = n[k].w[0];
        n[k].x[1] = n[k].w[1];
    }
}

static int in_dam(Game *g) {
    return opt_get(OPT_ATMOSPHERE) && g->ram[GameMode] == 4 && g->ram[SideViewFlag] && g->ram[LevelNumber] == 1 && g->ram[AreaNumber] == 1;
}

void vines_update(Game *g) {
    if (!in_dam(g)) {
        memset(pool, 0, sizeof pool);
        prev_camera_x = -1;
        return;
    }
    int camera_x = g->ram[CameraXLo] | (g->ram[CameraXHi] << 8), camera_y = g->ram[CameraYLo] | (g->ram[CameraYHi] << 8);
    int vertical = (g->ram[VerticalLevelFlag] & 0x80) != 0;
    float scroll = prev_camera_x < 0 || abs(camera_x - prev_camera_x) > 16 ? 0.0f : (float)(camera_x - prev_camera_x);
    prev_camera_x = camera_x;
    for (int i = 0; i < MAX_VINES; i++) {
        pool[i].seen = 0;
    }
    const uint8_t *map = &g->ram[TerrainMap];
    /* the ceilings on the screen: a world tile that is not solid with a solid one above it (the solid class of the side view is 4) */
    for (int ty = camera_y / 8 + 1; ty <= (camera_y + ROW_LIMIT) / 8; ty++) {
        int py = ty * 8 + 4 - camera_y;
        if (py < 12 || py >= ROW_LIMIT) {
            continue;
        }
        for (int tx = camera_x / 8; tx <= (camera_x + 255) / 8; tx++) {
            int px = tx * 8 + 4 - camera_x;
            if (px < 0 || px > 255) {
                continue;
            }
            unsigned h = tile_hash(tx, ty);
            if (h % 100u >= 55u) { /* about half of the ceiling tiles carry a vine */
                continue;
            }
            if (terrain_class_at(map, g->ram[CameraXLo], g->ram[CameraXHi], g->ram[CameraYLo], g->ram[CameraYHi], vertical, (uint8_t)px, (uint8_t)py) == 4 ||
                terrain_class_at(map, g->ram[CameraXLo], g->ram[CameraXHi], g->ram[CameraYLo], g->ram[CameraYHi], vertical, (uint8_t)px, (uint8_t)(py - 8)) !=
                    4) {
                continue;
            }
            Vine *free_slot = NULL, *found = NULL;
            for (int i = 0; i < MAX_VINES; i++) {
                if (pool[i].used && pool[i].tile_x == tx && pool[i].tile_y == ty) {
                    found = &pool[i];
                    break;
                }
                if (!pool[i].used && !free_slot) {
                    free_slot = &pool[i];
                }
            }
            if (found) {
                found->seen = 1;
            } else if (free_slot) {
                make_vine(free_slot, tx, ty, h);
            }
        }
    }
    float box[4];
    int pushing = 0;
    if ((g->ram[EntFlags] & 2) &&
        (g->ram[EntAnimId] | g->ram[EntSprite])) { /* the active turtle: the box that the game tests against the enemies, in the world */
        uint8_t r[4];
        hitbox_entity_rect(g, 0, r);
        if (r[1] >= r[0] && r[3] >= r[2]) {
            box[0] = (float)(camera_x + r[0]);
            box[1] = (float)(camera_y + r[2]);
            box[2] = (float)(r[1] - r[0]);
            box[3] = (float)(r[3] - r[2]);
            pushing = 1;
        }
    }
    float t = (float)g->frame;
    for (int i = 0; i < MAX_VINES; i++) {
        Vine *v = &pool[i];
        if (!v->used) {
            continue;
        }
        if (!v->seen) {
            v->used = 0;
            continue;
        }
        float sway = 0.012f * sinf(t * 0.035f + (float)v->tile_x * 0.9f + (float)v->tile_y * 0.4f); /* a slow current */
        for (int r = 0; r < MAX_ROPES; r++) {
            rope_step(v->rope[r], v->nodes, scroll * CAMERA_WIND, sway, pushing ? box : NULL);
        }
    }
}

static void plot(uint32_t *screen, int x, int y, uint32_t colour) {
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < ROW_LIMIT) {
        screen[y * SCREEN_W + x] = colour;
    }
}

static void line(uint32_t *screen, int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        plot(screen, x0, y0, COLOUR_ROPE);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void vines_draw(Game *g, uint32_t *screen) {
    if (!in_dam(g)) {
        return;
    }
    int camera_x = g->ram[CameraXLo] | (g->ram[CameraXHi] << 8), camera_y = g->ram[CameraYLo] | (g->ram[CameraYHi] << 8);
    for (int i = 0; i < MAX_VINES; i++) {
        const Vine *v = &pool[i];
        if (!v->used) {
            continue;
        }
        for (int r = 0; r < MAX_ROPES; r++) {
            const Node *n = v->rope[r];
            for (int k = 0; k < v->nodes - 1; k++) {
                line(screen, (int)(n[k].x[0] - (float)camera_x) + n[k].flag, (int)(n[k].x[1] - (float)camera_y),
                     (int)(n[k + 1].x[0] - (float)camera_x) + n[k].flag, (int)(n[k + 1].x[1] - (float)camera_y));
            }
            int bud = (int)n[v->nodes - 2].q[0] + (int)(n[v->nodes - 2].q[1] * 13.0f);
            if (bud >= 0 && bud % 4 == 1) { /* a small bud at the tip of some ropes */
                int bx = (int)(n[v->nodes - 1].x[0] - (float)camera_x) - (1 - n[v->nodes - 2].flag), by = (int)(n[v->nodes - 1].x[1] - (float)camera_y);
                for (int dy = 0; dy < 3; dy++) {
                    for (int dx = 0; dx < 3; dx++) {
                        plot(screen, bx + dx, by + dy, COLOUR_BUD);
                    }
                }
            }
        }
    }
}
