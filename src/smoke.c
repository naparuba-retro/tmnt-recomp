/* Smoke behind the tanks: see smoke.h. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "options.h"
#include "platform.h"
#include "player/tiles.h"
#include "ram_map.h"
#include "smoke.h"

#define GW 128 /* the grid: a cell is 2 x 2 pixels */
#define GH 120
#define CELL 2
#define DT 0.016667f
#define SOLID_BELOW 0.85f
#define DECAY 0.005f  /* the alpha of the dye goes down this much at every frame (Animal Well: 0.005) */
#define ITERATIONS 20 /* of the pressure (Animal Well: 40; the grid is smaller) */
#define TANK_A 0x9A   /* the tanks of the street (purple, with a red cannon) */
#define TANK_B 0x9B

static float vel[GW * GH * 2], vel_adv[GW * GH * 2], dye[GW * GH * 4], dye_tmp[GW * GH * 4];
static float pressure[GW * GH], pressure_tmp[GW * GH], divergence[GW * GH], mask[GW * GH];
static int active, scroll_x, scroll_y, prev_cam_x, prev_cam_y, have_prev_camera;
static struct {
    int valid;
    int wx, wy; /* the world position of the tank at the previous frame */
} last[16];

static float bilinear(const float *t, int channels, int c, float x, float y) {
    float fx = x - 0.5f, fy = y - 0.5f;
    fx = fx < 0 ? 0 : fx > GW - 1 ? (float)(GW - 1) : fx;
    fy = fy < 0 ? 0 : fy > GH - 1 ? (float)(GH - 1) : fy;
    int x0 = (int)fx, y0 = (int)fy, x1 = x0 + 1 < GW ? x0 + 1 : GW - 1, y1 = y0 + 1 < GH ? y0 + 1 : GH - 1;
    float tx = fx - (float)x0, ty = fy - (float)y0;
    float a = t[(y0 * GW + x0) * channels + c], b = t[(y0 * GW + x1) * channels + c], d = t[(y1 * GW + x0) * channels + c],
          e = t[(y1 * GW + x1) * channels + c];
    return (a * (1.0f - tx) + b * tx) * (1.0f - ty) + (d * (1.0f - tx) + e * tx) * ty;
}

static void forget(void) {
    memset(vel, 0, sizeof vel);
    memset(dye, 0, sizeof dye);
    memset(pressure, 0, sizeof pressure);
    memset(last, 0, sizeof last);
    active = scroll_x = scroll_y = 0;
    have_prev_camera = 0;
}

/* The scroll of the camera: the field follows the picture (a whole number of cells at a time; the remainder waits for the next frames). */
static void shift_field(float *f, int channels, int dx, int dy) {
    if (!dx && !dy) {
        return;
    }
    static float copy[GW * GH * 4];
    memcpy(copy, f, sizeof(float) * (size_t)(GW * GH * channels));
    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            int sx = x + dx, sy = y + dy;
            for (int c = 0; c < channels; c++) {
                f[(y * GW + x) * channels + c] = (sx < 0 || sy < 0 || sx >= GW || sy >= GH) ? 0.0f : copy[(sy * GW + sx) * channels + c];
            }
        }
    }
}

/* A disc of radius `r` cells around (cx, cy). */
static int in_disc(int i, int j, int r) {
    return i * i + j * j <= r * r + r;
}

static void add_dye(int cx, int cy, int r, float red, float green, float blue, float alpha) {
    for (int j = -r; j <= r; j++) {
        for (int i = -r; i <= r; i++) {
            int x = cx + i, y = cy + j;
            if (!in_disc(i, j, r) || x < 0 || y < 0 || x >= GW || y >= GH || mask[y * GW + x] < SOLID_BELOW) {
                continue;
            }
            float *d = &dye[(y * GW + x) * 4];
            d[0] = red * alpha + d[0] * (1.0f - alpha);
            d[1] = green * alpha + d[1] * (1.0f - alpha);
            d[2] = blue * alpha + d[2] * (1.0f - alpha);
            d[3] = alpha + d[3] * (1.0f - alpha);
        }
    }
}

