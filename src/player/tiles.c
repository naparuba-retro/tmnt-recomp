/* Map: tile under a point and collision probes. */
#include "game.h"
#include "player.h"
#include "player/special_states.h"
#include "player/tiles.h"
#define TODO_AREA "player" /* prefix of the TODO traces of this file */
#define OFF_X(i) (rom_prg(g->rom, B7, (uint16_t)(0xC83E + (i) * 2)))
#define OFF_Y(i) (rom_prg(g->rom, B7, (uint16_t)(0xC83F + (i) * 2)))

/* --- map: tile under a point (B7 $C7AF / $C76B) --- */
static uint8_t tile_at_vertical(Game *g, uint8_t px, uint8_t py) /* B7 $C76F-$C7AC: vertical level, map of 32 columns of 8-pixel tiles */
{
    unsigned cam = M(CameraXLo) | M(CameraXHi) << 8;
    int left = cam < 0x80;           /* $C773: overflows on the left, only the wall ($04) counts */
    unsigned xx = px + M(CameraXLo); /* $C782 */
    M(Tmp0 + 2) = (uint8_t)xx;
    uint8_t hi = (uint8_t)(M(CameraXHi) + (xx > 0xFF));
    unsigned yy = py + M(CameraYLo);
    uint8_t ylo = (uint8_t)yy;
    int c = hi & 1; /* lsr $03: bit 0 of the high byte goes into ror */
    M(Tmp0 + 3) = hi >> 1;
    ylo = (uint8_t)(((c << 7) | (ylo >> 1)) & 0xF8);
    M(Tmp0 + 4) = ylo;
    uint8_t idx = (uint8_t)(((uint8_t)xx >> 5) | ylo);
    uint8_t v = M(TerrainMap + idx);
    if (xx & 0x10) {
        v >>= 4;
    }
    v &= 0x0F;
    if (left) {
        return v == 4 ? 4 : 0;
    }
    return v;
}

uint8_t tile_at(Game *g, uint8_t px, uint8_t py, uint8_t yreg) /* GetTileAt (Y = Y register on entry: $1A) */
{
    M(SavedY) = yreg;
    if (M(VerticalLevelFlag) & 0x80) {
        return tile_at_vertical(g, px, py);
    }
    unsigned sum = px + M(CameraXLo);
    uint8_t a = (uint8_t)sum;
    int c = sum > 0xFF;
    for (int i = 0; i < 4; i++) { /* 4 x rol a: 9-bit rotation */
        int nc = a >> 7;
        a = (uint8_t)((a << 1) | c);
        c = nc;
    }
    M(Tmp0) = a & 7;
    unsigned s1 = py + M(CameraYLo);
    uint8_t t1 = (uint8_t)s1, t2 = (uint8_t)(M(CameraYHi) + (s1 > 0xFF));
    uint8_t y = 0;
    while (t2 != 0 || t1 >= 0xF0) { /* height of a screen: $F0 */
        unsigned d = t1 - 0xF0;
        t2 = (uint8_t)(t2 - (t1 < 0xF0));
        t1 = (uint8_t)d;
        y++;
    }
    int ycarry = y & 1;
    t1 = (uint8_t)((ycarry << 7) | (t1 >> 1)); /* ror a / ror Tmp1 with C = 0 */
    uint8_t idx = (uint8_t)((t1 & 0xF8) | M(Tmp0));
    M(Tmp1_) = t1;
    M(Tmp2_) = t2;
    M(TileIndex) = idx;
    uint8_t v = M(TerrainMap + idx);
    if (c) {
        v >>= 4;
    }
    return v & 0x0F;
}

uint8_t tile_near_player(Game *g, uint8_t i) /* $C7AF: i = probe point around the player */
{
    uint8_t px = (uint8_t)(OFF_X(i) + M(EntX)), py = (uint8_t)(OFF_Y(i) + M(EntY));
    M(Tmp0) = px;
    M(Tmp1_) = py;
    return tile_at(g, px, py, (uint8_t)(i << 1));
}

uint8_t tile_at_xy(Game *g, uint8_t px, uint8_t py, uint8_t yreg) /* B7 $C7E4 */
{
    M(Tmp0) = px;
    M(Tmp0 + 1) = py;
    return tile_at(g, px, py, yreg);
}

uint8_t tile_at_offset(Game *g, int x, uint8_t dx, uint8_t dy) /* B7 $F091 GetTileAtOffset */
{
    uint8_t px = (uint8_t)(M(EntX + x) + dx), py = (uint8_t)(M(EntY + x) + dy);
    M(Tmp0) = px;
    M(Tmp0 + 1) = py;
    return tile_at(g, px, py, dy);
}
static int probe_table(Game *g, uint8_t tile, uint8_t a) /* B4 $A6B3: search for the tile in the table a */
{
    M(Tmp0) = tile;
    uint16_t p = rom_word(g->rom, B4, (uint16_t)(0xA6EC + (uint8_t)(a << 1)));
    M(Tmp2_) = (uint8_t)p;
    M(Tmp3_) = (uint8_t)(p >> 8);
    for (uint8_t i = 0;; i = (uint8_t)(i + 3)) {
        uint8_t v = R4(p + i);
        if (v == 0xFF) {
            return 0;
        }
        if (v != tile) {
            continue;
        }
        uint8_t code = R4(p + i + 1);
        if (code == 1) {
            return player_run_state(g, R4(p + i + 2)); /* $A6E6: immediately runs the routine of this state ($9CB0) */
        }
        if (code > 1) {
            return 1;
        }
        M(EntState) = R4(p + i + 2);
        return 1;
    }
}

int probe_tile(Game *g, uint8_t y, uint8_t a) {
    return probe_table(g, tile_near_player(g, y), a);
}

static uint8_t tile_near_player_vel(Game *g, uint8_t i) /* B7 $C7C7: like $C7AF, at the point where the player will be after its velocity */
{
    uint8_t px = (uint8_t)(OFF_X(i) + M(EntVelX) + M(EntX)), py = (uint8_t)(OFF_Y(i) + M(EntVelY) + M(EntY));
    M(Tmp0) = px;
    M(Tmp1_) = py;
    return tile_at(g, px, py, (uint8_t)(i << 1));
}

int probe_tile_vel(Game *g, uint8_t y, uint8_t a) /* B4 $A6A8 */
{
    return probe_table(g, tile_near_player_vel(g, y), a);
}
