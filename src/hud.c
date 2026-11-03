/* Side-view status bar (B0 $8055): fills the PPU buffer, one part every 8 frames.
 * The bank 0 routines are read from the ROM (tables at $8402 and $8489). */
#include "game.h"
#include "hal.h"
#include "hud.h"
#define TODO_AREA "hud" /* prefix of the TODO traces of this file */

#define put(v) ppu_buffer_append(g, (uint8_t)(v))

static void head(Game *g, uint8_t count, uint8_t off) /* B0 $842B: byte count, address $29/$2A + off */
{
    put(count);
    put(M(HudPpuAddrHi));
    put(M(HudPpuAddrLo) + off);
}

static void string(Game *g, uint8_t i) /* B0 $846E: string of the $8489 table up to $FF */
{
    uint16_t p = (uint16_t)(R0(0x8489 + i * 2) | R0(0x848A + i * 2) << 8);
    for (; R0(p) != 0xFF; p++) {
        put(R0(p));
    }
}

static void score(Game *g, uint8_t zp) /* B0 $841D: 3 BCD bytes of page 0 as 7 digits */
{
    for (int y = 2; y >= 0; y--) {
        uint8_t b = M(zp + y);
        put((b >> 4) ? (b >> 4) : 0x0A);
        put((b & 0x0F) ? (b & 0x0F) : 0x0A);
    }
    put(0x0A);
}

static void two_digits(Game *g, uint8_t a) /* B7 $C742: tens then units ($0A for zero) */
{
    uint8_t tens = 0;
    while (a >= 10) {
        a -= 10;
        tens++;
    }
    put(tens ? tens : 0x0A);
    put(a ? a : 0x0A);
}

static void health_bar_raw(Game *g, uint8_t a) /* B0 $81AC: 8 slots; the type comes from $08 */
{
    uint8_t kind = M(Tmp8);
    M(Tmp0 + 1) = a;
    int y = a >> 4;
    for (int n = 8; n; n--) {
        uint8_t t = 0x30;
        if (--y < 0) {
            uint8_t lo = M(Tmp0 + 1) & 0x0F;
            if (lo == 0) {
                if (kind == 5 && (!M(WaveActive) || !M(WaveState))) {
                    t = 0;
                } else {
                    t = 0x32;
                }
            } else if (lo < 8) {
                M(Tmp0 + 1) = 0;
                t = 0x31;
            } else {
                M(Tmp0 + 1) = 0;
                t = 0x30;
            }
        }
        put(t);
    }
}

static void health_bar(Game *g, uint8_t a, uint8_t kind) {
    M(Tmp8) = kind;
    health_bar_raw(g, a);
}

static void pair(Game *g, uint8_t a) {
    put(a);
    put(a + 2);
} /* two tiles: a then a+2 */

static void weapon_hud(Game *g) /* B0 $81F0-$8191: sub-weapon (blinks) */
{
    if (!M(SubWeaponSelected) || (M(FrameCounter) & 8)) {
        return;
    }
    head(g, 2, 0x38);
    string(g, 2);
    put(M(HudPpuAddrHi));
    put(M(HudPpuAddrLo) + 0x58);
    put(0);
    put(0);
}

#define counter_level(i) ROM_AT(0, 0x8402, (i)) /* B0 $8402: levels with a counter */

static void ammo_hud(Game *g) /* B0 $82E8 */
{
    M(Tmp8) = M(HudPpuAddrHi);
    M(Tmp9) = M(HudPpuAddrLo);
    uint8_t t = M(CurrentTurtle);
    M(TmpA) = M(SubWeaponType0 + t);
    M(TmpB) = M(SubWeaponAmmo0 + t);
    put(8);
    put(M(Tmp8));
    put(M(Tmp9) + 0x35);
    M(TmpC) = (uint8_t)(t * 4 + 0x40);
    pair(g, M(TmpC));
    put(0);
    M(TmpD) = M(TmpA);
    if (M(TmpA)) {
        uint8_t a = (uint8_t)(M(TmpA) * 4 + 0x4C);
        M(TmpD) = (uint8_t)(a + 1);
        pair(g, a);
    } else {
        put(0x3C);
        put(0x3E);
    }
    put(0);
    if (counter_level(M(LevelNumber))) {
        if (M(RopeCount)) {
            put(0x34);
            put(0x36);
        } else {
            put(0x3C);
            put(0x3E);
        }
    } else {
        put(0);
        put(0);
    }
    put(8);
    put(M(Tmp8));
    put(M(Tmp9) + 0x55);
    M(TmpC)++;
    pair(g, M(TmpC));
    put(0);
    if (M(TmpD)) {
        pair(g, M(TmpD));
    } else {
        put(0x3D);
        put(0x3F);
    }
    put(0);
    if (counter_level(M(LevelNumber))) {
        if (M(RopeCount)) {
            put(0x35);
            put(0x37);
        } else {
            put(0x3D);
            put(0x3F);
        }
    } else {
        put(0);
        put(0);
    }
    put(5);
    put(M(Tmp8));
    put(M(Tmp9) + 0x78);
    if (M(TmpB)) {
        two_digits(g, M(TmpB));
    } else {
        put(0);
        put(0);
    }
    put(0);
    if (counter_level(M(LevelNumber)) && M(RopeCount)) {
        two_digits(g, M(RopeCount));
    } else {
        put(0);
        put(0);
    }
}

