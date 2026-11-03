/* Scrolling of the top-view map: columns (ScrollCameraX, B7 $D7D6 + $DAE1-$DC7C) and rows (ScrollCameraY,
 * B7 $DC8B-$DD24, which reuses draw_map_row). The new tiles go through the PPU buffer; the attributes of a column
 * go through the spare buffer ($0360, index $23), copied into the main buffer on the next frame ($C54A). */
#include "game.h"
#include "hal.h"
#include "stage_map.h"
#include "stage_scroll.h"
#define TODO_AREA "stage_map"
#define Tmp0_ Tmp0
#define Tmp4_ (Tmp0 + 4)
#define Tmp5_ (Tmp0 + 5)

#include <stdlib.h>

static void set_prg_saved(Game *g, uint8_t b) {
    M(CurPrgBank) = b;
    g->prg_bank = b;
}

static void append_alt(Game *g, uint8_t v) /* B7 $CAC7 / $CAC9 */
{
    uint8_t x = M(AltBufferIndex);
    M(AltPpuBuffer + x) = v;
    M(AltBufferIndex) = (uint8_t)(x + 1);
}

static void push_attribute_column(Game *g) /* B7 $DC01: 4 pairs of attribute bytes (RAM $05C0 / $06C0) */
{
    M(Tmp17) = 8;
    do {
        append_alt(g, 2);
        append_alt(g, M(Tmp0 + 3) | 0x80);
        uint8_t o = rom_prg(g->rom, 7, (uint16_t)(0xDAD6 + M(Tmp17)));
        append_alt(g, (uint8_t)(o + M(Tmp0)));
        append_alt(g, M((uint16_t)(ptr(g, Tmp0) + o)));
        uint8_t o2 = rom_prg(g->rom, 7, (uint16_t)(0xDAD5 + M(Tmp17)));
        append_alt(g, M((uint16_t)(ptr(g, Tmp0) + o2)));
        M(Tmp17) = (uint8_t)(M(Tmp17) - 2);
    } while (M(Tmp17) != 0);
}

static void push_column_pair(Game *g) /* B7 $DC34 */
{
    ROM_TABLE(shift, 16, 7, 0xDE0F); /* $DE0F */
    map_append(g, 2);
    map_append(g, M(PpuAddrHi) | 0x80); /* "vertical" address: a column */
    map_append(g, M(PpuAddrLo));
    M(TmpE) = (M(MapYInScreen) & 0x18) >> 1;
    uint8_t idx = (uint8_t)(((M(Tmp8) & 0x18) >> 3) | M(TmpE));
    if ((M(Tmp8) & 0x0F) == 0) {
        select_palette_bits(g, shift[idx], 1);
    }
    uint8_t t = rdmem(g, (uint16_t)(ptr(g, 0x10) + idx));
    M(PpuEntryCount)++;
    map_append(g, t);
    record_tile_terrain(g, t, idx, 1);
    M(PpuEntryCount)++;
    map_append(g, rdmem(g, (uint16_t)(ptr(g, 0x10) + idx + 4)));
}

