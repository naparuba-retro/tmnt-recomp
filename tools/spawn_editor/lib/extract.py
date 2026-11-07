"""Extraction statique des spawns d'ennemis depuis la ROM (aucune execution du jeu).

Chemin des donnees (voir docs/conception_editeur_spawns.md et tools/level_data.py) :
  - vue du dessus : table `$AC8D` (bank 6) -> table de la zone -> pointeur par bloc (ecran de 256 px) -> entrees x, y, code, fin `$FF`
  - vue de cote   : table du niveau `$8681` (bank 6), meme suite
  - code < $10 : objet (pizza...) ; code >= $10 : ennemi, type = table[code & 7], la table dependant du jeu de sprites
    `WaveSpriteSet` : `$86E9 + jeu*2` (bank 6). En vue de cote le jeu de sprites change selon l'avancement (variantes) : `$82EB`.
Les adresses sont des adresses EU de la bank 6 (identiques pour la ROM americaine)."""
from collections import OrderedDict

B6, B7 = 6, 7
SIDE_ROWS, TOP_ROWS = 6, 8           # lignes de 32 px par bloc
SIDE_H, TOP_H = 0xC0, 0x100          # hauteur d'un ecran en pixels


class ZoneInfo:
    def __init__(self, level, zid, side, area, label):
        self.level, self.zid, self.side, self.area, self.label = level, zid, side, area, label

    @property
    def key(self):
        return "L%d.%s" % (self.level + 1, self.zid)


def side_zone_counts(rom):
    """Nombre de zones en vue de cote par niveau (differences de la table des palettes `$D658`)."""
    base = [rom.word(0xD658 + 2 * i, B7) for i in range(6)]
    base.append(0xD692)          # fin de la derniere liste : la table des palettes des tortues (`set_turtle_sprite_palette`) commence la
    return [base[i + 1] - base[i] for i in range(6)]


def list_zones(rom):
    counts = side_zone_counts(rom)
    zones = []
    for level in range(6):
        if level < 5:                      # le niveau 6 n'a pas de rue : la table `$D653` n'a que 5 octets
            zones.append(ZoneInfo(level, "rue", False, level, "Rue (vue du dessus)"))
        for a in range(counts[level]):
            zones.append(ZoneInfo(level, "bat%d" % a, True, a, "Batiment %d (vue de cote)" % a))
    return zones


def zone_tables(rom, z):
    """Ce que le jeu prepare a l'entree de la zone (stage.c : prepare_overhead_area / prepare_sideview_area)."""
    t = OrderedDict()
    if z.side:
        lvl, area = z.level, z.area
        a = lvl
        if lvl == 4:
            a = lvl + 2 + area
        elif lvl == 3:
            a = 0 if area == 0 else (0x0C if area == 0x11 else lvl)
        t["cell"] = rom.word(0x8015 + a * 2, 1, target=3)
        t["attr"] = rom.word(0x8033 + a * 2, 1, target=3)
        t["blk"] = rom.word(0x805D + lvl * 2, 1)
        tbl = rom.word(0xF4BD + lvl * 2, B7)
        t["map"] = rom.word(tbl + area * 2, B7, target=1)
        p = rom.word(0xD658 + lvl * 2, B7)
        t["pal"] = rom.byte(p + area, B7)
        c = rom.word(0xD5EB + lvl * 2, B7)
        t["chr1"] = rom.byte(c + area * 2, B7)
        t["chr0"] = rom.byte(c + area * 2 + 1, B7)    # jeu de sprites de depart
    else:
        area = z.area
        t["cell"] = rom.word(0x8001 + area * 2, 1, target=4)
        t["attr"] = rom.word(0x800B + area * 2, 1, target=4)
        t["blk"] = rom.word(0x8051 + area * 2, 1)
        t["map"] = rom.word(0xF4C9 + area * 2, B7, target=1)
        t["pal"] = rom.byte(0xD653 + area, B7)
        t["chr1"] = rom.byte(0xD5E5 + area, B7)
        t["chr0"] = 0x10
    t["w"] = rom.byte(t["map"], 1)
    t["h"] = rom.byte(t["map"] + 1, 1)
    return t


def zone_variants(rom, z, tab):
    """Jeux de sprites que la zone peut utiliser : [(rang, jeu)] ; la vue du dessus n'en a qu'un ($10)."""
    if not z.side:
        return [(0, 0x10)]
    t = rom.word(0x82EB + z.level * 2, B6)
    p = rom.word(t + z.area * 2, B6)
    return [(k, rom.byte(p + k, B6)) for k in range(4)]


def enemy_type(rom, code, wave_set):
    if code < 0x10:
        return code
    tbl = rom.word(0x86E9 + wave_set * 2, B6)
    return rom.byte(tbl + (code & 7), B6)


def read_entries(rom, z, tab):
    """Toutes les entrees des listes de la zone : (bx, by, adresse, x, y, code)."""
    if z.side:
        p = rom.word(0x8681 + z.level * 2, B6)
    else:
        p = rom.word(0xAC8D, B6)
    col_table = rom.word(p + z.area * 2, B6)
    out = []
    for blk in range(tab["w"] * tab["h"]):
        lp = rom.word(col_table + blk * 2, B6)
        i = 0
        while rom.byte(lp + i, B6) != 0xFF and i < 300:
            out.append((blk % tab["w"], blk // tab["w"], lp + i,
                        rom.byte(lp + i, B6), rom.byte(lp + i + 1, B6), rom.byte(lp + i + 2, B6)))
            i += 3
    return out


def kind_of(code, types):
    """ennemi | objet | chef | marqueur"""
    if code < 0x10:
        return "objet"
    if (code & 0xF0) == 0xF0:
        return "marqueur"
    if any(0x70 <= t < 0x80 for t in types):
        return "chef"
    return "ennemi"


def read_waves(rom, z):
    """Vagues et chefs de la zone (vue de cote) : table `$9E6D` (bank 6) -> table du niveau -> liste de la zone.
    Un enregistrement = camera (ecran Y, X+1), puis l'ennemi (x, y, code) en positions absolues a l'ecran ; un octet `$F0`-`$F6`
    qui le suit le transforme en chef (side_wave_triggers / wave_trigger, waves.c). L'adresse du jeu est celle de (x, y, code)."""
    if not z.side:
        return []
    t = rom.word(0x9E6D + z.level * 2, B6)
    p = rom.word(t + z.area * 2, B6)
    out, y, idx = [], 0, 0
    while y < 200:
        if rom.byte(p + y, B6) == 0xFF:
            break
        rec = [rom.byte(p + y + k, B6) for k in range(5)]
        mk = rom.byte(p + y + 5, B6)
        marker = mk if (mk & 0xF0) == 0xF0 and mk != 0xFF else None
        out.append({"idx": idx, "adresse": p + y + 2, "camy": rec[0], "h": rec[1], "x": rec[2], "y": rec[3], "code": rec[4],
                    "marqueur": marker})
        y += 6 if marker is not None else 5
        idx += 1
    return out


def boss_sets(rom, marker):
    """(jeu de sprites pour la table des types, banque CHR) d'un chef : table `$9E5F` (wave_trigger)."""
    i = (marker & 7) << 1
    return rom.byte(0x9E5F + i, B6), rom.byte(0x9E60 + i, B6)
