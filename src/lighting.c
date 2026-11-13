/* Lighting, see lighting.h. The light map is soft, so it is computed at a quarter of the size of the picture (a value per square of 4x4 pixels); the bloom and the glow of the
 * bright parts at half the size. Both are stretched back with a bilinear filter whose weights are worked out once per size. Every buffer is static (no allocation per frame). */
#include <math.h>
#include <string.h>
#include "lighting.h"
#include "parallel.h"

#define MAX_W 256
#define MAX_ROWS 240

/* A box blur of radius r (several passes of it look like a Gaussian), horizontally and then vertically, on one channel. */
static void box_blur(float *a, float *tmp, int w, int h, int r, int passes) {
    float inv = 1.0f / (float)(2 * r + 1);
    for (int pass = 0; pass < passes; pass++) {
        for (int y = 0; y < h; y++) {
            const float *s = &a[(size_t)y * (size_t)w];
            float *d = &tmp[(size_t)y * (size_t)w];
            float sum = 0.0f;
            for (int k = -r; k <= r; k++) {
                sum += s[k < 0 ? 0 : k >= w ? w - 1 : k];
            }
            for (int x = 0; x < w; x++) {
                d[x] = sum * inv;
                int add = x + r + 1, sub = x - r;
                sum += s[add >= w ? w - 1 : add] - s[sub < 0 ? 0 : sub];
            }
        }
        for (int x = 0; x < w; x++) {
            float sum = 0.0f;
            for (int k = -r; k <= r; k++) {
                sum += tmp[(size_t)(k < 0 ? 0 : k >= h ? h - 1 : k) * (size_t)w + (size_t)x];
            }
            for (int y = 0; y < h; y++) {
                a[(size_t)y * (size_t)w + (size_t)x] = sum * inv;
                int add = y + r + 1, sub = y - r;
                sum += tmp[(size_t)(add >= h ? h - 1 : add) * (size_t)w + (size_t)x] - tmp[(size_t)(sub < 0 ? 0 : sub) * (size_t)w + (size_t)x];
            }
        }
    }
}

static float clamp01(float v) {
    return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

/* The bilinear stretch of a map that is `f` times smaller than the picture: for every column (or line) of the picture, the two values of the map it lies between and the
 * weight of the second one. */
typedef struct {
    int i0[MAX_ROWS], i1[MAX_ROWS];
    float w1[MAX_ROWS];
} Stretch;

static void stretch_axis(Stretch *s, int n, int f, int map_n) {
    for (int x = 0; x < n; x++) {
        float u = ((float)x + 0.5f) / (float)f - 0.5f;
        int i = (int)floorf(u);
        float fr = u - (float)i;
        s->i0[x] = i < 0 ? 0 : i >= map_n ? map_n - 1 : i;
        s->i1[x] = i + 1 < 0 ? 0 : i + 1 >= map_n ? map_n - 1 : i + 1;
        s->w1[x] = fr;
    }
}

/* A smooth noise (value noise on a lattice) between 0 and 1. */
static float lattice(int x, int y) {
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 1023u) / 1023.0f;
}

static float noise2(float x, float y) {
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float fx = x - (float)xi, fy = y - (float)yi;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    float a = lattice(xi, yi) * (1.0f - fx) + lattice(xi + 1, yi) * fx, b = lattice(xi, yi + 1) * (1.0f - fx) + lattice(xi + 1, yi + 1) * fx;
    return a * (1.0f - fy) + b * fy;
}

/* Does a ray from (lx, ly) to (tx, ty) cross a solid square before it gets to the square of the target? (the square of the light and the square of the target do not count) */
static int blocked(const uint8_t *solid8, int cw, int ch, float lx, float ly, float tx, float ty) {
    float dx = tx - lx, dy = ty - ly, len = sqrtf(dx * dx + dy * dy);
    int steps = (int)(len * (1.0f / 3.0f));
    int c0x = (int)(lx * 0.125f), c0y = (int)(ly * 0.125f), c1x = (int)(tx * 0.125f), c1y = (int)(ty * 0.125f);
    float inv = steps > 0 ? 1.0f / (float)steps : 0.0f;
    for (int i = 1; i < steps; i++) {
        float t = (float)i * inv;
        int cx = (int)((lx + dx * t) * 0.125f), cy = (int)((ly + dy * t) * 0.125f);
        if (cx < 0 || cy < 0 || cx >= cw || cy >= ch || (cx == c0x && cy == c0y) || (cx == c1x && cy == c1y)) {
            continue;
        }
        if (solid8[cy * cw + cx]) {
            return 1;
        }
    }
    return 0;
}

