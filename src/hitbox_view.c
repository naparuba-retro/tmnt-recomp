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
#define COLOUR_HAZARD 0xFF8000   /* map tile that hurts (seaweed...) */
#define COLOUR_DEADLY 0xFF0000   /* map tile that kills in the swimming zone (deadly seaweed) */
#define COLOUR_PROBE 0x00FFFF    /* point of the turtle where the map is read while swimming */
#define COLOUR_SOLID 0x909090    /* outline of the solid scenery: floors, walls, platforms (sober grey) */
#define COLOUR_CLIMB 0xC0C0C0    /* outline of ladders and conveyor belts (a lighter grey) */
#define PLAYFIELD_H 200          /* lines of the picture above the status bar */

typedef struct {
    uint8_t l, r, t, b;
    uint8_t active, tested, touch;
} Box;

#define MAX_PROBES 15

typedef struct {
    uint32_t frame;
    int used;
    Box box[ENTITIES];
    /* the collision map and the camera, for the hazard tiles (seaweed...), and the points the swimming turtle probes the map at */
    uint8_t terrain[256];
    uint8_t cam_x_lo, cam_x_hi, cam_y_lo, cam_y_hi, vertical, swim, top_view, van;
    uint8_t nprobes, probe[MAX_PROBES][2];
} FrameLog;

static uint8_t terrain_class(const FrameLog *f, uint8_t px, uint8_t py);

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
    memcpy(log->terrain, &M(TerrainMap), sizeof log->terrain);
    log->cam_x_lo = M(CameraXLo);
    log->cam_x_hi = M(CameraXHi);
    log->cam_y_lo = M(CameraYLo);
    log->cam_y_hi = M(CameraYHi);
    log->vertical = (M(VerticalLevelFlag) & 0x80) != 0;
    log->swim = M(SwimZoneFlag) != 0;
    log->top_view = !M(SideViewFlag); /* the streets; the buildings and the dam are seen from the side */
    log->van = M(InVanFlag) != 0;
    /* The turtle is tested against the map at points around it, not with a box. Side view and swimming: points of the table B7 $C83E, the ones of
     * swim_collisions (B4 $A896) in the swimming zone, those of the walking, jumping and climbing code (side_move.c, ladder.c, pose.c...) elsewhere.
     * Top view (overhead.c, B6 $A8D0): one point in the direction of the walk, two on each side for the van. */
    static const uint8_t swim_probe_ids[] = {0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x2C, 0x2D, 0x2E, 0x2F};
    static const uint8_t walk_probe_ids[] = {0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x11, 0x12, 0x13};
    static const int8_t top_probes[][2] = {{7, 3}, {-7, 3}, {0, -3}, {0, 6}};
    static const int8_t van_probes[][2] = {{16, -9}, {16, 9}, {-16, -9}, {-16, 9}, {-9, -16}, {9, -16}, {-9, 14}, {9, 14}};
    if (log->top_view) {
        log->nprobes = (uint8_t)(log->van ? sizeof van_probes / sizeof *van_probes : sizeof top_probes / sizeof *top_probes);
        for (int i = 0; i < log->nprobes; i++) {
            const int8_t *d = log->van ? van_probes[i] : top_probes[i];
            log->probe[i][0] = (uint8_t)(M(EntX) + d[0]);
            log->probe[i][1] = (uint8_t)(M(EntY) + d[1]);
        }
    } else {
        const uint8_t *ids = log->swim ? swim_probe_ids : walk_probe_ids;
        log->nprobes = (uint8_t)(log->swim ? sizeof swim_probe_ids : sizeof walk_probe_ids);
        for (int i = 0; i < log->nprobes; i++) {
            log->probe[i][0] = (uint8_t)(M(EntX) + rom_prg(g->rom, 7, (uint16_t)(0xC83E + ids[i] * 2)));
            log->probe[i][1] = (uint8_t)(M(EntY) + rom_prg(g->rom, 7, (uint16_t)(0xC83F + ids[i] * 2)));
        }
    }
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

