/* NMI (B7 $C3A3): OAM DMA, PPU buffer, sprite drawing, game logic. And DrawSprites ($C005-$C22B). */
#include <string.h>
#include "game.h"
#include "nmi.h"
#include "sound.h"
#include "title.h"

#define Tmp4_ (Tmp0 + 4)
#define Tmp5_ (Tmp0 + 5)
#define Tmp6_ (Tmp0 + 6)
#define Tmp7_ (Tmp0 + 7)
#define OAM(i) M(OamBuffer + (i))

/* B7 $C395 MainLoop: between two frames the console spins in this loop (inc $45, a = ($45 + $1E) eor $1E) a number of times
 * that depends on the free time of the frame (of the order of 1000 laps). Only this number modulo 256 matters; the port cannot count
 * the console's cycles, so it draws it from a function of the frame: same law as the console, different sequence, replayable. */
void rng_idle(Game *g) {
    uint32_t h = g->frame * 2654435761u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    uint8_t s = M(RandomSeed), fc = M(FrameCounter);
    for (unsigned n = 1024 + (h & 0xFF); n; n--) {
        s = (uint8_t)((uint8_t)(s + 1 + fc) ^ fc);
    }
    M(RandomSeed) = s;
}

void blank_ppu(Game *g) /* $C4CB BlankPPU (+ DisableNMI $C4A6) */
{
    M(PpuCtrlShadow) &= 0x7F;
    ppu_write(g, 0, M(PpuCtrlShadow));
    ppu_write(g, 6, 0);
    ppu_write(g, 6, 0);
    ppu_write(g, 1, M(PpuMaskShadow) & 0xE7);
}

static void set_scroll(Game *g) /* $C460 SetScroll */
{
    if (M(DisplayDelay)) {
        return;
    }
    ppu_read_status(g);
    ppu_write(g, 6, 0x20);
    ppu_write(g, 6, 0x00);
    ppu_read_status(g);
    ppu_write(g, 5, M(ScrollX));
    ppu_write(g, 5, (M(StageState) >= 3 || rom_is_us(g->rom)) ? (uint8_t)(M(ScrollY) - 4) : M(ScrollY));
    M(PpuCtrlShadow) |= 0x80;
    ppu_write(g, 0, M(PpuCtrlShadow));
}

void flush_ppu_buffer(Game *g) /* $C26C FlushPpuBuffer */
{
    if (M(PpuBufferIndex) < 0x28 && M(PaletteDirty)) {
        M(PaletteDirty) = 0;
        ppu_write(g, 0, M(PpuCtrlShadow) & 0x7B);
        ppu_read_status(g);
        ppu_write(g, 6, 0x3F);
        ppu_write(g, 6, 0x00);
        for (int y = 0; y < 0x20; y++) {
            ppu_write(g, 7, M(PaletteBuffer + y));
        }
        ppu_write(g, 6, 0x3F);
        ppu_write(g, 6, 0x00);
        ppu_write(g, 6, 0x00);
        ppu_write(g, 6, 0x00);
    }
    uint8_t y = 0, x;
    int guard = 0;
    while ((x = M(PpuBuffer + y)) != 0) {
        if (++guard > 400) { /* endless buffer (ROM misread): do not block */
            TRACE("f=%u TODO endless PPU buffer, index=%02X\n", g->frame, M(PpuBufferIndex));
            for (int i = 0; i < 256; i += 16) {
                TRACE("f=%u   buf %02X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", g->frame, i, M(PpuBuffer + i),
                      M(PpuBuffer + i + 1), M(PpuBuffer + i + 2), M(PpuBuffer + i + 3), M(PpuBuffer + i + 4), M(PpuBuffer + i + 5), M(PpuBuffer + i + 6),
                      M(PpuBuffer + i + 7), M(PpuBuffer + i + 8), M(PpuBuffer + i + 9), M(PpuBuffer + i + 10), M(PpuBuffer + i + 11), M(PpuBuffer + i + 12),
                      M(PpuBuffer + i + 13), M(PpuBuffer + i + 14), M(PpuBuffer + i + 15));
            }
            break;
        }
        uint8_t hi = M(PpuBuffer + (uint8_t)(y + 1)), lo = M(PpuBuffer + (uint8_t)(y + 2));
        if (x & 0x80) { /* $C2E0: fill, 4 bytes */
            ppu_write(g, 0, M(PpuCtrlShadow));
            ppu_write(g, 6, hi);
            ppu_write(g, 6, lo);
            uint8_t v = M(PpuBuffer + (uint8_t)(y + 3));
            for (int n = x & 0x7F; n > 0; n--) {
                ppu_write(g, 7, v);
            }
            y = (uint8_t)(y + 4);
            continue;
        }
        ppu_write(g, 0, (uint8_t)(((hi & 0x80) ? (M(PpuCtrlShadow) | 4) : M(PpuCtrlShadow)) & 0x7F));
        ppu_write(g, 6, hi);
        ppu_write(g, 6, lo);
        y = (uint8_t)(y + 3);
        for (; x > 0; x--) {
            ppu_write(g, 7, M(PpuBuffer + y++));
        }
    }
    M(PpuBuffer) = 0; /* $C303 : stx PpuBuffer / PpuBufferIndex */
    M(PpuBufferIndex) = 0;
    ppu_write(g, 0, M(PpuCtrlShadow) | 0x80);
}

