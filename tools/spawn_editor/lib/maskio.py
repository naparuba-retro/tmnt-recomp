"""Fichier de masque : lecture, controle, ecriture (JSON pour l'editeur, texte pour le jeu).

data/spawn_mask.json  : l'etat voulu, seul fichier edite (un seul fichier global pour tout le jeu).
data/spawn_mask.txt   : genere a chaque sauvegarde, lu par le port (--spawn-mask) :
    # version 1
    B6 ADE0  21 40 89  07,01        adresse bank 6 (EU), signature x y code, jeux de sprites coupes (hex) ou * = toutes les variantes
Les adresses sont des adresses EU de la bank 6 : le meme fichier sert pour la ROM europeenne et la ROM americaine."""
import json
import os

VERSION = 1


def empty_mask():
    return {"version": VERSION, "desactives": []}


def index_dataset(ds):
    """adresse (4 hex) -> {entrees: [entree...], jeux: [jeux de sprites possibles], genre}."""
    idx = {}
    for lv in ds["niveaux"]:
        for z in lv["zones"]:
            for e in z["entrees"]:
                d = idx.setdefault(e["adresse"], {"entrees": [], "jeux": set(), "genre": e["genre"], "signature": e["signature"]})
                d["entrees"].append(e)
                d["jeux"].update(v["jeu"] for v in e["variantes"])
    return idx


def validate(mask, ds):
    """Renvoie (masque nettoye, liste d'avertissements). Un chef ou un marqueur n'est jamais desactivable."""
    idx = index_dataset(ds)
    out, warns, seen = [], [], set()
    for m in mask.get("desactives", []):
        addr = str(m.get("adresse", "")).upper()
        cle = m.get("cle", addr)
        if addr not in idx:
            warns.append("%s : adresse inconnue, ignoree" % cle)
            continue
        info = idx[addr]
        if m.get("signature") and m["signature"].upper() != info["signature"]:
            warns.append("%s : signature differente (%s dans le masque, %s dans la ROM), ignoree" % (cle, m["signature"], info["signature"]))
            continue
        if info["genre"] in ("chef", "marqueur"):
            warns.append("%s : un chef ou un declencheur ne peut pas etre desactive, ignore" % cle)
            continue
        if addr in seen:
            continue
        seen.add(addr)
        jeux = m.get("jeux_desactives") or ["*"]
        if "*" in jeux or set(int(j, 16) for j in jeux) >= info["jeux"]:
            jeux = ["*"]
        else:
            jeux = sorted(("%02X" % int(j, 16) for j in jeux))
        e0 = info["entrees"][0]
        item = {"cle": e0["cle"], "adresse": addr, "signature": info["signature"], "jeux_desactives": jeux}
        if len(info["entrees"]) > 1:
            item["partage"] = [e["cle"] for e in info["entrees"][1:]]
        if m.get("note"):
            item["note"] = str(m["note"])[:200]
        out.append(item)
    out.sort(key=lambda m: m["adresse"])
    return {"version": VERSION, "desactives": out}, warns


def load_mask(path):
    if not os.path.exists(path):
        return empty_mask()
    return json.load(open(path, encoding="utf-8"))


def game_text(mask):
    lines = ["# version %d" % VERSION,
             "# fichier genere par spawn_editor : ne pas editer a la main (voir spawn_mask.json)",
             "# B6 <adresse EU>  <x> <y> <code>  <jeux de sprites coupes, hex, ou *>"]
    for m in mask["desactives"]:
        sig = " ".join(m["signature"].split(","))
        lines.append("B6 %s  %s  %s" % (m["adresse"], sig, ",".join(m["jeux_desactives"])))
    return "\n".join(lines) + "\n"


def save_mask(mask, json_path, txt_path):
    os.makedirs(os.path.dirname(json_path), exist_ok=True)
    tmp = json_path + ".tmp"
    json.dump(mask, open(tmp, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    os.replace(tmp, json_path)
    tmp = txt_path + ".tmp"
    open(tmp, "w", encoding="utf-8", newline="\n").write(game_text(mask))
    os.replace(tmp, txt_path)
