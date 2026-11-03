/* Spawn generator of the top view (B4 $AB94): enemy list of the zone, the van. */
#include "game.h"
#include "enemy.h"
#include "stage_play.h"
#include "stage_play/spawn_top_view.h"
#define TODO_AREA "stage_play" /* prefix of the TODO traces of this file */

/* --- spawn generator (B4 $AB94): walks through the enemy list of the zone --- */
void spawn_table_pointer(Game *g) /* B4 $ACBE */
{
    uint16_t p = rom_word(g->rom, 4, (uint16_t)(0xACCD + (uint8_t)(M(TmpA) << 1)));
    M(Tmp0) = (uint8_t)p;
    M(Tmp0 + 1) = (uint8_t)(p >> 8);
}

void spawn_position(Game *g) /* B7 $F0A8: column ($7F) and list ($42) around the camera */
{
    uint8_t y = (uint8_t)(M(AreaNumber) << 1);
    M(Tmp8) = rdmem(g, (uint16_t)(ptr(g, Tmp0) + y));
    M(Tmp9) = rdmem(g, (uint16_t)(ptr(g, Tmp0) + y + 1));
    uint8_t lvl = M(SideViewFlag) ? M(LevelNumber) : 6;
    uint16_t t = rom_word(g->rom, 7, (uint16_t)(0xF13F + (uint8_t)(lvl << 1)));
    M(Tmp10) = R7(t + M(AreaNumber));
    unsigned s = M(CameraXLo) + M(Tmp0 + 4);
    M(SpawnLimitX) = (uint8_t)s;
    M(Tmp0) = (uint8_t)(M(CameraXHi) + (s > 0xFF));
    uint8_t a;
    if (!M(SideViewFlag)) {
        unsigned s2 = M(CameraYInScreen) + M(Tmp0 + 5);
        M(SpawnLimitY) = (uint8_t)s2;
        a = (uint8_t)(M(CameraYScreen) + (s2 > 0xFF));
        if (a == 0) {
            a = M(Tmp0);
        } else { /* $F11D : a * $10 + Tmp0 */
            uint8_t acc = 0;
            for (uint8_t n = a; n; n--) {
                acc = (uint8_t)(acc + M(Tmp10));
            }
            a = (uint8_t)(acc + M(Tmp0));
        }
    } else { /* $F0F4 */
        unsigned s2 = M(CameraYInScreen) + M(Tmp0 + 5);
        uint8_t v = (uint8_t)s2;
        if (s2 > 0xFF) {
            v = (uint8_t)(v + 0x40);
            a = (uint8_t)(M(CameraYScreen) + 1);
        } else if (v >= 0xC0) {
            v = (uint8_t)(v - 0xC0);
            a = (uint8_t)(M(CameraYScreen) + 1);
        } else {
            a = M(CameraYScreen);
        }
        M(SpawnLimitY) = v;
        if (a == 0) {
            a = M(Tmp0);
        } else {
            uint8_t acc = 0;
            for (uint8_t n = a; n; n--) {
                acc = (uint8_t)(acc + M(Tmp10));
            }
            a = (uint8_t)(acc + M(Tmp0));
        }
    }
    M(SpawnColumn) = a;
    uint8_t y2 = (uint8_t)(a << 1);
    M(SpawnListPtrLo) = rdmem(g, (uint16_t)(ptr(g, 0x08) + y2));
    M(SpawnListPtrHi) = rdmem(g, (uint16_t)(ptr(g, 0x08) + y2 + 1));
}

void level3_van_spawn(Game *g) /* B7 $CFB4 -> B4 $9712: the van (entity 5) and its spawn column */
{
    int x = 5;
    uint8_t saved_bank = g->prg_bank;
    g->prg_bank = 4; /* $CFB4 : SetPrgBank(4) */
    M(0x0593) = M(EntAnimId + x);
    M(EntType + x) = 0x10; /* $96F3 */
    M(EntAnimFrame + x) = 0;
    M(EntAnimTimer + x) = 8;
    M(EntHealth + x) = 0xFF;
    M(EntFlags + x) = (uint8_t)((M(EntFlags) & 1) | 0xCA);
    M(TmpA) = 6;
    spawn_table_pointer(g);
    M(Tmp4) = M(EntX + x);
    M(Tmp5) = M(EntY + x);
    spawn_position(g);
    M(0x0592) = M(SpawnColumn);
    M(SpawnPosX) = M(SpawnLimitX);
    M(SpawnPosY) = M(SpawnLimitY);
    g->prg_bank = saved_bank;
}

/* B4 $AB12: the van (entity 5) wakes up when the camera point reaches its column and its position */
static void van_wake_check(Game *g) {
    if (M(0x0592) != M(SpawnColumn)) {
        return;
    }
    if (M(Tmp6)) {
        goto l37;
    }
    if (M(CameraXLo) < 0x15) {
        goto l27;
    }
    if (M(CameraXLo) < 0xF0) {
        goto l2e;
    }
l27:
    if (M(SpawnPosX) >= 0xF7) {
        goto l37;
    }
l2e:
    if (M(SpawnPosX) >= M(SpawnLimitX)) {
        goto l53;
    }
    return;
l37:
    if (M(CameraXLo) < 0x15) {
        goto l41;
    }
    if (M(CameraXLo) < 0xF0) {
        goto l48;
    }
l41:
    if (M(SpawnPosX) < 0x0B) {
        goto l2e;
    }
l48:
    if (M(SpawnPosX) <= M(SpawnLimitX)) {
        goto l53;
    }
    return;
l53:
    if (M(Tmp7)) {
        goto l71;
    }
    if (M(CameraYLo) < 0x1F) {
        goto l61;
    }
    if (M(CameraYLo) < 0xF0) {
        goto l68;
    }
l61:
    if (M(SpawnPosY) >= 0xF7) {
        goto l82;
    }
l68:
    if (M(SpawnPosY) < M(SpawnLimitY)) {
        return;
    }
    goto l89;
l71:
    if (M(CameraYLo) < 0x1F) {
        goto l7b;
    }
    if (M(CameraYLo) < 0xF0) {
        goto l82;
    }
l7b:
    if (M(SpawnPosY) < 0x10) {
        goto l68;
    }
l82:
    if (M(SpawnPosY) >= M(SpawnLimitY)) {
        return;
    }
l89:
    M(EntFlags + 5) |= 2;
}

