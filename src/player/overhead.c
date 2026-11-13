/* Top view: walking, doors, camera limits, the van. */
#include "game.h"
#include "anim.h"
#include "hal.h"
#include "persist.h"
#include "player.h"
#include "sound.h"
#include "stage.h"
#include "stage_play.h"
#include "player/overhead.h"
#include "player/pose.h"
#include "player/subweapon.h"
#include "player/tiles.h"
#define TODO_AREA "player" /* prefix of the TODO traces of this file */

/* --- top-view walking (B6 $A8D0-$AB2D, B7 $F081-$F21A) --- */
static void set_vel_x(Game *g, uint8_t frac, uint8_t whole) {
    M(EntVelXFrac) = frac;
    M(EntVelX) = whole;
} /* $F081 */
static void set_vel_y(Game *g, uint8_t frac, uint8_t whole) {
    M(EntVelYFrac) = frac;
    M(EntVelY) = whole;
} /* $F089 */

static void overhead_stop_y(Game *g) {
    M(ScrollDirY) = 0;
    M(CameraDeltaY) = 0;
    clear_vel_y(g, 0);
} /* $A8BE */
static void overhead_stop_x(Game *g) {
    M(ScrollDirX) = 0;
    M(CameraDeltaX) = 0;
    clear_vel_x(g, 0);
} /* $A8C7 */

/* B7 $E6A9: the player is on a door tile; looks up the entry in the door table (11 bytes:
 * column, x, y, side view, zone, X, Y, camera X hi/lo, camera Y hi/lo) and changes zone */
void door_transition_entry(Game *g) {
    door_transition(g);
} /* B7 $E6A9 from the scene */

