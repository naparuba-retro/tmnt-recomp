/* Game (GameMode 3) and level (GameMode 4): B7 $C686 (new game), $CB24 (GameMode4), StageState states.
 * State of the port: GameMode 3 and StageState 0-2 (init, wait, start position). The following states stop
 * with a "TODO stage state" message. */
#include "game.h"
#include "enemy.h"
#include "hal.h"
#include "pause.h"
#include "player.h"
#include "sound.h"
#include "stage.h"
#include "stage_map.h"
#include "stage_play.h"

/* 2-byte pointer read from a ROM table -> two zero-page bytes; in the US ROM the value is converted back to an EU address (rom_word) */
#define STORE_PTR(zp, bank, addr)                                                                                                                              \
    do {                                                                                                                                                       \
        uint16_t w_ = rom_word(g->rom, (bank), (uint16_t)(addr));                                                                                              \
        M(zp) = (uint8_t)w_;                                                                                                                                   \
        M((zp) + 1) = (uint8_t)(w_ >> 8);                                                                                                                      \
    } while (0)

static void clear_all_entities(Game *g) /* B7 $EF5C */
{
    for (int x = 0; x < 16; x++) {
        clear_entity(g, x);
    }
}

void mode3_new_game(Game *g) /* B7 $C686 GameMode3 */
{
    M(StageState) = 0;
    clear_game_ram48(g);
    M(SideViewFlag)++;
    M(ScoreBcd) = M(0xC3) = M(0xC4) = 0;
    M(Lives) = 3;
    M(TurtleHealth0) = M(TurtleHealth1) = M(TurtleHealth2) = M(TurtleHealth3) = 0x80;
    M(SpecialShotCount) = 0;
    M(RopeCount) = 0;
    M(SubWeaponAmmo0) = M(SubWeaponAmmo1) = M(SubWeaponAmmo2) = M(SubWeaponAmmo3) = 0;
    M(GameMode)++; /* $C6B9 */
    M(GameSubMode) = 0;
}

static void stage_init(Game *g) /* $CB4A Stage_Init */
{
    uint8_t v[4]; /* $CB6D */
    for (int i = 0; i < 4; i++) {
        v[i] = rom_prg(g->rom, 7, (uint16_t)(0xCB6D + i));
    }
    M(0x03E7) = 0;
    M(0x55) = v[M(RandomSeed) & 3];
    M(StageState)++;
}

static void stage_start_position(Game *g) /* $D2ED StageState2 */
{
    M(0x03D6) = 0;
    M(StageState)++;
    clear_all_entities(g);
    player_spawn(g);       /* $D0EE */
    if (M(SideViewFlag)) { /* $D31F */
        if (M(LevelNumber) == 5) {
            M(AreaNumber) = 0;
        }
        uint16_t p = rom_word(g->rom, B7, (uint16_t)(0xD6AA + M(LevelNumber) * 2));
        uint8_t y = (uint8_t)(M(AreaNumber) << 2);
        M(CameraXLo) = R7(p + y);
        M(CameraXHi) = R7(p + y + 1);
        M(CameraYLo) = R7(p + y + 2);
        M(CameraYHi) = R7(p + y + 3);
        if (M(LevelNumber) != 5) { /* the level starts on the top-view map */
            M(AreaNumber) = M(LevelNumber);
            M(SideViewFlag) = 0;
        }
    } else {
        uint8_t y = (uint8_t)(M(AreaNumber) << 2);
        M(CameraXLo) = R7(0xD696 + y);
        M(CameraXHi) = R7(0xD697 + y);
        M(CameraYLo) = R7(0xD698 + y);
        M(CameraYHi) = R7(0xD699 + y);
    }
    if (M(LevelNumber) == 2) {  /* $D35B */
        camera_screen_split(g); /* $F1AC */
        level3_van_spawn(g);    /* $CFB4 */
    }
}

/* B7 $DF19: vertical position ($0A/$0B) -> remainder in the screen ($14) and screen number ($15); one screen = $F0 lines */
static void split_screen_y(Game *g) {
    unsigned y = M(TmpA) | M(TmpB) << 8, n = 0;
    while (y >= 0xF0) {
        y -= 0xF0;
        n++;
    }
    M(Tmp14) = (uint8_t)y;
    M(Tmp15) = (uint8_t)n;
}

