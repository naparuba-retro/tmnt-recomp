"""Dessin des cartes de zone et des sprites d'ennemis, lus dans la ROM (meme logique que le port : nmi.c draw_metasprite, anim.c)."""
import hashlib
import io

from PIL import Image

from extract import B6, B7, SIDE_ROWS, TOP_ROWS

# palette de Mesen (RVB), reprise de tools/level_data.py
PAL = [0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00, 0x333500, 0x0B4800, 0x005200, 0x004F08,
       0x00404D, 0, 0, 0, 0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00, 0x6B6D00, 0x388700,
       0x0D9300, 0x008F32, 0x007C8D, 0, 0, 0, 0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22,
       0xBCBE00, 0x88D800, 0x5CE430, 0x45E082, 0x48CDDE, 0x4F4F4F, 0, 0, 0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF,
       0xFEC4EA, 0xFECCC5, 0xF7D8A5, 0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0, 0]
BACKDROP = 0x0F


def rgb(c):
    v = PAL[c & 0x3F]
    return (v >> 16, (v >> 8) & 255, v & 255)


def run_palette_script(rom, p, x, y, buf):
    """B7 $F75F : copie un script de palette dans buf ; les entrees multiples de 4 prennent la couleur de fond sans consommer d'octet."""
    while x < 32:
        v = rom.byte(p + y, B7)
        if v == 0xFE:
            break
        if (x & 3) == 0:
            v = BACKDROP
        else:
            y += 1
        buf[x] = v
        x += 1


def zone_palette(rom, z, tab, wave_set=None):
    """32 couleurs : 0-15 fond, 16-31 sprites. `wave_set` : jeu de sprites actif (wave_palette, vue de cote seulement)."""
    buf = [BACKDROP] * 32
    run_palette_script(rom, rom.word(0xF7B5 + tab["pal"] * 2, B7), 0, 0, buf)
    if wave_set is not None:
        preset = rom.byte(0xF78B + wave_set, B7)
        if not preset & 0x80:
            p = rom.word(0xFA55 + preset * 2, B7)
            run_palette_script(rom, p, rom.byte(p, B7), 1, buf)
    return buf


