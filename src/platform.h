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
    int menu;       /* Esc, or the Back / View button of a pad */
    int pad_start;  /* the Start button of a pad was pressed (the menu: play / resume) */
    int tab_prev;   /* LB of a pad (the menu's tabs) */
    int tab_next;   /* RB of a pad */
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
    int fullscreen; /* F11 */
    int restart;    /* F5 */
    int rewind;     /* F9 */
    int capture;    /* F10 */
    int any_key;    /* any key at all (used to leave the help screen) */
} PlatformEvents;

int platform_init(const char *title); /* opens the window; 0 = ok */
void platform_shutdown(void);
int platform_audio_open(void);          /* 1 = sound available */
void platform_poll(PlatformEvents *ev); /* handles the window messages and fills `ev` */
uint8_t platform_pad(
    void); /* NES pad: A=80 B=40 Select=20 Start=10 up=08 down=04 left=02 right=01 (0 when the window is not focused). On a game pad: A = A, X or B = B, Y = Select, Start = Start */
void platform_present(const uint32_t *argb);                  /* shows a 256x240 picture, 0xRRGGBB pixels */
void platform_present_hd(const uint32_t *argb, int w, int h); /* shows a picture the size of the window (the menu), 0xRRGGBB pixels */
void platform_window_size(int *w, int *h);                    /* the area of the window that the picture covers, in pixels */
void platform_mouse(
    int *x, int *y, int *clicked,
    int *down); /* the cursor in that area (-1 if it is elsewhere); clicked: the left button was pressed since the last call; down: it is held now */
/* The keyboard: each pad button (NES bit 7..0) has two keys. A key code is a number of the platform layer (never 0; Esc is not a pad key) that is saved in tmnt.ini
 * as it is, so a file does not carry from Windows to Linux. */
int platform_default_key(int bit, int slot);        /* the key that the button has without any choice of the player (0: none) */
void platform_set_key(int bit, int slot, int code); /* 0 = no key */
const char *platform_key_name(int code);            /* in capitals, for the menu */
int platform_capture_key(void);                     /* the last key pressed since the previous call (0 if none), whatever it does in the game; not Esc */
void platform_set_fullscreen(int on);               /* the window covers the whole screen (no border), or goes back to its place */
void platform_set_integer_scale(int on); /* the game's picture is shown at a whole multiple of 256x240, centred, with black bars (0: it fills the window) */
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
