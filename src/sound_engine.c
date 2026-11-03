/* Sound driver of bank 0 ($85F4-$9185): reads the music and sound-effect streams from the ROM and writes the APU registers.
 *
 * Channels: 0 pulse 1, 1 pulse 2, 2 triangle, 3 noise (the music); 4 pulse and 5 noise (the sound effects, which take the place of
 * the music on the same APU register). Each channel has one element in each RAM array $0100-$01A6 (same addresses as
 * the NES: some arrays are only used by channels 0 and 1, their addresses then overlap with the following ones).
 *
 * A stream is a sequence of events: a note (pitch + duration), a rest, or a command (base duration, volume envelope,
 * vibrato, echo, loop, sub-stream call, end). Two formats: music and sound effects; the first byte of the stream decides
 * ($30 and above: music).
 *
 * Entry points: sound_engine_init ($901E), sound_engine_play ($905F), sound_engine_update ($864C, once per frame). */
#include "game.h"

/* --- channel state (RAM) --- */
#define TIMER(x) M(SndChanTimer + (x))    /* frames left before the next event */
#define SOUND_ID(x) M(SndChanSound + (x)) /* number of the sound being played (0 = free channel) */
#define UNIT(x) M(SndChanParam + (x))     /* base duration of a note step */
#define FLAGS(x) M(SndChanFlags + (x))
#define LOOP_COUNT(x) M(0x011C + (x))   /* loops already done (command F-E) */
#define PTR_LO(x) M(SndChanPtrLo + (x)) /* position in the stream */
#define PTR_HI(x) M(SndChanPtrHi + (x))
#define LOOP_LO(x) M(0x012E + (x)) /* loop start (command F-B) */
#define LOOP_HI(x) M(0x0134 + (x))
#define RET_LO(x) M(0x013A + (x)) /* sub-stream return (command F-D) */
#define RET_HI(x) M(0x0140 + (x))
#define LAST_HI(x) M(SndChanPeriodHi + (x)) /* last period high byte written */
#define VOLUME(x) M(SndChanNote + (x))      /* current volume 0-15 */
#define VOLUME_SET(x) M(0x0152 + (x))       /* volume of the note */
#define DUTY(x) M(0x0158 + (x))             /* duty-cycle bits / volume mode of register $4000 */
#define OCTAVE(x) M(0x015E + (x))           /* octave 0-5 (5 = highest: period not shifted) */
#define TRANSPOSE(x) M(0x0161 + (x))        /* transposition in semitones x2 (bit 7: towards the low end) */
#define ENV_ID(x) M(0x0164 + (x))           /* number of the volume envelope (bit 7: simple decay) */
#define ENV_TICK(x) M(0x0166 + (x))
#define ENV_POS(x) M(0x0168 + (x))
#define ENV_LOOPS(x) M(0x016A + (x))
#define ENV_LOOP_POS(x) M(0x016C + (x))
#define RELEASE_AT(x) M(0x016E + (x)) /* the release starts when TIMER falls below this value */
#define RELEASE_PARAM(x) M(0x0170 + (x))
#define DECAY_RATE(x) M(0x0172 + (x))
#define DECAY_TICK(x) M(0x0174 + (x))
#define MIN_VOLUME(x) M(0x0176 + (x))
#define LOW_PERIOD(x) M(SndChanPeriodLo + (x)) /* last period low byte written (channels 0 and 1) */
#define PERIOD_LO(x) M(0x017A + (x))           /* period of the note (channels 0 and 1, after the octave shift) */
#define PERIOD_HI(x) M(0x017C + (x))
#define VIB_DELAY(x) M(0x017E + (x)) /* delay before the vibrato (channels 0 and 1) */
#define VIB_COUNT(x) M(0x0180 + (x))
#define VIB_DEPTH(x) M(0x0182 + (x))
#define VIB_TABLE(x) M(0x0184 + (x))
#define VIB_POS(x) M(0x0186 + (x))
#define VIB_LOOP_POS(x) M(0x0188 + (x))
#define VIB_LOOPS(x) M(0x018A + (x))
#define ENV_EXTRA(x) M(0x018C + (x))
#define ECHO_MODE(x) M(0x018E + (x)) /* bit 7: no echo; bit 6: ring to fill; bit 4: double time; bits 0-3: delay */
#define ECHO_POS(x) M(0x0190 + (x))
#define CUR_CHANNEL M(SndCurChan)
#define CUR_APU_OFFSET M(SndRegOffset) /* offset of the APU registers of the current channel (0, 4, 8, $C) */
#define SOUND_REQ M(SndRequest)        /* requested sound */
#define DRUM_ID M(0x0195)
#define FADE_LEVEL M(0x0197)
#define FADE_TICK M(0x0198)
#define FADE_STEPS M(MusicFadeSteps)
#define SWEEP_SFX M(0x019A)
#define TRI_LINEAR M(0x019C)
#define NOISE_DONE M(JingleDone)
#define TRI_BASE M(0x0154) /* base of the triangle's linear counter (command D) */
#define ECHO_LO(i) M(0x019E + (i))
#define ECHO_HI(i) M(0x01A6 + (i))

/* FLAGS: bit 0 sound-effect stream; bits 1-2 envelope phase (0 table, 2 decay, 4 end of table, 6 release);
 * bit 3 in a sub-stream; bit 4 vibrato; bit 5 envelope volume; bit 6 rest; bit 7 no sweep (sound effects) */
