/* Bosses $75-$78 of the vertical zone of level 5. */
#include "game.h"
#include "anim.h"
#include "hal.h"
#include "player.h"
#include "sound.h"
#include "enemy.h"
#include "enemies/patrollers.h"
#include "enemies/purple_ninja.h"
#include "enemies/spydrone.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- bosses $75-$78 of the vertical zone of level 5 (B5 $B50E-$BA0E): $75 is the boss (entity 5, it drives the camera), its elements are $75 (entities 6 to 10), $76 turrets, $77 fragments, $78 the shots --- */
static void core_scroll_entity(Game *g, int x) /* B5 $B54A: the entity follows the camera; it becomes visible or invisible when crossing the edge */
{
    unsigned f = M(EntXFrac + x) + M(EntVelXFrac + 5);
    M(EntXFrac + x) = (uint8_t)f;
    uint8_t old = M(EntX + x);
    uint8_t nw = (uint8_t)(old + M(EntVelX + 5) + (f > 0xFF));
    M(EntX + x) = nw;
    if (!((old ^ nw) & 0x80)) {
        return;
    }
    if (nw & 0x80) {
        if (!(M(EntVelX + 5) & 0x80)) {
            return;
        }
    } else if (M(EntVelX + 5) & 0x80) {
        return;
    }
    M(EntFlags + x) ^= 2;
}

static void core_scroll_all(Game *g) /* B5 $B522 */
{
    int x = 5;
    core_scroll_entity(g, x);
    uint8_t d = (uint8_t)(M(EntX + 5) - M(CameraSpeedX));
    if (d) {
        M(CameraDeltaX) = d;
        M(CameraSpeedX) = M(EntX + 5);
    }
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        x++;
        if (x < 0x0B || M(EntType + x) == 0x77) {
            core_scroll_entity(g, x);
        }
        if (x >= 0x0F) {
            break;
        }
    }
}

static void core_set_map_edges(Game *g, uint8_t y) /* B5 $B6A2: edges of the collision map according to the scrolling direction */
{
    uint8_t a = R5(0xB6C7 + y);
    for (int i = 0; i < 5; i++) {
        M(0x07CA + i) = a;
    }
    M(0x07C9) = M(0x07D1) = R5(0xB6CA + y);
    M(0x07CF) = M(0x07D7) = R5(0xB6CD + y);
}

static void core_set_speed(Game *g, uint8_t y) /* B5 $B67F */
{
    M(EntVelXFrac + 5) = R5(0xB69C + y);
    M(EntVelX + 5) = R5(0xB69D + y);
}

static void core_camera_path(Game *g) /* B5 $B620: the boss brings the camera to the next point of the route ($B68C) */
{
    if (M(VerticalLevelFlag) & 0x0F) {
        core_set_speed(g, 0);
        core_set_map_edges(g, 0);
        return;
    }
    uint8_t y = (uint8_t)(M(EntWait + 5) << 1);
    uint8_t thi = R5(0xB68D + y), tlo = R5(0xB68C + y);
    if (thi == M(CameraXHi) && tlo == M(CameraXLo)) {
        M(EntWait + 5) = (uint8_t)((M(EntWait + 5) + 1) & 7);
        return;
    }
    int above = thi > M(CameraXHi) || (thi == M(CameraXHi) && tlo > M(CameraXLo));
    if (above) { /* $B649: to the right */
        if ((M(CameraXHi) & 1) && M(ScrollXShadow) >= 0x80) {
            core_set_speed(g, 0);
            core_set_map_edges(g, 0);
            return;
        }
        core_set_speed(g, 2);
        core_set_map_edges(g, 2);
        return;
    }
    if (!(M(CameraXHi) & 1) && M(ScrollXShadow) < 0x80) {
        core_set_speed(g, 0);
        core_set_map_edges(g, 0);
        return;
    } /* $B65F: to the left */
    core_set_speed(g, 4);
    core_set_map_edges(g, 1);
}

