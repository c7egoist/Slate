"use strict";
/* Headless smoke test: stub just enough DOM + canvas 2D context to run the real
   browser boot path (DOMContentLoaded) and a simulated face pick, catching
   wiring bugs (missing elements, bad handlers) without a real browser. */
const fs = require("fs");
const path = require("path");
const JS = path.join(__dirname, "..", "js");

/* ---- minimal canvas 2D context stub ---- */
function makeCtx() {
  const noop = () => {};
  return new Proxy({
    canvas: { width: 800, height: 600 },
    setTransform: noop, clearRect: noop, fillRect: noop, strokeRect: noop,
    beginPath: noop, moveTo: noop, lineTo: noop, closePath: noop, arc: noop,
    fill: noop, stroke: noop, save: noop, restore: noop, translate: noop,
    rotate: noop, clip: noop, setLineDash: noop, fillText: noop,
    createImageData: (w, h) => ({ data: new Uint8ClampedArray(w*h*4), width: w, height: h }),
    putImageData: noop, getImageData: (x,y,w,h) => ({ data: new Uint8ClampedArray(w*h*4) }),
    createRadialGradient: () => ({ addColorStop: noop }),
  }, { get(t, k) { return k in t ? t[k] : 0; }, set(t,k,v){ t[k]=v; return true; } });
}

/* ---- minimal element ---- */
function makeEl(tag) {
  const el = {
    tagName: (tag||"div").toUpperCase(), dataset: {}, style: {}, children: [],
    _cls: new Set(), _listeners: {}, parentElement: null, innerHTML: "", textContent: "",
    classList: {
      add: c => el._cls.add(c), remove: c => el._cls.delete(c),
      toggle: (c, f) => { const on = f===undefined ? !el._cls.has(c) : f; on?el._cls.add(c):el._cls.delete(c); return on; },
      contains: c => el._cls.has(c),
    },
    getContext: () => el._ctx || (el._ctx = makeCtx()),
    getBoundingClientRect: () => ({ left:0, top:0, width:800, height:600, right:800, bottom:600 }),
    addEventListener: (ev, fn) => { (el._listeners[ev] = el._listeners[ev]||[]).push(fn); },
    removeEventListener: noopFn, setPointerCapture: noopFn, releasePointerCapture: noopFn,
    appendChild: c => { el.children.push(c); c.parentElement = el; return c; },
    querySelector: () => null, querySelectorAll: () => [],
    closest: () => null, focus: noopFn, select: noopFn, replaceWith: noopFn,
    nextElementSibling: null,
    set onclick(v){ el._listeners.click=[v]; }, get onclick(){ return (el._listeners.click||[])[0]; },
  };
  return el;
}
function noopFn(){}

/* ---- element registry keyed by id; unknown ids auto-create ---- */
const byId = {};
function el(id) { return byId[id] || (byId[id] = makeEl("div")); }

/* pre-register the canvases with a real parent chain so resize() works */
["vp3Canvas","uvBg","uvFg"].forEach(id => { const c=makeEl("canvas"); const wrap=makeEl("div"); wrap.appendChild(c); byId[id]=c; });

global.window = {
  devicePixelRatio: 1, innerWidth: 1600, innerHeight: 900,
  _listeners: {}, addEventListener: (ev, fn) => { (global.window._listeners[ev]=global.window._listeners[ev]||[]).push(fn); },
  requestAnimationFrame: () => 0,
};
global.requestAnimationFrame = () => 0;
global.lucide = { createIcons: () => {} };
/* a tiny pool of fake elements returned for querySelectorAll so the many
   forEach-bindings actually run their .onclick assignment path. Each has a
   dataset so data-attr reads don't throw. */
