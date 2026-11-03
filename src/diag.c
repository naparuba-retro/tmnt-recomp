/* Diagnostic helpers for the command-line tools : hang watchdog (Windows only) and RAM access watch (build with /DWATCHZP=<address>). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "game.h"
#include "diag.h"

#ifdef _WIN32
#include <windows.h>

static HANDLE watched_thread;

static DWORD WINAPI watchdog(LPVOID arg) {
    Sleep((DWORD)(intptr_t)arg * 1000);
    SuspendThread(watched_thread);
    CONTEXT c;
    c.ContextFlags = CONTEXT_CONTROL;
    GetThreadContext(watched_thread, &c);
    uintptr_t base = (uintptr_t)GetModuleHandle(NULL);
    fprintf(stderr, "WATCHDOG rip=%p base=%p\n", (void *)c.Rip, (void *)base);
    uintptr_t *sp = (uintptr_t *)c.Rsp; /* probable return addresses : stack values that fall inside the module */
    for (int i = 0; i < 400; i++) {
        if (sp[i] > base && sp[i] < base + 0x100000) {
            fprintf(stderr, " ret %zx", sp[i] - base);
        }
    }
    fprintf(stderr, "\n");
    fflush(stderr);
    TerminateProcess(GetCurrentProcess(), 3); /* not ExitProcess (noreturn) : keeps the compiler happy about the return below */
    return 0;
}

void diag_start_watchdog(int seconds) {
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &watched_thread, 0, FALSE, DUPLICATE_SAME_ACCESS);
    CreateThread(NULL, 0, watchdog, (LPVOID)(intptr_t)seconds, 0, NULL);
}
#else
void diag_start_watchdog(int seconds) {
    (void)seconds;
}
#endif

#ifdef WATCHZP
/* debug : lists the lines of the port that touch address WATCHZP (build with /DWATCHZP=14) ; WATCHFROM=<frame> logs every access from that frame on */
uint8_t *dbg_ptr(Game *g, unsigned a, const char *f, int l) {
    static const char *seen_f[256];
    static int seen_l[256], n;
    if (a == WATCHZP && getenv("WATCHFROM") && g->frame >= (unsigned)atoi(getenv("WATCHFROM"))) {
        static unsigned lf;
        static int ll;
        static const char *lff;
        if (lf != g->frame || ll != l || lff != f) {
            lf = g->frame;
            ll = l;
            lff = f;
            fprintf(stderr, "ACCESS %s:%d f=%u\n", f, l, g->frame);
        }
    }
    if (a == WATCHZP) {
        int i;
        for (i = 0; i < n; i++) {
            if (seen_l[i] == l && seen_f[i] == f) {
                break;
            }
        }
        if (i == n && n < 256) {
            seen_f[n] = f;
            seen_l[n++] = l;
            fprintf(stderr, "WATCH %s:%d f=%u\n", f, l, g->frame);
        }
    }
    return &g->ram[a];
}
#endif
