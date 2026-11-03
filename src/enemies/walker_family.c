/* Family of type $30: $9E, $9F, $A0, $A1 and their states. */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "enemy.h"
#include "enemies/chansawiac.h"
#include "enemies/common.h"
#include "enemies/common.h"
#include "enemies/foot_soldier.h"
#include "enemies/patrollers.h"
#include "enemies/purple_ninja.h"
#include "enemies/sewer.h"
#include "enemies/walker_dropper.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

static void walker_fall(Game *g, int x, uint8_t anim) /* $8B15 / $8BA2: no more ground, it falls */
{
    M(EntVelY + x) = 3;
    clear_vel_x(g, x);
    set_animation(g, x, anim);
    set_state(g, x, 6);
}

static void walker_follow(Game *g, int x, uint8_t whole, uint8_t frac) /* $8B6B / $8BDB then $8BE2: walks, stops in front of an obstacle */
{
    walk_and_stop(g, x, whole, frac);
    if (move_blocked(g, x, 0x0A, 0xE4)) {
        clear_vel_x(g, x); /* $8C64 */
    }
}

void ai_walker_stop_and_throw(Game *g, int x) /* B5 $8B99 */
{
    if (!front_tile_set(g, x, 7, 0xFF)) {
        walker_fall(g, x, 0xAD);
        return;
    }
    face_player_get_distance(g, x);
    uint8_t d = M(Tmp0), fc = M(FrameCounter);
    int mode; /* 0 = walks, 1 = stops, 2 = throws */
    if (d < 0x20) {
        mode = (fc & 8) ? 1 : 0;
    } else {
        mode = fc < 0x60 ? 1 : fc < 0xC0 ? 2 : 0;
    }
    if (mode == 0) {
        if (M(EntAnimId + x) != 0xA1) {
            set_animation(g, x, 0xA1);
        }
        walker_follow(g, x, 1, 0xC0);
    } else if (mode == 1) {
        clear_velocity(g, x);
        set_animation(g, x, 0xA2);
        set_state(g, x, 0x0B);
    } else {
        spawn_child_slot(g, x, 3);
        clear_velocity(g, x);
        set_state(g, x, 3);
        set_animation(g, x, 0xA5);
    }
}

void ai_walker_hopper(Game *g, int x) /* B5 $8B05 */
{
    int throw_it = 0, hop = 0; /* neither: the enemy keeps walking */
    if (M(EntFlags + x) & 4) {
        /* walks */
    } else if (!front_tile_set(g, x, 0x0C, 0xFF)) {
        walker_fall(g, x, 0xAC);
        return;
    } else {
        face_player_get_distance(g, x);
        uint8_t d = M(Tmp0), fc = M(FrameCounter);
        if (M(EntVelY)) {
            /* the player jumps: keeps walking */
        } else if (d >= 0x40) {
            if (fc < 0x60) {
                goto mid;
            } else {
                throw_it = 1;
            }
        } else if (d < 0x1C && (M(EntStatus) & M(Tmp4))) {
            /* walks */
        } else if (fc >= 0x60) {
            throw_it = 1;
        } else {
            goto mid;
        }
        goto done;
    mid:
        if (fc >= 0x20) {
            hop = 1;
        } else if (fc >= 0x18) {
            /* walks */
        } else {
            walker_follow(g, x, 1, 0x50);
            goto anim;
        }
    }
done:
    if (throw_it) {
        ninja_set_anim_state(g, x, 0xA8);
        turn_around(g, x);
        spawn_child_slot(g, x, 1);
        clear_vel_x(g, x);
        return;
    }
    if (hop) {
        hop_start(g, x, 0);
        return;
    }
    walker_follow(g, x, 2, 0xC0);
anim:
    if (M(EntAnimId + x) != 0xA7) {
        set_animation(g, x, 0xA7);
    }
}

void ai_walker_charge_left(Game *g, int x) {
    set_state(g, x, 7);
    walk_speed(g, x, 0xFC, 0xC0);
} /* B5 $8B8D */
void ai_walker_charge_right(Game *g, int x) {
    set_state(g, x, 7);
    walk_speed(g, x, 3, 0x40);
} /* B5 $8C06 */

void walker_toss_state(Game *g, int x) /* B5 $8D13 */
{
    ballistic(g, x, 2, 0);
    land_check(g, x, 0, 0xFC);
    if (M(FrameCounter) & 0x3F) {
        return;
    }
    turn_around(g, x);
    spawn_child_slot(g, x, 1);
}

void walker_recover_state(Game *g, int x) /* B5 $8342: end of animation -> state 0, and the AI runs right away */
{
    if (M(EntAnimFrame + x) != 0xFF) {
        return;
    }
    M(EntState + x) = 0;
    M(EntRoutine + x) = 0;
    M(EntAnimFrame + x) = 0;
    run_entity_ai(g, x);
}

void walker_start_walk_state(Game *g, int x) /* B5 $8D2E */
{
    if (M(EntAnimFrame + x) != 0xFF) {
        return;
    }
    walk_and_stop(g, x, 4, 0x80);
    M(EntRoutine + x)++;
    set_animation(g, x, 0xA3);
}

void walker_approach_state(Game *g, int x) /* B5 $8D44 */
{
    face_player_get_distance(g, x);
    if (M(Tmp0) < 0x50) {
        if (M(FrameCounter) >= 0x38) {
            M(EntState + x)--;
            hop_start(g, x, 1);
            return;
        } /* $8D69: $85B5, without changing state */
        if (!move_blocked(g, x, 8, 0xF0)) {
            return;
        }
    }
    clear_vel_x(g, x); /* $8D5C */
    M(EntRoutine + x) = 3;
    set_animation(g, x, 0xA4);
}

void walker_hop_state(Game *g, int x) /* B5 $8D6E */
{
    ballistic(g, x, 4, 0);
    if (M(EntVelY + x) & 0x80) {
        return;
    }
    land_check(g, x, 0, 0xFC);
}
