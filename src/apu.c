/* NES APU: 2 pulse channels, triangle, noise (the DMC is not emulated). Rendered at 44100 Hz, one CPU cycle at a time.
 * apu_write() receives the $4000-$4017 writes of the sound driver; they are also traced (frame=N APU reg val). */
#include "game.h"

static const uint8_t len_table[32] = {10, 254, 20, 2,  40, 4,  80, 6,  160, 8,  60, 10, 14, 12, 26, 14,
                                      12, 16,  24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30};
static const uint8_t duty_seq[4][8] = {{0, 1, 0, 0, 0, 0, 0, 0}, {0, 1, 1, 0, 0, 0, 0, 0}, {0, 1, 1, 1, 1, 0, 0, 0}, {1, 0, 0, 1, 1, 1, 1, 1}};
static const uint16_t noise_period[16] = {4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068};

static void env_clock(Apu *a, int i) {
    if (a->env_start[i]) {
        a->env_start[i] = 0;
        a->env_decay[i] = 15;
        a->env_div[i] = a->env_period[i];
    } else if (a->env_div[i] == 0) {
        a->env_div[i] = a->env_period[i];
        if (a->env_decay[i]) {
            a->env_decay[i]--;
        } else if (a->len_halt[i]) {
            a->env_decay[i] = 15;
        }
    } else {
        a->env_div[i]--;
    }
}

static int sweep_target(Apu *a, int i) {
    int d = a->pulse_timer[i] >> a->sweep_shift[i];
    if (a->sweep_neg[i]) {
        d = -d - (i == 0 ? 1 : 0);
    }
    return a->pulse_timer[i] + d;
}

static void quarter_frame(Apu *a) {
    env_clock(a, 0);
    env_clock(a, 1);
    env_clock(a, 3);
    if (a->tri_reload) {
        a->tri_lin = a->tri_reload_val;
    } else if (a->tri_lin) {
        a->tri_lin--;
    }
    if (!a->tri_ctrl) {
        a->tri_reload = 0;
    }
}

static void half_frame(Apu *a) {
    for (int i = 0; i < 4; i++) { /* length counters: 0, 1 = pulse, 2 = triangle, 3 = noise */
        int halt = i == 2 ? a->tri_ctrl : a->len_halt[i];
        if (!halt && a->len[i]) {
            a->len[i]--;
        }
    }
    for (int i = 0; i < 2; i++) {
        int target = sweep_target(a, i);
        if (a->sweep_div[i] == 0 && a->sweep_on[i] && a->sweep_shift[i] && a->pulse_timer[i] >= 8 && target <= 0x7FF) {
            a->pulse_timer[i] = (uint16_t)target;
        }
        if (a->sweep_div[i] == 0 || a->sweep_reload[i]) {
            a->sweep_div[i] = a->sweep_period[i];
            a->sweep_reload[i] = 0;
        } else {
            a->sweep_div[i]--;
        }
    }
}

#ifdef SNDDIFF /* log of the writes of each driver (comparison, see sound.c) */
Game *apu_dbg_g[2];
int apu_dbg_n[2];
uint16_t apu_dbg_log[2][4096];
#endif

void apu_write(Game *g, int r, uint8_t v) {
    Apu *a = &g->apu;
#ifdef SNDDIFF
    for (int k = 0; k < 2; k++) {
        if (g == apu_dbg_g[k] && apu_dbg_n[k] < 4096) {
            apu_dbg_log[k][apu_dbg_n[k]++] = (uint16_t)(r << 8 | v);
        }
    }
#endif
    TRACE("frame=%u APU %02X %02X\n", g->frame, r, v);
    switch (r) {
        case 0:
        case 4: {
            int i = r >> 2;
            a->duty[i] = v >> 6;
            a->len_halt[i] = (v >> 5) & 1;
            a->const_vol[i] = (v >> 4) & 1;
            a->env_period[i] = v & 15;
            break;
        }
        case 1:
        case 5: {
            int i = r >> 2;
            a->sweep_on[i] = v >> 7;
            a->sweep_period[i] = (v >> 4) & 7;
            a->sweep_neg[i] = (v >> 3) & 1;
            a->sweep_shift[i] = v & 7;
            a->sweep_reload[i] = 1;
            break;
        }
        case 2:
        case 6: {
            int i = r >> 2;
            a->pulse_timer[i] = (uint16_t)((a->pulse_timer[i] & 0x700) | v);
            break;
        }
        case 3:
        case 7: {
            int i = r >> 2;
            a->pulse_timer[i] = (uint16_t)((a->pulse_timer[i] & 0xFF) | (v & 7) << 8);
            if (a->enabled & (1 << i)) {
                a->len[i] = len_table[v >> 3];
            }
            a->pulse_phase[i] = 0;
            a->env_start[i] = 1;
            break;
        }
        case 8:
            a->tri_ctrl = v >> 7;
            a->tri_reload_val = v & 0x7F;
            break;
        case 10:
            a->tri_timer = (uint16_t)((a->tri_timer & 0x700) | v);
            break;
        case 11:
            a->tri_timer = (uint16_t)((a->tri_timer & 0xFF) | (v & 7) << 8);
            if (a->enabled & 4) {
                a->len[2] = len_table[v >> 3];
            }
            a->tri_reload = 1;
            break;
        case 12:
            a->len_halt[3] = (v >> 5) & 1;
            a->const_vol[3] = (v >> 4) & 1;
            a->env_period[3] = v & 15;
            break;
        case 14:
            a->noise_mode = v >> 7;
            a->noise_per = noise_period[v & 15];
            break;
        case 15:
            if (a->enabled & 8) {
                a->len[3] = len_table[v >> 3];
            }
            a->env_start[3] = 1;
            break;
        case 16:
        case 17:
        case 18:
        case 19:
            break; /* DMC: not emulated */
        case 21:
            a->enabled = v & 15;
            for (int i = 0; i < 4; i++) {
                if (!(a->enabled & (1 << i))) {
                    a->len[i] = 0;
                }
            }
            break;
        case 23:
            a->five_step = v >> 7;
            a->fs_cycle = 0;
            if (a->five_step) {
                quarter_frame(a);
                half_frame(a);
            }
            break;
    }
}

