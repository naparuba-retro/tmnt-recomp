/* The CRT look, see crt.h. Linear light in 12 bits, integer weights out of tables built when the size or the settings change.
 *
 * Flat screen (the fast path): each of the 240 rows of the game is first resampled across the width of the picture (with a sharpened bilinear filter) and multiplied by
 * the phosphor mask, once per frame; every row of the window then mixes the two nearest game rows with the weights of a Gaussian beam (exp2(hard * d * d), d = the
 * distance to the row's centre in game rows) - two multiplies per colour, no division.
 * Curved screen: the same with the position of each window pixel warped, computed pixel by pixel (slower, it is optional). */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "crt.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#define SW 256
#define SH 240
#define LIN_MAX 4095
#define MAX_WORKERS 8 /* threads that share the rows of a picture */
#define BLOOM 32      /* of 256: how much of the blurred picture is added back (the glow around bright pixels) */

static uint16_t lut_in[256];
static uint8_t lut_out[LIN_MAX + 1];
static int luts_ready;

static void init_luts(void) {
    if (luts_ready) {
        return;
    }
    for (int i = 0; i < 256; i++) {
        lut_in[i] = (uint16_t)(pow(i / 255.0, 2.2) * LIN_MAX + 0.5);
    }
    for (int i = 0; i <= LIN_MAX; i++) {
        lut_out[i] = (uint8_t)(pow((double)i / LIN_MAX, 1.0 / 2.2) * 255.0 + 0.5);
    }
    luts_ready = 1;
}

static struct {
    int valid, w, h;
    CrtParams p;
    int pw, ph, x0, y0; /* the rectangle of the window that the picture covers */
    float sharp;
    int mask_lit, mask_dark;     /* of 256 */
    uint16_t beam[257];          /* the weight of a game row at the distance i / 256 (of 256, normalised) */
    int *col_x0, *col_x1;        /* the two game columns of each window column */
    uint16_t *col_wx, *col_mask; /* the weight of the second one (of 256); the mask of the three colours (of 256) */
    int *row_y0, *row_y1;        /* the two game rows of each window row */
    uint16_t *row_w0, *row_w1;   /* and their beam weights */
    uint16_t *hrow;              /* SH rows of pw pixels of three colours: the game's rows resampled, with the mask */
} st;

static uint16_t lin[SH * SW * 3], glow[SH * SW * 3], tmp[SH * SW * 3];

static void free_tables(void) {
    free(st.col_x0);
    free(st.col_x1);
    free(st.col_wx);
    free(st.col_mask);
    free(st.row_y0);
    free(st.row_y1);
    free(st.row_w0);
    free(st.row_w1);
    free(st.hrow);
    st.col_x0 = st.col_x1 = st.row_y0 = st.row_y1 = NULL;
    st.col_wx = st.col_mask = st.row_w0 = st.row_w1 = st.hrow = NULL;
}

static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

/* Rows [0, rows) are cut into slices that run on several threads (the picture of a big window is a few million pixels at 60 frames per second). */
typedef void (*RowFn)(int first, int last);
typedef struct {
    RowFn fn;
    int first, last;
} Slice;

#ifdef _WIN32
static DWORD WINAPI slice_main(LPVOID arg) {
    Slice *s = (Slice *)arg;
    s->fn(s->first, s->last);
    return 0;
}
#else
static void *slice_main(void *arg) {
    Slice *s = (Slice *)arg;
    s->fn(s->first, s->last);
    return NULL;
}
#endif

