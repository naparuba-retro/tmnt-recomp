"use strict";
/* Editeur de spawns : voir conception_editeur_spawns.md. Etat du masque : adresse (4 hex) -> {cle, signature, jeux:Set, note}.
   jeux contient "*" (toutes les variantes) ou des jeux de sprites en hex a 2 chiffres. */
const $ = (id) => document.getElementById(id);
const hex2 = (n) => n.toString(16).toUpperCase().padStart(2, "0");
const LOCKED = new Set(["chef", "marqueur"]);

const S = {
  data: null, mask: new Map(), zone: null, view: "zone", screen: [0, 0], varIdx: 0, auto: false, side: false,
  zoom: 2, fType: "", fOff: false, labels: true, sel: null, autoTimer: null, saveTimer: null, hits: [], imgs: new Map(),
};

/* ---------------------------------------------------------------------------------------- donnees */
function allZones() { return S.data.niveaux.flatMap((l) => l.zones); }
function setsOf(z) { return z.jeux_sprites; }
function variantOf(e, setIdx) { return e.variantes.length === 1 ? e.variantes[0] : e.variantes[Math.min(setIdx, e.variantes.length - 1)]; }
function allJeux(e) { return [...new Set(e.variantes.map((v) => hex2(v.jeu)))]; }
function isOff(e, v) {
  const m = S.mask.get(e.adresse);
  return !!m && (m.jeux.has("*") || m.jeux.has(hex2(v.jeu)));
}
function offCount(e) { const m = S.mask.get(e.adresse); return m ? 1 : 0; }

function loadImg(src) {
  if (!src) return null;
  let im = S.imgs.get(src);
  if (!im) {
    im = new Image();
    im.onload = () => render();
    im.src = "/build/" + src;
    S.imgs.set(src, im);
  }
  return im;
}

/* ---------------------------------------------------------------------------------------- masque */
function maskToJson() {
  const out = [];
  for (const [adresse, m] of S.mask) {
    out.push({ cle: m.cle, adresse, signature: m.signature, jeux_desactives: [...m.jeux], note: m.note || "" });
  }
  return { version: 1, desactives: out };
}
function scheduleSave() {
  setStatus("Enregistrement...", "");
  clearTimeout(S.saveTimer);
  S.saveTimer = setTimeout(saveNow, 250);
}
async function saveNow() {
  try {
    const r = await fetch("/api/mask", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(maskToJson()) });
    const j = await r.json();
    if (!r.ok) throw new Error(j.erreur || r.status);
    if (j.avertissements && j.avertissements.length) setStatus("Enregistre, " + j.avertissements.length + " avertissement(s) : " + j.avertissements[0], "err");
    else setStatus("Enregistre (" + j.fichier_jeu + ")", "ok");
  } catch (e) {
    setStatus("Echec de l'enregistrement : " + e.message, "err");
  }
}
function setStatus(t, cls) { const s = $("status"); s.textContent = t; s.className = "status " + (cls || ""); }

function toggleVariant(e, v) {
  if (LOCKED.has(e.genre)) { setStatus("Un chef ou un declencheur ne peut pas etre coupe", "err"); return; }
  const all = allJeux(e);
  let m = S.mask.get(e.adresse);
  const cur = new Set(m ? (m.jeux.has("*") ? all : [...m.jeux]) : []);
  const k = hex2(v.jeu);
  if (cur.has(k)) cur.delete(k); else cur.add(k);
  applyJeux(e, m, cur, all);
}
function setAll(e, off) {
  if (LOCKED.has(e.genre)) { setStatus("Un chef ou un declencheur ne peut pas etre coupe", "err"); return; }
  const all = allJeux(e);
  applyJeux(e, S.mask.get(e.adresse), off ? new Set(all) : new Set(), all);
}
function applyJeux(e, m, cur, all) {
  if (cur.size === 0) S.mask.delete(e.adresse);
  else {
    if (!m) m = { cle: e.cle, signature: e.signature, jeux: new Set(), note: "" };
    m.jeux = all.every((k) => cur.has(k)) ? new Set(["*"]) : cur;
    S.mask.set(e.adresse, m);
  }
  scheduleSave();
  refreshAll();
}

/* ---------------------------------------------------------------------------------------- vue */
function zoneEntries(z) {
  return z.entrees.filter((e) => {
    if (S.view === "ecran" && (e.bloc[0] !== S.screen[0] || e.bloc[1] !== S.screen[1])) return false;
    return true;
  });
}
function visibleEntries(z, setIdx) {
  return zoneEntries(z).filter((e) => {
    const v = variantOf(e, setIdx);
    if (S.fType && String(v.type) !== S.fType) return false;
    if (S.fOff && !isOff(e, v)) return false;
    return true;
  });
}
function colorOf(e) {
  const c = getComputedStyle(document.documentElement);
  return { chef: c.getPropertyValue("--boss"), marqueur: c.getPropertyValue("--mark"), objet: c.getPropertyValue("--obj"), vague: c.getPropertyValue("--wave") }[e.genre] || "rgba(255,255,255,.35)";
}

