/* Type $9C: passer-by. */
#include "game.h"
#include "player.h"
#include "enemy.h"
#include "enemies/foot_soldier.h"
#include "enemies/patrollers.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* --- type $9C (B5 $87F8): passer-by, changes direction every 8 frames ($1E bit 3) --- */
void ai_passer_by(Game *g, int x) {
    if (!(M(FrameCounter) & 8)) {
        return;
    }
    face_player_get_distance(g, x); /* $8381 */
    if (M(Tmp0) >= 8) {
        if (!(M(EntFlags + x) & 1)) {
            patrol_right(g, x); /* the carry of $882D is ignored */
        }
        if (patrol_left(g, x)) {
            return;
        }
    }
    if (M(Tmp0 + 1) >= 8) {
        if (M(Tmp0 + 2) && patrol_up(g, x)) {
            return;
        }
        if (patrol_down(g, x)) {
            return;
        }
    }
    clear_vel_x(g, x);
    clear_vel_y(g, x); /* $EFA8 */
}
