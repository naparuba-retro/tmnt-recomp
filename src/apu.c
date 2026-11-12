/* NES APU: 2 pulse channels, triangle, noise (the DMC is not emulated). Rendered at 44100 Hz, one CPU cycle at a time.
 * apu_write() receives the $4000-$4017 writes of the sound driver; they are also traced (frame=N APU reg val). */
#include <math.h>
#include <string.h>
#include "game.h"

/* The level of the output: the same as BizHawk's about twice over (its loudest sounds peak around 9 000 of 32 767). */
#define OUTPUT_GAIN 30000.0

static const uint8_t len_table[32] = {10, 254, 20, 2,  40, 4,  80, 6,  160, 8,  60, 10, 14, 12, 26, 14,
                                      12, 16,  24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30};
static const uint8_t duty_seq[4][8] = {{0, 1, 0, 0, 0, 0, 0, 0}, {0, 1, 1, 0, 0, 0, 0, 0}, {0, 1, 1, 1, 1, 0, 0, 0}, {1, 0, 0, 1, 1, 1, 1, 1}};
static const uint16_t noise_period[16] = {4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068};

/* The DMC output rates, in CPU cycles (NTSC). */
static const uint16_t dmc_period[16] = {428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54};

/* The reader: fills the sample buffer from the ROM (the fixed bank, $C000-$FFFF) when it is empty and there are bytes left. The cycles that the real DMC steals from the
 * CPU are not modelled. */
static void dmc_fill(Apu *a) {
    if (a->dmc_buf_full || !a->dmc_len || !a->dmc_rom) {
        return;
    }
    a->dmc_buf = rom_prg_native(a->dmc_rom, 7, a->dmc_addr);
    a->dmc_buf_full = 1;
    a->dmc_addr = a->dmc_addr == 0xFFFF ? 0x8000 : (uint16_t)(a->dmc_addr + 1);
    if (--a->dmc_len == 0 && a->dmc_loop) {
        a->dmc_addr = a->dmc_addr_start;
        a->dmc_len = a->dmc_len_start;
    }
}

static void dmc_restart(Apu *a) {
    a->dmc_addr = a->dmc_addr_start;
    a->dmc_len = a->dmc_len_start;
    dmc_fill(a);
}

/* One step of the output unit (every `dmc_period` cycles): the level moves by 2 towards the next bit of the sample. */
static void dmc_clock(Apu *a) {
    if (!a->dmc_silent) {
        if (a->dmc_shift & 1) {
            if (a->dmc_level <= 125) {
                a->dmc_level = (uint8_t)(a->dmc_level + 2);
            }
        } else if (a->dmc_level >= 2) {
            a->dmc_level = (uint8_t)(a->dmc_level - 2);
        }
    }
    a->dmc_shift >>= 1;
    if (a->dmc_bits <= 1) {
        a->dmc_bits = 8;
        if (a->dmc_buf_full) {
            a->dmc_shift = a->dmc_buf;
            a->dmc_buf_full = 0;
            a->dmc_silent = 0;
            dmc_fill(a);
        } else {
            a->dmc_silent = 1;
        }
    } else {
        a->dmc_bits--;
    }
}

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
            a->dmc_loop = (v >> 6) & 1;
            a->dmc_rate = v & 15;
            break;
        case 17:
            a->dmc_level = v & 0x7F;
            break;
        case 18:
            a->dmc_addr_start = (uint16_t)(0xC000 + v * 64);
            break;
        case 19:
            a->dmc_len_start = (uint16_t)(v * 16 + 1);
            break;
        case 21:
            if (v & 16) {
                if (!a->dmc_len) {
                    dmc_restart(a);
                }
            } else {
                a->dmc_len = 0;
            }
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
    if (a->dmc_len) {
        s |= 0x10;
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
    if (a->dmc_cnt == 0) {
        a->dmc_cnt = (uint16_t)(dmc_period[a->dmc_rate & 15] - 1);
        if (a->dmc_bits == 0) {
            a->dmc_bits = 8;
            a->dmc_silent = 1;
        }
        dmc_clock(a);
    } else {
        a->dmc_cnt--;
    }
    if (a->noise_cnt == 0) {
        a->noise_cnt = a->noise_per ? a->noise_per : 4;
        int fb = (a->lfsr & 1) ^ ((a->lfsr >> (a->noise_mode ? 6 : 1)) & 1);
        a->lfsr = (uint16_t)((a->lfsr >> 1) | (fb << 14));
    } else {
        a->noise_cnt--;
    }
}