void set_scroll_from_camera(Game *g) /* B7 $DEFD */
{
    M(ScrollXShadow) = M(CameraXLo);
    M(TmpA) = M(CameraYLo);
    M(TmpB) = M(CameraYHi);
    split_screen_y(g);
    M(ScrollYShadow) = M(Tmp14);
    M(NametableBit) = (uint8_t)((M(Tmp15) & 1) << 1);
}

void clear_object_slots(Game *g) /* B7 $EFDB */
{
    static const uint16_t arrays[] = {0x0650, 0x0658, 0x0660, 0x0668, 0x0670, 0x0618, 0x0620, 0x0688, 0x0690, 0x0698, 0x06A0, 0x06A8, 0x06B0, 0x06B8};
    for (int x = 0; x < 8; x++) {
        for (unsigned i = 0; i < sizeof arrays / sizeof *arrays; i++) {
            M(arrays[i] + x) = 0;
        }
    }
}

/* B7 $D442: preparation of a top-view level (zone data in bank 1) */
static void prepare_overhead_area(Game *g) {
    M(VerticalScrollMode) = 0;
    M(ScreenChangeDir) = 0;
    M(MapRowsToDraw) = 0x1E; /* number of rows to draw */
    set_chr_bank0(g, 0x10);
    set_chr_bank1(g, R7(0xD5E5 + M(AreaNumber)));
    uint8_t y = (uint8_t)(M(AreaNumber) << 1);
    STORE_PTR(0x4B, 1, 0x8001 + y);
    STORE_PTR(0x4D, 1, 0x800B + y);
    STORE_PTR(0x60, 1, 0x8051 + y);
    STORE_PTR(0x49, 7, 0xF4C9 + y);
    uint8_t i = (uint8_t)((M(ChrBank1) - 0x0D) << 1); /* $D553 */
    STORE_PTR(0x62, 7, 0xFCB2 + i);
    set_scroll_from_camera(g);
    M(MapRowIndex) = 0xFE;
    M(0x65) = 1;
    load_level_palette(g, R7(0xD653 + M(AreaNumber)));
    set_turtle_sprite_palette(g);
}

/* B7 $D591: display flags $29/$2A according to the camera height */
static void camera_height_flags(Game *g) {
    uint8_t h = (uint8_t)((M(CameraYLo) | M(CameraYHi) << 8) >> 4), y = 0;
    while (y < 0x10 && R7(0xD5C4 + y) != h) {
        y++;
    }
    M(HudPpuAddrHi) = R7(0xD5D4 + y);
    M(HudPpuAddrLo) = 0;
    if (M(HudPpuAddrHi) == 0x21) {
        M(HudPpuAddrLo) = 0x80;
    }
}

/* B7 $F77D: sprite/background palette according to $0604 */
void wave_palette(Game *g) {
    uint8_t p = R7(0xF78B + M(WaveSpriteSet));
    if (p & 0x80) {
        return;
    }
    load_palette_preset(g, p);
    set_turtle_sprite_palette(g);
}

/* B7 $D384: preparation of a side-view zone (zone data in bank 1) */
static void prepare_sideview_area(Game *g) {
    M(MapRowsToDraw) = 0x17;
    uint8_t lvl = M(LevelNumber), a = lvl;
    if (lvl == 4) {
        a = (uint8_t)(lvl + 2 + M(AreaNumber)); /* $D374 */
    } else if (lvl == 3) {
        a = !M(AreaNumber) ? 0 : (M(AreaNumber) == 0x11) ? 0x0C : lvl; /* $D392: zone 0: block set 0 (A stays equal to the zone); $D37C */
    }
    uint8_t y = (uint8_t)(a << 1);
    STORE_PTR(0x4B, 1, 0x8015 + y);
    STORE_PTR(0x4D, 1, 0x8033 + y);
    y = (uint8_t)(lvl << 1);
    STORE_PTR(0x60, 1, 0x805D + y);
    uint16_t p = rom_word(g->rom, B7, (uint16_t)(0xF4BD + y));
    STORE_PTR(0x49, 7, p + M(AreaNumber) * 2);
    p = rom_word(g->rom, B7, (uint16_t)(0xD658 + y));
    load_level_palette(g, R7(p + M(AreaNumber)));
    set_turtle_sprite_palette(g);
    if (lvl == 5 && M(AreaNumber) == 3 && !M(0x0609)) {
        load_palette_preset(g, 0x20);
    }
    camera_height_flags(g);
    if (!M(0x0609)) { /* $D56E */
        uint16_t t = rom_word(g->rom, B7, (uint16_t)(0xD5EB + y));
        set_chr_bank1(g, R7(t + M(AreaNumber) * 2));
        M(WaveSpriteSet) = R7(t + M(AreaNumber) * 2 + 1);
        set_chr_bank0(g, M(WaveSpriteSet));
        wave_palette(g);
    }
    if (M(0x58)) {
        load_palette_preset(g, (uint8_t)(M(RescuedTurtle) + 0x45));
    } else {
        wave_palette(g);
        if (M(WaveState) == 3 && M(WaveTriggerId) == 3) {
            load_palette_preset(g, 0x11);
        }
    }
    uint8_t i = (uint8_t)((M(ChrBank1) - 0x0D) << 1); /* $D553 */
    STORE_PTR(0x62, 7, 0xFCB2 + i);
    set_scroll_from_camera(g);
    M(MapRowIndex) = 0xFF;
    M(0x65) = 1;
}

