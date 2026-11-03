/* Other player states: roof flight, run state, push zones, conveyor. */
#include "game.h"
#include "anim.h"
#include "hal.h"
#include "player.h"
#include "rope.h"
#include "sound.h"
#include "player/ladder.h"
#include "player/overhead.h"
#include "player/pose.h"
#include "player/shared.h"
#include "player/side_move.h"
#include "player/special_states.h"
#include "player/swim.h"
#include "player/tiles.h"
#define TODO_AREA "player" /* prefix of the TODO traces of this file */

/* B4 $9CAD: 29 states */

static void conveyor_push(Game *g, uint8_t s) /* B4 $A4BF (state 21, right) / $A4DE (state 20, left): conveyor belt */
{
    static const uint8_t probes_right[3] = {0x12, 4, 0}, probes_left[3] = {0x13, 5, 1};
    const uint8_t *ys = s == 21 ? probes_right : probes_left;
    for (int i = 0; i < 3; i++) {
        if (probe_tile(g, ys[i], 5)) {
            return; /* a wall blocks: no push */
        }
    }
    if (M(EntStatus) & 0x20) {
        return; /* already pushed on this frame */
    }
    uint8_t y = (uint8_t)((s == 21 ? 1 : 0) << 1);
    unsigned lo = M(EntVelXFrac) + R4(0xA787 + y);
    M(EntVelXFrac) = (uint8_t)lo;
    M(EntVelX) = (uint8_t)(M(EntVelX) + R4(0xA788 + y) + (lo > 0xFF));
    M(EntStatus) |= 0x20;
    if (!(M(EntStatus) & 0x04) && !M(JoyHeld1) && !(M(EntStatus) & 0x10)) {
        set_animation(g, 0, 0x80); /* $D121 */
    }
}

/* B4 $A1C2: state 9, flight above the top of the screen (building rooftops): left / right command, gravity, falls back to state 7 */
static void player_roof_flight(Game *g) {
    uint8_t h = M(JoyHeld1);
    if (h & 3) {
        if (!(h & 1)) { /* left */
            int stop = 0;
            if (M(AreaNumber) == 0 && (!M(CameraXHi) || (M(CameraXHi) < 2 && !(M(CameraXLo) & 0x80)))) {
                stop = 1; /* $A1D5 */
            }
            if (stop) {
                clear_vel_x(g, 0);
            } else {
                walk_velocity(g, 2);
                M(EntFlags) |= 1;
            }
        } else { /* right */
            if (M(AreaNumber) == 0 && M(CameraXHi) >= 4) {
                clear_vel_x(g, 0);
            } else {
                walk_velocity(g, 1);
                M(EntFlags) &= 0xFE;
            }
        }
    } /* no direction: $A1C8 jumps to $A218, the velocity stays */
    player_gravity(g); /* $A218 */
    if (M(EntVelY) & 0x80) {
        return;
    }
    if (M(EntY) < 8) {
        return;
    }
    M(EntFlags) |= 2;
    if (M(EntY) < 0x10) {
        return;
    }
    M(EntState) = 7;
}