enum {
    F_SFX = 0x01,
    F_PHASE = 0x06,
    F_SUB = 0x08,
    F_VIBRATO = 0x10,
    F_EXTRA = 0x20,
    F_REST = 0x40,
    F_NOSWEEP = 0x80
};

enum {
    SQ1,
    SQ2,
    TRI,
    NOISE,
    SFX_SQ,
    SFX_NOISE,
    NCHANNELS
};

#define SOUND_TABLE_PTR K(g, 0, 0x901C) /* word: base of the sound table (3 bytes per sound: channel, stream address) */
#define PERIODS K(g, 0, 0x8FD4)         /* note periods, words, from the lowest to the highest */
#define ENV_TABLES K(g, 0, 0xBEB8)      /* pointers of the volume envelopes */
#define VIB_TABLES K(g, 0, 0xBEEA)      /* pointers of the vibrato tables */
#define DRUM_SOUNDS K(g, 0, 0x89C9)     /* sound or sample played by each drum hit of the noise channel */
#define DMC_SAMPLES K(g, 7, 0xFE60)     /* 4 bytes of registers $4010-$4013 per sample (bank 7) */

/* The sound engine works directly on the ROM as it is (addresses of the loaded ROM): in the US ROM its pointers are US addresses,
 * only the table addresses hard-coded here are translated (K). */
static uint16_t K(Game *g, int bank, uint16_t eu_addr) {
    return rom_eu_to_native(g->rom, bank, eu_addr);
}
static uint8_t rd(Game *g, uint16_t a) {
    return rom_prg_native(g->rom, a >= 0xC000 ? 7 : 0, a);
}
static uint16_t rd16(Game *g, uint16_t a) {
    return (uint16_t)(rd(g, a) | rd(g, (uint16_t)(a + 1)) << 8);
}

/* $EC/$ED: the period being computed (the 6502 leaves them in memory; the noise channel of the sound effects rereads an old $ED) */
static void set_ec_ed(Game *g, uint8_t lo, uint8_t hi) {
    M(0xEC) = lo;
    M(0xED) = hi;
}

static void set_stream_pos(Game *g, int x, uint16_t pos) {
    PTR_LO(x) = (uint8_t)pos;
    PTR_HI(x) = (uint8_t)(pos >> 8);
}
static uint16_t stream_pos(Game *g, int x) {
    return (uint16_t)(PTR_LO(x) | PTR_HI(x) << 8);
}

/* Channels 0 and 4 share the same APU register: as long as a sound effect plays on 4, the music does not write ($8CF0) */
static int apu_blocked(Game *g, int x) {
    return x == SQ1 && SOUND_ID(SFX_SQ) != 0;
}

static void apu_out(Game *g, int x, int reg, uint8_t v) {
    if (apu_blocked(g, x)) {
        return;
    }
    apu_write(g, (x == SQ1 ? 0 : CUR_APU_OFFSET) + reg, v);
}

/* offset of the APU registers of a channel: $4000, $4004, $4008, $400C; channel 4 takes that of 0 and 5 that of 3 */
static uint8_t apu_offset(int x) {
    return x == SFX_NOISE ? 0x0C : (uint8_t)((x << 2) & 0x0F);
}

static void triangle_off(Game *g) /* $8F4D */
{
    apu_write(g, 0x15, 0x0B);
    apu_write(g, 0x08, 0);
    apu_write(g, 0x15, 0x0F);
}

static void silence_all(Game *g) /* pulses and noise at volume 0, sweeps off */
{
    apu_write(g, 0x00, 0x30);
    apu_write(g, 0x04, 0x30);
    apu_write(g, 0x0C, 0x30);
    apu_write(g, 0x01, 0x7F);
    apu_write(g, 0x05, 0x7F);
}

void sound_engine_init(Game *g);
void sound_engine_play(Game *g, uint8_t id);

/* ------------------------------------------------------------------------------------------------ volume */
/* Volume written to the register: the requested volume minus the channel floor, minus the general fade; never 0 if it was not 0 */
static void output_volume(Game *g, int x, uint8_t vol) /* $8729 */
{
    int v = vol;
    if (v != 0) {
        v -= MIN_VOLUME(x);
        if (v <= 0) {
            v = 1;
        } else {
            v -= FADE_LEVEL;
            if (v <= 0) {
                v = 1;
            }
        }
    }
    uint8_t reg = (FLAGS(x) & F_EXTRA) ? (uint8_t)((ENV_EXTRA(x) & 0xF0) | v) : (uint8_t)(v | DUTY(x));
    apu_out(g, x, 0, reg);
}

static void set_volume(Game *g, int x, uint8_t vol) /* $8726 */
{
    VOLUME(x) = vol & 0x0F;
    output_volume(g, x, VOLUME(x));
}

