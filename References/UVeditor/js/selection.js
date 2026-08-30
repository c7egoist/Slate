"use strict";
/* ==========================================================================
   selection.js -- the single source of truth for what is selected on the 3D
   mesh, plus every selection operation exposed in the toolbar. Mode-aware:
   the active mode (vertex / edge / face / island) decides what a selection
   op means. Both viewports render from SEL; app.js re-broadcasts changes.

   Storage:
     SEL.faces  Set(faceIndex)
     SEL.verts  Set(vertIndex)
     SEL.edges  Set(edgeKey)
   The active-mode set is authoritative; the others are derived when needed
   (e.g. face highlight in the rasterizer always uses SEL.faces, which is
   synthesized from verts/edges so overlays stay consistent).
   ========================================================================== */
const SEL = {
  mode: "face",                 // vertex | edge | face | island
  faces: new Set(),
  verts: new Set(),
  edges: new Set(),
  lastPick: null,               // { mode, id } — anchor for two-pick loop/path
  onChange: null,               // set by app.js
};

function selClear() {
  SEL.faces.clear(); SEL.verts.clear(); SEL.edges.clear();
  selChanged();
}
function selChanged() { symApply(); if (SEL.onChange) SEL.onChange(); }

/* ========================================================================
   SYMMETRY / MIRROR SELECT -- when enabled, every selection edit is mirrored
   across the mesh's plane of symmetry, so ALL tools (pick / box / lasso /
   paint / grow / shrink / similar / invert / region add+subtract) stay
   symmetric with no per-tool code. It works by:
     1. building a vertex<->mirror-vertex map across the best symmetry plane
        (auto-detected among the X / Y / Z bbox-centre planes), derived up to
        edge<->edge and face<->face maps;
     2. hooking selChanged(): the DELTA since the last change (elements added /
        removed) is mirrored onto the active set — so unselecting one side also
        unselects its mirror, not just adding.
   Only meaningful on a symmetrical object; enabling refuses a mesh whose
   matched-vertex fraction falls below SYM.minFraction.
   ======================================================================== */
const SYM = {
  enabled: false,
  built: false,
  axis: 0, axisName: "X",
  fraction: 0,
  minFraction: 0.9,       // below this the mesh isn't symmetric enough to mirror
  v2v: new Map(),         // vertex -> mirror vertex
  e2e: new Map(),         // edgeKey -> mirror edgeKey
  f2f: new Map(),         // faceIndex -> mirror faceIndex
  prev: null,             // { mode, set:Set } snapshot of the last active set
};
let _symGuard = false;

function _symBbox() {
  const P = MESH.positions, n = MESH.vertCount;
  const mn = [1e9, 1e9, 1e9], mx = [-1e9, -1e9, -1e9];
  for (let i = 0; i < n; i++) for (let a = 0; a < 3; a++) {
    const c = P[i*3+a]; if (c < mn[a]) mn[a] = c; if (c > mx[a]) mx[a] = c;
  }
  return { mn, mx };
}
/* map every vertex to the nearest vertex across the `axis` plane at `center`,
   accepting a match only within `tol`. Returns { v2v, fraction }. */
