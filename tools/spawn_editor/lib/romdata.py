"""Lecture de la ROM (europeenne ou americaine) en adresses europeennes, comme le port.

Le port raisonne partout en adresses de la ROM europeenne (celles du desassemblage). Pour la ROM americaine chaque lecture est
traduite : adresse EU -> octet US grace aux blocs identiques (us_blocks.py). Un mot lu dans la ROM americaine est une adresse US :
il est ramene en adresse EU. Les fichiers produits (adresses des spawns) sont donc valables pour les deux ROM."""
import os
import zlib

from us_blocks import US_BLOCKS

US_PRG_HASH = 0xC9073E70   # FNV-1a des 128 Ko de PRG de la ROM americaine (port/src/rom.c)
EU_PRG_HASH = 0x8970409D


def fnv1a(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


def default_rom_dir(tool_root):
    """Where the ROMs are looked for: port/rom (next to the game, like its `rom` folder), else the `rom` folder one level above the port (the layout of the
    working repository). `tool_root` is the folder of the editor (port/tools/spawn_editor)."""
    port = os.path.dirname(os.path.dirname(tool_root))
    for d in (os.path.join(port, "rom"), os.path.join(os.path.dirname(port), "rom")):
        if os.path.isdir(d):
            return d
    return os.path.join(port, "rom")


class RomError(Exception):
    pass


class Rom:
    def __init__(self, path):
        with open(path, "rb") as f:
            raw = f.read()
        if raw[:4] != b"NES\x1a":
            raise RomError("%s : pas une ROM iNES" % path)
        self.path = path
        self.crc32 = "%08X" % (zlib.crc32(raw) & 0xFFFFFFFF)
        self.prg = raw[16:16 + 0x20000]
        self.chr = raw[16 + 0x20000:16 + 0x40000]
        h = fnv1a(self.prg)
        if h == EU_PRG_HASH:
            self.us, self.name = False, "EU"
        elif h == US_PRG_HASH:
            self.us, self.name = True, "US"
        else:
            raise RomError("%s : ROM inconnue (ni l'europeenne ni l'americaine)" % path)
        self.eu2us = {}
        self.us2eu = {}
        if self.us:
            for b in range(8):
                self.eu2us[b], self.us2eu[b] = self._build_maps(b)

    @staticmethod
    def _build_maps(b):
        """Memes regles que rom.c : blocs identiques, puis les zones qui different sont alignees sur leur debut."""
        n = 0x4000
        e2u, u2e = [None] * n, [None] * n
        blocks = US_BLOCKS[b]
        for e, u, ln in blocks:
            if not ln:
                break
            for k in range(ln):
                e2u[e + k] = u + k
                u2e[u + k] = e + k
        pe = pu = 0
        for e, u, ln in blocks:
            if not ln:
                e, u = n, n
            for k in range(pe, e):
                uo = pu + (k - pe)
                e2u[k] = min(uo, u - 1) if u else 0
            for k in range(pu, u):
                eo = pe + (k - pu)
                u2e[k] = min(eo, e - 1) if e else 0
            if not ln:
                break
            pe, pu = e + ln, u + ln
        return e2u, u2e

    def byte(self, addr, bank):
        """Octet a l'adresse EU `addr` ; >= $C000 : banque 7 fixe, sinon la banque donnee."""
        if addr >= 0xC000:
            bank = 7
        off = addr & 0x3FFF
        if self.us:
            off = self.eu2us[bank][off]
        return self.prg[bank * 0x4000 + off]

    def word(self, addr, bank, target=None):
        """Mot (adresse) lu a `addr` ; renvoye en adresse EU. `target` : banque visee si elle n'est pas `bank`."""
        v = self.byte(addr, bank) | self.byte(addr + 1, bank) << 8
        if self.us:
            tb = 7 if v >= 0xC000 else (bank if target is None else target)
            v = (v & 0xC000) | self.us2eu[tb][v & 0x3FFF] if v >= 0x8000 else v
        return v

    def chr_byte(self, bank4k, off):
        return self.chr[(bank4k & 31) * 0x1000 + (off & 0xFFF)]


def find_roms(rom_dir):
    """Les ROM du dossier, reconnues par leur contenu (pas par leur nom)."""
    found = {}
    if not os.path.isdir(rom_dir):
        return found
    for fn in sorted(os.listdir(rom_dir)):
        if fn.lower().endswith(".nes"):
            try:
                r = Rom(os.path.join(rom_dir, fn))
            except RomError:
                continue
            found.setdefault(r.name, r)
    return found