/* One step of the volume envelope (table of bank 0, bytes: duration/volume, $FB-$FD loop start, $FE n loop, $FF end) */
static void envelope_step(Game *g, int x) /* $86FB */
{
    uint16_t tbl = rd16(g, (uint16_t)(ENV_TABLES + ENV_ID(x) * 2));
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        uint8_t y = ENV_POS(x);
        uint8_t b = rd(g, (uint16_t)(tbl + y));
        if (b < 0xFB) {
            if (b >= 0x10) {
                ENV_TICK(x) = b >> 4;
            }
            ENV_POS(x)++;
            VOLUME(x) = b & 0x0F;
            output_volume(g, x, VOLUME(x));
            return;
        }
        if (b == 0xFF) {
            FLAGS(x) = (uint8_t)((FLAGS(x) | 0x04) & 0xFD);
            return;
        } /* $87EB: end of the table */
        if (b == 0xFE) { /* $8778: repeat n times */
            ENV_LOOPS(x)++;
            if (rd(g, (uint16_t)(tbl + y + 1)) == ENV_LOOPS(x)) {
                ENV_LOOPS(x) = 0;
                ENV_LOOP_POS(x) = 0;
                ENV_POS(x) = (uint8_t)(ENV_POS(x) + 2);
            } else {
                ENV_POS(x) = ENV_LOOP_POS(x);
            }
            continue;
        }
        ENV_POS(x)++; /* $FB-$FD: loop start */
        ENV_LOOP_POS(x) = ENV_POS(x);
    }
}

/* Enters the release when the note reaches its end ($87B8) */
static void release_check(Game *g, int x) {
    if ((FLAGS(x) & F_PHASE) == F_PHASE) {
        return;
    }
    if (TIMER(x) >= RELEASE_AT(x)) {
        return;
    }
    FLAGS(x) |= F_PHASE;
    uint8_t e = ENV_EXTRA(x);
    if (e & 0x0C) { /* $87F6 */
        uint8_t v = (e & 0x0C) >> 2;
        if (e & 2) {
            FLAGS(x) |= F_EXTRA;
        }
        set_volume(g, x, v);
    } else if (e & 2) {
        FLAGS(x) |= F_EXTRA; /* $87DB */
    }
}

/* ------------------------------------------------------------------------------------------------ pitch */
/* Writes the period to the APU: the high byte only if it changes or if the mode requires it ($8BF2) */
static void write_period(Game *g, int x, uint8_t lo, uint8_t hi) {
    int write_hi;
    if (hi != LAST_HI(x)) {
        write_hi = 1;
    } else if (FLAGS(x) & F_NOSWEEP) {
        write_hi = 1;
    } else {
        write_hi = !(DUTY(x) & 0x10);
    }
    if (write_hi) {
        LAST_HI(x) = hi;
        apu_out(g, x, 3, (uint8_t)(hi | 0x08));
    }
    if (x < 2) {
        LOW_PERIOD(x) = lo;
    }
    apu_out(g, x, 2, lo);
}

static void apply_period(Game *g, int x, uint8_t lo, uint8_t hi) /* $8BE2 */
{
    set_ec_ed(g, lo, hi);
    if (x != TRI) {
        if (M(0x0180 + x) == 0xFF) {
            return; /* (address shared with other arrays, see the header) */
        }
        if (x != SFX_NOISE) {
            write_period(g, x, lo, hi);
            return;
        }
    }
    LAST_HI(x) = hi; /* $8C05: triangle and noise: always written */
    apu_out(g, x, 3, (uint8_t)(hi | 0x08));
    if (x < 2) {
        LOW_PERIOD(x) = lo;
    }
    apu_out(g, x, 2, lo);
}

/* Stores the period in the ring of 4 ($019E/$01A6) used by the echo ($8C2C) */
static void push_echo(Game *g, int x, uint8_t lo, uint8_t hi) {
    uint8_t y = (uint8_t)(ECHO_POS(x) | (x == SQ2 ? 4 : 0));
    ECHO_LO(y) = lo;
    ECHO_HI(y) = hi;
    if (ECHO_MODE(x) & 0x40) { /* first note: fills the whole ring */
        ECHO_MODE(x) &= 0xBF;
        for (int n = 0; n < 3; n++) {
            y++;
            ECHO_LO(y) = lo;
            ECHO_HI(y) = hi;
        }
    }
    ECHO_POS(x) = (ECHO_POS(x) + 1) & 3;
}

/* Vibrato: a table of bank 0 gives, step by step, a period deviation (0-7 towards the high end, 8-15 towards the low end) times the depth;
 * bytes $FB-$FD loop start, $FE n loop, $FF end. Returns the modified period; 0 if the table is finished ($885F). */
