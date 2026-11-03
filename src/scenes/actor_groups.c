/* Groups of cutscene actors. */
#include "game.h"
#include "player.h"
#include "scenes.h"
#include "scenes/actor_groups.h"
#include "scenes/actors.h"
#include "scenes/scene_common.h"
#define TODO_AREA "scene" /* prefix of the TODO traces of this file */

/* --- actor groups (B2 $B8B7 / $B918) --- */
void group_setup(Game *g, uint8_t grp) {
    M(ActorGroupId) = grp;
    uint16_t p = rom_word(g->rom, 2, (uint16_t)(0xB956 + grp * 2));
    uint8_t n = R2(p);
    M(ActorGroupCount) = M(ActorGroupRemaining) = n;
    for (int i = 0; i < n; i++) {
        M(EntType + i) = R2(p + 1 + i);
    }
    for (int x = 0; M(ActorGroupRemaining); x++, M(ActorGroupRemaining)--) {
        entity_from_type(g, x);
        actor_init(g, x, (uint8_t)(M(EntType + x) - 0xAE));
    }
}

void group_frame(Game *g) {
    M(ActorGroupRemaining) = M(ActorGroupCount);
    for (int x = 0; M(ActorGroupRemaining); x++, M(ActorGroupRemaining)--) {
        actor_frame(g, x, (uint8_t)(M(EntType + x) - 0xAE));
        move_entity_y(g, x);
        move_entity_x(g, x);
    }
}
