/* Contact with the player, bonuses, barricades, crates, and the player's weapon hitting enemies. */
#include "game.h"
#include "persist.h"
#include "fixes.h"
#include "enemy.h"
#include "hal.h"
#include "player.h"
#include "sound.h"
#include "stage.h"
#include "enemies/foot_soldier.h"
#include "enemies/score.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

static void squash_entity(Game *g, int x, uint8_t type, uint8_t anim);

/* ---------------------------------------------------------- contact with the player (B6 $9784) --- */
/* B6 $965C: the player touches a bonus (entity of type < $10) */
static uint8_t capped_add(uint8_t a, uint8_t b) /* B6 $977A: sum capped at 99 */
{
    unsigned v = a + b;
    return v >= 0x63 ? 0x63 : (uint8_t)v;
}

static void pickup_done(Game *g, int x) /* B6 $970F: the bonus is marked taken, disappears, sound effect */
{
    M(PickedUpMask) |= R7(0xF137 + M(EntHealth + x));
    clear_entity(g, x);
    sound_play(g, 0x35);
}

static void player_pickup(Game *g, int x) {
    uint8_t type = M(EntType + x), t = M(CurrentTurtle);
    switch (type) {
        case 1:
        case 2:
        case 3: { /* pizzas: $80, $40, $20 of health */
            static const uint8_t heal[3] = {0x80, 0x40, 0x20};
            unsigned v = M(TurtleHealth0 + t) + heal[type - 1];
            M(TurtleHealth0 + t) = (v > 0xFF || v >= 0x80) ? 0x80 : (uint8_t)v;
            M(PickedUpMask) |= R7(0xF137 + M(EntHealth + x)); /* $96A2 */
            sound_play(g, 0x34);
            clear_entity(g, x);
            break;
        }
        case 5:
        case 6:
        case 7:
        case 8: { /* sub-weapons: the type gives the weapon, +20 ammo */
            uint8_t w = (uint8_t)(type - 4);
            if (w != M(SubWeaponType0 + t)) {
                M(SubWeaponType0 + t) = w;
                M(SubWeaponSelected) = 1;
                M(SubWeaponAmmo0 + t) = 0x14;
            } else {
                M(SubWeaponAmmo0 + t) = capped_add(M(SubWeaponAmmo0 + t), 0x14);
            }
            if (M(SubWeaponType0 + t) == 4) {
                M(PickedUpMask) |= R7(0xF137 + M(EntHealth + x));
            }
            clear_entity(g, x);
            sound_play(g, 0x35);
            break;
        }
        case 11: /* $9722 */
            M(EntStatus) |= 0x10;
            M(EntStatus) &= 0xFB;
            M(EntVar0) = 0;
            pickup_done(g, x);
            break;
        case 12:
            M(SpecialShotCount) = capped_add(M(SpecialShotCount), 0x0A);
            pickup_done(g, x);
            break;
        case 13:
            M(RopeCount) = capped_add(M(RopeCount), 0x03);
            pickup_done(g, x);
            break;
        case 14: /* $9756: a turtle comes back with all its health */
            M(TurtleHealth0 + M(RescuedTurtle)) = 0x80;
            wave_palette(g);
            set_chr_bank0(g, M(WaveSpriteSet));
            M(0x56) = 1;
            sound_play(g, 0x3D);
            clear_entity(g, x);
            break;
        case 4:
        case 10:
        case 15:
            break;
        case 9: /* $96B5: the turtle defuses a bomb */
            if (!M(TurtleHealth0 + t)) {
                break;
            }
            if (M(EntHealth + x) & M(BombsDefusedMask)) {
                break; /* already defused */
            }
            M(BombsDefusedMask) |= M(EntHealth + x);
            M(EntFlags) &= 0xDB;
            M(EntAttr) = 0;
            M(DefusedBombEntity) = (uint8_t)x;
            M(SceneId) = 6; /* $D0F9 -> B4 $A983 : scene $06 */
            clear_entity(g, 1);
            break;
        default:
            TODO("contact unknown bonus type");
            break;
    }
}