function symComputeAxisMap(axis, center, tol) {
  const P = MESH.positions, n = MESH.vertCount, m = new Map();
  let matched = 0;
  const tol2 = tol * tol;
  for (let v = 0; v < n; v++) {
    const t = [P[v*3], P[v*3+1], P[v*3+2]];
    t[axis] = 2*center[axis] - t[axis];
    let best = -1, bd = tol2;
    for (let u = 0; u < n; u++) {
      const dx = P[u*3]-t[0], dy = P[u*3+1]-t[1], dz = P[u*3+2]-t[2], d = dx*dx+dy*dy+dz*dz;
      if (d < bd) { bd = d; best = u; }
    }
    if (best >= 0) { m.set(v, best); matched++; }
  }
  return { v2v: m, fraction: n ? matched / n : 0 };
}
/* build the full symmetry map for the best-fitting axis; idempotent. */
function buildSymmetryMap() {
  const { mn, mx } = _symBbox();
  const center = [(mn[0]+mx[0])/2, (mn[1]+mx[1])/2, (mn[2]+mx[2])/2];
  const diag = Math.hypot(mx[0]-mn[0], mx[1]-mn[1], mx[2]-mn[2]) || 1;
  const tol = diag * 5e-3;
  let best = null, bestAxis = 0;
  for (let a = 0; a < 3; a++) {
    const r = symComputeAxisMap(a, center, tol);
    if (!best || r.fraction > best.fraction) { best = r; bestAxis = a; }
  }
  // keep only mutually-consistent pairs so the map is a true involution
  // (mirror(mirror(v)) === v): a stray nearest-neighbour tie that isn't
  // reciprocated would otherwise break the delta-mirror in symApply.
  const v2v = new Map();
  for (const [v, mv] of best.v2v) if (best.v2v.get(mv) === v) v2v.set(v, mv);
  SYM.v2v = v2v; SYM.fraction = MESH.vertCount ? v2v.size / MESH.vertCount : 0;
  SYM.axis = bestAxis; SYM.axisName = ["X", "Y", "Z"][bestAxis];
  // edge map: an edge maps iff both endpoints map and the mirrored pair is a real edge
  SYM.e2e = new Map();
  for (const k of TOPO.edgeKeys) {
    const [a, b] = TOPO.edgeVerts[k];
    const ma = SYM.v2v.get(a), mb = SYM.v2v.get(b);
    if (ma == null || mb == null) continue;
    const mk = edgeKey(ma, mb);
    if (TOPO.edgeVerts[mk]) SYM.e2e.set(k, mk);
  }
  // face map: match by the sorted mirrored vertex loop
  const bySig = new Map();
  for (let f = 0; f < TOPO.faceCount; f++) bySig.set(TOPO.faceVerts[f].slice().sort((x,y)=>x-y).join(","), f);
  SYM.f2f = new Map();
  for (let f = 0; f < TOPO.faceCount; f++) {
    const mv = TOPO.faceVerts[f].map(v => SYM.v2v.get(v));
    if (mv.some(x => x == null)) continue;
    const mf = bySig.get(mv.slice().sort((x,y)=>x-y).join(","));
    if (mf != null) SYM.f2f.set(f, mf);
  }
  SYM.built = true;
  return SYM.fraction;
}
/* the mirror of one element in the given mode (undefined if it has no mirror). */
function symMirror(mode, id) {
  if (mode === "vertex") return SYM.v2v.get(id);
  if (mode === "edge") return SYM.e2e.get(id);
  return SYM.f2f.get(id);       // face + island both key on faces
}
/* mirror the change since the last selChanged onto the active set. Guarded so
   the mutations it makes don't recurse. Called at the top of selChanged. */
function symApply() {
  if (!SYM.enabled) { SYM.prev = null; return; }
  if (_symGuard || !SYM.built) return;
  _symGuard = true;
  const mode = SEL.mode, set = activeSet();
  const prev = (SYM.prev && SYM.prev.mode === mode) ? SYM.prev.set : new Set();
  const toAdd = [], toDel = [];
  for (const id of set) if (!prev.has(id)) toAdd.push(id);
  for (const id of prev) if (!set.has(id)) toDel.push(id);
  for (const id of toDel) { const m = symMirror(mode, id); if (m != null) set.delete(m); }
  for (const id of toAdd) { const m = symMirror(mode, id); if (m != null) set.add(m); }
  SYM.prev = { mode, set: new Set(set) };
  _symGuard = false;
}
/* turn symmetry on: build the map if needed, refuse an asymmetric mesh, then
   mirror the CURRENT selection so both sides match immediately. Returns the
   matched-vertex fraction (or the built fraction) so the caller can report. */
function symEnable() {
  if (!SYM.built) buildSymmetryMap();
  if (SYM.fraction < SYM.minFraction) { SYM.enabled = false; return SYM.fraction; }
  SYM.enabled = true; SYM.prev = null;   // null prev => the whole current selection is mirrored
  selChanged();
  return SYM.fraction;
}
function symDisable() { SYM.enabled = false; SYM.prev = null; }

/* a stable fingerprint of the whole selection (mode + every member id), so a
   caller can tell whether a gesture actually changed what is selected before it
   bothers to record a history step. */
function selSignature() {
  const f = [...SEL.faces].sort((a, b) => a - b).join(",");
  const v = [...SEL.verts].sort((a, b) => a - b).join(",");
  const e = [...SEL.edges].sort().join(",");
  return SEL.mode + "|f:" + f + "|v:" + v + "|e:" + e;
}

/* ---- primary set for the active mode ---- */
function activeSet() {
  if (SEL.mode === "vertex") return SEL.verts;
  if (SEL.mode === "edge") return SEL.edges;
  return SEL.faces; // face + island both operate on faces
}

/* ---- faces implied by the current selection (drives 3D fill + unwrap) ---- */
function selectedFaceSet() {
  if (SEL.mode === "face" || SEL.mode === "island") return new Set(SEL.faces);
  const out = new Set();
  if (SEL.mode === "vertex") {
    // faces all of whose loop verts are selected (any arity)
    for (let f = 0; f < TOPO.faceCount; f++) {
      if (TOPO.faceVerts[f].every(v => SEL.verts.has(v))) out.add(f);
    }
  } else if (SEL.mode === "edge") {
    for (let f = 0; f < TOPO.faceCount; f++) {
      const es = TOPO.faceEdges[f];
      if (es.every(k => SEL.edges.has(k))) out.add(f);
    }
  }
  return out;
}