static void stage_load_level(Game *g) /* $CB79 Stage_LoadLevel */
{
    if (!M(SideViewFlag)) {
        for (int i = 0; i < 8; i++) {
            M(SceneState + i) = 0; /* $C6E4 */
        }
        for (int i = 0; i < 4; i++) {
            M(SavedCameraXLo + i) = M(CameraXLo + i); /* $CBC4 */
        }
        M(SavedPlayerX) = M(EntX);
        M(SavedPlayerY) = M(EntY);
    }
    M(VerticalLevelFlag) = (M(LevelNumber) == 4 && M(AreaNumber) == 6) ? 0x80 : 0x00;
    M(0x3C) = 1;
    M(0x36) = 1;
    load_screen_and_clear(g);
    if (M(SideViewFlag)) {
        prepare_sideview_area(g);
    } else {
        prepare_overhead_area(g);
    }
    if (!M(0x0609)) {
        clear_object_slots(g);
        if (M(SideViewFlag) && M(AreaNumber) == 1 && M(LevelNumber) == 1) {
            M(BombTimerSeconds) = 0x8C;
            M(BombTimerTicks) = 0x1E;
        } /* $CBA3 */
    }
    M(StageState)++;
}

/* B7 $CBF9: restarts the level music (end of a wave fight) */
void restart_level_music(Game *g) {
    ROM_TABLE(overhead_music, 6, 7, 0xCC29);
    ROM_TABLE(side_music, 6, 7, 0xCC2F);
    if (M(0x03E2)) {
        M(0x03E2) = 0;
        return;
    }
    sound_init(g);
    if (M(SideViewFlag)) {
        sound_play(g, (M(LevelNumber) == 1 && M(AreaNumber) == 0) ? 0x6B : side_music[M(LevelNumber) % 6]);
    } else {
        sound_play(g, overhead_music[M(LevelNumber) % 6]);
    }
}

static void stage_start_music(Game *g) /* $CBDC Stage_StartMusic */
{
    ROM_TABLE(overhead_music, 6, 7, 0xCC29); /* $CC29 */
    ROM_TABLE(side_music, 6, 7, 0xCC2F);     /* $CC2F */
    if (M(SideViewFlag) && !M(0x0609)) {
        spawn_initial_side(g);
    }
    M(StageState)++;
    M(StageIntroTimer) = 0x10;
    if (M(0x0609)) {
        M(0x0609) = 0;
        return;
    }
    if (M(0x03E2)) {
        M(0x03E2) = 0;
        return;
    }
    sound_init(g);
    if (M(SideViewFlag)) {
        sound_play(g, (M(LevelNumber) == 1 && M(AreaNumber) == 0) ? 0x6B : side_music[M(LevelNumber) % 6]);
        return;
    }
    sound_play(g, overhead_music[M(LevelNumber) % 6]);
}

static void stage_to_title(Game *g) /* B7 $CD84: back to the title */
{
    M(StageState) = 0;
    M(0x2E) = 0;
    M(GameMode) = 0;
    M(GameSubMode) = 0;
}

