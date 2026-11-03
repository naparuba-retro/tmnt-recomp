/* Hit box display, see hitbox_view.h. */
#include <string.h>
#include "game.h"
#include "hitbox_view.h"
#include "platform.h"

#define LOG_FRAMES 4
#define ENTITIES 16     /* 0 = player body, 1-4 = weapons, 5-15 = enemies and their shots */
#define COLLISION_GAP 2 /* the boxes are shown while the collision routine ran at most this many frames ago (it runs every other frame) */

#define COLOUR_PLAYER 0x00FF00
#define COLOUR_WEAPON 0xFFFF00
#define COLOUR_ENEMY 0xFF3030
#define COLOUR_UNTESTED 0x707070 /* active entity that the game did not test (excluded type, invulnerable player...) */
#define COLOUR_TOUCH 0xFFFFFF    /* box of a pair that touches */
#define COLOUR_WRAPPED 0xFF00FF  /* the 8-bit box wraps around (left > right or top > bottom): the game compares it in an odd way */

typedef struct {
    uint8_t l, r, t, b;
    uint8_t active, tested, touch;
} Box;

typedef struct {
    uint32_t frame;
    int used;
    Box box[ENTITIES];
} FrameLog;

static FrameLog ring[LOG_FRAMES];
static uint8_t tested[ENTITIES], touch[ENTITIES]; /* what the last collision frame did; kept for the frames in between */
static uint32_t last_collision_frame;
static int have_collision, enabled;
static unsigned mismatches, checked;

void hitbox_set_enabled(int on) {
    enabled = on;
    hitbox_clear();
}

int hitbox_enabled(void) {
    return enabled;
}

void hitbox_clear(void) {
    memset(ring, 0, sizeof ring);
    memset(tested, 0, sizeof tested);
    memset(touch, 0, sizeof touch);
    have_collision = 0;
}

unsigned hitbox_mismatches(void) {
    return mismatches;
}

unsigned hitbox_checked(void) {
    return checked;
}

/* The box of entity x as GetEntityHitboxX (B6 $9B11) computes it, without writing anything. rect = left, right, top, bottom. */
static void entity_rect(Game *g, int x, uint8_t rect[4]) {
    uint8_t id = M(EntAnimId + x), a;
    if (M(EntType + x) < 0x80) {
        a = (id & 0x80) ? R6(0x8835 + id) : R6(0x8935 + id);
    } else {
        a = (id & 0x80) ? R6(0x895B + id) : R6(0x89B5 + id);
    }
    uint8_t i2 = (uint8_t)(a << 1), w = R6(0x8A43 + i2), h = R6(0x8A44 + i2);
    rect[0] = (uint8_t)(M(EntX + x) - w);
    rect[1] = (uint8_t)(M(EntX + x) + w);
    if (x < 5) {
        uint8_t ey = M(EntY + x);
        if (M(EntStatus) & 2) {
            ey = (uint8_t)(ey + 8);
        }
        rect[2] = (uint8_t)(ey - h);
        rect[3] = (uint8_t)(ey + h);
        return;
    }
    rect[2] = (uint8_t)(M(EntY + x) - h);
    for (uint8_t i = 0;; i = (uint8_t)(i + 2)) {
        uint8_t lo = R6(0x9BC6 + i);
        if (lo == 0xFF) {
            rect[3] = (uint8_t)(M(EntY + x) - 2);
            return;
        }
        uint8_t t = M(EntType + x);
        if (t >= lo && t < R6(0x9BC7 + i)) {
            rect[3] = (uint8_t)(M(EntY + x) + h);
            return;
        }
    }
}

static int entity_active(Game *g, int x) {
    if (x == 0) {
        return (M(EntFlags) & 2) && (M(EntAnimId) | M(EntSprite));
    }
    if (x < 5) {
        return M(EntAnimId + x) != 0;
    }
    return M(EntType + x) != 0 && (M(EntFlags + x) & 2);
}

