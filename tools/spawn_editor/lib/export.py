"""Produit build/ (spawns.json, maps/, sprites/) a partir des ROM. Voir extract.py et render.py."""
import json
import os
import shutil
import sys

from extract import (SIDE_H, TOP_H, SIDE_ROWS, TOP_ROWS, boss_sets, kind_of, enemy_type, list_zones, read_entries, read_waves,
                     zone_tables, zone_variants)
from render import render_entity, render_map, sprite_name
from romdata import default_rom_dir, find_roms

LEVEL_NAMES = ["Niveau 1", "Niveau 2", "Niveau 3", "Niveau 4", "Niveau 5", "Niveau 6"]

# noms deja decides dans le projet (docs/ennemis_references.md, entities/names.txt) ; data/names.json les complete
SEED_NAMES = {
    0x70: "bebop (chef)", 0x11: "rocksteady (decor)", 0x49: "rock_soldier", 0x4A: "rock_soldier_crescent", 0x86: "roof_leaper",
    0x91: "chansawiac", 0x92: "rocksteady", 0x94: "april", 0x98: "purple_ninja", 0xAA: "spydrone",
}


def load_names(data_dir):
    names = dict(SEED_NAMES)
    p = os.path.join(data_dir, "names.json")
    if os.path.exists(p):
        with open(p, encoding="utf-8") as f:
            types = json.load(f).get("types", {})
        for k, v in types.items():
            names[int(k, 16)] = v
    return names


def type_name(names, t):
    return names.get(t) or "type $%02X" % t


