/* Objects (bonuses, scenery, crates, currents): B4 $B554 UpdateObjectSlots. */
#include "game.h"
#include "hal.h"
#include "player.h"
#include "stage_play.h"
#include "stage_play/objects.h"
#include "stage_play/spawn_top_view.h"
#define TODO_AREA "stage_play" /* prefix of the TODO traces of this file */

/* --- objects (bonuses): B4 $B554 UpdateObjectSlots --- */
void clear_object(Game *g, int x) /* B7 $EFE6 */
{
    static const uint16_t arrays[] = {0x0650, 0x0658, 0x0660, 0x0668, 0x0670, 0x0618, 0x0620, 0x0688, 0x0690, 0x0698, 0x06A0, 0x06A8, 0x06B0, 0x06B8};
    for (unsigned i = 0; i < sizeof arrays / sizeof *arrays; i++) {
        M(arrays[i] + x) = 0;
    }
}

static int object_slot_free_entity(Game *g) /* B7 $EEEC: free entity among 7 to 15, -1 if none */
{
    for (int e = 7; e < 16; e++) {
        if (!M(EntType + e)) {
            return e;
        }
    }
    return -1;
}

static int object_to_entity(Game *g, int x, uint8_t type) /* B4 $B98D: object x becomes an entity; returns the entity or -1 */
{
    int e = object_slot_free_entity(g);
    if (e < 0) {
        clear_object(g, x);
        return -1;
    }
    uint16_t o = (uint16_t)(0xB9E5 + type * 5);
    M(EntX + e) = M(ObjX + x);
    M(EntY + e) = M(ObjY + x);
    M(EntType + e) = R4(o);
    M(EntAnimId + e) = R4(o + 1);
    M(EntHealth + e) = R4(o + 2);
    M(EntFlags + e) = R4(o + 3);
    M(EntAnimTimer + e) = R4(o + 4);
    M(EntStatus + e) = 0x9B;
    return e;
}

static void object_generic(Game *g, int x) /* B7 $FE64: ordinary bonus */
{
    int e = object_to_entity(g, x, M(ObjType + x));
    if (e < 0) {
        return;
    }
    uint8_t id = M(EntHealth + e);
    if (M(PickedUpMask) & R7(0xF137 + id)) { /* already picked up in this level: removed */
        clear_entity(g, e);
        clear_object(g, x);
        return;
    }
    M(EntVar0 + e) = 0xFF; /* unlimited lifetime */
    M(ObjState + x) = 0;
    M(ObjCreated + x)++;
}

static void object_palette_cycle(Game *g, int x, int table) /* B7 $FE9B: object that scrolls palettes (lit scenery) */
{
    uint16_t p = rom_word(g->rom, 7, (uint16_t)(0xFED5 + table * 2));
    if ((int8_t)(--M(ObjScriptTimer + x)) >= 0) {
        return;
    }
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        uint8_t y = M(ObjScriptPos + x), a = R7(p + y);
        if (a == 0xFF) {
            M(ObjScriptPos + x) = 0;
            continue;
        }
        M(ObjScriptTimer + x) = a;
        M(ObjScriptPos + x) = (uint8_t)(y + 2);
        load_palette_preset(g, R7(p + y + 1));
        return;
    }
}

static void object_current(Game *g, int x, uint8_t zone) /* B4 $B813: current zone as long as the player is between the bounds of the object */
{
    if (M(0x8B) && M(0x8B) != M(ObjId + x)) {
        return;
    }
    if ((uint8_t)(M(ObjXMin + x) - M(CameraXLo)) >= M(EntX) || (uint8_t)(M(ObjXMax + x) - M(CameraXLo)) < M(EntX)) {
        M(WaterCurrent) = M(0x8B) = 0;
        return;
    }
    M(0x8B) = M(ObjId + x);
    M(WaterCurrent) = zone;
}

static void object_rotating_bar(Game *g, int x) /* B4 $B715: rotating bar, entities 5 and following */
{
    uint8_t px = M(ObjX + x), py = M(ObjY + x);
    for (int e = 5; e < 16; e++) {
        clear_entity(g, e);
    }
    int e = 5;
    for (uint16_t y = 0xB770; R4(y) != 0xFF; y += 3, e++) {
        M(EntX + e) = (uint8_t)(R4(y) + px);
        M(EntY + e) = (uint8_t)(R4(y + 1) + py);
        M(EntType + e) = R4(y + 2);
        M(EntHealth + e) = 0xFF;
        M(EntFlags + e) = 0xC9;
        if (e == 5) {
            M(EntAnimTimer + e) = 8;
        }
        M(EntAnimId + e) = e == 5 ? 0x57 : 0x58;
    }
    M(ObjState + x) = 1;
    M(ObjCreated + x)++;
}

