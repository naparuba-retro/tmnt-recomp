# Editeur de spawns d'ennemis (TMNT NES)

Outil a part du port : il lit les ROM fournies (`port/rom`, ou le dossier `rom` au-dessus du port : elles ne sont jamais dans le depot), montre chaque niveau, chaque zone et chaque ecran avec les ennemis qui y apparaissent
(sprite exact tire de la ROM), et permet de **couper / remettre** un spawn, variante par variante. L'etat est ecrit dans un fichier que le port pourra
lire pour sauter ces spawns. Conception : `docs/conception_editeur_spawns.md` du depot de travail (pas dans ce depot).

Aucune dependance autre que Python 3 et Pillow (deja utilise par `tools/level_data.py`).

## Lancer

```
python run.py                 # exporte depuis les ROM si besoin, lance l'editeur, ouvre http://127.0.0.1:8765/
python run.py export          # regenere build/ puis s'arrete
python run.py --rom-dir DIR   # autre dossier de ROM
python -m unittest discover -s tests
```

Les ROM (europeenne et/ou americaine, `.nes`) sont reconnues par leur contenu, pas par leur nom. Les adresses de spawns sont des adresses de la bank 6 en
adresses europeennes, communes aux deux ROM : le test `EuContreUs` verifie que les deux donnent exactement les memes entrees.

## Ce que l'editeur montre

- **Niveaux 1 a 6**, leurs zones (rue en vue du dessus, batiments en vue de cote) et **chaque ecran** (bouton « Ecran par ecran »).
- Chaque spawn avec son sprite, son orientation par defaut, sa position (x, y), son code et son adresse ROM. Les listes partagees entre plusieurs ecrans sont signalees.
- **Variantes** : en vue de cote le jeu change de jeu de sprites selon l'avancement (score, temps) : un meme point peut donner des ennemis differents.
  Chaque variante est affichee a part (fleches, defilement automatique, ou « cote a cote ») et se coupe a part (case « Actif » de la variante, ou Espace).
- **Vagues** (combats a camera verrouillee) : listees comme spawns `vague`, desactivables.
- **Chefs** : listes avec leur sprite mais **non desactivables** (couper un chef sauterait le combat et la scene suivante).
- Filtre par type, par spawns coupes, etiquettes, zoom, liste detaillee, note par spawn.

Raccourcis : fleches = variante, Espace = couper/remettre la variante affichee du spawn choisi, double-clic sur un sprite = idem, Echap = deselectionner.

## Fichiers

| Fichier | Role |
|---|---|
| `../../data/spawn_mask.json` | **l'etat voulu**, seul fichier edite (un seul fichier global), dans les donnees du port. |
| `../../data/spawn_mask.txt` | genere a chaque sauvegarde : c'est le fichier que le **jeu** lit (touche S, `--spawn-mask`), livre avec le port. Une ligne par spawn coupe : `B6 <adresse>  <x> <y> <code>  <jeux de sprites coupes ou *>` |
| `data/names.json` | noms des types d'ennemis (complete ceux du projet) |
| `build/` | genere (cartes PNG, sprites, `spawns.json`) ; ignore par git |
| `lib/` | `romdata.py` (lecture ROM EU/US), `extract.py` (listes de spawns), `render.py` (cartes et sprites), `export.py`, `maskio.py` |
| `web/` | la page (HTML, CSS, JS sans bibliotheque) |

Une variante est identifiee par son **jeu de sprites** (`WaveSpriteSet`) : c'est la valeur que le jeu a sous la main au moment du spawn. `*` coupe toutes les variantes.
Le jeu peut verifier la **signature** (x, y, code) lue a l'adresse : si elle ne correspond pas, la ligne est ignoree.

## Etat

Fait : extraction, editeur, fichiers de masque, tests, et la lecture du masque par le jeu (`src/spawn_mask.c`, voir `FIXES.md` PIZZA-005). `python -m unittest discover -s tests` : les
tests qui ont besoin d'une ROM absente sont ignores.

## Attention aux images

`build/` (cartes des niveaux, sprites, `spawns.json`) est **genere depuis votre ROM** et contient des graphismes du jeu : il est ignore par git et ne doit jamais etre
publie, ni mis dans une archive du port. Le code de l'editeur, lui, ne contient ni ROM ni image.

## Limites connues

- Les objets (pizzas, `code < $10`) ne sont pas dans les listes d'ennemis lues ici (aucune entree de ce genre trouvee) ; les listes d'objets de la bank 4 (`$ACCD`) ne sont pas lues.
- Les chefs a plusieurs elements (marqueur `$F5`) et quelques chefs n'ont pas de sprite (rectangle de repli).
- Les noms de types sont ceux decides dans le projet ; les autres s'affichent « type $xx » (a completer dans `data/names.json`).
- Position des vagues : calculee depuis la camera verrouillee (fin de l'ecran X-1) ; a confirmer visuellement.