function drawZone(canvas, z, setIdx, hits) {
  const sc = S.zoom;
  const W = z.ecrans[0] * z.ecran_w, H = z.ecrans[1] * z.ecran_h;
  let vx = 0, vy = 0, vw = W, vh = H;
  if (S.view === "ecran") { vx = S.screen[0] * z.ecran_w; vy = S.screen[1] * z.ecran_h; vw = z.ecran_w; vh = z.ecran_h; }
  canvas.width = vw * sc; canvas.height = vh * sc;
  const ctx = canvas.getContext("2d");
  ctx.imageSmoothingEnabled = false;
  ctx.fillStyle = "#111"; ctx.fillRect(0, 0, canvas.width, canvas.height);
  const map = loadImg(z.carte);
  if (map && map.complete) ctx.drawImage(map, vx, vy, vw, vh, 0, 0, vw * sc, vh * sc);
  ctx.strokeStyle = "rgba(255,255,255,.35)"; ctx.setLineDash([4, 4]); ctx.lineWidth = 1;
  for (let bx = 1; bx < z.ecrans[0]; bx++) { const x = (bx * z.ecran_w - vx) * sc; if (x > 0 && x < canvas.width) { ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, canvas.height); ctx.stroke(); } }
  for (let by = 1; by < z.ecrans[1]; by++) { const y = (by * z.ecran_h - vy) * sc; if (y > 0 && y < canvas.height) { ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(canvas.width, y); ctx.stroke(); } }
  ctx.setLineDash([]);
  ctx.font = "11px system-ui, sans-serif"; ctx.textBaseline = "bottom";
  for (const e of visibleEntries(z, setIdx)) {
    const v = variantOf(e, setIdx);
    const off = isOff(e, v);
    const ax = e.wx - vx, ay = e.wy - vy;
    let bx, by, bw, bh;
    const im = v.sprite ? loadImg(v.sprite.img) : null;
    if (v.sprite) { bx = (ax + v.sprite.ox) * sc; by = (ay + v.sprite.oy) * sc; bw = v.sprite.w * sc; bh = v.sprite.h * sc; }
    else { bx = (ax - 6) * sc; by = (ay - 6) * sc; bw = 12 * sc; bh = 12 * sc; }
    ctx.globalAlpha = off ? 0.35 : 1;
    if (im && im.complete) ctx.drawImage(im, bx, by, bw, bh);
    else { ctx.fillStyle = "rgba(200,200,200,.5)"; ctx.fillRect(bx, by, bw, bh); }
    ctx.globalAlpha = 1;
    ctx.lineWidth = 1;
    ctx.strokeStyle = off ? getComputedStyle(document.documentElement).getPropertyValue("--off") : colorOf(e);
    ctx.setLineDash(off ? [3, 2] : []);
    ctx.strokeRect(bx + .5, by + .5, bw - 1, bh - 1);
    ctx.setLineDash([]);
    if (off) { ctx.strokeStyle = "#ff4b36"; ctx.lineWidth = 2; ctx.beginPath(); ctx.moveTo(bx, by); ctx.lineTo(bx + bw, by + bh); ctx.moveTo(bx + bw, by); ctx.lineTo(bx, by + bh); ctx.stroke(); }
    if (S.sel === e) { ctx.strokeStyle = getComputedStyle(document.documentElement).getPropertyValue("--sel"); ctx.lineWidth = 3; ctx.strokeRect(bx - 2, by - 2, bw + 4, bh + 4); }
    if (S.labels) {
      const t = v.nom + (e.variantes.length > 1 && !S.side ? "" : "");
      ctx.fillStyle = "rgba(0,0,0,.65)"; const tw = ctx.measureText(t).width + 4; ctx.fillRect(bx, by - 13, tw, 13);
      ctx.fillStyle = "#fff"; ctx.fillText(t, bx + 2, by - 1);
    }
    hits.push({ e, v, x0: bx, y0: by, x1: bx + bw, y1: by + bh });
  }
}

