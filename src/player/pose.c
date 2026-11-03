/* Player animation and poses. */
#include "game.h"
#include "anim.h"
#include "player.h"
#include "sound.h"
#include "player/pose.h"
#include "player/tiles.h"
#define TODO_AREA "player" /* prefix of the TODO traces of this file */

/* --- animation and states --- */
void player_set_pose(Game *g, uint8_t a) /* B4 $A634 */
{
    if (M(EntStatus) & 0x10) {
        return;
    }
    if (a == M(EntAnimId)) {
        return;
    }
    set_animation(g, 0, a);
}

int is_special_area(Game *g) /* B4 $996F: sets $D3 */
{
    if (M(AreaNumber) == 1 && M(LevelNumber) == 1 && M(SideViewFlag)) {
        M(SwimZoneFlag) = 1;
        return 1;
    }
    M(SwimZoneFlag) = 0;
    return 0;
}

void player_idle(Game *g) /* B4 $9F84 */
{
    clear_vel_x(g, 0);
    clear_vel_y(g, 0);
    M(EntState) = 0;
    probe_tile(g, 0x07, 0x09);
    if (!probe_tile(g, 0x11, 0x00)) {
        return;
    }
    if (M(Tmp0) == 0x09 || M(Tmp0) == 0x0A) {
        probe_tile(g, 0x0D, 0x06);
    }
}

/* attack sound effect per turtle: B4 $9F7C; the 4th turtle has $10 but $12 in profile */
void attack_sound(Game *g) /* B4 $9F5C */
{
    uint8_t s = R4(0x9F7C + M(CurrentTurtle));
    if (s == 0x10 && M(SideViewFlag)) {
        s = 0x12;
    }
    sound_play(g, s);
}

void player_attack_end(Game *g) /* B4 $9F35 */
{
    M(EntStatus) &= 0xFB;
    if (M(EntAnimId + 1) < 0xD0) {
        clear_entity(g, 1);
    }
}

void overhead_weapon_start(Game *g) /* B7 $E132: top-view weapon */
{
    M(EntFlags + 1) = 0xC2;
    M(EntAnimTimer + 1) = M(EntAnimTimer);
    M(EntAnimId + 1) = rom_prg(g->rom, B7, (uint16_t)(0xE152 + (uint8_t)(M(CurrentTurtle) * 4 + M(EntRoutine))));
}

void weapon_effect_start(Game *g) /* B7 $E125 (+ $E144) */
{
    M(EntFlags + 1) = 0xC2;
    M(EntAnimTimer + 1) = M(EntAnimTimer);
    M(EntAnimId + 1) = (uint8_t)(M(EntAnimId) + 0x0C);
}
