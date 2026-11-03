/* ROM access: every read of the game data goes through here (EU or US release). */
#ifndef ROM_H
#define ROM_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;
typedef struct Rom Rom;

uint8_t rom_chr(const Rom *r, int bank4k, uint16_t off);
Rom *rom_load(const char *path);                              /* NULL on failure : see rom_error() */
const char *rom_error(void);                                  /* reason of the last rom_load() failure */
void rom_allow_unknown(int allow);                            /* 1 : accept a PRG that is neither the European nor the American release */
uint8_t rom_prg(const Rom *r, int bank, uint16_t cpu_addr);   /* bank 0-6 : $8000-$BFFF, bank 7 : $C000-$FFFF */
uint16_t rom_word(const Rom *r, int bank, uint16_t cpu_addr); /* US ROM: the address read is converted back to an EU address */
uint16_t rom_ptr_to_eu(const Rom *r, int bank, uint16_t v);
int rom_is_us(const Rom *r);
uint8_t rom_prg_native(const Rom *r, int bank, uint16_t cpu_addr);
uint16_t rom_word_visible(const Rom *r, int bank, uint16_t eu_addr,
                          int visible_bank); /* pointer read from a table of `bank` ; if it points below $C000 it lives in `visible_bank` */
uint16_t rom_word_native(const Rom *r, int bank, uint16_t eu_addr);
uint16_t rom_eu_to_native(const Rom *r, int bank, uint16_t eu_addr);

#endif