static void draw_column_tile(Game *g) /* B7 $DB8E */
{
    uint16_t map = ptr(g, 0x49);
    M(Tmp0) = 0;
    M(Tmp0 + 1) = rdmem(g, map);
    int c = 0;
    for (uint8_t y = M(MapYScreen);;) {
        y--;
        if (y & 0x80) {
            break;
        }
        unsigned sum = M(Tmp0) + M(Tmp0 + 1);
        M(Tmp0) = (uint8_t)sum;
        c = sum > 0xFF;
        if (M(Tmp0) == 0) {
            break;
        }
    }
    unsigned y2 = M(Tmp0) + M(Tmp9) + c; /* no clc in the original */
    uint8_t a = rdmem(g, (uint16_t)(map + (uint8_t)(y2 + 2)));
    M(Tmp0 + 1) = a >> 2;
    int cc = (a >> 1) & 1;
    uint8_t v = a;
    v = map_ror(v, &cc);
    v = map_ror(v, &cc);
    v = map_ror(v, &cc);
    M(Tmp0) = v & 0xC0;
    unsigned s4 = M(TileDefPtrLo) + M(Tmp0);
    M(Tmp4_) = (uint8_t)s4;
    M(Tmp5_) = (uint8_t)(M(TileDefPtrHi) + M(Tmp0 + 1) + (s4 > 0xFF));
    M(TmpE) = (M(MapYInScreen) >> 2) & 0x38;
    uint8_t y = (uint8_t)((M(Tmp8) >> 5) | M(TmpE));
    uint8_t tile = rdmem(g, (uint16_t)(ptr(g, Tmp4_) + y));
    unsigned lo = (unsigned)((tile << 4) & 0xFF) + M(TileBaseLo); /* $DE41 */
    M(Tmp10) = (uint8_t)lo;
    M(Tmp11) = (uint8_t)((tile >> 4) + M(TileBaseHi) + (lo > 0xFF));
    set_prg_saved(g, 4);
    M(Tmp13) = rdmem(g, (uint16_t)(ptr(g, 0x4D) + tile));
    split_y(g);
    ppu_row_address(g, 0xC0);
    push_column_pair(g);
    if ((M(Tmp8) & 0x0F) == 0) {
        update_attribute_shadow(g);
    }
    set_prg_saved(g, 1);
}

/* --- horizontal scrolling of the side view (B7 $D7F3-$DAC2): a column of 6 blocks of 32 lines --- */
static void append_pair_flush(Game *g) /* B7 $DF9A: writes the byte count of the current entry */
{
    M(PpuBuffer + M(PpuEntryPos)) = M(PpuEntryCount);
    M(PpuEntryCount) = 0;
    M(PpuEntryPos) = M(PpuBufferIndex);
}

static void side_push_attributes(Game *g) /* B7 $DA1F: 4 attribute pairs in the main buffer */
{
    M(Tmp17) = 8;
    do {
        map_append(g, 2);
        map_append(g, M(Tmp0 + 3) | 0x80);
        uint8_t o = rom_prg(g->rom, 7, (uint16_t)(0xDAD6 + M(Tmp17)));
        map_append(g, (uint8_t)(o + M(Tmp0)));
        map_append(g, M((uint16_t)(ptr(g, Tmp0) + o)));
        uint8_t o2 = rom_prg(g->rom, 7, (uint16_t)(0xDAD5 + M(Tmp17)));
        map_append(g, M((uint16_t)(ptr(g, Tmp0) + o2)));
        M(Tmp17) = (uint8_t)(M(Tmp17) - 2);
    } while (M(Tmp17) != 0);
}

static void side_palette_bits(Game *g, uint8_t y) /* B7 $D96F: palette bits of the tile ($13) and of the next one ($17) */
{
    if (M(Tmp8) & 0x0F) {
        return;
    }
    uint8_t v = M(Tmp13), a;
    if (y == 0) {
        M(Tmp13) = v & 3;
        a = v >> 4;
    } else {
        int c = (v >> 1) & 1;
        M(Tmp13) = (v >> 2) & 3;
        a = map_rol(v, &c);
        a = map_rol(a, &c);
        a = map_rol(a, &c);
    }
    M(Tmp17) = a & 3;
}

static void side_advance_row(Game *g, uint8_t y) /* B7 $DA7C: PPU address of the next line (+$40) */
{
    M(SavedY) = y;
    unsigned s = M(PpuAddrLo) + 0x40;
    M(PpuAddrLo) = (uint8_t)s;
    M(PpuAddrHi) = (uint8_t)(M(PpuAddrHi) + (s > 0xFF));
    if ((M(PpuAddrHi) & 7) == 3 && M(PpuAddrLo) >= 0xC0) {
        M(PpuAddrLo) &= 0x3F;
        M(PpuAddrHi) = (uint8_t)((M(PpuAddrHi) & 0xFC) ^ 0x08);
        append_pair_flush(g);
        if (M(TmpD) != 1) {
            map_append(g, 2);
            map_append(g, M(PpuAddrHi) | 0x80);
            map_append(g, M(PpuAddrLo));
        }
    }
}