/* ---- click-pick toggles ---- */
function selectFace(f, additive) {
  if (!additive) selClearSilent();
  if (SEL.mode === "island") {
    for (const nf of islandOf(f)) SEL.faces.add(nf);
  } else {
    SEL.faces.has(f) && additive ? SEL.faces.delete(f) : SEL.faces.add(f);
  }
  SEL.lastPick = { mode: SEL.mode, id: f };
  selChanged();
}
function selectVert(v, additive) {
  if (!additive) selClearSilent();
  SEL.verts.has(v) && additive ? SEL.verts.delete(v) : SEL.verts.add(v);
  SEL.lastPick = { mode: "vertex", id: v };
  selChanged();
}
function selectEdge(k, additive) {
  if (!additive) selClearSilent();
  SEL.edges.has(k) && additive ? SEL.edges.delete(k) : SEL.edges.add(k);
  SEL.lastPick = { mode: "edge", id: k };
  selChanged();
}
function selClearSilent() { SEL.faces.clear(); SEL.verts.clear(); SEL.edges.clear(); }

/* ========================================================================
   GROW -- expand the active selection by one adjacency ring
   ======================================================================== */
function selGrow() {
  if (SEL.mode === "vertex") {
    const add = [];
    for (const v of SEL.verts) for (const n of TOPO.vertNeighbors[v]) add.push(n);
    add.forEach(v => SEL.verts.add(v));
  } else if (SEL.mode === "edge") {
    const add = [];
    for (const k of SEL.edges) {
      const [v0, v1] = TOPO.edgeVerts[k];
      for (const vf of [v0, v1]) {
        for (const f of TOPO.vertFaces[vf]) {
          for (const ek of TOPO.faceEdges[f]) add.push(ek);
        }
      }
    }
    add.forEach(k => SEL.edges.add(k));
  } else {
    const add = [];
    for (const f of SEL.faces) for (const nf of faceNeighbors(f)) add.push(nf);
    add.forEach(f => SEL.faces.add(f));
  }
  selChanged();
}

/* ========================================================================
   SHRINK -- peel off boundary elements (those touching an unselected one)
   ======================================================================== */
function selShrink() {
  if (SEL.mode === "vertex") {
    const remove = [];
    for (const v of SEL.verts) {
      for (const n of TOPO.vertNeighbors[v]) if (!SEL.verts.has(n)) { remove.push(v); break; }
    }
    remove.forEach(v => SEL.verts.delete(v));
  } else if (SEL.mode === "edge") {
    const remove = [];
    for (const k of SEL.edges) {
      // an edge is on the boundary if it shares a vert with an unselected edge
      const [v0, v1] = TOPO.edgeVerts[k];
      let boundary = false;
      for (const vf of [v0, v1]) {
        for (const f of TOPO.vertFaces[vf]) {
          for (const ek of TOPO.faceEdges[f]) if (ek !== k && !SEL.edges.has(ek)) boundary = true;
        }
      }
      if (boundary) remove.push(k);
    }
    remove.forEach(k => SEL.edges.delete(k));
  } else {
    const remove = [];
    for (const f of SEL.faces) {
      for (const nf of faceNeighbors(f)) if (!SEL.faces.has(nf)) { remove.push(f); break; }
    }
    remove.forEach(f => SEL.faces.delete(f));
  }
  selChanged();
}

/* ========================================================================
   SELECT SIMILAR -- match by geometric property of the current selection
     face   : coplanar-ish (normal within angle threshold of any selected)
     vertex : same valence (one-ring size)
     edge   : similar length (within 20%)
   ======================================================================== */
function selSimilar() {
  if (SEL.mode === "face") {
    const refs = [...SEL.faces].map(f => TOPO.faceNormal[f]);
    if (!refs.length) return;
    const cosThresh = Math.cos(15 * Math.PI / 180);
    for (let f = 0; f < TOPO.faceCount; f++) {
      const n = TOPO.faceNormal[f];
      if (refs.some(r => r[0]*n[0]+r[1]*n[1]+r[2]*n[2] >= cosThresh)) SEL.faces.add(f);
    }
  } else if (SEL.mode === "vertex") {
    const vals = new Set([...SEL.verts].map(v => TOPO.vertNeighbors[v].size));
    if (!vals.size) return;
    for (let v = 0; v < TOPO.vertNeighbors.length; v++) {
      if (vals.has(TOPO.vertNeighbors[v].size)) SEL.verts.add(v);
    }
  } else if (SEL.mode === "edge") {
    const lens = [...SEL.edges].map(edgeLen);
    if (!lens.length) return;
    for (const k of TOPO.edgeKeys) {
      const l = edgeLen(k);
      if (lens.some(r => Math.abs(l - r) <= r * 0.2)) SEL.edges.add(k);
    }
  }
  selChanged();
}
function edgeLen(k) {
  const [v0, v1] = TOPO.edgeVerts[k], P = MESH.positions;
  return Math.hypot(P[v0*3]-P[v1*3], P[v0*3+1]-P[v1*3+1], P[v0*3+2]-P[v1*3+2]);
}