static float src[MAX_W * MAX_ROWS * 3], lit[MAX_W * MAX_ROWS * 3];
static float gm[(MAX_W / 4) * (MAX_ROWS / 4) * 3]; /* the light that is added to the picture, 4x4 pixels per value */
static float lm[(MAX_W / 4) * (MAX_ROWS / 4) * 3]; /* the light map, 4x4 pixels per value */
static float pool[(MAX_W / 2) * (MAX_ROWS / 2)], bloom[(MAX_W / 2) * (MAX_ROWS / 2) * 3], scratch[(MAX_W / 2) * (MAX_ROWS / 2)];
static float wat[(MAX_W / 2) * (MAX_ROWS / 2)];  /* how much of the picture is water (blue), at half size */
static float mist[(MAX_W / 4) * (MAX_ROWS / 4)]; /* the density of the mist, 4x4 pixels per value */
static Stretch sx2, sy2, sx4, sy4;
static int stretch_w, stretch_rows;

/* What the phases below, which run on several threads, work with (the picture is cut in pairs of lines: a pair is one line of the half-size maps). */
static struct {
    uint32_t *pic;
    const LightParams *p;
    int w, rows, hw, hh, qw, qh, graded;
    float time, bloom_gain, hole_x, hole_y, hole_r;
    float lut[3][1024];
} job;

/* Pass 1: the colours as floats, and the brightness of the picture at half size (the bright parts are small lamps of their own). */
static void phase_convert(int first, int last) {
    for (int pair = first; pair < last; pair++) {
        for (int j = 0; j < 2; j++) {
            int y = 2 * pair + j;
            for (int x = 0; x < job.w; x++) {
                uint32_t c = job.pic[(size_t)y * (size_t)job.w + (size_t)x];
                size_t i = ((size_t)y * (size_t)job.w + (size_t)x) * 3;
                src[i] = (float)((c >> 16) & 255) * (1.0f / 255.0f);
                src[i + 1] = (float)((c >> 8) & 255) * (1.0f / 255.0f);
                src[i + 2] = (float)(c & 255) * (1.0f / 255.0f);
                float mx = src[i] > src[i + 1] ? src[i] : src[i + 1];
                mx = mx > src[i + 2] ? mx : src[i + 2];
                pool[(size_t)pair * (size_t)job.hw + (size_t)(x >> 1)] += clamp01((mx - 0.75f) * 4.0f) * 0.25f;
                wat[(size_t)pair * (size_t)job.hw + (size_t)(x >> 1)] += (src[i + 2] > 0.75f && src[i] < 0.25f) ? 0.25f : 0.0f;
            }
        }
    }
}

