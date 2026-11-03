/* Rope between the rooftops (level 3): player state $19 (B4 $A438) and scene $0B (B6 $A39B start, $A422 continuation).
 * The player walks on an anchor tile (terrain 7) with at least one rope ($A7): the section ($87) must be free ($03F7).
 * The scene scrolls the camera to its stop ($A63D), throws the rope (entity 1), extends it over the background via the PPU buffer,
 * then the turtle jumps onto the rope; $03E4 remembers the ropes laid (continuous drawing: B7 $E462, not ported yet). */
#include "game.h"
#include "stage_map.h"
#include "stage_scroll.h"
#include "hal.h"
#include "player.h"
#include "rope.h"
#include "sound.h"

static void clear_from(Game *g, int first) {
    for (int x = first; x < 16; x++) {
        clear_entity(g, x);
    }
} /* B7 $EF5E */

/* B4 $A4B1: position of the turtle in the zone (camera + X): A = low byte, Y = high byte */
static uint8_t zone_x(Game *g, uint8_t *hi) {
    unsigned s = M(CameraXLo) + M(EntX);
    *hi = (uint8_t)(M(CameraXHi) + (s > 0xFF));
    return (uint8_t)s;
}

/* B4 $A461: section number ($87) and return "not used yet" ($03F7) */
static int rope_segment_unused(Game *g) {
    uint8_t hi, lo = zone_x(g, &hi);
    if (M(LevelNumber) == 3) { /* $A479: level 4, up to 8 sections */
        uint8_t a;
        if (hi == 4) {
            a = 8;
        } else if (hi == 3) {
            a = lo < 0x70 ? 6 : 7;
        } else if (hi == 2) {
            a = lo < 0x70 ? 4 : 5;
        } else {
            a = 3;
        }
        M(RopeSection) = a;
    } else {
        M(RopeSection) = hi >= 4 ? 2 : 1;
    }
    return M(0x03F7 + ((M(RopeSection) - 1) >> 1)) == 0;
}

void rope_player_state(Game *g) /* B4 $A438 */
{
    if (!M(RopeCount) || (M(EntFlags) & 0x10)) {
        return;
    }
    if (!rope_segment_unused(g)) {
        return;
    }
    M(0x03F7 + ((M(RopeSection) - 1) >> 1))++;
    clear_vel_x(g, 0);
    clear_vel_y(g, 0);
    M(SceneId) = 0x0B;
}

/* B6 $A860 / $A87B: the camera scrolls 2 pixels and the turtle steps back by as much */
static void rope_scroll_right(Game *g) {
    M(CameraSpeedX) = 2;
    M(ScrollDirX) = 2;
    M(CameraSpeedXFrac) = 0;
    M(CurPrgBank) = 1;
    g->prg_bank = 1; /* B7 $D032 : SetPrgBankSaved(1), ScrollCameraX, SetPrgBankSaved(6) */
    scroll_camera_x(g);
    M(CurPrgBank) = 6;
    g->prg_bank = 6;
    if (M(EntX) >= 0x11) {
        M(EntX) = (uint8_t)(M(EntX) - 2);
    }
}

static void rope_scroll_left(Game *g) {
    M(ScrollDirX) = 1;
    M(CameraSpeedX) = 0xFE;
    M(CameraSpeedXFrac) = 0;
    M(CurPrgBank) = 1;
    g->prg_bank = 1; /* B7 $D032 : SetPrgBankSaved(1), ScrollCameraX, SetPrgBankSaved(6) */
    scroll_camera_x(g);
    M(CurPrgBank) = 6;
    g->prg_bank = 6;
    if (M(EntX) < 0xF0) {
        M(EntX) = (uint8_t)(M(EntX) + 2);
    }
}