/* ========================================================================
   SELECT ISLAND -- flood fill connected faces across shared edges, stopping
   at marked seams. Returns the face set; also selects it into SEL.faces.
   ======================================================================== */
function islandOf(seed) {
  const island = new Set([seed]);
  const stack = [seed];
  while (stack.length) {
    const f = stack.pop();
    for (const k of TOPO.faceEdges[f]) {
      if (SEAMS.has(k)) continue;              // seam blocks the flood
      for (const nf of TOPO.edgeFaces[k]) {
        if (!island.has(nf)) { island.add(nf); stack.push(nf); }
      }
    }
  }
  return island;
}
function selIsland() {
  const seeds = [...selectedFaceSet()];
  if (!seeds.length) return;
  for (const s of seeds) for (const f of islandOf(s)) SEL.faces.add(f);
  SEL.mode = "face";
  selChanged();
}

/* ========================================================================
   SELECT LINKED (Blender "Ctrl+L") -- the connected component reachable from
   the current selection by walking SHARED VERTICES (welded/merged geometry).
   Unlike islandOf this ignores seams: it is pure topological connectivity, so
   a mesh whose eyes are separate shells from the head yields the head alone
   when a head element is seeded, never the whole model. Grows the selection in
   whatever mode is active (vertex/edge/face/island) using the same component.
   ======================================================================== */
function connectedVertsFrom(seedVerts) {
  const comp = new Set(seedVerts);
  const stack = [...seedVerts];
  while (stack.length) {
    const v = stack.pop();
    for (const nb of TOPO.vertNeighbors[v]) {
      if (!comp.has(nb)) { comp.add(nb); stack.push(nb); }
    }
  }
  return comp;
}
function seedVertsFromSelection() {
  const seeds = new Set();
  if (SEL.mode === "vertex") { for (const v of SEL.verts) seeds.add(v); }
  else if (SEL.mode === "edge") { for (const k of SEL.edges) for (const v of TOPO.edgeVerts[k]) seeds.add(v); }
  else { for (const f of SEL.faces) for (const v of TOPO.faceVerts[f]) seeds.add(v); }
  return seeds;
}
function selLinked() {
  const seeds = seedVertsFromSelection();
  if (!seeds.size) return 0;
  const comp = connectedVertsFrom(seeds);   // every vertex of the shell(s) touched
  if (SEL.mode === "vertex") {
    for (const v of comp) SEL.verts.add(v);
  } else if (SEL.mode === "edge") {
    // every edge both of whose endpoints are in the component
    for (const k of TOPO.edgeKeys) {
      const [a, b] = TOPO.edgeVerts[k];
      if (comp.has(a) && comp.has(b)) SEL.edges.add(k);
    }
  } else {
    // face / island: every face all of whose loop verts are in the component
    for (let f = 0; f < TOPO.faceCount; f++) {
      if (TOPO.faceVerts[f].every(v => comp.has(v))) SEL.faces.add(f);
    }
    SEL.mode = SEL.mode === "island" ? "island" : "face";
  }
  selChanged();
  return comp.size;
}

/* ========================================================================
   SELECT PERIMETER -- boundary edges enclosing the selected face region
   (edges with exactly one incident selected face). Switches to edge mode.
   ======================================================================== */
function selPerimeter() {
  const faces = selectedFaceSet();
  if (!faces.size) return;
  const rim = new Set();
  for (const f of faces) {
    for (const k of TOPO.faceEdges[f]) {
      const incident = TOPO.edgeFaces[k].filter(nf => faces.has(nf)).length;
      if (incident === 1) rim.add(k);
    }
  }
  SEL.mode = "edge";
  SEL.edges = rim;
  SEL.faces.clear(); SEL.verts.clear();
  selChanged();
}

/* ========================================================================
   EDGE LOOP -- walk an edge loop from a seed edge. An edge LOOP is a chain of
   edges joined end-to-end that runs ALONG the surface (Blender Alt-click), as
   opposed to an edge RING of parallel rungs across faces. On quad topology the
   loop rule is vertex-centric: at each endpoint vertex of valence 4, continue
   to the edge that shares NO face with the current edge -- the edge "straight
   across" the vertex. This is topology-driven and independent of camera facing.
   Where the topology is irregular (tris, ngons, poles, non-valence-4), we fall
   back to the "straightest continuing edge" heuristic so a loop is still traced.
   Both selLoopFromEdge and loopEdgeSet call the same computeLoopEdges core.
   ======================================================================== */

/* the loop continuation at vertex v coming in along curEdge: the edge incident
   to v that shares no face with curEdge (the one directly across the vertex on
   a valence-4 quad fan). Returns { k, nb } or null when the vertex is not a
   clean valence-4 crossing (pole / boundary / tri fan) so the caller can fall
   back to the straightness heuristic. */