static void side_tile_pair(Game *g, uint8_t y) /* B7 $D954: two stacked 8x8 tiles */
{
    uint8_t t = rdmem(g, (uint16_t)(ptr(g, 0x10) + y));
    M(PpuEntryCount)++;
    map_append(g, t);
    record_tile_terrain(g, t, y, 1);
    y = (uint8_t)(y + 4);
    t = rdmem(g, (uint16_t)(ptr(g, 0x10) + y));
    M(PpuEntryCount)++;
    map_append(g, t);
    if (!(M(Tmp8) & 0x0F)) { /* $D99A */
        uint8_t keep = M(Tmp13);
        update_attribute_shadow(g);
        M(Tmp13) = keep;
    }
    side_advance_row(g, y);
}

static void side_draw_block(Game *g) /* B7 $D8EC: a block of 32 lines of the column */
{
#ifdef DIAG_ZPTRACE
    if (getenv("ZPTRACE") &&
        g->frame + 1 == (uint32_t)atoi(getenv("ZPTRACE"))) { /* ZPTRACE=<Mesen frame>: zero page at each block (compare with tools/lt_fuzz.lua) */
        static FILE *zf;
        if (!zf) {
            char fn[256];
            snprintf(fn, sizeof fn, "%s/zpt_port.txt", getenv("PCDIR") ? getenv("PCDIR") : ".");
            zf = fopen(fn, "w");
        }
        if (zf) {
            for (int i = 0; i < 256; i++) {
                fprintf(zf, i ? " %02x" : "%02x", M(i));
            }
            fputc(10, zf);
            fflush(zf);
        }
    }
#endif
    M(TmpE) = (M(MapYInScreen) >> 2) & 0x38;
    uint8_t y = (uint8_t)((M(Tmp8) >> 5) | M(TmpE));
    uint8_t tile = rdmem(g, (uint16_t)(ptr(g, 0x04) + y));
    unsigned lo = (unsigned)((tile << 4) & 0xFF) + M(TileBaseLo); /* $DE41 */
    M(Tmp10) = (uint8_t)lo;
    M(Tmp11) = (uint8_t)((tile >> 4) + M(TileBaseHi) + (lo > 0xFF));
    set_prg_saved(g, 3);
    M(Tmp13) = rdmem(g, (uint16_t)(ptr(g, 0x4D) + tile));
    if (!M(PpuEntryCount) && !M(PpuBufferIndex)) { /* $D917: start of an entry in the buffer */
        map_append(g, 2);
        map_append(g, M(PpuAddrHi) | 0x80);
        map_append(g, M(PpuAddrLo));
    }
    ROM_TABLE(first, 8, 7, 0xDACF);   /* $DACF */
    M(TmpE) = M(MapYInScreen) & 0x10; /* $D934 */
    uint8_t idx = (uint8_t)((((M(Tmp8) >> 1) & 0x0C) | M(TmpE)) >> 2);
    uint8_t ty = first[idx];
    side_palette_bits(g, ty);
    side_tile_pair(g, ty);
    M(Tmp13) = M(Tmp17);
    side_tile_pair(g, (uint8_t)(ty + 8));
    set_prg_saved(g, 1);
}