void door_transition(Game *g) {
    clear_vel_x(g, 0);
    clear_vel_y(g, 0);
    camera_screen_split(g); /* $F1AC */
    uint16_t tab = rom_word(g->rom, B7, (uint16_t)(0xE86E + (M(SideViewFlag) ? M(LevelNumber) : 6) * 2));
    M(Tmp0) = (uint8_t)tab;
    M(Tmp0 + 1) = (uint8_t)(tab >> 8);
    M(Tmp0 + 4) = M(EntX);
    M(Tmp0 + 5) = M(EntY);
    spawn_position(g); /* $F0A8 : $AC/$AD, $7F */
    tab = rom_word(g->rom, B7, (uint16_t)(0xE86E + (M(SideViewFlag) ? M(LevelNumber) : 6) * 2));
    uint16_t p = rom_word(g->rom, B7, (uint16_t)(tab + M(AreaNumber) * 2));
    M(SpawnListPtrLo) = (uint8_t)p;
    M(SpawnListPtrHi) = (uint8_t)(p >> 8);
    M(SavedSpawnLimitX) = M(SpawnLimitX);
    M(SavedSpawnLimitY) = M(SpawnLimitY);
    M(Tmp8) = M(LevelNumber) == 5 ? 0x28 : 0x20;
    for (;; p = (uint16_t)(p + 11)) {   /* $E6E7 */
        M(SpawnListPtrLo) = (uint8_t)p; /* $E71A: the pointer advances with the loop (it ends on the $FF entry if nothing matches) */
        M(SpawnListPtrHi) = (uint8_t)(p >> 8);
        uint8_t c = rom_prg(g->rom, B7, p);
        if (c == 0xFF) {
            return;
        }
        if (c != M(SpawnColumn)) {
            continue;
        }
        int8_t dx = (int8_t)(rom_prg(g->rom, B7, (uint16_t)(p + 1)) - M(SpawnLimitX));
        if ((uint8_t)(dx < 0 ? -dx : dx) >= M(Tmp8)) {
            continue;
        }
        int8_t dy = (int8_t)(rom_prg(g->rom, B7, (uint16_t)(p + 2)) - M(SpawnLimitY));
        if ((uint8_t)(dy < 0 ? -dy : dy) >= M(Tmp8)) {
            continue;
        }
        break;
    }
    M(SpawnListPtrLo) = (uint8_t)p;
    M(SpawnListPtrHi) = (uint8_t)(p >> 8);
#define D(i) rom_prg(g->rom, B7, (uint16_t)(p + (i)))
    M(0x03EA) = 1; /* $E72A */
    if (D(3) && M(SideViewFlag)) {
        M(0x03E2) = 1;
    }
    M(SideViewFlag) = D(3);
    uint8_t area = D(4);
    if (area >= 5 && area < 9 && M(LevelNumber) == 4) {
        uint8_t a = M(0x55);
        if (a < 2) {
            M(0x55) = 4;
        } else if (a >= 5) {
            M(0x55) = 2;
        }
        if (M(AreaNumber) == M(0x55)) {
            area = 6;
        }
    }
    M(AreaNumber) = area;
    M(EntX) = D(5);
    M(EntY) = D(6);
    M(CameraXHi) = D(7);
    M(CameraXLo) = D(8);
    M(CameraYHi) = D(9);
    M(CameraYLo) = D(10);
#undef D
    if (!M(SideViewFlag)) { /* exit to the street */
        M(EntState) = 0;    /* $E7DA */
    } else {
        uint8_t t = tile_near_player(g, 0x16); /* $E7C4 */
        if (!((t == 9 || t == 0x0A) && probe_tile(g, 0x11, 6))) {
            M(EntState) = 0;
        }
    }
    M(ZoneChangeDelay) = 7;
    if (!M(0x03E2)) {
        sound_init(g);
    }
    M(EntFlags) &= 0xDB;  /* $D0E3 */
    player_attack_end(g); /* $D0CD */
    clear_object_slots(g);
    for (int x = 1; x < 16; x++) {
        clear_entity(g, x);
    }
    clear_vel_x(g, 0);
    clear_vel_y(g, 0);
    M(EntXFrac) = M(EntYFrac) = M(EntAttr) = M(EntSprite) = M(EntAnimId) = 0;
    M(EntRoutine) = M(EntSprite + 1) = 0;
    M(PickedUpMask) = M(0x58) = M(WaveTriggersUsed) = M(WaveActive) = M(WaterCurrent) = M(PushZone) = M(ColumnsDestroyedMask) = 0;
    M(WaveTriggersUsed) = persist_waves_over(g); /* Whole Pizza: the waves that are over stay over */
    M(ElapsedTimeLo) = M(ElapsedTimeHi) = 0;
    M(LevelScoreBcd) = M(0xC6) = M(0xC7) = 0;
    M(RopeSectionUsed) = M(0x03F8) = M(0x03F9) = M(0x03FA) = 0;
    M(RopesLaidMask) = M(0x03E7) = M(0x03EB) = M(CameraXLoSaved) = M(CameraYLoSaved) = 0;
    M(0x82) = 1;
    M(EntStatus) = 1;
    M(StageIntroTimer) = 0x10;
    M(StageState) = 3;
    if (!M(SideViewFlag) && M(AreaNumber) == 2) { /* $E79E: back in the street of level 3, the van is where it was left ($0593-$0595) */
        int v = 5;
        M(EntAnimId + v) = M(0x0593);
        M(EntX + v) = (uint8_t)(M(SpawnPosX) - M(CameraXLo));
        M(EntY + v) = (uint8_t)(M(SpawnPosY) - M(CameraYLo));
        M(EntType + v) = 0x10; /* $CFBF -> B4 $96F3 */
        M(EntAnimFrame + v) = 0;
        M(EntAnimTimer + v) = 8;
        M(EntHealth + v) = 0xFF;
        M(EntFlags + v) = 0xC8;
    }
}

/* B7 $F20E: does the tile block walking? (4 and $0C block; 6 only in the van; 2: not ported) */
static int tile_blocks(Game *g, uint8_t t) {
    if (t == 2) { /* $F229 */
        if (M(LevelNumber) == 1 && (M(CameraXLo) & 0x80)) {
            M(SceneId) = 0x0D;
            return 0;
        } /* $F22E: scene $0D; Z = 0: the tile does not block */
        if (M(InVanFlag)) {
            return 1; /* $F23B: in the van, the tile blocks without changing zone */
        }
        door_transition(g);
        return 1;
    }
    if (t == 6) {
        return M(InVanFlag) != 0;
    }
    return t == 4 || t == 0x0C;
}