void core_palette_swap(Game *g, int x) /* B5 $BD24: circular permutation of three palette colours */
{
    uint8_t a0 = M(PaletteBuffer + 1 + x), a1 = M(PaletteBuffer + 2 + x), a2 = M(PaletteBuffer + 3 + x);
    M(PaletteBuffer + 2 + x) = a0;
    M(PaletteBuffer + 3 + x) = a1;
    M(PaletteBuffer + 1 + x) = a2;
    M(PaletteDirty) = 1;
}

static void core_palette_twinkle(Game *g, int x) /* B5 $B72E: the palette twinkles; x may be $10 after the boss's death */
{
    ROM_TABLE(tbl, 4, 5, 0xB76B); /* $B76B */
    M(0x05BA) = tbl[((M(FrameCounter) >> 1) & 7) >> 1];
    if (M(EntState + x)) {
        return;
    }
    uint8_t y = (uint8_t)(M(EntVar1 + x) + 1);
    if (y >= 3) {
        y = 0;
    }
    M(EntVar1 + x) = y;
    if (y) {
        return;
    }
    if (!(M(EntVelXFrac + x) | M(EntVelX + x) | M(EntState + x))) {
        return;
    }
    core_palette_swap(g, 0x0C);
    if (M(EntVelX + 5) & 0x80) {
        core_palette_swap(g, 0x0C);
    }
}

static void core_explode(Game *g, int x) /* B5 $B9E0 */
{
    M(EntAnimId + x) = 0xD3;
    M(EntType + x) = 0x15;
    M(EntFlags + x) = 0x82;
    M(EntAnimTimer + x) = 2;
    M(EntHealth + x) = 0;
    M(EntAnimFrame + x) = 0;
    M(EntState + x) = 0;
    uint8_t snd = 0x42;
    if (x == 5) {
        M(SceneSubTimer) = 0x70;
        snd = 0x48;
    }
    sound_play(g, snd);
}

static int core_hit_check(Game *g, int x) /* B5 $B95D: carry = destroyed */
{
    if (!(M(EntWait + x) & 0x80)) {
        return 0;
    }
    if (x == 5 && !M(TurtleHealth0 + M(CurrentTurtle))) { /* the turtle is KO: the boss gets up */
        M(EntHealth + x) = 4;
        M(EntWait + x) = 0;
        return 0;
    }
    M(EntState + x) = 1;
    M(EntVar0 + x) = 0x28;
    M(VerticalLevelFlag) |= R5(0xB954 + x);
    return 1;
}

static int core_die_step(
    Game *g, int x,
    uint8_t y) /* B5 $B992: the element goes out, the background tiles empty in steps; returns the X register on output ($22: end of the PPU buffer) */
{
    if (--M(EntVar0 + x) == 0) {
        M(VerticalLevelFlag) &= (uint8_t)~R5(0xB954 + x);
        if (M(EntFlags + x) & 2) {
            core_explode(g, x);
        } else {
            M(EntType + x) = 0;
            M(EntHealth + x) = 0;
        }
    }
    if (!(M(EntVar0 + x) & 1)) {
        y++;
    }
    uint16_t t = rom_word(g->rom, 5, (uint16_t)(0xBA0E + (uint8_t)(y << 1)));
    uint8_t n = R5(t);
    for (int i = 1; i <= n; i++) {
        ppu_buffer_append(g, R5(t + i));
    }
    return M(PpuBufferIndex);
}

static int core_die(Game *g, int x, uint8_t y) /* B5 $B98C: one frame out of four */
{
    if (M(FrameCounter) & 3) {
        return x;
    }
    return core_die_step(g, x, y);
}

uint8_t core_aim(Game *g, int x) /* B5 $BAE6: direction (0-7) of entity x towards the player */
{
    M(Tmp0) = M(EntX);
    M(Tmp1) = M(EntY);
    vector_to(g, x);
    uint8_t a = R5(0xA8D6 + ((M(Tmp8) << 3) + M(Tmp9)));
    uint8_t c = (a >> 1) & 1;
    a >>= 2;
    return (uint8_t)((a + c - 2) & 7); /* adc #0: only the carry of the last lsr counts */
}

