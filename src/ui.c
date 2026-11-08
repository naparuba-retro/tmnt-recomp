/* The high-resolution user interface layer, see ui.h. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "third_party/stb_truetype.h"
#include "third_party/stb_image.h"

struct UiFont {
    unsigned char *data;
    stbtt_fontinfo info;
};

struct UiImage {
    int w, h;
    unsigned char *rgba;
};

/* ------------------------------------------------------------------ surface and pixels --- */
void ui_resize(Ui *ui, int w, int h) {
    if (w < 1) {
        w = 1;
    }
    if (h < 1) {
        h = 1;
    }
    if (ui->px && ui->w == w && ui->h == h) {
        return;
    }
    free(ui->px);
    ui->px = (uint32_t *)calloc((size_t)w * (size_t)h, sizeof(uint32_t));
    ui->w = w;
    ui->h = h;
}

void ui_free(Ui *ui) {
    free(ui->px);
    ui->px = NULL;
    ui->w = ui->h = 0;
}

static inline uint32_t mix(uint32_t d, uint32_t s, int a) {
    int dr = (int)((d >> 16) & 255), dg = (int)((d >> 8) & 255), db = (int)(d & 255);
    int sr = (int)((s >> 16) & 255), sg = (int)((s >> 8) & 255), sb = (int)(s & 255);
    dr += ((sr - dr) * a + 127) / 255;
    dg += ((sg - dg) * a + 127) / 255;
    db += ((sb - db) * a + 127) / 255;
    return (uint32_t)((dr << 16) | (dg << 8) | db);
}

static inline void put(Ui *ui, int x, int y, uint32_t c, int a) {
    if (a <= 0 || x < 0 || y < 0 || x >= ui->w || y >= ui->h) {
        return;
    }
    if (a > 255) {
        a = 255;
    }
    uint32_t *p = &ui->px[(size_t)y * (size_t)ui->w + (size_t)x];
    *p = a == 255 ? c : mix(*p, c, a);
}

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

void ui_fill_rect(Ui *ui, int x, int y, int w, int h, uint32_t colour, int alpha) {
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            put(ui, i, j, colour, alpha);
        }
    }
}

/* ------------------------------------------------------------------ quads --- */
UiQuad ui_quad_rect(float x, float y, float w, float h) {
    UiQuad q = {{x, x + w, x + w, x}, {y, y, y + h, y + h}};
    return q;
}

UiQuad ui_quad_skew(float x, float y, float w, float h, float skew) {
    UiQuad q = {{x + skew, x + w + skew, x + w, x}, {y, y, y + h, y + h}};
    return q;
}

UiQuad ui_quad_offset(UiQuad q, float dx, float dy) {
    for (int i = 0; i < 4; i++) {
        q.x[i] += dx;
        q.y[i] += dy;
    }
    return q;
}

/* Signed distance of a point to the quad's edges (positive inside, the smallest of the four lines): the convex shape's coverage and outline come from it. */
static float quad_dist(const UiQuad *q, float px, float py) {
    float best = 1e9f;
    for (int i = 0; i < 4; i++) {
        int j = (i + 1) & 3;
        float dx = q->x[j] - q->x[i], dy = q->y[j] - q->y[i];
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1e-4f) {
            continue;
        }
        float d = (dx * (py - q->y[i]) - dy * (px - q->x[i])) / len;
        if (d < best) {
            best = d;
        }
    }
    return best;
}

void ui_desaturate_quad(Ui *ui, const UiQuad *q) {
    float x0 = q->x[0], x1 = q->x[0], y0 = q->y[0], y1 = q->y[0];
    for (int i = 1; i < 4; i++) {
        x0 = q->x[i] < x0 ? q->x[i] : x0;
        x1 = q->x[i] > x1 ? q->x[i] : x1;
        y0 = q->y[i] < y0 ? q->y[i] : y0;
        y1 = q->y[i] > y1 ? q->y[i] : y1;
    }
    int ix0 = (int)floorf(x0) < 0 ? 0 : (int)floorf(x0), ix1 = (int)ceilf(x1) > ui->w ? ui->w : (int)ceilf(x1);
    int iy0 = (int)floorf(y0) < 0 ? 0 : (int)floorf(y0), iy1 = (int)ceilf(y1) > ui->h ? ui->h : (int)ceilf(y1);
    for (int y = iy0; y < iy1; y++) {
        for (int x = ix0; x < ix1; x++) {
            if (!ui_quad_contains(q, (float)x + 0.5f, (float)y + 0.5f)) {
                continue;
            }
            uint32_t c = ui->px[(size_t)y * (size_t)ui->w + (size_t)x];
            uint32_t lum = (299u * ((c >> 16) & 255u) + 587u * ((c >> 8) & 255u) + 114u * (c & 255u)) / 1000u;
            ui->px[(size_t)y * (size_t)ui->w + (size_t)x] = (lum << 16) | (lum << 8) | lum;
        }
    }
}