static void stage_player_died(Game *g) /* B7 $CCE3 Stage_PlayerDied (state 7) */
{
    M(EntFlags) = 0xC2;
    uint8_t t = M(CurrentTurtle);
    M(SubWeaponAmmo0 + t) = 0;
    M(SubWeaponType0 + t) = 0;
    M(EntStatus) = 0;
    M(0x89) = M(WaveActive) = M(WaveState) = M(WaterCurrent) = M(PushZone) = M(0x36) = M(ColumnsDestroyedMask) = 0;
    M(PauseAfterDeath) = 1;
    for (int x = 5; x < 16; x++) {
        clear_entity(g, x); /* ClearEntitiesFrom5 */
    }
    M(PauseState) = 0; /* $CE0E: enters the pause screen (turtle choice) */
    M(EntSprite) = 0;
    M(PauseFlag) = 1;
    M(PpuCtrlSaved) = M(PpuCtrlShadow);
    M(EntTimer) = M(EntSprite);
}

static void stage_game_over(Game *g) /* B7 $CD2E Stage_GameOver (state 8) */
{
    M(0x36) = 1;
    M(0x03D9) = 1;
    if (++M(ElapsedTimeLo) != 0) {
        return;
    }
    if (++M(ElapsedTimeHi) != 0) {
        if (M(ElapsedTimeHi) == 0xFF) {
            sound_play(g, 0x8B); /* game over jingle */
        }
        return;
    }
    if (--M(Lives) == 0) {
        stage_to_title(g);
        return;
    } /* no life left: back to the title */
    load_screen_and_clear(g); /* $CD48: "continue?" screen */
    print_text(g, 6);
    print_text(g, 7);
    M(EntSprite) = 0x49;
    M(EntY) = 0x60;
    M(EntX) = 0x60;
    M(EntFlags) = 0x02;
    M(MenuSelection) = 0;
    M(StageState) = 0x0A;
}

static void stage_continue_menu(Game *g) /* B7 $CD72 (state 10) */
{
    ROM_TABLE(cursor_y, 2, 7, 0xCDCF); /* $CDCF */
    M(0x03D9) = 1;
    if (M(JoyPressed1) & 0x10) {
        if ((M(MenuSelection) >> 1) & 1) {
            stage_to_title(g);
            return;
        }
        M(0x2E) = M(LevelNumber); /* $CD8D: the level restarts with 4 turtles */
        clear_game_ram48(g);
        M(LevelNumber) = M(0x2E);
        M(AreaNumber) = M(0x2E);
        if (M(LevelNumber) == 5) {
            M(SideViewFlag) = 1;
            M(AreaNumber) = 0;
        }
        M(TurtleHealth0) = M(TurtleHealth1) = M(TurtleHealth2) = M(TurtleHealth3) = 0x80;
        M(StageState) = 2;
        M(0x2E) = 0;
        M(0x36)++;
        return;
    }
    if (M(JoyPressed1) & 0x20) {
        M(MenuSelection)++;
    }
    M(EntY) = cursor_y[(M(MenuSelection) >> 1) & 1];
}

static void stage_game_over_screen(Game *g) // B7 $CD1A (state 11)
{
    load_screen_and_clear(g);
    load_screen(g, 7, 0x0C);
    M(StageState) = 8;
}

void stage_run(Game *g) /* B7 $CB24 GameMode4 */
{
    stage_check_pause(g); /* Start toggles the pause */
    if (!M(0x03EA) && M(StageState) >= 6 && M(PauseFlag)) {
        pause_screen(g);
        return;
    } /* jmp Call_Entry_B3_A91C */
    M(0x03EA) = 0;
    switch (M(StageState)) {
        case 0:
            stage_init(g);
            break;
        case 1:
            M(StageState)++;
            break; /* Stage_Wait */
        case 2:
            stage_start_position(g);
            break;
        case 3:
            stage_load_level(g);
            break;
        case 4:
            stage_draw_map(g);
            break;
        case 5:
            stage_start_music(g);
            break;
        case 6:
            stage_play(g);
            break;
        case 7:
            stage_player_died(g);
            break;
        case 8:
            stage_game_over(g);
            break;
        case 9:
            M(StageState) = 2;
            break; /* Stage_Restart */
        case 10:
            stage_continue_menu(g);
            break;
        case 11:
            stage_game_over_screen(g);
            break;
        default:
            TRACE("f=%u TODO stage state %d\n", g->frame, M(StageState));
            g->left_attract = 1; /* out of scope for now */
            break;
    }
}
