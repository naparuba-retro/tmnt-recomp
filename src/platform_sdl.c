/* SDL2 platform layer (Linux): window, keyboard and sound. Build: python3 build.py (needs libsdl2-dev). */
#define _POSIX_C_SOURCE 200809L /* readlink, strcasecmp, opendir with -std=c11 */
#include <SDL.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include "platform.h"

#define SCALE 3

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture, *hd_texture;
static int hd_w, hd_h, hd_mode, click_pending;
static int key_bind[8][2]; /* the keys of the pad buttons: two per button (an SDL scancode, 0 = none) */
static int captured_key;
static unsigned char tap_latch[8]; /* per pad bit: frames left of a key press that was too short to be seen (key down and up in the same pump of events) */
static SDL_AudioDeviceID audio_dev;
static PlatformEvents pending;
static uint64_t last_tick;

int platform_init(const char *title) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_W * SCALE, SCREEN_H * SCALE, SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }
    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, SCREEN_W, SCREEN_H);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        return 1;
    }
    for (int bit = 0; bit < 8; bit++) {
        key_bind[bit][0] = platform_default_key(bit, 0);
        key_bind[bit][1] = platform_default_key(bit, 1);
    }
    last_tick = SDL_GetPerformanceCounter();
    return 0;
}

void platform_shutdown(void) {
    if (audio_dev) {
        SDL_CloseAudioDevice(audio_dev);
    }
    if (texture) {
        SDL_DestroyTexture(texture);
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
}

int platform_audio_open(void) {
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0); /* no callback: frames are queued with SDL_QueueAudio */
    if (!audio_dev) {
        fprintf(stderr, "SDL_OpenAudioDevice: %s (no sound)\n", SDL_GetError());
        return 0;
    }
    int16_t silence[SAMPLES_PER_FRAME] = {0};
    for (int i = 0; i < 2; i++) {
        SDL_QueueAudio(audio_dev, silence, sizeof silence);
    }
    SDL_PauseAudioDevice(audio_dev, 0);
    return 1;
}

static SDL_GameController *get_controller(void);

/* Escape opens the menu, except when it comes with the pad's Start button: on a Steam Deck in desktop mode, Steam Input turns Start into an Escape key press
 * as well as the controller button. The Escape is held for a short while to see whether the button comes with it (in either order). */
#define ESCAPE_HOLD_MS 100
#define START_PAIR_MS 300

/* The keys of the pad buttons: two per button (an SDL scancode, 0 = none). */
int platform_default_key(int bit, int slot) { /* the labels Z and W: B is at the Z position of a QWERTY keyboard and of an AZERTY one */
    switch (bit) {
        case 7:
            return slot ? 0 : (int)SDL_GetScancodeFromKey(SDLK_x);
        case 6:
            return (int)SDL_GetScancodeFromKey(slot ? SDLK_w : SDLK_z);
        case 5:
            return slot ? SDL_SCANCODE_RSHIFT : SDL_SCANCODE_TAB;
        case 4:
            return slot ? 0 : SDL_SCANCODE_RETURN;
        case 3:
            return slot ? 0 : SDL_SCANCODE_UP;
        case 2:
            return slot ? 0 : SDL_SCANCODE_DOWN;
        case 1:
            return slot ? 0 : SDL_SCANCODE_LEFT;
        default:
            return slot ? 0 : SDL_SCANCODE_RIGHT;
    }
}

void platform_set_key(int bit, int slot, int code) {
    key_bind[bit & 7][slot & 1] = code;
}

