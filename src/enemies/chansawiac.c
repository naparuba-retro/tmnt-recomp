/* Type $91: chansawiac. */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "enemy.h"
#include "enemies/common.h"
#include "enemies/foot_soldier.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* ------------------------------------------------------------ chansawiac ($91): B5 $91C6, jump $85B2 --- */
void hop_start(Game *g, int x, uint8_t a) /* B5 $85B2: start of a jump; a = index in the tables $8614-$8638 */
{
    M(EntState + x)++;
    uint8_t dx = (M(EntFlags + x) & 1) ? 0xF2 : 0x0E;
    M(EntVelY + x) = R5(0x8614 + a);
    M(EntVelYFrac + x) = R5(0x8620 + a);
    int blocked;
    if (M(EntVelY + x) & 0x80) {
        blocked = in_block_list(tile_at_offset(g, x, dx, 0xDC));
    } else {
        blocked = in_block_list(tile_at_offset(g, x, dx, 0xFC));
    }
    if (!blocked) {
        blocked = in_block_list(tile_at_offset(g, x, dx, 0xF4));
    }
    if (blocked) { /* $85FD: jump cancelled */
        M(EntState + x) = 0;
        clear_vel_x(g, x);
        clear_vel_y(g, x);
        return;
    }
    M(EntRoutine + x) = R5(0x862C + a);
    set_animation(g, x, R5(0x8638 + a));
}

void ai_chansawiac(Game *g, int x) /* B5 $91C6 */
{
    if (M(EntFlags + x) & 4) { /* hit */
        set_animation(g, x, 0x84);
        clear_vel_x(g, x);
        clear_vel_y(g, x);
        return;
    }
    if (!front_tile_set(g, x, 0x0C, 0xFF)) { /* no more ground: it jumps */
        hop_start(g, x, 9);
        clear_vel_x(g, x);
        return;
    }
    M(EntVar0 + x)++;
    if ((M(EntVar0 + x) & 0x1F) == 0) { /* $922E: stops and strikes */
        clear_vel_x(g, x);
        set_animation(g, x, (M(FrameCounter) & 2) ? 0x83 : 0x82);
        M(EntRoutine + x) = 3;
        M(EntState + x)++;
        return;
    }
    if (M(EntAnimId + x) != 0x80) {
        if (!(M(EntFlags + x) & 1)) {
            if (M(EntX + x) >= 0xE8) {
                M(EntFlags + x) |= 1;
            }
        } else if (M(EntX + x) < 0x18) {
            M(EntFlags + x) &= 0xFE;
        }
        walk_speed(g, x, 1, 0xC0);
        if (move_blocked(g, x, 0x0A, 0xF0)) {
            clear_vel_x(g, x);
        }
        set_animation(g, x, 0x80);
    }
    if (move_blocked(g, x, 0x14, 0xDC)) {
        turn_around(g, x); /* $9178 */
    }
}