function loopContinueAtVert(v, curEdge) {
  // edges at v = curEdge + one per neighbour; a clean loop crossing has 4.
  const nbrs = [...TOPO.vertNeighbors[v]];
  if (nbrs.length !== 4) return null;               // valence-4 only for the topology rule
  const curFaces = new Set(TOPO.edgeFaces[curEdge]); // the 1-2 faces along curEdge
  let found = null;
  for (const nb of nbrs) {
    const k = edgeKey(v, nb);
    if (k === curEdge) continue;
    // the continuation shares no face with curEdge
    if (TOPO.edgeFaces[k].some(f => curFaces.has(f))) continue;
    if (found) return null;                          // ambiguous -> bail to fallback
    found = { k, nb };
  }
  return found;
}

/* one direction of the vertex-centric loop walk, advancing from startV.
   Returns true if it made any topology-rule progress (so the caller knows
   whether a straightness fallback is still needed for that direction). */
function walkLoopDir(loop, seedKey, startV) {
  let curEdge = seedKey, curV = startV, guard = 0, advanced = false;
  while (guard++ < 4000) {
    const step = loopContinueAtVert(curV, curEdge);
    if (!step || loop.has(step.k)) break;            // pole/boundary or closed the loop
    loop.add(step.k); advanced = true;
    curEdge = step.k; curV = step.nb;
  }
  return advanced;
}

/* straightness fallback (the original heuristic) -- traced from an endpoint
   of the seed by continuing to the edge whose direction best matches the
   incoming direction. Used only where the topology-rule walk cannot continue. */
function walkStraightestDir(loop, seedKey, startV) {
  let curEdge = seedKey, curV = startV, guard = 0;
  while (guard++ < 4000) {
    const [a, b] = TOPO.edgeVerts[curEdge];
    const prevV = (curV === a) ? b : a;
    const dirPrev = edgeDir(prevV, curV);
    let best = null, bestScore = -2;
    for (const nb of TOPO.vertNeighbors[curV]) {
      const k = edgeKey(curV, nb);
      if (k === curEdge) continue;
      const d = edgeDir(curV, nb);
      const score = dirPrev[0]*d[0] + dirPrev[1]*d[1] + dirPrev[2]*d[2]; // straightest
      if (score > bestScore) { bestScore = score; best = { k, nb }; }
    }
    if (!best || loop.has(best.k) || bestScore < 0.3) break;
    loop.add(best.k); curEdge = best.k; curV = best.nb;
  }
}

/* compute the edge-loop Set grown from a seed edge, without mutating SEL.
   Walks the vertex-centric quad loop rule from BOTH endpoints; where a
   direction cannot follow the rule (pole / boundary / non-quad) it falls back
   to the straightness heuristic from that endpoint, preserving old behaviour on
   irregular topology. */
function computeLoopEdges(seedKey) {
  const loop = new Set([seedKey]);
  const [va, vb] = TOPO.edgeVerts[seedKey];
  const okA = walkLoopDir(loop, seedKey, va);
  const okB = walkLoopDir(loop, seedKey, vb);
  if (!okA) walkStraightestDir(loop, seedKey, va);
  if (!okB) walkStraightestDir(loop, seedKey, vb);
  return loop;
}

function selLoopFromEdge(seedKey) {
  SEL.mode = "edge";
  SEL.edges = computeLoopEdges(seedKey); SEL.faces.clear(); SEL.verts.clear();
  selChanged();
}
function edgeDir(v0, v1) {
  const P = MESH.positions;
  let d = [P[v1*3]-P[v0*3], P[v1*3+1]-P[v0*3+1], P[v1*3+2]-P[v0*3+2]];
  const l = Math.hypot(d[0], d[1], d[2]) || 1;
  return [d[0]/l, d[1]/l, d[2]/l];
}

/* ---- invert / all ---- */
function selInvert() {
  if (SEL.mode === "vertex") {
    const inv = new Set();
    for (let v = 0; v < MESH.vertCount; v++) if (!SEL.verts.has(v)) inv.add(v);
    SEL.verts = inv;
  } else if (SEL.mode === "edge") {
    const inv = new Set();
    for (const k of TOPO.edgeKeys) if (!SEL.edges.has(k)) inv.add(k);
    SEL.edges = inv;
  } else {
    const inv = new Set();
    for (let f = 0; f < TOPO.faceCount; f++) if (!SEL.faces.has(f)) inv.add(f);
    SEL.faces = inv;
  }
  selChanged();
}
function selAll() {
  if (SEL.mode === "vertex") { for (let v = 0; v < MESH.vertCount; v++) SEL.verts.add(v); }
  else if (SEL.mode === "edge") { for (const k of TOPO.edgeKeys) SEL.edges.add(k); }
  else { for (let f = 0; f < TOPO.faceCount; f++) SEL.faces.add(f); }
  selChanged();
}

