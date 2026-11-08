/* The high-resolution user interface layer: a software renderer on an ARGB surface the size of the window (menu.c uses it for the menu).
 * Shapes are convex polygons with soft edges, thick outlines, halftone dots (the comic-book look), hard shadows, text from TrueType fonts
 * (stb_truetype) and PNG images (stb_image). Nothing here touches the game. Colours are 0xRRGGBB. */
#ifndef UI_H
#define UI_H
#include <stdint.h>

typedef struct {
    uint32_t *px; /* w * h pixels, 0xRRGGBB (the top byte is not used) */
    int w, h;
} Ui;

typedef struct {
    float x[4], y[4]; /* a convex quadrilateral, clockwise on the screen (y down) */
} UiQuad;

typedef struct UiFont UiFont;
typedef struct UiImage UiImage;

void ui_resize(Ui *ui, int w, int h); /* (re)allocates the pixels */
void ui_free(Ui *ui);

/* Shapes. `alpha` is 0..255. */
UiQuad ui_quad_rect(float x, float y, float w, float h);
UiQuad ui_quad_skew(float x, float y, float w, float h, float skew); /* a parallelogram: the top edge is shifted right by `skew` */
UiQuad ui_quad_offset(UiQuad q, float dx, float dy);
void ui_fill_rect(Ui *ui, int x, int y, int w, int h, uint32_t colour, int alpha);
void ui_fill_quad(Ui *ui, const UiQuad *q, uint32_t colour, int alpha);
void ui_stroke_quad(Ui *ui, const UiQuad *q, float thickness, uint32_t colour); /* centred on the edges, mitred corners */
/* Halftone dots inside the quad: the dot size goes from 0 to full along the vertical (t = 0 at the top, 1 at the bottom) between `t0` and `t1`. */
void ui_halftone_quad(Ui *ui, const UiQuad *q, uint32_t colour, float cell, float t0, float t1, int alpha);
void ui_fill_burst(Ui *ui, float cx, float cy, float r_out, float r_in, int points, float rot, uint32_t colour); /* the pointed star of a comic-book "POW!" */
void ui_fill_triangle(Ui *ui, float x0, float y0, float x1, float y1, float x2, float y2, uint32_t colour);
int ui_quad_contains(const UiQuad *q, float x, float y);
/* The pixels inside the quad lose their colour (they become the grey of their brightness). */
void ui_desaturate_quad(Ui *ui, const UiQuad *q);

/* Fonts: TrueType files read once. `size` is the height of the em in pixels. */
UiFont *ui_font_load(const char *path); /* NULL if the file cannot be read */
float ui_text_width(UiFont *f, float size, const char *utf8);
void ui_text(Ui *ui, UiFont *f, float size, float x, float baseline, uint32_t colour, const char *utf8);
void ui_text_outlined(Ui *ui, UiFont *f, float size, float x, float baseline, uint32_t colour, uint32_t outline, float thickness, const char *utf8);
/* Wrapped on spaces inside `max_w`; returns the number of lines (the y of the next line is baseline + n * line_h). */
int ui_text_wrapped(Ui *ui, UiFont *f, float size, float x, float baseline, float max_w, float line_h, uint32_t colour, const char *utf8);
int ui_text_lines(UiFont *f, float size, float max_w, const char *utf8); /* how many lines it takes */

/* Images: PNG (or anything stb_image reads) with alpha. */
UiImage *ui_image_load(const char *path);
UiImage *ui_image_from_rgba(const unsigned char *rgba, int w, int h); /* a copy of w * h pixels, 4 bytes each (R G B A) */
int ui_image_w(const UiImage *im);
int ui_image_h(const UiImage *im);
void ui_image_draw(Ui *ui, const UiImage *im, float x, float y, float w, float h, int alpha, int smooth);

/* The game's 256x240 picture (0xRRGGBB) scaled into a rectangle, darkened by `dim` (0..255). */
void ui_picture(Ui *ui, const uint32_t *argb, int sw, int sh, int x, int y, int w, int h, int dim);

#endif
