/* Ladders (states 11 and 12). */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "player/ladder.h"
#include "player/pose.h"
#include "player/tiles.h"
#define TODO_AREA "player" /* prefix of the TODO traces of this file */

/* --- ladder (states 11 and 12): B4 $A23C-$A330 --- */
static int ladder_side_ok(Game *g, int left) /* B4 $A3B4 / $A3DC: carry Z = free side or ladder */
{
    static const uint8_t right_probes[3] = {0x24, 0x26, 0x22}, left_probes[3] = {0x25, 0x27, 0x23};
    if (left) {
        M(EntFlags) |= 1;
    } else {
        M(EntFlags) &= 0xFE;
    }
    const uint8_t *ys = left ? left_probes : right_probes;
    for (int i = 0; i < 3; i++) {
        uint8_t t = tile_near_player(g, ys[i]);
        if (i < 2) {
            if (t != 0 && t != 9) {
                return 0;
            }
        } else {
            return t == 0 || t == 9;
        }
    }
    return 0;
}

static void ladder_set_vel_y(Game *g, uint8_t frac, uint8_t whole) {
    M(EntVelYFrac) = frac;
    M(EntVelY) = whole;
} /* $A612 / $A618 */

static void ladder_leave(Game *g) /* $A317: leaves the ladder */
{
    M(EntState) = 0;
    clear_vel_x(g, 0);
    clear_vel_y(g, 0);
    set_animation(g, 0, 0x80);
}

static void ladder_idle(Game *g) /* $A326 */
{
    set_animation(g, 0, 0x88);
    clear_vel_x(g, 0);
    clear_vel_y(g, 0);
}

void player_ladder_enter(Game *g) /* $A23C: state 11 */
{
    M(EntState) = (M(EntStatus) & 0x04) ? 3 : 0x0C;
}

void player_ladder(Game *g) /* $A24D: state 12 */
{
    if (M(EntStatus) & 0x04) {
        M(EntState) = 3;
        return;
    }
    uint8_t h = M(JoyHeld1);
    if (h & 3) {
        if (ladder_side_ok(g, (h & 2) != 0)) { /* jumps off the ladder */
            M(EntState) = 7;
            M(EntRoutine) = 8;
            clear_vel_y(g, 0);
        } else {
            ladder_idle(g);
        }
        return;
    }
    if (!(h & 0x0C)) {
        ladder_idle(g);
        return;
    }
    player_set_pose(g, 0x88);
    if (h & 8) { /* up */
        ladder_set_vel_y(g, 0xC0, 0xFD);
        if (M(EntY) < 0x13) {
            if (M(WaveState)) {
                ladder_idle(g);
                return;
            }
            if (tile_near_player(g, 9) != 0x0B || M(WaveState)) {
                return;
            }
            M(EntState) = 0x12;
            clear_vel_x(g, 0);
            clear_vel_y(g, 0);
            return;
        }
        uint8_t t = tile_near_player(g, 0x29);
        if (t == 9 || t == 0x0A) {
            return;
        }
        if (t == 4) {
            clear_vel_y(g, 0);
            return;
        }
        if (tile_near_player(g, 8) != 0) {
            return;
        }
        do {
            M(EntY)++;
        } while (tile_near_player(g, 6) == 0);
        ladder_leave(g);
        return;
    }
    if (M(0x8D)) {
        M(EntState) = 0;
        return;
    } /* down */
    ladder_set_vel_y(g, 0x40, 2);
    if (!probe_tile(g, 8, 4)) {
        if (M(Tmp0) != 0) {
            return;
        }
        if (tile_near_player(g, 0x16) != 0) {
            return;
        }
        M(EntState) = 6;
        return;
    }
    if (!probe_tile(g, 7, 4)) {
        return;
    }
    do {
        M(EntY)--;
    } while (probe_tile(g, 6, 4));
    ladder_leave(g);
}