/* B7 $F1F3: carry = the tile at (EntX + dx, EntY + dy) blocks */
static int probe_walk(Game *g, uint8_t dx, uint8_t dy) {
    uint8_t px = (uint8_t)(M(EntX) + dx), py = (uint8_t)(M(EntY) + dy);
    M(Tmp0) = px;
    M(Tmp0 + 1) = py;
    return tile_blocks(g, tile_at(g, px, py, dy));
}

/* B7 $F1DC / $F1FF: the van probes two points (width of the van); carry = one of the two blocks */
static int van_probe2(Game *g, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    return probe_walk(g, x1, y1) || probe_walk(g, x2, y2);
}

static int camera_down_free(Game *g) /* B6 $AAFA: carry = the camera cannot go down any more */
{
    unsigned s = M(CameraYLo) + 0xF2;
    uint8_t hi = (uint8_t)(M(CameraYHi) + (s > 0xFF));
    return hi >= rdmem(g, (uint16_t)(ptr(g, 0x49) + 1));
}

static int camera_up_at_top(Game *g) /* B6 $AB0D: flag N = the camera is at the top */
{
    unsigned s = M(CameraYLo) + (M(InVanFlag) ? 0xFD : 0xFE);
    return ((M(CameraYHi) + 0xFF + (s > 0xFF)) & 0x80) != 0;
}

static int camera_right_limit(Game *g) /* B6 $AAD9: carry = the camera cannot go right any more */
{
    unsigned s = M(CameraXLo) + 2;
    uint8_t hi = (uint8_t)(M(CameraXHi) + 1 + (s > 0xFF));
    return hi >= rdmem(g, ptr(g, 0x49));
}

static int camera_left_at_edge(Game *g) /* B6 $AAEC: flag N = the camera is on the left */
{
    unsigned s = M(CameraXLo) + 0xFE;
    return ((M(CameraXHi) + 0xFF + (s > 0xFF)) & 0x80) != 0;
}

static void overhead_walk(Game *g) /* B6 $A8D0 */
{
    if (M(JoyHeld1) & 0x03) { /* $A9D1: horizontal walk */
        overhead_stop_y(g);
        if (!(M(JoyHeld1) & 0x02)) { /* right */
            M(EntFlags) &= 0xFE;
            M(EntRoutine) = 2;
            if (M(InVanFlag) ? van_probe2(g, 0x10, 0xF7, 0x10, 9) : probe_walk(g, 7, 3)) {
                overhead_stop_x(g);
                return;
            }
            if (M(ZoneChangeDelay)) {
                return;
            }
            set_vel_x(g, 0, 2);
            if ((uint8_t)((M(EntX) + 2) & 0xFC) >= 0x80 && !camera_right_limit(g)) {
                M(ScrollDirX) = 2;
                return;
            }
            M(ScrollDirX) = 0;
            M(EntX) = (uint8_t)(M(EntX) + (M(InVanFlag) ? 3 : 2));
            return;
        }
        M(EntFlags) |= 1; /* left */
        M(EntRoutine) = 3;
        if (M(InVanFlag) ? van_probe2(g, 0xF0, 0xF7, 0xF0, 9) : probe_walk(g, 0xF9, 3)) {
            overhead_stop_x(g);
            return;
        }
        if (M(ZoneChangeDelay)) {
            return;
        }
        set_vel_x(g, 0, 0xFE);
        if ((uint8_t)((M(EntX) + 2) & 0xFC) < 0x80) {
            if (!camera_left_at_edge(g)) {
                M(ScrollDirX) = 1;
                return;
            }
            if (!M(SideViewFlag)) {
                M(CameraXLo) = 0;
                M(CameraXHi) = 0;
            }
        }
        M(ScrollDirX) = 0;
        M(EntX) = (uint8_t)(M(EntX) + (M(InVanFlag) ? 0xFD : 0xFE));
        return;
    }
    overhead_stop_x(g);       /* $A8D9: vertical walk */
    if (M(JoyHeld1) & 0x08) { /* up */
        M(EntRoutine) = 1;
        if (M(InVanFlag) ? van_probe2(g, 0xF7, 0xF0, 9, 0xF0) : probe_walk(g, 0, 0xFD)) {
            overhead_stop_y(g);
            return;
        }
        if (M(ZoneChangeDelay)) {
            return;
        }
        set_vel_y(g, 0, 0xFE);
        if ((uint8_t)((M(EntY) + 2) & 0xFC) < 0x78) {
            if (!camera_up_at_top(g)) {
                M(ScrollDirY) = 1;
                return;
            }
            if (!M(SideViewFlag)) {
                M(CameraYLo) = 0;
                M(CameraYHi) = 0;
            }
        }
        M(ScrollDirY) = 0;
        M(EntY) = (uint8_t)(M(EntY) + (M(InVanFlag) ? 0xFD : 0xFE));
        return;
    }
    M(EntRoutine) = 0; /* down */
    if (M(InVanFlag) ? van_probe2(g, 0xF7, 0x0E, 9, 0x0E) : probe_walk(g, 0, 6)) {
        overhead_stop_y(g);
        return;
    }
    if (M(ZoneChangeDelay)) {
        return;
    }
    set_vel_y(g, 0, 2);
    if ((uint8_t)((M(EntY) + 2) & 0xFC) >= 0x78 && !camera_down_free(g)) {
        M(ScrollDirY) = 2;
        return;
    }
    M(ScrollDirY) = 0;
    M(EntY) = (uint8_t)(M(EntY) + (M(InVanFlag) ? 3 : 2));
}