static void object_push_zone(Game *g, int x, uint8_t zone, int right) /* B4 $BAE9/$BAED (right) / $BB0C/$BB10 (left): zone that pushes the player ($88) */
{
    if (M(0x8A) && M(0x8A) != M(ObjId + x)) {
        return;
    }
    uint8_t ox = M(ObjX + x), px = M(EntX);
    int inside;
    if (!right) {
        inside = !(ox < px) && (uint8_t)(ox - 0x2C) < px;
    } else {
        inside = !(ox >= px) && (uint8_t)(ox + 0x2C) >= px;
    }
    if (inside) {
        uint8_t oy = M(ObjY + x), py = M(EntY);
        inside = !(oy >= py) && (uint8_t)(oy + 0x40) >= py;
    }
    if (!inside) {
        M(PushZone) = 0;
        M(0x8A) = 0;
        return;
    }
    M(PushZone) = zone;
    M(0x8A) = M(ObjId + x);
}

static void object_push_creator(Game *g, int x, uint8_t state, int flag) /* B4 $B87C-$B898: the conveyor creates an entity $0A offset by $34 */
{
    M(ObjState + x) = state;
    M(ObjCreated + x)++; /* $B986 */
    int e = object_to_entity(g, x, 0x26);
    uint8_t y = (uint8_t)(e >= 0 ? e : x); /* Y returned by $B98D */
    M(EntY + y) = (uint8_t)(M(EntY + y) + 0x34);
    if (flag) {
        M(EntFlags + y) |= 1; /* $B889 */
    }
}

static void object_crate(Game *g, int x, uint8_t etype) /* B4 $B8BE-$B8EC: crate (entity $A2) that the blow destroys, unless already destroyed ($8C) */
{
    uint8_t id = (uint8_t)(M(ObjId + x) - 1);
    if (R7(0xF137 + id) & M(ColumnsDestroyedMask)) {
        clear_object(g, x);
        return;
    }
    int e = object_to_entity(g, x, etype);
    uint8_t y = (uint8_t)(e >= 0 ? e : x);
    M(EntHealth + y) = id;
    M(EntVar1 + y) = (uint8_t)x;
    M(ObjEntity + x) = y;
    M(ObjState + x) = 7;
    M(ObjCreated + x)++;
}

static void object_crate_state(Game *g, int x) /* B4 $B6F4: the crate makes the tile above it solid ($44) */
{
    uint8_t y = M(ObjEntity + x);
    M(Tmp0) = M(EntX + y);
    M(Tmp1) = M(EntY + y);
    tile_at_xy(g, M(EntX + y), M(EntY + y), y);
    if (M(EntSprite + y) & 1) {
        return;
    }
    M(0x06F8 + M(0x0598)) = 0x44;
}

/* Wrappers of the object routes that need arguments (one per ROM address: the address of the original routine is the identity of the case) */
#define OBJ_ROUTE2(name, fn, a)                                                                                                                                \
    static void name(Game *g, int x) {                                                                                                                         \
        fn(g, x, a);                                                                                                                                           \
    }
#define OBJ_ROUTE3(name, fn, a, b)                                                                                                                             \
    static void name(Game *g, int x) {                                                                                                                         \
        fn(g, x, a, b);                                                                                                                                        \
    }

static void object_spawn_entity(Game *g, int x, uint8_t entity_type) /* entity type in A, then $B986: state 0, object created */
{
    object_to_entity(g, x, entity_type);
    M(ObjState + x) = 0;
    M(ObjCreated + x)++;
}

static void object_state_bomb_palette(Game *g, int x) /* state 6: the defused bomb changes the palette */
{
    uint8_t k = (uint8_t)(M(ObjType + x) - 0x17);
    if (M(BombsDefusedMask) & R7(0xF137 + k)) {
        load_palette_preset(g, 0x0B);
    }
}
static void object_state_bar(Game *g, int x) /* state 1: the bar disappears when flag $06B8 bit 1 is zero */
{
    if (!(M(ObjFlags + x) & 2)) {
        clear_object(g, x);
        for (int e = 5; e < 15; e++) {
            clear_entity(g, e);
        }
    }
}
OBJ_ROUTE3(object_state_push_40, object_push_zone, 0x40, 0)
OBJ_ROUTE3(object_state_push_41, object_push_zone, 0x41, 0)
OBJ_ROUTE3(object_state_push_80, object_push_zone, 0x80, 1)
OBJ_ROUTE3(object_state_push_81, object_push_zone, 0x81, 1)