static void side_scroll_x(Game *g) /* B7 $D7F3 */
{
    uint8_t old = M(CameraXLo), hi = 0;
    if (M(CameraSpeedX) & 0x80) {
        hi = 0xFF;
    }
    unsigned s = M(CameraSpeedXFrac) + M(CameraXFrac);
    M(CameraXFrac) = (uint8_t)s;
    unsigned s2 = M(CameraSpeedX) + M(CameraXLo) + (s > 0xFF);
    M(CameraXLo) = (uint8_t)s2;
    M(CameraXHi) = (uint8_t)(hi + M(CameraXHi) + (s2 > 0xFF));
    if (M(CameraXHi) & 0x80) {
        M(CameraXHi) = M(CameraXLo) = M(CameraXFrac) = 0;
    }
    M(CameraDeltaX) = (uint8_t)(M(CameraXLo) - old);
    if (!((M(CameraXLo) ^ M(CameraXLoSaved)) & 8) && (M(CameraXHi) | M(CameraXLo))) {
        return; /* no new column */
    }
    unsigned s3 = rom_prg(g->rom, 7, (uint16_t)(0xDADF + (uint8_t)(M(ScrollDirX) - 1))) + M(CameraXLo);
    M(Tmp8) = (uint8_t)s3 & 0xF8;
    M(Tmp9) = (uint8_t)(M(CameraXHi) + (s3 > 0xFF));
    M(TmpA) = M(CameraYLo);
    M(TmpB) = M(CameraYHi);
    split_y(g);
    ppu_row_address(g, 0xC0);
    M(PpuEntryCount) = 0;
    M(PpuEntryPos) = M(PpuBufferIndex);
    split_c0(g);
    uint8_t width = rdmem(g, ptr(g, 0x49)), sum = 0;
    for (int n = M(MapYScreen); n > 0; n--) {
        sum = (uint8_t)(sum + width);
        if (!sum) {
            break;
        }
    }
    uint8_t b = rdmem(g, (uint16_t)(ptr(g, 0x49) + (uint8_t)(sum + M(Tmp9) + 2)));
    uint16_t block = (uint16_t)(M(TileDefPtrLo) | M(TileDefPtrHi) << 8);
    block = (uint16_t)(block + 3 * (uint16_t)(b << 4));
    M(Tmp4) = (uint8_t)block;
    M(Tmp5) = (uint8_t)(block >> 8);
    M(TmpD) = 6;
    do {
        split_c0(g);
        side_draw_block(g);
        unsigned a = M(TmpA) + 0x20;
        M(TmpA) = (uint8_t)a;
        M(TmpB) = (uint8_t)(M(TmpB) + (a > 0xFF));
    } while (--M(TmpD));
    append_pair_flush(g);
    if (M(Tmp8) & 0x0F) {
        return;
    }
    int c = 0;
    uint8_t v = M(Tmp8);
    v = map_rol(v, &c);
    v = map_rol(v, &c);
    v = map_rol(v, &c);
    v = map_rol(v, &c);
    M(Tmp0) = (v & 7) | 0xC0;
    M(Tmp0 + 1) = 5;
    M(Tmp0 + 3) = 0x23;
    side_push_attributes(g);
    M(Tmp0 + 1)++;
    M(Tmp0 + 3) = 0x2B;
    side_push_attributes(g);
}

void scroll_camera_x(Game *g) /* B7 $D7D6 (top view: $DAE1) */
{
    uint8_t dir = M(ScrollDirX);
    if (!dir && ((M(CameraXLo) | M(CameraXHi)) || !M(CameraXLoSaved))) {
        return;
    }
    if (M(StageState) == 3) {
        return;
    }
    if (M(SideViewFlag)) {
        side_scroll_x(g);
        return;
    }
    M(Tmp0) = (dir == 1) ? 6 : 0; /* $DAE1 */
    if (M(InVanFlag)) {
        M(Tmp0) += 3;
    }
    M(Tmp0 + 1) = M(CameraXLo);
    uint8_t y = M(Tmp0);
    unsigned s = rom_prg(g->rom, 7, (uint16_t)(0xDC7F + y)) + M(CameraXFrac);
    M(CameraXFrac) = (uint8_t)s;
    unsigned s2 = rom_prg(g->rom, 7, (uint16_t)(0xDC80 + y)) + M(CameraXLo) + (s > 0xFF);
    M(CameraXLo) = (uint8_t)s2;
    M(CameraXHi) = (uint8_t)(rom_prg(g->rom, 7, (uint16_t)(0xDC81 + y)) + M(CameraXHi) + (s2 > 0xFF));
    M(CameraDeltaX) = (uint8_t)(M(CameraXLo) - M(Tmp0 + 1));
    if (!((M(CameraXLo) ^ M(CameraXLoSaved)) & 8) && (M(CameraXHi) | M(CameraXLo))) {
        return; /* no new column */
    }
    unsigned s3 = rom_prg(g->rom, 7, (uint16_t)(0xDADF + (uint8_t)(dir - 1))) + M(CameraXLo); /* $DB29 */
    M(Tmp8) = (uint8_t)s3 & 0xF8;
    M(Tmp9) = (uint8_t)(M(CameraXHi) + (s3 > 0xFF));
    M(TmpA) = M(CameraYLo) & 0xF0;
    M(TmpB) = M(CameraYHi);
    split_y(g);
    ppu_row_address(g, 0xC0);
    M(TmpD) = 0x0F;
    do {
        split_c0(g);
        draw_column_tile(g);
        unsigned a = M(TmpA) + 0x10;
        M(TmpA) = (uint8_t)a;
        M(TmpB) = (uint8_t)(M(TmpB) + (a > 0xFF));
    } while (--M(TmpD));
    if (M(Tmp8) & 0x0F) {
        return;
    }
    int c = 0;
    uint8_t v = M(Tmp8);
    v = map_rol(v, &c);
    v = map_rol(v, &c);
    v = map_rol(v, &c);
    v = map_rol(v, &c);
    M(Tmp0) = (v & 7) | 0xC0;
    M(Tmp0 + 1) = 5;
    M(Tmp0 + 3) = 0x23;
    push_attribute_column(g);
    M(Tmp0 + 1)++;
    M(Tmp0 + 3) = 0x2B;
    push_attribute_column(g);
}