static void add_push(int cx, int cy, int r, float vx, float vy) {
    for (int j = -r; j <= r; j++) {
        for (int i = -r; i <= r; i++) {
            int x = cx + i, y = cy + j;
            if (!in_disc(i, j, r) || x < 0 || y < 0 || x >= GW || y >= GH || mask[y * GW + x] < SOLID_BELOW) {
                continue;
            }
            vel[(y * GW + x) * 2] += vx;
            vel[(y * GW + x) * 2 + 1] += vy;
        }
    }
}

/* One step of the fluid, in the order of Animal Well: the dye is carried by the old speed, the speed by itself, the dye fades, the pushes are added, the divergence
 * is found, the pressure is relaxed (Jacobi), and the speed loses the gradient of the pressure. */
static void step(void) {
    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            int i = y * GW + x;
            float cx = (float)x + 0.5f, cy = (float)y + 0.5f, px = cx - vel[i * 2] * DT, py = cy - vel[i * 2 + 1] * DT;
            if (mask[i] >= SOLID_BELOW) {
                vel_adv[i * 2] = bilinear(vel, 2, 0, px, py);
                vel_adv[i * 2 + 1] = bilinear(vel, 2, 1, px, py);
            } else {
                vel_adv[i * 2] = vel_adv[i * 2 + 1] = 0.0f;
            }
            if (px < 0.5f || py < 0.5f || px > (float)GW - 0.5f || py > (float)GH - 0.5f || mask[i] < SOLID_BELOW) {
                for (int c = 0; c < 4; c++) {
                    dye_tmp[i * 4 + c] = 0.0f;
                }
            } else {
                for (int c = 0; c < 4; c++) {
                    dye_tmp[i * 4 + c] = bilinear(dye, 4, c, px, py);
                }
            }
            float a = dye_tmp[i * 4 + 3] - DECAY;
            dye_tmp[i * 4 + 3] = a > 0.0f ? a : 0.0f;
        }
    }
    memcpy(dye, dye_tmp, sizeof dye);
    for (int y = 0; y < GH; y++) { /* the divergence: the speed at half a cell on each side, nothing inside a wall */
        for (int x = 0; x < GW; x++) {
            float cx = (float)x + 0.5f, cy = (float)y + 0.5f, v[4];
            static const float ox[4] = {0.5f, -0.5f, 0.0f, 0.0f}, oy[4] = {0.0f, 0.0f, 0.5f, -0.5f};
            for (int k = 0; k < 4; k++) {
                v[k] = bilinear(mask, 1, 0, cx + ox[k], cy + oy[k]) < SOLID_BELOW ? 0.0f : bilinear(vel_adv, 2, k < 2 ? 0 : 1, cx + ox[k], cy + oy[k]);
            }
            divergence[y * GW + x] = (v[0] - v[1] + v[2] - v[3]) * 0.5f;
        }
    }
    float *p = pressure, *q = pressure_tmp;
    for (int it = 0; it < ITERATIONS; it++) {
        for (int y = 0; y < GH; y++) {
            for (int x = 0; x < GW; x++) {
                int i = y * GW + x, l = y * GW + (x > 0 ? x - 1 : 0), r = y * GW + (x < GW - 1 ? x + 1 : x), d = (y > 0 ? y - 1 : 0) * GW + x,
                    u = (y < GH - 1 ? y + 1 : y) * GW + x;
                float s = (mask[l] < SOLID_BELOW ? p[i] : p[l]) + (mask[r] < SOLID_BELOW ? p[i] : p[r]) + (mask[d] < SOLID_BELOW ? p[i] : p[d]) +
                          (mask[u] < SOLID_BELOW ? p[i] : p[u]);
                q[i] = (s - divergence[i]) * 0.25f;
            }
        }
        float *t = p;
        p = q;
        q = t;
    }
    if (p != pressure) {
        memcpy(pressure, p, sizeof pressure);
    }
    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            int i = y * GW + x;
            if (mask[i] < SOLID_BELOW) {
                vel[i * 2] = vel[i * 2 + 1] = 0.0f;
                continue;
            }
            float cx = (float)x + 0.5f, cy = (float)y + 0.5f;
            float gx = bilinear(pressure, 1, 0, cx + 0.5f, cy) - bilinear(pressure, 1, 0, cx - 0.5f, cy);
            float gy = bilinear(pressure, 1, 0, cx, cy + 0.5f) - bilinear(pressure, 1, 0, cx, cy - 0.5f);
            vel[i * 2] = vel_adv[i * 2] - gx;
            vel[i * 2 + 1] = vel_adv[i * 2 + 1] - gy;
            float s2 = vel[i * 2] * vel[i * 2] + vel[i * 2 + 1] * vel[i * 2 + 1], cap = 60.0f; /* the speed stays reasonable (cells per second) */
            if (s2 > cap * cap) {
                float k = cap / sqrtf(s2);
                vel[i * 2] *= k;
                vel[i * 2 + 1] *= k;
            }
        }
    }
}

