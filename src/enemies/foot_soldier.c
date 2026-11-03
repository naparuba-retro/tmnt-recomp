/* Type $40: foot soldier of the top-view streets. */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "enemy.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* ------------------------------------------------------------- AI of type $40 (B5 $8907) --- */
int blocked(Game *g, int x, uint8_t dx, uint8_t dy) /* B5 $88F8: carry = tile 4 or $0C */
{
    uint8_t t = tile_at_offset(g, x, dx, dy);
    return t == 4 || t == 0x0C;
}

void face_player_get_distance(Game *g, int x) /* B5 $8381 */
{
    if (M(EntX) >= M(EntX + x)) {
        M(Tmp0) = (uint8_t)(M(EntX) - M(EntX + x));
        if (M(Tmp0) >= 8) {
            M(EntFlags + x) &= 0xFE; /* the player is on the right */
        }
    } else {
        M(Tmp0) = (uint8_t)(M(EntX + x) - M(EntX));
        if (M(Tmp0) >= 8) {
            M(EntFlags + x) |= 0x01; /* on the left */
        }
    }
    if (M(EntY) >= M(EntY + x)) {
        M(Tmp0 + 1) = (uint8_t)(M(EntY) - M(EntY + x));
        M(Tmp0 + 2) = 0;
    } else {
        M(Tmp0 + 1) = (uint8_t)(M(EntY + x) - M(EntY));
        M(Tmp0 + 2) = 1;
    }
}

void set_vel_x(Game *g, int x, uint8_t frac, uint8_t whole) {
    M(EntVelXFrac + x) = frac;
    M(EntVelX + x) = whole;
}
void set_vel_y(Game *g, int x, uint8_t frac, uint8_t whole) {
    M(EntVelYFrac + x) = frac;
    M(EntVelY + x) = whole;
}

static int walk_right(Game *g, int x) /* B5 $8950: carry = has moved (or sets the animation) */
{
    if (blocked(g, x, 0x0C, 0xFA)) {
        return 0;
    }
    set_vel_x(g, x, 0x20, 2);
    M(EntFlags + x) &= 0xFE;
    goto walk_anim;
walk_anim: /* $8983 */
    if (M(EntAnimId + x) != 0x70) {
        set_animation(g, x, 0x70);
        clear_vel_y(g, x);
    }
    return 1;
}

static int walk_left(Game *g, int x) /* B5 $896B */
{
    if (blocked(g, x, 0xF4, 0xFA)) {
        return 0;
    }
    set_vel_x(g, x, 0xE0, 0xFD);
    M(EntFlags + x) |= 0x01;
    if (M(EntAnimId + x) != 0x70) {
        set_animation(g, x, 0x70);
        clear_vel_y(g, x);
    }
    return 1;
}

static int walk_up(Game *g, int x) /* B5 $8994 */
{
    if (blocked(g, x, 0x00, 0xED)) {
        return 0;
    }
    set_vel_y(g, x, 0xE0, 0xFD);
    if (M(EntAnimId + x) != 0x6E) {
        set_animation(g, x, 0x6E);
        clear_vel_x(g, x);
    }
    return 1;
}

static int walk_down(Game *g, int x) /* B5 $89AD */
{
    if (blocked(g, x, 0x00, 0x01)) {
        return 0;
    }
    set_vel_y(g, x, 0x20, 2);
    if (M(EntAnimId + x) != 0x6F) {
        set_animation(g, x, 0x6F);
        clear_vel_x(g, x);
    }
    return 1;
}

void ai_foot_soldier(Game *g, int x) {
    face_player_get_distance(g, x);
    int horizontal = M(Tmp0) >= 0x0A; /* $8907: too far in X -> walks first */
    if (!horizontal) {                /* $893A */
        if (M(Tmp0 + 1) < 0x0A) {
            clear_vel_x(g, x);
            clear_vel_y(g, x);
            return;
        }
        int moved = M(Tmp0 + 2) ? walk_up(g, x) : walk_down(g, x);
        if (moved) {
            return;
        }
    }
    int moved = (M(EntFlags + x) & 1) ? walk_left(g, x) : walk_right(g, x); /* $8910 */
    if (moved) {
        return;
    }
    if (M(Tmp0 + 1) < 0x0A) {
        clear_vel_x(g, x);
        clear_vel_y(g, x);
        return;
    } /* $8922 */
    moved = M(Tmp0 + 2) ? walk_up(g, x) : walk_down(g, x);
    if (moved) {
        return;
    }
    clear_vel_x(g, x);
    clear_vel_y(g, x);
}
