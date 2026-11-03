/* Types $A3 / $A4: bounces while advancing. */
#include "game.h"
#include "player.h"
#include "enemy.h"
#include "enemies/purple_ninja.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- type $A3 (B5 $9C5C-$9D9E): bounces while advancing, throws a projectile when the player is close --- */
int tile_in_wall_set(Game *g, int x, uint8_t dx, uint8_t dy) /* B5 $9A07: F091 then $844E (tiles 4, $0C, $0D) */
{
    uint8_t t = tile_at_offset(g, x, dx, dy);
    return t == 4 || t == 0x0C || t == 0x0D;
}

uint8_t anim_if_changed(Game *g, int x, uint8_t a) /* B5 $A87B; returns A on return: the animation if it has not changed, 0 otherwise ($A885: lda #$00) */
{
    if (M(EntAnimId + x) == a) {
        return a;
    }
    M(EntAnimId + x) = a;
    M(EntAnimTimer + x) = 1;
    M(EntAnimFrame + x) = 0;
    return 0;
}

void ai_bouncer(Game *g, int x) /* B5 $9C5C: start, towards the player; goes to state $1D */
{
    if (M(EntX) >= M(EntX + x)) {
        M(EntVelX + x) = 2;
        M(EntVelXFrac + x) = 0;
        M(EntFlags + x) &= 0xFE;
    } else {
        M(EntVelX + x) = 0xFE;
        M(EntVelXFrac + x) = 0;
        M(EntFlags + x) |= 1;
    }
    M(EntWait + x) = 0xFF; /* $9C95 */
    M(EntVar0 + x) = 2;
    M(EntVar1 + x) = 4;
    M(EntRoutine + x) = 0x1D; /* $8644 */
    M(EntState + x)++;
}

void ai_bouncer_shot(Game *g, int x) /* B5 $9D88: projectile of the A3, falls (velocity 2) then state $1E */
{
    M(EntVelY + x) = 2;
    M(EntVelYFrac + x) = 0;
    M(EntVelXFrac + x) = 0;
    M(EntRoutine + x) = 0x1E;
    M(EntState + x)++;
}

static void bouncer_bounce_velocity(Game *g, int x, uint8_t phase) /* $9D69: vertical velocity according to the phase */
{
    ROM_TABLE2(vy, 5, 2, 5, 0x9D7E);
    uint8_t i = (uint8_t)(phase & 0x7F);
    M(EntVelY + x) = i < 5 ? vy[i][0] : R5(0x9D7E + 2 * i);
    M(EntVelYFrac + x) = i < 5 ? vy[i][1] : R5(0x9D7F + 2 * i);
}

void bouncer_state(Game *g, int x) /* B5 $9CA5 */
{
    int carry = 0;
    uint8_t dx = (M(EntVelX + x) & 0x80) ? 0xF8 : 0x08;
    if (tile_in_wall_set(g, x, dx, 0)) { /* wall ahead: the direction is reversed */
        unsigned f = (unsigned)((uint8_t)~M(EntVelXFrac + x)) + 1;
        M(EntVelXFrac + x) = (uint8_t)f;
        unsigned v = (unsigned)((uint8_t)~M(EntVelX + x)) + (f > 0xFF);
        M(EntVelX + x) = (uint8_t)v;
        carry = v > 0xFF;
    }
    if (!M(EntTimer + x)) {                                        /* $9CCE */
        int d = (int)M(EntX) - (int)M(EntX + x) - (carry ? 0 : 1); /* SBC with the carry left by what precedes */
        uint8_t a = (uint8_t)d;
        if (d < 0) {
            a = (uint8_t)(~a + 1);
        }
        if (a < 6) {
            spawn_child(g, x, 0x1A);
            M(EntTimer + x) = 1;
            anim_if_changed(g, x, (uint8_t)(1 + M(EntAnimId + x)));
        }
    }
    if (--M(EntVar0 + x) == 0) { /* $9CF8 */
        M(EntVar0 + x) = 2;
        M(EntVar1 + x) = (uint8_t)(M(EntVar1 + x) + M(EntWait + x));
        uint8_t a = M(EntVar1 + x) & 0x7F;
        int flip = 0;
        if (a == 0) {
            M(EntVar1 + x) = (M(EntVar1 + x) & 0x80) ? (uint8_t)(M(EntVar1 + x) & 0x7F) : (uint8_t)(M(EntVar1 + x) | 0x80);
            flip = 1;
        } else if (a == 4) {
            flip = 1;
        }
        if (flip) {
            M(EntWait + x) = (uint8_t)(~M(EntWait + x) + 1); /* $9D37 */
        }
    }
    bouncer_bounce_velocity(g, x, M(EntVar1 + x)); /* $9D42 */
    if (M(EntVar1 + x) & 0x80) {
        unsigned f = (unsigned)((uint8_t)~M(EntVelYFrac + x)) + 1;
        M(EntVelYFrac + x) = (uint8_t)f;
        M(EntVelY + x) = (uint8_t)((uint8_t)~M(EntVelY + x) + (f > 0xFF));
    }
}