/* B0 $81F0: during a vertical screen change, one step per frame ($3E - $20 or $3A, from 10 down to 0: indices 5 to 0 of the $8207 table) */
static void put_row_header(Game *g, uint8_t count, uint8_t add) /* B0 $8437 + adc: count, address $84/$85 */
{
    put(count);
    put(M(HudRowAddrHi));
    put(M(HudRowAddrLo) + add);
}

static void hud_scroll_lock(Game *g) {
    uint8_t a = (M(VerticalScrollMode) == 1) ? (uint8_t)(M(VerticalScrollTimer) - 0x20) : (uint8_t)(M(VerticalScrollTimer) - 0x3A);
    if (a >= 0x0B) {
        return;
    }
    switch (a >> 1) {
        case 5: {                      /* $8213 */
            M(MapYInScreen) = M(TmpA); /* B7 $DF40: screen row in $54 */
            M(MapYScreen) = M(TmpB);
            if (M(SideViewFlag)) {
                uint8_t y = 0;
                while (M(MapYScreen) != 0 || M(MapYInScreen) >= 0xC0) {
                    unsigned d = M(MapYInScreen) - 0xC0;
                    M(MapYScreen) = (uint8_t)(M(MapYScreen) - (M(MapYInScreen) < 0xC0));
                    M(MapYInScreen) = (uint8_t)d;
                    y++;
                }
                M(MapYScreen) = y;
            }
            uint8_t y = M(MapYScreen);
            if (M(VerticalScrollMode) == 2) {
                y++;
            }
            M(HudRowAddrHi) = R0(0x84A2 + y);
            M(HudRowAddrLo) = (M(HudRowAddrHi) == 0x21) ? 0x80 : 0;
            put_row_header(g, 0xD0, 0x50);
            put(0);
            break;
        }
        case 4: /* $823B */
            put_row_header(g, 0xD0, 0);
            put(0);
            break;
        case 3: { /* $8248 */
            put(0xA0);
            put(M(HudRowAddrHi) == 0x21 ? 0x22 : M(HudRowAddrHi));
            put(M(HudRowAddrLo) + 0xA0);
            put(0);
            put(0x90);
            put((M(HudRowAddrHi) & 8) | 0x23);
            uint8_t lo = (uint8_t)(((M(HudRowAddrHi) & 3) << 4) | 0xC0);
            lo |= (uint8_t)(M(HudRowAddrLo) >> 4);
            put(lo);
            put(0);
            uint16_t p = (uint16_t)(((M(HudRowAddrHi) & 8) ? 6 : 5) << 8 | lo);
            for (int y = 7; y >= 0; y--) {
                M(p + y) = 0;
            }
            for (int y = 15; y >= 8; y--) {
                M(p + y) &= 0xF0;
            }
            break;
        }
        case 2: /* $82A2 */
            put_row_header(g, 0x0A, 0x21);
            string(g, 0);
            score(g, 0xC2);
            put_row_header(g, 0x0A, 0x61);
            string(g, 1);
            score(g, 0xC8);
            break;
        case 1: /* $82CA */
            put_row_header(g, 8, 0x2C);
            health_bar_raw(g, M(TurtleHealth0 + M(CurrentTurtle)));
            break;
        case 0: /* $82DC */
            M(HudPpuAddrHi) = M(HudRowAddrHi);
            M(HudPpuAddrLo) = M(HudRowAddrLo);
            ammo_hud(g);
            break;
    }
}

void hud_update(Game *g) /* B0 $8055 */
{
    if (M(SceneState)) {
        return;
    }
    if (M(VerticalScrollMode)) {
        hud_scroll_lock(g);
        return;
    }
    if (M(FrameCounter) & 1) {
        return;
    }
    if (M(PpuBufferIndex) >= 0x30) {
        return;
    }
    if (!M(SideViewFlag)) {
        return;
    }
    switch ((M(FrameCounter) >> 2) & 3) {
        case 0:
            head(g, 0x0A, 0x21);
            string(g, 0);
            score(g, 0xC2);
            head(g, 0x0A, 0x61);
            string(g, 1);
            score(g, 0xC8);
            if (!M(WaveActive) && M(LevelNumber) == 1 && M(AreaNumber) == 1) {
                head(g, 4, 0x6E);
                string(g, 3);
            }
            weapon_hud(g);
            break;
        case 1:
            if (M(LevelNumber) == 5 && M(AreaNumber) == 1 && !M(CameraXLo) && (M(CameraXHi) & M(CameraYHi)) == 2 && M(CameraYLo) == 0x40) {
                put(0x88);
                put(0x2B);
                put(0xE0);
                put(0);
            } /* $80EA: 8 bytes of the attribute table cleared */
            put(8);
            put(M(HudPpuAddrHi));
            put(0x2C + M(HudPpuAddrLo));
            health_bar(g, M(TurtleHealth0 + M(CurrentTurtle)), 0);
            weapon_hud(g);
            break;
        case 2:
            if (M(LevelNumber) == 1 && M(AreaNumber) == 1) { /* $8146: bomb timer */
                head(g, 4, 0x6E);
                uint8_t a = M(BombTimerSeconds), y = 0;
                while (a >= 0x3C) {
                    a = (uint8_t)(a - 0x3C);
                    y++;
                }
                put(y ? y : 0x0A);
                put(0x2E);
                two_digits(g, a);
                return;
            }
            head(g, 8, 0x6C);
            health_bar(g, M(WaveActive) ? M(EntHealth + 5) : 0, 5);
            weapon_hud(g);
            break;
        case 3:
            ammo_hud(g);
            break;
    }
}