function render() {
  const z = S.zone;
  if (!z) return;
  const stage = $("stage");
  const keepX = stage.scrollLeft, keepY = stage.scrollTop;
  const sets = setsOf(z);
  S.hits = [];
  stage.textContent = "";
  const idxs = S.side && sets.length > 1 ? sets.map((_, i) => i) : [S.varIdx];
  for (const i of idxs) {
    if (idxs.length > 1) { const cap = document.createElement("div"); cap.className = "cap"; cap.textContent = varText(z, i); stage.appendChild(cap); }
    const cv = document.createElement("canvas");
    const hits = [];
    drawZone(cv, z, i, hits);
    cv.addEventListener("click", (ev) => onCanvasClick(ev, cv, hits));
    stage.appendChild(cv);
  }
  stage.scrollLeft = keepX; stage.scrollTop = keepY;
  renderList(); renderPanel();
  $("var-label").textContent = varText(z, S.varIdx);
}
function varText(z, i) {
  const s = setsOf(z);
  if (s.length === 1) return "Cette zone n'a qu'une variante : rien a parcourir (jeu de sprites $" + hex2(s[0].jeu) + ")";
  return "Variante " + (i + 1) + "/" + s.length + " - jeu $" + hex2(s[i].jeu) + " (rangs " + s[i].rangs.join(",") + ")";
}
function onCanvasClick(ev, cv, hits) {
  const r = cv.getBoundingClientRect();
  const x = (ev.clientX - r.left) * (cv.width / r.width), y = (ev.clientY - r.top) * (cv.height / r.height);
  for (let i = hits.length - 1; i >= 0; i--) {
    const h = hits[i];
    if (x >= h.x0 && x <= h.x1 && y >= h.y0 && y <= h.y1) { S.sel = h.e; if (ev.detail === 2) toggleVariant(h.e, h.v); else refreshAll(); return; }
  }
  S.sel = null; refreshAll();
}

function renderList() {
  const z = S.zone, tb = $("list").tBodies[0];
  tb.textContent = "";
  for (const e of visibleEntries(z, S.varIdx)) {
    const v = variantOf(e, S.varIdx), off = isOff(e, v);
    const tr = document.createElement("tr");
    tr.className = "row" + (off ? " off" : "") + (S.sel === e ? " sel" : "");
    const td = (t) => { const c = document.createElement("td"); c.textContent = t; tr.appendChild(c); return c; };
    const c0 = document.createElement("td"); const cb = document.createElement("input");
    cb.type = "checkbox"; cb.checked = !off; cb.disabled = LOCKED.has(e.genre); cb.setAttribute("aria-label", "Actif : " + e.cle);
    cb.addEventListener("click", (ev) => { ev.stopPropagation(); toggleVariant(e, v); });
    c0.appendChild(cb); tr.appendChild(c0);
    td(e.declencheur ? "vague " + e.declencheur.numero : e.bloc[0] + "," + e.bloc[1]); td(e.x); td(e.y); td("$" + e.code); td(v.nom + " ($" + hex2(v.type) + ")");
    const g = td(""); const b = document.createElement("span"); b.className = "badge b-" + e.genre; b.textContent = e.genre; g.appendChild(b);
    tr.addEventListener("click", () => { S.sel = e; refreshAll(); });
    tb.appendChild(tr);
  }
}

