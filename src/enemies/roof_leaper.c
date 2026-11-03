/* Type $86 (roof_leaper / divebombs). */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "enemy.h"
#include "enemies/common.h"
#include "enemies/foot_soldier.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* ------------------------------------------------------------ spider ($86) : B5 $982B, $987C-$9974 --- */
static void sub_vel_y(Game *g, int x, uint8_t a) /* B7 $F023: vertical velocity -= a */
{
    unsigned s = M(EntVelYFrac + x) + (uint8_t)(((uint8_t)~a) + 1);
    M(EntVelYFrac + x) = (uint8_t)s;
    M(EntVelY + x) = (uint8_t)(M(EntVelY + x) + 0xFF + (s > 0xFF));
}

void go_left(Game *g, int x) /* B5 $9805 */
{
    M(EntFlags + x) |= 1;
    M(EntVelX + x) = 0xFE;
    M(EntVelXFrac + x) = 0xA0;
}

void go_right(Game *g, int x) /* B5 $9818 */
{
    M(EntFlags + x) &= 0xFE;
    M(EntVelX + x) = 0x01;
    M(EntVelXFrac + x) = 0x60;
}

void ai_roof_leaper(Game *g, int x) /* B5 $982B: the spider leaves in the direction of the scrolling */
{
    M(EntRoutine + x) = 0x90;
    M(EntState + x)++;
    if (!(M(CameraDeltaX) & 0x80)) {
        go_left(g, x);
    } else {
        go_right(g, x);
    }
}

void spider_walk(Game *g, int x) /* B5 $987C: state $90, walks along the scenery */
{
    int right_facing = (M(EntFlags + x) & 1) != 0;
    uint8_t t = tile_at_offset(g, x, right_facing ? 0x0C : 0xF4, 0xDE);
    if (t == 0) { /* $98B3: no more scenery, it lets itself fall */
        M(EntRoutine + x)++;
        M(EntVelY + x) = 0xFD;
        clear_vel_x(g, x);
        return;
    }
    uint8_t t2 = tile_at_offset(g, x, right_facing ? 0xF4 : 0x0C, 0xF3);
    if (in_block_list(t2)) {
        if (right_facing) {
            go_right(g, x);
        } else {
            go_left(g, x);
        }
        return;
    } /* wall: U-turn */
    uint8_t d = (M(EntX) >= M(EntX + x)) ? (uint8_t)(M(EntX) - M(EntX + x)) : (uint8_t)(M(EntX + x) - M(EntX));
    if (d >= 0x28) {
        return;
    }
    set_animation(g, x, 7);
    M(EntRoutine + x) += 2;
    M(EntVelY + x) = 6;
}

void spider_climb(Game *g, int x) /* B5 $98D7: state $91, climbs to the ceiling */
{
    sub_vel_y(g, x, 0x18);
    if (!in_block_list(tile_at_offset(g, x, 0, 0xE8))) {
        return;
    }
    for (uint8_t n = 0x18;;) {
        if (--n == 0) {
            break;
        }
        M(EntY + x)++;
        if (!in_block_list(tile_at_offset(g, x, 0, 0xE8))) {
            break;
        }
    }
    M(EntState + x) = 0;
    clear_vel_y(g, x);
}

void spider_drop(Game *g, int x) /* B5 $9908: state $92, dives onto the player */
{
    sub_vel_y(g, x, 0x2D);
    int blocked;
    if (!(M(EntFlags + x) & 1)) {
        blocked = in_block_list(tile_at_offset(g, x, 0x0A, 0xF0));
    } else {
        blocked = !in_block_list(tile_at_offset(g, x, 0xF6, 0xF0)) ? 0 : 1;
    }
    if (blocked) { /* $992E */
        set_animation(g, x, 6);
        M(EntRoutine + x)--;
        M(EntVelY + x) = 0xFD;
        clear_vel_x(g, x);
        return;
    }
    if (!(M(EntVelY + x) & 0x80)) { /* $993E: it goes down */
        if (in_block_list(tile_at_offset(g, x, 0, 0))) {
            clear_vel_y(g, x);
        }
        return;
    }
    if (!in_block_list(tile_at_offset(g, x, 0, 0xE8))) {
        return; /* $994D: it goes up */
    }
    do {
        M(EntY + x)++;
    } while (in_block_list(tile_at_offset(g, x, 0, 0xE8)));
    set_animation(g, x, 6);
    M(EntRoutine + x) -= 2;
    clear_vel_y(g, x);
}