/* B6 $ABF9: health gauge (at the top of the screen, top view) and turtle icon (entity 15) */
void health_display_update(Game *g) {
    if (M(SideViewFlag)) {
        return;
    }
    if (M(AreaNumber) == 2) { /* $ABFE: counter $BF in entities 12 to 14 */
        M(EntType + 12) = M(EntType + 13) = M(EntType + 14) = M(EntType + 15) = 0x12;
        M(EntFlags + 12) = M(EntFlags + 13) = M(EntFlags + 14) = 0xC2;
        M(EntY + 12) = 0xB4;
        M(EntX + 12) = 0x10;
        M(EntSprite + 12) = 0x8D;
        M(EntY + 13) = M(EntY + 14) = 0xC5;
        M(EntX + 13) = 0x0C;
        M(EntX + 14) = 0x14;
        uint8_t a = M(SpecialShotCount), units = 0, tens = 0; /* B7 $C742 */
        for (units = a; units >= 10; units -= 10) {
            tens++;
        }
        if (!units) {
            units = 0x0A;
        }
        if (!tens) {
            tens = 0x0A;
        }
        M(EntSprite + 14) = rom_prg(g->rom, 6, (uint16_t)(0xABEE + units));
        M(EntSprite + 13) = rom_prg(g->rom, 6, (uint16_t)(0xABEE + tens));
    }
    uint8_t h = M(TurtleHealth0 + M(CurrentTurtle)); /* $AC53 */
    M(EntVar0 + 15) = h >> 4;
    if (h & 0x0F) {
        if (h & 0x08) {
            M(EntVar0 + 15)++;
        } else {
            M(EntVar0 + 15) |= 0x80;
        }
    }
    M(EntFlags + 15) = 0xC2; /* entity 15: turtle icon */
    M(EntAttr + 15) = 0x03;
    M(EntY + 15) = 0xD3;
    M(EntX + 15) = 0x68;
}

/* B6 $ABB3 / $AB7B / $AB97: a shot from the van (entity 2 to 4) created at the van's position; o = table (anim, type, X velocity, Y velocity) */
static void van_shot(Game *g, int x, uint16_t table, uint8_t dir) {
    clear_entity(g, x);
    M(EntX + x) = M(EntX);
    M(EntY + x) = M(EntY);
    M(EntFlags + x) = M(EntFlags);
    uint16_t o = (uint16_t)(table + dir * 4);
    M(EntAnimId + x) = rom_prg(g->rom, 6, o);
    M(EntType + x) = rom_prg(g->rom, 6, (uint16_t)(o + 1));
    M(EntVelX + x) = rom_prg(g->rom, 6, (uint16_t)(o + 2));
    M(EntVelY + x) = rom_prg(g->rom, 6, (uint16_t)(o + 3));
}

