/* Camera spawn list (B6 $80F6): enemies at the screen edge. */
#include "game.h"
#include "enemy.h"
#include "stage_play.h"
#include "stage_play/camera_split.h"
#include "stage_play/spawn_list.h"
#include "stage_play/spawn_top_view.h"
#include "stage_play/waves.h"
#define TODO_AREA "stage_play" /* prefix of the TODO traces of this file */

static void spawn_scan_left(Game *g);
static void spawn_scan_down(Game *g);

/* --- camera spawn list (B6 $80F6) --- */
static void spawn_list_pointer(Game *g) /* B6 $84FC */
{
    uint16_t p = rom_word(g->rom, 6, 0xAC8D);
    M(Tmp0) = (uint8_t)p;
    M(Tmp0 + 1) = (uint8_t)(p >> 8);
}

static void spawn_entity(Game *g, uint8_t y) /* B6 $8146: creates the enemy described by entry y of the list */
{
    spawn_enemy(g, y);
}

static void save_spawn_pointer(Game *g) /* B6 $845E */
{
    M(SavedSpawnPtrLo) = M(SpawnListPtrLo);
    M(SavedSpawnPtrHi) = M(SpawnListPtrHi);
}

static int same_spawn_pointer(Game *g) /* B6 $8469: carry = same list as before */
{
    return M(SpawnListPtrLo) == M(SavedSpawnPtrLo) && M(SpawnListPtrHi) == M(SavedSpawnPtrHi);
}

/* B6 $84AD: walks through the list for a horizontal move; side = 0 (near edge) or 1 (far edge) */
static void spawn_list_horizontal(Game *g, uint8_t side) {
    M(Tmp0 + 6) = side;
    M(SavedY) = 0xFD;
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        uint8_t y = (uint8_t)(M(SavedY) + 3);
        M(SavedY) = y;
        uint8_t a = rdmem(g, (uint16_t)(ptr(g, SpawnListPtrLo) + y));
        if (a >= 0xFF) {
            return;
        }
        M(0x18) = a;
        uint8_t d = (uint8_t)(M(SpawnLimitX) - a);
        if (d != 0) {
            if (!(M(CameraDeltaX) & 0x80)) {
                if (d >= M(CameraDeltaX)) {
                    continue;
                }
            } else if (d <= M(CameraDeltaX)) {
                continue;
            }
        }
        y = M(SavedY) + 1;
        a = rdmem(g, (uint16_t)(ptr(g, SpawnListPtrLo) + y));
        if (side == 0) {
            if (a < M(SpawnLimitY)) {
                continue;
            }
        } else if (a != M(SpawnLimitY) && a > M(SpawnLimitY)) {
            continue;
        }
        spawn_entity(g, M(SavedY));
    }
}

/* B6 $8570: same for a vertical move */
static void spawn_list_vertical(Game *g, uint8_t side) {
    M(Tmp0 + 6) = side;
    M(SavedY) = 0xFD;
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        uint8_t y = (uint8_t)(M(SavedY) + 3);
        M(SavedY) = y;
        uint8_t k = rdmem(g, (uint16_t)(ptr(g, SpawnListPtrLo) + y));
        if (k == 0xFF) {
            return;
        }
        M(0x18) = rdmem(g, (uint16_t)(ptr(g, SpawnListPtrLo) + y + 1));
        uint8_t d = (uint8_t)(M(SpawnLimitY) - M(0x18));
        if (d != 0) {
            if (!(M(CameraDeltaY) & 0x80)) {
                if (d >= M(CameraDeltaY)) {
                    continue;
                }
            } else if (d <= M(CameraDeltaY)) {
                continue;
            }
        }
        uint8_t a = rdmem(g, (uint16_t)(ptr(g, SpawnListPtrLo) + M(SavedY)));
        if (side == 0) {
            if (a < M(SpawnLimitX)) {
                continue;
            }
        } else if (a != M(SpawnLimitX) && a > M(SpawnLimitX)) {
            continue;
        }
        uint8_t keep = M(SpawnLimitX);
        M(SpawnLimitX) = a;
        spawn_entity(g, M(SavedY));
        M(SpawnLimitX) = keep;
    }
}

static void spawn_edge(Game *g, uint8_t t4, uint8_t t5) /* 84FC + F0A8 for an edge */
{
    spawn_list_pointer(g);
    M(Tmp0 + 4) = t4;
    M(Tmp0 + 5) = t5;
    spawn_position(g);
}

