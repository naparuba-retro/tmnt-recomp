/* Type $97: bird that glides around the player. */
#include "game.h"
#include "player.h"
#include "enemy.h"
#include "enemies/bouncer.h"
#include "enemies/purple_ninja.h"
#include "enemies/spydrone.h"
#include "enemies/walker_dropper.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- type $97 (B5 $9A0E) and its state $1C (B5 $9A8F): bird that glides around the player and throws $1B --- */
static void bird_aim(Game *g, int x) /* B5 $9A16: aims at a point beside the player, velocities towards it */
{
    uint8_t k = (uint8_t)((R5(0xA903 + (M(0x03C1) | 0x0F)) & 7) << 1); /* $A8F6 then $A913 */
    M(0x03C1)++;
    M(Tmp0) = (uint8_t)(R5(0xA934 + k) + M(EntX));
    M(Tmp1) = (uint8_t)(R5(0xA935 + k) + M(EntY));
    vector_to(g, x); /* $A944: $00-$03 = velocities, $04 / $05 = distances */
    M(Tmp4) >>= 1;
    M(EntVar0 + x) = (uint8_t)(M(Tmp4) + (M(Tmp5) >> 1));
    if (!M(EntVar0 + x)) {
        M(EntVar0 + x) = 1;
    }
    uint8_t d = R5(0xA8D6 + ((M(Tmp8) << 3) + M(Tmp9))); /* $A8C9 */
    M(EntVar1 + x) = d;
    M(EntVelX + x) = M(Tmp0);
    M(EntVelY + x) = M(Tmp2);
    M(EntVelXFrac + x) = M(Tmp1);
    M(EntVelYFrac + x) = M(Tmp3);
    uint8_t anim = (uint8_t)(R5(0x9A6F + d) + 0x8B);
    if (anim_if_changed(g, x, anim) >= 0x8D) {
        M(EntFlags + x) |= 1;
    } else {
        M(EntFlags + x) &= 0xFE; /* the cmp #$8D follows an A modified by $A87B */
    }
    M(EntWait + x) = 0xFF;
}

static void bird_face_player(Game *g, int x) /* B5 $9B3F */
{
    M(Tmp0) = M(EntX);
    M(Tmp1) = M(EntY);
    vector_to(g, x);
    uint8_t d = R5(0xA8D6 + ((M(Tmp8) << 3) + M(Tmp9)));
    uint8_t anim = (uint8_t)(R5(0x9A6F + d) + 0x8B);
    if (anim_if_changed(g, x, anim) >= 0x8D) {
        M(EntFlags + x) |= 1;
    } else {
        M(EntFlags + x) &= 0xFE; /* the cmp #$8D follows an A modified by $A87B */
    }
}

void ai_bird(Game *g, int x) {
    bird_aim(g, x);
    set_state(g, x, 0x1C);
}

void bird_glide_state(Game *g, int x) {
    if (++M(EntTimer + x) == 0x20) {
        M(EntTimer + x) = 0;
        int c = spawn_child_slot(g, x, 0x1B);
        if (c >= 0) {
            M(EntVar0 + c) = (uint8_t)(M(EntAnimId + x) - 0x8B);
        }
    }
    uint8_t v = M(EntVar1 + x);
    uint16_t tb = (v & 7) ? 0x9B73 : 0x9B7B;
    uint8_t y = (uint8_t)((v & 0xF8) >> 2); /* $9AB8 / $9ACE: two lsr */
    uint8_t dy = R5(tb + y), dx = R5(tb + y + 1);
    if (tile_in_wall_set(g, x, dx, dy)) { /* $9AE7: obstacle, the direction turns */
        M(EntVar1 + x) = (uint8_t)((M(EntVar1 + x) + 8) & 0x18);
        uint8_t k = (uint8_t)(M(EntVar1 + x) >> 1);
        M(EntVelX + x) = R5(0x9B83 + k);
        M(EntVelXFrac + x) = R5(0x9B84 + k);
        M(EntVelY + x) = R5(0x9B85 + k);
        M(EntVelYFrac + x) = R5(0x9B86 + k);
    } else {
        uint8_t a = M(EntWait + x);
        if (!a) {
            bird_aim(g, x);
            return;
        } /* $9B70 */
        if (a == 0xFF) {
            M(EntVar0 + x)--;
            if (!M(EntVar0 + x)) {
                M(EntWait + x) = 0x40;
            }
        } else {
            M(EntWait + x)--;
            M(EntVelY + x) = M(EntVelYFrac + x) = M(EntVelX + x) = M(EntVelXFrac + x) = 0;
        }
    }
    bird_face_player(g, x);
}

void shrapnel_state(Game *g, int x) /* B5 $96EB: state $96, the explosion throws four fragments then disappears */
{
    if (M(EntAnimFrame + x) != 0xFF) {
        return;
    }
    for (int k = 3; k >= 0; k--) {
        int n = spawn_child_slot(g, x, 7);
        if (n < 0) {
            continue;
        }
        M(EntVelX + n) = R5(0x971E + 2 * k);
        M(EntVelY + n) = R5(0x971F + 2 * k);
        M(EntAnimId + n) = 0x12;
    }
    clear_entity(g, x);
}