static int worker_count(void) {
    static int n;
    if (!n) {
#ifdef _WIN32
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        n = (int)si.dwNumberOfProcessors;
#else
        n = (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
        n = n < 1 ? 1 : n > MAX_WORKERS ? MAX_WORKERS : n;
    }
    return n;
}

static void run_rows(RowFn fn, int rows) {
    int n = worker_count();
    if (n < 2 || rows < 64) {
        fn(0, rows);
        return;
    }
    Slice sl[MAX_WORKERS];
#ifdef _WIN32
    HANDLE th[MAX_WORKERS];
#else
    pthread_t th[MAX_WORKERS];
#endif
    int started[MAX_WORKERS] = {0};
    for (int i = 0; i < n; i++) {
        sl[i].fn = fn;
        sl[i].first = rows * i / n;
        sl[i].last = rows * (i + 1) / n;
    }
    for (int i = 1; i < n; i++) { /* the slice 0 runs here */
#ifdef _WIN32
        th[i] = CreateThread(NULL, 0, slice_main, &sl[i], 0, NULL);
        started[i] = th[i] != NULL;
#else
        started[i] = pthread_create(&th[i], NULL, slice_main, &sl[i]) == 0;
#endif
        if (!started[i]) {
            fn(sl[i].first, sl[i].last);
        }
    }
    fn(sl[0].first, sl[0].last);
    for (int i = 1; i < n; i++) {
        if (started[i]) {
#ifdef _WIN32
            WaitForSingleObject(th[i], INFINITE);
            CloseHandle(th[i]);
#else
            pthread_join(th[i], NULL);
#endif
        }
    }
}

/* The rectangle of the picture in a window: a whole multiple of 256x240 if asked and if it fits, else the largest one that keeps the proportions. */
static void layout(int w, int h, int integer, int *x0, int *y0, int *pw, int *ph) {
    int k = w / SW < h / SH ? w / SW : h / SH;
    if (integer && k >= 1) {
        *pw = SW * k;
        *ph = SH * k;
    } else if (w * SH < h * SW) {
        *pw = w;
        *ph = w * SH / SW;
    } else {
        *ph = h;
        *pw = h * SW / SH;
    }
    *pw = *pw < 1 ? 1 : *pw;
    *ph = *ph < 1 ? 1 : *ph;
    *x0 = (w - *pw) / 2;
    *y0 = (h - *ph) / 2;
}

static void rebuild(int w, int h, const CrtParams *p) {
    free_tables();
    st.w = w;
    st.h = h;
    st.p = *p;
    layout(w, h, p->integer, &st.x0, &st.y0, &st.pw, &st.ph);
    int pw = st.pw, ph = st.ph;
    float scale_x = (float)pw / SW;
    st.sharp = scale_x * 0.9f > 1.0f ? scale_x * 0.9f : 1.0f;

    /* the beam: exp2(hard * d * d), normalised so that the average brightness stays the same whatever the strength (the peaks may reach a little over 1) */
    if (p->plain) { /* sharp bilinear: the weight of the nearer game row goes from 1 to 0 over about one window pixel, so the rows are not mixed more than needed */
        float sharp_y = (float)ph / SH * 0.9f > 1.0f ? (float)ph / SH * 0.9f : 1.0f;
        for (int i = 0; i <= 256; i++) {
            float k = ((float)i / 256.0f - 0.5f) * sharp_y + 0.5f;
            st.beam[i] = (uint16_t)((1.0f - (k < 0 ? 0 : k > 1 ? 1 : k)) * 256.0f + 0.5f);
        }
    } else {
        double hard = -(p->scanlines / 100.0) * 14.0, avg = 0.0, g[257];
        for (int i = 0; i <= 256; i++) {
            double d = i / 256.0;
            g[i] = exp2(hard * d * d);
        }
        for (int i = 0; i <= 256; i++) {
            avg += g[i] + g[256 - i];
        }
        avg /= 257.0;
        double norm = 1.0 / avg;
        norm = norm > 1.6 ? 1.6 : norm;
        for (int i = 0; i <= 256; i++) {
            st.beam[i] = (uint16_t)(g[i] * norm * 256.0 + 0.5);
        }
    }

    float fade = (scale_x - 1.5f) / 1.5f; /* the mask needs several window pixels per game pixel */
    fade = fade < 0 ? 0 : fade > 1 ? 1 : fade;
    float m = p->plain ? 0.0f : p->mask / 100.0f * fade;
    st.mask_lit = (int)(256.0f * (1.0f + 0.5f * m) + 0.5f);
    st.mask_dark = (int)(256.0f * (1.0f - 0.5f * m) + 0.5f);

    st.col_x0 = (int *)malloc((size_t)pw * sizeof(int));
    st.col_x1 = (int *)malloc((size_t)pw * sizeof(int));
    st.col_wx = (uint16_t *)malloc((size_t)pw * sizeof(uint16_t));
    st.col_mask = (uint16_t *)malloc((size_t)pw * 3 * sizeof(uint16_t));
    st.row_y0 = (int *)malloc((size_t)ph * sizeof(int));
    st.row_y1 = (int *)malloc((size_t)ph * sizeof(int));
    st.row_w0 = (uint16_t *)malloc((size_t)ph * sizeof(uint16_t));
    st.row_w1 = (uint16_t *)malloc((size_t)ph * sizeof(uint16_t));
    st.hrow = (uint16_t *)malloc((size_t)SH * (size_t)pw * 3 * sizeof(uint16_t));
    if (!st.col_x0 || !st.col_x1 || !st.col_wx || !st.col_mask || !st.row_y0 || !st.row_y1 || !st.row_w0 || !st.row_w1 || !st.hrow) {
        free_tables();
        st.valid = 0;
        return;
    }
    for (int ox = 0; ox < pw; ox++) {
        float sx = ((float)ox + 0.5f) * SW / (float)pw - 0.5f;
        int x0 = (int)floorf(sx);
        float t = (sx - (float)x0 - 0.5f) * st.sharp + 0.5f;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        st.col_x0[ox] = clampi(x0, 0, SW - 1);
        st.col_x1[ox] = clampi(x0 + 1, 0, SW - 1);
        st.col_wx[ox] = (uint16_t)(t * 256.0f + 0.5f);
        for (int c = 0; c < 3; c++) {
            st.col_mask[ox * 3 + c] = (uint16_t)(ox % 3 == c ? st.mask_lit : st.mask_dark);
        }
    }
    for (int oy = 0; oy < ph; oy++) {
        float sy = ((float)oy + 0.5f) * SH / (float)ph - 0.5f;
        int y0 = (int)floorf(sy);
        float d0 = sy - (float)y0;
        st.row_y0[oy] = clampi(y0, 0, SH - 1);
        st.row_y1[oy] = clampi(y0 + 1, 0, SH - 1);
        st.row_w0[oy] = st.beam[clampi((int)(d0 * 256.0f), 0, 256)];
        st.row_w1[oy] = st.beam[clampi((int)((1.0f - d0) * 256.0f), 0, 256)];
    }
    st.valid = 1;
}

/* The glow: a box blur of the picture (radius 3), added back a little. */
static void add_glow(void) {
    enum {
        R = 3
    };
    for (int y = 0; y < SH; y++) {
        for (int x = 0; x < SW; x++) {
            for (int c = 0; c < 3; c++) {
                int sum = 0;
                for (int k = -R; k <= R; k++) {
                    sum += lin[((size_t)y * SW + (size_t)clampi(x + k, 0, SW - 1)) * 3 + (size_t)c];
                }
                tmp[((size_t)y * SW + (size_t)x) * 3 + (size_t)c] = (uint16_t)(sum / (2 * R + 1));
            }
        }
    }
    for (int y = 0; y < SH; y++) {
        for (int x = 0; x < SW; x++) {
            for (int c = 0; c < 3; c++) {
                int sum = 0;
                for (int k = -R; k <= R; k++) {
                    sum += tmp[((size_t)clampi(y + k, 0, SH - 1) * SW + (size_t)x) * 3 + (size_t)c];
                }
                glow[((size_t)y * SW + (size_t)x) * 3 + (size_t)c] = (uint16_t)(sum / (2 * R + 1));
            }
        }
    }
    for (int i = 0; i < SH * SW * 3; i++) {
        lin[i] = (uint16_t)(lin[i] + ((glow[i] * BLOOM) >> 8));
    }
}

static uint32_t *job_out;
static int job_w;

static void flat_resample(int first, int last) { /* every game row, across the width of the picture, with the mask */
    int pw = st.pw;
    for (int sy = first; sy < last; sy++) {
        for (int ox = 0; ox < pw; ox++) {
            const uint16_t *a = &lin[((size_t)sy * SW + (size_t)st.col_x0[ox]) * 3], *b = &lin[((size_t)sy * SW + (size_t)st.col_x1[ox]) * 3];
            uint16_t *hp = &st.hrow[((size_t)sy * (size_t)pw + (size_t)ox) * 3];
            int wx = st.col_wx[ox];
            for (int c = 0; c < 3; c++) {
                int v = (a[c] * (256 - wx) + b[c] * wx) >> 8;
                hp[c] = (uint16_t)((v * st.col_mask[ox * 3 + c]) >> 8);
            }
        }
    }
}

static void flat_beam(int first, int last) { /* every window row mixes its two game rows */
    int pw = st.pw;
    for (int oy = first; oy < last; oy++) {
        const uint16_t *h0 = &st.hrow[(size_t)st.row_y0[oy] * (size_t)pw * 3], *h1 = &st.hrow[(size_t)st.row_y1[oy] * (size_t)pw * 3];
        int w0 = st.row_w0[oy], w1 = st.row_w1[oy];
        uint32_t *o = &job_out[(size_t)(st.y0 + oy) * (size_t)job_w + (size_t)st.x0];
        for (int ox = 0; ox < pw; ox++) {
            int r = (h0[ox * 3] * w0 + h1[ox * 3] * w1) >> 8, g = (h0[ox * 3 + 1] * w0 + h1[ox * 3 + 1] * w1) >> 8,
                b = (h0[ox * 3 + 2] * w0 + h1[ox * 3 + 2] * w1) >> 8;
            r = r > LIN_MAX ? LIN_MAX : r;
            g = g > LIN_MAX ? LIN_MAX : g;
            b = b > LIN_MAX ? LIN_MAX : b;
            o[ox] = ((uint32_t)lut_out[r] << 16) | ((uint32_t)lut_out[g] << 8) | lut_out[b];
        }
    }
}

static void render_flat(uint32_t *out, int w) {
    job_out = out;
    job_w = w;
    run_rows(flat_resample, SH);
    run_rows(flat_beam, st.ph);
}

/* The curved screen: the position of every window pixel is pushed outwards like the glass of a tube (more at the corners, as in a barrel distortion), the picture ends
 * where it leaves the game's picture, with a soft edge. In a row, the game column is a linear function of the window column; the game row is a product of a
 * term of the row and a term of the column. So the positions are walked in 16.16 fixed point, one multiply-add and one conversion per pixel, and the game's picture is
 * read from a copy with one more pixel all around, so that no position has to be clamped. */
#define PW_ (SW + 2)
#define PH_ (SH + 2)
static uint16_t linp[PH_ * PW_ * 3];
static uint16_t sharpen[256]; /* the fraction of a game pixel (of 256) made sharper: the pixels stay pixels, with an edge about one window pixel wide */

static struct {
    float *ucol, *ysc;
    float warp_x;
    int64_t mul_x, mul_y;
    int edge_w16, edge_h16;
} cv;

static void curved_rows(int first, int last) {
    int pw = st.pw, ph = st.ph;
    float *ucol = cv.ucol, *ysc = cv.ysc, warp_x = cv.warp_x;
    int64_t mul_x = cv.mul_x, mul_y = cv.mul_y;
    int edge_w16 = cv.edge_w16, edge_h16 = cv.edge_h16;
    uint32_t *out = job_out;
    int w = job_w;
    for (int oy = first; oy < last; oy++) {
        float vc = ((float)oy + 0.5f) / (float)ph * 2.0f - 1.0f, fxr = 1.0f + vc * vc * warp_x;
        float sx0 = (ucol[0] * fxr * 0.5f + 0.5f) * SW + 0.5f,
              dsx = (ucol[1 < pw ? 1 : 0] - ucol[0]) * fxr * 0.5f * SW; /* (+1 for the padding, -0.5 of the pixel centre) */
        int X = (int)(sx0 * 65536.0f), DX = (int)(dsx * 65536.0f);
        float base = 0.5f * SH + 0.5f;
        uint32_t *o = &out[(size_t)(st.y0 + oy) * (size_t)w + (size_t)st.x0];
        for (int ox = 0; ox < pw; ox++, X += DX) {
            int Y = (int)((vc * ysc[ox] + base) * 65536.0f);
            int dl = X - 32768, dr = (SW * 65536 + 32768) - X, du = Y - 32768, dd = (SH * 65536 + 32768) - Y; /* the distances to the four edges, 16.16 */
            int dx_ = dl < dr ? dl : dr, dy_ = du < dd ? du : dd;
            if (dx_ < 0 || dy_ < 0) {
                o[ox] = 0;
                continue;
            }
            int edge = 256;
            if (dx_ < edge_w16 || dy_ < edge_h16) { /* only the last few pixels at the border are faded */
                int ex = (int)(((int64_t)dx_ * mul_x) >> 24), ey = (int)(((int64_t)dy_ * mul_y) >> 24);
                edge = ex < ey ? ex : ey;
                edge = edge > 256 ? 256 : edge;
            }
            int px = X >> 16, py = Y >> 16, wx = sharpen[(X >> 8) & 255], iwx = 256 - wx, d0 = (Y >> 8) & 255, w0 = st.beam[d0], w1 = st.beam[256 - d0];
            const uint16_t *a = &linp[((size_t)py * PW_ + (size_t)px) * 3], *cc = a + PW_ * 3;
            const uint16_t *mk = &st.col_mask[ox * 3];
            int r = ((((a[0] * iwx + a[3] * wx) >> 8) * w0 + ((cc[0] * iwx + cc[3] * wx) >> 8) * w1) >> 8) * mk[0] >> 8;
            int g = ((((a[1] * iwx + a[4] * wx) >> 8) * w0 + ((cc[1] * iwx + cc[4] * wx) >> 8) * w1) >> 8) * mk[1] >> 8;
            int b = ((((a[2] * iwx + a[5] * wx) >> 8) * w0 + ((cc[2] * iwx + cc[5] * wx) >> 8) * w1) >> 8) * mk[2] >> 8;
            if (edge < 256) {
                r = r * edge >> 8;
                g = g * edge >> 8;
                b = b * edge >> 8;
            }
            r = r > LIN_MAX ? LIN_MAX : r;
            g = g > LIN_MAX ? LIN_MAX : g;
            b = b > LIN_MAX ? LIN_MAX : b;
            o[ox] = ((uint32_t)lut_out[r] << 16) | ((uint32_t)lut_out[g] << 8) | lut_out[b];
        }
    }
}

static void render_curved(uint32_t *out, int w, const CrtParams *p) {
    int pw = st.pw, ph = st.ph;
    for (int y = 0; y < PH_; y++) { /* the padded copy */
        int sy = clampi(y - 1, 0, SH - 1);
        for (int x = 0; x < PW_; x++) {
            int sx = clampi(x - 1, 0, SW - 1);
            memcpy(&linp[((size_t)y * PW_ + (size_t)x) * 3], &lin[((size_t)sy * SW + (size_t)sx) * 3], 3 * sizeof(uint16_t));
        }
    }
    for (int i = 0; i < 256; i++) {
        float t = ((float)i + 0.5f) / 256.0f, k = (t - 0.5f) * st.sharp + 0.5f;
        sharpen[i] = (uint16_t)((k < 0 ? 0 : k > 1 ? 1 : k) * 256.0f + 0.5f);
    }
    float c = (float)p->curve / 100.0f, warp_x = 0.10f * c, warp_y = 0.13f * c;
    float *ucol = (float *)malloc((size_t)pw * sizeof(float)), *ysc = (float *)malloc((size_t)pw * sizeof(float));
    if (!ucol || !ysc) {
        free(ucol);
        free(ysc);
        return;
    }
    for (int ox = 0; ox < pw; ox++) {
        ucol[ox] = ((float)ox + 0.5f) / (float)pw * 2.0f - 1.0f;
        ysc[ox] = (1.0f + ucol[ox] * ucol[ox] * warp_y) * 0.5f * SH; /* the row term of a column, in game rows */
    }
    float scale_x = (float)pw / SW, scale_y = (float)ph / SH;
    /* the soft edge: about six window pixels, in game pixels; edge = distance * mul (16.16 distances give 0..256) */
    int64_t mul_x = (int64_t)((1 << 24) / (6.0f / scale_x * 256.0f)), mul_y = (int64_t)((1 << 24) / (6.0f / scale_y * 256.0f));
    cv.ucol = ucol;
    cv.ysc = ysc;
    cv.warp_x = warp_x;
    cv.mul_x = mul_x;
    cv.mul_y = mul_y;
    cv.edge_w16 = (int)(6.0f / scale_x * 65536.0f) + 1; /* where the fade ends, in 16.16 game pixels */
    cv.edge_h16 = (int)(6.0f / scale_y * 65536.0f) + 1;
    job_out = out;
    job_w = w;
    run_rows(curved_rows, ph);
    free(ucol);
    free(ysc);
}

void crt_render(uint32_t *out, int w, int h, const uint32_t *src, const CrtParams *p) {
    init_luts();
    if (!st.valid || st.w != w || st.h != h || memcmp(&st.p, p, sizeof *p) != 0) {
        rebuild(w, h, p);
    }
    if (!st.valid) {
        return;
    }
    if (st.pw < w || st.ph < h) { /* the bars */
        memset(out, 0, (size_t)w * (size_t)h * sizeof *out);
    }
    for (int i = 0; i < SH * SW; i++) {
        uint32_t c = src[i];
        lin[i * 3] = lut_in[(c >> 16) & 255];
        lin[i * 3 + 1] = lut_in[(c >> 8) & 255];
        lin[i * 3 + 2] = lut_in[c & 255];
    }
    if (!p->plain) {
        add_glow();
    }
    if (p->curve > 0 && !p->plain) {
        render_curved(out, w, p);
    } else {
        render_flat(out, w);
    }
}