/* --- DrawSprites: OAM ($0200) from the entities --- */
static uint16_t metasprite_ptr(Game *g, uint8_t x, uint8_t sprite) {
    uint8_t y = (uint8_t)(sprite << 1);
    if (!(sprite & 0x80)) { /* $C09B */
        if ((M(0x03E7) != 0 && x == M(0x03E7)) || x == 1) {
            return rom_word(g->rom, B2, (uint16_t)(0x86F7 + y));
        }
        return rom_word(g->rom, B2, (uint16_t)(0x8001 + y));
    }
    uint16_t tbl = rom_word(g->rom, 7, (uint16_t)(0xC22C + M(ChrBank0) * 2));
    return rom_word(g->rom, B2, (uint16_t)(tbl + y));
}

static uint8_t step_slot(Game *g, uint8_t x) /* txa / clc / adc #$C4 ... cmp $09 / bcc */
{
    do {
        x = (uint8_t)(x + 0xC4);
    } while (x < M(Tmp9));
    return x;
}

static uint8_t ms_byte(Game *g, uint16_t a) /* metasprite data: bank 2 at $8000, but an invalid pointer may point to RAM (mirrors) or to the fixed bank */
{
    if (a < 0x2000) {
        return g->ram[a & 0x7FF];
    }
    if (a >= 0xC000) {
        return rom_prg(g->rom, 7, a);
    }
    return rom_prg(g->rom, B2, a);
}

static void draw_metasprite(Game *g) /* $C0C3 DrawMetasprite */
{
    uint8_t x = M(TmpC);
    M(Tmp5_) = M(EntFlags + x);
    if (!(M(Tmp5_) & 2)) {
        return;
    }
    uint8_t sprite = M(EntSprite + x);
    if (!sprite) {
        return;
    }
    uint16_t p = metasprite_ptr(g, x, sprite);
    M(Tmp2_) = M(EntY + x);
    M(Tmp3_) = M(EntX + x);
    M(Tmp4_) = M(EntAttr + x);
    uint8_t y = 0;
    uint8_t count = ms_byte(g, p);
    y++;
    M(Tmp6_) = count;
    uint8_t ox = M(TmpB);
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        uint8_t a = ms_byte(g, (uint16_t)(p + y++));
        int c1 = a >= 0x80;
        int c2 = a & 1; /* bit 0: "same attribute as the previous sprite" */
        a = (uint8_t)((c1 << 7) | (a >> 1));
        OAM(ox) = (uint8_t)(a + M(Tmp2_));
        OAM(ox)--;
        OAM(ox + 1) = ms_byte(g, (uint16_t)(p + y++));
        if (!c2) {
            M(Tmp7_) = ms_byte(g, (uint16_t)(p + y++));
        }
        uint8_t at = (M(Tmp4_) & 3) ? (M(Tmp7_) & 0xE0) : M(Tmp7_);
        at ^= M(Tmp4_);
        int flip = M(Tmp5_) & 1;
        OAM(ox + 2) = flip ? (uint8_t)(at ^ 0x40) : at;
        uint8_t off = ms_byte(g, (uint16_t)(p + y++));
        if (flip) {
            off = (uint8_t)(0xF8 - off); /* eor #$FF / sbc #$07 with C=1 */
        }
        M(Tmp17) = off; /* $C14F */
        uint8_t tx = M(Tmp3_);
        int visible, sum = tx + off;
        if (tx < 0x80) {
            visible = (off < 0x80) ? 1 : (sum > 0xFF);
        } else {
            visible = (off >= 0x80) ? 1 : !(sum > 0xFF);
        }
        if (visible) {
            OAM(ox + 3) = (uint8_t)sum;
            if (M(TmpA)) {
                ox = (uint8_t)(ox + 4);
            } else {
                ox = step_slot(g, ox);
            }
            if (--M(Tmp8) == 0) {
                return; /* no room left: $0B is not updated */
            }
        }
        if (--M(Tmp6_) == 0) {
            M(TmpB) = ox;
            return;
        }
    }
}