const char *platform_key_name(int code) {
    static char name[64];
    const char *n = SDL_GetScancodeName((SDL_Scancode)code);
    size_t len = 0;
    int ok = n && n[0];
    for (; ok && n[len] && len + 1 < sizeof name; len++) {
        unsigned char c = (unsigned char)n[len];
        ok &= c >= 32 && c < 127;
        name[len] = (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
    }
    name[len] = 0;
    if (!ok || !len) {
        snprintf(name, sizeof name, "KEY %d", code);
    }
    return name;
}

int platform_capture_key(void) {
    int k = captured_key;
    captured_key = 0;
    return k;
}

void platform_poll(PlatformEvents *ev) {
    static int l2_was, escape_waiting, start_seen;
    static Uint32 escape_tick, start_tick;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            pending.quit = 1;
        } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            click_pending = 1;
        } else if (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
            start_seen = 1;
            start_tick = SDL_GetTicks();
            pending.pad_start = 1;
        } else if (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) {
            pending.menu = 1; /* Back / View = Esc */
        } else if (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
            pending.tab_prev = 1;
        } else if (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
            pending.tab_next = 1;
        } else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            pending.any_key = 1;
            {
                int code = (int)e.key.keysym.scancode, bit = -1; /* the pad bit of the key, to keep a very short tap for three frames */
                for (int b = 0; b < 8 && bit < 0; b++) {
                    if (code && (key_bind[b][0] == code || key_bind[b][1] == code)) {
                        bit = b;
                    }
                }
                if (e.key.keysym.scancode != SDL_SCANCODE_ESCAPE) {
                    captured_key = code;
                }
                if (bit >= 0) {
                    tap_latch[bit] = 3;
                    continue; /* a key that the pad uses is not a shortcut */
                }
            }
            switch (e.key.keysym.sym) {
                case SDLK_ESCAPE:
                    escape_waiting = 1;
                    escape_tick = SDL_GetTicks();
                    break;
                case SDLK_m:
                    pending.mute = 1;
                    break;
                case SDLK_p:
                    pending.pause = 1;
                    break;
                case SDLK_i:
                    pending.invincible = 1;
                    break;
                case SDLK_h:
                    pending.hitbox = 1;
                    break;
                case SDLK_j:
                    pending.fix = 1;
                    break;
                case SDLK_e:
                    pending.water = 1;
                    break;
                case SDLK_c:
                    pending.camera = 1;
                    break;
                case SDLK_s:
                    pending.spawn = 1;
                    break;
                case SDLK_r:
                    pending.balance = 1;
                    break;
                case SDLK_f:
                    pending.flicker = 1;
                    break;
                case SDLK_l:
                    pending.large = 1;
                    break;
                case SDLK_F11:
                    pending.fullscreen = 1;
                    break;
                case SDLK_F5:
                    pending.restart = 1;
                    break;
                case SDLK_F9:
                    pending.rewind = 1;
                    break;
                case SDLK_F10:
                    pending.capture = 1;
                    break;
                default:
                    break;
            }
        }
    }
    if (escape_waiting && SDL_GetTicks() - escape_tick >= ESCAPE_HOLD_MS) {
        escape_waiting = 0;
        Uint32 apart = start_tick > escape_tick ? start_tick - escape_tick : escape_tick - start_tick;
        if (!start_seen || apart > START_PAIR_MS) {
            pending.menu = 1;
        }
    }
    SDL_GameController *c = get_controller();
    int l2 = c && SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000;
    if (l2 && !l2_was) {
        pending.rewind = 1; /* L2 = F9 */
    }
    l2_was = l2;
    *ev = pending;
    memset(&pending, 0, sizeof pending);
}

/* B = Z or W (W is the key at the QWERTY Z position on an AZERTY keyboard), A = X. Each letter is accepted both by its label (SDL_GetScancodeFromKey)
 * and by its QWERTY position, so that any keyboard layout, local or through an X server, works. */
/* The first game controller found (looked for again whenever there is none). */
static SDL_GameController *controller;

static SDL_GameController *get_controller(void) {
    if (controller && !SDL_GameControllerGetAttached(controller)) {
        SDL_GameControllerClose(controller);
        controller = NULL;
    }
    for (int i = 0; !controller && i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
        }
    }
    return controller;
}

/* d-pad or left stick, A = jump (NES A), X or B = attack (NES B), Start, Back = Select (L2 = rewind, see platform_poll). */
static uint8_t controller_pad(void) {
    if (!get_controller()) {
        return 0;
    }
    uint8_t p = 0;
    int lx = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX), ly = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