/* ========================================================================
   REGION SELECT -- box / circle / lasso / paint marquee. The caller supplies
   `inside(x, y)`, a screen-space hit test, and `visible(kind, id)` telling us
   whether an element is front-facing (so we never grab occluded back geometry).
   We walk the active mode's candidates, project via `at(kind, id)` (also caller
   -supplied, returning [sx, sy] or null), and add / remove membership.
     additive  : keep the prior selection, add matches
     subtract  : remove matches from the prior selection
   Both false => replace. Returns the number of elements touched.
   ======================================================================== */
function selectRegion(inside, at, visible, additive, subtract) {
  const set = activeSet();
  if (!additive && !subtract) set.clear();
  let touched = 0;
  const apply = (id) => {
    if (subtract) { if (set.delete(id)) touched++; }
    else { if (!set.has(id)) touched++; set.add(id); }
  };
  if (SEL.mode === "vertex") {
    for (let v = 0; v < MESH.vertCount; v++) {
      if (!visible("vertex", v)) continue;
      const p = at("vertex", v); if (p && inside(p[0], p[1])) apply(v);
    }
  } else if (SEL.mode === "edge") {
    for (const k of TOPO.edgeKeys) {
      if (!visible("edge", k)) continue;
      const p = at("edge", k); if (p && inside(p[0], p[1])) apply(k);
    }
  } else {
    // face + island: hit the projected face centre; island floods each hit
    const hits = [];
    for (let f = 0; f < TOPO.faceCount; f++) {
      if (!visible("face", f)) continue;
      const p = at("face", f); if (p && inside(p[0], p[1])) hits.push(f);
    }
    for (const f of hits) {
      if (SEL.mode === "island") { for (const nf of islandOf(f)) apply(nf); }
      else apply(f);
    }
  }
  if (touched) selChanged();
  return touched;
}

/* ========================================================================
   SHORTEST PATH between two verts over the one-ring graph (Dijkstra with
   edge-length weights). Returns the vertex list [v0 .. v1] or null. Used by
   the two-pick "connect" when the picks don't share a clean loop.
   ======================================================================== */
function shortestVertPath(v0, v1) {
  if (v0 === v1) return [v0];
  const dist = new Map([[v0, 0]]);
  const prev = new Map();
  const seen = new Set();
  // tiny binary-heap-free Dijkstra (mesh is small): linear scan for the min
  const frontier = new Set([v0]);
  while (frontier.size) {
    let u = null, ud = Infinity;
    for (const x of frontier) { const d = dist.get(x); if (d < ud) { ud = d; u = x; } }
    frontier.delete(u); seen.add(u);
    if (u === v1) break;
    for (const nb of TOPO.vertNeighbors[u]) {
      if (seen.has(nb)) continue;
      const nd = ud + edgeLen(edgeKey(u, nb));
      if (nd < (dist.has(nb) ? dist.get(nb) : Infinity)) { dist.set(nb, nd); prev.set(nb, u); frontier.add(nb); }
    }
  }
  if (!prev.has(v1) && v0 !== v1) return null;
  const path = [v1];
  while (path[path.length - 1] !== v0) { const p = prev.get(path[path.length - 1]); if (p == null) return null; path.push(p); }
  return path.reverse();
}

/* ========================================================================
   TWO-PICK CONNECT -- given the previous pick (SEL.lastPick) and a new element,
   highlight the run between them. If they lie on a common edge loop we take the
   loop segment; otherwise we fall back to the shortest edge path. Works in edge
   and vertex modes. Adds the connecting elements to the active selection.
   ======================================================================== */
function selConnectPath(newId) {
  const anchor = SEL.lastPick;
  if (!anchor || anchor.mode !== SEL.mode) { pickForMode(newId); return false; }
  if (SEL.mode === "edge") {
    // does a loop grown from the anchor edge already contain the new edge?
    const loop = loopEdgeSet(anchor.id);
    if (loop.has(newId)) { for (const k of loop) SEL.edges.add(k); SEL.lastPick = { mode: "edge", id: newId }; selChanged(); return true; }
    // fall back: shortest vertex path -> its edges
    const [a0, a1] = TOPO.edgeVerts[anchor.id], [b0, b1] = TOPO.edgeVerts[newId];
    const path = bestVertPath([a0, a1], [b0, b1]);
    if (path) { for (let i = 0; i + 1 < path.length; i++) SEL.edges.add(edgeKey(path[i], path[i+1])); SEL.edges.add(anchor.id); SEL.edges.add(newId); }
    SEL.lastPick = { mode: "edge", id: newId }; selChanged(); return !!path;
  }
  if (SEL.mode === "vertex") {
    const path = shortestVertPath(anchor.id, newId);
    if (path) for (const v of path) SEL.verts.add(v);
    SEL.lastPick = { mode: "vertex", id: newId }; selChanged(); return !!path;
  }
  if (SEL.mode === "face") {
    const path = shortestFacePath(anchor.id, newId);
    if (path) for (const f of path) SEL.faces.add(f);
    SEL.lastPick = { mode: "face", id: newId }; selChanged(); return !!path;
  }
  pickForMode(newId); return false;
}
/* ========================================================================
   FACE PATH -- shortest run of faces from anchor to target across shared
   edges (BFS on the dual graph — the mesh is small and every step is one
   edge-crossing, so an unweighted BFS gives the fewest-faces path). Used by
   the two-pick face connect (Ctrl+click face A, then face B). Returns the
   inclusive face list [anchor, ..., target] or null if disconnected.
   ======================================================================== */