static void hud_countdown_sprites(Game *g) /* $C190: countdown bar ($3C != 0) */
{
    if (!M(0x3C)) {
        return;
    }
    uint8_t ycur = 0xC1, x = 0;
    M(Tmp16) = ycur; /* $C196 */
    do {
        OAM(x) = ycur;
        OAM(x + 1) = 0x0F;
        OAM(x + 2) = 0x03;
        OAM(x + 3) = 0xF8;
        int borrow = ycur < 0x10;
        ycur = (uint8_t)(ycur - 0x10);
        if (borrow) {
            ycur = (uint8_t)(ycur - 0x10);
        }
        M(Tmp16) = ycur; /* $C1B7 */
        x = (uint8_t)(x + 4);
        M(Tmp8)--;
    } while (x < 0x3C);
    M(TmpB) = x;
}

/* $C1FA: y sprites of the health bar (tile t), 8 pixels apart */
static void status_run(Game *g, uint8_t tile, uint8_t y) {
    uint8_t x = M(TmpB);
    M(Tmp11) = tile;
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        OAM(x) = 0xD0;
        OAM(x + 1) = M(Tmp11);
        OAM(x + 2) = 0x03;
        OAM(x + 3) = M(Tmp13);
        M(Tmp13) = (uint8_t)(M(Tmp13) + 8);
        x = step_slot(g, x);
        M(Tmp12)--;
        if (--M(Tmp8) == 0) {
            break;
        }
        if (--y == 0) {
            break;
        }
    }
    M(TmpB) = x;
}

/* $C1C6: health sprites (at the top of the screen in top view) */
static void status_sprites(Game *g) {
    if (M(0x03D9)) {
        return;
    }
    M(Tmp12) = 8;
    M(Tmp13) = 0x60;
    uint8_t y = M(EntVar0 + 15) & 0x0F;
    if (y) {
        status_run(g, 0xAA, y);
        if (!M(Tmp8)) {
            return;
        }
    }
    if (M(EntVar0 + 15) & 0x80) {
        status_run(g, 0xAC, 1);
        if (!M(Tmp8)) {
            return;
        }
    }
    if (M(Tmp12) == 0) {
        return;
    }
    status_run(g, 0xAE, M(Tmp12));
}

static void draw_sprites(Game *g) /* $C005 DrawSprites */
{
    ROM_TABLE(order, 4, 7, 0xC097); /* $C097 */
    M(Tmp8) = 0x40;
    M(TmpB) = 0;
    M(TmpC) = 0;
    if (M(PauseFlag)) {
        for (int x = 0; x < 256; x += 4) {
            OAM(x) = 0xF4;
        }
        return;
    }
    hud_countdown_sprites(g);
    if (M(StageState) == 6 && (M(FrameCounter) & 1)) {
        return;
    }
    M(TmpA) = 1;
    M(TmpD) = 0;
    draw_metasprite(g); /* the players, in order */
    M(Tmp9) = (uint8_t)((0x40 - M(Tmp8)) << 2);
    M(TmpA) = 0;
    uint8_t a = M(0x2D);
    do {
        a = (uint8_t)(a + 0x3C);
    } while (a < M(Tmp9));
    M(0x2D) = a;
    M(TmpB) = a;
    M(TmpC) = 0x0F;
    M(TmpD) = order[(M(FrameCounter) >> 1) & 3];
    do {
        draw_metasprite(g);
        if (M(Tmp8) == 0) {
            return;
        }
        M(TmpD) = (uint8_t)((M(TmpD) + 1) & 0x0F);
        if (M(TmpD) == 0) {
            M(TmpD)++;
        }
    } while (--M(TmpC) != 0);
    status_sprites(g);
    if (M(Tmp8) == 0) {
        return;
    }
    uint8_t x = M(TmpB);
    do {
        OAM(x) = 0xF4;
        x = step_slot(g, x);
    } while (--M(Tmp8) != 0);
}

/* $C54A: copy of a spare PPU buffer */
static void swap_backup_buffer(Game *g) {
    if (M(VerticalScrollMode)) {
        return;
    }
    if (M(AltPpuBuffer)) {
        for (int y = 0x60; y > 0; y--) {
            M(OamBuffer + 0xFF + y) = M(0x035F + y);
        }
        M(PpuBufferIndex) = M(AltBufferIndex);
        M(AltBufferIndex) = 0;
        M(AltPpuBuffer) = 0;
    }
}

