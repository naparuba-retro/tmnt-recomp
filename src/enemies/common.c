/* Helpers shared by the enemy AIs (side-view movement, collisions, vectors). */
#include "game.h"
#include "player.h"
#include "enemy.h"
#include "enemies/foot_soldier.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* ------------------------------------------------------------ update (B5 $8001) --- */
/* ---------------------------------------------- helpers of the side-view enemies (B5 $8381-$85B1) --- */
int in_block_list(uint8_t t) {
    return t == 4 || t == 0x0C || t == 0x0D;
} /* B5 $844E : table $8462 */

int front_tile_set(Game *g, int x, uint8_t a, uint8_t dy) /* B5 $83D6: carry = the tile ahead is not empty */
{
    uint8_t dx = (M(EntFlags + x) & 1) ? a : (uint8_t)(0 - a);
    return tile_at_offset(g, x, dx, dy) != 0;
}

int move_blocked(Game *g, int x, uint8_t a, uint8_t dy) /* B5 $8466: carry = an obstacle on the way */
{
    uint8_t dx = (M(EntVelX + x) & 0x80) ? (uint8_t)(0 - a) : a;
    dx = (uint8_t)(dx + M(EntVelX + x));
    if (in_block_list(tile_at_offset(g, x, dx, dy))) {
        return 1;
    }
    if (in_block_list(tile_at_offset(g, x, dx, 0xF2))) {
        return 1;
    }
    return in_block_list(tile_at_offset(g, x, dx, 0xFA));
}

void turn_around(Game *g, int x) /* B5 $83F4: changes direction and reverses the velocity */
{
    M(EntFlags + x) ^= 1;
    unsigned v = (unsigned)(0x10000 - (M(EntVelX + x) << 8 | M(EntVelXFrac + x))) & 0xFFFF;
    M(EntVelXFrac + x) = (uint8_t)v;
    M(EntVelX + x) = (uint8_t)(v >> 8);
}

void walk_speed(Game *g, int x, uint8_t whole, uint8_t frac) /* B5 $8593: horizontal velocity according to the orientation */
{
    if (M(EntFlags + x) & 1) {
        unsigned v = (unsigned)(0x10000 - (whole << 8 | frac)) & 0xFFFF;
        whole = (uint8_t)(v >> 8);
        frac = (uint8_t)v;
    }
    set_vel_x(g, x, frac, whole); /* B7 $F081 */
}

int land_check(Game *g, int x, uint8_t dx, uint8_t dy) /* B5 $8421: lands on the ground; carry = landed */
{
    if (!in_block_list(tile_at_offset(g, x, dx, dy))) {
        return 0;
    }
    for (uint8_t n = 0x20;;) {
        if (--n == 0) {
            break;
        }
        M(EntY + x)--;
        if (!in_block_list(tile_at_offset(g, x, dx, dy))) {
            break;
        }
    }
    M(EntState + x) = 0;
    clear_vel_x(g, x);
    clear_vel_y(g, x);
    return 1;
}

void add_vel_y(Game *g, int x, uint8_t a) /* B7 $F013: gravity */
{
    unsigned s = M(EntVelYFrac + x) + a;
    M(EntVelYFrac + x) = (uint8_t)s;
    M(EntVelY + x) = (uint8_t)(M(EntVelY + x) + (s > 0xFF));
}

void ai_walker_start(Game *g, int x) /* B5 $978E: walker, first frame */
{
    walk_speed(g, x, 1, 0x60);
    M(EntRoutine + x) = M(EntType + x); /* $8644 */
    M(EntState + x)++;
}

void walker_patrol_step(Game *g, int x) /* B5 $9851 */
{
    if (!front_tile_set(g, x, 0x0C, 0xFF)) { /* no more ground ahead: it falls */
        M(EntRoutine + x)++;
        M(EntVelY + x) = 2;
        clear_vel_x(g, x);
        return;
    }
    if (move_blocked(g, x, 0x0C, 0xF4)) {
        turn_around(g, x);
    }
}

void ai_charger(Game *g, int x) /* B5 $97D7: charges left or right according to the scrolling */
{
    M(EntRoutine + x) = 0x81; /* $8644 */
    M(EntState + x)++;
    if (!(M(CameraDeltaX) & 0x80)) {
        M(EntFlags + x) |= 1;
    } else {
        M(EntFlags + x) &= 0xFE;
    }
    walk_speed(g, x, 2, 0x80);
}

/* ------------------------------------------------------------ bosses ($70-$7F): B5 $AA96-$AC0B --- */
int ai_leak_x = -1;

/* --- velocities towards a direction (B5 $A944, $A9B0, $A88E, $A8C9, $A52D) --- */
static void neg16_ram(Game *g, int lo, int hi) /* lo/hi: RAM addresses; 16-bit two's complement */
{
    unsigned f = (uint8_t)~M(lo) + 1u;
    M(lo) = (uint8_t)f;
    M(hi) = (uint8_t)((uint8_t)~M(hi) + (f > 0xFF));
}

static void direction_vector(Game *g, uint8_t a) /* $A9B0: M0/M1 = X velocity (integer, fraction), M2/M3 = Y velocity; signs according to M8 */
{
    uint8_t y = (uint8_t)(a << 1);
    M(Tmp1) = R5(0xAA76 + y);
    M(Tmp0) = R5(0xAA86 + y);
    if (M(Tmp8) & 2) {
        neg16_ram(g, 0x01, 0x00);
    }
    M(Tmp3) = R5(0xAA77 + y);
    M(Tmp2) = R5(0xAA87 + y);
    if (!(M(Tmp8) & 1)) {
        neg16_ram(g, 0x03, 0x02);
    }
}

void aim_at(Game *g, int x) /* $A944: M0/M1 = target point; M8 = quadrant, M9 = direction, then the vector */
{
    M(Tmp8) = 0;
    uint8_t d = (uint8_t)(M(Tmp0) - M(EntX + x));
    if (M(Tmp0) < M(EntX + x)) {
        M(Tmp0) = (uint8_t)(~d + 1);
        M(Tmp8) = 2;
    } else {
        M(Tmp0) = d;
    }
    M(Tmp4) = M(Tmp0);
    d = (uint8_t)(M(Tmp1) - M(EntY + x));
    if (M(Tmp1) < M(EntY + x)) {
        M(Tmp1) = (uint8_t)(~d + 1);
    } else {
        M(Tmp1) = d;
        M(Tmp8)++;
    }
    M(Tmp5) = M(Tmp1);
    M(Tmp0) >>= 4;
    M(Tmp9) = (uint8_t)((M(Tmp1) & 0xF0) + M(Tmp0));
    uint8_t t = R5(0xA9F6 + (M(Tmp9) >> 1));
    M(Tmp9) = (M(Tmp9) & 1) ? (uint8_t)(t >> 4) : (uint8_t)(t & 0x0F);
    direction_vector(g, M(Tmp9));
}

void set_velocity_from_m(Game *g, int x) /* $A52D */
{
    M(EntVelX + x) = M(Tmp0);
    M(EntVelXFrac + x) = M(Tmp1);
    M(EntVelY + x) = M(Tmp2);
    M(EntVelYFrac + x) = M(Tmp3);
}

void aim_index(Game *g, int x) /* $A88E then $A52D: M0 = direction 0-31 */
{
    M(Tmp8) = R5(0xA8A5 + (M(Tmp0) >> 3));
    M(Tmp9) = R5(0xA8A9 + M(Tmp0));
    direction_vector(g, M(Tmp9));
    set_velocity_from_m(g, x);
}