def build_dataset(rom, names, out_dir, log=print):
    maps_dir = os.path.join(out_dir, "maps")
    spr_dir = os.path.join(out_dir, "sprites")
    os.makedirs(maps_dir, exist_ok=True)
    os.makedirs(spr_dir, exist_ok=True)
    sprite_cache = {}

    def sprite_for(z, tab, t, wave_set, flip, chr_bank=None):
        k = (z.key, t, wave_set, flip, chr_bank)
        if k not in sprite_cache:
            try:
                r = render_entity(rom, z, tab, t, wave_set, flip, chr_bank)
            except Exception as e:                       # un type exotique ne doit pas arreter l'export
                log("  sprite $%02X (jeu $%02X) : %s" % (t, wave_set, e))
                r = None
            if r is None:
                sprite_cache[k] = None
            else:
                png, ox, oy, w, h = r
                name = sprite_name(png)
                path = os.path.join(spr_dir, name)
                if not os.path.exists(path):
                    with open(path, "wb") as f:
                        f.write(png)
                sprite_cache[k] = {"img": "sprites/" + name, "ox": ox, "oy": oy, "w": w, "h": h}
        return sprite_cache[k]

    levels = [{"id": i + 1, "nom": LEVEL_NAMES[i], "zones": []} for i in range(6)]
    by_addr = {}                                         # adresse de l'entree -> cles qui la partagent
    all_entries = []
    for z in list_zones(rom):
        tab = zone_tables(rom, z)
        variants = zone_variants(rom, z, tab)
        sets = []                                        # jeux de sprites distincts, dans l'ordre des rangs
        for rank, s in variants:
            for d in sets:
                if d["jeu"] == s:
                    d["rangs"].append(rank)
                    break
            else:
                sets.append({"jeu": s, "rangs": [rank]})
        rows = SIDE_ROWS if z.side else TOP_ROWS
        screen_h = SIDE_H if z.side else TOP_H
        map_rel = "maps/%s.png" % z.key
        try:
            render_map(rom, z, tab).save(os.path.join(out_dir, map_rel))
        except Exception as e:
            log("%s : carte non dessinee (%s)" % (z.key, e))
            map_rel = None
        counters = {}
        entries = []
        for bx, by, addr, x, y, code in read_entries(rom, z, tab):
            idx = counters.get((bx, by), 0)
            counters[(bx, by)] = idx + 1
            key = "%s.%d-%d.#%d" % (z.key, bx, by, idx)
            flip = x >= 0x80
            if code < 0x10:
                use_sets = [{"jeu": tab["chr0"], "rangs": [0]}]
            else:
                use_sets = sets
            vs = []
            for s in use_sets:
                t = enemy_type(rom, code, s["jeu"]) if (code & 0xF0) != 0xF0 else code
                v = {"jeu": s["jeu"], "rangs": s["rangs"], "type": t,
                     "nom": ("objet $%02X" % code) if code < 0x10 else type_name(names, t), "sprite": None}
                if (code & 0xF0) != 0xF0:
                    v["sprite"] = sprite_for(z, tab, t, s["jeu"], flip)
                vs.append(v)
            genre = kind_of(code, [v["type"] for v in vs])
            e = {"cle": key, "adresse": "%04X" % addr, "bloc": [bx, by], "x": x, "y": y, "wx": bx * 256 + x, "wy": by * screen_h + y,
                 "code": "%02X" % code,
                 "signature": "%02X,%02X,%02X" % (x, y, code), "genre": genre, "flip": flip, "partage": [], "variantes": vs}
            by_addr.setdefault(addr, []).append(key)
            entries.append(e)
            all_entries.append(e)
        for w in read_waves(rom, z):
            marker = w["marqueur"]
            wx = (w["h"] * 256 - 5 if w["h"] else 0) + w["x"]       # camera verrouillee a la fin de l'ecran h-1 (CameraXLo $FB-$FD)
            wy = w["camy"] * screen_h + w["y"]
            code = w["code"]
            vs = []
            if marker is None:
                for s in sets:
                    t = enemy_type(rom, code, s["jeu"])
                    vs.append({"jeu": s["jeu"], "rangs": s["rangs"], "type": t, "nom": type_name(names, t),
                               "sprite": sprite_for(z, tab, t, s["jeu"], w["x"] >= 0x80)})
            elif marker == 0xF5:                                   # chef de six elements (wave_f5) : types de la table $9D84
                els = [rom.byte(0x9D86 + 6 * k, 6) for k in range(6)]
                vs.append({"jeu": rom.byte(0x9E5F + 10, 6), "rangs": [0], "type": els[0], "nom": "chef (6 elements : %s)" % ", ".join(
                    "$%02X" % v for v in dict.fromkeys(els)), "sprite": None})
            else:
                jeu, chr_bank = boss_sets(rom, marker)
                t = enemy_type(rom, code, jeu)
                vs.append({"jeu": jeu, "rangs": [0], "type": t, "nom": type_name(names, t) if names.get(t) else "chef $%02X" % t,
                           "sprite": sprite_for(z, tab, t, jeu, w["x"] >= 0x80, chr_bank)})
            key = "%s.W%d" % (z.key, w["idx"])
            e = {"cle": key, "adresse": "%04X" % w["adresse"], "bloc": [min(wx // 256, tab["w"] - 1), min(w["camy"], tab["h"] - 1)],
                 "x": w["x"], "y": w["y"], "wx": wx, "wy": wy, "code": "%02X" % code, "signature": "%02X,%02X,%02X" % (w["x"], w["y"], code),
                 "genre": "chef" if marker is not None else "vague", "flip": w["x"] >= 0x80, "partage": [], "variantes": vs,
                 "declencheur": {"numero": w["idx"], "camera_ecran_y": w["camy"], "camera_ecran_x_plus_1": w["h"],
                                 "marqueur": None if marker is None else "%02X" % marker}}
            by_addr.setdefault(w["adresse"], []).append(key)
            entries.append(e)
            all_entries.append(e)
        zone_json = {"id": z.zid, "cle": z.key, "libelle": z.label, "niveau": z.level + 1, "vue": "cote" if z.side else "dessus",
                     "ecrans": [tab["w"], tab["h"]], "ecran_w": 256, "ecran_h": screen_h, "carte": map_rel,
                     "jeux_sprites": sets, "jeu_depart": tab["chr0"], "entrees": entries}
        levels[z.level]["zones"].append(zone_json)
        log("%-9s %dx%d ecrans, %3d entrees, %d variante(s)" % (z.key, tab["w"], tab["h"], len(entries), len(sets)))
    for e in all_entries:
        a = int(e["adresse"], 16)
        e["partage"] = [k for k in by_addr[a] if k != e["cle"]]
    return {"rom": {"nom": rom.name, "crc32": rom.crc32}, "niveaux": levels, "nb_entrees": len(all_entries)}


def export(rom_dir, data_dir, out_dir, log=print):
    roms = find_roms(rom_dir)
    if not roms:
        raise SystemExit("Aucune ROM reconnue dans %s (attendu : la ROM europeenne et/ou americaine, .nes)" % rom_dir)
    primary = roms.get("EU") or next(iter(roms.values()))
    names = load_names(data_dir)
    os.makedirs(out_dir, exist_ok=True)
    for name in os.listdir(out_dir):                     # on vide le contenu sans supprimer le dossier (Windows : dossier courant d'un shell)
        p = os.path.join(out_dir, name)
        shutil.rmtree(p, ignore_errors=True) if os.path.isdir(p) else os.remove(p)
    log("ROM de reference : %s (%s, CRC32 %s)" % (os.path.basename(primary.path), primary.name, primary.crc32))
    ds = build_dataset(primary, names, out_dir, log)
    ds["roms"] = {n: {"fichier": os.path.basename(r.path), "crc32": r.crc32} for n, r in roms.items()}
    with open(os.path.join(out_dir, "spawns.json"), "w", encoding="utf-8") as f:
        json.dump(ds, f, ensure_ascii=False, indent=1)
    # controle : l'autre ROM doit donner exactement les memes entrees (listes en adresses EU, communes aux deux)
    for n, r in roms.items():
        if r is primary:
            continue
        other = build_dataset(r, names, os.path.join(out_dir, "_" + n), lambda *_: None)
        a = [(e["cle"], e["adresse"], e["signature"]) for lv in ds["niveaux"] for z in lv["zones"] for e in z["entrees"]]
        b = [(e["cle"], e["adresse"], e["signature"]) for lv in other["niveaux"] for z in lv["zones"] for e in z["entrees"]]
        shutil.rmtree(os.path.join(out_dir, "_" + n), ignore_errors=True)
        log("controle %s contre %s : %s (%d entrees)" % (n, primary.name, "IDENTIQUE" if a == b else "DIFFERENT", len(b)))
        ds["controle_" + n.lower()] = "identique" if a == b else "different"
    return ds


if __name__ == "__main__":
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    export(default_rom_dir(root), os.path.join(root, "data"), os.path.join(root, "build"))