int ui_quad_contains(const UiQuad *q, float x, float y) {
    return quad_dist(q, x, y) >= 0.0f;
}

static void quad_bounds(const UiQuad *q, float pad, int *x0, int *y0, int *x1, int *y1) {
    float mnx = q->x[0], mxx = q->x[0], mny = q->y[0], mxy = q->y[0];
    for (int i = 1; i < 4; i++) {
        mnx = q->x[i] < mnx ? q->x[i] : mnx;
        mxx = q->x[i] > mxx ? q->x[i] : mxx;
        mny = q->y[i] < mny ? q->y[i] : mny;
        mxy = q->y[i] > mxy ? q->y[i] : mxy;
    }
    *x0 = (int)floorf(mnx - pad);
    *y0 = (int)floorf(mny - pad);
    *x1 = (int)ceilf(mxx + pad);
    *y1 = (int)ceilf(mxy + pad);
}

void ui_fill_quad(Ui *ui, const UiQuad *q, uint32_t colour, int alpha) {
    int x0, y0, x1, y1;
    quad_bounds(q, 1.0f, &x0, &y0, &x1, &y1);
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float c = clampf(quad_dist(q, (float)x + 0.5f, (float)y + 0.5f) + 0.5f, 0.0f, 1.0f);
            if (c > 0.0f) {
                put(ui, x, y, colour, (int)(c * (float)alpha + 0.5f));
            }
        }
    }
}

void ui_stroke_quad(Ui *ui, const UiQuad *q, float thickness, uint32_t colour) {
    int x0, y0, x1, y1;
    quad_bounds(q, thickness + 1.0f, &x0, &y0, &x1, &y1);
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float d = quad_dist(q, (float)x + 0.5f, (float)y + 0.5f);
            float c = clampf(thickness * 0.5f - fabsf(d) + 0.5f, 0.0f, 1.0f);
            if (c > 0.0f) {
                put(ui, x, y, colour, (int)(c * 255.0f + 0.5f));
            }
        }
    }
}

void ui_halftone_quad(Ui *ui, const UiQuad *q, uint32_t colour, float cell, float t0, float t1, int alpha) {
    int x0, y0, x1, y1;
    quad_bounds(q, 1.0f, &x0, &y0, &x1, &y1);
    float top = (float)y0, span = (float)(y1 - y0 + 1);
    const float k = 0.70710678f;
    for (int y = y0; y <= y1; y++) {
        float t = (((float)y + 0.5f - top) / span - t0) / (t1 - t0);
        t = clampf(t, 0.0f, 1.0f);
        float radius = cell * 0.7071f * sqrtf(t);
        if (radius < 0.4f) {
            continue;
        }
        for (int x = x0; x <= x1; x++) {
            float px = (float)x + 0.5f, py = (float)y + 0.5f;
            float cover = clampf(quad_dist(q, px, py) + 0.5f, 0.0f, 1.0f);
            if (cover <= 0.0f) {
                continue;
            }
            float u = (px + py) * k, v = (px - py) * k; /* the grid is turned 45 degrees */
            float du = u - floorf(u / cell + 0.5f) * cell, dv = v - floorf(v / cell + 0.5f) * cell;
            float dot = clampf(radius - sqrtf(du * du + dv * dv) + 0.5f, 0.0f, 1.0f);
            if (dot > 0.0f) {
                put(ui, x, y, colour, (int)(dot * cover * (float)alpha + 0.5f));
            }
        }
    }
}

static int in_triangle(float px, float py, float ax, float ay, float bx, float by, float cx, float cy) {
    float d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
    float d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
    float d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);
    int neg = (d1 < 0) || (d2 < 0) || (d3 < 0), pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(neg && pos);
}