static int in_street(Game *g) {
    return opt_get(OPT_ATMOSPHERE) && g->ram[GameMode] == 4 && g->ram[StageState] == 6 && !g->ram[SideViewFlag];
}

void smoke_update(Game *g) {
    if (!in_street(g)) {
        if (active) {
            forget();
        }
        return;
    }
    int camera_x = g->ram[CameraXLo] | (g->ram[CameraXHi] << 8), camera_y = g->ram[CameraYLo] | (g->ram[CameraYHi] << 8);
    int tanks = 0;
    for (int e = 5; e < 16; e++) {
        tanks += (g->ram[EntType + e] == TANK_A || g->ram[EntType + e] == TANK_B) && (g->ram[EntFlags + e] & 2);
    }
    if (!tanks && !active) {
        have_prev_camera = 0;
        return;
    }
    if (have_prev_camera &&
        (abs(camera_x - prev_cam_x) > 24 || abs(camera_y - prev_cam_y) > 24)) { /* a jump of the camera: the smoke of before is somewhere else */
        forget();
    }
    int dx = have_prev_camera ? camera_x - prev_cam_x : 0, dy = have_prev_camera ? camera_y - prev_cam_y : 0;
    prev_cam_x = camera_x;
    prev_cam_y = camera_y;
    have_prev_camera = 1;
    scroll_x += dx;
    scroll_y += dy;
    int cells_x = scroll_x / CELL, cells_y = scroll_y / CELL; /* whole cells: the field goes the other way from the camera */
    scroll_x -= cells_x * CELL;
    scroll_y -= cells_y * CELL;
    if (cells_x || cells_y) {
        shift_field(vel, 2, cells_x, cells_y);
        shift_field(dye, 4, cells_x, cells_y);
        shift_field(pressure, 1, cells_x, cells_y);
    }
    int vertical = (g->ram[VerticalLevelFlag] & 0x80) != 0;
    for (int cy = 0; cy < GH / 4; cy++) { /* the walls: the buildings (class 4 of the map seen from above) */
        for (int cx = 0; cx < GW / 4; cx++) {
            uint8_t c = terrain_class_at(&g->ram[TerrainMap], g->ram[CameraXLo], g->ram[CameraXHi], g->ram[CameraYLo], g->ram[CameraYHi], vertical,
                                         (uint8_t)(cx * 8 + 4), (uint8_t)(cy * 8 + 4));
            for (int j = 0; j < 4; j++) {
                for (int i = 0; i < 4; i++) {
                    mask[(cy * 4 + j) * GW + cx * 4 + i] = c == 4 ? 0.0f : 1.0f;
                }
            }
        }
    }
    for (int e = 5; e < 16; e++) { /* the emitters: dye and a push behind each tank */
        int is_tank = (g->ram[EntType + e] == TANK_A || g->ram[EntType + e] == TANK_B) && (g->ram[EntFlags + e] & 2);
        if (!is_tank) {
            last[e].valid = 0;
            continue;
        }
        int wx = camera_x + g->ram[EntX + e], wy = camera_y + g->ram[EntY + e];
        float mx = 0.0f, my = 0.0f;
        if (last[e].valid) {
            mx = (float)(wx - last[e].wx);
            my = (float)(wy - last[e].wy);
        }
        last[e].valid = 1;
        last[e].wx = wx;
        last[e].wy = wy;
        float len = sqrtf(mx * mx + my * my);
        if (len > 6.0f) { /* a jump (the tank appears somewhere else): no smoke this time */
            continue;
        }
        float bx = len > 0.05f ? -mx / len : 0.0f,
              by = len > 0.05f ? -my / len : -1.0f; /* behind: against the movement (a tank that stands lifts a little dust) */
        float nx = -by, ny = bx;                    /* across the movement: the two tracks */
        for (int side = -1; side <= 1; side += 2) { /* dust lifted by each track, behind the tank */
            float sx = (float)g->ram[EntX + e] + bx * 20.0f + nx * 6.0f * (float)side,
                  sy = (float)g->ram[EntY + e] - 6.0f + by * 20.0f + ny * 6.0f * (float)side;
            int cx = (int)(sx / (float)CELL), cy = (int)(sy / (float)CELL);
            unsigned noise = (unsigned)(g->frame * 2654435761u + (unsigned)(e * 2 + (side > 0)) * 40503u);
            float jx = (float)((noise >> 8) % 100u) / 100.0f - 0.5f, jy = (float)((noise >> 16) % 100u) / 100.0f - 0.5f;
            add_dye(cx, cy, 3, 0.86f, 0.80f, 0.70f, 0.30f); /* a pale dust, thin */
            add_push(cx, cy, 3, bx * 4.0f + nx * (float)side * 6.0f + jx * 26.0f,
                     by * 4.0f + ny * (float)side * 6.0f + jy * 26.0f); /* it spreads sideways and billows */
        }
    }
    step();
    active = 1;
    float total = 0.0f;
    for (int i = 0; i < GW * GH; i += 7) {
        total += dye[i * 4 + 3];
    }
    if (!tanks && total < 0.05f) { /* nothing left: the field sleeps until the next tank */
        forget();
    }
}