static int vibrato_step(Game *g, int x, uint8_t *lo, uint8_t *hi) {
    uint16_t tbl = rd16(g, (uint16_t)(VIB_TABLES + VIB_TABLE(x) * 2));
    for (unsigned iterations = 0;; iterations++) {
        LOOP_GUARD(iterations)
        uint8_t y = VIB_POS(x);
        uint8_t b = rd(g, (uint16_t)(tbl + y));
        if (b < 0xFB) {
            if (b >= 0x10) {
                VIB_COUNT(x) = b >> 4;
            }
            VIB_POS(x)++;
            uint8_t n = b & 0x0F;
            uint8_t amp = n < 8 ? n : (uint8_t)(0x10 - n);
            uint8_t d = VIB_DEPTH(x), prod = amp; /* amp x depth, on 8 bits (6502 loop) */
            for (;;) {
                d--;
                if (d == 0) {
                    break;
                }
                prod = (uint8_t)(prod + amp);
            }
            uint16_t base = (uint16_t)(PERIOD_LO(x) | PERIOD_HI(x) << 8);
            uint16_t per = n < 8 ? (uint16_t)(base + prod) : (uint16_t)(base - prod);
            *lo = (uint8_t)per;
            *hi = (uint8_t)(per >> 8);
            set_ec_ed(g, *lo, *hi);
            return 1;
        }
        if (b == 0xFF) {
            VIB_COUNT(x) = 0xFF;
            return 0;
        }
        if (b == 0xFE) {
            VIB_LOOPS(x)++;
            if (rd(g, (uint16_t)(tbl + y + 1)) == VIB_LOOPS(x)) {
                VIB_LOOPS(x) = 0;
                VIB_LOOP_POS(x) = 0;
                VIB_POS(x) = (uint8_t)(VIB_POS(x) + 2);
            } else {
                VIB_POS(x) = VIB_LOOP_POS(x);
            }
            continue;
        }
        VIB_POS(x)++;
        VIB_LOOP_POS(x) = VIB_POS(x);
    }
    return 0; /* loop guard tripped (corrupt vibrato table) */
}

/* ------------------------------------------------------------------------------------------------ stream events */
static void dmc_play(Game *g, uint8_t sample);

/* Drum hit of the noise channel: the byte gives a sound effect or a DMC sample ($897A) */
static void drum_hit(Game *g, uint16_t pos) {
    uint8_t drum = rd(g, pos) >> 4;
    if (drum == 0x0C) {
        return;
    }
    DRUM_ID = drum;
    uint8_t what = rd(g, (uint16_t)(DRUM_SOUNDS + drum));
    if (what >= 0x8F) {
        dmc_play(g, (uint8_t)(what - 0x8F));
    } else {
        sound_engine_play(g, what);
    }
}

/* Duration of the event: TIMER = UNIT x (n + 1); restarts the vibrato; computes the release threshold ($8C7D).
 * For the noise channel the event is a drum hit: it ends here. */
static void begin_event(Game *g, int x, uint8_t n, uint16_t pos) {
    uint8_t t = UNIT(x);
    for (int i = 0; i < (n & 0x0F); i++) {
        t = (uint8_t)(t + UNIT(x));
    }
    TIMER(x) = t;
    if (x < 2 && (FLAGS(x) & F_VIBRATO)) {
        VIB_COUNT(x) = (uint8_t)(VIB_DELAY(x) + 1);
        VIB_POS(x) = VIB_LOOPS(x) = VIB_LOOP_POS(x) = 0;
    }
    if (x == TRI) {
        return;
    }
    if (x == NOISE) {
        set_stream_pos(g, x, (uint16_t)(pos + 1));
        drum_hit(g, pos);
        return;
    }
    RELEASE_AT(x) = (uint8_t)((unsigned)(RELEASE_PARAM(x) & 0x0F) * TIMER(x) >> 4); /* $8CC5: param x duration / 16 */
}

/* Period of the note, transposed then shifted by the octave ($8B78) */
static void note_period(Game *g, int x, uint16_t pos, uint8_t *lo, uint8_t *hi) {
    uint8_t idx = (uint8_t)((rd(g, pos) & 0xF0) >> 3); /* 2 bytes per note */
    uint8_t t = TRANSPOSE(x);
    uint16_t addr;
    if (t == 0) {
        addr = (uint16_t)(PERIODS + 0x18 + idx);
    } else if (!(t & 0x80)) {
        addr = (uint16_t)(PERIODS + 0x18 + (uint8_t)(idx + t));
    } else {
        uint8_t mag = t & 0x7F;
        if (idx >= mag) {
            addr = (uint16_t)(PERIODS + 0x18 + (uint8_t)(idx - mag));
        } else {
            addr = (uint16_t)(PERIODS + (uint8_t)(idx - mag + 0x18));
        }
    }
    uint16_t per = rd16(g, addr);
    for (int o = OCTAVE(x); o != 5;) { /* shift right down to octave 5 */
        per >>= 1;
        o = (o + 1) & 0xFF;
        if (o == 0) {
            break;
        }
    }
    *lo = (uint8_t)per;
    *hi = (uint8_t)(per >> 8);
    set_ec_ed(g, *lo, *hi);
}

/* End of a melodic note: echo, period, vibrato, APU write; the stream advances by one byte */
static void finish_note(Game *g, int x, uint16_t pos) {
    uint8_t lo, hi;
    note_period(g, x, pos, &lo, &hi);
    if (x != TRI) {
        if (!(ECHO_MODE(x) & 0x80)) {
            push_echo(g, x, lo, hi);
        }
        PERIOD_LO(x) = lo;
        PERIOD_HI(x) = hi;
        if ((FLAGS(x) & F_VIBRATO) && VIB_DELAY(x) == 0) {
            vibrato_step(g, x, &lo, &hi);
        }
    }
    apply_period(g, x, lo, hi);
    set_stream_pos(g, x, (uint16_t)(pos + 1));
}

