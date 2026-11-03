/* Enemies: update, dispatch tables (type -> routine) and the routes report. The enemies themselves are in enemies/. */
#include "game.h"
#include "anim.h"
#include "enemy.h"
#include "player.h"
#include "stage_play.h"
#include "enemies/bird.h"
#include "enemies/boss_final.h"
#include "enemies/bouncer.h"
#include "enemies/chansawiac.h"
#include "enemies/chaser_jumper.h"
#include "enemies/common.h"
#include "enemies/foot_soldier.h"
#include "enemies/homing_shot.h"
#include "enemies/jumping_thrower.h"
#include "enemies/palette_band.h"
#include "enemies/passer_by.h"
#include "enemies/patrollers.h"
#include "enemies/platform_shooter.h"
#include "enemies/player_projectiles.h"
#include "enemies/purple_ninja.h"
#include "enemies/rock_soldier.h"
#include "enemies/rocksteady.h"
#include "enemies/roof_leaper.h"
#include "enemies/satellite.h"
#include "enemies/sewer.h"
#include "enemies/spider_platform.h"
#include "enemies/spydrone.h"
#include "enemies/swooper.h"
#include "enemies/thrown_shot.h"
#include "enemies/turrets.h"
#include "enemies/van_shadow.h"
#include "enemies/walker_dropper.h"
#include "enemies/walker_family.h"
#define TODO_AREA "enemy" /* prefix of the TODO traces of this file */

/* Routines of the anim-state table ($82E3): the 6502 jumps to the address read from the ROM; each address is routed to its C function. */
static void route_fall(Game *g, int x) /* $8A7F */
{
    add_vel_y(g, x, 0x24);
    if (front_tile_set(g, x, 0x0C, 0x06)) {
        land_check(g, x, 0, 0xFC);
    }
}
static void route_clear_when_anim_ends(Game *g, int x) {
    if (M(EntAnimFrame + x) == 0xFF) {
        clear_entity(g, x);
    }
} /* $8357 */
static void route_clear_on_wall_tile(Game *g, int x) {
    if (tile_in_wall_set(g, x, 0, 0)) {
        clear_entity(g, x);
    }
} /* $9D9E: state $1E */
static void route_ballistic_land_aa(Game *g, int x) /* $8CD4: state $05: ballistic jump, landing -> animation $AA */
{
    ballistic(g, x, 1, 0);
    if (land_check(g, x, 0, 0xFC)) {
        set_animation(g, x, 0xAA);
        clear_vel_x(g, x);
        clear_vel_y(g, x);
        M(EntRoutine + x) = 3;
        M(EntState + x)++;
    }
}
static void route_landing_step(Game *g, int x) {
    landing_step(g, x);
} /* $8CE7: state $06: gravity then landing */
static void route_ballistic_land_idle(Game *g, int x) /* $933B: states $12 / $25: ballistic jump, at landing $8336 */
{
    ballistic(g, x, 1, 0);
    if (land_check(g, x, 0, 0xFC)) {
        M(EntState + x) = 0;
        M(EntRoutine + x) = 0;
        M(EntAnimFrame + x) = 0;
    }
}
static void route_anim_end_to_18(Game *g, int x) {
    if (M(EntAnimFrame + x) == 0xFF) {
        M(EntRoutine + x) = 0x18;
    }
} /* $934C / $93C6 */
static void route_landing_to_18(Game *g, int x) {
    if (landing_step(g, x)) {
        M(EntRoutine + x) = 0x18;
    }
} /* $93BD */
static void route_anim_end_to_50(Game *g, int x) /* $93D3 */
{
    if (M(EntAnimFrame + x) == 0xFF) {
        M(EntAttr + x) = 2;
        set_animation(g, x, 0x50);
        M(EntState + x) = 0;
        M(EntRoutine + x) = 0;
        M(EntAnimFrame + x) = 0;
    }
}
static void route_projectile_wall(Game *g, int x) {
    if (tile_at_offset(g, x, 0, 0xFC) == 4) {
        clear_entity(g, x);
    }
} /* $8A6A / $8CFE: projectile: stops at the wall */
static void route_turret_state9(Game *g, int x) {
    turret_state(g, x, 9);
}
static void route_turret_state10(Game *g, int x) {
    turret_state(g, x, 10);
}
static void route_turret_state11(Game *g, int x) {
    turret_state(g, x, 11);
}
static void route_turret_state12(Game *g, int x) {
    turret_state(g, x, 12);
}