/* B6 $97FA: the barricade pushes the player back by 3 pixels and blocks its walk against it ($86) */
static void barricade_contact(Game *g, int x) {
    face_player_get_distance(g, x); /* $D13E -> B5 $8381 */
    if (M(EntAnimId + x) == 2) {    /* $9829: horizontal barricade */
        if (M(Tmp0 + 1) < 0x13) {
            int up = (uint8_t)(M(EntY + x) - 6) >= M(EntY);
            M(EntY) = (uint8_t)(M(EntY) + (up ? -3 : 3));
        }
    } else if (M(Tmp0) < 0x13) {
        M(EntX) = (uint8_t)(M(EntX) + ((M(EntFlags + x) & 1) ? -3 : 3));
    }
    uint8_t joy = M(JoyHeld1); /* $984F */
    if (!(joy & 0x0F)) {
        return;
    }
    if (joy & 0x0C) {
        if (M(EntAnimId + x) == 1) {
            return;
        }
        if (joy & 0x08) {
            if (M(Tmp0 + 2)) {
                return;
            }
        } else if (!M(Tmp0 + 2)) {
            return;
        }
    } else {
        if (M(EntAnimId + x) == 2) {
            return;
        }
        int flip = M(EntFlags + x) & 1;
        if (joy & 1) {
            if (!flip) {
                return;
            }
        } else if (flip) {
            return;
        }
    }
    M(0x86) = 1;
}

void player_hit_by_enemy(Game *g, int x) {
    uint8_t t = M(EntType + x);
    if (t < 0x10) {
        player_pickup(g, x);
        return;
    }
    if (M(EntStatus) & 0x10) {
        return; /* player in the air */
    }
    if (t == 0x10) { /* B6 $97C8: the player climbs into the van */
        if (M(0x03E5) || !(M(EntFlags) & 0x80)) {
            return;
        }
        M(InVanFlag) = 1;
        M(EntAnimId) = M(EntAnimId + x);
        M(EntX) = M(EntX + x);
        M(EntY) = M(EntY + x);
        M(EntState) = 2;
        sound_play(g, 0x32);
        clear_entity(g, x);
        return;
    }
    if (t == 0x2B) {
        barricade_contact(g, x);
        return;
    }
    if (t == 0x51) { /* B6 $97BC: the exit of the level */
        sound_play(g, 0x83);
        M(SceneId) = 0x10;
        clear_entity(g, x);
        return;
    }
    if (t == 0xA2) {
        crate_contact(g, x);
        return;
    }
    if (t == 0x40 && M(InVanFlag)) { /* B6 $97AF: the van crushes the enemy */
        sound_play(g, 0x22);
        persist_note_death(g, x);
        squash_entity(g, x, 0x2C, 0xAE);
        return;
    }
    if (M(EntFlags) & 4) {
        return; /* B6 $9896: already hit */
    }
    if (M(EntX + x) < M(EntX)) {
        M(EntStatus) &= 0x7F;
    } else {
        M(EntStatus) |= 0x80;
    }
    M(EntActionFlags + x) |= 1;
    M(Tmp0) = R6(0x8B01 + (uint8_t)(t + 4)); /* damage */
    if (M(InVanFlag)) {
        M(Tmp0) = 7;
    }
    uint8_t *h = &M(TurtleHealth0 + M(CurrentTurtle));
    if (*h) {
        uint8_t v = (uint8_t)(*h - M(Tmp0));
        *h = (v >= 0x81) ? 0 : v;
    }
    if (!*h) {
        health_display_update(g); /* $98E3 : jsr B6_ABF9 */
    }
    /* B7 $D0D8 -> B4 $A657: the player is hit (blinks, $24), X = the enemy */
    M(EntHurtTimer) = 0x18;
    M(EntFlags) |= 0x24;
    if (M(SideViewFlag)) {
        player_side_hurt(g, x);
    } else {
        if (!M(InVanFlag)) {
            M(EntState) = 0;
        }
        clear_vel_x(g, x);
        clear_vel_y(g, x);
    }
    sound_play(g, 0x25);
    if (t == 0x99 || t == 0x93) {
        clear_entity(g, x);
    }
}

/* B6 $9A5A: enemy x becomes a corpse / debris (type, animation) */
static void squash_entity(Game *g, int x, uint8_t type, uint8_t anim) {
    M(EntType + x) = type;
    M(EntAnimId + x) = anim;
    M(EntFlags + x) = (uint8_t)((M(EntFlags + x) & 1) | 0x82);
    M(EntAnimTimer + x) = 2;
    M(EntState + x) = 0;
    M(EntAnimFrame + x) = 0;
    M(EntAttr + x) = 0;
    M(EntHealth + x) = 0;
}