void ui_fill_triangle(Ui *ui, float x0, float y0, float x1, float y1, float x2, float y2, uint32_t colour) {
    int bx0 = (int)floorf(fminf(x0, fminf(x1, x2))) - 1, bx1 = (int)ceilf(fmaxf(x0, fmaxf(x1, x2))) + 1;
    int by0 = (int)floorf(fminf(y0, fminf(y1, y2))) - 1, by1 = (int)ceilf(fmaxf(y0, fmaxf(y1, y2))) + 1;
    for (int y = by0; y <= by1; y++) {
        for (int x = bx0; x <= bx1; x++) {
            int hit = 0;
            for (int sy = 0; sy < 3; sy++) {
                for (int sx = 0; sx < 3; sx++) {
                    hit += in_triangle((float)x + ((float)sx + 0.5f) / 3.0f, (float)y + ((float)sy + 0.5f) / 3.0f, x0, y0, x1, y1, x2, y2);
                }
            }
            if (hit) {
                put(ui, x, y, colour, hit * 255 / 9);
            }
        }
    }
}

void ui_fill_burst(Ui *ui, float cx, float cy, float r_out, float r_in, int points, float rot, uint32_t colour) {
    int n = points * 2;
    if (n < 6 || n > 64) {
        return;
    }
    float vx[64], vy[64];
    for (int i = 0; i < n; i++) {
        float a = rot + (float)i * 3.14159265f / (float)points, r = (i & 1) ? r_in : r_out;
        vx[i] = cx + cosf(a) * r;
        vy[i] = cy + sinf(a) * r;
    }
    int x0 = (int)(cx - r_out) - 1, x1 = (int)(cx + r_out) + 1, y0 = (int)(cy - r_out) - 1, y1 = (int)(cy + r_out) + 1;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            int hit = 0;
            for (int sy = 0; sy < 3; sy++) {
                for (int sx = 0; sx < 3; sx++) {
                    float px = (float)x + ((float)sx + 0.5f) / 3.0f, py = (float)y + ((float)sy + 0.5f) / 3.0f;
                    for (int i = 0; i < n; i++) { /* the star is a fan of triangles around its centre */
                        int j = (i + 1) % n;
                        if (in_triangle(px, py, cx, cy, vx[i], vy[i], vx[j], vy[j])) {
                            hit++;
                            break;
                        }
                    }
                }
            }
            if (hit) {
                put(ui, x, y, colour, hit * 255 / 9);
            }
        }
    }
}

/* ------------------------------------------------------------------ text --- */
UiFont *ui_font_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = size > 0 ? (unsigned char *)malloc((size_t)size) : NULL;
    if (!data || fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    UiFont *font = (UiFont *)calloc(1, sizeof *font);
    if (!font || !stbtt_InitFont(&font->info, data, stbtt_GetFontOffsetForIndex(data, 0))) {
        free(data);
        free(font);
        return NULL;
    }
    font->data = data;
    return font;
}

#define GLYPH_SLOTS 2048
typedef struct {
    const UiFont *font;
    int size4, cp;
    unsigned char *bmp;
    int w, h, xoff, yoff;
    float advance;
    int used;
} Glyph;

static Glyph glyphs[GLYPH_SLOTS];

static const Glyph *glyph_for(UiFont *f, float size, int cp) {
    if (!f) {
        return NULL; /* no font: no text (the callers skip a missing glyph) */
    }
    int size4 = (int)(size * 4.0f + 0.5f);
    unsigned slot = ((unsigned)cp * 2654435761u + (unsigned)size4 * 40503u + (unsigned)(size_t)f) % GLYPH_SLOTS;
    for (int probe = 0; probe < GLYPH_SLOTS; probe++, slot = (slot + 1) % GLYPH_SLOTS) {
        Glyph *g = &glyphs[slot];
        if (g->used && g->font == f && g->size4 == size4 && g->cp == cp) {
            return g;
        }
        if (!g->used) {
            float scale = stbtt_ScaleForMappingEmToPixels(&f->info, (float)size4 / 4.0f);
            int adv, lsb;
            stbtt_GetCodepointHMetrics(&f->info, cp, &adv, &lsb);
            g->font = f;
            g->size4 = size4;
            g->cp = cp;
            g->advance = (float)adv * scale;
            g->bmp = stbtt_GetCodepointBitmap(&f->info, scale, scale, cp, &g->w, &g->h, &g->xoff, &g->yoff);
            g->used = 1;
            return g;
        }
    }
    return NULL;
}