/* Pass 2: the picture times (ambient + light), and its luminance-weighted bloom, summed at half size. */
static void phase_light(int first, int last) {
    const LightParams *p = job.p;
    int w = job.w, hw = job.hw, qw = job.qw;
    for (int pair = first; pair < last; pair++) {
        for (int j = 0; j < 2; j++) {
            int y = 2 * pair + j;
            int ya = sy4.i0[y], yb = sy4.i1[y];
            float wy = sy4.w1[y], wpy = sy2.w1[y];
            const float *p0 = &pool[(size_t)sy2.i0[y] * (size_t)hw], *p1 = &pool[(size_t)sy2.i1[y] * (size_t)hw];
            for (int x = 0; x < w; x++) {
                size_t i = ((size_t)y * (size_t)w + (size_t)x) * 3;
                float wx = sx4.w1[x], wpx = sx2.w1[x];
                const float *a = &lm[((size_t)ya * (size_t)qw + (size_t)sx4.i0[x]) * 3], *b = &lm[((size_t)ya * (size_t)qw + (size_t)sx4.i1[x]) * 3];
                const float *c = &lm[((size_t)yb * (size_t)qw + (size_t)sx4.i0[x]) * 3], *d = &lm[((size_t)yb * (size_t)qw + (size_t)sx4.i1[x]) * 3];
                float pl = ((p0[sx2.i0[x]] * (1.0f - wpx) + p0[sx2.i1[x]] * wpx) * (1.0f - wpy) + (p1[sx2.i0[x]] * (1.0f - wpx) + p1[sx2.i1[x]] * wpx) * wpy) *
                           p->pools;
                for (int ch = 0; ch < 3; ch++) {
                    float l = (a[ch] * (1.0f - wx) + b[ch] * wx) * (1.0f - wy) + (c[ch] * (1.0f - wx) + d[ch] * wx) * wy;
                    l += pl * (ch == 0 ? 1.0f : ch == 1 ? 0.95f : 0.85f);
                    float gl = (gm[((size_t)ya * (size_t)qw + (size_t)sx4.i0[x]) * 3 + (size_t)ch] * (1.0f - wx) +
                                gm[((size_t)ya * (size_t)qw + (size_t)sx4.i1[x]) * 3 + (size_t)ch] * wx) *
                                   (1.0f - wy) +
                               (gm[((size_t)yb * (size_t)qw + (size_t)sx4.i0[x]) * 3 + (size_t)ch] * (1.0f - wx) +
                                gm[((size_t)yb * (size_t)qw + (size_t)sx4.i1[x]) * 3 + (size_t)ch] * wx) *
                                   wy;
                    lit[i + (size_t)ch] = src[i + (size_t)ch] * (p->ambient[ch] + l) + gl;
                }
                float lum = 0.2126f * lit[i] + 0.7152f * lit[i + 1] + 0.0722f * lit[i + 2];
                lum = lum > 1.0f ? 1.0f : lum;
                float *bl = &bloom[((size_t)pair * (size_t)hw + (size_t)(x >> 1)) * 3];
                float k = lum * 0.125f;
                if (job.hole_r > 0.0f) { /* no glow right around the character that carries the first light */
                    float dx = (float)x - job.hole_x, dy = (float)y - job.hole_y, d2 = dx * dx + dy * dy, r2 = job.hole_r * job.hole_r;
                    if (d2 < r2) {
                        k = 0.0f;
                    } else if (d2 < (job.hole_r + 8.0f) * (job.hole_r + 8.0f)) {
                        k *= (sqrtf(d2) - job.hole_r) * 0.125f;
                    }
                }
                bl[0] += lit[i] * k;
                bl[1] += lit[i + 1] * k;
                bl[2] += lit[i + 2] * k;
            }
        }
    }
}