function renderPanel() {
  const p = $("panel"); p.textContent = "";
  const e = S.sel;
  const h = (tag, txt, cls) => { const n = document.createElement(tag); if (txt != null) n.textContent = txt; if (cls) n.className = cls; return n; };
  if (!e) { p.appendChild(h("p", "Choisis un spawn (clic sur un sprite ou une ligne). Double-clic sur un sprite : couper ou remettre la variante affichee.", "muted")); legend(p, h); return; }
  p.appendChild(h("h3", e.cle));
  const dl = h("dl");
  const row = (k, v) => { dl.appendChild(h("dt", k)); dl.appendChild(h("dd", v)); };
  row("Adresse ROM", "B6:" + e.adresse); row("Ecran", e.bloc[0] + "," + e.bloc[1]); row("Position", "x=" + e.x + "  y=" + e.y);
  row("Code", "$" + e.code + " (" + parseInt(e.code, 16).toString(2).padStart(8, "0") + ")"); row("Genre", e.genre);
  row("Orientation", e.flip ? "vers la gauche (x >= $80)" : "vers la droite");
  if (e.declencheur) {
    const d = e.declencheur;
    row("Declencheur", "vague " + d.numero + " : camera a l'ecran Y " + d.camera_ecran_y + ", fin de l'ecran X " + (d.camera_ecran_x_plus_1 ? d.camera_ecran_x_plus_1 - 1 : 0) + (d.marqueur ? ", marqueur $" + d.marqueur : ""));
  }
  p.appendChild(dl);
  if (LOCKED.has(e.genre)) p.appendChild(h("div", "Chef ou declencheur : visible mais non desactivable (couper un chef ferait croire a la fin du combat et sauterait la scene suivante).", "warn lock"));
  if (e.partage.length) p.appendChild(h("div", "Liste partagee : cette entree est aussi utilisee par " + e.partage.join(", ") + ". Couper l'une coupe les autres.", "warn"));
  const t = h("table", null, "vars");
  e.variantes.forEach((v, i) => {
    const tr = h("tr"); if (variantOf(e, S.varIdx) === v) tr.className = "cur";
    const c0 = h("td"); const cb = h("input"); cb.type = "checkbox"; cb.checked = !isOff(e, v); cb.disabled = LOCKED.has(e.genre);
    cb.setAttribute("aria-label", "Actif : variante " + (i + 1)); cb.addEventListener("click", () => toggleVariant(e, v)); c0.appendChild(cb); tr.appendChild(c0);
    const c1 = h("td"); if (v.sprite) { const im = h("img"); im.src = "/build/" + v.sprite.img; im.alt = v.nom; im.width = v.sprite.w * 2; im.height = v.sprite.h * 2; c1.appendChild(im); } tr.appendChild(c1);
    const c2 = h("td"); c2.appendChild(h("div", v.nom)); c2.appendChild(h("div", "type $" + hex2(v.type) + " - jeu $" + hex2(v.jeu) + (e.variantes.length > 1 ? " - rangs " + v.rangs.join(",") : ""), "muted")); tr.appendChild(c2);
    t.appendChild(tr);
  });
  p.appendChild(t);
  if (!LOCKED.has(e.genre)) {
    const b = h("div", null, "btns");
    const b1 = h("button", "Tout couper"); b1.type = "button"; b1.onclick = () => setAll(e, true);
    const b2 = h("button", "Tout remettre"); b2.type = "button"; b2.onclick = () => setAll(e, false);
    b.appendChild(b1); b.appendChild(b2); p.appendChild(b);
    const lab = h("label", "Note (enregistree avec le masque)"); lab.htmlFor = "note"; p.appendChild(lab);
    const ta = h("textarea", null, "note"); ta.id = "note"; ta.value = (S.mask.get(e.adresse) || {}).note || "";
    ta.disabled = !S.mask.has(e.adresse); ta.title = S.mask.has(e.adresse) ? "" : "Coupe d'abord le spawn pour pouvoir ajouter une note";
    ta.addEventListener("change", () => { const m = S.mask.get(e.adresse); if (m) { m.note = ta.value; scheduleSave(); } });
    p.appendChild(ta);
  }
  legend(p, h);
}
function legend(p, h) {
  p.appendChild(h("p", "Raccourcis : fleches gauche/droite = variante, espace = couper/remettre la variante affichee du spawn choisi, Echap = deselectionner.", "muted"));
}