/* $F4A0 */
static void update_hud_flag(Game *g) {
    M(0x03D9) = 1;
    if (M(GameMode) == 4 && M(StageState) < 8 && !M(SideViewFlag) && !M(PauseFlag)) {
        M(0x03D9) = 0;
    }
}

/* $C413: scroll values for the next NMI */
static void prepare_scroll(Game *g) {
    if (M(VerticalLevelFlag)) {
        if (!M(PauseFlag)) {
            M(PpuCtrlShadow) = (uint8_t)((M(PpuCtrlShadow) & 0xFE) | (M(CameraXHi) & 1));
        }
        M(ScrollX) = M(ScrollXShadow);
        M(ScrollY) = M(ScrollYShadow);
        return;
    }
    M(NametableBitDelay2) = M(NametableBitDelay1);
    M(NametableBitDelay1) = M(NametableBit);
    M(ScrollY) = M(ScrollYDelayed);
    M(ScrollX) = M(ScrollXDelayed);
    M(ScrollYDelayed) = M(ScrollYShadow);
    M(ScrollXDelayed) = M(ScrollXShadow);
    M(PpuCtrlShadow) = (uint8_t)((M(PpuCtrlShadow) & 0xFD) | M(NametableBitDelay2));
}

/* B7 $C4DE ReadControllers: rising edges; JoyPressed1 also keeps the presses of the previous frame */
void read_controllers(Game *g) {
    uint8_t old_pressed2 = M(JoyPressed2), cur = g->pad;
    M(Tmp6) = old_pressed2;  /* $C4DE */
    M(Tmp0) = M(Tmp2) = cur; /* $C4EE/$C525: leftovers of the reads of the two controllers in the zero page */
    M(Tmp1) = M(Tmp3) = 0;   /* (bad sprite reads see them: player sprite $BD) */
    M(Tmp4) = (uint8_t)(0x40 | (cur & 1));
    M(Tmp5) = 0x40;
    uint8_t pressed = (uint8_t)((cur ^ M(JoyPrev1)) & cur);
    M(JoyPressed1) = M(JoyPressedRaw1) = pressed;
    M(JoyHeld1) = M(JoyPrev1) = cur;
    M(JoyPressed2) = M(JoyPressedRaw2) = 0; /* controller 2: no button */
    M(JoyHeld2) = M(JoyPrev2) = 0;
    M(JoyPressed2) = M(JoyPressed1);
    M(JoyPressed1) |= old_pressed2;
}

void game_init(Game *g) /* B7 $C352 InitGame + EnableDisplay */
{
    apu_write(g, 0x15, 0x0F); /* InitAPU */
    apu_write(g, 0x17, 0xC0);
    memset(g->ram, 0, 0xF9);
    memset(g->ram + 0x100, 0, 0x700);
    g->chr_bank0 = 0;
    g->chr_bank1 = 0x0F;
    M(ChrBank0) = 0;
    M(ChrBank1) = 0x0F;
    M(PpuCtrlShadow) = 0xB0;
    ppu_write(g, 0, 0xB0);
    M(PpuMaskShadow) = 0x18;
    M(DisplayDelay) = 5;
    sound_init(g); /* InitGame calls SoundInit just before EnableDisplay */
}

void nmi_frame(Game *g) /* $C3CE NMI_RunFrame (without the sound) */
{
    M(NmiReentryFlag) = 0; /* $C3CE: NMI clears the re-entry flag */
    g->frame_vram_bytes = g->frame_lag_updates = 0;
    memcpy(g->ppu.oam, &g->ram[OamBuffer], 256); /* OAM DMA */
    blank_ppu(g);
    flush_ppu_buffer(g);
    uint8_t mask = M(PpuMaskShadow);
    if (M(DisplayDelay) && --M(DisplayDelay)) {
        mask &= 0xE7;
    }
    ppu_write(g, 1, mask);
    set_scroll(g);
    sound_update(g);
    swap_backup_buffer(g);
    update_hud_flag(g);
    draw_sprites(g);
    g->ppu.split_on = 0;
    g->ppu_writes_acc = 0; /* only the PPU writes of the logic are measured */
    run_game_mode(g);
    read_controllers(g);
    prepare_scroll(g);
    M(PpuBuffer + M(PpuBufferIndex)) = 0;
    M(PpuBufferIndex)++;
}