void hitbox_begin(Game *g) {
    if (!enabled) {
        return;
    }
    memset(tested, 0, sizeof tested);
    memset(touch, 0, sizeof touch);
    last_collision_frame = g->frame;
    have_collision = 1;
}

void hitbox_note_tested(Game *g, int x, int yi) {
    if (!enabled) {
        return;
    }
    uint8_t rect[4], seen[4] = {M(Tmp8 + yi), M(Tmp9 + yi), M(TmpA + yi), M(TmpB + yi)};
    entity_rect(g, x, rect);
    checked++;
    if (memcmp(rect, seen, sizeof rect)) {
        mismatches++;
    }
    tested[x] = 1;
}

void hitbox_note_pair(Game *g, int x, int y, int touching) {
    (void)g;
    if (!enabled || !touching) {
        return;
    }
    touch[x] = 1;
    touch[y] = 1;
}

void hitbox_end_frame(Game *g) {
    if (!enabled || !have_collision || g->frame - last_collision_frame > COLLISION_GAP) {
        return;
    }
    FrameLog *log = &ring[g->frame % LOG_FRAMES];
    memset(log, 0, sizeof *log);
    log->used = 1;
    log->frame = g->frame;
    for (int x = 0; x < ENTITIES; x++) {
        if (entity_active(g, x)) {
            uint8_t rect[4];
            entity_rect(g, x, rect);
            Box *bx = &log->box[x];
            bx->l = rect[0];
            bx->r = rect[1];
            bx->t = rect[2];
            bx->b = rect[3];
            bx->active = 1;
            bx->tested = tested[x];
            bx->touch = touch[x];
        }
    }
}

/* a run of pixels from x0 to x1 included, going forward and wrapping at 256 (a wrapped box is drawn in its two pieces) */
static void hline(uint32_t *s, int x0, int x1, int y, uint32_t c) {
    if (y < 0 || y >= SCREEN_H) {
        return;
    }
    int n = ((x1 - x0) & 255) + 1;
    for (int i = 0; i < n; i++) {
        s[y * SCREEN_W + ((x0 + i) & 255)] = c;
    }
}

static void vline(uint32_t *s, int x, int y0, int y1, uint32_t c) {
    int n = ((y1 - y0) & 255) + 1;
    for (int i = 0; i < n; i++) {
        int y = (y0 + i) & 255;
        if (y < SCREEN_H) {
            s[y * SCREEN_W + (x & 255)] = c;
        }
    }
}

static void outline(uint32_t *s, int l, int r, int t, int b, uint32_t c) {
    hline(s, l, r, t, c);
    hline(s, l, r, b, c);
    vline(s, l, t, b, c);
    vline(s, r, t, b, c);
}

void hitbox_draw(Game *g, uint32_t *screen) {
    if (!enabled || g->frame < 2) {
        return;
    }
    const FrameLog *log = &ring[(g->frame - 2) % LOG_FRAMES];
    if (!log->used || log->frame != g->frame - 2) {
        return;
    }
    for (int x = 0; x < ENTITIES; x++) {
        const Box *bx = &log->box[x];
        if (!bx->active) {
            continue;
        }
        uint32_t c = x == 0 ? COLOUR_PLAYER : x < 5 ? COLOUR_WEAPON : COLOUR_ENEMY;
        int wrapped = bx->l > bx->r || bx->t > bx->b;
        if (!bx->tested) {
            c = COLOUR_UNTESTED;
        }
        if (wrapped) {
            c = COLOUR_WRAPPED;
        }
        if (bx->touch) {
            c = COLOUR_TOUCH;
        }
        outline(screen, bx->l, bx->r, bx->t, bx->b, c);
        if (bx->touch && !wrapped && bx->r - bx->l > 2 && bx->b - bx->t > 2) {
            outline(screen, bx->l + 1, bx->r - 1, bx->t + 1, bx->b - 1, c); /* a touching pair is drawn thicker */
        }
    }
}