/* Triangle: the linear counter (volume) depends on the duration of the note ($8ABF) */
static void triangle_note(Game *g, int x, uint16_t pos) {
    unsigned n = (rd(g, pos) & 0x0F) + 1u;
    uint8_t base = TRI_BASE, v;
    if (base == 0) {
        v = 0x90;
    } else if (base >= 0x80) {
        v = (uint8_t)(base - 0x80);
    } else {
        unsigned acc = base + 1u, sum = acc;
        int over = 0;
        for (unsigned k = 1; k < n; k++) {
            sum += acc;
            if (sum > 0xFF) {
                over = 1;
                break;
            }
        }
        v = (over || sum >= 0x80) ? 0x90 : (uint8_t)sum;
    }
    apu_write(g, 0x08, v);
    TRI_LINEAR = v;
    finish_note(g, x, pos);
}

static void note_event(Game *g, int x, uint16_t pos) /* $8B46 */
{
    begin_event(g, x, rd(g, pos) & 0x0F, pos);
    if (x == TRI) {
        triangle_note(g, x, pos);
        return;
    }
    FLAGS(x) &= 0xDF; /* $8B2F: envelope volume? */
    if (ENV_EXTRA(x) & 1) {
        FLAGS(x) |= F_EXTRA;
    }
    if (ENV_ID(x) & 0x80) { /* $8AF4: simple decay, ENV_ID & 15 steps */
        ENV_TICK(x) = 1;
        set_volume(g, x, VOLUME_SET(x));
        ENV_POS(x) = ENV_ID(x) & 0x0F;
        FLAGS(x) = (uint8_t)((FLAGS(x) & 0x9B) | 0x02);
    } else {
        ENV_POS(x) = ENV_LOOP_POS(x) = ENV_LOOPS(x) = 0;
        ENV_TICK(x) = 1;
        envelope_step(g, x);
        FLAGS(x) &= 0x99;
    }
    finish_note(g, x, pos);
}

/* End of stream ($8F18): the channel is freed and its registers cut; at the end of a sound effect of channel 4 the music of 0 is restored */
static void restore_music_pulse(Game *g) /* $85FB */
{
    if ((FLAGS(SQ1) & 0x41) | M(0x0196)) {
        apu_write(g, 0x00, 0x30);
    } else {
        output_volume(g, SQ1, VOLUME(SQ1));
    }
    apu_write(g, CUR_APU_OFFSET + 2, LOW_PERIOD(SQ1));
    apu_write(g, CUR_APU_OFFSET + 3, LAST_HI(SQ1));
}

static void stream_end(Game *g, int x) {
    uint8_t old = SOUND_ID(x);
    SOUND_ID(x) = 0;
    switch (x) {
        case TRI:
            triangle_off(g);
            break;
        case NOISE:
            apu_write(g, 0x0C, 0x30);
            if (old == 0x83 || old == 0x7F) {
                NOISE_DONE = 0xFF;
            }
            break;
        case SFX_SQ:
            apu_write(g, CUR_APU_OFFSET, 0x30);
            apu_write(g, CUR_APU_OFFSET + 1, 0x7F);
            if (SOUND_ID(CUR_APU_OFFSET)) {
                restore_music_pulse(g);
            }
            break;
        default:
            apu_out(g, x, 0, 0x30);
            break;
    }
}

/* F commands (music) / $FB-$FF (sound effects): volume floor, loop start, call, repeat, end.
 * Returns 1 if reading continues on the new *pos, 0 if the stream stops ($8E6E). */
static int control_command(Game *g, int x, uint8_t n, uint16_t *pos) {
    if (n > 0x0E) { /* F-F: sub-stream return, or end */
        if (FLAGS(x) & F_SUB) {
            *pos = (uint16_t)(RET_LO(x) | RET_HI(x) << 8);
            FLAGS(x) &= (uint8_t)~F_SUB;
            return 1;
        }
        stream_end(g, x);
        return 0;
    }
    if (n == 0x0E) { /* F-E: repeat n times */
        LOOP_COUNT(x)++;
        uint8_t count = rd(g, (uint16_t)(*pos + 1));
        if (LOOP_COUNT(x) == count) {
            LOOP_COUNT(x) = 0;
            *pos = (uint16_t)(*pos + 2);
        } else if ((uint8_t)(LOOP_COUNT(x) - count) & 0x80) {
            *pos = (uint16_t)(LOOP_LO(x) | LOOP_HI(x) << 8);
        } else {
            LOOP_COUNT(x)--;
            *pos = rd16(g, (uint16_t)(*pos + 2));
        }
        return 1;
    }
    if (n == 0x0D) { /* F-D: call of a sub-stream */
        uint16_t ret = (uint16_t)(*pos + 3);
        RET_LO(x) = (uint8_t)ret;
        RET_HI(x) = (uint8_t)(ret >> 8);
        FLAGS(x) |= F_SUB;
        *pos = rd16(g, (uint16_t)(*pos + 1));
        return 1;
    }
    if (n == 0x0B) { /* F-B: marks the loop start */
        uint16_t p = (uint16_t)(*pos + 1);
        LOOP_LO(x) = (uint8_t)p;
        LOOP_HI(x) = (uint8_t)(p >> 8);
        *pos = p;
        return 1;
    }
    MIN_VOLUME(x) = rd(g, *pos) & 0x0F; /* other F-n: volume floor */
    *pos = (uint16_t)(*pos + 1);
    return 1;
}

