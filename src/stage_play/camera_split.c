/* Status bar split and camera/screen flags of the top view. */
#include "game.h"
#include "stage_play.h"
#include "stage_play/camera_split.h"
#define TODO_AREA "stage_play" /* prefix of the TODO traces of this file */

/* SetPrgBankSaved / RestorePrgBank: the bank remembered in CurPrgBank ($21) */
void set_prg_saved(Game *g, uint8_t b) {
    M(CurPrgBank) = b;
    g->prg_bank = b;
}

/* --- status bar: no visible effect in top view (B0 $8001) --- */
void status_bar_split(Game *g) {
    g->prg_bank = 0;
    if (!(M(NmiReentryFlag) & 0x80)) { /* B0 $8001 */
        M(0x64) = 0;
        M(0x65) = 0;
        if (M(SceneState) && !M(0x03E9)) {
            goto out;
        }
        if (M(SideViewFlag) && M(StageState) == 6) {
            Ppu *p = &g->ppu;
            uint16_t sv = p->v, st = p->t; /* the wait for sprite 0: the scroll that follows is noted */
            uint8_t sx = p->x, sw = p->w;
            M(NmiReentryFlag) = 0x80;
            ppu_read_status(g);
            ppu_write(g, 6, M(HudPpuAddrHi));
            ppu_write(g, 6, M(HudPpuAddrLo));
            ppu_read_status(g);
            ppu_write(g, 5, 0);
            ppu_write(g, 5, 0);
            p->split_on = 1;
            p->split_v = p->v;
            p->split_x = p->x;
            p->v = sv;
            p->t = st;
            p->x = sx;
            p->w = sw;
            if (M(VerticalLevelFlag) & 0x80) { /* B0 $8047: the status bar stays on name table 0 */
                M(PpuCtrlShadow) &= 0xFE;
                p->split_v &= (uint16_t)~0x0400; /* $2000 only changes the name table bit of the following lines: the status bar comes from table 0 */
            }
        }
    }
out:
    g->prg_bank = M(CurPrgBank); /* RestorePrgBank */
}

/* B7 $DED8 */
void update_scroll_edge_flag(Game *g) {
    M(ScrollEdgeFlag) = 0;
    if ((M(CameraXLo) | M(CameraXHi)) == 0) {
        M(ScrollEdgeFlag) = 2;
        return;
    }
    unsigned s = M(CameraXLo) + 2;
    uint8_t a = (uint8_t)(M(CameraXHi) + 1 + (s > 0xFF));
    if (a >= rdmem(g, ptr(g, 0x49))) {
        M(ScrollEdgeFlag)++;
    }
}

/* B7 $F1AC: $68/$69 = vertical position of the camera; in side view, $69 = screen number (height $C0) and $68 the remainder */
void camera_screen_split(Game *g) {
    M(CameraYInScreen) = M(CameraYLo);
    M(CameraYScreen) = M(CameraYHi);
    M(Tmp0) = 0xC0;
    if (!M(SideViewFlag)) {
        return;
    }
    uint8_t y = 0;
    while (M(CameraYScreen) != 0 || M(CameraYInScreen) >= 0xC0) {
        unsigned d = M(CameraYInScreen) - 0xC0;
        M(CameraYScreen) = (uint8_t)(M(CameraYScreen) - (M(CameraYInScreen) < 0xC0));
        M(CameraYInScreen) = (uint8_t)d;
        y++;
    }
    M(CameraYScreen) = y;
}