int player_run_state(Game *g, uint8_t s) /* B4 $9CB0: routine of state s; carry of the routine */
{
    int carry = 1; /* carry handed to PlayerProbeTile; $A404 sets it to 0 */
    switch (s) {
        case 0:
        case 1:
        case 2:
        case 3:
            player_on_ground(g);
            break;
        case 4:
        case 5:
            if (M(EntAnimFrame) == 0xFF) {
                M(EntState) = 0;
            }
            break; /* $9CED */
        case 6:
            M(EntState) = 7;
            M(EntRoutine) = 8;
            clear_vel_y(g, 0);
            break; /* $A1B3: the ground is missing, we fall */
        case 7:
        case 8:
            player_airborne(g);
            break;
        case 10:
            player_falling(g);
            break;
        case 11:
            player_ladder_enter(g);
            break;
        case 12:
            player_ladder(g);
            break;
        case 13:
            door_transition(g);
            break; /* $E6A9: door */
        case 15:
            player_swim(g);
            break; /* $A78B: swimming (special zone) */
        case 18:   /* $A404: at the top of the ladder, goes through the door */
            if (!M(WaveState)) {
                M(EntState) = 0x0D;
                clear_vel_x(g, 0);
                clear_vel_y(g, 0);
            }
            carry = 0;
            break;
        case 14: /* $A5BB: seaweed, damage (0x10 of health); $D3: special zone */
            carry = 0;
            if (M(EntFlags) & 0x04) {
                break;
            }
            sound_play(g, M(SwimZoneFlag) ? 0x2F : 0x25);
            M(EntHurtTimer) = 0x18; /* $A649 */
            M(EntFlags) |= 0x24;
            M(TurtleHealth0 + M(CurrentTurtle)) = M(TurtleHealth0 + M(CurrentTurtle)) >= 0x10 ? (uint8_t)(M(TurtleHealth0 + M(CurrentTurtle)) - 0x10) : 0;
            if (M(SwimZoneFlag)) {
                M(0x03EC)++;
            }
            break;
        case 19: /* $A412: deadly seaweed, the turtle dies */
            sound_play(g, 0x83);
            M(EntFlags) &= 0x7F;
            M(EntY) = (uint8_t)(M(EntY) - 0x0D);
            M(TurtleHealth0 + M(CurrentTurtle)) = 0;
            set_animation(g, 0, 0xDB); /* $D121 */
            clear_vel_x(g, 0);
            clear_vel_y(g, 0);
            carry = 0;
            break;
        case 16:
            clear_vel_x(g, 0);
            clear_vel_y(g, 0);
            if (M(EntAnimFrame) == 0xFF) {
                M(EntState) = 0x0F;
            }
            break;
        case 20:
        case 21:
            conveyor_push(g, s);
            carry = 0;
            break;
        case 9:
            player_roof_flight(g);
            break; /* $A1C2 */
        case 25:
            rope_player_state(g);
            carry = 0;
            break; /* $A438: rope between the rooftops */
        case 22:   /* $A538: fall into the void, the turtle exits on the right then the room restarts */
            if (M(DelayTimerLo) && --M(DelayTimerLo)) {
                break;
            }
            set_chr_bank0(g, 0x11);
            if (M(EntX) >= 0xEB) {
                M(WaveState) = 0;
                M(WaveActive) = 0;
                M(StageState) = 2;
            } /* $9A68 */
            else {
                for (int x = 1; x < 16; x++) {
                    clear_entity(g, x);
                }
            }
            break;
        case 23: /* $A554: dive, then the screen shakes */
            if (--M(DelayTimerLo) == 0) {
                M(DelayTimerLo) = 0x30;
                M(EntState)++;
                sound_play(g, 0x2A);
            }
            break;
        case 24:                          /* $A564 */
            if (--M(DelayTimerLo) == 0) { /* $9A56: loss of 0x10 of health (1 at minimum), then the room restarts */
                uint8_t *h = &M(TurtleHealth0 + M(CurrentTurtle));
                *h = *h >= 0x10 ? (uint8_t)(*h - 0x10) : 1;
                M(EntHurtTimer) = 0x18;
                M(EntFlags) |= 0x24;
                M(WaveState) = 0;
                M(WaveActive) = 0;
                M(StageState) = 2;
            } else if (M(DelayTimerLo) >= 0x10) {
                M(ScrollYShadow) = (uint8_t)(M(ScrollYShadow) + ((M(DelayTimerLo) & 1) ? 3 : 0xFD));
            }
            break;
        case 28:
            break;
        case 27: /* $A5CC: damage (0x10 of health) without a blink test */
            sound_play(g, 0x25);
            M(EntHurtTimer) = 0x18; /* $A649 */
            M(EntFlags) |= 0x24;
            M(TurtleHealth0 + M(CurrentTurtle)) = M(TurtleHealth0 + M(CurrentTurtle)) >= 0x10 ? (uint8_t)(M(TurtleHealth0 + M(CurrentTurtle)) - 0x10) : 0;
            if (M(SwimZoneFlag)) {
                M(0x03EC)++;
            }
            carry = 0;
            break;
        case 26:
            carry = !(M(EntFlags) & 0x04);
            break; /* $A456: the carry says whether the tile under the turtle counts (not while blinking) */
        default:
            TRACE("f=%u TODO player state %d\n", g->frame, s);
            break;
    }
    return carry;
}

void push_zone(Game *g) /* B4 $9BE4: push zone (objects $33-$36), the X velocity tends to -2 or 2 */
{
    if (M(EntState) == 0x0C) {
        return;
    }
    uint8_t a;
    if (M(PushZone) & 0x80) {
        a = (uint8_t)(M(EntVelX) - 1);
        if ((a & 0x80) && a < 0xFE) {
            a = 0xFE;
        }
    } else if (M(PushZone) & 0x40) {
        a = (uint8_t)(M(EntVelX) + 1);
        if (!(a & 0x80) && a >= 2) {
            a = 2;
        }
    } else {
        return;
    }
    M(EntVelX) = a;
}

void player_animation_state(Game *g) /* B4 $9C69 */
{
    ROM_TABLE(special_anims, 7, 4, 0x9C62); /* $9C62 */
    if (M(StageIntroTimer)) {
        return;
    }
    int hit = 0;
    for (int i = 0; special_anims[i] != 0xFF; i++) {
        if (special_anims[i] == M(EntAnimId)) {
            hit = 1;
        }
    }
    if (hit) {
        M(EntStatus) |= 0x02;
    } else {
        M(EntStatus) &= 0xFD;
    }
    M(EntStatus) &= 0xDF;
    if (M(EntStatus) & 0x10) { /* $A582: invulnerability after the bonus, the turtle blinks */
        hurt_blink_tick(g, 0);
        if (--M(EntVar0) == 0) {
            M(EntStatus) &= 0xEF;
            M(EntFlags) &= 0xDF;
            M(EntAttr) = 0;
        } else {
            if (M(EntVar0) < 0x30) {
                M(EntFlags) |= 0x20;
            }
            uint8_t anim = R4(0xA5B7 + M(CurrentTurtle)); /* $A63F */
            if (anim != M(EntAnimId)) {
                set_animation(g, 0, anim);
            }
        }
        if (M(EntState) == 0x11) {
            M(EntState) = 0;
        }
    }
    player_state_dispatch(g);
}

void player_pose_anim(Game *g) /* B4 $9C1A */
{
    if (M(EntStatus) & 0x04) {
        if (M(EntAnimFrame) != 0xFF) {
            return; /* attack in progress */
        }
        M(EntStatus) &= 0xFB;
    }
    if (M(EntStatus) & 0x10) {
        return;
    }
    uint8_t a = R4(0x9C45 + M(EntState));
    if (a == 0xFF) {
        return;
    }
    player_set_pose(g, a);
}