static int core_main(Game *g, int x) /* B5 $B6D0: the boss (entity 5); returns the X register on output ($10 after the death) */
{
    if (M(EntState + x)) {
        return core_die(g, x, 2);
    }
    uint8_t y = 0;
    while (M(FrameCounter) >= R5(0xB725 + y)) {
        y++;
    }
    set_animation(g, x, R5(0xB721 + y));
    core_hit_check(g, x);
    if (!(M(EntWait + 5) & 0x80)) {
        return x;
    }
    sound_init(g);
    sound_play(g, 0x45);
    for (int e = 6; e < 0x10; e++) {
        uint8_t t = M(EntType + e);
        if (!t) {
            continue;
        }
        if (t == 0x98 || (!M(EntState + e) && e >= 9)) {
            core_explode(g, e);
            continue;
        }
        if (M(EntState + e)) {
            continue;
        }
        M(EntWait + e) = 0xFF;
        M(EntHealth + e) = 0;
    }
    return 0x10;
}

static void core_element(Game *g, int x) /* B5 $B776: elements of the boss (entities 6 to 10) */
{
    if (x == 6) {
        if (M(EntState + x)) {
            core_die(g, x, 0);
            return;
        }
        if (core_hit_check(g, x)) {
            return;
        }
        if (!(M(EntFlags + x) & 2)) {
            return;
        }
        if (M(EntX + x) < 0x20 || M(EntX + x) >= 0xF0) {
            return;
        }
        for (int y = 0x0B; y < 0x10; y++) {
            if (M(EntType + y) == 0x98) {
                return;
            }
        }
        int c = spawn_child_slot(g, x, 0x34);
        if (c < 0) {
            return;
        }
        M(EntHealth + c) = 8;
        M(EntRoutine + c) = 1;
        M(EntState + c) = 1;
        M(EntVelY + c) = 0xFD;
        int k = M(EntX + c) >= 0x40;
        M(EntVelX + c) = R5(0xB7D9 + k);
        M(EntFlags + c) = R5(0xB7DB + k);
        return;
    }
    uint8_t first;
    if (x == 7) {
        if (M(EntState + x)) {
            if ((M(FrameCounter) & 3) == 2) {
                return;
            }
            core_die_step(g, x, 6);
            return;
        }
        if (core_hit_check(g, x)) {
            return;
        }
        if (!(M(EntFlags + x) & 2)) {
            return;
        }
        if ((M(FrameCounter) & 0x3E) != 0x3E) {
            return;
        }
        if (M(EntX + x) < 0x60) {
            return;
        }
        first = 0x2E;
    } else {
        if (M(EntState + x)) {
            core_die(g, x, 4);
            return;
        }
        if (core_hit_check(g, x)) {
            return;
        }
        if (!(M(EntFlags + x) & 2)) {
            return;
        }
        if ((M(FrameCounter) & 0x3E) != 0x3E) {
            return;
        }
        if (M(EntX + x) >= 0xD0) {
            return;
        }
        first = 0x31;
    }
    int n = 0;
    for (int y = 0; y < 5; y++) {
        if (M(EntType + 11 + y)) {
            n++;
        }
    }
    if (n >= 3) {
        return;
    }
    for (int k = 0; k < 3; k++) {
        int c = spawn_child_slot(g, x, (uint8_t)(first + k));
        if (c < 0) {
            return;
        }
        clear_velocity(g, c);
        set_animation(g, c, (uint8_t)(first + k - 0x14));
        M(EntWait + x) = 1;
    }
}

void boss_core(Game *g, int x) /* B5 $B5E2 */
{
    if (x != 5) {
        core_element(g, x);
        return;
    }
    int xr = core_main(g, x);
    core_camera_path(g);
    core_palette_twinkle(g, xr);
    core_scroll_all(g);
    int c = M(CameraXLo) >= M(CameraDeltaX);
    M(CameraXLo) = (uint8_t)(M(CameraXLo) - M(CameraDeltaX));
    if (M(CameraDeltaX) & 0x80) {
        M(CameraXHi) = (uint8_t)((M(CameraXHi) + c) & 1);
    } else {
        M(CameraXHi) = (uint8_t)((M(CameraXHi) - 1 + c) & 1);
    }
    if (!(M(VerticalLevelFlag) & 0x0F)) {
        M(PaletteDirty) = 1; /* B7 $F778 */
    }
}

