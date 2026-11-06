/* Special zone of level 2 (dam): swimming. */
#include "fixes.h"
#include "game.h"
#include "anim.h"
#include "hal.h"
#include "player.h"
#include "sound.h"
#include "player/pose.h"
#include "player/swim.h"
#include "player/tiles.h"
#define TODO_AREA "player" /* prefix of the TODO traces of this file */

/* --- special zone (level 2, dam): swimming, B4 $A78B-$A9B2 --- */
static void swim_palette(Game *g) /* B4 $A992: the palette blinks (key or every $FC frames) */
{
    if (M(SwimBlinkTimer)) {
        M(SwimBlinkTimer)--;
        load_palette_preset(g, (M(SwimBlinkTimer) & 2) ? 0x2B : 0x2C);
        return;
    }
    uint8_t a = M(FrameCounter) >> 1;
    if (a == 0) {
        load_palette_preset(g, 0x2C);
    } else if (a == 0x7E) {
        load_palette_preset(g, 0x2B);
    }
}

static int swim_timer(Game *g) /* B4 $A94E: bomb timer; 0 = time is up */
{
    if (!M(BombTimerSeconds)) {
        return 0;
    }
    if (--M(BombTimerTicks) != 0) {
        return 1;
    }
    M(BombTimerTicks) = 0x1E;
    M(BombTimerSeconds)--;
    uint8_t a = M(BombTimerSeconds);
    if (a == 0x16) {
        sound_play(g, 0x33);
    } else if (a == 0x15) {
        sound_play(g, 0x63);
    } else if (a == 0x05) {
        sound_play(g, 0x67);
    }
    return 1;
}

static void swim_bubble(Game *g) /* B4 $A910: the swimmer's weapon (entity 1, animation $D8) follows the turtle */
{
    if (M(EntAnimId + 1) == 0xD8) {
        return;
    }
    M(EntX + 1) = (M(EntFlags) & 1) ? (uint8_t)(M(EntX) - 8) : (uint8_t)(M(EntX) + 0x0E);
    M(EntY + 1) = (uint8_t)(M(EntY) - 0x0C);
    M(EntFlags + 1) = 0xC2;
    M(EntVelY + 1) = 0xFE;
    M(EntVelYFrac + 1) = 0x80;
    set_animation(g, 1, 0xD8);
}

static void swim_collisions(Game *g) /* B4 $A896 */
{
    if (M(EntVelXFrac) | M(EntVelX)) {
        int blocked;
        if (!(M(EntVelX) & 0x80)) {
            blocked = probe_tile_vel(g, 0x2C, 7) || probe_tile_vel(g, 0x2E, 7);
        } else {
            blocked = probe_tile_vel(g, 0x2D, 7) || probe_tile_vel(g, 0x2F, 7);
        }
        if (blocked) {
            clear_vel_x(g, 0);
        }
    }
    if (M(EntVelY) & 0x80) {
        if (probe_tile_vel(g, 0x19, 7) || probe_tile_vel(g, 0x1B, 7) || probe_tile_vel(g, 0x1C, 7)) {
            clear_vel_y(g, 0);
        }
    } else {
        if (probe_tile_vel(g, 0x1A, 7) || probe_tile_vel(g, 0x1D, 7) || probe_tile_vel(g, 0x1E, 7)) {
            clear_vel_y(g, 0);
        }
    }
}

static void swim_clamp_vy(Game *g) /* B4 $A877 */
{
    uint8_t v = M(EntVelY);
    if (!(v & 0x80)) {
        if (v < 2) {
            return;
        }
        M(EntVelY) = 2;
    } else {
        if (v >= 0xFE) {
            return;
        }
        M(EntVelY) = 0xFE;
    }
    M(EntVelYFrac) = 0;
}

void player_swim(Game *g) /* B4 $A78B: state 15 */
{
    M(CameraXLoSaved) = M(CameraXLo);
    swim_palette(g);
    if (!swim_timer(g)) {
        M(SceneId) = 5;
        sound_play(g, 0x4B);
        return;
    } /* $A97A: the bomb explodes */
    swim_bubble(g);
    uint8_t pad = M(JoyHeld1);
    if (pad & 3) {
        M(EntFlags) = (uint8_t)((M(EntFlags) & 0xFE) | ((pad & 2) ? 1 : 0));
    }
    uint8_t anim;
    if ((pad & 0x0F) == 0) {
        anim = 0xBC;
        if (!(pad & 0x80) && !(M(EntVelY) & 0x80)) {
            anim = 0xBF;
        }
    } else if (!(pad & 0x0C)) {
        anim = 0xBE;
    } else {
        anim = (pad & 4) ? 0x8C : 0xBC;
    }
    player_set_pose(g, anim);
    ROM_TABLE(vx, 4, 4, 0xA9B5); /* $A9B5 */
    M(EntVelX) = vx[pad & 3];
    unsigned s = M(EntVelYFrac) + 0x10; /* the turtle sinks slowly */
    M(EntVelYFrac) = (uint8_t)s;
    uint8_t v = (uint8_t)(M(EntVelY) + (s > 0xFF));
    if (!(v & 0x80) && v >= 2) {
        v = 2;
    }
    M(EntVelY) = v;
    if (M(JoyPressed1) & 0x80) { /* A: upward swim stroke */
        s = M(EntVelYFrac) + 0x20;
        M(EntVelYFrac) = (uint8_t)s;
        v = (uint8_t)(M(EntVelY) + 0xFF + (s > 0xFF));
        if ((v & 0x80) && v < 0xFD) {
            v = 0xFD;
        }
        M(EntVelY) = v;
    }
    if (!M(WaterCurrent) && fix_enabled(FIX_WATER_DRIFT)) {
        M(EntVelXFrac) = 0; /* PIZZA-002: the current acts on the fraction of the speed, which the d-pad does not reset: it pushed the turtle for ever */
    }
    if (M(WaterCurrent)) { /* current */
        uint8_t y = R4(0xA9C0 + (M(WaterCurrent) >> 4));
        unsigned sx = R4(0xA9C5 + y) + M(EntVelXFrac);
        M(EntVelXFrac) = (uint8_t)sx;
        M(EntVelX) = (uint8_t)(R4(0xA9C6 + y) + M(EntVelX) + (sx > 0xFF));
        unsigned sy = R4(0xA9C7 + y) + M(EntVelYFrac);
        M(EntVelYFrac) = (uint8_t)sy;
        M(EntVelY) = (uint8_t)(R4(0xA9C8 + y) + M(EntVelY) + (sy > 0xFF));
        if (pad & 4) {
            sy = M(EntVelYFrac) + 0x30;
            M(EntVelYFrac) = (uint8_t)sy;
            M(EntVelY) = (uint8_t)(M(EntVelY) + (sy > 0xFF));
        }
    }
    swim_clamp_vy(g);
    swim_collisions(g);
}