/* Pass 3: the bloom added, the haze, the grade, the saturation, the shimmer, the vignette; written back to the picture. */
static void phase_finish(int first, int last) {
    const LightParams *p = job.p;
    int w = job.w, hw = job.hw, rows = job.rows;
    for (int pair = first; pair < last; pair++) {
        for (int j = 0; j < 2; j++) {
            int y = 2 * pair + j;
            float fogf = clamp01((float)y / (float)rows - 0.3f) * p->fog_amount, vy = ((float)y - (float)rows * 0.5f) / ((float)rows * 0.9f);
            float wy = sy2.w1[y];
            const float *r0 = &bloom[(size_t)sy2.i0[y] * (size_t)hw * 3], *r1 = &bloom[(size_t)sy2.i1[y] * (size_t)hw * 3];
            for (int x = 0; x < w; x++) {
                size_t i = ((size_t)y * (size_t)w + (size_t)x) * 3;
                float wx = sx2.w1[x], o[3];
                float ms = 0.0f;
                {
                    int ya = sy4.i0[y], yb = sy4.i1[y], x0 = sx4.i0[x], x1 = sx4.i1[x], qw4 = job.qw;
                    float wx4 = sx4.w1[x], wy4 = sy4.w1[y];
                    ms = (mist[ya * qw4 + x0] * (1.0f - wx4) + mist[ya * qw4 + x1] * wx4) * (1.0f - wy4) +
                         (mist[yb * qw4 + x0] * (1.0f - wx4) + mist[yb * qw4 + x1] * wx4) * wy4;
                }
                float ff = fogf + ms;
                ff = ff > 0.85f ? 0.85f : ff;
                const float *a = &r0[(size_t)sx2.i0[x] * 3], *b = &r0[(size_t)sx2.i1[x] * 3], *c = &r1[(size_t)sx2.i0[x] * 3], *d = &r1[(size_t)sx2.i1[x] * 3];
                for (int ch = 0; ch < 3; ch++) {
                    float bl = (a[ch] * (1.0f - wx) + b[ch] * wx) * (1.0f - wy) + (c[ch] * (1.0f - wx) + d[ch] * wx) * wy;
                    float col = lit[i + (size_t)ch] + job.bloom_gain * bl;
                    col = col * (1.0f - ff) + p->fog[ch] * ff;
                    col = clamp01(col);
                    o[ch] = job.graded ? job.lut[ch][(int)(col * 1023.0f + 0.5f)] : col;
                }
                float lum = 0.2126f * o[0] + 0.7152f * o[1] + 0.0722f * o[2];
                float nz = p->noise > 0.0f
                               ? 1.0f + (noise2((float)x * 0.06f + job.time * 0.012f, (float)y * 0.06f - job.time * 0.008f) - 0.5f) * p->noise * 2.0f
                               : 1.0f;
                float vx = ((float)x - (float)w * 0.5f) / ((float)w * 0.8f);
                float vg = 1.0f - p->vignette * (vx * vx + vy * vy);
                vg = vg < 0.45f ? 0.45f : vg > 1.0f ? 1.0f : vg;
                float k = nz * vg;
                uint32_t rgb = 0;
                for (int ch = 0; ch < 3; ch++) {
                    float col = (lum + (o[ch] - lum) * p->saturation) * k;
                    rgb = (rgb << 8) | (uint32_t)(clamp01(col) * 255.0f + 0.5f);
                }
                job.pic[(size_t)y * (size_t)w + (size_t)x] = rgb;
            }
        }
    }
}