uint8_t apu_read_status(Game *g) {
    Apu *a = &g->apu;
    uint8_t s = 0;
    for (int i = 0; i < 4; i++) {
        if (a->len[i]) {
            s |= (uint8_t)(1 << i);
        }
    }
    return s;
}

static void apu_cycle(Apu *a) {
    /* frame sequencer: 4 or 5 steps */
    a->fs_cycle++;
    int c = a->fs_cycle;
    if (c == 7457 || c == 22371) {
        quarter_frame(a);
    } else if (c == 14913) {
        quarter_frame(a);
        half_frame(a);
    } else if (!a->five_step && c == 29829) {
        quarter_frame(a);
        half_frame(a);
        a->fs_cycle = 0;
    } else if (a->five_step && c == 37281) {
        quarter_frame(a);
        half_frame(a);
        a->fs_cycle = 0;
    }

    if (a->cycle_odd ^= 1) { /* pulse channels: one step every 2 CPU cycles */
        for (int i = 0; i < 2; i++) {
            if (a->pulse_cnt[i] == 0) {
                a->pulse_cnt[i] = a->pulse_timer[i];
                a->pulse_phase[i] = (uint8_t)((a->pulse_phase[i] + 1) & 7);
            } else {
                a->pulse_cnt[i]--;
            }
        }
    }
    if (a->tri_cnt == 0) {
        a->tri_cnt = a->tri_timer;
        if (a->len[2] && a->tri_lin && a->tri_timer >= 2) {
            a->tri_phase = (uint8_t)((a->tri_phase + 1) & 31);
        }
    } else {
        a->tri_cnt--;
    }
    if (a->noise_cnt == 0) {
        a->noise_cnt = a->noise_per ? a->noise_per : 4;
        int fb = (a->lfsr & 1) ^ ((a->lfsr >> (a->noise_mode ? 6 : 1)) & 1);
        a->lfsr = (uint16_t)((a->lfsr >> 1) | (fb << 14));
    } else {
        a->noise_cnt--;
    }
}

static int pulse_out(Apu *a, int i) {
    if (!a->len[i] || a->pulse_timer[i] < 8 || sweep_target(a, i) > 0x7FF) {
        return 0;
    }
    if (!duty_seq[a->duty[i]][a->pulse_phase[i]]) {
        return 0;
    }
    return a->const_vol[i] ? a->env_period[i] : a->env_decay[i];
}

/* produces n 16-bit mono samples at 44100 Hz */
void apu_mix(Game *g, int16_t *out, int n) {
    Apu *a = &g->apu;
    if (!a->lfsr) {
        a->lfsr = 1;
    }
    for (int s = 0; s < n; s++) {
        a->cyc_acc += 1789773.0 / 44100.0;
        while (a->cyc_acc >= 1.0) {
            a->cyc_acc -= 1.0;
            apu_cycle(a);
        }
        int p = pulse_out(a, 0) + pulse_out(a, 1);
        int tri = (a->len[2] && a->tri_lin) ? (a->tri_phase < 16 ? 15 - a->tri_phase : a->tri_phase - 16) : 0;
        int noi = 0;
        if (a->len[3] && !(a->lfsr & 1)) {
            noi = a->const_vol[3] ? a->env_period[3] : a->env_decay[3];
        }
        double po = p ? 95.88 / (8128.0 / p + 100.0) : 0.0;
        double tnd_in = tri / 8227.0 + noi / 12241.0;
        double to = tnd_in > 0 ? 159.79 / (1.0 / tnd_in + 100.0) : 0.0;
        double v = po + to;
        a->hp += (v - a->hp) * 0.0028; /* high-pass ~20 Hz: removes the DC component */
        double o = (v - a->hp) * 60000.0;
        out[s] = (int16_t)(o > 32767 ? 32767 : o < -32768 ? -32768 : o);
    }
}
