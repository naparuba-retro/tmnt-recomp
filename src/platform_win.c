/* Windows platform layer: window (GDI), keyboard and sound (waveOut). */
#include <windows.h>
#include <mmsystem.h>
#include <xinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"

#define SCALE 3
#define NBUF 4

static unsigned char tap_latch[8]; /* per pad bit: frames left of a key press that was too short to be seen (key down and up in the same instant) */

/* The pad bit of a key, or -1. */
static int key_pad_bit(WPARAM vk) {
    switch (vk) {
        case 'X':
            return 7;
        case 'Z':
        case 'W':
            return 6;
        case VK_TAB:
        case VK_RSHIFT:
            return 5;
        case VK_RETURN:
            return 4;
        case VK_UP:
            return 3;
        case VK_DOWN:
            return 2;
        case VK_LEFT:
            return 1;
        case VK_RIGHT:
            return 0;
        default:
            return -1;
    }
}
static BITMAPINFO bmi, bmi_hd;
static uint32_t *hd_pixels; /* the menu's picture (the size of the window), when it is shown */
static int hd_w, hd_h, clicked, integer_scale, is_fullscreen;
static WINDOWPLACEMENT saved_place;
static HWND hwnd;
static uint32_t screen[SCREEN_W * SCREEN_H];
static PlatformEvents pending; /* key presses seen by the window procedure since the last platform_poll() */