void scene_rope_start(Game *g) /* B6 $A39B */
{
    clear_from(g, 1);
    M(EntAttr) = 0;
    M(CameraXLo) &= 0xFE;
    if (M(RopeSection) & 1) {
        M(EntFlags) &= 0xDA;
        rope_scroll_right(g);
    } else {
        M(EntFlags) |= 1;
        rope_scroll_left(g);
    }
    uint8_t y = (uint8_t)(M(RopeSection) * 2); /* $A3C9: the camera must be at the section's stop */
    if (R6(0xA63D + y) != M(CameraXLo) || R6(0xA63E + y) != M(CameraXHi)) {
        return;
    }
    M(PpuAddrLo) = R6(0xA5EA + M(RopeSection));
    M(PpuAddrHi) = 0x22;
    M(WaveState) = 1;
    M(DelayTimerLo) = 0;
    M(RopeCount)--;
    M(DelayTimerLo) = M(RopeSection) >= 7 ? 0x1A : 0x12;
    M(EntAnimTimer) = 2;
    M(EntAnimFrame) = 0;
    M(EntSprite + 1) = 0;
    M(EntFlags + 1) = M(EntFlags);
    M(SavedChrBank0) = M(ChrBank0);
    set_chr_bank0(g, 5);
    sound_play(g, 0x19);
    M(SceneId) |= 0x80; /* $A83B */
    M(SceneStep) = 0;
}

static void rope_gravity(Game *g) /* B4 $A5F0 */
{
    unsigned f = M(EntVelYFrac) + 0x54;
    M(EntVelYFrac) = (uint8_t)f;
    uint8_t a = (uint8_t)(M(EntVelY) + (f > 0xFF));
    if (a & 0x80) {
        if (a < 0xF9) {
            a = 0xF9;
        }
    } else if (a >= 9) {
        a = 9;
    }
    M(EntVelY) = a;
}

void scene_rope_continue(Game *g) /* B6 $A422 */
{
    switch (M(SceneStep)) {
        case 0: { /* $A42F: the turtle throws the rope */
            if (--M(EntAnimTimer) == 0) {
                M(EntAnimTimer) = 2;
                M(EntAnimFrame)++;
            }
            uint8_t y = M(EntAnimFrame);
            M(EntSprite) = R6(0xA5A7 + y);
            M(EntSprite + 1) = R6(0xA5C5 + y);
            uint8_t i = (uint8_t)((R6(0xA5C5 + y) - 0x84) << 1);
            uint8_t off = R6(0xA5E3 + i);
            uint8_t px = (uint8_t)(M(EntX) + off); /* $A45B: "bne" only tests the result of the addition */
            if ((M(EntFlags) & 1) || px == 0) {
                px = (uint8_t)(M(EntX) - off);
            }
            M(EntX + 1) = px;
            M(EntY + 1) = (uint8_t)(R6(0xA5E4 + i) + M(EntY));
            if (y < 0x1D) {
                return;
            }
            M(EntX + 1) = M(EntX); /* $A47E: the rope leaves */
            M(EntVelX + 1) = (M(RopeSection) & 1) ? 0x08 : 0xF8;
            M(EntSprite + 1) = 0x88;
            M(EntY + 1) = 0x9A;
            sound_play(g, 0x1B);
            M(SceneStep)++;
            break;
        }
        case 1: { /* $A4A2: the rope runs to the edge */
            move_entity_x(g, 1);
            uint8_t y = M(EntX + 1), a = M(RopeSection);
            int done;
            if (a == 7) {
                done = y >= 0x20;
            } else if (a == 8) {
                done = y < 0xD9;
            } else if (a & 1) {
                done = y >= 0x50;
            } else {
                done = y < 0xB1;
            }
            if (done) {
                M(SceneStep)++;
            }
            break;
        }
        case 2: {                                /* $A4DC: the rope is drawn on the background */
            if ((int8_t)--M(DelayTimerLo) < 0) { /* $A519: end of the drawing */
                uint8_t y = (uint8_t)(((M(RopeSection) - 1) << 2) & 0x18);
                for (int x = 0; x < 8; x++) {
                    uint8_t v = R6(0xA61F + y + x);
                    if (v != 0xFF) {
                        M(0x0748 + x) = v; /* terrain: the rope carries the turtle */
                    }
                }
                M(EntVelX) = (M(RopeSection) & 1) ? 2 : 0xFE;
                M(EntVelY) = 0xF9;
                M(EntSprite) = 0x10;
                M(EntAnimId) = 0x85;
                M(EntAnimFrame) = 0;
                M(EntSprite + 1) = 0;
                M(EntFlags + 1) = 0;
                M(EntFlags) |= 0x10;
                M(SceneStep)++;
                sound_play(g, 0x1C);
                break;
            }
            move_entity_x(g, 1);
            ppu_buffer_append(g, 1);
            ppu_buffer_append(g, M(PpuAddrHi));
            ppu_buffer_append(g, M(PpuAddrLo));
            uint8_t t = M(RopeSection) >= 7 ? R6(0xA605 + M(DelayTimerLo)) : R6(0xA5F3 + M(DelayTimerLo));
            ppu_buffer_append(g, t);
            M(PpuAddrLo)++;
            if (!(M(RopeSection) & 1)) {
                M(PpuAddrLo)--;
                M(PpuAddrLo)--;
            }
            M(PpuAddrLo) = (uint8_t)((M(PpuAddrLo) & 0x1F) | 0x40);
            break;
        }
        case 3: /* $A56B: the turtle crosses the rope by jumping */
            rope_gravity(g);
            move_entity_x(g, 0);
            move_entity_y(g, 0);
            if (M(EntVelY) & 0x80) {
                return;
            }
            M(RopesLaidMask) |= R6(0xA5A2 + ((M(RopeSection) + 1) >> 1));
            M(EntStatus) = 0;
            M(EntState) = 0;
            set_chr_bank0(g, M(SavedChrBank0));
            M(0x03FB) = 1;
            M(SceneStep) = 0; /* $A846: end of the scene */
            M(SceneId) = 0;
            break;
    }
}