static void van_weapons(Game *g) /* B6 $AB30: A = small shot, B = special shot limited by $BF */
{
    if (!M(InVanFlag)) {
        return;
    }
    uint8_t dir = 0, a = M(EntAnimId) & 0xFE;
    if (a != 0xCC) {
        dir = 1;
        if (a != 0xCE) {
            dir = 2;
            if (M(EntFlags) & 1) {
                dir = 3;
            }
        }
    }
    M(Tmp17) = dir;
    if (M(JoyPressed1) & 0x80) {
        int x = free_projectile_slot(g);
        if (x < 0) {
            return;
        }
        van_shot(g, x, 0xABCE, dir);
        sound_play(g, 0x20);
    } else if ((M(JoyPressed1) & 0x40) && M(SpecialShotCount)) {
        int x = free_projectile_slot(g);
        if (x < 0) {
            return;
        }
        M(SpecialShotCount)--;
        van_shot(g, x, 0xABDE, dir);
        sound_play(g, 0x1E);
    }
}

/* B6 $A898: top-view player */
void overhead_player(Game *g) {
    M(CurPrgBank) = 6; /* Call_PlayerUpdate ($D04F) : SetPrgBankSaved(6) */
    g->prg_bank = 6;
    for (int i = 0; i < 4; i++) {
        M(SavedCameraXLo + i) = M(CameraXLo + i); /* $CBC4 */
    }
    M(SavedPlayerX) = M(EntX);
    M(SavedPlayerY) = M(EntY);
    if (!(M(EntFlags) & 0x80) || M(0x86)) {
        clear_vel_x(g, 0);
        clear_vel_y(g, 0);
    } else if (M(JoyHeld1) & 0x0F) {
        overhead_walk(g);
    } else {
        M(ScrollDirX) = 0; /* $A8C7 */
        M(CameraDeltaX) = 0;
        clear_vel_x(g, 0);
        M(ScrollDirY) = 0; /* $A8BE */
        M(CameraDeltaY) = 0;
        clear_vel_y(g, 0);
    }
    van_weapons(g); /* $AB30 */
    health_display_update(g);
}

/* B4 $97A3: the player is hit (EntFlags bit 2): blinking, end of the "hit" state */
int player_hurt_state(Game *g) {
    if (!M(TurtleHealth0 + M(CurrentTurtle))) { /* $97B5: no health left -> death animation */
        M(EntFlags) &= 0x7F;
        sound_play(g, 0x83);
        if (M(SideViewFlag)) {                                  /* $97C6 */
            set_animation(g, 0, M(SwimZoneFlag) ? 0xDA : 0x8A); /* jmp $D121 */
            return 1;
        }
        uint8_t a = (uint8_t)((M(EntRoutine) >> 2) + 0xAB + ((M(EntRoutine) >> 1) & 1)); /* $97CF */
        if (M(InVanFlag)) {
            a = 0xAE;
        }
        set_animation(g, 0, a); /* jmp $D121: leaves PlayerUpdate */
        return 1;
    }
    if (M(EntFlags) & 0x20) { /* $F199 */
        M(EntHurtTimer)--;
        M(EntAttr) = M(EntHurtTimer) & 3;
    }
    if (M(SwimZoneFlag) && M(0x03EC)) {
        load_palette_preset(g, (M(EntHurtTimer) & 1) ? 0x2C : 0x2B); /* $97E7 */
    }
    if (M(EntHurtTimer) == 0) { /* $97FC: end of the blinking */
        M(EntFlags) &= 0xDB;
        M(EntAttr) = 0;
        M(0x03EC) = 0;
        if (M(SwimZoneFlag)) {
            load_palette_preset(g, 0x2C); /* $980C */
        }
    }
    return 0;
}
