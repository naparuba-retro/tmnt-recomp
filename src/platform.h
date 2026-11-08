/* What the player front-end (frontend.c) needs from the operating system: a window, a pad, sound, a clock and a folder listing.
 * Two implementations: platform_win.c (Windows: GDI + waveOut) and platform_sdl.c (SDL2, for Linux). */
#ifndef PLATFORM_H
#define PLATFORM_H
#include <stdint.h>

#ifdef _WIN32
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

#define SCREEN_W 256
#define SCREEN_H 240
#define SAMPLE_RATE 44100
#define SAMPLES_PER_FRAME 735 /* 44100 Hz / 60 frames per second */

/* Key presses since the previous platform_poll() (one event per key press, no auto-repeat). */
typedef struct {
    int quit;       /* window closed */
    int menu;       /* Esc */
    int mute;       /* M */
    int pause;      /* P */
    int invincible; /* I */
    int hitbox;     /* H */
    int fix;        /* J */
    int water;      /* E */
    int camera;     /* C */
    int spawn;      /* S */
    int balance;    /* R */
    int flicker;    /* F */
    int large;      /* L */
    int restart;    /* F5 */
    int rewind;     /* F9 */
    int capture;    /* F10 */
    int any_key;    /* any key at all (used to leave the help screen) */
} PlatformEvents;

int platform_init(const char *title); /* opens the window; 0 = ok */
void platform_shutdown(void);
int platform_audio_open(void);               /* 1 = sound available */
void platform_poll(PlatformEvents *ev);      /* handles the window messages and fills `ev` */
uint8_t platform_pad(void);                  /* NES pad: A=80 B=40 Select=20 Start=10 up=08 down=04 left=02 right=01 (0 when the window is not focused) */
void platform_present(const uint32_t *argb); /* shows a 256x240 picture, 0xRRGGBB pixels */
void platform_present_hd(const uint32_t *argb, int w, int h); /* shows a picture the size of the window (the menu), 0xRRGGBB pixels */
void platform_window_size(int *w, int *h);                    /* the area of the window that the picture covers, in pixels */
void platform_mouse(int *x, int *y,
                    int *clicked); /* the cursor in that area (-1 if it is elsewhere); clicked: the left button was pressed since the last call */
void platform_set_title(const char *title);
void platform_wait_frame(void);                 /* paces the game at 60 frames per second (by the sound card when there is one) */
void platform_audio_submit(const int16_t *pcm); /* SAMPLES_PER_FRAME mono 16-bit samples for the next frame */
void platform_error_box(const char *text);      /* message shown to the user */
void platform_make_dir(const char *path);       /* creates a folder if it does not exist */
void platform_exe_dir(char *out, int size);     /* folder of the executable */

/* Calls `cb` for every *.nes file of `dir` (any letter case) until it returns non-zero. */
typedef int (*NesFileCallback)(const char *path, void *user);
void platform_for_each_nes(const char *dir, NesFileCallback cb, void *user);

#endif
