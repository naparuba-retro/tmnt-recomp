#!/usr/bin/env python3
"""Editeur de spawns d'ennemis de TMNT (NES).

    python run.py            exporte les donnees depuis les ROM si besoin, lance l'editeur et ouvre le navigateur
    python run.py export     regenere build/ (spawns.json, cartes, sprites) puis s'arrete
    python run.py serve      lance l'editeur sans regenerer

Les ROM sont lues dans port/rom, sinon dans le dossier rom au-dessus du port (ou --rom-dir) : la ROM europeenne et/ou l'americaine, reconnues par leur contenu."""
import argparse
import json
import mimetypes
import os
import sys
import threading
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import unquote, urlparse

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ROOT, "lib"))

import maskio                      # noqa: E402
from export import export         # noqa: E402
from romdata import default_rom_dir   # noqa: E402

DATA = os.path.join(ROOT, "data")
BUILD = os.path.join(ROOT, "build")
WEB = os.path.join(ROOT, "web")
# the mask is the port's own data: the game reads port/data/spawn_mask.txt (next to its executable, `--spawn-mask` or key S), written at each save
PORT_DATA = os.path.join(os.path.dirname(os.path.dirname(ROOT)), "data")
MASK_JSON = os.path.join(PORT_DATA, "spawn_mask.json")
MASK_TXT = os.path.join(PORT_DATA, "spawn_mask.txt")
MAX_BODY = 2 * 1024 * 1024


def newest(paths):
    t = 0
    for p in paths:
        if os.path.isdir(p):
            for dp, _, fns in os.walk(p):
                for fn in fns:
                    t = max(t, os.path.getmtime(os.path.join(dp, fn)))
        elif os.path.exists(p):
            t = max(t, os.path.getmtime(p))
    return t


def needs_export(rom_dir):
    out = os.path.join(BUILD, "spawns.json")
    if not os.path.exists(out):
        return True
    sources = [os.path.join(ROOT, "lib"), os.path.join(DATA, "names.json")]
    if os.path.isdir(rom_dir):
        sources += [os.path.join(rom_dir, f) for f in os.listdir(rom_dir) if f.lower().endswith(".nes")]
    return newest(sources) > os.path.getmtime(out)


_ds = {"mtime": 0, "data": None}


def current_dataset():
    """spawns.json, relu si `python run.py export` l'a regenere pendant que l'editeur tourne."""
    p = os.path.join(BUILD, "spawns.json")
    m = os.path.getmtime(p)
    if _ds["data"] is None or m != _ds["mtime"]:
        _ds["data"], _ds["mtime"] = json.load(open(p, encoding="utf-8")), m
    return _ds["data"]


class Handler(BaseHTTPRequestHandler):
    server_version = "SpawnEditor/1"

    def log_message(self, fmt, *args):
        pass

    # --- utilitaires
    def send_bytes(self, code, body, ctype, extra=None):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        for k, v in (extra or {}).items():
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def send_json(self, obj, code=200):
        self.send_bytes(code, json.dumps(obj, ensure_ascii=False).encode("utf-8"), "application/json; charset=utf-8")

    def host_ok(self):
        """Le serveur n'ecoute que sur 127.0.0.1 ; on refuse aussi les requetes dont l'hote ou l'origine ne sont pas locaux."""
        host = (self.headers.get("Host") or "").split(":")[0]
        if host not in ("127.0.0.1", "localhost"):
            return False
        origin = self.headers.get("Origin")
        if origin and urlparse(origin).hostname not in ("127.0.0.1", "localhost"):
            return False
        return True

    def serve_file(self, base, rel):
        path = os.path.normpath(os.path.join(base, rel))
        if not path.startswith(base + os.sep) or not os.path.isfile(path):
            return self.send_bytes(404, b"introuvable", "text/plain; charset=utf-8")
        ctype = mimetypes.guess_type(path)[0] or "application/octet-stream"
        if ctype.startswith("text/") or ctype in ("application/javascript", "application/json"):
            ctype += "; charset=utf-8"
        self.send_bytes(200, open(path, "rb").read(), ctype)

    # --- routes
    def do_GET(self):
        if not self.host_ok():
            return self.send_bytes(403, b"interdit", "text/plain; charset=utf-8")
        path = unquote(urlparse(self.path).path)
        if path == "/":
            return self.serve_file(WEB, "index.html")
        if path == "/api/data":
            return self.serve_file(BUILD, "spawns.json")
        if path == "/api/mask":
            return self.send_json(maskio.load_mask(MASK_JSON))
        if path.startswith("/web/"):
            return self.serve_file(WEB, path[5:])
        if path.startswith("/build/"):
            return self.serve_file(BUILD, path[7:])
        return self.send_bytes(404, b"introuvable", "text/plain; charset=utf-8")

    def do_POST(self):
        if not self.host_ok():
            return self.send_bytes(403, b"interdit", "text/plain; charset=utf-8")
        if urlparse(self.path).path != "/api/mask":
            return self.send_bytes(404, b"introuvable", "text/plain; charset=utf-8")
        n = int(self.headers.get("Content-Length") or 0)
        if n <= 0 or n > MAX_BODY:
            return self.send_json({"erreur": "corps invalide"}, 400)
        try:
            mask = json.loads(self.rfile.read(n).decode("utf-8"))
            if not isinstance(mask, dict):
                raise ValueError("objet attendu")
        except (ValueError, UnicodeDecodeError) as e:
            return self.send_json({"erreur": "JSON invalide : %s" % e}, 400)
        clean, warns = maskio.validate(mask, current_dataset())
        maskio.save_mask(clean, MASK_JSON, MASK_TXT)
        self.send_json({"masque": clean, "avertissements": warns, "fichier_jeu": os.path.relpath(MASK_TXT, ROOT)})


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("commande", nargs="?", default="run", choices=["run", "export", "serve"])
    ap.add_argument("--rom-dir", default=default_rom_dir(ROOT))
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--no-browser", action="store_true")
    a = ap.parse_args()
    if a.commande in ("run", "export") and (a.commande == "export" or needs_export(a.rom_dir)):
        print("Export depuis les ROM de %s ..." % a.rom_dir)
        export(a.rom_dir, DATA, BUILD)
    if a.commande == "export":
        return
    if not os.path.exists(os.path.join(BUILD, "spawns.json")):
        raise SystemExit("build/spawns.json manque : lancer `python run.py export`")
    current_dataset()
    srv = ThreadingHTTPServer(("127.0.0.1", a.port), Handler)
    url = "http://127.0.0.1:%d/" % a.port
    print("Editeur de spawns : %s   (Ctrl+C pour arreter)" % url)
    print("Masque : %s  ->  fichier du jeu : %s" % (os.path.relpath(MASK_JSON, ROOT), os.path.relpath(MASK_TXT, ROOT)))
    if not a.no_browser:
        threading.Timer(0.5, lambda: webbrowser.open(url)).start()
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
