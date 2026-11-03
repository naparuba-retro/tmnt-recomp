/* Drawing of a level's map, one row of 16 tiles of 16x16 per frame (StageState 4): B7 $D4E5-$D542, $DD25-$DEFC,
 * $DF19-$E067, $D9AF-$DA51. The pointers of these routines target the current bank at $8000 (g->prg_bank), bank 7
 * or RAM, as on the console: rdmem() does the same. */
#include "game.h"
#include "hal.h"
#include "stage_map.h"

#define Tmp0_ Tmp0
#define Tmp4_ (Tmp0 + 4)
#define Tmp5_ (Tmp0 + 5)

/* --- 6502 helpers: rotate through the carry --- */
uint8_t map_rol(uint8_t v, int *c) {
    int n = v >> 7;
    v = (uint8_t)((v << 1) | *c);
    *c = n;
    return v;
}
uint8_t map_ror(uint8_t v, int *c) {
    int n = v & 1;
    v = (uint8_t)((v >> 1) | (*c << 7));
    *c = n;
    return v;
}

void map_append(Game *g, uint8_t v) {
    ppu_buffer_append(g, v);
}

/* B7 $DF19 */
void split_y(Game *g) {
    unsigned y = M(TmpA) | M(TmpB) << 8, n = 0;
    while (y >= 0xF0) {
        y -= 0xF0;
        n++;
    }
    M(Tmp14) = (uint8_t)y;
    M(Tmp15) = (uint8_t)n;
}

/* B7 $DF40: side view only, vertical position in screens of $C0 lines -> $53 (remainder), $54 (screen) */
void split_c0(Game *g) {
    M(MapYInScreen) = M(TmpA);
    M(MapYScreen) = M(TmpB);
    M(Tmp0) = 0xC0;
    if (!M(SideViewFlag)) {
        return;
    }
    uint8_t y = 0;
    while (M(MapYScreen) != 0 || M(MapYInScreen) >= M(Tmp0)) {
        unsigned d = M(MapYInScreen) - M(Tmp0);
        M(MapYScreen) = (uint8_t)(M(MapYScreen) - (M(MapYInScreen) < M(Tmp0)));
        M(MapYInScreen) = (uint8_t)d;
        y++;
    }
    M(MapYScreen) = y;
}

/* B7 $DFF8: A = 16x16 tile number -> address of its definition in ($61:$60) = Tmp5:Tmp4 */
static void tile_definition_addr(Game *g, uint8_t a) {
    uint8_t y;
    if (!M(SideViewFlag)) {
        M(Tmp0 + 1) = a >> 2;
        int c = (a >> 1) & 1;
        uint8_t v = a;
        v = map_ror(v, &c);
        v = map_ror(v, &c);
        v = map_ror(v, &c);
        M(Tmp0) = v & 0xC0;
        y = 1;
    } else {
        M(Tmp0 + 1) = a >> 4;
        M(Tmp0) = (uint8_t)(a << 4);
        y = 3;
    }
    M(Tmp4_) = M(TileDefPtrLo);
    M(Tmp5_) = M(TileDefPtrHi);
    do {
        unsigned s = M(Tmp4_) + M(Tmp0);
        M(Tmp4_) = (uint8_t)s;
        M(Tmp5_) = (uint8_t)(M(Tmp5_) + M(Tmp0 + 1) + (s > 0xFF));
    } while (--y);
}

/* B7 $DFAA: finds in the map ($49) the two tile columns of the current block -> $B1/$B2 and $B3/$B4 */
static void locate_blocks(Game *g) {
    uint16_t map = ptr(g, 0x49);
    M(Tmp0) = 0;
    M(Tmp0 + 1) = rdmem(g, map); /* width of the map in blocks */
    for (uint8_t y = M(MapYScreen);;) {
        y--;
        if (y & 0x80) {
            break;
        }
        M(Tmp0) = (uint8_t)(M(Tmp0) + M(Tmp0 + 1));
        if (M(Tmp0) == 0) {
            break;
        }
    }
    uint8_t y = (uint8_t)(M(Tmp0) + M(Tmp9) + 2);
    M(Tmp0 + 2) = rdmem(g, (uint16_t)(map + y));
    if (M(Tmp0 + 2) == 0xFF) {
        M(Tmp0 + 2)++;
    }
    y++;
    M(Tmp0 + 3) = rdmem(g, (uint16_t)(map + y));
    if (M(Tmp0 + 3) == 0xFF) {
        M(Tmp0 + 3)++;
    }
    tile_definition_addr(g, M(Tmp0 + 2));
    M(0xB1) = M(Tmp4_);
    M(0xB2) = M(Tmp5_);
    tile_definition_addr(g, M(Tmp0 + 3));
    M(0xB3) = M(Tmp4_);
    M(0xB4) = M(Tmp5_);
}

