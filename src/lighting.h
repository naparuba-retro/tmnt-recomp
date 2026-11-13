/* Lighting for the picture of the game (display only, like the CRT filter: the game itself is not touched): the picture is made darker and tinted, lights and glows make
 * pools of brighter picture, the bright parts of the picture glow (bloom), a gentle haze rises from the bottom, and the colours are graded. It follows the structure of the
 * lighting pass of Animal Well as measured by the session that recompiled it: an accumulated light map (the lights, and soft coloured glows that drift), blurred; the picture
 * mixed with a fog colour, then multiplied by "ambient + a fraction of the light map"; a bloom that weighs the picture by its own luminance; a grade per colour with a
 * saturation; a slow noise over it all. The code is written for this project; it works on the 256x240 picture, in place. */
#ifndef LIGHTING_H
#define LIGHTING_H
#include <stdint.h>

typedef struct {
    float x, y;      /* in pixels of the picture */
    float radius;    /* where the light is gone */
    float intensity; /* 1.0 = the picture is shown as it is at the centre */
    float r, g, b;   /* the colour of the light (1, 1, 1 = white) */
    float glow;      /* how much the light is also added to the picture (a lamp that shines, not only lights): 0 = not at all */
} LightSource;

typedef struct {
    float ambient[3];  /* what is left of the picture away from the lights (0 = black, 1 = unchanged), per colour */
    float pools;       /* how much the bright parts of the picture light their surroundings */
    float bloom;       /* how much the bright parts glow */
    float fog[3];      /* the colour of the haze, which rises from the bottom of the picture */
    float fog_amount;  /* 0 to 1: how much of the picture is replaced by the haze at the bottom */
    float gamma[3];    /* the grade: the colours are raised to these powers (1 = no change) */
    float saturation;  /* the saturation is multiplied by this (1 = no change) */
    float noise;       /* the slow shimmer over the picture (0 to 0.1) */
    float glows;       /* the strength of the dark violet glows that drift in the background (0 = none) */
    float solid_light; /* the part of the light that reaches floors, walls and platforms (0 to 1): they are lit less than the background */
    float far_light;   /* how much farther the lights reach over the background than over the solid parts (1 = the same) */
    float waves;       /* the moving bands of light (like light under water) that sweep over the background (0 = none) */
    int particles;     /* the share of the columns (per 100) where water drips from a ceiling or a platform (0 = none) */
    float bloom_hole;  /* the radius around the first light where nothing glows (the character that carries it would be a bright blob) */
    float vignette;    /* 0 = none, 1 = the corners are much darker */
    float flicker;     /* how much the lights waver with time (0 to 0.2) */
    int rows;          /* the rows of the picture that are lit (the status bar at the bottom is left alone) */
    int scroll_x;      /* the horizontal scroll of the level in pixels: the drops of water belong to the world, not to the screen */
    float mist;        /* the drifting mist: its density over the land (0 = none) */
    float mist_water;  /* and the density it adds over the water */
    float mist_speed;  /* how fast it drifts (1 = slowly) */
    int waves_water;   /* the waves of light are only over the water */
    float wave_speed;  /* how fast the waves of light move (1 = the default, 0 counts as 1) */
    int dust;          /* the motes of dust that float in the light (how many, 0 = none) */
} LightParams;

/* Lights the picture in place. `frame` is the frame counter (the flicker, the noise, the glows). `solid8` (may be NULL) tells which squares of 8x8 pixels are solid (floors,
 * walls, platforms): w / 8 columns, `rows` / 8 lines, 1 = solid. The light does not pass through them, and what it lights of them is only what faces it. */
void lighting_apply(uint32_t *pic, int w, int h, const LightSource *lights, int count, const LightParams *p, unsigned frame, const uint8_t *solid8);

#endif