void smoke_draw(Game *g, uint32_t *screen) {
    if (!active || !in_street(g)) {
        return;
    }
    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            const float *d = &dye[(y * GW + x) * 4];
            if (d[3] < 0.2f) {
                continue;
            }
            int checker = ((x & 1) == 0) != ((y & 1) == 1);
            int level = (int)floorf((d[3] + 0.055556f) * 9.0f);
            int partner = checker ? level - 1 : (level + 1 < 9 ? level + 1 : 9);
            int shown = ((level & 1) == 1 && level < 4) ? partner
                                                        : level; /* the dither of Animal Well: the thin levels are replaced by a neighbour in a checkerboard */
            if (shown <= 0) {
                continue;
            }
            float alpha = (float)shown * 0.111111f * 0.6f; /* thin: dust, not a cloud of smoke */
            float rgb[3];
            for (int c = 0; c < 3; c++) {
                rgb[c] = floorf((d[c] + 0.125f) * 8.0f) / 8.0f * 255.0f; /* the colours in eighths */
            }
            for (int j = 0; j < CELL; j++) {
                for (int i = 0; i < CELL; i++) {
                    int px = x * CELL + i, py = y * CELL + j;
                    if (px >= SCREEN_W || py >= SCREEN_H) {
                        continue;
                    }
                    uint32_t c = screen[py * SCREEN_W + px];
                    float r = (float)((c >> 16) & 255), gg = (float)((c >> 8) & 255), b = (float)(c & 255);
                    r += (rgb[0] - r) * alpha;
                    gg += (rgb[1] - gg) * alpha;
                    b += (rgb[2] - b) * alpha;
                    screen[py * SCREEN_W + px] =
                        ((uint32_t)(r > 255.0f ? 255.0f : r) << 16) | ((uint32_t)(gg > 255.0f ? 255.0f : gg) << 8) | (uint32_t)(b > 255.0f ? 255.0f : b);
                }
            }
        }
    }
}
