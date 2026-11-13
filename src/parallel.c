/* Rows of work cut into slices that run on several threads (the CRT filter and the lighting: a picture of a big window is a few million pixels at 60 frames per second). */
#include "parallel.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#define MAX_WORKERS 8 /* threads that share the rows */

/* Rows [0, rows) are cut into slices that run on several threads (the picture of a big window is a few million pixels at 60 frames per second). */
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

void parallel_rows(RowFn fn, int rows) {
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