/* Runs `cycles` CPU cycles, same result as calling apu_cycle() that many times. Most cycles only count the timers down: those are done in one go,
 * up to the next cycle where something happens (a frame sequencer step or a timer reaching 0), which apu_cycle() then runs. */
static void apu_run(Apu *a, int cycles) {
    while (cycles > 0) {
        int skip = cycles;
        int next = a->five_step ? 37281 : 29829; /* the frame sequencer steps at these counts only (equality) */
        static const int steps[] = {7457, 14913, 22371};
        for (int i = 0; i < 3; i++) {
            if (steps[i] > a->fs_cycle && steps[i] < next) {
                next = steps[i];
            }
        }
        if (next > a->fs_cycle && next - a->fs_cycle - 1 < skip) {
            skip = next - a->fs_cycle - 1;
        }
        if (a->tri_cnt < skip) {
            skip = a->tri_cnt;
        }
        if (a->noise_cnt < skip) {
            skip = a->noise_cnt;
        }
        if (a->dmc_cnt < skip) {
            skip = a->dmc_cnt;
        }
        int pulse_min = a->pulse_cnt[0] < a->pulse_cnt[1] ? a->pulse_cnt[0] : a->pulse_cnt[1];
        /* a pulse timer counts down every other cycle, starting with the first one when cycle_odd is 0 */
        if ((a->cycle_odd ? skip / 2 : (skip + 1) / 2) > pulse_min) {
            skip = a->cycle_odd ? 2 * pulse_min + 1 : 2 * pulse_min;
        }
        if (skip > 0) {
            int pulse_steps = a->cycle_odd ? skip / 2 : (skip + 1) / 2;
            a->fs_cycle += skip;
            a->tri_cnt = (uint16_t)(a->tri_cnt - skip);
            a->noise_cnt = (uint16_t)(a->noise_cnt - skip);
            a->dmc_cnt = (uint16_t)(a->dmc_cnt - skip);
            a->pulse_cnt[0] = (uint16_t)(a->pulse_cnt[0] - pulse_steps);
            a->pulse_cnt[1] = (uint16_t)(a->pulse_cnt[1] - pulse_steps);
            a->cycle_odd ^= (uint8_t)(skip & 1);
            cycles -= skip;
            if (!cycles) {
                break;
            }
        }
        apu_cycle(a);
        cycles--;
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
/* The mixed level of the channels right now (the console's non-linear DAC), before the high-pass filter. */
static double mixed_level(Apu *a) {
    int p = pulse_out(a, 0) + pulse_out(a, 1);
    /* the triangle keeps its last level when its counters stop it (the console's DAC holds the sequencer step): falling to 0 would be a click at the end of every note */
    int tri = a->tri_phase < 16 ? 15 - a->tri_phase : a->tri_phase - 16;
    int noi = 0;
    if (a->len[3] && !(a->lfsr & 1)) {
        noi = a->const_vol[3] ? a->env_period[3] : a->env_decay[3];
    }
    double po = 0.00752 * p; /* the linear approximation of the console's mixer (the one that BizHawk uses), not the exact non-linear formula */
    double to = 0.00851 * tri + 0.00494 * noi + 0.00335 * a->dmc_level;
    return po + to;
}

/* Band-limited synthesis, in the way of blip_buf: every change of the level (a "step", at an exact CPU cycle) is added to the output as the response of an ideal low-pass
 * filter (a windowed sinc), at the right fraction of a sample. Looking at the level once per output sample (what this did at first) puts every edge of a square wave on
 * the nearest sample: the frequencies above half the sample rate fold back as a rough, crackling noise; the emulators that sound right (BizHawk...) do this instead. */
#define BLIP_PHASES 32
#define BLIP_CUTOFF 0.42 /* of the sample rate: about 18.5 kHz at 44.1 kHz */
#define BLIP_MAX_BLOCK 2048
static double blip_kernel[BLIP_PHASES][BLIP_WIDTH]; /* the impulse response at each fraction of a sample, normalised to a sum of 1 */
static int blip_ready;

static void blip_init(void) {
    const double pi = 3.14159265358979323846;
    for (int p = 0; p < BLIP_PHASES; p++) {
        double f = (double)p / BLIP_PHASES, sum = 0.0;
        for (int i = 0; i < BLIP_WIDTH; i++) {
            double x = (double)i - BLIP_WIDTH / 2 + 1 - f; /* the distance from the step to this sample */
            double sinc = fabs(x) < 1e-9 ? 2.0 * BLIP_CUTOFF : sin(2.0 * pi * BLIP_CUTOFF * x) / (pi * x);
            double w = 0.42 + 0.5 * cos(2.0 * pi * x / BLIP_WIDTH) + 0.08 * cos(4.0 * pi * x / BLIP_WIDTH); /* Blackman */
            blip_kernel[p][i] = sinc * (fabs(x) < BLIP_WIDTH / 2.0 ? w : 0.0);
            sum += blip_kernel[p][i];
        }
        for (int i = 0; i < BLIP_WIDTH; i++) {
            blip_kernel[p][i] /= sum;
        }
    }
    blip_ready = 1;
}

void apu_mix(Game *g, int16_t *out, int n) {
    Apu *a = &g->apu;
    if (!a->lfsr) {
        a->lfsr = 1;
    }
    a->dmc_rom = g->rom;
    if (!blip_ready) {
        blip_init();
    }
    static double buf[BLIP_MAX_BLOCK + BLIP_WIDTH + 2];
    if (n > BLIP_MAX_BLOCK) {
        n = BLIP_MAX_BLOCK;
    }
    memset(buf, 0, ((size_t)n + BLIP_WIDTH + 2) * sizeof buf[0]);
    memcpy(buf, a->blip_tail, sizeof a->blip_tail); /* what the previous block left */
    const double per_sample = 1789773.0 / 44100.0;
    double pos = 0.0; /* in samples from the start of the block */
    for (int s = 0; s < n; s++) {
        a->cyc_acc += per_sample;
        int cycles = (int)a->cyc_acc; /* same as subtracting 1.0 while it is >= 1.0: those subtractions are exact */
        a->cyc_acc -= cycles;
        for (int left = cycles; left > 0;) {
            int step = left < 2 ? left : 2;
            apu_run(a, step);
            pos += (double)step / per_sample;
            double level = mixed_level(a);
            if (level !=
                a->blip_last) { /* a step: added with its fraction of a sample (the output is delayed by half the width, so that it is never in the past) */
                int k = (int)pos;
                const double *kern = blip_kernel[(int)((pos - k) * BLIP_PHASES)];
                double delta = level - a->blip_last;
                for (int i = 0; i < BLIP_WIDTH; i++) {
                    buf[k + 1 + i] += delta * kern[i];
                }
                a->blip_last = level;
            }
            left -= step;
        }
    }
    for (int s = 0; s < n; s++) {
        a->blip_int += buf[s];
        double v = a->blip_int;
        a->hp += (v - a->hp) * 0.0028; /* high-pass ~20 Hz: removes the DC component */
        double o = (v - a->hp) * OUTPUT_GAIN;
        out[s] = (int16_t)(o > 32767 ? 32767 : o < -32768 ? -32768 : o);
    }
    memcpy(a->blip_tail, &buf[n], sizeof a->blip_tail);
}