/* Release byte: low nibble = fraction of the duration before the end where the release starts, high nibble = speed ($8D53) */
static void release_params(Game *g, int x, uint16_t pos) {
    uint8_t b = rd(g, pos);
    RELEASE_PARAM(x) = b & 0x0F;
    DECAY_RATE(x) = b >> 4;
    DECAY_TICK(x) = DECAY_RATE(x) ? DECAY_RATE(x) : 1;
}

/* E commands (music): octave, base duration, envelope, release, envelope volume, transposition, vibrato, echo ($8D78) */
static int edit_command(Game *g, int x, uint8_t n, uint16_t *pos) {
    if (n < 6) {
        OCTAVE(x) = n;
        *pos += 1;
        return 1;
    }
    uint16_t p = (uint16_t)(*pos + 1);
    uint8_t b = rd(g, p);
    switch (n) {
        case 6:
            UNIT(x) = b;
            *pos = (uint16_t)(p + 1);
            break;
        case 7:
            ENV_ID(x) = b;
            *pos = (uint16_t)(p + 1);
            break;
        case 8:
            release_params(g, x, p);
            *pos = (uint16_t)(p + 1);
            break;
        case 9: /* envelope volume */
            if (b == 0) {
                FLAGS(x) &= 0xDF;
            } else if ((b & 0x0F) == 0) {
                DUTY(x) = b;
            } else {
                ENV_EXTRA(x) = b;
                FLAGS(x) |= F_EXTRA;
            }
            *pos = (uint16_t)(p + 1);
            break;
        case 10:
            TRANSPOSE(x) = (uint8_t)((b << 1) | (b & 0x80));
            *pos = (uint16_t)(p + 1);
            break;
        case 11: /* vibrato: delay, depth, table */
            if (b == 0) {
                FLAGS(x) &= 0xEF;
                *pos = (uint16_t)(p + 1);
                break;
            }
            {
                uint8_t k = b >> 4, d = UNIT(x);
                if (k == 0) {
                    d = 0;
                }
                for (int i = 1; i < k; i++) {
                    d = (uint8_t)(d + UNIT(x));
                }
                VIB_DELAY(x) = d;
                VIB_DEPTH(x) = b & 0x0F;
                VIB_TABLE(x) = rd(g, (uint16_t)(p + 1));
                FLAGS(x) |= F_VIBRATO;
                *pos = (uint16_t)(p + 2);
            }
            break;
        default: /* echo */
            if (b == 0) {
                ECHO_MODE(x) = 0x80;
            } else {
                uint8_t m = b;
                if (m >= 4) {
                    m = (uint8_t)((m - 3) | 0x10);
                }
                ECHO_MODE(x) = (uint8_t)(m | 0x40);
                ECHO_POS(x) = 0;
            }
            *pos = (uint16_t)(p + 1);
            break;
    }
    return 1;
}

/* One event of the music stream ($892D); returns 1 if reading continues on *pos */
static int music_step(Game *g, int x, uint16_t *pos) {
    uint8_t b = rd(g, *pos);
    if (x == NOISE) { /* $8959: noise has its own format */
        uint8_t hi = b & 0xF0;
        if (hi == 0xF0) {
            return control_command(g, x, b & 0x0F, pos);
        }
        if (hi == 0xD0) {
            UNIT(x) = b & 0x0F;
            *pos += 1;
            return 1;
        }
        begin_event(g, x, b & 0x0F, *pos);
        return 0;
    }
    if ((b & 0xF0) < 0xC0) {
        note_event(g, x, *pos);
        return 0;
    }
    uint8_t n = b & 0x0F;
    switch ((b >> 4) & 3) {
        case 0: /* C : silence */
            begin_event(g, x, n, *pos);
            if (x == TRI) {
                triangle_off(g);
            } else {
                apu_out(g, x, 0, 0x30);
            }
            FLAGS(x) |= F_REST;
            set_stream_pos(g, x, (uint16_t)(*pos + 1));
            return 0;
        case 1: { /* D: base duration, volume, envelope, release */
            UNIT(x) = n;
            uint16_t p = (uint16_t)(*pos + 1);
            uint8_t b1 = rd(g, p);
            if (x == TRI) {
                TRI_BASE = b1;
                *pos = (uint16_t)(p + 1);
                return 1;
            }
            DUTY(x) = b1 & 0xF0;
            VOLUME_SET(x) = b1 & 0x0F;
            p++;
            ENV_ID(x) = rd(g, p);
            MIN_VOLUME(x) = (ENV_ID(x) & 0x80) ? 0 : VOLUME_SET(x);
            p++;
            release_params(g, x, p);
            *pos = (uint16_t)(p + 1);
            return 1;
        }
        case 2:
            return edit_command(g, x, n, pos);
        default:
            return control_command(g, x, n, pos);
    }
}