void scroll_camera_y(Game *g) /* B7 $DC8B */
{
    if (M(StageIntroTimer)) {
        return;
    }
    uint8_t dir;
    if (M(VerticalScrollMode) && M(VerticalScrollTimer)) { /* $DC98: vertical screen change: the camera is moved by the player */
        dir = M(ScrollDirY) = M(VerticalScrollMode);
        if (M(CameraYLo) & 7) {
            return;
        }
    } else {
        dir = M(ScrollDirY);
        if (!dir) {
            return;
        }
        uint8_t y = (dir == 1) ? 6 : 0;
        if (M(InVanFlag)) {
            y += 3;
        }
        M(Tmp0 + 1) = M(CameraYLo);
        unsigned s = rom_prg(g->rom, 7, (uint16_t)(0xE077 + y)) + M(CameraYFrac);
        M(CameraYFrac) = (uint8_t)s;
        unsigned s2 = rom_prg(g->rom, 7, (uint16_t)(0xE078 + y)) + M(CameraYLo) + (s > 0xFF);
        M(CameraYLo) = (uint8_t)s2;
        M(CameraYHi) = (uint8_t)(rom_prg(g->rom, 7, (uint16_t)(0xE079 + y)) + M(CameraYHi) + (s2 > 0xFF));
        M(CameraDeltaY) = (uint8_t)(M(CameraYLo) - M(Tmp0 + 1));
        if (!((M(CameraYLo) ^ M(CameraYLoSaved)) & 8)) {
            return; /* no new row */
        }
    }
    uint8_t i = (uint8_t)((dir << 1) - 2); /* $DCED */
    if (M(SideViewFlag)) {                 /* $DD07: in side view, no $F8 mask */
        unsigned a = rom_prg(g->rom, 7, (uint16_t)(0xE06F + i)) + M(CameraYLo);
        M(TmpA) = (uint8_t)a;
        M(TmpB) = (uint8_t)(rom_prg(g->rom, 7, (uint16_t)(0xE070 + i)) + M(CameraYHi) + (a > 0xFF));
    } else {
        unsigned a = rom_prg(g->rom, 7, (uint16_t)(0xE073 + i)) + M(CameraYLo);
        M(TmpA) = (uint8_t)a & 0xF8;
        M(TmpB) = (uint8_t)(rom_prg(g->rom, 7, (uint16_t)(0xE074 + i)) + M(CameraYHi) + (a > 0xFF));
    }
    unsigned x = M(CameraXLo) + 8;
    M(Tmp8) = (uint8_t)x & 0xF0;
    M(Tmp9) = (uint8_t)(M(CameraXHi) + (x > 0xFF));
    draw_map_row(g); /* $DD25 */
}