static const Route anim_state_routes[] = {
    {0x8341, route_nop}, /* rts */
    {0x8A3B, anim_state_arc},
    {0x8A7F, route_fall},
    {0x832F, wait_anim_end},
    {0x8D7F, walker_dropper_drop_state},
    {0x8357, route_clear_when_anim_ends},
    {0x8DA9, walker_dropper_transform_state},
    {0x9CA5, bouncer_state},
    {0x9E03, chaser_substate_dispatch},
    {0x9C39, thrown_shot_scenery_state},
    {0xA0F5, platform_shooter_state},
    {0xA3E5, swooper_trajectory_state},
    {0xB45F, homing_shot_state},
    {0xA62D, palette_band_state},
    {0x8D13, walker_toss_state},
    {0x9A8F, bird_glide_state},
    {0x8342, walker_recover_state},
    {0x8D2E, walker_start_walk_state},
    {0x8D44, walker_approach_state},
    {0x8D6E, walker_hop_state},
    {0x9006, route_turret_state9},
    {0x900F, route_turret_state10},
    {0x9018, route_turret_state11},
    {0x9021, route_turret_state12},
    {0x9D9E, route_clear_on_wall_tile},
    {0x8CD4, route_ballistic_land_aa},
    {0xA357, spydrone_chase_state},
    {0x8CE7, route_landing_step},
    {0x933B, route_ballistic_land_idle},
    {0x934C, route_anim_end_to_18},
    {0x9359, rock_soldier_walk_state},
    {0x93BD, route_landing_to_18},
    {0x93C6, route_anim_end_to_18},
    {0x93D3, route_anim_end_to_50},
    {0x8A6A, route_projectile_wall},
    {0x8CFE, route_projectile_wall},
};

static void run_anim_state(Game *g, int x) {
    uint16_t st = rom_word(g->rom, 5, (uint16_t)(0x82E3 + (uint8_t)(M(EntRoutine + x) << 1)));
    if (!ROUTE_RUN(anim_state_routes, st, g, x)) {
        TRACE("f=%u TODO enemy anim state %02X (routine $%04X)\n", g->frame, M(EntRoutine + x), st);
    }
}

/* Routines of the state table of the types >= $80 ($9477, read through EntRoutine) */
static void route_state_81_advance(Game *g, int x) {
    if (move_blocked(g, x, 0x0A, 0xF6)) {
        turn_around(g, x);
    }
} /* $9833: $81: advances */
static void route_state_fall(Game *g, int x) {
    add_vel_y(g, x, 0x18);
    land_check(g, x, 0, 0xFB);
} /* $9870: fall */

static const Route high_state_routes[] = {
    {0x9464, route_nop},
    {0x9851, walker_patrol_step},
    {0x987C, spider_walk},
    {0x98D7, spider_climb},
    {0x9908, spider_drop},
    {0x969F, ninja_state_fall},
    {0x96EB, shrapnel_state},
    {0x983F, ninja_state_jump},
    {0x832F, wait_anim_end},
    {0x9726, sewer_walker_leap},
    {0x9833, route_state_81_advance},
    {0x9870, route_state_fall},
};

static void route_typea7_first(Game *g, int x) {
    ai_platform_shooter(g, x, 0);
}
static void route_typea7_second(Game *g, int x) {
    ai_platform_shooter(g, x, 1);
}