/* state table of the objects already created ($B5FE) */
static const Route object_state_routes[] = {
    {0xBAE8, route_nop},
    {0xBB51, object_state_bomb_palette},
    {0xBAD0, object_state_bar},
    {0xB6F4, object_crate_state},
    {0xBAE9, object_state_push_40},
    {0xBAED, object_state_push_41},
    {0xBB0C, object_state_push_80},
    {0xBB10, object_state_push_81},
};

OBJ_ROUTE2(object_palette_0, object_palette_cycle, 0)
OBJ_ROUTE2(object_palette_1, object_palette_cycle, 1)
OBJ_ROUTE2(object_palette_2, object_palette_cycle, 2)
OBJ_ROUTE2(object_crate_29, object_crate, 0x29)
OBJ_ROUTE2(object_crate_2a, object_crate, 0x2A)
OBJ_ROUTE2(object_crate_2b, object_crate, 0x2B)
OBJ_ROUTE2(object_crate_2c, object_crate, 0x2C)
OBJ_ROUTE3(object_creator_2, object_push_creator, 2, 1)
OBJ_ROUTE3(object_creator_3, object_push_creator, 3, 1)
OBJ_ROUTE3(object_creator_4, object_push_creator, 4, 0)
OBJ_ROUTE3(object_creator_5, object_push_creator, 5, 0)
OBJ_ROUTE2(object_current_10, object_current, 0x10)
OBJ_ROUTE2(object_current_11, object_current, 0x11)
OBJ_ROUTE2(object_current_20, object_current, 0x20)
OBJ_ROUTE2(object_current_21, object_current, 0x21)
OBJ_ROUTE2(object_current_40, object_current, 0x40)
OBJ_ROUTE2(object_current_41, object_current, 0x41)
OBJ_ROUTE2(object_current_80, object_current, 0x80)
OBJ_ROUTE2(object_current_81, object_current, 0x81)
OBJ_ROUTE2(object_scenery_10, object_spawn_entity, 0x10) /* B4 $B786-$B7B8: dam scenery */
OBJ_ROUTE2(object_scenery_11, object_spawn_entity, 0x11)
OBJ_ROUTE2(object_scenery_12, object_spawn_entity, 0x12)
OBJ_ROUTE2(object_scenery_13, object_spawn_entity, 0x13)
OBJ_ROUTE2(object_scenery_1e, object_spawn_entity, 0x1E)
OBJ_ROUTE2(object_scenery_1f, object_spawn_entity, 0x1F)
OBJ_ROUTE2(object_scenery_20, object_spawn_entity, 0x20)
OBJ_ROUTE2(object_scenery_21, object_spawn_entity, 0x21)
OBJ_ROUTE2(object_scenery_22, object_spawn_entity, 0x22)
OBJ_ROUTE2(object_scenery_23, object_spawn_entity, 0x23)
OBJ_ROUTE2(object_scenery_24, object_spawn_entity, 0x24)
OBJ_ROUTE2(object_scenery_25, object_spawn_entity, 0x25)

static void object_column(Game *g, int x) /* $B8F5: column ($4D) or its ruins ($4F) */
{
    uint8_t id = (uint8_t)(M(ObjId + x) - 1);
    int whole = !(R7(0xF137 + id) & M(ColumnsDestroyedMask));
    int e = object_to_entity(g, x, whole ? 0x2D : 0x2E);
    uint8_t y = (uint8_t)(e >= 0 ? e : x);
    if (whole) {
        M(EntTimer + y) = id;
    } else {
        M(EntFlags + y) = M(ObjFlags + x);
        M(EntY + y) = (uint8_t)(M(EntY + y) + 7);
        M(EntX + y) = (uint8_t)(M(EntX + y) - 0x0A);
    }
    M(ObjState + x) = 0;
    M(ObjCreated + x)++;
}

static void object_flagged_entity(Game *g, int x, uint8_t entity_type) /* $B8B0: entity $41 / $48, direction of the table, state 0 */
{
    int e = object_to_entity(g, x, entity_type);
    uint8_t y = (uint8_t)(e >= 0 ? e : x);
    M(EntFlags + y) = M(ObjFlags + x);
    M(ObjState + x) = 0;
    M(ObjCreated + x)++;
}
OBJ_ROUTE2(object_flagged_27, object_flagged_entity, 0x27)
OBJ_ROUTE2(object_flagged_28, object_flagged_entity, 0x28)