#define B(b) SDL_GameControllerGetButton(controller, b)
    if (B(SDL_CONTROLLER_BUTTON_A)) {
        p |= 0x80;
    }
    if (B(SDL_CONTROLLER_BUTTON_X) || B(SDL_CONTROLLER_BUTTON_B)) {
        p |= 0x40;
    }
    if (B(SDL_CONTROLLER_BUTTON_Y)) { /* Y = Select: the Back button opens the menu */
        p |= 0x20;
    }
    if (B(SDL_CONTROLLER_BUTTON_START)) {
        p |= 0x10;
    }
    if (B(SDL_CONTROLLER_BUTTON_DPAD_UP) || ly < -16000) {
        p |= 0x08;
    }
    if (B(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || ly > 16000) {
        p |= 0x04;
    }
    if (B(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || lx < -16000) {
        p |= 0x02;
    }
    if (B(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || lx > 16000) {
        p |= 0x01;
    }
#undef B
    return p;
}

uint8_t platform_pad(void) {
    const uint8_t *k = SDL_GetKeyboardState(NULL);
    uint8_t p = 0;
    for (int bit = 0; bit < 8; bit++) {
        if ((key_bind[bit][0] && k[key_bind[bit][0]]) || (key_bind[bit][1] && k[key_bind[bit][1]])) {
            p |= (uint8_t)(1 << bit);
        }
    }
    for (int bit = 0; bit < 8; bit++) { /* the taps that were too short to be seen last for three frames */
        if (tap_latch[bit]) {
            p |= (uint8_t)(1 << bit);
            tap_latch[bit]--;
        }
    }
    p |= controller_pad();
    if ((p & 0x0C) == 0x0C) {
        p &= 0xF3; /* no simultaneous up+down */
    }
    if ((p & 0x03) == 0x03) {
        p &= 0xFC;
    }
    return p;
}

void platform_present_hd(const uint32_t *argb, int w, int h) {
    if (!hd_texture || hd_w != w || hd_h != h) {
        if (hd_texture) {
            SDL_DestroyTexture(hd_texture);
        }
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear"); /* the menu may be drawn smaller than the window: smooth stretch */
        hd_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
        hd_w = w;
        hd_h = h;
    }
    if (!hd_texture) {
        return;
    }
    if (!hd_mode) {
        SDL_RenderSetLogicalSize(renderer, 0, 0); /* the picture is the size of the window */
        hd_mode = 1;
    }
    SDL_UpdateTexture(hd_texture, NULL, argb, w * (int)sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, hd_texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void platform_window_size(int *w, int *h) {
    SDL_GetRendererOutputSize(renderer, w, h);
}

void platform_mouse(int *x, int *y, int *click, int *down) {
    int mx, my, ww, wh, ow, oh;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    *down = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    SDL_GetWindowSize(window, &ww, &wh);
    SDL_GetRendererOutputSize(renderer, &ow, &oh);
    *x = *y = -1;
    if (SDL_GetMouseFocus() == window && ww > 0 && wh > 0) { /* window units to pixels (they differ on a high-density screen) */
        *x = mx * ow / ww;
        *y = my * oh / wh;
    }
    *click = click_pending;
    click_pending = 0;
}

void platform_present(const uint32_t *argb) {
    if (hd_mode) {
        SDL_RenderSetLogicalSize(renderer, SCREEN_W, SCREEN_H);
        hd_mode = 0;
    }
    SDL_UpdateTexture(texture, NULL, argb, SCREEN_W * (int)sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void platform_set_integer_scale(int on) {
    SDL_RenderSetIntegerScale(renderer, on ? SDL_TRUE : SDL_FALSE); /* works with the logical size of the game's picture; the menu is 1:1 anyway */
}

void platform_set_fullscreen(int on) {
    SDL_SetWindowFullscreen(window, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void platform_set_title(const char *title) {
    SDL_SetWindowTitle(window, title);
}

void platform_wait_frame(void) {
    if (audio_dev) { /* the pace comes from the sound card: keep about two frames queued */
        while (SDL_GetQueuedAudioSize(audio_dev) > 2 * SAMPLES_PER_FRAME * sizeof(int16_t)) {
            SDL_Delay(1);
        }
        return;
    }
    uint64_t freq = SDL_GetPerformanceFrequency(), t;
    do { /* no sound card: 60 frames per second */
        SDL_Delay(1);
        t = SDL_GetPerformanceCounter();
    } while ((t - last_tick) * 60 < freq);
    last_tick = t;
}

/* TMNT_AUDIOLOG=<file>: why the sound crackles (see platform_win.c): the number of frames of sound still queued when the next one is added, every 5 seconds. */
static void audio_log_frame(void) {
    static FILE *alog;
    static int tried, frames, underruns, min_frames = 99;
    static Uint32 last_ms;
    if (!tried) {
        tried = 1;
        const char *path = getenv("TMNT_AUDIOLOG");
        alog = path ? fopen(path, "w") : NULL;
        last_ms = SDL_GetTicks();
    }
    if (!alog) {
        return;
    }
    int queued = (int)(SDL_GetQueuedAudioSize(audio_dev) / (SAMPLES_PER_FRAME * sizeof(int16_t)));
    frames++;
    underruns += SDL_GetQueuedAudioSize(audio_dev) == 0;
    min_frames = queued < min_frames ? queued : min_frames;
    if (SDL_GetTicks() - last_ms >= 5000) {
        fprintf(alog, "%d frames: fewest frames of sound queued %d, underruns %d\n", frames, min_frames, underruns);
        fflush(alog);
        frames = underruns = 0;
        min_frames = 99;
        last_ms = SDL_GetTicks();
    }
}

void platform_audio_submit(const int16_t *pcm) {
    audio_log_frame();
    SDL_QueueAudio(audio_dev, pcm, SAMPLES_PER_FRAME * sizeof(int16_t));
}

void platform_error_box(const char *text) {
    fprintf(stderr, "%s\n", text);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "TMNT C port", text, NULL);
}

void platform_make_dir(const char *path) {
    mkdir(path, 0755);
}

void platform_exe_dir(char *out, int size) {
    ssize_t n = readlink("/proc/self/exe", out, (size_t)size - 1);
    if (n <= 0) {
        snprintf(out, (size_t)size, ".");
        return;
    }
    out[n] = 0;
    char *slash = strrchr(out, '/');
    if (slash) {
        *slash = 0;
    }
}

void platform_for_each_nes(const char *dir, NesFileCallback cb, void *user) {
    DIR *d = opendir(dir);
    if (!d) {
        return;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t n = strlen(e->d_name);
        if (n > 4 && strcasecmp(e->d_name + n - 4, ".nes") == 0) {
            char path[4096];
            snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
            if (cb(path, user)) {
                break;
            }
        }
    }
    closedir(d);
}