/* The class (0-15) of the collision map under the screen point (px, py), as GetTileAt (B7 $C76B) reads it, from a recorded map and camera. */
static uint8_t terrain_class(const FrameLog *f, uint8_t px, uint8_t py) {
    if (f->vertical) {
        unsigned cam = f->cam_x_lo | f->cam_x_hi << 8, xx = px + f->cam_x_lo;
        uint8_t hi = (uint8_t)(f->cam_x_hi + (xx > 0xFF)), ylo = (uint8_t)(py + f->cam_y_lo);
        ylo = (uint8_t)((((hi & 1) << 7) | (ylo >> 1)) & 0xF8);
        uint8_t v = f->terrain[(uint8_t)(((uint8_t)xx >> 5) | ylo)];
        if (xx & 0x10) {
            v >>= 4;
        }
        v &= 0x0F;
        return cam < 0x80 ? (v == 4 ? 4 : 0) : v;
    }
    unsigned value = ((px + f->cam_x_lo) > 0xFF ? 0x100 : 0) | (uint8_t)(px + f->cam_x_lo); /* 9 bits: carry and byte, rotated left by 4 */
    unsigned rot = ((value << 4) | (value >> 5)) & 0x1FF;
    unsigned s1 = py + f->cam_y_lo;
    uint8_t t1 = (uint8_t)s1, t2 = (uint8_t)(f->cam_y_hi + (s1 > 0xFF)), rows = 0;
    while (t2 != 0 || t1 >= 0xF0) { /* one screen is 240 lines */
        t2 = (uint8_t)(t2 - (t1 < 0xF0));
        t1 = (uint8_t)(t1 - 0xF0);
        rows++;
    }
    t1 = (uint8_t)(((rows & 1) << 7) | (t1 >> 1));
    uint8_t v = f->terrain[(t1 & 0xF8) | (rot & 7)];
    return (rot & 0x100 ? v >> 4 : v) & 0x0F;
}

static void tint(uint32_t *pixel, uint32_t colour, int percent) {
    uint32_t out = 0;
    for (int shift = 0; shift <= 16; shift += 8) {
        uint32_t a = (*pixel >> shift) & 255, b = (colour >> shift) & 255;
        out |= ((a * (100 - percent) + b * percent) / 100) << shift;
    }
    *pixel = out;
}

/* What a class of the map is for the scenery drawing: 1 = solid, 2 = a lighter outline (ladder, conveyor belt, door). In the side view the solid
 * class is 4; in the top view the classes that block the walk are 4 and $0C ($06 only in the van, B7 $F20E) and 2 is a door. */
static int scenery_kind(const FrameLog *log, uint8_t c) {
    if (log->top_view) {
        return (c == 4 || c == 0x0C || (c == 6 && log->van)) ? 1 : c == 2 ? 2 : 0;
    }
    return c == 4 ? 1 : (c == 9 || c == 0x0A || c == 0x0C || c == 0x0D) ? 2 : 0;
}

/* The map under every point of the playfield (below it is the status bar, which does not read the map the same way). */
static void draw_terrain(const FrameLog *log, uint32_t *screen) {
    static uint8_t cls[PLAYFIELD_H][SCREEN_W];
    for (int y = 0; y < PLAYFIELD_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            cls[y][x] = terrain_class(log, (uint8_t)x, (uint8_t)y);
        }
    }
    for (int y = 0; y < PLAYFIELD_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            uint8_t c = cls[y][x];
            uint32_t *pixel = &screen[y * SCREEN_W + x];
            if (c == 1) { /* class 1 hurts the turtle (seaweed: state 14, -0x10 of health) */
                tint(pixel, COLOUR_HAZARD, 40);
            } else if (c == 0x0F && log->swim) { /* class F kills it in the swimming zone (deadly seaweed: state 19) */
                tint(pixel, COLOUR_DEADLY, 50);
            }
            int kind = scenery_kind(log, c);
            if (!kind) {
                continue;
            }
            /* the scenery is drawn as the outline of its regions: a point that has a neighbour of another kind */
            int edge = (x > 0 && scenery_kind(log, cls[y][x - 1]) != kind) || (x < SCREEN_W - 1 && scenery_kind(log, cls[y][x + 1]) != kind) ||
                       (y > 0 && scenery_kind(log, cls[y - 1][x]) != kind) || (y < PLAYFIELD_H - 1 && scenery_kind(log, cls[y + 1][x]) != kind);
            if (edge) {
                *pixel = kind == 1 ? COLOUR_SOLID : COLOUR_CLIMB;
            }
        }
    }
}

void hitbox_draw(Game *g, uint32_t *screen) {
    if (!enabled || g->frame < 2) {
        return;
    }
    const FrameLog *log = &ring[(g->frame - 2) % LOG_FRAMES];
    if (!log->used || log->frame != g->frame - 2) {
        return;
    }
    draw_terrain(log, screen);
    {
        for (int i = 0; i < log->nprobes; i++) { /* the points of the turtle where the map is read: a hazard or solid tile under one of them counts */
            for (int d = -1; d <= 1; d++) {
                int x = log->probe[i][0] + d, y = log->probe[i][1];
                if (y < SCREEN_H) {
                    screen[y * SCREEN_W + (x & 255)] = COLOUR_PROBE;
                }
                y = log->probe[i][1] + d;
                if (y >= 0 && y < SCREEN_H) {
                    screen[y * SCREEN_W + log->probe[i][0]] = COLOUR_PROBE;
                }
            }
        }
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