/* ---------------------------------------------------------------------------------------- navigation */
function zoneStats(z) {
  let cut = 0;
  for (const e of z.entrees) if (S.mask.has(e.adresse)) cut++;
  return { n: z.entrees.length, cut };
}
function buildSide() {
  const side = $("side"); side.textContent = "";
  for (const lv of S.data.niveaux) {
    const d = document.createElement("details"); d.open = S.zone ? S.zone.niveau === lv.id : lv.id === 1;
    const s = document.createElement("summary"); s.textContent = lv.nom; d.appendChild(s);
    for (const z of lv.zones) {
      const b = document.createElement("button"); b.type = "button"; b.className = "zbtn" + (S.zone === z ? " on" : "");
      const st = zoneStats(z);
      b.innerHTML = "";
      b.appendChild(document.createTextNode(z.libelle.replace(/ \(.*\)/, "")));
      const n = document.createElement("span"); n.className = "n";
      n.textContent = st.n + (st.cut ? " " : "");
      if (st.cut) { const c = document.createElement("span"); c.className = "cut"; c.textContent = "(" + st.cut + " coupes)"; n.appendChild(c); }
      b.appendChild(n);
      b.addEventListener("click", () => selectZone(z));
      d.appendChild(b);
    }
    side.appendChild(d);
  }
}
function selectZone(z) {
  S.zone = z; S.varIdx = 0; S.sel = null; S.screen = [0, 0]; S.fType = "";
  const types = new Map();
  for (const e of z.entrees) for (const v of e.variantes) types.set(String(v.type), v.nom);
  const sel = $("f-type"); sel.textContent = "";
  const o0 = document.createElement("option"); o0.value = ""; o0.textContent = "tous"; sel.appendChild(o0);
  for (const [k, n] of [...types].sort((a, b) => a[1].localeCompare(b[1]))) { const o = document.createElement("option"); o.value = k; o.textContent = n; sel.appendChild(o); }
  $("ztitle").textContent = "Niveau " + z.niveau + " - " + z.libelle + " (" + z.ecrans[0] + " x " + z.ecrans[1] + " ecrans)";
  refreshAll(true);
}
function buildScreenBar() {
  const bar = $("screenbar"), z = S.zone;
  bar.hidden = S.view !== "ecran"; bar.textContent = "";
  if (bar.hidden) return;
  for (let by = 0; by < z.ecrans[1]; by++) for (let bx = 0; bx < z.ecrans[0]; bx++) {
    const n = z.entrees.filter((e) => e.bloc[0] === bx && e.bloc[1] === by).length;
    const b = document.createElement("button"); b.type = "button"; b.textContent = "Ecran " + bx + "," + by + " (" + n + ")";
    if (bx === S.screen[0] && by === S.screen[1]) b.className = "on";
    b.addEventListener("click", () => { S.screen = [bx, by]; S.sel = null; refreshAll(); });
    bar.appendChild(b);
  }
}
function refreshAll(sideToo) {
  if (!S.zone) return;
  $("v-zone").className = "seg" + (S.view === "zone" ? " on" : "");
  $("v-screen").className = "seg" + (S.view === "ecran" ? " on" : "");
  const multi = setsOf(S.zone).length > 1;
  for (const id of ["var-prev", "var-next"]) $(id).hidden = !multi;
  for (const id of ["var-auto", "var-side"]) $(id).parentElement.hidden = !multi;
  $("var-label").className = "varlabel" + (multi ? "" : " muted");
  buildScreenBar(); buildSide(); render();
  let tot = 0, ent = 0;
  for (const m of S.mask.values()) { ent++; tot += m.jeux.has("*") ? 1 : m.jeux.size; }
  $("summary").textContent = ent ? ent + " spawn(s) coupe(s)" : "Aucun spawn coupe";
}
function stepVariant(d) {
  const n = setsOf(S.zone).length; if (n < 2) return;
  S.varIdx = (S.varIdx + d + n) % n; refreshAll();
}

/* ---------------------------------------------------------------------------------------- demarrage */
async function init() {
  try {
    const [d, m] = await Promise.all([fetch("/api/data").then((r) => r.json()), fetch("/api/mask").then((r) => r.json())]);
    S.data = d;
    for (const x of m.desactives || []) S.mask.set(x.adresse, { cle: x.cle, signature: x.signature, jeux: new Set(x.jeux_desactives || ["*"]), note: x.note || "" });
  } catch (e) { $("stage").textContent = "Impossible de charger les donnees : " + e.message; return; }
  const roms = Object.entries(S.data.roms || {}).map(([n, r]) => n + " " + r.crc32).join(" / ");
  $("rominfo").textContent = "ROM : " + S.data.rom.nom + " (" + roms + ") - " + S.data.nb_entrees + " spawns";
  $("v-zone").onclick = () => { S.view = "zone"; refreshAll(); };
  $("v-screen").onclick = () => { S.view = "ecran"; refreshAll(); };
  $("var-prev").onclick = () => stepVariant(-1);
  $("var-next").onclick = () => stepVariant(1);
  $("var-auto").onchange = (ev) => { clearInterval(S.autoTimer); if (ev.target.checked) S.autoTimer = setInterval(() => stepVariant(1), 1200); };
  $("var-side").onchange = (ev) => { S.side = ev.target.checked; refreshAll(); };
  $("f-type").onchange = (ev) => { S.fType = ev.target.value; refreshAll(); };
  $("f-off").onchange = (ev) => { S.fOff = ev.target.checked; refreshAll(); };
  $("f-labels").onchange = (ev) => { S.labels = ev.target.checked; refreshAll(); };
  $("f-zoom").onchange = (ev) => { S.zoom = +ev.target.value; refreshAll(); };
  document.addEventListener("keydown", (ev) => {
    if (/^(INPUT|TEXTAREA|SELECT)$/.test(ev.target.tagName) && ev.key !== "Escape") return;
    if (ev.key === "ArrowLeft") { stepVariant(-1); ev.preventDefault(); }
    else if (ev.key === "ArrowRight") { stepVariant(1); ev.preventDefault(); }
    else if (ev.key === "Escape") { S.sel = null; refreshAll(); }
    else if (ev.key === " " && S.sel) { toggleVariant(S.sel, variantOf(S.sel, S.varIdx)); ev.preventDefault(); }
  });
  selectZone(allZones()[0]);
}
init();