/* EnemyAITable of the types >= $80 ($940A) */
static const Route high_type_routes[] = {
    {0x9464, route_nop},
    {0x877E, ai_patroller_a},
    {0x87C0, ai_patroller_b},
    {0x978E, ai_walker_start},
    {0x97D7, ai_charger},
    {0x99E5, ai_charger}, /* $96: same code as $81 */
    {0x864B, ai_purple_ninja},
    {0x982B, ai_roof_leaper},
    {0x94A5, ai_sewer_walker},
    {0xA34F, ai_spydrone},
    {0x979B, ai_leaper},
    {0x9977, ai_fast_walker},
    {0x8A94, ai_defensive_hopper},
    {0x9C5C, ai_bouncer},
    {0x87F8, ai_passer_by},
    {0x9D88, ai_bouncer_shot},
    {0xA34C, ai_bouncer_shot},
    {0x91C6, ai_chansawiac},
    {0x9288, ai_rocksteady},
    {0x92F3, ai_rocksteady_shot},
    {0x9983, ai_april},
    {0x99D9, ai_april_shot},
    {0x8708, ai_purple_ninja_shot},
    {0x9DAB, ai_chaser_jumper},
    {0x9B93, ai_thrown_shot},
    {0xA3C3, ai_swooper},
    {0xA59F, ai_palette_band},
    {0x94AF, ai_hopper},
    {0x9A0E, ai_bird},
    {0x94BE, ai_spider_anchor},
    {0x9539, ai_spider_platform},
    {0x8B05, ai_walker_hopper},
    {0x8B8D, ai_walker_charge_left},
    {0x8B99, ai_walker_stop_and_throw},
    {0x8C06, ai_walker_charge_right},
    {0xA0BF, route_typea7_first},
    {0xA0BA, route_typea7_second},
};

static void route_turret(Game *g, int x) {
    ai_turret(g, x, M(EntType + x));
}
static void route_boss74_shot_4(Game *g, int x) {
    ai_boss74_shot(g, x, 8, 4);
}
static void route_boss74_shot_7(Game *g, int x) {
    ai_boss74_shot(g, x, 7, 7);
}
static void route_boss74_shot_8(Game *g, int x) {
    ai_boss74_shot(g, x, 4, 8);
}
static void route_boss74_shot_9(Game *g, int x) {
    ai_boss74_shot(g, x, 0, 9);
}
static void route_set_state_0f(Game *g, int x) {
    set_state(g, x, 0x0F);
}

/* EnemyAITable of the types $10-$7F ($823B) */
static const Route low_type_routes[] = {
    {0x82D0, route_nop}, /* rts: motionless scenery */
    {0x8907, ai_foot_soldier},
    {0x8733, ai_defeated_enemy},
    {0x9249, ai_rock_soldier},
    {0x927C, ai_rock_soldier_crescent},
    {0xB3E2, ai_hostage_boss_shot},
    {0x8E79, ai_dam_scenery_swap},
    {0x8E9B, ai_scenery_swap},
    {0x9122, ai_sewer_thrower},
    {0x919B, ai_sewer_thrower_shot},
    {0x8DE2, rotbar_visibility},
    {0x8DE5, ai_rotbar},
    {0xCFD5, ai_van},
    {0x8C12, ai_walker_dropper},
    {0xB331, ai_van_shadow},
    {0xB442, ai_homing_shot},
    {0x89CC, ai_jumping_thrower},
    {0x8EBD, route_turret},
    {0x8ECA, route_turret},
    {0x8ED7, route_turret},
    {0x8EEB, route_turret},
    {0x8F0E, route_turret},
    {0x8F1C, route_turret},
    {0x8F2A, route_turret},
    {0x8F38, route_turret},
    {0x8F64, ai_turret_shot},
    {0x8FE0, ai_slow_advancer},
    {0xBEF0, ai_final_boss_sword},
    {0xCF7A, ai_orbiting_satellite},
    {0xCF87, ai_satellite_shot},
    {0xB3FF, route_boss74_shot_4},
    {0xB40D, route_boss74_shot_7},
    {0xB41B, route_boss74_shot_8},
    {0xB429, route_boss74_shot_9},
    {0x90D7, ai_close_attacker},
    {0x8A0F, ai_thrower_recover},
    {0x8C8D, ai_flyer},
    {0x8CCF, route_set_state_0f},
    {0xFD6A, ai_van_shot},
    {0xFD7E, ai_shuriken},
    {0xFD9A, subweapon_offscreen},
    {0xFDBE, ai_boomerang},
};

