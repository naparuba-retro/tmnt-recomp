/* The CRT look, drawn by the processor (no GPU, no shader): the 256x240 picture of the game becomes a picture the size of the window, with scanlines, a phosphor
 * mask, a little glow, and optionally the curved screen of a tube. The ideas are those of any CRT filter (a Gaussian beam per scanline, a mask of RGB triads, a
 * barrel distortion); the code is written for this project. The proportions of the picture are always kept: the rest of the window is black. */
#ifndef CRT_H
#define CRT_H
#include <stdint.h>

typedef struct {
    int scanlines; /* 0 to 100: how dark the gap between two scanlines is */
    int mask;      /* 0 to 100: the strength of the RGB phosphor mask (it fades away when the picture is not much larger than the game's) */
    int curve;     /* 0 to 100: the curvature of the screen (0 = flat: the fast path) */
    int integer;   /* the picture is a whole multiple of 256x240 (otherwise the largest one that fits) */
    int plain; /* no CRT look at all: only a sharp bilinear stretch (each game pixel stays a clean block with a one pixel soft edge), for a window that is not a whole multiple */
} CrtParams;

/* Draws `src` (256x240, 0xRRGGBB) into `out` (w * h, 0xRRGGBB). */
void crt_render(uint32_t *out, int w, int h, const uint32_t *src, const CrtParams *p);

#endif