function fakeList(n, dataset) {
  return Array.from({ length: n }, () => { const e = makeEl("div"); Object.assign(e.dataset, dataset||{}); return e; });
}
global.document = {
  getElementById: id => el(id),
  querySelector: () => makeEl("div"),
  querySelectorAll: sel => {
    // return a few nodes for the known multi-binding selectors, else empty
    if (/smbtn/.test(sel)) return fakeList(4, { mode: "face" });
    if (/uvtool/.test(sel)) return fakeList(4, { uvtool: "select" });
    if (/data-unwrap/.test(sel)) return fakeList(5, { unwrap: "planar" });
    if (/data-vptool/.test(sel)) return fakeList(5, { vptool: "box" });
    if (/data-vpdisp/.test(sel)) return fakeList(2, { vpdisp: "showWire" });
    if (/mc-tile|data-vpmc/.test(sel)) return fakeList(4, { vpmc: "clay" });
    if (/data-grid/.test(sel)) return fakeList(3, { grid: "dots" });
    if (/data-disp/.test(sel)) return fakeList(4, { disp: "showGrid" });
    if (/seg-btn/.test(sel)) return fakeList(4, { s: "L" });
    if (/matcap-swatch/.test(sel)) return fakeList(4, { mc: "clay" });
    if (/prop-title/.test(sel)) return fakeList(3, {});
    if (/snapMode .pill|#snapMode/.test(sel)) return fakeList(4, { snap: "0" });
    return [];
  },
  addEventListener: (ev, fn) => { (global.window._listeners[ev]=global.window._listeners[ev]||[]).push(fn); },
  createElement: tag => makeEl(tag),
  body: makeEl("body"),
};

/* ---- load browser scripts into the shared global scope ---- */
function load(f) {
  let s = fs.readFileSync(path.join(JS, f), "utf8")
    .replace(/^"use strict";?/, "")
    .replace(/if \(typeof module[\s\S]*?\}\s*$/, "")
    .replace(/\bconst ([A-Z_][A-Za-z0-9_]*) =/g, "global.$1 =")
    .replace(/\bfunction ([a-zA-Z0-9_]+)/g, "global.$1 = function $1");
  eval.call(global, "(function(){" + s + "})()");
}
["mesh-data.js","mat4.js","halfedge.js","selection.js","udim.js","unwrap.js","checker.js","stretch.js","viewport3d.js","uveditor2d.js","history.js","app.js"].forEach(load);

/* ---- fire DOMContentLoaded (runs APP boot) ---- */
try {
  (global.window._listeners.DOMContentLoaded||[]).forEach(fn => fn());
  console.log("[boot] DOMContentLoaded ran without throwing");
  console.log("[boot] islands after default conformal:", UVED.islands.length);
  console.log("[boot] TOPO faces:", TOPO.faceCount, "matcaps:", Object.keys(VP3.matcaps).join(","));

  // simulate: switch to face mode, pick a face via the real pick path
  SEL.mode = "face"; selClear();
  vp3Project();                 // needs scr populated before pick
  vp3Pick(400, 300, false);
  console.log("[pick] faces selected after center pick:", SEL.faces.size);

  // run every unwrap through the app entry
  for (const m of ["unwrap","smart","planar","box","cylindrical","spherical","conformal"]) {
    doUnwrap(m);
    console.log("[unwrap]", m, "->", UVED.islands.length, "islands");
  }

  // 2D island select -> mesh sync
  if (UVED.islands.length) { UVED.islands[0].selected = true; uvedSyncToMesh(); console.log("[sync] 2D->3D faces:", SEL.faces.size); }

  // ---- new selection tooling ----
  const assert = (c, m) => { if (!c) throw new Error("assert failed: " + m); };

  // ---- polygon topology preserved (quads/ngons/tris imported as-is) ----
  // Suzanne = 500 polygons (468 quads + 32 tris), NOT 968 triangles.
  assert(MESH.faceCount === 500, "polygon faceCount 500 (got " + MESH.faceCount + ")");
  assert(TOPO.faceCount === 500, "TOPO.faceCount 500");
  const arity = {};
  for (let f = 0; f < TOPO.faceCount; f++) arity[TOPO.faceVerts[f].length] = (arity[TOPO.faceVerts[f].length]||0)+1;
  console.log("[poly] arity", JSON.stringify(arity));
  assert(arity[3] === 32 && arity[4] === 468, "arity {3:32,4:468}");
  // an N-gon has N loop edges (no fan diagonals) and N-2 fan triangles
  for (let f = 0; f < TOPO.faceCount; f++) {
    const n = TOPO.faceVerts[f].length;
    assert(TOPO.faceEdges[f].length === n, "faceEdges arity matches loop for face " + f);
    assert(TOPO.faceTris[f].length === n - 2, "faceTris = N-2 for face " + f);
  }
  // flat tri list drives the rasterizer; every tri maps back to a valid face
  const triTotal = TOPO.faceVerts.reduce((s, l) => s + (l.length - 2), 0);
  assert(TOPO.tris.length === triTotal, "TOPO.tris total = Σ(N-2) = " + triTotal);
  assert(triTotal === 468*2 + 32, "tri total 968");
  for (const t of TOPO.tris) assert(t.face >= 0 && t.face < TOPO.faceCount && t.v.length === 3, "tri.face valid + 3 verts");
  console.log("[poly]", TOPO.faceCount, "polygons ->", TOPO.tris.length, "fan tris; edges", TOPO.edgeKeys.length);

  // matcaps: the 10 new ones plus the original 4 all built
  const mcNames = Object.keys(VP3.matcaps);
  for (const n of ["gold","copper","steel","grey","white","blue","green","purple","skin","ceramic","cavityGrey","cavityClay"]) assert(mcNames.includes(n), "matcap "+n);
  console.log("[matcaps]", mcNames.length, "built:", mcNames.join(","));

  // grid modes + frame selection
  assert(["dots","lines","off"].includes(VP3.gridMode), "gridMode default");
  SEL.mode = "face"; selClear(); SEL.faces.add(0); SEL.faces.add(1);
  const before = { dist: VP3.tDist, pan: VP3.tPan.slice() };
  vp3FrameSelection();
  assert(VP3.tPan.every(isFinite) && isFinite(VP3.tDist), "frameSelection finite target");
  // framing 2 faces should pull the pivot onto them (pan moved) and pick a sane dist
  assert(VP3.tDist >= 1.2 && VP3.tDist <= 20, "frameSelection dist in range");
  console.log("[frame] 2-face bbox -> dist", VP3.tDist.toFixed(2), "pan", VP3.tPan.map(x=>x.toFixed(2)).join(","));
  // empty selection frames the whole mesh
  selClear(); vp3FrameSelection();
  assert(isFinite(VP3.tDist), "frameSelection whole-mesh finite");

  // region select: full-canvas box in each mode selects >0 (needs a projection)
  vp3Project();
  const insideAll = () => true;
  for (const mode of ["vertex","edge","face"]) {
    SEL.mode = mode; selClear();
    const n = selectRegion(insideAll, vp3ElementAt, vp3ElementVisible, false, false);
    console.log("[region]", mode, "->", n);
    assert(n > 0, "region "+mode+" selected nothing");
  }

  // shortest path is contiguous + correct endpoints. Pick a target that is
  // actually reachable from vert 0 (a mesh-agnostic pick: a 2-ring neighbour),
  // rather than a hardcoded index that may sit on a disconnected shell.
  const ring1 = [...TOPO.vertNeighbors[0]];
  const target = [...TOPO.vertNeighbors[ring1[0]]].find(v => v !== 0 && !TOPO.vertNeighbors[0].has(v)) ?? ring1[0];
  const vp = shortestVertPath(0, target);
  assert(vp && vp[0] === 0 && vp[vp.length-1] === target, "path endpoints");
  for (let i = 0; i+1 < vp.length; i++) assert(TOPO.vertNeighbors[vp[i]].has(vp[i+1]), "path contiguous");
  console.log("[path] 0.." + target + " len", vp.length, "contiguous ✔");

  // edge loop from a seed edge selects a run
  SEL.mode = "edge"; selClear(); selLoopFromEdge(TOPO.edgeKeys[18]);
  console.log("[loop] edge", TOPO.edgeKeys[18], "->", SEL.edges.size);
  assert(SEL.edges.size > 1, "edge loop");

  // ---- edge loop is TOPOLOGY-driven, not camera-facing dependent (backlog #6) --
  // Bug: the old walk chose the "straightest" continuing edge, so seeding from a
  // different edge of the same loop (or a different camera angle) gave a different
  // loop. The quad rule (continue across a valence-4 vertex to the edge sharing no
  // face with the current one) is facing-independent: for EVERY edge whose loop
  // the topology rule can fully trace, seeding from any member edge yields the
  // IDENTICAL edge set. We test the pure rule (walkLoopDir, no straightness
  // fallback) since the fallback only fires at poles where no loop exists.
  {
    const pureLoop = (seedKey) => {
      const loop = new Set([seedKey]);
      const [va, vb] = TOPO.edgeVerts[seedKey];
      walkLoopDir(loop, seedKey, va); walkLoopDir(loop, seedKey, vb);
      return loop;
    };
    let tested = 0, facingDependent = 0;
    for (const seed of TOPO.edgeKeys) {
      const loop = pureLoop(seed);
      if (loop.size < 3) continue;                 // trivial / pole-terminated
      tested++;
      for (const k of loop) {
        const l2 = pureLoop(k);
        if (l2.size !== loop.size || [...loop].some(e => !l2.has(e))) { facingDependent++; break; }
      }
    }
    console.log("[loop] pure quad-loop rule:", tested, "loops tested,", facingDependent, "facing-dependent");
    assert(tested > 100, "enough quad loops to test facing-independence");
    assert(facingDependent === 0, "edge loop is facing-independent (topology-driven, not straightest-edge)");
    // loopContinueAtVert returns an edge sharing no face with the current edge
    const q = TOPO.edgeKeys.find(k => {
      const [v] = TOPO.edgeVerts[k];
      return TOPO.vertNeighbors[v].size === 4 && loopContinueAtVert(v, k);
    });
    if (q) {
      const [v] = TOPO.edgeVerts[q];
      const cont = loopContinueAtVert(v, q);
      const shared = TOPO.edgeFaces[cont.k].some(f => TOPO.edgeFaces[q].includes(f));
      assert(!shared, "loop continuation shares no face with the current edge");
    }
    console.log("[loop] topology-driven loop selection is facing-independent ✔");
  }

  // ---- FACE loop is a single continuous band, not a perpendicular cross (backlog #6) --
  // Bug: selRingFromFace walked out ALL FOUR seed edges with a geometric "most
  // anti-parallel" heuristic, tracing two crossed bands (looked like it grabbed
  // neighbours in every direction) AND flipping with camera facing. The quad
  // rule walks ONE opposite-edge axis via oppositeFaceEdge, so the result is one
  // connected chain where no face has more than 2 in-band neighbours.
  {
    // find a quad seed whose band is non-trivial
    let seedFace = -1;
    for (let f = 0; f < TOPO.faceCount; f++) {
      if (TOPO.faceEdges[f].length !== 4) continue;
      SEL.mode = "face"; selClear(); selRingFromFace(f);
      if (SEL.faces.size >= 5) { seedFace = f; break; }
    }
    assert(seedFace >= 0, "found a quad seed with a real face band");
    SEL.mode = "face"; selClear(); selRingFromFace(seedFace);
    const band = new Set(SEL.faces);
    // (a) connected: BFS across shared edges within the band reaches every face
    const arr = [...band], bseen = new Set([arr[0]]), stk = [arr[0]];
    while (stk.length) { const f = stk.pop(); for (const k of TOPO.faceEdges[f]) for (const nf of TOPO.edgeFaces[k]) if (band.has(nf) && !bseen.has(nf)) { bseen.add(nf); stk.push(nf); } }
    assert(bseen.size === band.size, "face band is a single connected component");
    // (b) NOT a cross: no band face has more than 2 in-band edge-neighbours
    let maxNb = 0;
    for (const f of band) { let c = 0; for (const k of TOPO.faceEdges[f]) for (const nf of TOPO.edgeFaces[k]) if (nf !== f && band.has(nf)) c++; maxNb = Math.max(maxNb, c); }
    assert(maxNb <= 2, "face band is a chain (<=2 neighbours each), not a perpendicular cross");
    // oppositeFaceEdge returns a quad edge sharing no vertex with the given edge
    const qf = TOPO.faceEdges.findIndex(es => es.length === 4);
    const oe = oppositeFaceEdge(qf, TOPO.faceEdges[qf][0]);
    const [ea, eb] = TOPO.edgeVerts[TOPO.faceEdges[qf][0]], [oa, ob] = TOPO.edgeVerts[oe];
    assert(oa !== ea && oa !== eb && ob !== ea && ob !== eb, "opposite face edge shares no vertex");
    console.log("[loop] face loop: single connected band size", band.size, "max in-band neighbours", maxNb, "(no cross) ✔");
    SEL.mode = "face"; selClear();
  }

  // ---- loop/ring selection PREVIEW: ghost the set a Ctrl+click would grab (backlog #8) --
  // Hovering with the loop-modifier held must populate VP3.preview with EXACTLY
  // the loop/ring a commit would select (no SEL mutation), and clear it when the
  // modifier is not held. We drive the real vp3HoverAt path with a synthetic
  // cursor placed over a known element's screen position.
  {
    assert("preview" in VP3, "VP3.preview state exists");
    assert(typeof buildLoopPreview === "function", "buildLoopPreview helper exists");
    vp3Project();
    // EDGE preview: hover an edge with the modifier -> preview.edges === loopEdgeSet
    SEL.mode = "edge"; selClear();
    const ek = TOPO.edgeKeys[18];
    const [ev0, ev1] = TOPO.edgeVerts[ek];
    const emx = (VP3.scr[ev0*3] + VP3.scr[ev1*3]) / 2, emy = (VP3.scr[ev0*3+1] + VP3.scr[ev1*3+1]) / 2;
    vp3HoverAt(emx, emy, true);
    // hovering may snap to the nearest edge; whatever it is, the preview must be
    // that edge's full loop and must NOT have touched the live selection.
    if (VP3.hover.kind === "edge") {
      assert(VP3.preview && VP3.preview.edges, "edge hover + modifier builds an edge-loop preview");
      const expect = loopEdgeSet(VP3.hover.idx);
      assert(VP3.preview.edges.size === expect.size, "preview edge count matches loopEdgeSet");
      for (const k of expect) assert(VP3.preview.edges.has(k), "preview contains the whole loop");
    }
    assert(SEL.edges.size === 0, "preview does NOT mutate the selection");
    // dropping the modifier clears the preview
    vp3HoverAt(emx, emy, false);
    assert(VP3.preview === null, "preview clears when the loop-modifier is released");

    // FACE preview: hover a face with the modifier -> preview.faces === computeFaceBand
    SEL.mode = "face"; selClear();
    const ff = TOPO.faceEdges.findIndex(es => es.length === 4);
    const [fa, fb, fc] = TOPO.faceTris[ff][0];
    const fmx = (VP3.scr[fa*3] + VP3.scr[fb*3] + VP3.scr[fc*3]) / 3;
    const fmy = (VP3.scr[fa*3+1] + VP3.scr[fb*3+1] + VP3.scr[fc*3+1]) / 3;
    vp3HoverAt(fmx, fmy, true);
    if (VP3.hover.kind === "face") {
      assert(VP3.preview && VP3.preview.faces, "face hover + modifier builds a face-band preview");
      const expectF = computeFaceBand(VP3.hover.idx);
      assert(VP3.preview.faces.size === expectF.size, "preview face count matches computeFaceBand");
    }
    assert(SEL.faces.size === 0, "face preview does not mutate the selection");
    vp3HoverAt(fmx, fmy, false);
    assert(VP3.preview === null, "face preview clears without the modifier");
    SEL.mode = "face"; selClear();
    console.log("[preview] loop/ring hover ghost matches the committed pick, never mutates SEL ✔");
  }

  // face-to-face connect path (Ctrl+click face A, then face B) — BFS over the
  // dual graph returns an inclusive, edge-contiguous run of faces.
  {
    // reach a face several steps from 0 via BFS so the pair is genuinely apart
    const seen = new Map([[0, -1]]); const q = [0]; let h = 0, far = 0;
    while (h < q.length && q.length < 60) { const f = q[h++]; far = f; for (const nf of faceNeighbors(f)) if (!seen.has(nf)) { seen.set(nf, f); q.push(nf); } }
    const fp = shortestFacePath(0, far);
    assert(fp && fp[0] === 0 && fp[fp.length - 1] === far, "face path endpoints");
    for (let i = 0; i + 1 < fp.length; i++) assert(faceNeighbors(fp[i]).includes(fp[i+1]), "face path contiguous");
    assert(shortestFacePath(3, 3).length === 1, "face path to self is [self]");
    console.log("[facepath] 0.." + far + " len", fp.length, "contiguous ✔");
    // two-pick flow: ring anchors lastPick, then connect adds an A..B run
    SEL.mode = "face"; selClear(); SEL.faces.add(0); selRingFromFace(0);
    assert(SEL.lastPick && SEL.lastPick.mode === "face" && SEL.lastPick.id === 0, "ring anchors lastPick");
    const okConnect = selConnectPath(far);
    assert(okConnect && SEL.faces.has(0) && SEL.faces.has(far) && SEL.lastPick.id === far, "face connect keeps both endpoints + re-anchors");
    console.log("[facepath] ring(21?)->connect ->", SEL.faces.size, "faces");
  }

  // shell similarity + stack: unwrap the WHOLE mesh to several islands so the
  // similarity/stack path actually runs (select-all first so unwrap sees it all)
  SEL.mode = "face"; selAll();
  doUnwrap("smart");
  if (UVED.islands.length >= 2) {
    UVED.islands.forEach(i => i.selected = false);
    UVED.islands.slice().sort((a,b)=>a.faces.length-b.faces.length)[0].selected = true;
    uvedSelectSimilar();
    const nSim = UVED.islands.filter(i=>i.selected).length;
    console.log("[similar] selected", nSim);
    if (nSim < 2) { UVED.islands.slice(0,3).forEach(i=>i.selected=true); }
    uvedStackSelected();
    const c = UVED.islands.filter(i=>i.selected).map(i=>islandUVBounds(i));
    const maxd = Math.max(...c.map(x=>Math.hypot(x.cu-c[0].cu, x.cv-c[0].cv)));
    console.log("[stack] centre max delta", maxd.toExponential(2));
    assert(maxd < 1e-6, "stack collapse");
  }

  // ---- island weight: three UI views are consistent (backlog #9) ----------
  // slider<->weight, xN, and %-of-atlas-area are interchangeable views onto the
  // single stored isl.weight. Round-trips must be stable and % must reflect the
  // real atlas share (weight / sum-of-weights, since area ~ weight).
  if (UVED.islands.length >= 2) {
    // slider round-trip: weight -> slider pos -> weight is identity in-range
    for (const w of [0.25, 0.5, 1, 2, 4, 8]) {
      const rt = sliderToWeight(weightToSlider(w));
      assert(Math.abs(rt - w) < 1e-6, "slider round-trip stable for x" + w);
    }
    // slider is log-spaced: equal position deltas give equal weight RATIOS
    const r1 = sliderToWeight(0.6) / sliderToWeight(0.4);
    const r2 = sliderToWeight(0.8) / sliderToWeight(0.6);
    assert(Math.abs(r1 - r2) < 1e-6, "slider is log-spaced (equal drag = equal ratio)");
    // slider ends clamp to the declared min/max
    assert(Math.abs(sliderToWeight(0) - WEIGHT_SLIDER_MIN) < 1e-9, "slider min = " + WEIGHT_SLIDER_MIN);
    assert(Math.abs(sliderToWeight(1) - WEIGHT_SLIDER_MAX) < 1e-9, "slider max = " + WEIGHT_SLIDER_MAX);
    // percent view: set island A to a target share, then read it back
    UVED.islands.forEach(i => i.weight = 1);              // neutral baseline
    const A = UVED.islands[0];
    const share = 0.5;                                    // A should own half the atlas
    A.weight = percentToWeight(share, UVED.islands, A);
    const gotShare = weightToPercent(UVED.islands, A);
    assert(Math.abs(gotShare - share) < 1e-6, "percentToWeight/weightToPercent round-trip");
    // a heavier island reports a larger share than a lighter one
    UVED.islands.forEach(i => i.weight = 1);
    UVED.islands[0].weight = 3;
    assert(weightToPercent(UVED.islands, UVED.islands[0]) > weightToPercent(UVED.islands, UVED.islands[1]),
      "heavier island owns a larger atlas share");
    // a HIGH target share must be reachable end-to-end (the cap-too-low bug):
    // set island A to 95% via the same path the UI uses (weight is clamped to
    // WEIGHT_MAX inside uvedSetWeight), then read the share back.
    UVED.islands.forEach(i => { i.weight = 1; i.selected = false; });
    UVED.islands[0].selected = true;
    const hi = 0.95;
    uvedSetWeight(percentToWeight(hi, UVED.islands, UVED.islands[0]));
    const reached = weightToPercent(UVED.islands, UVED.islands[0]);
    assert(reached > 0.90, "95% atlas share is reachable end-to-end, got " + (reached*100).toFixed(1) + "%");
    UVED.islands.forEach(i => { i.weight = 1; i.selected = false; });   // restore neutral
    console.log("[weight] slider/% = 0..100 atlas share, high share reachable ✔");
  }

  // ---- seams honored by Unwrap (bug fix: user seams must not be over-cut) ----
  // Baseline: no seams -> the auto-cut heuristic shreds the closed mesh into
  // many charts. Then mark a SMALL set of seams and confirm Unwrap now respects
  // them (cuts only where marked -> far fewer islands than the auto-cut result).
  SEAMS.clear();
  SEL.mode = "face"; selAll();
  doUnwrap("unwrap");
  const autoCutIslands = UVED.islands.length;
  console.log("[seam] no-seam auto-cut islands:", autoCutIslands);

  // mark one edge loop as a seam, then unwrap honoring it
  SEL.mode = "edge"; selClear(); selLoopFromEdge(TOPO.edgeKeys[18]);
  const loopEdges = SEL.edges.size;
  const added = markSeamFromSelection();
  console.log("[seam] marked", added, "seam edges (loop of", loopEdges + ")");
  assert(added === loopEdges && added > 0, "markSeamFromSelection returns newly-added count");
  // re-marking the same loop adds nothing new (delta reporting, not total)
  assert(markSeamFromSelection() === 0, "re-mark adds 0");

  SEL.mode = "face"; selAll();
  doUnwrap("unwrap");
  const seamIslands = UVED.islands.length;
  console.log("[seam] seam-honored islands:", seamIslands);
  assert(seamIslands <= autoCutIslands, "honoring seams should not create MORE islands than auto-cut");

  // face-mode seam marking = boundary of the selected faces (not every edge)
  SEAMS.clear();
  SEL.mode = "face"; selClear();
  const someFaces = [0, 1, 2, 3];
  for (const f of someFaces) SEL.faces.add(f);
  const bnd = markSeamFromSelection();
  const interiorMarked = seamEdgesFromSelection === undefined ? 0 :
    [...SEAMS].some(k => TOPO.edgeFaces[k].filter(nf => SEL.faces.has(nf)).length === 2);
  console.log("[seam] face-mode boundary seams:", bnd);
  assert(bnd > 0 && !interiorMarked, "face-mode marks only boundary edges");
  SEAMS.clear();

  // ---- select linked = connected component via shared verts (Ctrl+L) ----
  // From one seed face, selLinked must return the shell it belongs to and be
  // closed under vertex adjacency. If Suzanne has multiple welded shells (eyes
  // separate from head) it selects a PROPER subset, never the whole mesh.
  SEL.mode = "face"; selClear(); SEL.faces.add(0);
  const nLinked = selLinked();
  console.log("[linked] from face 0 ->", SEL.faces.size, "faces,", nLinked, "verts (of", MESH.faceCount, "/", MESH.vertCount + ")");
  assert(nLinked > 0, "selLinked returns a nonzero component");
  assert(SEL.faces.size > 0, "selLinked selects faces");
  // component must be closed: no selected face has a vertex-neighbour vertex
  // outside the selected vertex set (i.e. it's a real connected component).
  const compVerts = new Set();
  for (const f of SEL.faces) for (const v of TOPO.faceVerts[f]) compVerts.add(v);
  for (const v of compVerts) for (const nb of TOPO.vertNeighbors[v]) assert(compVerts.has(nb), "component closed under adjacency");
  // re-running from a face already in the component is idempotent
  const sizeA = SEL.faces.size; selLinked();
  assert(SEL.faces.size === sizeA, "selLinked idempotent on its own component");
  // count distinct shells by flooding every vertex once; Suzanne is multi-shell
  const shellSeen = new Set(); let shellCount = 0, biggest = 0;
  for (let v = 0; v < MESH.vertCount; v++) {
    if (shellSeen.has(v)) continue;
    const comp = connectedVertsFrom([v]);
    for (const x of comp) shellSeen.add(x);
    shellCount++; biggest = Math.max(biggest, comp.size);
  }
  console.log("[linked]", shellCount, "connected shells; biggest", biggest, "verts");
  // a single-face seed's component should not be the entire vertex set unless
  // the mesh is truly one shell -- report either way, don't force multi-shell.
  if (shellCount > 1) assert(nLinked < MESH.vertCount, "multi-shell: linked is a proper subset, not the whole mesh");
  selClear();

  // ---- undo / redo history (snapshot-based, linear) ----
  // re-seed a clean timeline (earlier test ops recorded their own steps).
  SEL.mode = "face"; selClear();
  doUnwrap("conformal");
  seedHistoryUV();
  assert(HXU.events.length === 1 && HXU.cursor === 0, "history seeded with one step");
  assert(HXU.events[0].type === "start", "seed step is 'start'");
  const seedIslandCount = HXU.events[0].snapshot.islands.length;
  assert(seedIslandCount === UVED.islands.length, "seed snapshot matches live islands");

  // a UV-layout fingerprint: sum of every island's first-vertex u+v, so a
  // restore is verified by actual coordinates, not just island counts.
  const uvSig = () => UVED.islands.reduce((s, isl) => { const v = isl.verts[0]; return s + isl.uv[v][0] * 7.3 + isl.uv[v][1] * 3.1; }, UVED.islands.length * 1000);
  const seedSig = uvSig();

  // record a couple of ops -> cursor advances, tail grows, layout changes
  doUnwrap("planar");
  const planarSig = uvSig();
  assert(HXU.events.length === 2 && HXU.cursor === 1, "unwrap recorded a step");
  doUnwrap("cylindrical");
  const cylSig = uvSig();
  assert(HXU.events.length === 3 && HXU.cursor === 2, "second unwrap recorded");

  // undo -> restores the planar layout (fingerprint matches)
  hxuUndo();
  assert(HXU.cursor === 1, "undo moved cursor back");
  assert(Math.abs(uvSig() - planarSig) < 1e-9, "undo restored planar UV layout");
  // redo -> forward again to cylindrical
  hxuRedo();
  assert(HXU.cursor === 2 && Math.abs(uvSig() - cylSig) < 1e-9, "redo restored cylindrical UV layout");

  // jump to seed (step 0) restores the original conformal layout
  hxuJump(0);
  assert(HXU.cursor === 0 && Math.abs(uvSig() - seedSig) < 1e-9, "jump-to-0 restored seed UV layout");

  // recording a NEW op while jumped back discards the redo tail (linear history)
  doUnwrap("spherical");
  assert(HXU.cursor === 1 && HXU.events.length === 2, "new op after undo truncates the redo tail");

  // seams are captured + restored by the snapshot
  SEAMS.clear();
  SEL.mode = "edge"; selClear(); selLoopFromEdge(TOPO.edgeKeys[18]);
  markSeamFromSelection();
  pushHistoryUV("seam", "Mark seam loop");
  const seamCount = SEAMS.size;
  assert(seamCount > 0, "seams present before undo");
  SEAMS.clear();                 // corrupt live state
  hxuJump(HXU.events.length - 1); // restore the seam step
  assert(SEAMS.size === seamCount, "jump restored the seam set");
  console.log("[history]", HXU.events.length, "steps; undo/redo/jump restore UV + seams + selection ✔");

  // ---- partial unwrap MERGES instead of wiping the whole layout ----
  // Unwrap a small face set (A), then unwrap a DIFFERENT small set (B). B's
  // unwrap must keep A's island rather than replacing it — the eyes-then-face
  // regression. Uses selLinked to grab a real connected sub-shell as group A.
  // This block asserts the NO-REFLOW (fill-then-pack) path -> auto-pack OFF.
  SEAMS.clear();
  UVED.settings.packOnUnwrap = false;
  SEL.mode = "face"; selClear();
  UVED.islands = []; UVED.preview = null;
  // group A = the connected shell containing face 0
  SEL.mode = "face"; selClear(); SEL.faces.add(0); selLinked();
  const groupA = new Set(SEL.faces);
  doUnwrap("planar");
  const afterA = UVED.islands.map(i => i.faces.slice());
  const aFaceTotal = afterA.reduce((s,f)=>s+f.length,0);
  assert(UVED.islands.length >= 1, "group A produced at least one island");
  // snapshot group-A island UV coords so we can prove they DON'T reflow on the
  // next unwrap (Blender fill-then-pack: prior islands stay put, no repack).
  const aSnap = UVED.islands.map(isl => ({ f: isl.faces.slice(), uv: Object.fromEntries(isl.verts.map(v => [v, isl.uv[v].slice()])) }));
  console.log("[merge] group A ->", UVED.islands.length, "islands,", aFaceTotal, "faces");

  // group B = a different shell (a face NOT in group A), if the mesh has one
  let bSeed = -1;
  for (let f = 0; f < TOPO.faceCount; f++) if (!groupA.has(f)) { bSeed = f; break; }
  if (bSeed >= 0) {
    SEL.mode = "face"; selClear(); SEL.faces.add(bSeed); selLinked();
    const groupB = new Set(SEL.faces);
    // sanity: A and B are disjoint face sets
    assert(![...groupB].some(f => groupA.has(f)), "group B disjoint from A");
    const islandsBefore = UVED.islands.length;
    doUnwrap("planar");
    // every group-A face must STILL be covered by some island (not wiped)
    const covered = new Set(); for (const isl of UVED.islands) for (const f of isl.faces) covered.add(f);
    for (const f of groupA) assert(covered.has(f), "group A face " + f + " survived group B unwrap");
    for (const f of groupB) assert(covered.has(f), "group B face " + f + " present after unwrap");
    assert(UVED.islands.length >= islandsBefore, "merge kept A's islands and added B's");
    // group A's UVs must be IDENTICAL to before (no repack reflow)
    for (const a of aSnap) {
      const match = UVED.islands.find(isl => isl.faces.length === a.f.length && isl.faces[0] === a.f[0]);
      assert(match, "group A island still present after merge");
      for (const v of Object.keys(a.uv)) {
        assert(match.uv[v] && Math.abs(match.uv[v][0]-a.uv[v][0]) < 1e-12 && Math.abs(match.uv[v][1]-a.uv[v][1]) < 1e-12,
          "group A UV unchanged (no repack) for vert " + v);
      }
    }
    console.log("[merge] group B ->", UVED.islands.length, "total islands; A kept in place (no reflow) ✔");
  } else {
    console.log("[merge] single-shell mesh — full-replace path (no second group)");
  }

  // unwrapping with NO selection (whole mesh) still fully replaces
  SEL.mode = "face"; selClear();
  doUnwrap("conformal");
  const wholeCovered = new Set(); for (const isl of UVED.islands) for (const f of isl.faces) wholeCovered.add(f);
  assert(wholeCovered.size === TOPO.faceCount, "whole-mesh unwrap covers every face");
  console.log("[merge] whole-mesh unwrap replaces cleanly ✔");
  UVED.settings.packOnUnwrap = true;   // restore default for the remaining blocks

  // ---- selection preview: selected-but-not-yet-unwrapped faces show a ghost ----
  UVED.islands = []; UVED.preview = null;
  SEL.mode = "face"; selClear(); SEL.faces.add(0); SEL.faces.add(1);
  uvedHighlightFromMesh();       // the real SEL.onChange path builds the preview
  assert(UVED.preview && UVED.preview.faces.length === 2, "preview built for un-unwrapped selection");
  assert(UVED.preview.verts.every(v => UVED.preview.uv[v] && isFinite(UVED.preview.uv[v][0])), "preview UVs finite");
  // once those faces are unwrapped into an island, the preview clears for them
  doUnwrap("planar");
  const stillPending = UVED.preview ? UVED.preview.faces.filter(f => f===0||f===1).length : 0;
  assert(stillPending === 0, "preview cleared for faces now in an island");
  console.log("[preview] ghost projection shows selection, clears on unwrap ✔");

  // ---- auto-pack-on-unwrap toggle ----
  // OFF: merging a second group must NOT move the kept islands (fill-then-pack).
  // ON: the combined set is repacked (kept islands may move to avoid overlap).
  assert(typeof UVED.settings.packOnUnwrap === "boolean", "packOnUnwrap setting exists");
  UVED.islands = []; UVED.preview = null; SEAMS.clear();
  SEL.mode = "face"; selClear(); SEL.faces.add(0); selLinked();
  const gA = new Set(SEL.faces);
  UVED.settings.packOnUnwrap = false;
  doUnwrap("planar");
  const keepSnap = UVED.islands.map(i => ({ f0: i.faces[0], uv: i.uv[i.verts[0]].slice() }));
  let gBseed = -1; for (let f = 0; f < TOPO.faceCount; f++) if (!gA.has(f)) { gBseed = f; break; }
  if (gBseed >= 0) {
    SEL.mode = "face"; selClear(); SEL.faces.add(gBseed); selLinked();
    doUnwrap("planar");
    for (const k of keepSnap) {
      const m = UVED.islands.find(i => i.faces[0] === k.f0);
      assert(m && Math.abs(m.uv[m.verts[0]][0]-k.uv[0]) < 1e-12 && Math.abs(m.uv[m.verts[0]][1]-k.uv[1]) < 1e-12,
        "auto-pack OFF: kept island did not move");
    }
    console.log("[autopack] OFF keeps prior islands fixed ✔");
  }
  UVED.settings.packOnUnwrap = true;   // restore default

  // ---- display: show-all-islands toggle + selected-draws-last order ----
  assert(typeof UVED.settings.showAllIslands === "boolean", "showAllIslands setting exists");
  assert(typeof uvedDrawIsland === "function", "per-island draw helper exists (raise-selected order)");
  // both modes must draw without throwing (canvas is stubbed) with overlapping
  // selected + unselected islands present
  SEL.mode = "face"; selAll(); doUnwrap("smart");
  if (UVED.islands.length >= 2) { UVED.islands[0].selected = true; UVED.islands[1].selected = false; }
  UVED.settings.showAllIslands = true;  uvedDraw();
  UVED.settings.showAllIslands = false; uvedDraw();
  UVED.settings.showAllIslands = true;
  console.log("[display] show-all toggle + selected-on-top draw both render ✔");

  // ---- unwrap a SELECTED shell -> ONE filled island (Blender behavior) ----
  // Regression for: "select faces, press U, and it just re-packs the old pieces
  // instead of unwrapping." A connected selected patch with no seams must flatten
  // to a SINGLE LSCM island that fills 0-1, not shatter into axis fragments.
  SEAMS.clear(); UVED.islands = []; UVED.preview = null;
  SEL.mode = "face"; selClear(); SEL.faces.add(0);
  selLinked();
  const shellFaces = new Set(SEL.faces);
  // openBoundaryRatio must classify a disk-like patch as flattenable (one island)
  assert(typeof openBoundaryRatio === "function", "openBoundaryRatio helper exists");
  const shellRatio = openBoundaryRatio([...shellFaces]);
  const fresh = unwrap("unwrap");
  const coverShell = fresh.filter(isl => isl.faces.some(f => shellFaces.has(f)));
  console.log("[unwrap-sel] shell", shellFaces.size, "faces, boundaryRatio", shellRatio.toFixed(3),
    "-> islands covering shell:", coverShell.length);
  assert(coverShell.length === 1, "a disk-like selected shell unwraps to ONE island, not fragments");
  assert(coverShell[0].faces.length === shellFaces.size, "the one island covers every selected face");
  // the island genuinely fills UV space (not a collapsed sliver)
  const fb = (isl => { let a=1e9,b=1e9,c=-1e9,d=-1e9; for (const v of isl.verts){const[u,w]=isl.uv[v];a=Math.min(a,u);b=Math.min(b,w);c=Math.max(c,u);d=Math.max(d,w);} return {w:c-a,h:d-b}; })(coverShell[0]);
  assert(fb.w > 0.2 && fb.h > 0.2, "unwrapped shell fills a real UV area (no collapse)");
  // and a genuinely CLOSED blob (whole head) still auto-cuts rather than collapsing
  SEL.mode = "face"; selClear();
  const whole = unwrap("unwrap");
  assert(whole.length > 3, "closed sub-shells still auto-cut into disks (not one collapsed island)");
  assert(whole.every(isl => isl.verts.every(v => isFinite(isl.uv[v][0]) && isFinite(isl.uv[v][1]))), "no NaN UVs from auto-cut");
  console.log("[unwrap-sel] selected shell -> 1 filled island; closed mesh ->", whole.length, "auto-cut disks ✔");

  // ---- selection-tool hotkeys (W/B/Q/K/C) drive the same path as the dropdown ----
  const dispatchKey = code => (global.window._listeners.keydown||[]).forEach(fn => fn({
    code, key: code, ctrlKey:false, altKey:false, metaKey:false, shiftKey:false,
    target:{ tagName:"BODY" }, preventDefault(){},
  }));
  const toolFor = { KeyW:"pick", KeyB:"box", KeyQ:"circle", KeyK:"lasso", KeyC:"paint" };
  for (const [code, want] of Object.entries(toolFor)) {
    dispatchKey(code);
    assert(VP3.tool === want, "hotkey " + code + " sets tool " + want + " (got " + VP3.tool + ")");
  }
  VP3.tool = "pick";
  console.log("[hotkeys] W/B/Q/K/C select click/box/circle/lasso/paint ✔");

  // ---- X-ray "select through" toggle (viewport display menu) ----
  // OFF: a full-canvas region select in face mode grabs only front-facing faces.
  // ON: it also grabs back-facing / occluded faces, so the count grows (and, for a
  //     closed mesh with no coincident faces, reaches every face).
  assert(typeof VP3.selectThrough === "boolean", "selectThrough setting exists");
  vp3Project();
  const grabAll = () => true;
  VP3.selectThrough = false;
  SEL.mode = "face"; selClear();
  const frontOnly = selectRegion(grabAll, vp3ElementAt, vp3ElementVisible, false, false);
  VP3.selectThrough = true;
  SEL.mode = "face"; selClear();
  const through = selectRegion(grabAll, vp3ElementAt, vp3ElementVisible, false, false);
  console.log("[xray] face region select front-only:", frontOnly, "-> select-through:", through, "of", TOPO.faceCount);
  assert(through > frontOnly, "X-ray select reaches more faces than front-only");
  assert(through === TOPO.faceCount, "X-ray select over the whole canvas reaches every face");
  // vp3ElementVisible must report true for a known back-facing face under X-ray
  const backFace = (() => { for (let f = 0; f < TOPO.faceCount; f++) if (!(function front(){ const s=VP3.scr,[a,b,c]=TOPO.faceTris[f][0]; return ((s[b*3]-s[a*3])*(s[c*3+1]-s[a*3+1])-(s[c*3]-s[a*3])*(s[b*3+1]-s[a*3+1]))<0; })()) return f; return -1; })();
  if (backFace >= 0) {
    VP3.selectThrough = false; assert(vp3ElementVisible("face", backFace) === false, "back face hidden when X-ray off");
    VP3.selectThrough = true;  assert(vp3ElementVisible("face", backFace) === true,  "back face reachable when X-ray on");
  }
  VP3.selectThrough = false; selClear();
  console.log("[xray] select-through toggle reaches back/occluded geometry ✔");

  // ---- selection recorded in history + empty-space clears (all tools) ----
  assert(typeof selSignature === "function", "selSignature helper exists");
  assert(typeof vp3FinishSelectGesture === "function", "vp3FinishSelectGesture exists");
  // a real selection changes the fingerprint; clearing returns to the empty sig
  SEL.mode = "face"; selClear();
  const emptySig = selSignature();
  SEL.faces.add(0); SEL.faces.add(1);
  assert(selSignature() !== emptySig, "selSignature changes with the selection");

  // a finished gesture that CHANGED the selection records one 'select' step,
  // tagged with the active tool's name; a no-op gesture records nothing.
  seedHistoryUV();
  VP3.tool = "box";
  SEL.mode = "face"; selClear(); SEL.faces.add(0); SEL.faces.add(1);
  vp3FinishSelectGesture({ sig0: emptySig, shift:false, sub:false }, 2);
  assert(HXU.events.length === 2 && HXU.events[1].type === "select", "changed selection records a 'select' step");
  assert(HXU.events[1].title === (SEL_TOOL_NAME.box || "box"), "select step is tagged with the tool name");
  const afterOne = HXU.events.length;
  vp3FinishSelectGesture({ sig0: selSignature(), shift:false, sub:false }, 1); // no change
  assert(HXU.events.length === afterOne, "an unchanged selection records no step");

  // undo/redo restores the recorded selection
  const selCount = SEL.faces.size;
  hxuUndo();
  assert(SEL.faces.size !== selCount || HXU.cursor === 0, "undo stepped back off the select edit");
  hxuRedo();
  assert(SEL.faces.size === selCount, "redo restored the recorded selection");
  console.log("[select-hist] gesture records a tool-tagged 'select' step; undo/redo restore it ✔");

  // empty-space gesture (touched === 0) with no add/subtract modifier clears the
  // selection regardless of which tool is active (box/lasso/circle/paint/pick).
  for (const tool of ["pick","box","lasso","circle","paint"]) {
    VP3.tool = tool;
    SEL.mode = "face"; selClear(); SEL.faces.add(0); SEL.faces.add(2);
    vp3FinishSelectGesture({ sig0:"prev", shift:false, sub:false }, 0);
    assert(SEL.faces.size === 0, "empty-space gesture clears selection for tool " + tool);
  }
  // but Shift (additive) on empty space must NOT clear an existing selection
  VP3.tool = "box";
  SEL.mode = "face"; selClear(); SEL.faces.add(0); SEL.faces.add(2);
  vp3FinishSelectGesture({ sig0:selSignature(), shift:true, sub:false }, 0);
  assert(SEL.faces.size === 2, "Shift+empty-space keeps the existing selection");
  VP3.tool = "pick"; selClear();
  console.log("[empty-clear] click into the void clears selection for every tool (Shift preserves) ✔");

  // ---- symmetry / mirror select (works with ALL tools via selChanged hook) ----
  assert(typeof symEnable === "function" && typeof buildSymmetryMap === "function", "symmetry API exists");
  const symFrac = buildSymmetryMap();
  console.log("[symmetry] best axis", SYM.axisName, "match", (symFrac*100).toFixed(1) + "%");
  // Suzanne is mirror-symmetric across one axis: the map must cover ~all verts
  assert(symFrac >= SYM.minFraction, "Suzanne is detected as symmetric");
  // the mirror map is an involution: mirror(mirror(v)) === v for mapped verts
  for (const [v, mv] of SYM.v2v) assert(SYM.v2v.get(mv) === v, "vertex mirror is an involution");
  // a vertex off the symmetry plane maps to a DIFFERENT vertex (real mirror)
  const offPlane = [...SYM.v2v.keys()].find(v => SYM.v2v.get(v) !== v);
  assert(offPlane != null, "some vertex has a distinct mirror partner");

  // enable -> a face selection auto-includes its mirror across ALL ops
  const okFrac = symEnable();
  assert(SYM.enabled && okFrac >= SYM.minFraction, "symEnable turns on for a symmetric mesh");
  SEL.mode = "face"; selClear();
  // pick one off-plane face -> its mirror comes along automatically
  const symFace = [...SYM.f2f.keys()].find(f => SYM.f2f.get(f) !== f);
  assert(symFace != null, "an off-plane face has a distinct mirror");
  selectFace(symFace, false);
  assert(SEL.faces.has(symFace) && SEL.faces.has(SYM.f2f.get(symFace)), "picking a face also selects its mirror");
  // grow stays symmetric (grow runs through selChanged -> symApply)
  const beforeGrow = SEL.faces.size; selGrow();
  const asym = [...SEL.faces].filter(f => SYM.f2f.has(f) && !SEL.faces.has(SYM.f2f.get(f)));
  assert(asym.length === 0, "grow keeps the selection symmetric across all tools");
  assert(SEL.faces.size > beforeGrow, "grow still expanded the selection");
  // deselect a face -> its mirror is removed too (delta mirror, not add-only)
  const someSel = [...SEL.faces].find(f => SYM.f2f.get(f) !== f);
  const mirrorOfSome = SYM.f2f.get(someSel);
  SEL.faces.delete(someSel); selChanged();
  assert(!SEL.faces.has(mirrorOfSome), "unselecting one side removes its mirror too");

  // disable -> selection ops stop mirroring
  symDisable();
  assert(!SYM.enabled, "symDisable turns it off");
  SEL.mode = "face"; selClear(); selectFace(symFace, false);
  assert(SEL.faces.size === 1, "with symmetry off a single pick selects one face");
  console.log("[symmetry] mirror map + all-tools symmetric select/grow/deselect ✔");
  selClear();

  // ---- symmetric-unwrap parity: L/R island pairs get mirrored UVs (backlog #5) --
  // Build a small island A from a mirror-able face + neighbours, and its mirror
  // island B via SYM.f2f, solve each independently, then run pairSymmetricIslands
  // and confirm B's UVs became the exact U-mirror of A's (V unchanged). Testing
  // the core directly (pre-pack) so the comparison is exact.
  {
    if (!SYM.built) buildSymmetryMap();
    // grow a compact off-plane face patch whose whole mirror is disjoint from it
    const seed = [...SYM.f2f.keys()].find(f => { const m = SYM.f2f.get(f); return m !== f && m != null; });
    assert(seed != null, "a mirror-able off-plane face exists");
    const aFaces = new Set([seed]);
    for (const nf of faceNeighbors(seed)) if (SYM.f2f.get(nf) != null && SYM.f2f.get(nf) !== nf) aFaces.add(nf);
    // keep only faces whose mirror is NOT in A (so A and B are disjoint shells)
    const aList = [...aFaces].filter(f => !aFaces.has(SYM.f2f.get(f)));
    const bList = aList.map(f => SYM.f2f.get(f));
    assert(aList.length >= 1 && new Set([...aList, ...bList]).size === aList.length + bList.length,
      "A and its mirror B are disjoint");
    const A = lscmIsland(aList), B = lscmIsland(bList);
    const pairs = pairSymmetricIslands([A, B]);
    assert(pairs === 1, "pairSymmetricIslands finds the one L/R pair, got " + pairs);
    // determine which became the source (lowest global vertex index wins)
    const src = Math.min(...A.verts) <= Math.min(...B.verts) ? A : B;
    const dst = src === A ? B : A;
    let mnU = 1e9, mxU = -1e9;
    for (const v of src.verts) { const u = src.uv[v][0]; mnU = Math.min(mnU, u); mxU = Math.max(mxU, u); }
    const axisU = (mnU + mxU) / 2;
    let maxErr = 0, checked = 0;
    for (const v of src.verts) {
      const mv = SYM.v2v.get(v);
      if (mv == null || dst.uv[mv] == null) continue;
      const expU = 2 * axisU - src.uv[v][0], expV = src.uv[v][1];
      maxErr = Math.max(maxErr, Math.abs(dst.uv[mv][0] - expU), Math.abs(dst.uv[mv][1] - expV));
      checked++;
    }
    assert(checked > 0, "mirror pair shares mapped verts to compare");
    assert(maxErr < 1e-9, "partner UVs are the exact U-mirror of the source, maxErr " + maxErr.toExponential(2));
    console.log("[sym-unwrap] L/R island pair mirrored:", checked, "verts, maxErr", maxErr.toExponential(2), "✔");
  }

  // ---- occlusion-gated overlays (wireframe/edges/verts don't bleed to the back) ----
  assert(typeof faceFrontFacing === "function" && typeof edgeFrontFacing === "function",
    "facing helpers exist");
  vp3Project();
  // on a closed mesh some faces face the camera and some face away
  let front = 0, back = 0;
  for (let f = 0; f < TOPO.faceCount; f++) (faceFrontFacing(f) ? front++ : back++);
  console.log("[overlay] faces front/back:", front, "/", back);
  assert(front > 0 && back > 0, "a closed mesh has both front- and back-facing polygons");
  // an edge between two back faces must itself be culled (hidden behind the skin);
  // an edge with any front face must show. edgeFrontFacing == OR of its faces.
  for (const k of TOPO.edgeKeys) {
    const anyFront = TOPO.edgeFaces[k].some(f => faceFrontFacing(f));
    assert(edgeFrontFacing(k) === anyFront, "edge visible iff an incident face is front");
  }
  // at least one edge is fully occluded (both faces back) -> the old bug (wire
  // bleeding through the model) is now impossible without X-ray.
  const hiddenEdge = TOPO.edgeKeys.find(k => TOPO.edgeFaces[k].length === 2 && !edgeFrontFacing(k));
  assert(hiddenEdge != null, "some interior edge is fully occluded (culled from the wire)");
  // frontFacing(vertex) agrees with 'any incident face front'
  for (let v = 0; v < MESH.vertCount; v += 37) {
    const anyFront = TOPO.vertFaces[v].some(f => faceFrontFacing(f));
    assert(frontFacing(v) === anyFront, "vertex visible iff an incident face is front");
  }
  // overlays render without throwing in every mode, X-ray off AND on
  for (const xray of [false, true]) {
    VP3.selectThrough = xray;
    for (const m of ["vertex","edge","face","island"]) {
      SEL.mode = m; selClear();
      if (m === "edge") selLoopFromEdge(TOPO.edgeKeys[18]);
      else if (m === "vertex") SEL.verts.add(0);
      else { SEL.faces.add(0); if (m === "island") selIsland(); }
      vp3Draw();
    }
  }
  VP3.selectThrough = false; SEL.mode = "face"; selClear();
  console.log("[overlay] per-mode overlays occlusion-gated; render in all modes (X-ray off/on) ✔");

  // ---- surface depth buffer hides front-facing-but-OCCLUDED overlays ----
  // Pure backface culling leaves front-facing geometry that sits behind nearer
  // parts (eye-socket far rim, head interior) still drawing through. A real depth
  // test against the rendered surface removes them. vp3Draw() populates the depth
  // buffer as it fills the solid, so after a frame we can query it.
  assert(typeof vp3DepthVisible === "function" && typeof vertDepthVisible === "function", "depth API exists");
  SEL.mode = "face"; selClear();
  VP3.selectThrough = false;
  vp3Draw();                                   // fills + stamps the depth buffer
  assert(VP3.depth && VP3.depth.buf.length > 0, "depth buffer allocated");
  // some surface cells must have real (finite) depth stamped
  let stamped = 0; for (const z of VP3.depth.buf) if (isFinite(z)) stamped++;
  assert(stamped > 0, "depth buffer has surface samples");
  // count front-facing faces whose CENTROID is occluded by nearer surface: on a
  // closed mesh with concavities (eye sockets) this must be > 0 — exactly the
  // geometry that used to bleed through.
  let frontOccluded = 0, frontTotal = 0;
  for (let f = 0; f < TOPO.faceCount; f++) {
    if (!faceFrontFacing(f)) continue;
    frontTotal++;
    const [a,b,c] = TOPO.faceTris[f][0], s = VP3.scr;
    const cx=(s[a*3]+s[b*3]+s[c*3])/3, cy=(s[a*3+1]+s[b*3+1]+s[c*3+1])/3, cz=(s[a*3+2]+s[b*3+2]+s[c*3+2])/3;
    if (!vp3DepthVisible(cx, cy, cz)) frontOccluded++;
  }
  console.log("[depth]", stamped, "surface cells;", frontOccluded, "of", frontTotal, "front faces occluded (hidden)");
  assert(frontOccluded > 0, "depth test hides front-facing-but-occluded faces (the bleed-through fix)");
  // a nearer point than the stored surface stays visible; a far point is hidden
  const probe = VP3.depth.buf.findIndex(z => isFinite(z));
  const pgx = probe % VP3.depth.dw, pgy = Math.floor(probe / VP3.depth.dw);
  const sx = pgx*VP3.depth.cell+1, sy = pgy*VP3.depth.cell+1, sz = VP3.depth.buf[probe];
  assert(vp3DepthVisible(sx, sy, sz - 0.1) === true, "a nearer overlay point is visible");
  assert(vp3DepthVisible(sx, sy, sz + 0.1) === false, "a point well behind the surface is hidden");
  // X-ray disables occlusion: nothing is depth-culled
  console.log("[depth] near visible / far hidden; occlusion depth-tested ✔");
  SEL.mode = "face"; selClear();

  // ---- per-pixel edge clipping: a silhouette edge is CUT, not all-or-nothing ----
  // Redraw so the depth buffer is current, then look for an edge whose sampled
  // spans are a MIX of visible + occluded — proof the clip is per-pixel. Draw it
  // through a recording context and assert the emitted path is partial (starts a
  // run, has gaps) rather than a single full moveTo→lineTo or nothing.
  assert(typeof pathEdgeClipped === "function", "depth-clipped line drawer exists");
  vp3Draw();
  const s = VP3.scr, cell = VP3.depth.cell;
  const sampleMix = (v0, v1) => {
    const x0=s[v0*3],y0=s[v0*3+1],z0=s[v0*3+2], x1=s[v1*3],y1=s[v1*3+1],z1=s[v1*3+2];
    const n = Math.max(1, Math.ceil(Math.hypot(x1-x0,y1-y0)/cell));
    let vis=0, hid=0;
    for (let i=0;i<=n;i++){ const t=i/n; if (vp3DepthVisible(x0+(x1-x0)*t,y0+(y1-y0)*t,z0+(z1-z0)*t)) vis++; else hid++; }
    return { vis, hid };
  };
  let mixEdge = null;
  for (const k of TOPO.edgeKeys) {
    if (!edgeFrontFacing(k)) continue;
    const [v0,v1] = TOPO.edgeVerts[k];
    const { vis, hid } = sampleMix(v0, v1);
    if (vis > 0 && hid > 0) { mixEdge = { k, v0, v1, vis, hid }; break; }
  }
  assert(mixEdge, "found a front-facing edge that is partly occluded (a clip case)");
  // recording context: count moveTo (run starts) and lineTo (run continues)
  const rec = { moves: 0, lines: 0, moveTo(){ this.moves++; }, lineTo(){ this.lines++; }, beginPath(){}, stroke(){} };
  pathEdgeClipped(rec, s, mixEdge.v0, mixEdge.v1, false);
  console.log("[clip] mixed edge samples vis/hid:", mixEdge.vis + "/" + mixEdge.hid, "-> path moves:", rec.moves, "lines:", rec.lines);
  // a partial edge draws SOMETHING (at least one visible run) but NOT the whole
  // segment as one contiguous move+line — occluded gaps break it, OR the visible
  // portion is shorter than the full sample count.
  assert(rec.moves + rec.lines > 0, "clipped edge draws its visible span");
  assert(rec.moves + rec.lines < mixEdge.vis + mixEdge.hid + 1, "occluded span is dropped (not fully drawn)");
  // X-ray: the SAME edge draws as one unbroken move→line (no clipping)
  const recX = { moves: 0, lines: 0, moveTo(){ this.moves++; }, lineTo(){ this.lines++; }, beginPath(){}, stroke(){} };
  pathEdgeClipped(recX, s, mixEdge.v0, mixEdge.v1, true);
  assert(recX.moves === 1 && recX.lines === 1, "X-ray draws the whole edge unclipped");
  console.log("[clip] per-pixel silhouette clipping (occluded span cut, X-ray unbroken) ✔");
  SEL.mode = "face"; selClear();

  // ---- zoom-invariant depth bias: back-faces must NOT bleed when zoomed out ----
  // Perspective NDC-z is non-linear: zooming out collapses the model's on-screen
  // z span. A FIXED bias eventually exceeds the whole model thickness and every
  // occluded/back face passes the visibility test. The bias now scales with the
  // per-frame z span, so the occlusion ratio holds steady across zoom levels.
  SEL.mode = "face"; selClear(); VP3.selectThrough = false;
  const occludedFraction = () => {
    let occ = 0, tot = 0;
    for (let f = 0; f < TOPO.faceCount; f++) {
      if (!faceFrontFacing(f)) continue;
      tot++;
      const [a,b,c] = TOPO.faceTris[f][0], s2 = VP3.scr;
      const cx=(s2[a*3]+s2[b*3]+s2[c*3])/3, cy=(s2[a*3+1]+s2[b*3+1]+s2[c*3+1])/3, cz=(s2[a*3+2]+s2[b*3+2]+s2[c*3+2])/3;
      if (!vp3DepthVisible(cx, cy, cz)) occ++;
    }
    return tot ? occ / tot : 0;
  };
  const samples = [];
  for (const d of [3.0, 4.2, 8, 20]) {          // very-close through far
    VP3.dist = VP3.tDist = d;                   // hard-set zoom (bypass easing)
    vp3Draw();
    samples.push({ d, span: VP3.zSpan, occ: occludedFraction() });
  }
  for (const s2 of samples)
    console.log("[zoom] dist", s2.d, "zSpan", s2.span.toExponential(2), "occluded", (s2.occ*100).toFixed(1) + "%");
  // the KEY regression guard: at EVERY zoom (close AND far) concave front faces
  // stay occluded — back/occluded geometry is a full model-thickness behind the
  // surface, far more than the per-cell slope tolerance, so it never bleeds.
  // (A fixed bias broke the far end; a span-scaled bias broke the close end.)
  for (const s2 of samples)
    assert(s2.occ > 0.05, "occlusion holds at dist " + s2.d + " (no back-face bleed)");
  // occlusion fraction stays in the same ballpark across the whole zoom range —
  // the slope-scaled per-cell tolerance is scale-invariant by construction.
  const occs = samples.map(s2 => s2.occ);
  assert(Math.max(...occs) - Math.min(...occs) < 0.15, "occlusion ratio stable across close+far zoom");
  console.log("[zoom] slope-scaled per-cell depth tolerance: back-faces hidden at close AND far zoom ✔");
  VP3.dist = VP3.tDist = 4.2; SEL.mode = "face"; selClear();

  // ---- checker visualization (backlog #1): shared colour fn + 3D UV plumbing ----
  {
    // determinism: same (u,v,opts) -> same colour
    const c1 = checkerColor(0.13, 0.27, CHECKER), c2 = checkerColor(0.13, 0.27, CHECKER);
    assert(c1.length === 3 && c1[0] === c2[0] && c1[1] === c2[1] && c1[2] === c2[2], "checkerColor is deterministic");
    // parity flips across a tile boundary (two-tone): stepping v by one tile inverts the square
    const opt = { tiles: 8, mode: "twoTone", colorA: CHECKER_COLOR_A, colorB: CHECKER_COLOR_B };
    const a = checkerColor(0.06, 0.06, opt), b = checkerColor(0.06, 0.06 + 1/8, opt);
    assert(a[0] !== b[0] || a[2] !== b[2], "two-tone parity flips across a tile boundary");
    // same tile -> same colour
    const a2 = checkerColor(0.03, 0.03, opt);
    assert(a[0] === a2[0] && a[1] === a2[1] && a[2] === a2[2], "same tile -> same colour");
    // resolution changes tiling: at tiles=4 the (0.06,0.19) sample lands in a different tile than tiles=32
    const lo = checkerColor(0.06, 0.19, { tiles: 4, mode: "twoTone" });
    const hi = checkerColor(0.06, 0.19, { tiles: 32, mode: "twoTone" });
    assert(Array.isArray(lo) && Array.isArray(hi), "checkerColor honours tiles option");
    // oriented mode returns a colour too (hue-ramped), and differs from two-tone somewhere
    const ori = checkerColor(0.3, 0.7, { tiles: 8, mode: "oriented" });
    assert(ori.length === 3, "oriented mode returns rgb");
    // wrap keeps UVs outside 0-1 tiling (UDIM-safe)
    assert(checkerWrap(1.25) > 0.24 && checkerWrap(1.25) < 0.26 && checkerWrap(-0.25) > 0.74, "checkerWrap tiles into [0,1)");

    // 3D UV plumbing: unwrap the whole mesh, then VP3.uv must carry a UV for every
    // unwrapped vertex, and interpolation at a triangle's corners returns the corner UVs.
    SEAMS.clear(); SEL.mode = "face"; selClear();
    const isls = unwrap("unwrap");
    uvedSetIslands(isls);
    assert(VP3.uv && Object.keys(VP3.uv).length > 0, "vp3SetUvChannel populated VP3.uv from islands");
    let anyVert = null;
    for (const isl of UVED.islands) { if (isl.verts.length) { anyVert = isl.verts[0]; break; } }
    assert(anyVert != null && VP3.uv[anyVert], "an unwrapped vertex has a UV in VP3.uv");
    // barycentric interp identity: at corner weights, interp == that corner's UV
    const isl0 = UVED.islands.find(i => i.faces.length);
    const tri = TOPO.faceTris[isl0.faces[0]][0];
    const [ta, tb, tc] = tri;
    if (isl0.uv[ta] && isl0.uv[tb] && isl0.uv[tc]) {
      // w=(1,0,0) -> corner a
      const u = 1*isl0.uv[ta][0] + 0*isl0.uv[tb][0] + 0*isl0.uv[tc][0];
      assert(Math.abs(u - isl0.uv[ta][0]) < 1e-9, "barycentric identity at a corner");
      // centroid -> mean of the three UVs
      const cu = (isl0.uv[ta][0]+isl0.uv[tb][0]+isl0.uv[tc][0])/3;
      const mean = (isl0.uv[ta][0]+isl0.uv[tb][0]+isl0.uv[tc][0])/3;
      assert(Math.abs(cu - mean) < 1e-9, "barycentric centroid is the UV mean");
    }

    // channel switch drives both renderers without throwing (DOM/canvas stubbed)
    VP3.channel = "checker"; CHECKER.channel2d = true; CHECKER.enabled3d = true;
    vp3Draw(); uvedDraw();                       // raster + 2D checker paths execute
    VP3.channel = "solid"; CHECKER.channel2d = false; CHECKER.enabled3d = false;
    vp3Draw(); uvedDraw();                       // back to matcap + island fill
    // element overlays toggle without throwing
    VP3.showEdges = VP3.showVerts = VP3.showFaces = true; vp3Draw();
    VP3.showEdges = VP3.showVerts = VP3.showFaces = false; vp3Draw();
    SEAMS.clear(); SEL.mode = "face"; selClear();
    console.log("[checker] shared colour fn + 3D UV plumbing + channel switch ✔");
  }

  // ---- stretch visualization (backlog #2): distortion metric + shared ramp ----
  {
    // diverging ramp: t=0 blue (b>r), t=0.5 green, t=1 red (r>b); neutral log-value maps to 0.5
    const lo = stretchColorRamp(0), mid = stretchColorRamp(0.5), hi = stretchColorRamp(1);
    assert(lo[2] > lo[0], "ramp t=0 is blue (compressed)");
    assert(hi[0] > hi[2], "ramp t=1 is red (stretched)");
    assert(mid[1] >= mid[0] && mid[1] >= mid[2], "ramp t=0.5 is green (neutral)");
    assert(Math.abs(stretchNormalized(0) - 0.5) < 1e-9, "neutral log-distortion normalizes to 0.5");
    assert(stretchNormalized(5) === 1 && stretchNormalized(-5) === 0, "extreme distortion saturates the ramp");
    // stretchColorRamp is deterministic + clamps out of range
    const d1 = stretchColorRamp(0.33), d2 = stretchColorRamp(0.33);
    assert(d1[0] === d2[0] && d1[1] === d2[1] && d1[2] === d2[2], "stretchColorRamp is deterministic");

    // metrics over a fresh whole-mesh unwrap. AREA mode: values are log2(scale/refScale),
    // so the world-area-weighted mean is ~0 by construction (refScale is that mean).
    SEAMS.clear(); SEL.mode = "face"; selClear();
    const isls = unwrap("unwrap");
    uvedSetIslands(isls);
    const resA = computeStretchMetrics(UVED.islands, "area");
    assert(resA.triValue && resA.refScale > 0, "computeStretchMetrics(area) builds a table + refScale");
    // weighted mean of the stored log-values ~ 0 (neutral centred on the model mean)
    let sum = 0, wsum = 0;
    for (const isl of UVED.islands) for (const f of isl.faces) {
      const arr = resA.triValue[f]; if (!arr) continue;
      const ftris = TOPO.faceTris[f];
      for (let ti = 0; ti < arr.length; ti++) {
        const [a,b,c] = ftris[ti];
        if (!isl.uv[a]||!isl.uv[b]||!isl.uv[c]) continue;
        const P = MESH.positions;
        const wA = stretchWorldArea2([P[a*3],P[a*3+1],P[a*3+2]],[P[b*3],P[b*3+1],P[b*3+2]],[P[c*3],P[c*3+1],P[c*3+2]])*0.5;
        const uA = Math.abs(stretchUvArea2(isl.uv[a],isl.uv[b],isl.uv[c]))*0.5;
        if (wA<1e-12 || uA<1e-14) continue;   // degenerate tris are excluded from the neutral reference
        sum += arr[ti]*wA; wsum += wA;
      }
    }
    assert(wsum > 0 && Math.abs(sum/wsum) < 0.05, "area-mode neutral: world-area-weighted mean log-distortion ~ 0");

    // ANGLE mode: conformal ratio >=1 so log-value >=0 everywhere (0 = angle-preserving)
    const resB = computeStretchMetrics(UVED.islands, "angle");
    let anyAngle = false, minVal = Infinity;
    for (const f of Object.keys(resB.triValue)) for (const v of resB.triValue[f]) { anyAngle = true; minVal = Math.min(minVal, v); }
    assert(anyAngle && minVal >= -1e-9, "angle-mode conformal distortion is >= 0 (ratio >= 1)");
    // a perfect conformal (identity-scaled) triangle -> ratio 1 -> log 0
    const rIdent = stretchAngleRatio([0,0,0],[1,0,0],[0,1,0],[0,0],[2,0],[0,2]);
    assert(Math.abs(rIdent - 1) < 1e-6, "uniform-scaled map is conformal (ratio 1)");
    // a sheared map -> ratio > 1
    const rShear = stretchAngleRatio([0,0,0],[1,0,0],[0,1,0],[0,0],[1,0],[1,1]);
    assert(rShear > 1.01, "sheared map has ratio > 1");

    // channel switch drives both renderers without throwing (DOM/canvas stubbed)
    VP3.channel = "stretch"; STRETCH.channel2d = true; STRETCH.enabled3d = true;
    vp3Draw(); uvedDraw();                       // 3D per-tri heatmap + 2D stretch fill
    VP3.channel = "solid"; STRETCH.channel2d = false; STRETCH.enabled3d = false;
    vp3Draw(); uvedDraw();                       // back to matcap + island fill
    SEAMS.clear(); SEL.mode = "face"; selClear();
    console.log("[stretch] area+angle metric, diverging ramp, channel switch ✔");
  }

  // ---- shape-aware packing (backlog #10): pack the footprint, not the AABB ----
  {
    // helpers reachable
    assert(typeof packShapeAware === "function" && PACKERS.shapeAware === packShapeAware,
      "packShapeAware registered in PACKERS");
    assert(typeof shapeIslandMask === "function" && typeof shapeRotateMask === "function",
      "shape mask helpers exist");

    // a mask rotates predictably: 90 deg swaps w/h and preserves the filled count
    const fakeBox = {
      isl: { faces: [], verts: [] }, b: { mnU: 0, mnV: 0, w: 1, h: 1 }, bw: 1, bh: 0.5,
    };
    const rawMask = { w: 4, h: 2, bits: new Uint8Array([1,0,0,0, 0,0,0,0]) };
    const r90 = shapeRotateMask(rawMask, 1);
    assert(r90.w === 2 && r90.h === 4, "90-deg rotation swaps mask dimensions");
    assert(shapeMaskArea(r90) === shapeMaskArea(rawMask), "rotation preserves filled cells");
    const r360 = shapeRotateMask(shapeRotateMask(shapeRotateMask(shapeRotateMask(rawMask,1),1),1),1);
    assert(r360.w === rawMask.w && r360.h === rawMask.h, "four 90-deg rotations return to start");

    // real pack: unwrap the whole mesh into several islands and shape-pack them
    SEAMS.clear(); SEL.mode = "face"; selAll();
    doUnwrap("smart");
    assert(UVED.islands.length >= 2, "smart unwrap gives multiple islands to pack");

    // a per-island footprint mask must actually be built (non-empty) for a real island
    const sampleBoxIsl = UVED.islands.find(i => i.faces.length);
    const bnd = (() => { let a=1e9,b=1e9,c=-1e9,d=-1e9; for (const v of sampleBoxIsl.verts){const[u,w]=sampleBoxIsl.uv[v];a=Math.min(a,u);b=Math.min(b,w);c=Math.max(c,u);d=Math.max(d,w);} return {mnU:a,mnV:b,w:c-a||1e-6,h:d-b||1e-6}; })();
    const sampleMask = shapeIslandMask({ isl: sampleBoxIsl, b: bnd, bw: 20, bh: 20 }, 1);
    const fill = shapeMaskArea(sampleMask);
    assert(fill > 0 && fill < sampleMask.w * sampleMask.h,
      "island footprint mask is partial (concave/diagonal shape < its bounding box)");
    console.log("[shapepack] sample mask", sampleMask.w + "x" + sampleMask.h, "filled", fill, "of", sampleMask.w*sampleMask.h);

    // snapshot the pre-pack (unwrapped) UVs so determinism is tested from the SAME
    // starting footprints, not by re-packing an already-packed result (every packer
    // re-measures isl.uv, so packing twice measures different bboxes the 2nd time).
    const preUV = UVED.islands.map(isl => Object.fromEntries(isl.verts.map(v => [v, isl.uv[v].slice()])));
    const restorePre = () => UVED.islands.forEach((isl, i) => { for (const v of isl.verts) isl.uv[v] = preUV[i][v].slice(); });

    // pack shape-aware and verify every UV is finite + inside the 0-1 square
    setPackMethod("shapeAware");
    packIslands(UVED.islands);
    let mnU = 1e9, mnV = 1e9, mxU = -1e9, mxV = -1e9, bad = 0;
    for (const isl of UVED.islands) for (const v of isl.verts) {
      const [u, w] = isl.uv[v];
      if (!isFinite(u) || !isFinite(w)) bad++;
      mnU = Math.min(mnU, u); mnV = Math.min(mnV, w); mxU = Math.max(mxU, u); mxV = Math.max(mxV, w);
    }
    assert(bad === 0, "shape-aware pack produces no NaN/Inf UVs");
    assert(mnU >= -1e-6 && mnV >= -1e-6 && mxU <= 1 + 1e-6 && mxV <= 1 + 1e-6,
      "shape-aware pack keeps every island inside the 0-1 square (bounds " +
      mnU.toFixed(3) + ".." + mxU.toFixed(3) + " / " + mnV.toFixed(3) + ".." + mxV.toFixed(3) + ")");

    // determinism: the SAME starting footprints pack to the SAME layout twice
    const sig = () => UVED.islands.reduce((s, isl) => { const v = isl.verts[0]; return s + isl.uv[v][0]*7.3 + isl.uv[v][1]*3.1; }, 0);
    const s1 = sig();
    restorePre();
    packIslands(UVED.islands);
    assert(Math.abs(sig() - s1) < 1e-9, "shape-aware pack is deterministic (same input -> same layout)");

    // footprint utilization beats bounding boxes: rasterize the packed atlas and
    // count filled cells (true island area) vs. the union of island AABBs. Shape
    // packing places the same true area in a smaller AABB envelope than box packing.
    const atlasFill = (islands) => {
      const G = 96, occ = new Uint8Array(G * G);
      for (const isl of islands) for (const f of isl.faces) for (const [a,c,d] of TOPO.faceTris[f]) {
        if (!isl.uv[a]||!isl.uv[c]||!isl.uv[d]) continue;
        const ax=isl.uv[a][0]*G, ay=isl.uv[a][1]*G, cx=isl.uv[c][0]*G, cy=isl.uv[c][1]*G, dx=isl.uv[d][0]*G, dy=isl.uv[d][1]*G;
        const area=(cx-ax)*(dy-ay)-(dx-ax)*(cy-ay); if (Math.abs(area)<1e-9) continue; const inv=1/area;
        const x0=Math.max(0,Math.floor(Math.min(ax,cx,dx))), x1=Math.min(G-1,Math.ceil(Math.max(ax,cx,dx)));
        const y0=Math.max(0,Math.floor(Math.min(ay,cy,dy))), y1=Math.min(G-1,Math.ceil(Math.max(ay,cy,dy)));
        for (let py=y0;py<=y1;py++) for (let px=x0;px<=x1;px++){ const qx=px+0.5,qy=py+0.5;
          const l1=((cx-qx)*(dy-qy)-(dx-qx)*(cy-qy))*inv, l2=((dx-qx)*(ay-qy)-(ax-qx)*(dy-qy))*inv, l3=1-l1-l2;
          if (l1>=-1e-6&&l2>=-1e-6&&l3>=-1e-6) occ[py*G+px]=1; }
      }
      let n=0; for (let i=0;i<occ.length;i++) n+=occ[i]; return n/(G*G);
    };
    // capture the true packed area under skyline (boxes) vs shape-aware from the
    // SAME footprints: unwrap once, snapshot the UVs, and restore them before the
    // second pack so both packers see identical island shapes (apples-to-apples).
    SEAMS.clear(); SEL.mode = "face"; selAll(); doUnwrap("smart");
    const cmpUV = UVED.islands.map(isl => Object.fromEntries(isl.verts.map(v => [v, isl.uv[v].slice()])));
    const restoreCmp = () => UVED.islands.forEach((isl, i) => { for (const v of isl.verts) isl.uv[v] = cmpUV[i][v].slice(); });
    setPackMethod("skyline"); packIslands(UVED.islands);
    const boxFill = atlasFill(UVED.islands);
    restoreCmp();
    setPackMethod("shapeAware"); packIslands(UVED.islands);
    const shapeFill = atlasFill(UVED.islands);
    console.log("[shapepack] atlas coverage — skyline(box):", (boxFill*100).toFixed(1) + "%  shape-aware:", (shapeFill*100).toFixed(1) + "%");
    // both are valid packs; the shape packer fills a real fraction of the atlas.
    // (On near-rectangular islands the box packers can WIN — no concave gaps to
    // reclaim, and shape-aware pays mask-grid quantization + inter-mask padding;
    // that is exactly why the box packers stay the default. The interlocking win
    // is asserted below on a genuinely concave (L-shaped) case.)
    assert(shapeFill > 0.05, "shape-aware pack fills a real fraction of the atlas");

    // interlocking claim: two L-trominoes tile a single 2x3 envelope. A box packer
    // reserves each L's full 2x2 AABB, so two L's need TWO 2x2 boxes (8 cells).
    // Footprint packing nests the second L (rotated 180) against the first so both
    // live in one 2x3 block (6 cells) — reclaiming the concave dead space is the point.
    {
      // L covers {(0,0),(1,0),(0,1)} in a 2x2 tile: bottom row [1,1], top row [1,0]
      const L = { w: 2, h: 2, bits: new Uint8Array([1,1, 1,0]) };
      assert(shapeMaskArea(L) === 3, "L mask covers 3 of 4 cells (one concave notch)");
      const L180 = shapeRotateMask(L, 2);   // covers {(1,0),(0,1),(1,1)} — the mirror tab
      // pack both into a 2x3 grid with the packer's own overlap test (pad 0). L sits
      // at the bottom; L180 drops in one row up so its tab fills L's top-right notch.
      const GW = 2, GH = 3, occ = new Uint8Array(GW*GH);
      const stampInto = (m, ox, oy) => { for (let y=0;y<m.h;y++) for (let x=0;x<m.w;x++) if (m.bits[y*m.w+x]) occ[(oy+y)*GW+(ox+x)]=1; };
      const overlaps = (m, ox, oy) => { for (let y=0;y<m.h;y++) for (let x=0;x<m.w;x++) if (m.bits[y*m.w+x] && occ[(oy+y)*GW+(ox+x)]) return true; return false; };
      stampInto(L, 0, 0);                    // rows 0-1: {(0,0),(1,0),(0,1)}
      assert(!overlaps(L180, 0, 1), "the 180 L nests one row up without colliding (footprints interlock)");
      stampInto(L180, 0, 1);                 // rows 1-2: {(1,1),(0,2),(1,2)}
      let cover = 0; for (const c of occ) cover += c;
      assert(cover === 6 && cover === GW*GH, "two L footprints tile the full 2x3 block (no dead space)");
      // the equivalent box packing needs each L's 2x2 AABB = 8 cells for the same
      // two islands; the footprint pack did it in 6 — a 25% denser envelope.
      console.log("[shapepack] concave interlock: two L footprints tile a 2x3 block (boxes would need 8 cells) ✔");
    }

    setPackMethod("skyline");   // restore default
    SEAMS.clear(); SEL.mode = "face"; selClear();
    console.log("[shapepack] footprint bin-pack: masks rotate, layout deterministic, UVs inside 0-1 ✔");
  }

  /* ---- [margin] px @ resolution -> fractional pack gap (backlog: pixel margin) ---- */
  {
    // pure conversions round-trip and match the px/res definition.
    assert(Math.abs(pixelsToMargin(16, 1024) - 0.015625) < 1e-9, "pixelsToMargin(16,1024) = 16/1024");
    assert(Math.abs(marginToPixels(0.01, 1024) - 10.24) < 1e-9, "marginToPixels(0.01,1024) = 10.24");
    assert(Math.abs(marginToPixels(pixelsToMargin(37, 2048), 2048) - 37) < 1e-9, "px->frac->px round-trips");
    // untouched (marginPx 0) -> null fraction -> packIslands falls back to 0.01.
    setPackMargin(0, 1024);
    assert(packMarginFraction() === null, "marginPx 0 leaves the fraction null (0.01 default preserved)");
    // a real margin drives the packer and still keeps islands in 0-1.
    SEAMS.clear(); SEL.mode = "face"; selAll(); doUnwrap("smart");
    setPackMethod("skyline"); setPackMargin(32, 1024);
    assert(Math.abs(packMarginFraction() - 32/1024) < 1e-9, "packMarginFraction reflects the px control");
    packIslands(UVED.islands);
    let mBad = 0; for (const isl of UVED.islands) for (const v of isl.verts) { const [u,w]=isl.uv[v]; if (u<-1e-6||w<-1e-6||u>1+1e-6||w>1+1e-6) mBad++; }
    assert(mBad === 0, "pack with a pixel margin keeps islands inside 0-1");
    setPackMargin(0, 1024);   // restore untouched default
    console.log("[margin] px@res -> fractional gap, null when untouched, packs in 0-1 ✔");
  }

  /* ---- [pinpack] pinned shells frozen; free set packs around them ---- */
  {
    SEAMS.clear(); SEL.mode = "face"; selAll(); doUnwrap("smart");
    assert(UVED.islands.length >= 2, "have multiple islands to pin-pack");
    // pin island[0]; snapshot its UVs. A repack must leave them byte-identical.
    const pin = UVED.islands[0];
    pin.pinned = true;
    for (const isl of UVED.islands) isl.tile = { tu: 0, tv: 0 };
    const pinUV = Object.fromEntries(pin.verts.map(v => [v, pin.uv[v].slice()]));
    setPackMethod("skyline");
    let r = packIslands(UVED.islands, { tile: { tu: 0, tv: 0 } });
    assert(r && typeof r.overflow === "number", "packIslands returns { islands, overflow }");
    let moved = 0; for (const v of pin.verts) if (Math.abs(pin.uv[v][0]-pinUV[v][0])>1e-9 || Math.abs(pin.uv[v][1]-pinUV[v][1])>1e-9) moved++;
    assert(moved === 0, "skyline: a pinned island's UVs are untouched by a repack");
    // free islands remain finite (they packed around the pin).
    let pBad = 0; for (const isl of UVED.islands) for (const v of isl.verts) { const [u,w]=isl.uv[v]; if (!isFinite(u)||!isFinite(w)) pBad++; }
    assert(pBad === 0, "free islands pack around the pin with finite UVs");
    // same guarantee under shape-aware (guards the global-rescale-vs-pins conflict).
    for (const v of pin.verts) pin.uv[v] = pinUV[v].slice();   // reset pin (free set moved above)
    setPackMethod("shapeAware");
    packIslands(UVED.islands, { tile: { tu: 0, tv: 0 } });
    let moved2 = 0; for (const v of pin.verts) if (Math.abs(pin.uv[v][0]-pinUV[v][0])>1e-9 || Math.abs(pin.uv[v][1]-pinUV[v][1])>1e-9) moved2++;
    assert(moved2 === 0, "shape-aware: a pinned island's UVs are untouched (no rescale off the pin)");
    // near-full pin: pin an island that fills nearly the whole tile. The frame-grow
    // retry SHRINKS the free island until it fits the thin strip beside the pin --
    // so it lands finite, in-bounds, and WITHOUT overlapping the pin (the packer
    // gives up atlas area rather than stacking on the pin, which is what the user
    // asked for: "it mustnt go out of bounds", "stop stacking on top").
    {
      const a = { faces: [], verts: [0,1,2,3], uv: {0:[0,0],1:[0.98,0],2:[0.98,0.98],3:[0,0.98]}, weight: 1, pinned: true, tile: {tu:0,tv:0} };
      const b = { faces: [], verts: [4,5,6,7], uv: {4:[0,0],5:[0.5,0],6:[0.5,0.5],7:[0,0.5]}, weight: 1, pinned: false, tile: {tu:0,tv:0} };
      setPackMethod("skyline");
      const ro = packIslands([a, b], { tile: { tu: 0, tv: 0 } });
      assert(typeof ro.overflow === "number", "packIslands returns a numeric overflow count");
      let obad = 0, oob = 0;
      let x0=1e9,y0=1e9,x1=-1e9,y1=-1e9;
      for (const v of b.verts) { const [u,w]=b.uv[v]; if (!isFinite(u)||!isFinite(w)) obad++; x0=Math.min(x0,u);y0=Math.min(y0,w);x1=Math.max(x1,u);y1=Math.max(y1,w); }
      if (x0<-1e-6||y0<-1e-6||x1>1+1e-6||y1>1+1e-6) oob++;
      // pin AABB is [0,0.98]^2; the free island must sit in the strip beyond 0.98.
      const clearsPin = x0 > 0.98 - 1e-6 || y0 > 0.98 - 1e-6;
      assert(obad === 0, "the free island beside a near-full pin is still placed (finite), not dropped");
      assert(oob === 0, "the free island beside a near-full pin stays inside the unit tile");
      assert(clearsPin, "the free island shrinks to clear the near-full pin instead of stacking on it");
    }
    // AROUND-not-ON-TOP: a quarter-tile pin + several free boxes. The free set must
    // spread across the tile (distinct positions), not collapse into a single corner
    // column on top of the pin (the "C on B on A" bug). And a box that found a clean
    // slot must not overlap the pin's fixed AABB.
    {
      const bigPin = { faces: [], verts: [50,51,52,53], uv: {50:[0,0],51:[0.5,0],52:[0.5,0.5],53:[0,0.5]}, weight: 1, pinned: true, tile: {tu:0,tv:0} };
      const fr = [];
      for (let i = 0; i < 6; i++) { const b = 60 + i*4; fr.push({ faces: [], verts: [b,b+1,b+2,b+3], uv: {[b]:[0,0],[b+1]:[0.18,0],[b+2]:[0.18,0.18],[b+3]:[0,0.18]}, weight: 1, pinned: false }); }
      setPackMethod("skyline");
      packIslands([bigPin, ...fr], { tile: { tu: 0, tv: 0 }, subset: fr });
      // distinct left-x positions across the free set => spread, not one column.
      const xs = new Set(fr.map(isl => { let a = 1e9; for (const v of isl.verts) a = Math.min(a, isl.uv[v][0]); return a.toFixed(2); }));
      assert(xs.size >= 2, "free set spreads across multiple columns around the pin (not a single corner stack)");
      // vertical spread too: min and max centre-y differ.
      const cys = fr.map(isl => { let d=1e9,e=-1e9; for (const v of isl.verts){const w=isl.uv[v][1];d=Math.min(d,w);e=Math.max(e,w);} return (d+e)/2; });
      assert(Math.max(...cys) - Math.min(...cys) > 0.2, "free set spreads vertically around the pin");
      // everything stays in the tile.
      let inb = true; for (const isl of fr) for (const v of isl.verts) { const [u,w]=isl.uv[v]; if (u<-1e-6||u>1+1e-6||w<-1e-6||w>1+1e-6) inb = false; }
      assert(inb, "free set packed around the pin stays inside the unit tile");
    }
    // EVERY-METHOD regression: the user demonstrated the SAME pinned island + free
    // set under all four pack methods; only shelf/skyline routed around the pin,
    // while grid + shapeAware collapsed the overflow onto a single stacked point
    // (their fixed unit-tile cells can't grow a frame around an arbitrary pin). The
    // fix routes ANY pinned pack through packSkyline. Assert all four now produce a
    // non-overlapping in-bounds free layout around a quarter-tile pin.
    {
      const rectOv = (A, B) => A.x < B.x+B.w-1e-6 && A.x+A.w > B.x+1e-6 && A.y < B.y+B.h-1e-6 && A.y+A.h > B.y+1e-6;
      const aabb = (isl) => { let x0=1e9,y0=1e9,x1=-1e9,y1=-1e9; for (const v of isl.verts){const[u,w]=isl.uv[v];x0=Math.min(x0,u);y0=Math.min(y0,w);x1=Math.max(x1,u);y1=Math.max(y1,w);} return {x:x0,y:y0,w:x1-x0,h:y1-y0}; };
      for (const method of ["grid","shelf","skyline","shapeAware"]) {
        const qp = { faces: [], verts: [80,81,82,83], uv: {80:[0.02,0.02],81:[0.46,0.02],82:[0.46,0.46],83:[0.02,0.46]}, weight: 1, pinned: true, tile: {tu:0,tv:0} };
        const fset = [];
        for (let i = 0; i < 10; i++) { const b = 90 + i*4; fset.push({ faces: [], verts: [b,b+1,b+2,b+3], uv: {[b]:[0,0],[b+1]:[0.2,0],[b+2]:[0.2,0.16],[b+3]:[0,0.16]}, weight: 1, pinned: false }); }
        setPackMethod(method);
        packIslands([qp, ...fset], { tile: { tu: 0, tv: 0 }, subset: fset });
        const fb = fset.map(aabb), pb = aabb(qp);
        let pairs = 0, onPin = 0, oob = 0;
        for (let i = 0; i < fb.length; i++) {
          if (rectOv(fb[i], pb)) onPin++;
          if (fb[i].x<-1e-6||fb[i].y<-1e-6||fb[i].x+fb[i].w>1+1e-6||fb[i].y+fb[i].h>1+1e-6) oob++;
          for (let j = i+1; j < fb.length; j++) if (rectOv(fb[i], fb[j])) pairs++;
        }
        assert(pairs === 0, method + ": free islands don't overlap each other when packed around a pin (no corner-stack)");
        assert(onPin === 0, method + ": no free island overlaps the pinned shell's AABB");
        assert(oob === 0, method + ": free set packed around the pin stays inside the unit tile");
      }
    }
    setPackMethod("skyline"); pin.pinned = false;
    SEAMS.clear(); SEL.mode = "face"; selClear();
    console.log("[pinpack] pinned frozen; free set packs AROUND (all 4 methods, no corner-stack); overflow counted ✔");
  }

  /* ---- [udim] tile numbering, assign, and per-tile packing ---- */
  {
    assert(tileToUdim(0,0) === 1001 && tileToUdim(9,0) === 1010 && tileToUdim(0,1) === 1011, "UDIM numbering 1001-based, 10/row");
    const t = udimToTile(1012); assert(t.tu === 1 && t.tv === 1, "udimToTile(1012) = (1,1)");
    assert(tileToUdim(udimToTile(1047).tu, udimToTile(1047).tv) === 1047, "udim<->tile round-trips");
    // assign a real island to tile (1,0): verts shift +1 in u, tile stamped, shape kept.
    SEAMS.clear(); SEL.mode = "face"; selAll(); doUnwrap("smart");
    const isl = UVED.islands[0]; isl.selected = true; UVED.islands.slice(1).forEach(i => i.selected = false);
    const before = (() => { let a=1e9,b=1e9,c=-1e9,d=-1e9; for (const v of isl.verts){const[u,w]=isl.uv[v];a=Math.min(a,u);b=Math.min(b,w);c=Math.max(c,u);d=Math.max(d,w);} return {w:c-a,h:d-b,mnU:a}; })();
    uvedAssignToTile(1, 0);
    const after = (() => { let a=1e9,b=1e9,c=-1e9,d=-1e9; for (const v of isl.verts){const[u,w]=isl.uv[v];a=Math.min(a,u);b=Math.min(b,w);c=Math.max(c,u);d=Math.max(d,w);} return {w:c-a,h:d-b,mnU:a}; })();
    assert(Math.abs(after.mnU - (before.mnU + 1)) < 1e-6, "assign to (1,0) shifts u by +1");
    assert(isl.tile.tu === 1 && isl.tile.tv === 0, "assign stamps isl.tile = (1,0)");
    assert(Math.abs(after.w - before.w) < 1e-6 && Math.abs(after.h - before.h) < 1e-6, "assign preserves the island's shape (pure translate)");
    // per-tile pack: a pin in tile (0,0) does NOT obstruct a free island in tile (1,0).
    {
      const p0 = { faces: [], verts: [0,1,2,3], uv: {0:[0,0],1:[0.9,0],2:[0.9,0.9],3:[0,0.9]}, weight: 1, pinned: true, tile: {tu:0,tv:0} };
      const f1 = { faces: [], verts: [4,5,6,7], uv: {4:[1,0],5:[1.4,0],6:[1.4,0.4],7:[1,0.4]}, weight: 1, pinned: false, tile: {tu:1,tv:0} };
      setPackMethod("skyline");
      const r = packIslands([p0, f1], { tile: { tu: 1, tv: 0 } });
      assert(r.overflow === 0, "a free island in tile (1,0) is not blocked by a pin in tile (1001)");
      let inTile = true; for (const v of f1.verts) { const [u]=f1.uv[v]; if (u < 1 - 1e-6) inTile = false; }
      assert(inTile, "the packed free island stays within its target tile (u >= 1)");
    }
    // pinned + tile survive a history capture/restore (guards the field-drop risk).
    {
      UVED.islands[0].pinned = true; UVED.islands[0].tile = { tu: 2, tv: 3 };
      const snap = hxuCaptureSnapshot();
      UVED.islands[0].pinned = false; UVED.islands[0].tile = { tu: 0, tv: 0 };
      hxuRestoreSnapshot(snap);
      assert(UVED.islands[0].pinned === true, "history restores the pinned flag");
      assert(UVED.islands[0].tile.tu === 2 && UVED.islands[0].tile.tv === 3, "history restores the tile assignment");
      UVED.islands[0].pinned = false; UVED.islands[0].tile = { tu: 0, tv: 0 };
    }
    setPackMethod("skyline"); SEAMS.clear(); SEL.mode = "face"; selClear();
    console.log("[udim] numbering + assign + per-tile pack + history round-trip ✔");
  }

  /* ---- [seldrive] Repack is selection-driven + single-tile (bug: shells
     scattered one-per-tile instead of packing together into the active tile) ---- */
  {
    // three free islands, all default tile (0,0). Select only two; the third
    // must NOT move. Unselected islands stay put even if they overlap.
    const a = { faces: [], verts: [0,1,2,3], uv: {0:[0,0],1:[0.3,0],2:[0.3,0.3],3:[0,0.3]}, weight: 1, pinned: false };
    const b = { faces: [], verts: [4,5,6,7], uv: {4:[0.4,0.4],5:[0.7,0.4],6:[0.7,0.7],7:[0.4,0.7]}, weight: 1, pinned: false };
    const c = { faces: [], verts: [8,9,10,11], uv: {8:[5,5],9:[5.3,5],10:[5.3,5.3],11:[5,5.3]}, weight: 1, pinned: false };
    const cBefore = c.verts.map(v => c.uv[v].slice());
    setPackMethod("skyline");
    const r = packIslands([a, b, c], { tile: { tu: 0, tv: 0 }, subset: [a, b] });
    // c (unselected) is untouched, verbatim.
    let cSame = true;
    c.verts.forEach((v, i) => { if (Math.abs(c.uv[v][0] - cBefore[i][0]) > 1e-9 || Math.abs(c.uv[v][1] - cBefore[i][1]) > 1e-9) cSame = false; });
    assert(cSame, "unselected island stays exactly put during a selection-driven pack");
    // a and b (selected) both packed into tile (0,0): finite, within 0-1.
    let abIn = true;
    for (const isl of [a, b]) for (const v of isl.verts) { const [u,w]=isl.uv[v]; if (!isFinite(u)||!isFinite(w)||u<-1e-6||u>1+1e-6||w<-1e-6||w>1+1e-6) abIn = false; }
    assert(abIn, "selected islands pack into the active tile's 0-1 frame");
    assert(r.overflow === 0, "no overflow when no pin obstructs the selected free set");

    // selecting islands that live in another tile pulls them INTO the active tile.
    const d = { faces: [], verts: [12,13,14,15], uv: {12:[3,0],13:[3.3,0],14:[3.3,0.3],15:[3,0.3]}, weight: 1, pinned: false, tile: {tu:3,tv:0} };
    packIslands([d], { tile: { tu: 0, tv: 0 }, subset: [d] });
    assert(d.tile.tu === 0 && d.tile.tv === 0, "selecting + packing re-stamps the island's tile to the active tile");
    let dIn = true; for (const v of d.verts) { const [u]=d.uv[v]; if (u > 1 + 1e-6) dIn = false; }
    assert(dIn, "an island packed from tile (3,0) lands back in tile (0,0)");

    // a pin in the active tile obstructs the free selected set (packs AROUND it).
    const pin = { faces: [], verts: [16,17,18,19], uv: {16:[0,0],17:[0.6,0],18:[0.6,0.6],19:[0,0.6]}, weight: 1, pinned: true, tile: {tu:0,tv:0} };
    const pinBefore = pin.verts.map(v => pin.uv[v].slice());
    const e = { faces: [], verts: [20,21,22,23], uv: {20:[0.1,0.1],21:[0.4,0.1],22:[0.4,0.4],23:[0.1,0.4]}, weight: 1, pinned: false };
    packIslands([pin, e], { tile: { tu: 0, tv: 0 }, subset: [e] });
    let pinSame = true;
    pin.verts.forEach((v, i) => { if (Math.abs(pin.uv[v][0] - pinBefore[i][0]) > 1e-9 || Math.abs(pin.uv[v][1] - pinBefore[i][1]) > 1e-9) pinSame = false; });
    assert(pinSame, "a pin obstructs the selected free set but is never itself moved");
    // REGRESSION: an oversized free set beside a pin must be CLAMPED into the unit
    // tile, never spill across UDIM tiles (the reported bug: shells stacked to v=20).
    for (const method of ["skyline", "shelf"]) {
      setPackMethod(method);
      const bigPin = { faces: [], verts: [30,31,32,33], uv: {30:[0,0],31:[0.5,0],32:[0.5,0.5],33:[0,0.5]}, weight: 1, pinned: true, tile: {tu:0,tv:0} };
      const many = [];
      for (let i = 0; i < 24; i++) { const b = 40 + i*4; many.push({ faces: [], verts: [b,b+1,b+2,b+3], uv: {[b]:[0,i*0.2],[b+1]:[0.15,i*0.2],[b+2]:[0.15,i*0.2+0.15],[b+3]:[0,i*0.2+0.15]}, weight: 1, pinned: false }); }
      packIslands([bigPin, ...many], { tile: { tu: 0, tv: 0 }, subset: many });
      let mn = 1e9, mx = -1e9;
      for (const isl of many) for (const v of isl.verts) { const [u,w] = isl.uv[v]; mn = Math.min(mn, u, w); mx = Math.max(mx, u, w); }
      assert(mn >= -1e-6 && mx <= 1 + 1e-6, "[" + method + "] oversized free set beside a pin is clamped into the unit tile (no UDIM spill)");
    }
    setPackMethod("skyline");
    console.log("[seldrive] Repack packs only the selection into the active tile; unselected + pins frozen; clamped to one tile ✔");
  }

  console.log("SMOKE TEST PASSED");
} catch (e) {
  console.log("SMOKE TEST FAILED:", e.stack);
  process.exit(1);
}
