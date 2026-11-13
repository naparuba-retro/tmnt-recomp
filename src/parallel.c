/* Rows of work cut into slices that run on several threads (the CRT filter and the lighting: a picture of a big window is a few million pixels at 60 frames per second).
 * The threads are made once and wait between two jobs: creating and joining a thread for every job, several times per frame, cost more than the work itself. */
#include "parallel.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#define MAX_WORKERS 8 /* threads that share the rows (the calling one included) */

#ifdef _WIN32
typedef CRITICAL_SECTION Lock;
typedef CONDITION_VARIABLE Cond;
#define LOCK(l) EnterCriticalSection(l)
#define UNLOCK(l) LeaveCriticalSection(l)
#define WAIT(c, l) SleepConditionVariableCS(c, l, INFINITE)
#define WAKE_ALL(c) WakeAllConditionVariable(c)
#define WAKE_ONE(c) WakeConditionVariable(c)
#else
typedef pthread_mutex_t Lock;
typedef pthread_cond_t Cond;
#define LOCK(l) pthread_mutex_lock(l)
#define UNLOCK(l) pthread_mutex_unlock(l)
#define WAIT(c, l) pthread_cond_wait(c, l)
#define WAKE_ALL(c) pthread_cond_broadcast(c)
#define WAKE_ONE(c) pthread_cond_signal(c)
#endif

static struct {
    Lock lock;
    Cond start, finish;
    RowFn fn;
    int rows, n, done;
    unsigned generation;
    int ready, workers;
} pool;

typedef struct {
    int id;
} Worker;

static void run_slice(int id) {
    pool.fn(pool.rows * id / pool.n, pool.rows * (id + 1) / pool.n);
}

static void worker_loop(Worker *w) {
    unsigned seen = 0;
    for (;;) {
        LOCK(&pool.lock);
        while (pool.generation == seen) {
            WAIT(&pool.start, &pool.lock);
        }
        seen = pool.generation;
        UNLOCK(&pool.lock);
        run_slice(w->id);
        LOCK(&pool.lock);
        if (++pool.done == pool.n - 1) {
            WAKE_ONE(&pool.finish);
        }
        UNLOCK(&pool.lock);
    }
}

#ifdef _WIN32
static DWORD WINAPI worker_main(LPVOID arg) {
    worker_loop((Worker *)arg);
    return 0;
}
#else
static void *worker_main(void *arg) {
    worker_loop((Worker *)arg);
    return NULL;
}
#endif

static int cpu_count(void) {
    int n;
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    n = (int)si.dwNumberOfProcessors;
#else
    n = (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
    return n < 1 ? 1 : n > MAX_WORKERS ? MAX_WORKERS : n;
}

/* Starts the threads the first time; returns how many threads share a job (1 = none could be made: everything runs on the calling one). */
static int pool_start(void) {
    static Worker workers[MAX_WORKERS];
    if (pool.ready) {
        return pool.n;
    }
    pool.ready = 1;
    pool.n = 1;
    int want = cpu_count();
    if (want < 2) {
        return 1;
    }
#ifdef _WIN32
    InitializeCriticalSection(&pool.lock);
    InitializeConditionVariable(&pool.start);
    InitializeConditionVariable(&pool.finish);
#else
    pthread_mutex_init(&pool.lock, NULL);
    pthread_cond_init(&pool.start, NULL);
    pthread_cond_init(&pool.finish, NULL);
#endif
    pool.n = want; /* the workers read it when they run their first job */
    int made = 0;
    for (int i = 1; i < want; i++) {
        workers[i].id = i;
#ifdef _WIN32
        HANDLE h = CreateThread(NULL, 0, worker_main, &workers[i], 0, NULL);
        if (!h) {
            break;
        }
        CloseHandle(h);
#else
        pthread_t t;
        if (pthread_create(&t, NULL, worker_main, &workers[i]) != 0) {
            break;
        }
        pthread_detach(t);
#endif
        made++;
    }
    if (made != want - 1) { /* the slices are numbered up to `want`: with fewer threads some would never run, so use none */
        pool.n = 1;
    }
    return pool.n;
}

void parallel_rows(RowFn fn, int rows) {
    if (rows < 32 || pool_start() < 2) {
        fn(0, rows);
        return;
    }
    LOCK(&pool.lock);
    pool.fn = fn;
    pool.rows = rows;
    pool.done = 0;
    pool.generation++;
    WAKE_ALL(&pool.start);
    UNLOCK(&pool.lock);
    run_slice(0);
    LOCK(&pool.lock);
    while (pool.done < pool.n - 1) {
        WAIT(&pool.finish, &pool.lock);
    }
    UNLOCK(&pool.lock);
}