/* One event of the sound-effect stream ($89D5): settings ($F8-$FA), channel setting ($0n), or note */
static int sfx_step(Game *g, int x, uint16_t *pos) {
    uint8_t b = rd(g, *pos);
    if (b >= 0xFB) {
        return control_command(g, x, b & 0x0F, pos);
    }
    if (x != SFX_NOISE && b >= 0xF8) {
        uint8_t n = b & 0x0F;
        uint8_t v = rd(g, (uint16_t)(*pos + 1));
        if (n == 10) { /* FA: sweep register */
            if (x == SFX_SQ) {
                SWEEP_SFX = v;
            }
            apu_out(g, x, 1, v);
        } else if (n == 9) {
            DUTY(x) = v; /* F9: volume bits */
        } else {
            UNIT(x) = v; /* F8: base duration */
        }
        *pos = (uint16_t)(*pos + 2);
        return 1;
    }
    if ((b & 0xF0) == 0 && (b & 0x0F) != 0) { /* 0n: channel setting */
        UNIT(x) = b & 0x0F;
        uint16_t p = (uint16_t)(*pos + 1);
        uint8_t v = rd(g, p);
        if (x == TRI) {
            apu_write(g, 0x08, v);
            *pos = (uint16_t)(p + 1);
            return 1;
        }
        if (x == SFX_NOISE) {
            DUTY(x) = 0x30;
            *pos = p;
            return 1;
        } /* the setting byte is not consumed: it is reread as an event ($8A71 jumps to $8A5A, without the INY) */
        DUTY(x) = v;
        p++;
        uint8_t v2 = rd(g, p);
        if (v2 == 0x88) {
            FLAGS(x) &= 0x7F;
            v2 = 0x7F;
        } else {
            FLAGS(x) |= F_NOSWEEP;
        }
        if (x == SFX_SQ) {
            SWEEP_SFX = v2;
        }
        apu_out(g, x, 1, v2);
        *pos = (uint16_t)(p + 1);
        return 1;
    }
    /* note: volume (high nibble), 12-bit period; the duration is the base duration */
    TIMER(x) = UNIT(x);
    if (x != TRI) {
        VOLUME_SET(x) = b >> 4;
        uint8_t reg = (DUTY(x) & 0x10) ? (uint8_t)(VOLUME_SET(x) | DUTY(x)) : DUTY(x);
        apu_out(g, x, 0, reg);
    }
    uint8_t hi = b & 0x0F, lo;
    uint16_t p = *pos;
    if (x == SFX_NOISE) {
        lo = hi;
        hi = M(0xED);
    } /* 4-bit noise period; $ED stays what it was */
    else {
        p++;
        lo = rd(g, p);
    }
    apply_period(g, x, lo, hi);
    set_stream_pos(g, x, (uint16_t)(p + 1));
    return 0;
}

static void read_events(Game *g, int x, uint16_t pos) /* $8927 */
{
    for (int guard = 0;; guard++) {
        int more = (FLAGS(x) & F_SFX) ? sfx_step(g, x, &pos) : music_step(g, x, &pos);
        if (!more) {
            return;
        }
        if (guard > 100000) {
            TRACE("f=%u TODO sound: endless stream channel %d pos=%04X\n", g->frame, x, pos);
            return;
        } /* misread data (US ROM): the game is not blocked */
    }
}

/* ------------------------------------------------------------------------------------------------ channel */
/* Simple decay (ENV_ID bit 7): the volume drops one notch at each step, ENV_POS steps in total ($879D) */
static void simple_decay(Game *g, int x) {
    if ((int8_t)--ENV_POS(x) < 0) {
        FLAGS(x) = (uint8_t)((FLAGS(x) | 0x04) & 0xFD);
        return;
    }
    if ((int8_t)--VOLUME(x) < 0) {
        VOLUME(x)++;
        return;
    }
    set_volume(g, x, VOLUME(x));
}

/* Release: possible pitch echo, then the volume drops one notch every DECAY_RATE steps, without falling to 0 ($8802) */
static void release_phase(Game *g, int x) {
    if (!(ECHO_MODE(x) & 0x80)) {
        uint8_t i = (uint8_t)((((ECHO_POS(x) - 1) & 3) - (ECHO_MODE(x) & 0x0F)) & 3);
        if (x == SQ2) {
            i |= 4;
        }
        set_ec_ed(g, ECHO_LO(i), ECHO_HI(i));
        write_period(g, x, ECHO_LO(i), ECHO_HI(i));
    }
    if (ENV_EXTRA(x) & 0x0C) {
        return;
    }
    if (--DECAY_TICK(x) != 0) {
        return;
    }
    DECAY_TICK(x) = DECAY_RATE(x) ? DECAY_RATE(x) : 1;
    if ((int8_t)--VOLUME(x) < 0 || VOLUME(x) == 0) {
        VOLUME(x)++;
        return;
    }
    output_volume(g, x, VOLUME(x));
}

static void channel_update(Game *g, int x) /* $8679 */
{
    uint16_t pos = stream_pos(g, x);
    uint8_t flags = FLAGS(x);
    if (--TIMER(x) == 0) {
        read_events(g, x, pos);
        return;
    }
    if (x >= 2) {
        return; /* triangle, noise and sound effects: no envelope */
    }
    if (flags & (F_SFX | F_REST)) {
        return;
    }
    int skip_vibrato = 0;
    if (!(ECHO_MODE(x) & 0x80)) {
        uint8_t unit = UNIT(x);
        if (ECHO_MODE(x) & 0x10) {
            unit = (uint8_t)(unit << 1);
        }
        uint8_t t = TIMER(x);
        while (t >= unit) { /* is TIMER a multiple of unit? */
            t = (uint8_t)(t - unit);
            if (t == 0) {
                set_ec_ed(g, PERIOD_LO(x), PERIOD_HI(x));
                push_echo(g, x, PERIOD_LO(x), PERIOD_HI(x));
                break;
            }
        }
        skip_vibrato = (FLAGS(x) & F_PHASE) == F_PHASE;
    }
    if (!skip_vibrato && (FLAGS(x) & F_VIBRATO) && --VIB_COUNT(x) == 0) { /* $86B8 */
        VIB_COUNT(x)++;
        uint8_t lo = M(0xEC), hi = M(0xED); /* table finished: the period stays that of the last computed note */
        vibrato_step(g, x, &lo, &hi);
        apply_period(g, x, lo, hi);
    }
    if (--ENV_TICK(x) != 0) {
        return; /* $86CD: one envelope step */
    }
    ENV_TICK(x)++;
    release_check(g, x);
    switch (FLAGS(x) & F_PHASE) {
        case 0:
            envelope_step(g, x);
            break;
        case 2:
            simple_decay(g, x);
            break;
        case 6:
            release_phase(g, x);
            break;
        default:
            break;
    }
}