static void object_bomb(Game *g, int x) /* type 23: bomb */
{
    int e = object_to_entity(g, x, (uint8_t)(M(ObjType + x) - 3));
    uint8_t y = (uint8_t)(e >= 0 ? e : x); /* Y returned by $B98D */
    M(ObjState + x) = 6;
    M(ObjCreated + x)++;
    uint8_t k = (uint8_t)(M(ObjType + x) - 0x17);
    if (M(BombsDefusedMask) & R7(0xF137 + k)) {
        M(EntAnimId + y) = 0x39;
        load_palette_preset(g, 0x0B);
    } else {
        load_palette_preset(g, 0x0A);
    }
}

static void object_dead_turtle(Game *g, int x) /* type 14: a dead turtle can be rescued ($0077-$007A = health) */
{
    if (M(0x56)) {
        clear_object(g, x);
        return;
    }
    M(Tmp0) = 0;
    M(Tmp1) = (M(FrameCounter) >> 1) & 3;
    uint8_t ty = 0;
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        uint8_t a = (uint8_t)(M(Tmp0) + M(Tmp1));
        if (a >= 4) {
            M(Tmp1) = 0xFF;
            a = 0;
        }
        ty = a;
        if (!M(TurtleHealth0 + ty)) {
            break;
        }
        if (++M(Tmp0) == 4) {
            clear_object(g, x);
            return;
        } /* all the turtles are alive */
    }
    M(RescuedTurtle) = ty;
    load_palette_preset(g, (uint8_t)(0x45 + ty));
    set_chr_bank0(g, 9);
    M(0x58) = 1;
    object_to_entity(g, x, M(ObjType + x));
    M(ObjState + x) = 0;
    M(ObjCreated + x)++;
}

static void object_street_dam(Game *g, int x, uint8_t entity_type) /* street dam: entity $1C / $1D, unless already destroyed ($80/$81) */
{
    uint8_t id = M(ObjId + x);
    uint8_t gone = id < 0x10 ? (R7(0xF137 + (uint8_t)(id - 1)) & M(DamDestroyedMask0)) : (R7(0xF137 + (id & 0x0F)) & M(DamDestroyedMask1));
    if (gone) {
        clear_object(g, x);
        return;
    }
    int e = object_to_entity(g, x, entity_type);
    uint8_t y = (uint8_t)(e >= 0 ? e : x); /* Y returned by $B98D */
    M(EntHealth + y) = id;
    M(ObjState + x) = 0;
    M(ObjCreated + x)++;
}
OBJ_ROUTE2(object_dam_1c, object_street_dam, 0x1C)
OBJ_ROUTE2(object_dam_1d, object_street_dam, 0x1D)

/* type table of the objects not created yet ($B57C) */
static const Route object_type_routes[] = {
    {0xB714, route_nop},           {0xFE64, object_generic},    {0xFE9B, object_palette_0},  {0xFE97, object_palette_1},   {0xFE93, object_palette_2},
    {0xB715, object_rotating_bar}, {0xB8BE, object_crate_29},   {0xB8C2, object_crate_2a},   {0xB8C6, object_crate_2b},    {0xB8CA, object_crate_2c},
    {0xB87C, object_creator_2},    {0xB884, object_creator_3},  {0xB892, object_creator_4},  {0xB896, object_creator_5},   {0xB8F5, object_column},
    {0xB8AA, object_flagged_27},   {0xB8AE, object_flagged_28}, {0xB7F5, object_current_10}, {0xB7F9, object_current_11},  {0xB7FD, object_current_20},
    {0xB801, object_current_21},   {0xB805, object_current_40}, {0xB809, object_current_41}, {0xB80D, object_current_80},  {0xB811, object_current_81},
    {0xB786, object_scenery_10},   {0xB78A, object_scenery_11}, {0xB78E, object_scenery_12}, {0xB792, object_scenery_13},  {0xB79C, object_scenery_1e},
    {0xB7A0, object_scenery_1f},   {0xB7A4, object_scenery_20}, {0xB7A8, object_scenery_21}, {0xB7AC, object_scenery_22},  {0xB7B0, object_scenery_23},
    {0xB7B4, object_scenery_24},   {0xB7B8, object_scenery_25}, {0xB7BC, object_bomb},       {0xB934, object_dead_turtle}, {0xB845, object_dam_1c},
    {0xB849, object_dam_1d},
};