static int next_cp(const char **s) { /* one UTF-8 code point */
    const unsigned char *p = (const unsigned char *)*s;
    int cp = *p;
    if (cp < 0x80) {
        *s += 1;
    } else if ((cp & 0xE0) == 0xC0 && p[1]) {
        cp = ((cp & 0x1F) << 6) | (p[1] & 0x3F);
        *s += 2;
    } else if ((cp & 0xF0) == 0xE0 && p[1] && p[2]) {
        cp = ((cp & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        *s += 3;
    } else {
        *s += 1;
        cp = '?';
    }
    return cp;
}

float ui_text_width(UiFont *f, float size, const char *s) {
    float w = 0.0f;
    int prev = 0;
    while (*s) {
        int cp = next_cp(&s);
        const Glyph *g = glyph_for(f, size, cp);
        if (!g) {
            continue;
        }
        if (prev) {
            w += (float)stbtt_GetCodepointKernAdvance(&f->info, prev, cp) * stbtt_ScaleForMappingEmToPixels(&f->info, size);
        }
        w += g->advance;
        prev = cp;
    }
    return w;
}

void ui_text(Ui *ui, UiFont *f, float size, float x, float baseline, uint32_t colour, const char *s) {
    float pen = x;
    int prev = 0;
    while (*s) {
        int cp = next_cp(&s);
        const Glyph *g = glyph_for(f, size, cp);
        if (!g) {
            continue;
        }
        if (prev) {
            pen += (float)stbtt_GetCodepointKernAdvance(&f->info, prev, cp) * stbtt_ScaleForMappingEmToPixels(&f->info, size);
        }
        int gx = (int)floorf(pen + 0.5f) + g->xoff, gy = (int)floorf(baseline + 0.5f) + g->yoff;
        for (int j = 0; j < g->h; j++) {
            for (int i = 0; i < g->w; i++) {
                put(ui, gx + i, gy + j, colour, g->bmp[j * g->w + i]);
            }
        }
        pen += g->advance;
        prev = cp;
    }
}

void ui_text_outlined(Ui *ui, UiFont *f, float size, float x, float baseline, uint32_t colour, uint32_t outline, float thickness, const char *s) {
    for (int k = 0; k < 16; k++) { /* the outline: the text drawn in a ring of offsets */
        float a = (float)k * 3.14159265f / 8.0f;
        ui_text(ui, f, size, x + cosf(a) * thickness, baseline + sinf(a) * thickness, outline, s);
    }
    ui_text(ui, f, size, x, baseline, colour, s);
}

/* Line breaking: calls `emit(start, length)` for every line; returns the number of lines. */
typedef void (*LineFn)(const char *start, int len, int line, void *user);

static int break_lines(UiFont *f, float size, float max_w, const char *s, LineFn emit, void *user) {
    int lines = 0;
    const char *line_start = s, *word = s, *p = s;
    float line_w = 0.0f;
    char buf[512];
    for (;;) {
        int at_end = *p == 0, newline = *p == '\n', space = *p == ' ';
        if (at_end || newline || space) {
            int len = (int)(p - word);
            if (len > 0 && len < (int)sizeof buf) {
                memcpy(buf, word, (size_t)len);
                buf[len] = 0;
                float w = ui_text_width(f, size, buf), sp = line_w > 0.0f ? ui_text_width(f, size, " ") : 0.0f;
                if (line_w > 0.0f && line_w + sp + w > max_w) { /* the word does not fit: the line ends before it */
                    int llen = (int)(word - line_start);
                    while (llen > 0 && line_start[llen - 1] == ' ') {
                        llen--;
                    }
                    emit(line_start, llen, lines++, user);
                    line_start = word;
                    line_w = w;
                } else {
                    line_w += sp + w;
                }
            }
            word = p + 1;
            if (at_end) {
                emit(line_start, (int)(p - line_start), lines++, user);
                break;
            }
            if (newline) {
                emit(line_start, (int)(p - line_start), lines++, user);
                line_start = p + 1;
                line_w = 0.0f;
            }
        }
        p++;
    }
    return lines;
}

typedef struct {
    Ui *ui;
    UiFont *f;
    float size, x, baseline, line_h;
    uint32_t colour;
} WrapDraw;

static void draw_line(const char *start, int len, int line, void *user) {
    WrapDraw *d = (WrapDraw *)user;
    char buf[512];
    if (len >= (int)sizeof buf) {
        len = (int)sizeof buf - 1;
    }
    memcpy(buf, start, (size_t)len);
    buf[len] = 0;
    ui_text(d->ui, d->f, d->size, d->x, d->baseline + (float)line * d->line_h, d->colour, buf);
}

static void count_line(const char *start, int len, int line, void *user) {
    (void)start;
    (void)len;
    (void)line;
    (void)user;
}

int ui_text_wrapped(Ui *ui, UiFont *f, float size, float x, float baseline, float max_w, float line_h, uint32_t colour, const char *s) {
    WrapDraw d = {ui, f, size, x, baseline, line_h, colour};
    return break_lines(f, size, max_w, s, draw_line, &d);
}

int ui_text_lines(UiFont *f, float size, float max_w, const char *s) {
    return break_lines(f, size, max_w, s, count_line, NULL);
}

/* ------------------------------------------------------------------ images --- */
UiImage *ui_image_load(const char *path) {
    int w, h, n;
    unsigned char *rgba = stbi_load(path, &w, &h, &n, 4);
    if (!rgba) {
        return NULL;
    }
    UiImage *im = (UiImage *)calloc(1, sizeof *im);
    if (!im) {
        stbi_image_free(rgba);
        return NULL;
    }
    im->w = w;
    im->h = h;
    im->rgba = rgba;
    return im;
}

UiImage *ui_image_from_rgba(const unsigned char *rgba, int w, int h) {
    UiImage *im = (UiImage *)calloc(1, sizeof *im);
    if (!im) {
        return NULL;
    }
    im->rgba = (unsigned char *)malloc((size_t)w * (size_t)h * 4);
    if (!im->rgba) {
        free(im);
        return NULL;
    }
    memcpy(im->rgba, rgba, (size_t)w * (size_t)h * 4);
    im->w = w;
    im->h = h;
    return im;
}

int ui_image_w(const UiImage *im) {
    return im ? im->w : 0;
}

int ui_image_h(const UiImage *im) {
    return im ? im->h : 0;
}

void ui_image_draw(Ui *ui, const UiImage *im, float x, float y, float w, float h, int alpha, int smooth) {
    if (!im || w < 1.0f || h < 1.0f) {
        return;
    }
    int x0 = (int)floorf(x), y0 = (int)floorf(y), x1 = (int)ceilf(x + w), y1 = (int)ceilf(y + h);
    for (int py = y0; py < y1; py++) {
        for (int px = x0; px < x1; px++) {
            float u = ((float)px + 0.5f - x) / w * (float)im->w, v = ((float)py + 0.5f - y) / h * (float)im->h;
            if (u < 0 || v < 0 || u >= (float)im->w || v >= (float)im->h) {
                continue;
            }
            unsigned r, g, b, a;
            if (smooth) { /* bilinear */
                float fu = u - 0.5f, fv = v - 0.5f;
                int iu = (int)floorf(fu), iv = (int)floorf(fv);
                float tu = fu - (float)iu, tv = fv - (float)iv;
                float acc[4] = {0, 0, 0, 0};
                for (int k = 0; k < 4; k++) {
                    int sx = iu + (k & 1), sy = iv + (k >> 1);
                    sx = sx < 0 ? 0 : sx >= im->w ? im->w - 1 : sx;
                    sy = sy < 0 ? 0 : sy >= im->h ? im->h - 1 : sy;
                    float wt = ((k & 1) ? tu : 1 - tu) * ((k >> 1) ? tv : 1 - tv);
                    const unsigned char *s = &im->rgba[((size_t)sy * (size_t)im->w + (size_t)sx) * 4];
                    float sa = (float)s[3] * wt;
                    acc[0] += (float)s[0] * sa;
                    acc[1] += (float)s[1] * sa;
                    acc[2] += (float)s[2] * sa;
                    acc[3] += sa;
                }
                a = (unsigned)acc[3];
                if (acc[3] < 1e-3f) {
                    continue;
                }
                r = (unsigned)(acc[0] / acc[3]);
                g = (unsigned)(acc[1] / acc[3]);
                b = (unsigned)(acc[2] / acc[3]);
            } else {
                const unsigned char *s = &im->rgba[((size_t)(int)v * (size_t)im->w + (size_t)(int)u) * 4];
                r = s[0];
                g = s[1];
                b = s[2];
                a = s[3];
            }
            put(ui, px, py, (r << 16) | (g << 8) | b, (int)(a * (unsigned)alpha / 255u));
        }
    }
}

void ui_picture(Ui *ui, const uint32_t *argb, int sw, int sh, int x, int y, int w, int h, int dim) {
    for (int j = 0; j < h; j++) {
        int sy = j * sh / h;
        for (int i = 0; i < w; i++) {
            int sx = i * sw / w;
            uint32_t c = argb[sy * sw + sx] & 0xFFFFFF;
            if (dim > 0) {
                c = mix(c, 0x000000, dim);
            }
            put(ui, x + i, y + j, c, 255);
        }
    }
}