/* General music fade: FADE_STEPS levels of $2F frames each ($861A) */
static void fade_update(Game *g) {
    if (FADE_STEPS == 0) {
        return;
    }
    if (FADE_STEPS == 1) {
        sound_engine_init(g);
        return;
    }
    if (FADE_STEPS == 2 && FADE_TICK == 0x20) {
        SOUND_ID(NOISE) = 0;
    }
    if (++FADE_TICK == 0x2F) {
        FADE_TICK = 0;
        FADE_LEVEL++;
        FADE_STEPS--;
    }
}

void sound_engine_update(Game *g) /* $864C */
{
    fade_update(g);
    for (int x = 0; x < NCHANNELS; x++) {
        CUR_CHANNEL = (uint8_t)x;
        CUR_APU_OFFSET = apu_offset(x);
        if (SOUND_ID(x)) {
            channel_update(g, x);
        }
    }
}

/* ------------------------------------------------------------------------------------------------ starting a sound */
void sound_engine_init(Game *g) /* $901E */
{
    if (SOUND_ID(NOISE) == 0x83) {
        return;
    }
    for (int i = 0; i < NCHANNELS; i++) {
        SOUND_ID(i) = 0;
    }
    M(0x0180) = M(0x0181) = 0;
    FADE_TICK = FADE_STEPS = FADE_LEVEL = 0;
    triangle_off(g);
    silence_all(g);
}

static void dmc_play(Game *g, uint8_t sample) /* $8996 */
{
    SOUND_REQ = sample;
    apu_write(g, 0x15, 0x0F);
    for (int i = 0; i < 4; i++) {
        apu_write(g, 0x10 + i, rd(g, (uint16_t)(DMC_SAMPLES + (uint8_t)(sample * 4) + i)));
    }
    apu_write(g, 0x15, 0x1F);
}

/* Starts the stream of a channel ($9102): the first byte of the stream says whether it is in music ($30 and above) or sound-effect format */
static void start_stream(Game *g, int x, uint8_t id, uint16_t stream) {
    SOUND_ID(x) = 0;
    set_stream_pos(g, x, stream);
    LAST_HI(x) = 0xF8;
    TIMER(x) = 1;
    if (x < 3) {
        if (x != TRI) {
            MIN_VOLUME(x) = 0;
            ENV_EXTRA(x) = 0;
            ECHO_MODE(x) = 0x80;
        }
        TRANSPOSE(x) = 0;
    }
    LOOP_COUNT(x) = 0;
    uint8_t flags = F_SFX;
    if (rd(g, stream) >= 0x30) {
        flags = 0;
        FADE_TICK = FADE_LEVEL = 0;
    }
    FLAGS(x) = flags;
    SOUND_ID(x) = id;
    if (x >= 1) {
        uint8_t off = apu_offset(x);
        apu_write(g, off, off == 8 ? 0 : 0x30);
        apu_write(g, off + 1, 0x7F);
    } else if (SOUND_ID(SFX_SQ) == 0) {
        apu_write(g, 0x00, 0x30);
        apu_write(g, 0x01, 0x7F);
    }
}

void sound_engine_play(Game *g, uint8_t id) /* $905F */
{
    SOUND_REQ = id;
    if (id >= 0x8F) {
        dmc_play(g, (uint8_t)(id - 0x8F));
        return;
    }
    if (id >= 0x4B) {
        if (SOUND_ID(NOISE) == 0x83) {
            return;
        }
        M(0x0180) = M(0x0181) = 0;
        if (id != 0x77 && id != 0x7F) {
            sound_engine_init(g);
        }
        if (id == 0x83 || id == 0x7F) {
            NOISE_DONE = 0;
        }
    } else {
        if (id == 0x16 && SOUND_ID(SFX_NOISE) == 0x16) {
            return;
        }
        if (SOUND_ID(SQ2) == 0x77 && id >= 0x07 && id < 0x3E) {
            return;
        }
    }
    uint16_t entry = (uint16_t)(rd16(g, SOUND_TABLE_PTR) + id * 3);
    int n = (rd(g, entry) >> 3) & 3;
    for (int i = 0; i <= n; i++) {
        uint16_t e = (uint16_t)(entry + i * 3);
        int x = rd(g, e) & 7;
        if (id >= SOUND_ID(x)) {
            start_stream(g, x, id, rd16(g, (uint16_t)(e + 1)));
        }
    }
}