/* B7 $E036: copies 8 bytes of each block into $03B0 and $03B8 */
static void copy_block_rows(Game *g) {
    uint8_t y = (uint8_t)(((M(MapYInScreen) >> 2)) & 0x38);
    for (int x = 0; x < 8; x++, y++) {
        M(0x03B0 + x) = rdmem(g, (uint16_t)(ptr(g, 0xB1) + y));
        M(0x03B8 + x) = rdmem(g, (uint16_t)(ptr(g, 0xB3) + y));
    }
    M(0xB1) = M(Tmp9);
    M(0xB2) = (uint8_t)((M(MapYInScreen) & 0x18) >> 1);
}

/* B7 $DF70: start PPU address of the row ($6F:$70) and variant $DA52 (attributes, mask $C0) */
void ppu_row_address(Game *g, uint8_t mask) {
    M(PpuAddrHi) = (uint8_t)((M(Tmp15) & 1) << 3);
    int c = 0;
    uint8_t v = M(Tmp14);
    v = map_rol(v, &c);
    v = map_rol(v, &c);
    v = map_rol(v, &c);
    M(PpuAddrHi) = (uint8_t)((v & 3) | 0x20 | M(PpuAddrHi));
    M(PpuAddrLo) = (uint8_t)((M(Tmp14) << 2) & mask);
    M(PpuAddrLo) |= (M(Tmp8) >> 3) & 0x1F;
}

/* B7 $E05B: next column (+2), back to column 0 at the end of the row */
static void next_column(Game *g) {
    M(PpuAddrLo) = (uint8_t)(M(PpuAddrLo) + 2);
    if ((M(PpuAddrLo) & 0x1F) == 0) {
        M(PpuAddrLo) = (uint8_t)(M(PpuAddrLo) - 0x20);
    }
}

/* B7 $D9AF: attribute byte (RAM $05C0 / $06C0): replaces 2 bits with $13 */
void update_attribute_shadow(Game *g) {
    int c = M(PpuAddrLo) >> 7;
    uint8_t a = M(PpuAddrHi);
    for (int i = 0; i < 4; i++) {
        a = map_rol(a, &c);
    }
    M(Tmp16) = a & 0x38;
    int c2 = (M(PpuAddrLo) >> 1) & 1; /* lsr a, lsr a: carry = bit 1 of $70 */
    uint8_t lo = (uint8_t)(((M(PpuAddrLo) >> 2) & 7) | M(Tmp16) | 0xC0);
    M(Tmp14) = lo;
    M(Tmp15) = (M(PpuAddrHi) & 8) ? 6 : 5;
    M(Tmp16) = M(ptr(g, 0x14));
    uint8_t v = M(PpuAddrLo);
    v = map_rol(v, &c2);
    v = map_rol(v, &c2);
    unsigned idx = (M(PpuAddrLo) & 2) + c2;
    uint16_t a14 = ptr(g, 0x14);
    switch (idx) {
        case 0:
            M(a14) = (M(Tmp16) & 0xFC) | M(Tmp13);
            break;
        case 1:
            M(Tmp13) = (uint8_t)(M(Tmp13) << 4);
            M(a14) = (M(Tmp16) & 0xCF) | M(Tmp13);
            break;
        case 2:
            M(Tmp13) = (uint8_t)(M(Tmp13) << 2);
            M(a14) = (M(Tmp16) & 0xF3) | M(Tmp13);
            break;
        default: {
            int cc = 0;
            uint8_t t = M(Tmp13);
            t = map_ror(t, &cc);
            t = map_ror(t, &cc);
            t = map_ror(t, &cc);
            M(Tmp13) = t;
            M(a14) = (M(Tmp16) & 0x3F) | t;
            break;
        }
    }
    (void)v;
}