static void object_dispatch(Game *g, int x) /* B4 $B56E */
{
    if (M(ObjCreated + x)) { /* $B5F8: object already created */
        uint16_t st = rom_word(g->rom, 4, (uint16_t)(0xB5FE + (uint8_t)(M(ObjState + x) << 1)));
        if (!ROUTE_RUN(object_state_routes, st, g, x)) {
            TRACE("f=%u TODO object state %d (routine $%04X)\n", g->frame, M(ObjState + x), st);
        }
        return;
    }
    uint8_t t = M(ObjType + x);
    uint16_t r = rom_word(g->rom, 4, (uint16_t)(0xB57C + t * 2));
    if (!ROUTE_RUN(object_type_routes, r, g, x)) {
        TRACE("f=%u TODO object type %d (routine $%04X)\n", g->frame, t, r);
    }
}

void object_routes_report(Game *g, FILE *out) /* --routes report (see enemy_routes_report) */
{
    for (int i = 0; i < 0x3E; i++) {
        uint16_t addr = rom_word(g->rom, 4, (uint16_t)(0xB57C + i * 2));
        int found = 0;
        for (unsigned k = 0; k < sizeof object_type_routes / sizeof *object_type_routes; k++) {
            if (object_type_routes[k].rom_addr == addr) {
                found = 1;
            }
        }
        if (!found) {
            fprintf(out, "object type %02X: routine $%04X not ported\n", i, addr);
        }
    }
    for (int i = 0; i < 8; i++) {
        uint16_t addr = rom_word(g->rom, 4, (uint16_t)(0xB5FE + i * 2));
        int found = 0;
        for (unsigned k = 0; k < sizeof object_state_routes / sizeof *object_state_routes; k++) {
            if (object_state_routes[k].rom_addr == addr) {
                found = 1;
            }
        }
        if (!found) {
            fprintf(out, "object state %02X: routine $%04X not ported\n", i, addr);
        }
    }
}

static int object_out_of_range(Game *g, int x) /* B4 $B6B1: carry = out of the zone */
{
    uint8_t ac = M(SpawnLimitX), ad = M(SpawnLimitY);
    if (!M(Tmp0 + 6)) {
        if (M(ObjXMax + x) < ac) {
            return 1;
        }
    } else {
        if (M(ObjXMin + x) >= ac) {
            return 1;
        }
    }
    if (!M(Tmp0 + 7)) {
        return !(M(ObjYMax + x) >= ad);
    }
    return M(ObjYMin + x) >= ad;
}

static void object_visibility(Game *g, int x) /* B4 $B60E: removes the object when it leaves the zone near the camera */
{
    static const uint8_t probe[4][4] = {{0x08, 0xFC, 0, 1}, {0xF8, 0xFC, 1, 1}, {0x08, 0x08, 0, 0}, {0xF8, 0x08, 1, 0}}; /* $B616, $B63A, $B65C, $B67C */
    M(TmpA) = M(LevelNumber);
    int out = 1;
    for (int i = M(SideViewFlag) ? 2 : 0; i < 4; i++) { /* side view: only the last two probes */
        spawn_table_pointer(g);
        M(Tmp0 + 4) = probe[i][0];
        M(Tmp0 + 5) = probe[i][1];
        spawn_position(g);
        if (M(ObjColumn + x) != M(SpawnColumn)) {
            continue;
        }
        M(Tmp0 + 6) = probe[i][2];
        M(Tmp0 + 7) = probe[i][3];
        out = object_out_of_range(g, x);
        break;
    }
    if (!out) {
        return;
    }
    if (M(ObjFlags + x) & 8) {
        M(ObjFlags + x) &= 0xFD;
        return;
    } /* $B69E */
    clear_object(g, x);
}

void update_object_slots(Game *g) /* B4 $B554 */
{
    if (M(VerticalScrollMode)) {
        return;
    }
    for (int x = 0; x < 8; x++) {
        if (!M(ObjType + x)) {
            continue;
        }
        object_dispatch(g, x);
        M(ObjX + x) = (uint8_t)(M(ObjX + x) - M(CameraDeltaX)); /* $B6DD: the object follows the scrolling */
        M(ObjY + x) = (uint8_t)(M(ObjY + x) - M(CameraDeltaY));
        object_visibility(g, x);
    }
}