static void spawn_scan_right(Game *g) /* B6 $8429 */
{
    if (M(ScrollEdgeFlag) & 1) {
        spawn_scan_left(g);
        return;
    }
    spawn_edge(g, 0xF8, 0x08);
    save_spawn_pointer(g);
    spawn_list_horizontal(g, 0);
    spawn_edge(g, 0xF8, 0xE8);
    if (same_spawn_pointer(g)) {
        return;
    }
    spawn_list_horizontal(g, 1);
}

static void spawn_scan_left(Game *g) /* B6 $847B */
{
    if (M(ScrollEdgeFlag) & 2) {
        spawn_scan_right(g);
        return;
    }
    spawn_edge(g, 0x08, 0x08);
    save_spawn_pointer(g);
    spawn_list_horizontal(g, 0);
    spawn_edge(g, 0x08, 0xE8);
    if (same_spawn_pointer(g)) {
        return;
    }
    spawn_list_horizontal(g, 1);
}

static void spawn_scan_up(Game *g) /* B6 $8509 */
{
    if (M(ScrollEdgeFlag) & 4) {
        spawn_scan_down(g);
        return;
    }
    spawn_edge(g, 0x08, 0x08);
    save_spawn_pointer(g);
    spawn_list_vertical(g, 0);
    spawn_edge(g, 0xF8, 0x08);
    if (same_spawn_pointer(g)) {
        return;
    }
    spawn_list_vertical(g, 1);
}

static void spawn_scan_down(Game *g) /* B6 $853E */
{
    if (M(ScrollEdgeFlag) & 8) {
        spawn_scan_up(g);
        return;
    }
    spawn_edge(g, 0x08, 0xE8);
    save_spawn_pointer(g);
    spawn_list_vertical(g, 0);
    spawn_edge(g, 0xF8, 0xE8);
    if (same_spawn_pointer(g)) {
        return;
    }
    spawn_list_vertical(g, 1);
}

/* B6 $864A: list of the enemies around the screen edge in side view */
static void side_list_position(Game *g) {
    uint16_t t = rom_word(g->rom, 6, (uint16_t)(0x8681 + M(LevelNumber) * 2)); /* $8672 */
    M(Tmp0) = (uint8_t)t;
    M(Tmp0 + 1) = (uint8_t)(t >> 8);
    uint8_t x = 0xF8;
    if ((M(CameraDeltaX) & 0x80) || (M(ScrollEdgeFlag) & 1)) {
        if (!(M(ScrollEdgeFlag) & 2)) {
            x = 0x07;
        }
    }
    M(Tmp0 + 4) = x;
    M(Tmp0 + 5) = (M(CameraDeltaY) & 0x80) ? 0x17 : 0xB8;
    spawn_position(g);
}

int enemy_present(Game *g) /* B7 $EF08: an enemy of type >= $10 among entities 5 to 15 */
{
    for (int i = 5; i < 16; i++) {
        if (M(EntType + i) >= 0x10) {
            return 1;
        }
    }
    return 0;
}

static void spawn_list_side(Game *g) /* B6 $8100 */
{
    side_list_position(g);
    side_wave_triggers(g);
    if (M(WaveState)) {
        return;
    }
    side_list_position(g);
    M(SavedY) = 0xFD;
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        M(SavedY) = (uint8_t)(M(SavedY) + 3);
        uint8_t x = rdmem(g, ptr(g, SpawnListPtrLo) + M(SavedY));
        if (x == 0xFF) {
            return;
        }
        M(0x18) = x;
        uint8_t d = (uint8_t)(M(SpawnLimitX) - x);
        if (!(M(CameraDeltaX) & 0x80)) {
            if (d < M(CameraDeltaX)) {
                spawn_enemy(g, M(SavedY));
            }
        } else if (d != M(CameraDeltaX) && d > M(CameraDeltaX)) {
            spawn_enemy(g, M(SavedY));
        }
    }
}

void process_spawn_list(Game *g) {
    camera_screen_split(g);
    if (M(SideViewFlag)) {
        spawn_list_side(g);
        return;
    }
    if (M(CameraDeltaY)) { /* B6 $8417: the camera moves, looks for the enemies at the edge */
        if (M(CameraDeltaY) & 0x80) {
            spawn_scan_up(g);
        } else {
            spawn_scan_down(g);
        }
        return;
    }
    if (!M(CameraDeltaX)) {
        return;
    }
    if (M(CameraDeltaX) & 0x80) {
        spawn_scan_left(g);
    } else {
        spawn_scan_right(g);
    }
}

void update_enemies(Game *g) /* B5 $8001 */
{
    enemy_update_all(g);
}