/* B7 $DEC2: table ($62) of (limit, value) pairs: value of the 1st pair whose limit >= A */
static uint8_t lookup_palette(Game *g, uint8_t a) {
    M(Tmp0) = a;
    uint8_t y = 0;
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        uint8_t v = rdmem(g, (uint16_t)(ptr(g, 0x62) + y));
        if (v == 0xFF || v >= M(Tmp0)) {
            break;
        }
        y = (uint8_t)(y + 2);
    }
    return rdmem(g, (uint16_t)(ptr(g, 0x62) + y + 1));
}

/* B7 $DE57: copies the tile's palette into the RAM terrain table $0700 (one nibble per 16x16 tile) */
void record_tile_terrain(Game *g, uint8_t tile, uint8_t saved_y, int column) {
    M(Tmp0 + 3) = saved_y;
    if (column ? (M(Tmp8) & 0x0F) : (M(TmpA) & 8)) {
        return; /* $DE63 (column) or $DE57 (row) */
    }
    uint8_t t2 = lookup_palette(g, tile);
    M(Tmp0 + 2) = t2;
    if (M(PpuAddrLo) & 2) {
        M(Tmp0 + 2) = (uint8_t)(t2 << 4);
    }
    uint8_t a = M(PpuAddrLo) >> 2;
    int c = a & 1; /* lsr a x3: carry = bit 2 of $70 */
    uint8_t p = (uint8_t)(((a >> 1) & 0x18) | (a & 7));
    M(Tmp0) = p;
    uint8_t v = M(PpuAddrHi);
    v = map_ror(v, &c);
    v = map_ror(v, &c);
    v = map_ror(v, &c);
    v = map_ror(v, &c);
    M(Tmp0 + 1) = v & 0x60;
    v = map_ror(v, &c);
    M(Tmp0) = (uint8_t)((v & 0x80) | M(Tmp0 + 1) | M(Tmp0));
    M(Tmp0 + 1) = 7;
    uint16_t a07 = (uint16_t)(0x0700 + M(Tmp0));
    uint8_t cur = M(a07);
    cur = (M(PpuAddrLo) & 2) ? (cur & 0x0F) : (cur & 0xF0);
    M(a07) = cur | M(Tmp0 + 2);
}

/* B7 $DE1F: extracts 2 bits of $13 (palette index) */
void select_palette_bits(Game *g, uint8_t y, int column) {
    if (!column && (M(TmpA) & 8)) {
        return;
    }
    while (y--) {
        M(Tmp13) >>= 2;
    }
    M(Tmp13) &= 3;
}

/* B7 $DDE0: pushes a pair of 8x8 tiles (2 tiles) at address $6F:$70 */
static void push_tile_pair(Game *g) {
    ROM_TABLE(shift, 16, 7, 0xDE0F); /* $DE0F */
    map_append(g, 2);
    map_append(g, M(PpuAddrHi));
    map_append(g, M(PpuAddrLo));
    uint8_t t = rdmem(g, (uint16_t)(ptr(g, 0x10) + M(SavedY)));
    map_append(g, t);
    M(PpuEntryCount)++;
    record_tile_terrain(g, t, M(SavedY), 0);
    select_palette_bits(g, shift[M(SavedY) & 15], 0);
    map_append(g, rdmem(g, (uint16_t)(ptr(g, 0x10) + M(SavedY) + 1)));
    M(PpuEntryCount)++;
}

/* B7 $DDA8: one 16x16 tile of the row */
static void draw_tile_16(Game *g) {
    uint8_t col = M(Tmp8) >> 5, c = (M(Tmp8) >> 4) & 1;
    M(Tmp0) = col;
    uint8_t base = (M(Tmp9) != M(0xB1)) ? 8 : 0;
    M(0xB3) = M(0x03B0 + (uint8_t)(base + col));
    uint8_t y = M(0xB2);
    if (c) {
        y = (uint8_t)(y + 2);
    }
    M(SavedY) = y;
    /* $DE41: ($11:$10) = $4B/$4C + tile * 16 */
    uint8_t tile = M(0xB3);
    unsigned lo = (unsigned)(tile << 4 & 0xFF) + M(TileBaseLo);
    M(Tmp10) = (uint8_t)lo;
    M(Tmp11) = (uint8_t)((tile >> 4) + M(TileBaseHi) + (lo > 0xFF));
    M(Tmp13) = rdmem(g, (uint16_t)(ptr(g, 0x4D) + tile));
    push_tile_pair(g);
    if (!(M(TmpA) & 8)) {
        update_attribute_shadow(g);
    }
    next_column(g);
}

