"""Tests de l'extracteur et du masque. Usage : python -m unittest discover -s tests   (depuis spawn_editor/)
Ils lisent les ROM de port/rom (ou du dossier rom au-dessus du port) ; ceux qui ont besoin d'une ROM absente sont ignores."""
import os
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "lib"))

import maskio                       # noqa: E402
from export import build_dataset, load_names   # noqa: E402
from romdata import default_rom_dir, find_roms       # noqa: E402

ROMS = find_roms(default_rom_dir(ROOT))
NAMES = load_names(os.path.join(ROOT, "data"))
_cache = {}


def dataset(name):
    if name not in _cache:
        with tempfile.TemporaryDirectory() as d:
            _cache[name] = build_dataset(ROMS[name], NAMES, d, lambda *_: None)
    return _cache[name]


def entries(ds):
    return [e for lv in ds["niveaux"] for z in lv["zones"] for e in z["entrees"]]


@unittest.skipUnless("EU" in ROMS, "ROM europeenne absente")
class Extraction(unittest.TestCase):
    def test_niveau1_161_apparitions(self):
        # docs/plan_niveau1.md : 161 apparitions d'ennemis au niveau 1 (rue 12, batiments 21 / 24 / 37 / 32 / 16 / 19)
        lv1 = [e for z in dataset("EU")["niveaux"][0]["zones"] for e in z["entrees"] if e["genre"] == "ennemi"]
        self.assertEqual(len(lv1), 161)

    def test_chefs_listes(self):
        chefs = [e for e in entries(dataset("EU")) if e["genre"] == "chef"]
        self.assertEqual(len(chefs), 6)
        self.assertTrue(any(v["type"] == 0x70 for c in chefs for v in c["variantes"]))   # bebop

    def test_adresses_uniques_par_cle(self):
        keys = [e["cle"] for e in entries(dataset("EU"))]
        self.assertEqual(len(keys), len(set(keys)))

    def test_variantes_alignees_sur_les_jeux_de_la_zone(self):
        for lv in dataset("EU")["niveaux"]:
            for z in lv["zones"]:
                for e in z["entrees"]:
                    if e["genre"] == "ennemi":
                        self.assertEqual([v["jeu"] for v in e["variantes"]], [s["jeu"] for s in z["jeux_sprites"]])

    def test_signature_dans_la_rom(self):
        rom = ROMS["EU"]
        for e in entries(dataset("EU")):
            a = int(e["adresse"], 16)
            self.assertEqual("%02X,%02X,%02X" % (rom.byte(a, 6), rom.byte(a + 1, 6), rom.byte(a + 2, 6)), e["signature"], e["cle"])


@unittest.skipUnless("EU" in ROMS and "US" in ROMS, "il faut les deux ROM")
class EuContreUs(unittest.TestCase):
    def test_memes_entrees(self):
        sig = lambda ds: [(e["cle"], e["adresse"], e["signature"]) for e in entries(ds)]
        self.assertEqual(sig(dataset("EU")), sig(dataset("US")))


@unittest.skipUnless("EU" in ROMS, "ROM europeenne absente")
class Masque(unittest.TestCase):
    def setUp(self):
        self.ds = dataset("EU")
        self.enn = next(e for e in entries(self.ds) if e["genre"] == "ennemi" and len(e["variantes"]) > 1)
        self.chef = next(e for e in entries(self.ds) if e["genre"] == "chef")

    def item(self, e, jeux):
        return {"cle": e["cle"], "adresse": e["adresse"], "signature": e["signature"], "jeux_desactives": jeux}

    def test_variante_seule(self):
        jeu = "%02X" % self.enn["variantes"][0]["jeu"]
        clean, warns = maskio.validate({"desactives": [self.item(self.enn, [jeu])]}, self.ds)
        self.assertEqual(warns, [])
        self.assertEqual(clean["desactives"][0]["jeux_desactives"], [jeu])
        x, y, c = self.enn["signature"].split(",")
        lines = [l for l in maskio.game_text(clean).splitlines() if not l.startswith("#")]
        self.assertEqual(lines, ["B6 %s  %s %s %s  %s" % (self.enn["adresse"], x, y, c, jeu)])

    def test_toutes_les_variantes_deviennent_etoile(self):
        jeux = sorted({"%02X" % v["jeu"] for v in self.enn["variantes"]})
        clean, _ = maskio.validate({"desactives": [self.item(self.enn, jeux)]}, self.ds)
        self.assertEqual(clean["desactives"][0]["jeux_desactives"], ["*"])

    def test_chef_refuse(self):
        clean, warns = maskio.validate({"desactives": [self.item(self.chef, ["*"])]}, self.ds)
        self.assertEqual(clean["desactives"], [])
        self.assertEqual(len(warns), 1)

    def test_mauvaise_signature_ou_adresse(self):
        bad = self.item(self.enn, ["*"])
        bad["signature"] = "00,00,00"
        clean, warns = maskio.validate({"desactives": [bad, {"adresse": "0000", "cle": "x", "jeux_desactives": ["*"]}]}, self.ds)
        self.assertEqual(clean["desactives"], [])
        self.assertEqual(len(warns), 2)

    def test_texte_du_jeu(self):
        clean, _ = maskio.validate({"desactives": [self.item(self.enn, ["*"])]}, self.ds)
        lines = [l for l in maskio.game_text(clean).splitlines() if not l.startswith("#")]
        x, y, c = self.enn["signature"].split(",")
        self.assertEqual(lines, ["B6 %s  %s %s %s  *" % (self.enn["adresse"], x, y, c)])


if __name__ == "__main__":
    unittest.main()
