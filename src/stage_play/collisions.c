/* Entity hit boxes and collisions (B6 $9535). */
#include "game.h"
#include "enemy.h"
#include "stage_play.h"
#include "stage_play/collisions.h"
#include "hitbox_view.h"
#include "fixes.h"
#define TODO_AREA "stage_play" /* prefix of the TODO traces of this file */

/* --- collisions (B6 $9535) --- */
static void entity_hitbox_raw(Game *g, int x, uint8_t yi) /* B6 $9B11 GetEntityHitboxX */
{
    uint8_t id = M(EntAnimId + x), a;
    if (M(EntType + x) < 0x80) {
        a = (id & 0x80) ? R6(0x8835 + id) : R6(0x8935 + id);
    } else {
        a = (id & 0x80) ? R6(0x895B + id) : R6(0x89B5 + id);
    }
    uint8_t i2 = (uint8_t)(a << 1);
    M(Tmp0 + 2) = yi;
    M(Tmp0) = R6(0x8A43 + i2);
    M(Tmp0 + 1) = R6(0x8A44 + i2);
    M(Tmp8 + yi) = (uint8_t)(M(EntX + x) - M(Tmp0));
    M(Tmp9 + yi) = (uint8_t)(M(EntX + x) + M(Tmp0));
    if (x < 5) {
        uint8_t ey = M(EntY + x);
        if (M(EntStatus) & 2) {
            ey = (uint8_t)(ey + 8);
        }
        M(TmpA + yi) = (uint8_t)(ey - M(Tmp0 + 1));
        M(TmpB + yi) = (uint8_t)(ey + M(Tmp0 + 1));
        return;
    }
    M(TmpA + yi) = (uint8_t)(M(EntY + x) - M(Tmp0 + 1));                                            /* $9B90: enemies */
    if (fix_enabled(FIX_WATER_SPINNER) && (M(EntAnimId + x) == 0x57 || M(EntAnimId + x) == 0x58)) { /* PIZZA-003: the rotating bar, whose Y is its centre */
        uint8_t half = (uint8_t)(M(Tmp0) - 1);                                        /* a square a little smaller than the sprite, centred on it */
        uint8_t centre = (uint8_t)(M(EntY + x) + (M(EntAnimId + x) == 0x58 ? 4 : 0)); /* the disc is drawn in the lower half of its 8x16 sprite */
        M(TmpA + yi) = (uint8_t)(centre - half);
        M(TmpB + yi) = (uint8_t)(centre + half);
        return;
    }
    for (uint8_t i = 0;; i = (uint8_t)(i + 2)) {
        uint8_t lo = R6(0x9BC6 + i);
        if (lo == 0xFF) {
            M(TmpB + yi) = (uint8_t)(M(EntY + x) - 2);
            return;
        }
        uint8_t t = M(EntType + x);
        if (t >= lo && t < R6(0x9BC7 + i)) {
            M(TmpB + yi) = (uint8_t)(M(EntY + x) + M(Tmp0 + 1));
            return;
        }
    }
}

static void entity_hitbox(Game *g, int x, uint8_t yi) {
    entity_hitbox_raw(g, x, yi);
    hitbox_note_tested(g, x, yi); /* hit box display (key H): only reads the box that was just computed */
}

static int entity_in_hit_list(Game *g, int x) /* B6 $9617: carry = the target is in the table */
{
    for (uint8_t y = 0;; y = (uint8_t)(y + 2)) {
        uint8_t t = R6(0x963F + y);
        if (t == 0xFF) {
            return 0;
        }
        if (t != M(EntType + x)) {
            continue;
        }
        uint8_t a = R6(0x9640 + y);
        if (a == 0xFF || a == M(EntAnimId + x)) {
            return 1;
        }
    }
}

static int hitboxes_overlap_raw(Game *g, int x, int y) /* B6 $9AD3 */
{
    if (M(EntType + x) == 0) {
        return 0;
    }
    M(SavedY) = (uint8_t)y;
    entity_hitbox(g, x, 8);
    if (M(EntX + x) >= M(EntX + y)) {
        if (M(Tmp10) >= M(Tmp9)) {
            return 0;
        }
    } else if (M(Tmp11) < M(Tmp8)) {
        return 0;
    }
    if (M(EntY + x) >= M(EntY + y)) {
        return M(Tmp12) < M(TmpB);
    }
    return !(M(Tmp13) < M(TmpA));
}

static int hitboxes_overlap(Game *g, int x, int y) {
    int touching = hitboxes_overlap_raw(g, x, y);
    hitbox_note_pair(g, x, y, touching); /* hit box display (key H) */
    return touching;
}

void check_entity_collisions(Game *g) {
    hitbox_begin(g); /* hit box display (key H): reads the entities before anything is tested */
    if (M(WaveState) != 3) {
        M(0xD2) = 0;
    }
    M(0x86) = 0;
    if (M(0xD2) | M(SceneId)) {
        return;
    }
    if ((M(EntFlags) & 2) && !(M(EntStatus) & 8) && (M(EntAnimId) | M(EntSprite))) {
        entity_hitbox(g, 0, 0);
        M(0x03E6) = 0;
        for (int x = 5; x < 16; x++) {
            if (entity_in_hit_list(g, x)) {
                continue;
            }
            if (!(M(EntFlags + x) & 2)) {
                continue;
            }
            if (M(0x03ED)) {
                continue;
            }
            if (M(0xD2) | M(SceneId)) {
                goto second;
            }
            if (hitboxes_overlap(g, x, 0)) {
                M(0x03E6) = 1;
                player_hit_by_enemy(g, x);
            }
        }
        if (!M(0x03E6)) {
            M(0x8D) = 0;
            M(0x03E5) = 0;
        }
    }
second:
    if (M(SwimZoneFlag)) {
        return;
    }
    for (int y = (M(EntStatus) & 0x10) ? 0 : 1; y < 5; y++) {
        if (!M(EntAnimId + y)) {
            continue;
        }
        entity_hitbox(g, y, 0);
        for (int x = 5; x < 16; x++) { /* $95C9 */
            if (!M(EntAnimId + y)) {
                break;
            }
            if (!(M(EntFlags + x) & 2) || (M(EntFlags + x) & 4)) {
                continue;
            }
            if (entity_in_hit_list(g, x)) {
                continue;
            }
            if (!hitboxes_overlap(g, x, y)) {
                continue;
            }
            uint8_t t = M(EntType + x);
            if (t < 0x10) {
                continue;
            }
            if (t == 0x2B) {
                if (M(EntType + y) != 0x2A) {
                    continue;
                }
            } else if (t == 0x75 && M(EntAnimId + x) == 0x10) {
                continue;
            }
            enemy_hit_by_weapon(g, x, y);
        }
    }
}