/* B6 $9A8C: the blow touches a barricade (type $2B) there: a small shot ($29) disappears, a missile destroys it for good */
static void barricade_hit(Game *g, int x, int y) {
    int small = M(EntType + y) == 0x29;
    clear_entity(g, y); /* $9AC9 */
    if (small) {
        return;
    }
    if (M(EntType + x) == 0x2C) {
        return;
    }
    uint8_t id = M(EntHealth + x);
    if (id < 0x10) {
        M(DamDestroyedMask0) |= R7(0xF137 + (uint8_t)(id - 1));
    } else {
        M(DamDestroyedMask1) |= R7(0xF137 + (id & 0x0F));
    }
    sound_play(g, 0x27);
    squash_entity(g, x, 0x2C, 0xAE);
}

/* B6 $9913: blow y (entity 0-4) touches enemy x */
void enemy_hit_by_weapon(Game *g, int x, int y) {
    M(0xD1) = (uint8_t)y;
    uint8_t type = M(EntType + x);
    if (type == 0x2B) {
        barricade_hit(g, x, y);
        return;
    }
    if (type == 0xA2) {
        crate_hit(g, x);
        return;
    }
    if (type == 0x2C || type == 0x15 || M(EntHealth + x) == 0xFF) {
        return;
    }
    sound_play(g, (type == 0x74 || (type == 0x75 && x == 5)) ? 0x41 : 0x21);
    M(EntFlags + x) |= 0x04;
    M(EntHurtTimer + x) = 0x18;
    uint8_t idx, w = M(EntType + y);
    if (w == 0x2A) {
        idx = 0x12;
    } else if (w == 0x29) {
        idx = 0x13;
    } else {
        uint8_t a;
        if (M(EntStatus) & 0x10) {
            a = 8;
        } else if (M(SubWeaponSelected)) {
            a = (uint8_t)(M(SubWeaponType0 + M(CurrentTurtle)) + 3);
        } else {
            a = M(CurrentTurtle);
        }
        M(0x03EF) = (uint8_t)(a << 1);
        idx = M(0x03EF);
        if (M(TurtleHealth0 + M(CurrentTurtle)) < 0x40) {
            idx++;
        }
    }
    uint8_t hp = M(EntHealth + x), dmg = R6(0x9BCB + idx);
    if (fix_enabled(FIX_BALANCE_DAMAGE) && idx < 8) { /* PIZZA-006: normal attack (not the jump, not a sub-weapon, not a special weapon): 14 / 20 / 14 / 15 */
        static const uint8_t balance[8] = {14, 17, 20, 20, 14, 19, 15, 16}; /* turtle * 2 (+ 1 at low health): the original's bonus at low health is kept */
        dmg = balance[idx];
    }
    if (hp > dmg) {
        M(EntHealth + x) = (uint8_t)(hp - dmg);
        return;
    }
    M(EntHealth + x) = 0; /* the enemy is defeated */
    persist_note_death(g, x);
    if (type >= 0x70 && type < 0x80) {
        M(0xD2) = R6(0x9A7F + (type & 0x0F));
        M(0x03D8) = 0;
    }
    if (type == 0x73) {
        clear_entity(g, 13);
    }
    if (type == 0x79 || type == 0x75) {
        M(EntWait + x) = 0xFF;
        if (x == 5) {
            M(0xD2) = 1;
        }
        return;
    }
    if (type == 0x70) {
        M(EntType + 9) = 0;
    }
    if (M(WaveState) != 3 || type == 0x98 || type == 0x8D) {
        sound_play(g, 0x22); /* $99FC */
    } else if (type == 0x76) {
        sound_play(g, 0x42);
    }
    enemy_score(g, x);
    if (M(EntType + y) == 0x29 || M(EntType + y) == 0x1A) {
        clear_entity(g, y); /* the projectile disappears */
    }
    uint8_t nt, na;
    if (!M(SideViewFlag)) {
        if (M(EntType + x) >= 0x12 && M(EntType + x) < 0x15) {
            sound_play(g, 0x01);
        }
        nt = 0x2C;
        na = 0xAE;
    } else {
        nt = 0x15;
        na = 0xD3;
    }
    M(EntType + x) = nt;
    M(EntAnimId + x) = na;
    M(EntFlags + x) = (uint8_t)((M(EntFlags + x) & 1) | 0x82);
    M(EntAnimTimer + x) = 2;
    M(EntState + x) = 0;
    M(EntAnimFrame + x) = 0;
    M(EntAttr + x) = 0;
    M(EntHealth + x) = 0;
}