/* ---------------------------------------------------------------------------------------------------------------------
 * Permanent rope drawing (B7 $E415, called every frame when $03E4 != 0): as long as the camera intersects the rope,
 * its tiles are rewritten into the PPU buffer and the terrain ($0748-$074F) makes it walkable. Level 3: zone 4 only. */
static int rope_overlap(Game *g) /* B7 $E4B6: carry = the camera intersects the rope ($00:$01 to $02:$03) */
{
    unsigned cam = M(CameraXHi) << 8 | M(CameraXLo), end = M(Tmp2) << 8 | M(Tmp3), start = M(Tmp0) << 8 | M(Tmp1);
    if (cam >= end) {
        return 0;
    }
    return cam + 0x100 >= start;
}

static void rope_draw(Game *g) /* B7 $E4E7: $07 = draw, $12 = rope number */
{
    M(TmpE) = 0;
    M(Tmp9) = M(Tmp1);
    M(TmpD) = (uint8_t)(M(Tmp1) - M(CameraXLo));
    int ge = (M(Tmp0) << 8 | M(Tmp1)) >= (M(CameraXHi) << 8 | M(CameraXLo));
    M(Tmp8) = M(Tmp0);
    if (!ge) { /* the start of the rope is before the camera */
        M(TmpE) = M(TmpD);
        M(Tmp9) = M(CameraXLo);
        M(Tmp8) = M(CameraXHi);
    }
    uint8_t y = M(Tmp12);
    M(Tmp10) = R7(0xE679 + y);
    M(Tmp11) = R7(0xE67A + y);
    unsigned right = (M(CameraXHi) << 8 | M(CameraXLo)) + 0xF0;
    M(TmpC) = (uint8_t)right;
    M(TmpD) = (uint8_t)(right >> 8);
    M(TmpB) = M(Tmp3);
    M(TmpA) = M(Tmp2);
    if ((unsigned)(M(Tmp2) << 8 | M(Tmp3)) >= right) {
        M(TmpB) = M(TmpC);
        M(TmpA) = M(TmpD);
    }
    if (M(Tmp7)) {
        M(Tmp9) &= 0xF8;
        M(TmpB) &= 0xF8;
        M(TmpC) = (uint8_t)(M(TmpB) - M(Tmp9));
        if ((M(TmpA) << 8 | M(TmpB)) < (M(Tmp8) << 8 | M(Tmp9))) {
            return;
        }
        M(Tmp16) = M(PpuBufferIndex);
        M(TmpC) = (uint8_t)((uint8_t)(M(TmpC) + 8) >> 3);
        M(TmpD) = M(TmpC);
        ppu_buffer_append(g, M(TmpC));
        ppu_buffer_append(g, 0x22);
        M(Tmp15) = (uint8_t)((M(Tmp9) >> 3) + 0x40);
        ppu_buffer_append(g, M(Tmp15));
        uint16_t tiles = (uint16_t)(M(Tmp10) | M(Tmp11) << 8);
        uint8_t ty = (uint8_t)(((uint8_t)(~M(TmpE) + 1)) >> 3);
        for (unsigned iterations = 0;; iterations++) { /* $E589 */
            LOOP_GUARD(iterations)
            ppu_buffer_append(g, R7(tiles + ty));
            M(Tmp15)++;
            if (M(Tmp15) >= 0x60) { /* the row of 32 tiles is full: new entry */
                M(TmpD)--;
                M(PpuBuffer + M(Tmp16)) = (uint8_t)(M(TmpC) - M(TmpD));
                ppu_buffer_append(g, M(TmpD));
                ppu_buffer_append(g, 0x22);
                ppu_buffer_append(g, 0x40);
                M(Tmp15) = 0x40;
                ty++;
                if (!M(TmpD)) {
                    break;
                }
                continue;
            }
            ty++;
            if (--M(TmpD) == 0) {
                break;
            }
        }
    }
    M(Tmp9) = (uint8_t)(M(Tmp9) + 0x10); /* $E5C2 : terrain */
    uint8_t x = (uint8_t)(M(Tmp9) >> 5);
    M(TmpD) = M(TmpC) >> 1;
    if (!M(TmpD)) {
        return;
    }
    M(TmpD)--;
    M(TmpF) = M(TmpD);
    if (!M(TmpD)) {
        return;
    }
    int low_first = (M(Tmp9) & 0x10) != 0; /* $E5E4 */
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        if (!low_first) {
            uint8_t a = M(0x0748 + x) & 0x0F;
            M(0x0748 + x) = M(Tmp7) ? (uint8_t)(a | 0x80) : a;
            if (--M(TmpD) == 0) {
                break;
            }
        }
        low_first = 0;
        uint8_t a = M(0x0748 + x) & 0xF0;
        M(0x0748 + x) = M(Tmp7) ? (uint8_t)(a | 0x08) : a;
        x = (uint8_t)((x + 1) & 7);
        if (--M(TmpD) == 0) {
            break;
        }
    }
    if (M(LevelNumber) == 2) {
        M(Tmp5) = 0x60;
        M(Tmp6) = 3;
    } /* $E621 */
    if (M(Tmp12) != 2) {
        return; /* $E635: rope number 2 */
    }
    M(Tmp5) = 0xA8;
    M(Tmp6) = 3;
    if ((M(CameraXHi) << 8 | M(CameraXLo)) < 0x03A8) {
        return;
    }
    int n = 0x0F - M(TmpF);
    if (n <= 0) {
        return;
    }
    M(TmpF) = (uint8_t)n;
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        M(0x0748 + x) &= 0x0F;
        if (--M(TmpF) == 0) {
            return;
        }
        M(0x0748 + x) &= 0xF0;
        x = (uint8_t)((x + 1) & 7);
        if (--M(TmpF) == 0) {
            return;
        }
    }
}

