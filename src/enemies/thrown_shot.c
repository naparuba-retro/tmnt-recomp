/* Type $A5: projectile thrown by an enemy. */
#include "game.h"
#include "player.h"
#include "enemy.h"
#include "enemies/bouncer.h"
#include "enemies/walker_dropper.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- type $A5 (B5 $9B93-$9C51): projectile thrown by an enemy ($0630 = direction) --- */
void ai_thrown_shot(Game *g, int x) {
    uint8_t d = M(EntVar0 + x);
    uint8_t y = (uint8_t)(d << 1);
    M(EntX + x) = (uint8_t)(M(EntX + x) + R5(0x9C2F + y));
    M(EntY + x) = (uint8_t)(M(EntY + x) + R5(0x9C30 + y));
    if (d >= 3) { /* $9BB4: to the left, opposite X velocity */
        M(Tmp0) = (uint8_t)(4 - d);
        y = (uint8_t)(M(Tmp0) << 2);
        unsigned f = (uint8_t)~R5(0x9C24 + y) + 1u;
        M(EntVelXFrac + x) = (uint8_t)f;
        M(EntVelX + x) = (uint8_t)((uint8_t)~R5(0x9C23 + y) + (f > 0xFF));
    } else { /* $9BDE */
        M(Tmp0) = d;
        y = (uint8_t)(d << 2);
        M(EntVelX + x) = R5(0x9C23 + y);
        M(EntVelXFrac + x) = R5(0x9C24 + y);
    }
    M(EntVelY + x) = R5(0x9C25 + y);
    M(EntVelYFrac + x) = R5(0x9C26 + y);
    anim_if_changed(g, x, (uint8_t)(M(Tmp0) + M(EntAnimId + x)));
    if (d >= 3) {
        M(EntFlags + x) |= 0x01;
    } else {
        M(EntFlags + x) &= 0xFE;
    }
    set_state(g, x, 0x1F);
}

void thrown_shot_scenery_state(Game *g, int x) /* B5 $9C39: disappears on contact with the scenery */
{
    uint8_t y = (uint8_t)(M(EntVar0 + x) << 1);
    if (tile_in_wall_set(g, x, R5(0x9C53 + y), R5(0x9C52 + y))) {
        clear_entity(g, x);
    }
}