# ----------------------------------------------------------------------------------------------------- cartes
def render_map(rom, z, tab):
    """Image de la carte entiere : blocs de 8 x rows cellules de 32 px (6 lignes en vue de cote, 8 en vue du dessus)."""
    side = z.side
    cb = 3 if side else 4
    rows = SIDE_ROWS if side else TOP_ROWS
    bs = 48 if side else 64
    w, h = tab["w"], tab["h"]
    pal = zone_palette(rom, z, tab, tab["chr0"] if side else None)
    img = Image.new("RGB", (w * 256, h * rows * 32))
    cache = {}

    def tile_img(tile, sub):
        k = (tile, sub)
        im = cache.get(k)
        if im is None:
            base = tab["chr1"] * 0x1000 + tile * 16
            cols = [rgb(pal[sub * 4 + v]) for v in range(4)]
            im = Image.new("RGB", (8, 8))
            px = im.load()
            for r in range(8):
                lo, hi = rom.chr[base + r], rom.chr[base + r + 8]
                for c in range(8):
                    px[c, r] = cols[((lo >> (7 - c)) & 1) | (((hi >> (7 - c)) & 1) << 1)]
            cache[k] = im
        return im

    for by in range(h):
        for bx in range(w):
            blk = rom.byte(tab["map"] + 2 + by * w + bx, 1)
            for cy in range(rows):
                for cx in range(8):
                    cell = rom.byte(tab["blk"] + blk * bs + cy * 8 + cx, 1)
                    attr = rom.byte(tab["attr"] + cell, cb)
                    for ty in range(4):
                        for tx in range(4):
                            tile = rom.byte(tab["cell"] + cell * 16 + ty * 4 + tx, cb)
                            sub = (attr >> (2 * ((ty // 2) * 2 + (tx // 2)))) & 3
                            img.paste(tile_img(tile, sub), (bx * 256 + cx * 32 + tx * 8, (by * rows + cy) * 32 + ty * 8))
    return img


# ---------------------------------------------------------------------------------------------------- sprites
def signed(v):
    return v - 256 if v >= 128 else v


def first_sprite(rom, t):
    """Premier sprite de l'animation de depart du type t (init_entity_type_tail + animate_entity), ou None."""
    if t & 0x80:
        anim = rom.byte(0x883E + ((t & 0x7F) << 1), B6)
        base = 0x8E11 if anim & 0x80 else 0x8DC5
    else:
        anim = rom.byte(0x874A + (t << 1), B6)
        base = 0x8BC5 if anim & 0x80 else 0x8CC5
    for _ in range(8):                                   # $FD : saut vers un autre script
        p = rom.word(base + ((anim << 1) & 0xFF), B6)
        v = rom.byte(p, B6)
        if v == 0xFD:
            anim = rom.byte(p + 1, B6)
            base = {(0, 1): 0x8BC5, (0, 0): 0x8CC5, (1, 1): 0x8E11, (1, 0): 0x8DC5}[(1 if t & 0x80 else 0, 1 if anim & 0x80 else 0)]
            continue
        if v in (0xFE, 0xFF):
            return None
        return v
    return None


def metasprite(rom, sprite, wave_set):
    """Liste (yoff, tile, attr, xoff) du metasprite (nmi.c metasprite_ptr / draw_metasprite)."""
    y = (sprite << 1) & 0xFF
    if not sprite & 0x80:
        p = rom.word(0x8001 + y, 2)
    else:
        tbl = rom.word(0xC22C + wave_set * 2, B7, target=2)
        p = rom.word(tbl + y, 2, target=2)
    count = rom.byte(p, 2)
    i, out, attr = 1, [], 0
    for _ in range(min(count, 40)):
        a = rom.byte(p + i, 2)
        i += 1
        same_attr = a & 1
        yoff = signed(a) >> 1
        tile = rom.byte(p + i, 2)
        i += 1
        if not same_attr:
            attr = rom.byte(p + i, 2)
            i += 1
        off = rom.byte(p + i, 2)
        i += 1
        out.append((yoff, tile, attr, off))
    return out


def render_entity(rom, z, tab, t, wave_set, flip, chr_bank=None):
    """Sprite d'un ennemi de type t pour un jeu de sprites donne. Renvoie (png, ox, oy, w, h) : (ox, oy) = coin haut-gauche relatif
    au point (EntX, EntY) de l'entite ; None si le type n'a pas de sprite."""
    sp = first_sprite(rom, t)
    if sp is None:
        return None
    if chr_bank is None:
        chr_bank = wave_set
    parts = metasprite(rom, sp, chr_bank)
    if not parts:
        return None
    side = z.side
    pal = zone_palette(rom, z, tab, wave_set if side else None)
    chr0 = chr_bank & 31
    chr1 = tab["chr1"]
    placed = []
    for yoff, tile, attr, off in parts:
        if flip:
            off = (0xF8 - off) & 0xFF
            attr ^= 0x40
        placed.append((signed(off), yoff, tile, attr))
    x0 = min(p[0] for p in placed)
    y0 = min(p[1] for p in placed)
    x1 = max(p[0] for p in placed) + 8
    y1 = max(p[1] for p in placed) + 16
    img = Image.new("RGBA", (x1 - x0, y1 - y0), (0, 0, 0, 0))
    px = img.load()
    for sx, sy, tile, attr in placed:
        bank = chr1 if tile & 1 else chr0
        for row in range(16):
            r = 15 - row if attr & 0x80 else row
            a = (tile & 0xFE) * 16 + (r & 7) + (16 if r >= 8 else 0)
            lo, hi = rom.chr_byte(bank, a), rom.chr_byte(bank, a + 8)
            for i in range(8):
                bit = i if attr & 0x40 else 7 - i
                c = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1)
                if c:
                    px[sx - x0 + i, sy - y0 + row] = rgb(pal[16 + (attr & 3) * 4 + c]) + (255,)
    buf = io.BytesIO()
    img.save(buf, "PNG")
    return buf.getvalue(), x0, y0, img.width, img.height


def sprite_name(png):
    return "s_" + hashlib.md5(png).hexdigest()[:12] + ".png"