void lighting_apply(uint32_t *pic, int w, int h, const LightSource *lights, int count, const LightParams *p, unsigned frame, const uint8_t *solid8) {
    int rows = p->rows > 0 && p->rows < h ? p->rows : h;
    rows &= ~7;
    if (w > MAX_W || rows > MAX_ROWS || w < 16 || rows < 16 || w % 8) {
        return;
    }
    int hw = w / 2, hh = rows / 2, qw = w / 4, qh = rows / 4;
    if (stretch_w != w || stretch_rows != rows) {
        stretch_axis(&sx2, w, 2, hw);
        stretch_axis(&sy2, rows, 2, hh);
        stretch_axis(&sx4, w, 4, qw);
        stretch_axis(&sy4, rows, 4, qh);
        stretch_w = w;
        stretch_rows = rows;
    }
    float time = (float)frame;
    float wave = 1.0f + p->flicker * sinf(time * 0.11f) + 0.5f * p->flicker * sinf(time * 0.37f + 1.3f);
    job.pic = pic;
    job.p = p;
    job.w = w;
    job.rows = rows;
    job.hw = hw;
    job.hh = hh;
    job.qw = qw;
    job.qh = qh;
    job.time = time;
    job.bloom_gain = p->bloom * 1.2f;
    job.hole_r = count > 0 ? p->bloom_hole : 0.0f;
    job.hole_x = count > 0 ? lights[0].x : 0.0f;
    job.hole_y = count > 0 ? lights[0].y : 0.0f;

    memset(pool, 0, (size_t)hw * (size_t)hh * sizeof pool[0]);
    memset(wat, 0, (size_t)hw * (size_t)hh * sizeof wat[0]);
    parallel_rows(phase_convert, rows / 2);
    box_blur(wat, scratch, hw, hh, 2, 1);
    if (solid8) { /* what the bright parts of a solid square light is halved */
        for (int y = 0; y < hh; y++) {
            for (int x = 0; x < hw; x++) {
                pool[(size_t)y * (size_t)hw + (size_t)x] *= solid8[((2 * y) >> 3) * (w / 8) + ((2 * x) >> 3)] ? 0.5f : 1.0f;
            }
        }
    }
    box_blur(pool, scratch, hw, hh, 2, 3);

    /* the light map (a value per 4x4 pixels): the lights, with their shadows, then the glows that drift in the background */
    for (int y = 0; y < qh; y++) {
        for (int x = 0; x < qw; x++) {
            float fx = (float)(4 * x + 2), fy = (float)(4 * y + 2), lr = 0.0f, lg = 0.0f, lb = 0.0f, gr = 0.0f, gg = 0.0f, gb = 0.0f;
            int cell_solid = solid8 && solid8[((4 * y + 2) >> 3) * (w / 8) + ((4 * x + 2) >> 3)];
            for (int k = 0; k < count; k++) {
                float dx = fx - lights[k].x, dy = (fy - lights[k].y) * 1.1f;
                float d = sqrtf(dx * dx + dy * dy) / (lights[k].radius * wave * (cell_solid ? 1.0f : p->far_light));
                if (d < 1.0f && !(solid8 && blocked(solid8, w / 8, rows / 8, lights[k].x, lights[k].y, fx, fy))) {
                    float e = 1.0f - d, f = e * sqrtf(e) * lights[k].intensity * (cell_solid ? p->solid_light : 1.0f); /* e to the power 1.5 */
                    lr += f * lights[k].r;
                    lg += f * lights[k].g;
                    lb += f * lights[k].b;
                    gr += e * e * (3.0f - 2.0f * e) * lights[k].glow * lights[k].r;
                    gg += e * e * (3.0f - 2.0f * e) * lights[k].glow * lights[k].g;
                    gb += e * e * (3.0f - 2.0f * e) * lights[k].glow * lights[k].b;
                }
            }
            if (p->waves > 0.0f && !cell_solid &&
                (!p->waves_water ||
                 wat[(2 * y) * hw + 2 * x] >
                     0.3f)) { /* bands of light that drift, like the light on the ceiling of a room above water: the product of two slow waves */
                float wtime = time * (p->wave_speed > 0.0f ? p->wave_speed : 1.0f);
                float wv = sinf(fx * 0.045f + wtime * 0.04f + 1.6f * sinf(fy * 0.06f + wtime * 0.025f)) *
                           sinf(fy * 0.07f - wtime * 0.03f + 1.3f * sinf(fx * 0.05f - wtime * 0.02f));
                wv = wv > 0.0f ? wv * wv : 0.0f;
                float f = p->waves * wv;
                if (p->waves_water) {
                    f *= wat[(2 * y) * hw + 2 * x];
                }
                lr += f * 0.30f;
                lg += f * 0.75f;
                lb += f;
            }
            if (p->glows > 0.0f && !cell_solid) {
                for (int k = 0; k < 5; k++) { /* dark violet glows (r, 0, 2r) that wander slowly: they colour the background as lights do */
                    float gx = (float)w * (0.5f + 0.48f * sinf(time * (0.0045f + 0.0011f * (float)k) + (float)k * 1.9f));
                    float gy = (float)rows * (0.5f + 0.40f * sinf(time * (0.0038f + 0.0013f * (float)k) + (float)k * 2.7f));
                    float dx = fx - gx, dy = fy - gy, d = sqrtf(dx * dx + dy * dy) * (1.0f / 42.0f);
                    if (d < 1.0f) {
                        float f = (1.0f - d) * (1.0f - d) * p->glows * (0.6f + 0.4f * sinf(time * 0.03f + (float)k * 1.1f));
                        lr += f;
                        lb += 2.0f * f;
                    }
                }
            }
            size_t i = ((size_t)y * (size_t)qw + (size_t)x) * 3;
            lm[i] = lr;
            lm[i + 1] = lg;
            lm[i + 2] = lb;
            gm[i] = gr;
            gm[i + 1] = gg;
            gm[i + 2] = gb;
        }
    }
    if (solid8) { /* the edges of the shadows of the platforms are soft */
        for (int c = 0; c < 3; c++) {
            for (int i = 0; i < qw * qh; i++) {
                scratch[i] = lm[(size_t)i * 3 + (size_t)c];
            }
            box_blur(scratch, bloom, qw, qh, 1, 2); /* (the bloom buffer is only used as scratch here) */
            for (int i = 0; i < qw * qh; i++) {
                lm[(size_t)i * 3 + (size_t)c] = scratch[i];
            }
        }
    }

    for (int c = 0; c < 3; c++) { /* the glow map is softened too */
        for (int i = 0; i < qw * qh; i++) {
            scratch[i] = gm[(size_t)i * 3 + (size_t)c];
        }
        box_blur(scratch, bloom, qw, qh, 1, 1);
        for (int i = 0; i < qw * qh; i++) {
            gm[(size_t)i * 3 + (size_t)c] = scratch[i];
        }
    }
    if (p->mist > 0.0f || p->mist_water > 0.0f) { /* the mist: two layers of slow noise, thicker over the water */
        float sp = p->mist_speed > 0.0f ? p->mist_speed : 1.0f;
        for (int y = 0; y < qh; y++) {
            for (int x = 0; x < qw; x++) {
                float fx = (float)(4 * x + 2), fy = (float)(4 * y + 2);
                float n = 0.6f * noise2(fx * 0.03f + time * 0.004f * sp, fy * 0.03f - time * 0.002f * sp) +
                          0.4f * noise2(fx * 0.08f - time * 0.007f * sp + 50.0f, fy * 0.08f + time * 0.003f * sp + 20.0f);
                float dens = clamp01((n - 0.38f) * 2.2f);
                mist[y * qw + x] = dens * (p->mist + p->mist_water * wat[(2 * y) * hw + 2 * x]);
            }
        }
    } else {
        memset(mist, 0, (size_t)qw * (size_t)qh * sizeof mist[0]);
    }
    memset(bloom, 0, (size_t)hw * (size_t)hh * 3 * sizeof bloom[0]);
    parallel_rows(phase_light, rows / 2);
    for (int c = 0; c < 3; c++) {
        for (int i = 0; i < hw * hh; i++) {
            scratch[i] = bloom[(size_t)i * 3 + (size_t)c];
        }
        box_blur(scratch, pool, hw, hh, 2, 3); /* (the brightness map is no longer needed: it is the scratch) */
        for (int i = 0; i < hw * hh; i++) {
            bloom[(size_t)i * 3 + (size_t)c] = scratch[i];
        }
    }

    job.graded = 0; /* the grade: each colour raised to its power (skipped when it is 1) */
    for (int c = 0; c < 3; c++) {
        job.graded |= fabsf(p->gamma[c] - 1.0f) > 0.001f;
    }
    if (job.graded) {
        for (int c = 0; c < 3; c++) {
            for (int i = 0; i < 1024; i++) {
                job.lut[c][i] = powf((float)i * (1.0f / 1023.0f), p->gamma[c] > 0.01f ? p->gamma[c] : 1.0f);
            }
        }
    }
    parallel_rows(phase_finish, rows / 2);
    if (solid8 &&
        p->particles > 0) { /* water drips from the underside of the ceilings and the platforms: a bead forms, grows, falls and splashes on what is below */
        int density = p->particles * 255 / 100, rows8 = rows / 8, cols8 = w / 8;
        for (int cx = 0; cx < cols8; cx++) {
            unsigned hsh = (unsigned)(((p->scroll_x >> 3) + cx) * 2654435761u) ^ 0x9E3779B9u;
            hsh = (hsh ^ (hsh >> 15)) * 2246822519u;
            if ((int)((hsh >> 8) & 255u) >= density) {
                continue;
            }
            int cy0 = -1, cyl = rows8; /* the underside the water comes from, and what it falls on */
            for (int cy = 1; cy < rows8; cy++) {
                if (solid8[(cy - 1) * cols8 + cx] && !solid8[cy * cols8 + cx]) {
                    cy0 = cy;
                    break;
                }
            }
            if (cy0 < 0) {
                continue;
            }
            for (int cy = cy0 + 1; cy < rows8; cy++) {
                if (solid8[cy * cols8 + cx]) {
                    cyl = cy;
                    break;
                }
            }
            float y0 = (float)(cy0 * 8), y1 = (float)(cyl * 8), dist = y1 - y0;
            if (dist < 12.0f) {
                continue;
            }
            const float accel = 0.06f, bead = 18.0f;
            float fall = sqrtf(2.0f * dist / accel);
            unsigned period = 100u + ((hsh >> 16) % 120u);
            float t = (float)((frame + ((hsh >> 20) & 255u) * 7u) % period);
            int x = cx * 8 + 3 + (int)((hsh >> 4) & 3u);
            float yy = -1.0f, k = 0.0f;
            int splash = 0;
            if (t < bead) { /* the bead grows */
                yy = y0 + 1.0f;
                k = 0.7f * t / bead;
            } else if (t < bead + fall) {
                float ft = t - bead;
                yy = y0 + 2.0f + 0.5f * accel * ft * ft;
                k = 1.0f;
            } else if (t < bead + fall + 6.0f) {
                splash = 1;
                yy = y1 - 1.0f;
                k = 0.6f * (1.0f - (t - bead - fall) / 6.0f);
            }
            if (yy < 0.0f) {
                continue;
            }
            for (int tail = 0; tail < (splash ? 1 : 4); tail++) {
                for (int dx = splash ? -2 : 0; dx <= (splash ? 2 : 0);
                     dx++) { /* a drop: a bright head and a tail of 3 pixels above it, fading; a splash: a few pixels sideways */
                    int px = x + dx, py = (int)yy - tail;
                    if (px < 1 || py < 1 || px >= w - 1 || py >= rows - 1 || (solid8[(py >> 3) * cols8 + (px >> 3)] && !splash)) {
                        continue;
                    }
                    float kk = k * (tail == 0 ? 1.0f : 0.55f / (float)tail) * (dx == 0 ? 1.0f : 0.5f);
                    uint32_t c = pic[(size_t)py * (size_t)w + (size_t)px];
                    float r = (float)((c >> 16) & 255) + 110.0f * kk, g = (float)((c >> 8) & 255) + 190.0f * kk, bl = (float)(c & 255) + 255.0f * kk;
                    pic[(size_t)py * (size_t)w + (size_t)px] =
                        ((uint32_t)(r > 255.0f ? 255.0f : r) << 16) | ((uint32_t)(g > 255.0f ? 255.0f : g) << 8) | (uint32_t)(bl > 255.0f ? 255.0f : bl);
                }
            }
        }
    }
    for (int i = 0; i < p->dust; i++) { /* motes of dust: they drift slowly, belong to the world, and catch the light */
        unsigned hsh = (unsigned)(i + 1) * 2654435761u;
        hsh = (hsh ^ (hsh >> 15)) * 2246822519u;
        hsh ^= hsh >> 13;
        float ph = (float)(hsh & 1023u) * 0.0061f, ph2 = (float)((hsh >> 10) & 1023u) * 0.0061f;
        float bx = (float)((hsh >> 3) % 400u), by = (float)((hsh >> 12) % (unsigned)rows);
        float sx = fmodf(bx - (float)p->scroll_x * 0.9f + 4000.0f, 400.0f) - 72.0f + 7.0f * sinf(time * 0.013f + ph);
        float sy = by + 5.0f * sinf(time * 0.017f + ph2) + 0.02f * time;
        sy = fmodf(sy, (float)rows);
        int px = (int)sx, py = (int)sy;
        if (px < 1 || py < 1 || px >= w - 1 || py >= rows - 1 || (solid8 && solid8[(py >> 3) * (w / 8) + (px >> 3)])) {
            continue;
        }
        float tw = 0.5f + 0.5f * sinf(time * 0.05f + ph * 3.0f);
        uint32_t c = pic[(size_t)py * (size_t)w + (size_t)px];
        float lum = (float)(((c >> 16) & 255) + ((c >> 8) & 255) + (c & 255)) / 765.0f;
        float k = tw * (0.15f + 0.9f * lum) * 0.55f;
        float r = (float)((c >> 16) & 255) + 255.0f * k, g = (float)((c >> 8) & 255) + 215.0f * k, bl = (float)(c & 255) + 150.0f * k;
        pic[(size_t)py * (size_t)w + (size_t)px] =
            ((uint32_t)(r > 255.0f ? 255.0f : r) << 16) | ((uint32_t)(g > 255.0f ? 255.0f : g) << 8) | (uint32_t)(bl > 255.0f ? 255.0f : bl);
    }
}
