#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "rom_us_map.h"

/* American ROM: the code is the same program as the European one, with a few bytes more or less here and there; the whole port
 * reasons in European addresses (those of the disassembly). For a US ROM every read is translated: EU address -> US byte,
 * thanks to the identical blocks (tools/rom_diff_us.py). Words read from the ROM are US addresses: rom_word turns them back into EU addresses. */

struct Rom {
    uint8_t prg[8 * 0x4000];
    uint8_t chr[32 * 0x1000];
    int us;
    int16_t eu2us[8][0x4000]; /* offset to add to the EU offset (INT16_MIN: no match) */
    int16_t us2eu[8][0x4000];
};

#define NOMAP INT16_MIN
#define US_PRG_HASH 0xC9073E70u /* FNV-1a of the 128 KB PRG of the US ROM (U) [!] */

static void build_maps(Rom *r) {
    for (int b = 0; b < 8; b++) {
        for (int i = 0; i < 0x4000; i++) {
            r->eu2us[b][i] = r->us2eu[b][i] = NOMAP;
        }
        for (const int(*t)[3] = us_blocks[b]; (*t)[2]; t++) {
            int e = (*t)[0], u = (*t)[1], n = (*t)[2];
            for (int k = 0; k < n; k++) {
                r->eu2us[b][e + k] = (int16_t)(u - e);
                r->us2eu[b][u + k] = (int16_t)(e - u);
            }
        }
        /* gaps (zones that differ): the start of the EU zone is aligned with the start of the matching US zone */
        int pe = 0, pu = 0;
        for (const int(*t)[3] = us_blocks[b];; t++) {
            int e = (*t)[2] ? (*t)[0] : 0x4000, u = (*t)[2] ? (*t)[1] : 0x4000;
            for (int k = 0; pe + k < e; k++) {
                int uo = pu + k < u ? pu + k : u - 1; /* shorter US zone: stay on its last byte */
                if (uo < 0) {
                    uo = 0;
                }
                r->eu2us[b][pe + k] = (int16_t)(uo - (pe + k));
            }
            for (int k = 0; pu + k < u; k++) {
                int eo = pe + k < e ? pe + k : e - 1;
                if (eo < 0) {
                    eo = 0;
                }
                r->us2eu[b][pu + k] = (int16_t)(eo - (pu + k));
            }
            if (!(*t)[2]) {
                break;
            }
            pe = e + (*t)[2];
            pu = u + (*t)[2];
        }
    }
}

static uint32_t prg_hash(const uint8_t *p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

uint8_t rom_chr(const Rom *r, int bank4k, uint16_t off) {
    return r->chr[(bank4k & 31) * 0x1000 + (off & 0xFFF)];
}

#define EU_PRG_HASH 0x8970409Du /* FNV-1a of the 128 KB PRG of the European ROM */

static char rom_error_text[160];
static int allow_unknown_rom;

const char *rom_error(void) {
    return rom_error_text;
}
void rom_allow_unknown(int allow) {
    allow_unknown_rom = allow;
}

static Rom *rom_fail(const char *fmt, const char *arg) {
    snprintf(rom_error_text, sizeof rom_error_text, fmt, arg);
    return NULL;
}

/* Loads and validates a ROM image : iNES header (8 x 16 KB PRG, 16 x 8 KB CHR, MMC1) and, unless rom_allow_unknown(1) was called,
 * the PRG must be the European or the American release of the game. Returns NULL and sets rom_error() on failure. */
Rom *rom_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return rom_fail("cannot open %s", path);
    }
    uint8_t hdr[16];
    Rom *r = malloc(sizeof *r);
    if (!r) {
        fclose(f);
        return rom_fail("out of memory loading %s", path);
    }
    if (fread(hdr, 1, 16, f) != 16 || fread(r->prg, 1, sizeof r->prg, f) != sizeof r->prg || fread(r->chr, 1, sizeof r->chr, f) != sizeof r->chr) {
        fclose(f);
        free(r);
        return rom_fail("%s is too short for a 256 KB MMC1 image", path);
    }
    fclose(f);
    if (memcmp(hdr, "NES\x1A", 4) != 0 || hdr[4] != 8 || hdr[5] != 16 || (hdr[6] >> 4) != 1) { /* the US dump has stray bytes in 7, 8 and 15 : not checked */
        free(r);
        return rom_fail("%s is not an iNES image with 128 KB PRG, 128 KB CHR and mapper 1", path);
    }
    uint32_t hash = prg_hash(r->prg, sizeof r->prg);
    r->us = hash == US_PRG_HASH;
    if (!r->us && hash != EU_PRG_HASH && !allow_unknown_rom) {
        free(r);
        return rom_fail("%s is not the European or American release of the game (use --allow-unknown-rom to force)", path);
    }
    if (r->us) {
        build_maps(r);
    }
    return r;
}

int rom_is_us(const Rom *r) {
    return r->us;
}

uint8_t rom_prg_native(const Rom *r, int bank, uint16_t a) /* byte of the loaded ROM, no translation */
{
    ASSERT(bank >= 0 && bank < 8);
    return r->prg[(bank & 7) * 0x4000 + (a & 0x3FFF)];
}

uint16_t rom_word_native(const Rom *r, int bank, uint16_t a) /* word of the loaded ROM, no translation (EU address of the table) */
{
    uint16_t e = rom_eu_to_native(r, bank, a);
    return (uint16_t)(rom_prg_native(r, bank, e) | rom_prg_native(r, bank, (uint16_t)(e + 1)) << 8);
}

uint16_t rom_word_visible(const Rom *r, int bank, uint16_t a,
                          int visible_bank) /* word (address) read from a table of bank `bank`, towards the bank visible at $8000 if < $C000 */
{
    uint16_t n = rom_word_native(r, bank, a);
    if (!r->us || n < 0x8000) {
        return n;
    }
    return rom_ptr_to_eu(r, n >= 0xC000 ? 7 : visible_bank, n);
}

uint16_t rom_eu_to_native(const Rom *r, int bank, uint16_t a) /* EU address -> address in the loaded ROM */
{
    ASSERT(bank >= 0 && bank < 8);
    return r->us ? (uint16_t)(a + r->eu2us[bank & 7][a & 0x3FFF]) : a;
}

uint8_t rom_prg(const Rom *r, int bank, uint16_t a) {
    ASSERT(bank >= 0 && bank < 8); /* real MMC1 ignores the upper bits : mask like the hardware */
    bank &= 7;
    int o = a & 0x3FFF;
    if (r->us) {
        o += r->eu2us[bank][o];
    }
    return r->prg[bank * 0x4000 + (o & 0x3FFF)];
}

uint16_t rom_word(const Rom *r, int bank, uint16_t a) {
    uint16_t v = (uint16_t)(rom_prg(r, bank, a) | rom_prg(r, bank, (uint16_t)(a + 1)) << 8);
    return r->us ? rom_ptr_to_eu(r, bank, v) : v;
}

uint16_t rom_ptr_to_eu(const Rom *r, int bank, uint16_t v) /* address read from the US ROM -> EU address (>= $C000: bank 7, otherwise the one of the table) */
{
    if (!r->us || v < 0x8000) {
        return v;
    }
    if (v < 0xC000 && bank == 7) {
        return v; /* pointer to a switchable bank read from the fixed bank: bank unknown, banks 1 to 6 are hardly shifted (see rom_word_visible for texts and screens) */
    }
    int b = (v >= 0xC000 ? 7 : bank) & 7, o = v & 0x3FFF;
    return (uint16_t)(v + r->us2eu[b][o]);
}