void run_entity_ai(Game *g, int x) /* B5 $802C RunEntityAI + $8212 EnemyAIDispatch */
{
    uint8_t type = M(EntType + x);
    if (type & 0x80) { /* $803E / $93F0 / $93F8 */
        if (M(EntState + x) && (M(EntAnimId + x) & 0x80)) {
            run_anim_state(g, x);
            return;
        } /* $8212 -> $82D1 */
        if (M(EntState + x)) { /* $9465: routine according to $0520 */
            uint16_t st = rom_word(g->rom, 5, (uint16_t)(0x9477 + (uint8_t)(M(EntRoutine + x) << 1)));
            if (!ROUTE_RUN(high_state_routes, st, g, x)) {
                TRACE("f=%u TODO enemy state of type %02X (routine $%04X)\n", g->frame, M(EntRoutine + x), st);
            }
            return;
        }
        uint16_t ai = rom_word(g->rom, 5, (uint16_t)(0x940A + (uint8_t)(type << 1)));
        if (!ROUTE_RUN(high_type_routes, ai, g, x)) {
            TRACE("f=%u TODO enemy AI of type %02X (routine $%04X)\n", g->frame, type, ai);
        }
        return;
    }
    if (type >= 0x70) {
        boss_ai(g, x);
        return;
    }
    if (M(EntState + x)) {
        run_anim_state(g, x);
        return;
    } /* $82D1 */
    if (type < 0x10) { /* $82C1: bonus, limited lifetime ($FF = unlimited) */
        if (M(EntVar0 + x) != 0xFF && --M(EntVar0 + x) == 0) {
            clear_entity(g, x);
        }
        return;
    }
    uint16_t ai = rom_word(g->rom, 5, (uint16_t)(0x823B + (uint8_t)((type - 0x10) << 1))); /* EnemyAITable */
    if (!ROUTE_RUN(low_type_routes, ai, g, x)) {
        TRACE("f=%u TODO enemy AI of type %02X (routine $%04X)\n", g->frame, type, ai);
    }
}

/* --routes report: the routine addresses found in the ROM dispatch tables that have no entry in the Route tables (= not ported). */
static void report_unrouted(Game *g, FILE *out, const char *what, const Route *table, int n, int bank, uint16_t base, int count) {
    for (int i = 0; i < count; i++) {
        uint16_t addr = rom_word(g->rom, bank, (uint16_t)(base + i * 2));
        int found = 0;
        for (int k = 0; k < n; k++) {
            if (table[k].rom_addr == addr) {
                found = 1;
            }
        }
        if (!found) {
            fprintf(out, "%s %02X: routine $%04X not ported\n", what, i, addr);
        }
    }
}

void enemy_routes_report(Game *g, FILE *out) {
    report_unrouted(g, out, "enemy type", low_type_routes, (int)(sizeof low_type_routes / sizeof *low_type_routes), 5, 0x823B, 0x2D);
    report_unrouted(g, out, "enemy type", high_type_routes, (int)(sizeof high_type_routes / sizeof *high_type_routes), 5, 0x940A, 0x2D);
    report_unrouted(g, out, "enemy anim state", anim_state_routes, (int)(sizeof anim_state_routes / sizeof *anim_state_routes), 5, 0x82E3, 0x26);
    report_unrouted(g, out, "enemy state", high_state_routes, (int)(sizeof high_state_routes / sizeof *high_state_routes), 5, 0x9477, 0x17);
}

void enemy_update_all(Game *g) {
    if (M(ChrBank0) == 2 && (M(FrameCounter) & 3) == 0) { /* $8001: the sewer palette rotates (B5 $BD24, X = $18) */
        uint8_t a = M(PaletteBuffer + 1 + 0x18), y = M(PaletteBuffer + 2 + 0x18);
        M(PaletteBuffer + 2 + 0x18) = a;
        a = M(PaletteBuffer + 3 + 0x18);
        M(PaletteBuffer + 3 + 0x18) = y;
        M(PaletteBuffer + 1 + 0x18) = a;
        M(PaletteDirty) = 1;
    }
    for (int x = 2; x < 16; x++) {
        if (!M(EntType + x)) {
            continue;
        }
        if (M(EntType + x) != 0x12 && !(M(VerticalLevelFlag) & 0x80)) { /* $F180: the enemy follows the scrolling */
            M(Tmp0) = M(EntX + x);
            M(EntX + x) = (uint8_t)(M(EntX + x) - M(CameraDeltaX));
            M(EntY + x) = (uint8_t)(M(EntY + x) - M(CameraDeltaY));
        }
        ai_leak_x = -1;
        run_entity_ai(g, x);
        if (ai_leak_x >= 0) {
            x = ai_leak_x; /* the X register changed in the boss AI */
        }
        update_hurt_flash(g, x);
        move_entity(g, x); /* B7 $EF22 */
        clamp_entity_position(g, x);
    }
}