void boss_turret(Game *g, int x) /* B5 $B87B: turret that follows the player */
{
    if (!(M(EntFlags + x) & 2)) {
        return;
    }
    uint8_t dir = core_aim(g, x);
    M(Tmp0) = dir;
    M(Tmp2) = 0;
    M(Tmp3) = 0;
    if ((uint8_t)(M(EntAnimId + x) - 0x11) != dir) {
        M(Tmp2) = 0xF8;
        M(Tmp3) = 0xFF;
        if (R5(0xB94D + dir) & R5(0xB955 + 0xFF)) {
            M(Tmp2) = 8;
            M(Tmp3) = 0;
        }
    }
    unsigned s = M(EntVar0 + x) + M(Tmp2);
    M(EntVar0 + x) = (uint8_t)s;
    set_animation(g, x, (uint8_t)(((M(Tmp3) + M(Tmp0) + (s > 0xFF)) & 7) + 0x11));
    if (M(EntVar1 + x)) {
        return;
    }
    int c = spawn_child_slot(g, x, (uint8_t)(M(EntAnimId + x) + 0x0C));
    if (c < 0) {
        return;
    }
    uint8_t dx = (uint8_t)(M(EntX + x) - M(EntX + c));
    if (M(EntX + x) < M(EntX + c)) {
        dx = (uint8_t)(0 - dx);
    }
    if (dx >= 0x80) {
        M(EntType + c) = 0;
        M(EntFlags + c) = 0;
        return;
    }
    uint8_t idx = (uint8_t)((M(EntAnimId + x) - 0x11) << 2);
    M(EntVelXFrac + c) = R5(0xB92D + idx);
    M(EntVelX + c) = R5(0xB92E + idx);
    M(EntVelYFrac + c) = R5(0xB92F + idx);
    M(EntVelY + c) = R5(0xB930 + idx);
    M(EntFlags + c) = 0x0A;
    M(EntWait + c) = (uint8_t)x;
    M(EntVar1 + x) = 1;
}

void boss_fragment(Game *g, int x) /* B5 $B50E: fragment that blinks then disappears */
{
    M(EntVar0 + x)++;
    if (M(EntVar0 + x) >= 0x40) {
        clear_entity(g, x);
        return;
    }
    if (!(M(EntVar0 + x) & 6)) {
        return;
    }
    M(EntFlags + x) ^= 2;
}

void boss_turret_shot(Game *g, int x) /* B5 $B581: shot of the $76 turrets */
{
    unsigned f = M(EntXFrac + x) + M(EntVelXFrac + x);
    M(EntXFrac + x) = (uint8_t)f;
    uint8_t old = M(EntX + x);
    uint8_t nw = (uint8_t)(old + M(EntVelX + x) + (f > 0xFF));
    M(EntX + x) = nw;
    int gone = 0;
    if ((old ^ nw) & 0x80) {
        if (nw & 0x80) {
            gone = (M(EntVelX + x) & 0x80) != 0;
        } else {
            gone = !(M(EntVelX + x) & 0x80);
        }
    }
    if (!gone) {
        unsigned fy = M(EntVelYFrac + x) + M(EntYFrac + x);
        M(EntYFrac + x) = (uint8_t)fy;
        M(EntY + x) = (uint8_t)(M(EntVelY + x) + M(EntY + x) + (fy > 0xFF));
        gone = M(EntY + x) >= 0xBC;
    }
    if (!gone) {
        return;
    }
    M(EntType + x) = 0;
    M(EntFlags + x) &= 0xFD;
    M(EntVar1 + M(EntWait + x)) = 0;
    M(EntWait + x) = 0;
    M(EntAttr + x) = 0;
}