static HWAVEOUT wo;
static WAVEHDR hdr[NBUF];
static int16_t buf[NBUF][SAMPLES_PER_FRAME];
static int audio, slot;
static LARGE_INTEGER freq, t0;

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_DESTROY:
            pending.quit = 1;
            PostQuitMessage(0);
            return 0;
        case WM_SYSKEYDOWN: /* F10 is a system key on Windows: it does not come as WM_KEYDOWN */
            if (w == VK_F10 && !(l & 0x40000000)) {
                pending.capture = 1;
                return 0;
            }
            return DefWindowProc(h, m, w, l);
        case WM_LBUTTONDOWN:
            clicked = 1;
            return 0;
        case WM_KEYDOWN:
            if (l & 0x40000000) {
                return 0; /* auto-repeat */
            }
            if (key_pad_bit(w) >= 0) {
                tap_latch[key_pad_bit(w)] = 3; /* a very short tap (down and up delivered together) is still three frames of the pad */
            }
            pending.any_key = 1;
            if (w == VK_ESCAPE) {
                pending.menu = 1;
            } else if (w == 'M') {
                pending.mute = 1;
            } else if (w == 'P') {
                pending.pause = 1;
            } else if (w == 'I') {
                pending.invincible = 1;
            } else if (w == 'H') {
                pending.hitbox = 1;
            } else if (w == 'J') {
                pending.fix = 1;
            } else if (w == 'E') {
                pending.water = 1;
            } else if (w == 'C') {
                pending.camera = 1;
            } else if (w == 'S') {
                pending.spawn = 1;
            } else if (w == 'R') {
                pending.balance = 1;
            } else if (w == 'F') {
                pending.flicker = 1;
            } else if (w == 'L') {
                pending.large = 1;
            } else if (w == VK_F11) {
                pending.fullscreen = 1;
            } else if (w == VK_F5) {
                pending.restart = 1;
            } else if (w == VK_F9) {
                pending.rewind = 1;
            } else if (w == VK_F10) {
                pending.capture = 1;
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT r;
            GetClientRect(h, &r);
            if (hd_w) {
                StretchDIBits(dc, 0, 0, r.right, r.bottom, 0, 0, hd_w, hd_h, hd_pixels, &bmi_hd, DIB_RGB_COLORS, SRCCOPY);
            } else if (integer_scale) { /* a whole multiple of the picture, centred, black around */
                int k = r.right / SCREEN_W < r.bottom / SCREEN_H ? r.right / SCREEN_W : r.bottom / SCREEN_H;
                if (k < 1) {
                    k = 1;
                }
                int pw = SCREEN_W * k, ph = SCREEN_H * k, x0 = (r.right - pw) / 2, y0 = (r.bottom - ph) / 2;
                HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
                RECT bar[4] = {
                    {0, 0, r.right, y0 > 0 ? y0 : 0}, {0, y0 + ph, r.right, r.bottom}, {0, 0, x0 > 0 ? x0 : 0, r.bottom}, {x0 + pw, 0, r.right, r.bottom}};
                for (int i = 0; i < 4; i++) {
                    FillRect(dc, &bar[i], black);
                }
                StretchDIBits(dc, x0, y0, pw, ph, 0, 0, SCREEN_W, SCREEN_H, screen, &bmi, DIB_RGB_COLORS, SRCCOPY);
            } else {
                StretchDIBits(dc, 0, 0, r.right, r.bottom, 0, 0, SCREEN_W, SCREEN_H, screen, &bmi, DIB_RGB_COLORS, SRCCOPY);
            }
            EndPaint(h, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProc(h, m, w, l);
}

int platform_init(const char *title) {
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "tmntport";
    RegisterClassA(&wc);
    RECT r = {0, 0, SCREEN_W * SCALE, SCREEN_H * SCALE};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    hwnd = CreateWindowA("tmntport", title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL,
                         wc.hInstance, NULL);
    if (!hwnd) {
        return 1;
    }
    bmi.bmiHeader.biSize = sizeof bmi.bmiHeader;
    bmi.bmiHeader.biWidth = SCREEN_W;
    bmi.bmiHeader.biHeight = -SCREEN_H;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    return 0;
}

void platform_shutdown(void) {
    if (audio) {
        waveOutReset(wo);
        for (int i = 0; i < NBUF; i++) {
            waveOutUnprepareHeader(wo, &hdr[i], sizeof hdr[i]);
        }
        waveOutClose(wo);
        audio = 0;
    }
}

int platform_audio_open(void) {
    WAVEFORMATEX fmt = {WAVE_FORMAT_PCM, 1, SAMPLE_RATE, SAMPLE_RATE * 2, 2, 16, 0};
    audio = waveOutOpen(&wo, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR;
    memset(hdr, 0, sizeof hdr);
    if (audio) {
        for (int i = 0; i < NBUF; i++) {
            hdr[i].lpData = (LPSTR)buf[i];
            hdr[i].dwBufferLength = sizeof buf[i];
            waveOutPrepareHeader(wo, &hdr[i], sizeof hdr[i]);
            waveOutWrite(wo, &hdr[i], sizeof hdr[i]); /* silence at the start: buffer already zeroed */
        }
    }
    return audio;
}

static void pump_messages(void) {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

static int xinput_l2(void); /* defined below */

static unsigned xinput_buttons(void); /* defined below */

void platform_poll(PlatformEvents *ev) {
    static int l2_was;
    static unsigned buttons_was;
    pump_messages();
    int l2 = GetForegroundWindow() == hwnd && xinput_l2();
    if (l2 && !l2_was) {
        pending.rewind = 1; /* L2 = F9 */
    }
    l2_was = l2;
    unsigned b = GetForegroundWindow() == hwnd ? xinput_buttons() : 0, edge = b & ~buttons_was; /* the buttons pressed since the last call */
    buttons_was = b;
    if (edge & XINPUT_GAMEPAD_BACK) {
        pending.menu = 1; /* Back / View = Esc */
    }
    if (edge & XINPUT_GAMEPAD_START) {
        pending.pad_start = 1;
    }
    if (edge & XINPUT_GAMEPAD_LEFT_SHOULDER) {
        pending.tab_prev = 1;
    }
    if (edge & XINPUT_GAMEPAD_RIGHT_SHOULDER) {
        pending.tab_next = 1;
    }
    *ev = pending;
    memset(&pending, 0, sizeof pending);
}

/* The first XInput controller found. Returns 0 when there is none (polling empty slots is slow: a new controller is looked for about once a second). */
static int xinput_state(XINPUT_STATE *st) {
    static int slot = -1, wait;
    if (slot >= 0 && XInputGetState((DWORD)slot, st) != ERROR_SUCCESS) {
        slot = -1;
    }
    if (slot < 0) {
        if (wait-- > 0) {
            return 0;
        }
        wait = 60;
        for (int i = 0; i < 4 && slot < 0; i++) {
            if (XInputGetState((DWORD)i, st) == ERROR_SUCCESS) {
                slot = i;
            }
        }
    }
    return slot >= 0;
}

static unsigned xinput_buttons(void) {
    XINPUT_STATE st;
    return xinput_state(&st) ? st.Gamepad.wButtons : 0;
}

/* Left trigger pressed (L2 = rewind). */
static int xinput_l2(void) {
    XINPUT_STATE st;
    return xinput_state(&st) && st.Gamepad.bLeftTrigger > 128;
}

/* d-pad or left stick, A = jump (NES A), X or B = attack (NES B), Start, Y = Select (Back opens the menu, LB and RB are the menu's tabs, L2 = rewind: see platform_poll). */
static uint8_t xinput_pad(void) {
    XINPUT_STATE st;
    if (!xinput_state(&st)) {
        return 0;
    }
    unsigned b = st.Gamepad.wButtons;
    int lx = st.Gamepad.sThumbLX, ly = st.Gamepad.sThumbLY;
    uint8_t p = 0;
    if (b & XINPUT_GAMEPAD_A) {
        p |= 0x80;
    }
    if (b & (XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_B)) {
        p |= 0x40;
    }
    if (b & XINPUT_GAMEPAD_Y) { /* Y = Select (the weapon, the van): the Back button opens the menu */
        p |= 0x20;
    }
    if (b & XINPUT_GAMEPAD_START) {
        p |= 0x10;
    }
    if ((b & XINPUT_GAMEPAD_DPAD_UP) || ly > 16000) {
        p |= 0x08;
    }
    if ((b & XINPUT_GAMEPAD_DPAD_DOWN) || ly < -16000) {
        p |= 0x04;
    }
    if ((b & XINPUT_GAMEPAD_DPAD_LEFT) || lx < -16000) {
        p |= 0x02;
    }
    if ((b & XINPUT_GAMEPAD_DPAD_RIGHT) || lx > 16000) {
        p |= 0x01;
    }
    return p;
}

uint8_t platform_pad(void) {
    if (GetForegroundWindow() != hwnd) {
        return 0;
    }
#define K(vk) (GetAsyncKeyState(vk) & 0x8000)
    uint8_t p = 0;
    if (K('X')) {
        p |= 0x80;
    }
    if (K('Z') || K('W')) { /* W: the key at the QWERTY Z position on an AZERTY keyboard */
        p |= 0x40;
    }
    if (K(VK_TAB) || K(VK_RSHIFT)) {
        p |= 0x20;
    }
    if (K(VK_RETURN)) {
        p |= 0x10;
    }
    if (K(VK_UP)) {
        p |= 0x08;
    }
    if (K(VK_DOWN)) {
        p |= 0x04;
    }
    if (K(VK_LEFT)) {
        p |= 0x02;
    }
    if (K(VK_RIGHT)) {
        p |= 0x01;
    }
    for (int bit = 0; bit < 8; bit++) { /* the taps that were too short to be seen last for three frames */
        if (tap_latch[bit]) {
            p |= (uint8_t)(1 << bit);
            tap_latch[bit]--;
        }
    }
    p |= xinput_pad();
    if ((p & 0x0C) == 0x0C) {
        p &= 0xF3; /* no simultaneous up+down */
    }
    if ((p & 0x03) == 0x03) {
        p &= 0xFC;
    }
    return p;
}

void platform_present_hd(const uint32_t *argb, int width, int height) {
    if (!hd_pixels || hd_w != width || hd_h != height) {
        free(hd_pixels);
        hd_pixels = (uint32_t *)malloc((size_t)width * (size_t)height * sizeof(uint32_t));
        if (!hd_pixels) {
            hd_w = 0;
            return;
        }
        hd_w = width;
        hd_h = height;
        bmi_hd = bmi;
        bmi_hd.bmiHeader.biWidth = width;
        bmi_hd.bmiHeader.biHeight = -height;
    }
    memcpy(hd_pixels, argb, (size_t)width * (size_t)height * sizeof(uint32_t));
    InvalidateRect(hwnd, NULL, FALSE);
    UpdateWindow(hwnd);
}

void platform_window_size(int *width, int *height) {
    RECT r;
    GetClientRect(hwnd, &r);
    *width = r.right > 0 ? r.right : 1;
    *height = r.bottom > 0 ? r.bottom : 1;
}

void platform_mouse(int *x, int *y, int *click, int *down) {
    POINT p;
    RECT r;
    *x = *y = -1;
    if (GetForegroundWindow() == hwnd && GetCursorPos(&p) && ScreenToClient(hwnd, &p) && GetClientRect(hwnd, &r) && p.x >= 0 && p.y >= 0 && p.x < r.right &&
        p.y < r.bottom) {
        *x = p.x;
        *y = p.y;
    }
    *click = clicked;
    clicked = 0;
    *down = GetForegroundWindow() == hwnd && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
}

void platform_present(const uint32_t *argb) {
    hd_w = 0; /* the game's picture again */
    memcpy(screen, argb, sizeof screen);
    InvalidateRect(hwnd, NULL, FALSE);
    UpdateWindow(hwnd);
}

void platform_set_integer_scale(int on) {
    if (integer_scale != on) {
        integer_scale = on;
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void platform_set_fullscreen(int on) {
    if (on == is_fullscreen) {
        return;
    }
    if (on) { /* borderless window over the monitor the window is on */
        MONITORINFO mi = {sizeof mi};
        saved_place.length = sizeof saved_place;
        GetWindowPlacement(hwnd, &saved_place);
        if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
            SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
            is_fullscreen = 1;
        }
    } else {
        SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPlacement(hwnd, &saved_place);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        is_fullscreen = 0;
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

void platform_set_title(const char *title) {
    SetWindowTextA(hwnd, title);
}

void platform_wait_frame(void) {
    if (audio) { /* the pace comes from the sound card */
        while (!(hdr[slot].dwFlags & WHDR_DONE)) {
            Sleep(1);
            pump_messages();
            if (pending.quit) {
                break;
            }
        }
    } else { /* no sound card: 60 frames per second */
        LARGE_INTEGER t;
        do {
            QueryPerformanceCounter(&t);
            Sleep(1);
        } while ((t.QuadPart - t0.QuadPart) * 60 < freq.QuadPart);
        t0 = t;
    }
}

void platform_audio_submit(const int16_t *pcm) {
    memcpy(buf[slot], pcm, sizeof buf[slot]);
    waveOutWrite(wo, &hdr[slot], sizeof hdr[slot]);
    slot = (slot + 1) % NBUF;
}

void platform_error_box(const char *text) {
    MessageBoxA(NULL, text, "TMNT C port", MB_ICONERROR);
}

void platform_make_dir(const char *path) {
    CreateDirectoryA(path, NULL);
}

void platform_exe_dir(char *out, int size) {
    GetModuleFileNameA(NULL, out, (DWORD)size);
    char *slash = strrchr(out, '\\');
    if (slash) {
        *slash = 0;
    }
}

void platform_for_each_nes(const char *dir, NesFileCallback cb, void *user) {
    char pattern[MAX_PATH * 2];
    snprintf(pattern, sizeof pattern, "%s\\*.nes", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        char path[MAX_PATH * 2];
        snprintf(path, sizeof path, "%s\\%s", dir, fd.cFileName);
        if (cb(path, user)) {
            break;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}