/* B7 $D9AF: one row of 16 tiles + (every 2 rows) the attribute byte */
void draw_map_row(Game *g) {
    split_c0(g);
    locate_blocks(g);
    copy_block_rows(g);
    split_y(g);
    ppu_row_address(g, 0xE0);
    M(TmpD) = 0x10;
    M(PpuEntryCount) = 0;
    M(PpuEntryPos) = M(PpuBufferIndex);
    static const uint8_t bank[2] = {4, 3}; /* $DDA6 */
    g->prg_bank = bank[M(SideViewFlag) & 1];
    do {
        draw_tile_16(g);
        unsigned s = M(Tmp8) + 0x10;
        M(Tmp8) = (uint8_t)s;
        M(Tmp9) = (uint8_t)(M(Tmp9) + (s > 0xFF));
    } while (--M(TmpD));
    g->prg_bank = 1;
    if (M(TmpA) & 8) {
        return;
    }
    split_y(g);
    M(Tmp0) = (uint8_t)(((M(Tmp14) >> 2) & 0x38) | 0xC0);
    M(Tmp0 + 3) = 0x23;
    M(Tmp0 + 1) = 5;
    if (M(Tmp15) & 1) {
        M(Tmp0 + 3) = 0x2B;
        M(Tmp0 + 1)++;
    }
    map_append(g, 8);
    map_append(g, M(Tmp0 + 3));
    map_append(g, M(Tmp0));
    for (int y = 0; y < 8; y++) {
        map_append(g, M((uint16_t)(ptr(g, Tmp0) + y)));
    }
}

/* B7 $D49F StageState4 of a vertical scrolling level (level 5, zone 6): the screen is written in one go, the $0700 collision map is new */
static void stage_draw_vertical_map(Game *g) {
    M(StageState)++;
    g->ppu.mirror_v = 1; /* B7 $CE73: MMC1 control $1E */
    g->prg_bank = 1;
    load_screen(g, 1, 0); /* $C97F: empty background, buffers $05C0 and $06C0 cleared */
    for (int y = 0x3F; y >= 0; y--) {
        M(AttrBufferA + y) = 0;
        M(AttrBufferB + y) = 0;
    }
    load_screen(g, 1, 0x16);
    M(HudPpuAddrHi) = 0x23;
    M(HudPpuAddrLo) = 0;
    if (!M(0x0609)) {
        load_screen(g, 1, 0x14);
        for (int x = 0; x < 256; x++) {
            M(TerrainMap + x) = 0; /* $D4C7: txa only once, the loop $D4C8 writes A = 0 */
        }
        for (int x = 7; x >= 0; x--) {
            M(0x0758 + x) = 0x44;
            M(0x07D8 + x) = 0x44;
        }
        return;
    }
    M(PpuCtrlShadow) = M(PpuCtrlSaved);
    M(ScrollXShadow) = M(CameraXLo);
}

/* B7 $D4E5 StageState4: one row per frame up to $A3 rows */
void stage_draw_map(Game *g) {
    if (M(VerticalLevelFlag) & 0x80) {
        stage_draw_vertical_map(g);
        return;
    }
    g->prg_bank = 1;
    M(MapRowIndex)++;
    if (M(MapRowIndex) & 0x80) { /* $D533: row above the camera, if there is one */
        if (M(SideViewFlag)) {
            return;
        }
        if (M(CameraYHi) == 0 && (M(CameraYLo) & 0xF8) == 0) {
            return;
        }
        unsigned d = M(CameraYLo) - 8;
        M(TmpA) = (uint8_t)d;
        M(TmpB) = (uint8_t)(M(CameraYHi) - (M(CameraYLo) < 8));
    } else {
        unsigned s = (uint8_t)(M(MapRowIndex) << 3) + M(CameraYLo);
        M(TmpA) = (uint8_t)s;
        M(TmpB) = (uint8_t)(M(CameraYHi) + (s > 0xFF));
    }
    split_c0(g); /* $D507 */
    unsigned s = M(CameraXLo) + 8;
    M(Tmp8) = (uint8_t)(s & 0xF0);
    M(Tmp9) = (uint8_t)(M(CameraXHi) + (s > 0xFF));
    split_y(g);
    ppu_row_address(g, 0xC0);
    M(TmpD) = 0x10;
    draw_map_row(g);
    if (M(MapRowIndex) == M(MapRowsToDraw)) {
        M(StageState)++;
        M(MapRowsToDraw) = 0;
    }
}