function shortestFacePath(f0, f1) {
  if (f0 === f1) return [f0];
  const prev = new Map([[f0, -1]]);
  const queue = [f0];
  let head = 0;
  while (head < queue.length) {
    const f = queue[head++];
    if (f === f1) break;
    for (const nf of faceNeighbors(f)) {
      if (prev.has(nf)) continue;
      prev.set(nf, f); queue.push(nf);
    }
  }
  if (!prev.has(f1)) return null;
  const path = [f1];
  while (path[path.length - 1] !== f0) { const p = prev.get(path[path.length - 1]); if (p === -1 || p == null) return null; path.push(p); }
  return path.reverse();
}
/* pick the shortest of the four endpoint-pair paths between two edges */
function bestVertPath(aEnds, bEnds) {
  let best = null, bestLen = Infinity;
  for (const a of aEnds) for (const b of bEnds) {
    const p = shortestVertPath(a, b);
    if (p && p.length < bestLen) { bestLen = p.length; best = p; }
  }
  return best;
}
/* the full edge-loop set grown from a seed edge -- the same quad-topology walk
   as selLoopFromEdge, returned as a Set without mutating SEL. */
function loopEdgeSet(seedKey) {
  return computeLoopEdges(seedKey);
}
/* dispatch a single-element pick honoring the current mode (used as fallback) */
function pickForMode(id) {
  if (SEL.mode === "vertex") selectVert(id, true);
  else if (SEL.mode === "edge") selectEdge(id, true);
  else selectFace(id, true);
}

/* ========================================================================
   FACE LOOP -- from a seed face, trace ONE continuous band of quads: step to
   the neighbour across an edge, then continue out that quad's TOPOLOGICALLY
   opposite edge (the one sharing no vertex with the edge we entered by). This
   is the face analogue of the edge-loop quad rule -- facing-independent, unlike
   the old "most anti-parallel edge midpoint" heuristic, which both flipped with
   camera angle AND (by walking all four seed edges) traced a perpendicular
   CROSS of two bands instead of a single loop. We walk only the seed quad's two
   opposite-edge pairs so exactly one continuous band results. Falls back to the
   geometric heuristic where the topology is not a clean quad chain (tris/poles).
   Selects into SEL.faces.
   ======================================================================== */

/* the edge of quad face f opposite `edgeK` -- the loop edge that shares no
   vertex with it. null when f isn't a quad or has no unique opposite. */
function oppositeFaceEdge(f, edgeK) {
  const es = TOPO.faceEdges[f];
  if (es.length !== 4) return null;
  const [a, b] = TOPO.edgeVerts[edgeK];
  let found = null;
  for (const k of es) {
    if (k === edgeK) continue;
    const [x, y] = TOPO.edgeVerts[k];
    if (x !== a && x !== b && y !== a && y !== b) {   // disjoint from edgeK
      if (found) return null;                          // ambiguous -> bail
      found = k;
    }
  }
  return found;
}

/* walk a face band in one direction, entering the seed's neighbour across
   `exitEdge`. Adds every face reached to `ring`. Returns false immediately if
   the seed neighbour across exitEdge doesn't exist, so the caller can fall
   back to the geometric heuristic for that direction. */
function walkFaceLoopDir(ring, seed, exitEdge) {
  let curFace = seed, entryEdge = exitEdge, guard = 0, advanced = false;
  while (guard++ < 4000) {
    const nbFace = TOPO.edgeFaces[entryEdge].find(nf => nf !== curFace);
    if (nbFace == null || ring.has(nbFace)) break;    // boundary or closed
    const nextEdge = oppositeFaceEdge(nbFace, entryEdge);
    if (nextEdge == null) break;                       // tri/pole -- not a clean quad chain
    ring.add(nbFace); advanced = true;
    entryEdge = nextEdge; curFace = nbFace;
  }
  return advanced;
}

/* geometric fallback (original heuristic) for one direction where the quad
   rule cannot continue: step to the neighbour, then exit the most anti-parallel
   edge. */