void rope_redraw(Game *g) /* B7 $E415 */
{
    if (!M(RopesLaidMask)) {
        return;
    }
    if (M(CameraYLo) | M(CameraYHi)) {
        return;
    }
    if (M(LevelNumber) == 3) { /* $E462: level 4, zone 0: up to 3 ropes ($E671: flags, $E675: numbers) */
        if (M(AreaNumber) || !(M(RopesLaidMask) & 0x0E)) {
            return;
        }
        for (uint8_t i = 3; i > 0; i--) {
            M(Tmp17) = i;
            M(Tmp7) = (R7(0xE671 + i) & M(RopesLaidMask)) ? 1 : 0;
            M(Tmp0) = R7(0xE4D7 + i * 2);
            M(Tmp1) = R7(0xE4D8 + i * 2);
            M(Tmp2) = R7(0xE4DF + i * 2);
            M(Tmp3) = R7(0xE4E0 + i * 2);
            if (!rope_overlap(g)) {
                continue;
            }
            M(Tmp12) = R7(0xE675 + i);
            rope_draw(g);
            M(WaveState) = 0;
        }
        return;
    }
    if (M(LevelNumber) != 2 || M(AreaNumber) != 4 || !(M(RopesLaidMask) & 1)) {
        return;
    }
    M(Tmp7) = 1;
    M(Tmp0) = R7(0xE4D7);
    M(Tmp1) = R7(0xE4D8);
    M(Tmp2) = R7(0xE4DF);
    M(Tmp3) = R7(0xE4E0);
    if (!rope_overlap(g)) {
        return;
    }
    M(Tmp12) = 0;
    rope_draw(g);
    M(WaveState) = 0;
}