void ai_van(Game *g, int x) /* B4 $AA9A: the van is parked; visible as long as it is on the screen */
{
    uint8_t saved = g->prg_bank;
    g->prg_bank = 4; /* $CFD5 */
    if (M(EntFlags + x) & 2) {
        clamp_entity_position(g, x); /* $CF45 -> B5 $8077: Tmp0 = 1 off the screen */
        if (M(Tmp0)) {
            M(EntFlags + x) &= 0xFD;
        }
        g->prg_bank = saved;
        return;
    }
    static const uint8_t probe[4][4] = {{0x0A, 0xF6, 0, 1}, {0xF6, 0xF6, 1, 1}, {0x0A, 0x0F, 0, 0}, {0xF6, 0x0F, 1, 0}};
    for (int i = 0; i < 4; i++) {
        M(TmpA) = 6;
        spawn_table_pointer(g);
        M(Tmp4) = probe[i][0];
        M(Tmp5) = probe[i][1];
        spawn_position(g);
        M(Tmp6) = probe[i][2];
        M(Tmp7) = probe[i][3];
        van_wake_check(g);
    }
    g->prg_bank = saved;
}

static void scan_spawn_list(Game *g) /* B4 $AC05: objects (bonuses) of the zone close to the camera */
{
    spawn_table_pointer(g);
    uint8_t y = (uint8_t)(M(AreaNumber) << 1);
    M(SpawnListPtrLo) = rdmem(g, (uint16_t)(ptr(g, Tmp0) + y));
    M(SpawnListPtrHi) = rdmem(g, (uint16_t)(ptr(g, Tmp0) + y + 1));
#define E(i) rdmem(g, (uint16_t)(ptr(g, SpawnListPtrLo) + (i)))
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        uint8_t k = E(0);
        if (k == 0xFF) {
            return;
        }
        if (k == M(SpawnColumn)) {
            int ok;
            if (!M(Tmp0 + 6)) {
                ok = E(2) >= M(SpawnLimitX); /* $AC29 */
            } else {
                ok = E(1) <= M(SpawnLimitX); /* $AC33 */
            }
            if (ok) {
                ok = M(Tmp0 + 7) ? E(3) < M(SpawnLimitY) : E(4) >= M(SpawnLimitY);
            }
            for (int x = 0; ok && x < 8; x++) {
                if (E(6) == M(ObjId + x)) {
                    ok = 0; /* already spawned */
                }
            }
            if (ok) {
                int x = 0;
                while (x < 8 && M(ObjType + x)) {
                    x++;
                }
                if (x == 8) {
                    return;
                }
                M(ObjColumn + x) = E(0);
                M(ObjXMin + x) = E(1);
                M(ObjXMax + x) = E(2);
                M(ObjX + x) = (uint8_t)(E(2) - M(CameraXLo));
                M(ObjYMin + x) = E(3);
                M(ObjY + x) = (uint8_t)(E(3) - M(CameraYInScreen));
                M(ObjYMax + x) = E(4);
                M(ObjType + x) = E(5);
                M(ObjId + x) = E(6);
                M(ObjFlags + x) = E(7);
            }
        }
        unsigned s = M(SpawnListPtrLo) + 8; /* $ACAE: next entry */
        M(SpawnListPtrLo) = (uint8_t)s;
        M(SpawnListPtrHi) = (uint8_t)(M(SpawnListPtrHi) + (s > 0xFF));
    }
#undef E
}

void spawn_enemies_near_camera(Game *g) {
    if (M(VerticalScrollMode)) {
        return;
    }
    g->prg_bank = 4;
    if (M(SideViewFlag)) {
        M(TmpA) = M(LevelNumber);
        goto objects;
    }
    M(TmpA) = 6;
    spawn_table_pointer(g);
    M(Tmp0 + 4) = 0x0A;
    M(Tmp0 + 5) = 0xF6;
    spawn_position(g);
    M(Tmp0 + 6) = 0;
    M(Tmp0 + 7) = 1;
    scan_spawn_list(g);
    spawn_table_pointer(g);
    M(Tmp0 + 4) = 0xF6;
    M(Tmp0 + 5) = 0xF6;
    spawn_position(g);
    M(Tmp0 + 7) = 1;
    M(Tmp0 + 6) = 1;
    scan_spawn_list(g);
objects:
    spawn_table_pointer(g); /* $ABD8 */
    M(Tmp0 + 4) = 0x0A;
    M(Tmp0 + 5) = 0x0F;
    spawn_position(g);
    M(Tmp0 + 7) = 0;
    M(Tmp0 + 6) = 0;
    scan_spawn_list(g);
    spawn_table_pointer(g);
    M(Tmp0 + 4) = 0xF6;
    M(Tmp0 + 5) = 0x0F;
    spawn_position(g);
    M(Tmp0 + 6) = 1;
    M(Tmp0 + 7) = 0;
    scan_spawn_list(g);
}