function walkFaceRingDirGeom(ring, seed, e0) {
  let curFace = seed, entryEdge = e0, guard = 0;
  while (guard++ < 4000) {
    const nbFace = TOPO.edgeFaces[entryEdge].find(nf => nf !== curFace);
    if (nbFace == null || ring.has(nbFace)) break;
    ring.add(nbFace);
    const entryDir = edgeMidDir(entryEdge);
    let best = null, bestScore = 2;
    for (const k of TOPO.faceEdges[nbFace]) {
      if (k === entryEdge) continue;
      const d = edgeMidDir(k);
      const score = entryDir[0]*d[0] + entryDir[1]*d[1] + entryDir[2]*d[2]; // most anti-parallel
      if (score < bestScore) { bestScore = score; best = k; }
    }
    if (!best) break;
    entryEdge = best; curFace = nbFace;
  }
}

/* trace the face band along ONE axis of the seed quad (its two opposite edges
   eA / eB). Returns the Set of faces without mutating SEL. */
function faceBandAlong(seed, eA, eB) {
  const ring = new Set([seed]);
  const okA = walkFaceLoopDir(ring, seed, eA);
  const okB = walkFaceLoopDir(ring, seed, eB);
  if (!okA) walkFaceRingDirGeom(ring, seed, eA);
  if (!okB) walkFaceRingDirGeom(ring, seed, eB);
  return ring;
}

/* the face band a bare face pick would grab, WITHOUT mutating SEL -- the pure
   core of selRingFromFace, reused by the selection preview (backlog #8). */
function computeFaceBand(seed) {
  const es = TOPO.faceEdges[seed];
  if (es.length === 4) {
    // a quad has TWO possible loop axes: the opposite-edge pairs es[0]/es[2] and
    // es[1]/es[3]. Each traces one continuous band (never a perpendicular cross).
    // From a bare face pick we can't know which the user meant, so choose the
    // axis whose band runs FURTHEST -- the more "loop-like" result, and a
    // deterministic tiebreak so the pick is stable, not camera-dependent.
    const bandA = faceBandAlong(seed, es[0], es[2]);
    const bandB = faceBandAlong(seed, es[1], es[3]);
    return bandA.size >= bandB.size ? bandA : bandB;
  }
  // non-quad seed: no topological loop axis -- fall back to the geometric
  // walk out every edge (the original behaviour).
  const ring = new Set([seed]);
  for (const e0 of es) walkFaceRingDirGeom(ring, seed, e0);
  return ring;
}

function selRingFromFace(seed) {
  const ring = computeFaceBand(seed);
  SEL.mode = "face";
  for (const f of ring) SEL.faces.add(f);
  SEL.verts.clear(); SEL.edges.clear();
  SEL.lastPick = { mode: "face", id: seed };   // anchor the next Ctrl+click path
  selChanged();
}
function edgeMidDir(k) {
  const [v0, v1] = TOPO.edgeVerts[k];
  return edgeDir(v0, v1);
}

/* ---- seams (marked edges that partition islands + guide LSCM) ----
   Marks the CURRENTLY SELECTED edges as seams. In face mode we mark the
   boundary loop of the selected faces (the Blender "select faces -> mark
   boundary seam" workflow) rather than every interior edge. Returns the count
   of NEWLY added seams so callers can report the delta, not the running total
   (marking one loop should say "+1 loop", not "93 seams"). */
const SEAMS = new Set();
function seamEdgesFromSelection() {
  if (SEL.mode === "edge") return [...SEL.edges];
  // face/island mode: boundary edges of the selected face set
  const faces = selectedFaceSet();
  if (!faces.size) return [];
  const out = [];
  for (const f of faces) for (const k of TOPO.faceEdges[f]) {
    const incident = TOPO.edgeFaces[k].filter(nf => faces.has(nf)).length;
    if (incident === 1) out.push(k);   // edge on the border of the selection
  }
  return out;
}
function markSeamFromSelection() {
  let added = 0;
  for (const k of seamEdgesFromSelection()) if (!SEAMS.has(k)) { SEAMS.add(k); added++; }
  selChanged();
  return added;
}
function clearSeams() { SEAMS.clear(); selChanged(); }

if (typeof module !== "undefined") {
  module.exports = {
    SEL, SEAMS, SYM, selClear, selSignature, selectedFaceSet, activeSet,
    buildSymmetryMap, symMirror, symApply, symEnable, symDisable,
    selectFace, selectVert, selectEdge,
    selGrow, selShrink, selSimilar, selIsland, selPerimeter, selLoopFromEdge,
    selLinked, connectedVertsFrom, seedVertsFromSelection,
    selInvert, selAll, islandOf, markSeamFromSelection, seamEdgesFromSelection, clearSeams,
    selectRegion, shortestVertPath, shortestFacePath, selConnectPath, selRingFromFace, loopEdgeSet,
    computeLoopEdges, loopContinueAtVert, oppositeFaceEdge, walkFaceLoopDir, computeFaceBand,
  };
}
