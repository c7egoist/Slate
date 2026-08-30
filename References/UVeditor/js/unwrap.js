"use strict";
/* ==========================================================================
   unwrap.js -- REAL UV unwrapping over a chosen set of faces.

   Every algorithm produces per-corner UVs, splits the input into connected
   islands (respecting seams), then hands the islands to the packer which lays
   their bounding boxes into the 0-1 square. The 2D editor consumes the packed
   islands as its editable "shells".

     Projections (exact, closed form):
       planar      -- project onto the plane facing the region's average normal
       box         -- assign each face to one of 6 axis planes -> up to 6 islands
       cylindrical -- (atan2(x,z)->u, y->v)
       spherical   -- (longitude->u, latitude->v)
     Conformal:
       lscm        -- Least-Squares Conformal Maps per island, solved with a
                      Gauss-Seidel / Jacobi iteration on the normal equations.

   An "island" here = { faces:[fi..], verts:[vi..], uv:{vi:[u,v]} } in a shared
   pre-pack space; packing rescales into 0-1 and the editor turns each face into
   a drawn polygon.
   ========================================================================== */

/* ---- gather the target faces (selection, or the whole mesh if empty) ---- */
function targetFaces() {
  const sel = selectedFaceSet();
  if (sel.size) return [...sel];
  const all = [];
  for (let f = 0; f < TOPO.faceCount; f++) all.push(f);
  return all;
}

/* ---- split faces into connected islands across shared, seam-free edges ---- */
function facesToIslands(faces) {
  const inSet = new Set(faces);
  const visited = new Set();
  const islands = [];
  for (const seed of faces) {
    if (visited.has(seed)) continue;
    const group = [];
    const stack = [seed];
    visited.add(seed);
    while (stack.length) {
      const f = stack.pop();
      group.push(f);
      for (const k of TOPO.faceEdges[f]) {
        if (SEAMS.has(k)) continue;
        for (const nf of TOPO.edgeFaces[k]) {
          if (inSet.has(nf) && !visited.has(nf)) { visited.add(nf); stack.push(nf); }
        }
      }
    }
    islands.push(group);
  }
  return islands;
}

function islandVerts(faces) {
  const s = new Set();
  for (const f of faces) for (const v of TOPO.faceVerts[f]) s.add(v);
  return [...s];
}

/* count edges of a face group that have exactly one incident group face */
function boundaryEdgeCount(faces) {
  const inSet = new Set(faces);
  let bnd = 0;
  const counted = new Set();
  for (const f of faces) for (const k of TOPO.faceEdges[f]) {
    if (counted.has(k)) continue; counted.add(k);
    if (TOPO.edgeFaces[k].filter(nf => inSet.has(nf)).length === 1) bnd++;
  }
  return bnd;
}

/* Fraction of a face group's unique edges that lie on its open boundary. This is
   the scale-invariant "is this a disk?" signal used to decide whether LSCM can
   flatten a chart as one island: a disk-like patch exposes a large share of its
   edges as boundary (an eye shell ~0.12), while a near-closed blob with only a
   pinhole opening exposes very few (Suzanne's head ~0.03). Unlike a sqrt(N)
   boundary bar, the ratio behaves the same for a tiny patch and a huge shell. */
function openBoundaryRatio(faces) {
  const inSet = new Set(faces);
  const counted = new Set();
  let total = 0, bnd = 0;
  for (const f of faces) for (const k of TOPO.faceEdges[f]) {
    if (counted.has(k)) continue; counted.add(k);
    total++;
    if (TOPO.edgeFaces[k].filter(nf => inSet.has(nf)).length === 1) bnd++;
  }
  return total ? bnd / total : 0;
}
/* a chart with at least this share of boundary edges is treated as a flattenable
   disk and unwrapped as ONE LSCM island; below it, the chart is auto-cut first. */
const DISK_BOUNDARY_RATIO = 0.08;

/* Split a near-closed face group into disk-like sub-groups by dominant-normal
   axis. LSCM needs an open (disk) island; a closed surface collapses when
   flattened. When the caller hasn't marked seams, we auto-cut along the 6-way
   box partition so each conformal sub-island has a real boundary to relax to. */
function autoCutForConformal(faces) {
  // enough boundary already? (roughly a disk) -> keep whole
  if (boundaryEdgeCount(faces) >= Math.sqrt(faces.length) * 2) return [faces];
  const groups = {};
  for (const f of faces) {
    const n = TOPO.faceNormal[f];
    const ax = [Math.abs(n[0]), Math.abs(n[1]), Math.abs(n[2])];
    let a = 0; if (ax[1] > ax[a]) a = 1; if (ax[2] > ax[a]) a = 2;
    const key = a + (n[a] >= 0 ? "+" : "-");
    (groups[key] = groups[key] || []).push(f);
  }
  // within each axis group, re-split by connectivity, then merge tiny stray
  // patches into their largest same-axis sibling so we get a handful of coherent
  // disk islands (not hundreds of single-face slivers).
  const out = [];
  for (const key of Object.keys(groups)) {
    const inSet = new Set(groups[key]);
    const visited = new Set();
    const patches = [];
    for (const seed of groups[key]) {
      if (visited.has(seed)) continue;
      const grp = [], stack = [seed]; visited.add(seed);
      while (stack.length) {
        const f = stack.pop(); grp.push(f);
        for (const k of TOPO.faceEdges[f]) for (const nf of TOPO.edgeFaces[k]) {
          if (inSet.has(nf) && !visited.has(nf)) { visited.add(nf); stack.push(nf); }
        }
      }
      patches.push(grp);
    }
    patches.sort((a, b) => b.length - a.length);
    // keep patches with >=4 faces as islands; fold the rest into the biggest
    const keep = patches.filter(p => p.length >= 4);
    const strays = patches.filter(p => p.length < 4).flat();
    if (keep.length && strays.length) keep[0].push(...strays);
    else if (!keep.length) keep.push(strays);
    for (const p of keep) if (p.length) out.push(p);
  }
  return out;
}

/* ======================= PROJECTION MAPS ======================= */
function avgNormal(faces) {
  let n = [0, 0, 0];
  for (const f of faces) { const fn = TOPO.faceNormal[f]; n[0]+=fn[0]; n[1]+=fn[1]; n[2]+=fn[2]; }
  const l = Math.hypot(n[0], n[1], n[2]) || 1;
  return [n[0]/l, n[1]/l, n[2]/l];
}
// an orthonormal basis (tangent,bitangent) for a plane with the given normal
function planeBasis(n) {
  const up = Math.abs(n[1]) > 0.99 ? [0, 0, 1] : [0, 1, 0];
  const t = V3.normalize(V3.cross(up, n));
  const b = V3.cross(n, t);
  return [t, b];
}
function projectPlanar(faces) {
  const n = avgNormal(faces);
  const [t, b] = planeBasis(n);
  const verts = islandVerts(faces), uv = {}, P = MESH.positions;
  for (const v of verts) {
    const p = [P[v*3], P[v*3+1], P[v*3+2]];
    uv[v] = [V3.dot(p, t), V3.dot(p, b)];
  }
  return [{ faces, verts, uv }];
}
function projectBox(faces) {
  // group faces by dominant normal axis (6 signed axes)
  const groups = {};
  for (const f of faces) {
    const n = TOPO.faceNormal[f];
    const ax = [Math.abs(n[0]), Math.abs(n[1]), Math.abs(n[2])];
    let a = 0; if (ax[1] > ax[a]) a = 1; if (ax[2] > ax[a]) a = 2;
    const sign = n[a] >= 0 ? "+" : "-";
    const key = a + sign;
    (groups[key] = groups[key] || []).push(f);
  }
  const P = MESH.positions, islands = [];
  for (const key of Object.keys(groups)) {
    const gf = groups[key], verts = islandVerts(gf), uv = {};
    const a = +key[0];
    const uAx = (a + 1) % 3, vAx = (a + 2) % 3;
    for (const v of verts) uv[v] = [P[v*3+uAx], P[v*3+vAx]];
    islands.push({ faces: gf, verts, uv });
  }
  return islands;
}
function projectCylindrical(faces) {
  const verts = islandVerts(faces), uv = {}, P = MESH.positions;
  for (const v of verts) {
    const x = P[v*3], y = P[v*3+1], z = P[v*3+2];
    uv[v] = [Math.atan2(x, z) / (2*Math.PI), y * 0.5];
  }
  return [{ faces, verts, uv }];
}
function projectSpherical(faces) {
  const verts = islandVerts(faces), uv = {}, P = MESH.positions;
  for (const v of verts) {
    const x = P[v*3], y = P[v*3+1], z = P[v*3+2];
    const r = Math.hypot(x, y, z) || 1;
    uv[v] = [Math.atan2(x, z) / (2*Math.PI), Math.asin(Math.max(-1, Math.min(1, y/r))) / Math.PI];
  }
  return [{ faces, verts, uv }];
}

/* ======================= LSCM CONFORMAL ======================= */
/* Least-Squares Conformal Maps. For each triangle we write two conformal
   equations relating its 3 corner UVs to the triangle's local 2D shape. We pin
   two vertices (the two farthest apart) to remove the similarity DOF, then
   minimize ||A x - b||^2 by iterating on the normal equations (A^T A) x = A^T b
   with Gauss-Seidel. The mesh is small post-decimation so this converges fast.  */
function lscmIsland(faces) {
  const verts = islandVerts(faces);
  if (verts.length < 3) return projectPlanar(faces)[0];
  const idxOf = new Map(verts.forEach ? undefined : undefined);
  const vmap = new Map(); verts.forEach((v, i) => vmap.set(v, i));
  const nV = verts.length;
  const P = MESH.positions;

  // pin the two farthest-apart verts to a stable segment
  let p0 = 0, p1 = 1, best = -1;
  for (let i = 0; i < nV; i++) for (let j = i + 1; j < nV; j++) {
    const a = verts[i], b = verts[j];
    const d = (P[a*3]-P[b*3])**2 + (P[a*3+1]-P[b*3+1])**2 + (P[a*3+2]-P[b*3+2])**2;
    if (d > best) { best = d; p0 = i; p1 = j; }
  }
  const pinned = { [p0]: [0, 0], [p1]: [1, 0] };

  // free-variable layout: each free vertex has (u,v). map var index.
  const freeList = [];
  const isFree = new Array(nV).fill(true);
  isFree[p0] = false; isFree[p1] = false;
  for (let i = 0; i < nV; i++) if (isFree[i]) freeList.push(i);
  const varOf = new Map(); freeList.forEach((vi, k) => { varOf.set(vi, 2*k); });
  const nVar = freeList.length * 2;
  if (nVar === 0) {
    const uv = {}; uv[verts[p0]] = [0,0]; uv[verts[p1]] = [1,0];
    return { faces, verts, uv };
  }

  // Build normal-equation accumulators as dense-ish via row list.
  // Each triangle contributes 2 rows. We assemble A^T A (nVar x nVar) sparsely
  // in a Map, and A^T b as an array.
  const ATA = new Map();      // key "i,j" -> value
  const ATb = new Float64Array(nVar);
  const addATA = (i, j, val) => {
    if (i < 0 || j < 0) return;
    const key = i + "," + j;
    ATA.set(key, (ATA.get(key) || 0) + val);
  };
  // per-triangle local frame -> conformal coefficients. Polygons contribute
  // through their fan triangles (LSCM is intrinsically per-triangle); UVs are
  // stored per vertex, so a quad's two fan tris yield the same per-vertex UVs
  // as the original quad. The island still reports the original polygon faces.
  const solveTris = [];
  for (const f of faces) for (const t of TOPO.faceTris[f]) solveTris.push(t);
  for (const [va, vb, vc] of solveTris) {
    const ia = vmap.get(va), ib = vmap.get(vb), ic = vmap.get(vc);
    const pa = [P[va*3], P[va*3+1], P[va*3+2]];
    const pb = [P[vb*3], P[vb*3+1], P[vb*3+2]];
    const pc = [P[vc*3], P[vc*3+1], P[vc*3+2]];
    // local orthonormal 2D coords of the triangle
    const x1 = V3.sub(pb, pa), lenX = V3.length(x1) || 1e-6;
    const ex = V3.scale(x1, 1/lenX);
    const x2 = V3.sub(pc, pa);
    const projX = V3.dot(x2, ex);
    const ortho = V3.sub(x2, V3.scale(ex, projX));
    const lenY = V3.length(ortho) || 1e-6;
    const ey = V3.scale(ortho, 1/lenY);
    // triangle corner coords in local frame
    const W = [[0, 0], [lenX, 0], [V3.dot(x2, ex), V3.dot(x2, ey)]];
    const dT = (W[0][0]*(W[1][1]-W[2][1]) + W[1][0]*(W[2][1]-W[0][1]) + W[2][0]*(W[0][1]-W[1][1]));
    const area = Math.abs(dT) * 0.5;
    const sqA = Math.sqrt(area) || 1e-4;
    // conformal (Wr, Wi) per corner (LSCM real/imag parts), scaled by 1/sqrt(area)
    const cornerV = [ia, ib, ic];
    const Wr = [W[2][0]-W[1][0], W[0][0]-W[2][0], W[1][0]-W[0][0]].map(x => x/sqA);
    const Wi = [W[2][1]-W[1][1], W[0][1]-W[2][1], W[1][1]-W[0][1]].map(x => x/sqA);
    // Two real rows (real & imaginary of the conformal condition).
    // Unknowns per vert: u (var), v (var+1). Contribution of each corner:
    //   row_re:  Wr*u - Wi*v
    //   row_im:  Wi*u + Wr*v
    // Assemble into A^T A / A^T b, moving pinned contributions to RHS.
    const rows = [
      // coefficients: for corner c, (coeffU_on_row, coeffV_on_row)
      { u: Wr, v: Wi.map(x => -x) }, // real row
      { u: Wi, v: Wr },              // imag row
    ];
    for (const row of rows) {
      // gather (varIndex or pinnedValue, coeff) entries for this row
      const entries = []; // {var:idx or -1, coeff, pinU} ...
      let rhs = 0;
      for (let cI = 0; cI < 3; cI++) {
        const vi = cornerV[cI];
        // u unknown
        if (isFree[vi]) entries.push({ idx: varOf.get(vi), coeff: row.u[cI] });
        else rhs -= row.u[cI] * pinned[vi][0];
        // v unknown
        if (isFree[vi]) entries.push({ idx: varOf.get(vi) + 1, coeff: row.v[cI] });
        else rhs -= row.v[cI] * pinned[vi][1];
      }
      // accumulate A^T A and A^T b for this single row
      for (let a = 0; a < entries.length; a++) {
        ATb[entries[a].idx] += entries[a].coeff * rhs;
        for (let b = 0; b < entries.length; b++) {
          addATA(entries[a].idx, entries[b].idx, entries[a].coeff * entries[b].coeff);
        }
      }
    }
  }
  // Gauss-Seidel on (A^T A) x = A^T b
  const x = new Float64Array(nVar);
  // pre-extract diagonal + row map for speed
  const rowMap = Array.from({ length: nVar }, () => []);
  for (const [key, val] of ATA) {
    const [i, j] = key.split(",").map(Number);
    rowMap[i].push([j, val]);
  }
  const diag = new Float64Array(nVar);
  for (let i = 0; i < nVar; i++) {
    for (const [j, val] of rowMap[i]) if (j === i) diag[i] = val;
    if (diag[i] === 0) diag[i] = 1e-6;
  }
  for (let iter = 0; iter < 300; iter++) {
    let maxDelta = 0;
    for (let i = 0; i < nVar; i++) {
      let sum = ATb[i];
      for (const [j, val] of rowMap[i]) if (j !== i) sum -= val * x[j];
      const nx = sum / diag[i];
      maxDelta = Math.max(maxDelta, Math.abs(nx - x[i]));
      x[i] = nx;
    }
    if (maxDelta < 1e-7) break;
  }
  // gather UVs
  const uv = {};
  uv[verts[p0]] = pinned[p0].slice();
  uv[verts[p1]] = pinned[p1].slice();
  for (const vi of freeList) {
    const b = varOf.get(vi);
    uv[verts[vi]] = [x[b], x[b + 1]];
  }
  // guard against NaN (degenerate island) -> fall back to planar
  for (const v of verts) if (!isFinite(uv[v][0]) || !isFinite(uv[v][1])) return projectPlanar(faces)[0];
  return { faces, verts, uv };
}

/* ======================= SYMMETRIC-UNWRAP PARITY (backlog #5) =======================
   L and R shells are geometrically mirror-images, but lscmIsland solves each island
   independently (pins its OWN farthest pair, Gauss-Seidels on its own island), so a
   left island and its mirror never converge to pixel-identical mirrored UVs. Fix:
   after the solve, detect L/R island PAIRS via the SYM map (built in selection.js)
   and COPY one side's solved UVs onto the other, mirrored, so the pair is exact. */

/* write mirrored UVs from `source` onto `target` using SYM.v2v: for each source
   vert v, its partner mv = SYM.v2v[v] receives the source UV flipped in U about
   the source island's own U-centre (shape mirrored, V unchanged). Returns the
   number of target verts written. */
function mirrorIslandUv(source, target) {
  let mnU = 1e9, mxU = -1e9;
  for (const v of source.verts) { const u = source.uv[v][0]; if (u < mnU) mnU = u; if (u > mxU) mxU = u; }
  const axisU = (mnU + mxU) / 2;
  let written = 0;
  for (const v of source.verts) {
    const mv = SYM.v2v.get(v);
    if (mv == null || !(mv in target.uv || target.verts.includes(mv))) continue;
    target.uv[mv] = [2 * axisU - source.uv[v][0], source.uv[v][1]];
    written++;
  }
  return written;
}

/* choose the canonical SOURCE of a mirror pair deterministically: the island
   whose lowest global vertex index is smaller (invariant under the symmetry map
   so mirror inputs always pick the same source). */
function canonicalSource(a, b) {
  const la = Math.min(...a.verts), lb = Math.min(...b.verts);
  return la <= lb ? a : b;
}

/* detect mirror island pairs and overwrite the partner with mirrored UVs.
   Gated on the mesh being symmetric enough (SYM.fraction >= SYM.minFraction).
   A and B pair iff EVERY face of A maps (SYM.f2f) to a face that lives in B and
   B maps back to A -- a mutual, whole-island match. Mutates islands in place;
   returns the number of pairs mirrored. */
function pairSymmetricIslands(islands) {
  if (typeof SYM === "undefined") return 0;
  if (!SYM.built && typeof buildSymmetryMap === "function") buildSymmetryMap();
  if (!SYM.built || SYM.fraction < SYM.minFraction) return 0;
  // face index -> which island owns it
  const faceIsland = new Map();
  islands.forEach((isl, i) => { for (const f of isl.faces) faceIsland.set(f, i); });
  const paired = new Set();
  let pairs = 0;
  for (let i = 0; i < islands.length; i++) {
    if (paired.has(i)) continue;
    const A = islands[i];
    // every face of A must map into ONE other island
    let partner = -1, ok = A.faces.length > 0;
    for (const f of A.faces) {
      const mf = SYM.f2f.get(f);
      const j = mf == null ? undefined : faceIsland.get(mf);
      if (j == null || j === i) { ok = false; break; }
      if (partner === -1) partner = j; else if (partner !== j) { ok = false; break; }
    }
    if (!ok || partner < 0 || paired.has(partner)) continue;
    const B = islands[partner];
    // require the match to be MUTUAL and cover all of B too (true pair, not subset)
    if (B.faces.length !== A.faces.length) continue;
    let mutual = true;
    for (const f of B.faces) { const mf = SYM.f2f.get(f); if (mf == null || faceIsland.get(mf) !== i) { mutual = false; break; } }
    if (!mutual) continue;
    const src = canonicalSource(A, B), dst = src === A ? B : A;
    if (mirrorIslandUv(src, dst) > 0) { paired.add(i); paired.add(partner); pairs++; }
  }
  return pairs;
}

/* ======================= NORMALIZE + PACK ======================= */
function islandBounds(isl) {
  let mnU = 1e9, mnV = 1e9, mxU = -1e9, mxV = -1e9;
  for (const v of isl.verts) {
    const [u, w] = isl.uv[v];
    mnU = Math.min(mnU, u); mnV = Math.min(mnV, w);
    mxU = Math.max(mxU, u); mxV = Math.max(mxV, w);
  }
  return { mnU, mnV, mxU, mxV, w: mxU - mnU || 1e-6, h: mxV - mnV || 1e-6 };
}
/* ---- island importance -------------------------------------------------
   Each island may carry a `weight` (default 1). Weight scales the LINEAR size
   an island is granted in the atlas, so a weight-4 island occupies ~4x the
   texel area of a weight-1 island of the same original shape. All packers read
   it, so "make this seam-critical face bigger" is a single knob. --------- */
function islandWeight(isl) {
  const w = isl.weight;
  return (typeof w === "number" && w > 0) ? w : 1;
}

/* ---- weight <-> UI-view conversions (backlog #9) ------------------------
   isl.weight is the single stored number. Three UI editors are equivalent
   views onto it: a log-spaced slider, a raw xN field, and a %-of-atlas-area
   field. These pure helpers convert between weight and each view so the three
   controls stay in sync without any second source of truth. --------------- */
const WEIGHT_SLIDER_MIN = 0.25;      // [x] slider low end
const WEIGHT_SLIDER_MAX = 8;         // [x] slider high end

/* slider position (0..1) -> weight, log-spaced so equal drag = equal ratio. */
function sliderToWeight(t) {
  const clamped = Math.max(0, Math.min(1, t));
  const lo = Math.log(WEIGHT_SLIDER_MIN), hi = Math.log(WEIGHT_SLIDER_MAX);
  return Math.exp(lo + (hi - lo) * clamped);
}
/* weight -> slider position (0..1), inverse of sliderToWeight (clamped). */
function weightToSlider(w) {
  const val = (typeof w === "number" && w > 0) ? w : 1;
  const lo = Math.log(WEIGHT_SLIDER_MIN), hi = Math.log(WEIGHT_SLIDER_MAX);
  const t = (Math.log(Math.max(WEIGHT_SLIDER_MIN, Math.min(WEIGHT_SLIDER_MAX, val))) - lo) / (hi - lo);
  return Math.max(0, Math.min(1, t));
}

/* target atlas-area share (0..1) for one island among the whole set ->
   the weight it must carry. Since measureBoxes scales linear box size by
   sqrt(weight), atlas area for an island is proportional to its weight, so
   share = weight / sum(all weights). Solving for one island's weight while the
   OTHER islands keep their current weights:
       share = w / (w + restSum)  ->  w = share*restSum / (1 - share).      */
function percentToWeight(share, islands, target) {
  const s = Math.max(0, Math.min(0.999, share));
  let restSum = 0;
  for (const isl of islands) if (isl !== target) restSum += islandWeight(isl);
  if (restSum <= 1e-9) return 1;                       // sole island -> neutral
  return (s * restSum) / (1 - s);
}
/* inverse: an island's current share of the summed atlas weight (0..1). */
function weightToPercent(islands, target) {
  let total = 0;
  for (const isl of islands) total += islandWeight(isl);
  if (total <= 1e-9) return 0;
  return islandWeight(target) / total;
}

/* Measure each island's placed box (native UV extent) and derive a target
   packing box: aspect preserved, longest side scaled by sqrt(weight) so the
   AREA grows linearly with weight. Returns {isl, b, bw, bh} pre-pack boxes. */
function measureBoxes(islands) {
  return islands.map(isl => {
    const b = islandBounds(isl);
    const side = Math.max(b.w, b.h) || 1e-6;
    const wgt = Math.sqrt(islandWeight(isl));
    return { isl, b, side, bw: (b.w / side) * wgt, bh: (b.h / side) * wgt };
  });
}

/* write an island's verts into the atlas rectangle [ox,oy] size [dw,dh],
   mapping its native UV box (b) into that rectangle preserving orientation. */
function placeIsland(box, ox, oy, dw, dh) {
  const { isl, b } = box;
  const sw = dw / (b.w || 1e-6), sh = dh / (b.h || 1e-6);
  for (const v of isl.verts) {
    const nu = (isl.uv[v][0] - b.mnU) * sw;
    const nv = (isl.uv[v][1] - b.mnV) * sh;
    isl.uv[v] = [ox + nu, oy + nv];
  }
}

/* GRID -- weighted uniform cells (baseline; every island gets one cell,
   larger-weight islands fill more of theirs). Kept for parity + speed. With
   pinned obstacles present, cells whose centred slot collides with a pin are
   skipped and the free box spills to the next free cell (counted as overflow if
   it still lands on a pin). */
function packGrid(boxes, margin, obstacles = [], packOpts = {}) {
  const pinned = obstacles.length > 0;
  const n = boxes.length;
  const cols = Math.max(1, Math.ceil(Math.sqrt(n)));
  const rows = Math.max(1, Math.ceil(n / cols));
  const cellW = 1 / cols, cellH = 1 / rows;
  boxes.sort((a, b) => (b.bw * b.bh) - (a.bw * a.bh));
  const hitsPin = (x, y, w, h) => obstacles.some(o => rectsOverlap(x, y, w, h, o.x, o.y, o.w, o.h, 0));
  // when pinned, walk cells and hand each box the next cell that clears the pins.
  let cell = 0;
  const nextFreeCell = (dw, dh) => {
    for (; cell < cols * rows; cell++) {
      const cx = cell % cols, cy = Math.floor(cell / cols);
      const ox = cx*cellW + (cellW-dw)/2, oy = cy*cellH + (cellH-dh)/2;
      if (!hitsPin(ox, oy, dw, dh)) { cell++; return { ox, oy, over: false }; }
    }
    // ran out of clear cells -> place on the first cell anyway (overlap).
    return { ox: (cellW-dw)/2, oy: (cellH-dh)/2, over: true };
  };
  let overflow = 0;
  boxes.forEach((bx, k) => {
    const availW = cellW - margin * 2, availH = cellH - margin * 2;
    const scale = Math.min(availW / bx.bw, availH / bx.bh);
    const dw = bx.bw * scale, dh = bx.bh * scale;
    if (pinned) {
      const slot = nextFreeCell(dw, dh);
      if (slot.over) overflow++;
      placeIsland(bx, slot.ox, slot.oy, dw, dh);
    } else {
      const cx = k % cols, cy = Math.floor(k / cols);
      placeIsland(bx, cx*cellW + (cellW-dw)/2, cy*cellH + (cellH-dh)/2, dw, dh);
    }
  });
  return overflow;
}

/* SHELF -- next-fit-decreasing rows. Sort tall-first, lay boxes left-to-right
   on a shelf until the row overflows, then start a new shelf. A global scale
   fits every shelf into the 0-1 square. Weight enlarges a box's shelf slot. */
function packShelf(boxes, margin, obstacles = [], packOpts = {}) {
  const pinned = obstacles.length > 0;
  boxes.sort((a, b) => b.bh - a.bh);
  // Size a frame that fits the free box area PLUS the pins' proportional share (with
  // headroom), the SAME way packSkyline does -- so free boxes stay small relative to
  // the frame and shelve AROUND the pins instead of each filling the tile and
  // stacking in a corner. Pins are scaled into this frame (pinScale) so a shelf that
  // slides past a pin lands beside it; the final 1/width normalize returns the pins
  // to their true UV.
  const rowLimit = boxes.reduce((s, b) => Math.max(s, b.bw), 0);
  const freeArea = boxes.reduce((s,b)=>s+(b.bw+margin)*(b.bh+margin),0);
  const pinAreaUV = obstacles.reduce((s,o)=>s+o.w*o.h,0);
  const HEADROOM = 1.3;
  const width = pinned
    ? Math.max(1, rowLimit, Math.sqrt((freeArea + freeArea * pinAreaUV / Math.max(1e-6, 1 - pinAreaUV)) * HEADROOM))
    : Math.max(1, Math.sqrt(freeArea), rowLimit);
  const pinScale = width;
  const fobs = obstacles.map(o => ({ x: o.x*pinScale, y: o.y*pinScale, w: o.w*pinScale, h: o.h*pinScale }));
  const hitsPin = (x, y, w, h) => fobs.some(o => rectsOverlap(x, y, w, h, o.x, o.y, o.w, o.h, margin));
  let x = 0, y = 0, rowH = 0, usedW = 0, totalH = 0, overflow = 0;
  const placed = [];
  for (const bx of boxes) {
    const w = bx.bw + margin * 2, h = bx.bh + margin * 2;
    if (x + w > width && x > 0) { y += rowH; x = 0; rowH = 0; }
    // when pinned, slide right past any obstacle this slot collides with.
    if (pinned) {
      let guard = 0;
      while (hitsPin(x + margin, y + margin, bx.bw, bx.bh) && guard < 64) {
        // step past the colliding obstacle's right edge, wrapping to a new shelf.
        const hit = fobs.find(o => rectsOverlap(x + margin, y + margin, bx.bw, bx.bh, o.x, o.y, o.w, o.h, margin));
        x = hit ? hit.x + hit.w + margin : x + w;
        if (x + w > width) { y += rowH; x = 0; rowH = 0; }
        guard++;
      }
      if (hitsPin(x + margin, y + margin, bx.bw, bx.bh)) overflow++;   // couldn't clear
    }
    placed.push({ bx, x: x + margin, y: y + margin, w: bx.bw, h: bx.bh });
    x += w; rowH = Math.max(rowH, h); usedW = Math.max(usedW, x); totalH = Math.max(totalH, y + rowH);
  }
  // With pins: normalize by the FRAME width so the free set stays aligned with the
  // pins (seeded at pin_uv * width); the extent may exceed width if shelves spilled,
  // so guard with the actual extent to keep everything in 0-1. Without pins: fit.
  const scale = pinned
    ? 1 / Math.max(usedW, totalH, width, 1e-6)
    : 1 / Math.max(usedW, totalH, 1e-6);
  for (const p of placed) placeIsland(p.bx, p.x*scale, p.y*scale, p.w*scale, p.h*scale);
  return overflow;
}

/* do two AABBs (x,y,w,h) overlap, with a `gap` clearance? */
function rectsOverlap(ax, ay, aw, ah, bx, by, bw, bh, gap) {
  const g = gap || 0;
  return ax < bx + bw + g && ax + aw + g > bx && ay < by + bh + g && ay + ah + g > by;
}

/* SKYLINE -- bottom-left skyline (guillotine-free) packer. Maintains a skyline
   of segments; each box drops into the lowest, leftmost slot that fits. Packs
   tighter than shelf for mixed sizes. A final global scale normalizes to 0-1.
   When `obstacles` (pinned-shell AABBs, 0-1 frame) are present the pack runs in
   the pins' FIXED coordinate frame: obstacles seed the skyline, candidates that
   collide with a pin are rejected, and the final rescale is suppressed (the free
   set must stay aligned with the immovable pins). Returns the overflow count --
   free boxes that couldn't clear every pin (placed anyway). */
function packSkyline(boxes, margin, obstacles = [], packOpts = {}) {
  const pinned = obstacles.length > 0;
  boxes.sort((a, b) => (b.bh) - (a.bh));
  // Size a square frame that fits the summed box area PLUS the pinned obstacles'
  // area -- so free boxes stay small relative to the frame and tile AROUND the
  // pins instead of each filling the whole tile and stacking in a corner. The
  // pins are expressed in this same frame (pinScale below), so a bottom-left
  // skyline routes free boxes into the gaps beside/above/below them. A final
  // normalize maps the whole frame back to 0-1, returning pins to their UV.
  const freeArea = boxes.reduce((s,b)=>s+(b.bw+margin*2)*(b.bh+margin*2),0);
  // pin area in the SAME box-units as the free set: the pin AABB spans a fraction
  // (o.w*o.h) of the unit tile, and the free boxes fill the rest, so the frame must
  // hold BOTH. HEADROOM(1.3) leaves slack a skyline can't otherwise use -- without
  // it a frame sized to the exact summed area always overflows and free boxes fall
  // to the corner-stack fallback (the "C on B on A" bug).
  const HEADROOM = 1.3;
  const pinAreaUV = obstacles.reduce((s,o)=>s+o.w*o.h,0);
  // frame area = free box area + the pin's proportional share, with headroom.
  const frameArea = (freeArea + freeArea * pinAreaUV / Math.max(1e-6, 1 - pinAreaUV)) * HEADROOM;
  const width0 = Math.max(1, Math.sqrt(frameArea));

  // attempt(width) runs one full bottom-left skyline pass in a square frame of the
  // given width: pins seeded at pin_uv*width, free boxes routed around them. Returns
  // the placements (in frame units) + the overflow count (boxes that couldn't clear
  // every pin, stacked on the fallback). Pure of outer state so the caller can RETRY
  // with a larger frame when it overflows -- growing the frame keeps pins at true UV
  // after the 1/width normalize while shrinking the free boxes to make room, which
  // is the correct trade (give up atlas area to fit around a big/corner pin rather
  // than stacking islands on one point -- the "C on B on A" bug the retry defeats).
  function attempt(width) {
    const pinScale = width;
    let skyline = [{ x: 0, y: 0, w: width }];
    const placed = [];
    function addRect(x, y, w, h) {
      const nx0 = x, nx1 = x + w, top = y + h;
      const next = [];
      for (const seg of skyline) {
        const sx0 = seg.x, sx1 = seg.x + seg.w;
        if (sx1 <= nx0 || sx0 >= nx1) { next.push(seg); continue; }   // untouched
        if (sx0 < nx0) next.push({ x: sx0, y: seg.y, w: nx0 - sx0 });
        if (sx1 > nx1) next.push({ x: nx1, y: seg.y, w: sx1 - nx1 });
      }
      next.push({ x: nx0, y: top, w: w });
      next.sort((a, b) => a.x - b.x);
      // merge equal-height neighbours
      skyline = next.reduce((acc, s) => {
        const last = acc[acc.length - 1];
        if (last && Math.abs(last.y - s.y) < 1e-9 && Math.abs(last.x + last.w - s.x) < 1e-9) last.w += s.w;
        else acc.push({ ...s });
        return acc;
      }, []);
    }
    // pinned obstacles scaled into the frame (UV -> frame units).
    const fobs = obstacles.map(o => ({ x: o.x*pinScale, y: o.y*pinScale, w: o.w*pinScale, h: o.h*pinScale }));
    // seed each pinned obstacle: raise the skyline over its x-span to its top edge
    // so free boxes never drop into a pin's column below the pin.
    for (const o of fobs) addRect(o.x, 0, o.w, o.y + o.h);
    // does a candidate free rect [x,y,w,h] collide with any pinned obstacle?
    const hitsPin = (x, y, w, h) => fobs.some(o => rectsOverlap(x, y, w, h, o.x, o.y, o.w, o.h, margin));
    // lowest y at which a box of width `w` starting at `x` clears the CURRENT skyline
    // (placed boxes + seeded pins). Returns null if [x,x+w] runs off the frame.
    function skylineHeightAt(x, w) {
      if (x < -1e-9 || x + w > width + 1e-9) return null;
      let y = 0;
      for (const seg of skyline) {
        if (seg.x + seg.w <= x + 1e-9 || seg.x >= x + w - 1e-9) continue;   // no x-overlap
        if (seg.y > y) y = seg.y;
      }
      return y;
    }
    let totalH = 0, usedW = 0, overflow = 0;
    // candidate x positions: every skyline segment start PLUS every pinned obstacle's
    // right edge (+margin). A skyline seed raises the bar ABOVE a pin, so without the
    // pin-right-edge candidate a box could only stack ON TOP of the pin; this lets it
    // drop into the free column BESIDE the pin. Each candidate's y is the skyline
    // height there (so boxes never overlap each other) and is then pin-tested.
    const pinRightEdges = fobs.map(o => o.x + o.w + margin);
    for (const bx of boxes) {
      const w = bx.bw + margin * 2, h = bx.bh + margin * 2;
      let best = null, bestY = Infinity, bestX = Infinity;
      const consider = (x) => {
        let y = skylineHeightAt(x, w);
        if (y == null) return;
        // if the slot abuts a pin (margin-touch), lift it above the pin by the gap
        // and retest -- so a box can sit in the row directly ABOVE a pin, not skip it.
        if (hitsPin(x, y, w, h)) {
          let lift = 0;
          for (const o of fobs) if (rectsOverlap(x, y, w, h, o.x, o.y, o.w, o.h, margin)) lift = Math.max(lift, o.y + o.h + margin);
          if (lift > y) y = lift;
          if (hitsPin(x, y, w, h)) return;                  // still on a pin (side overlap) -> skip
        }
        if (y + h > width + 1e-9) return;                   // would exceed frame height -> wrap
        if (y < bestY || (y === bestY && x < bestX)) { best = { x, y }; bestY = y; bestX = x; }
      };
      for (const seg of skyline) consider(seg.x);
      for (const rx of pinRightEdges) consider(rx);
      if (!best) {
        // no slot in THIS frame -> place within it anyway and, when pins are present,
        // count it so the caller grows the frame and retries (a real overflow only
        // survives when the free set genuinely cannot fit around the pin). With NO
        // pins the frame is just a wrap boundary and the final extent-normalize fits
        // everything, so this is not an overflow -- don't count it.
        best = { x: 0, y: Math.min(totalH, Math.max(0, width - h)) };
        if (obstacles.length) overflow++;
      }
      placed.push({ bx, x: best.x + margin, y: best.y + margin, w: bx.bw, h: bx.bh });
      addRect(best.x, best.y, w, h);
      totalH = Math.max(totalH, best.y + h); usedW = Math.max(usedW, best.x + w);
    }
    return { placed, overflow, usedW, totalH };
  }

  let width = width0, result = attempt(width);
  if (pinned) {
    // retry with a 1.25x-larger frame while boxes still overflow (stack), capped so a
    // genuinely-unfittable set (pin covers nearly the whole tile) terminates + reports
    // honest overflow rather than looping. Each grow shrinks the free set by 1/width
    // after normalize, buying room around the pin.
    let tries = 0;
    while (result.overflow > 0 && tries < 12) { width *= 1.25; result = attempt(width); tries++; }
  }
  const { placed, usedW, totalH } = result;
  // With pins: normalize by the FRAME width, NOT the packed extent. The pins were
  // seeded at pin_uv * width, so dividing the free set by width returns the pins to
  // their true UV AND keeps the free boxes in the gaps they were routed into (same
  // scale as the pins -- normalizing by a larger extent would shrink the free set
  // off the pins and cause overlap). Any surviving overflow box was clamped into the
  // frame above, so 1/width keeps everything in 0-1. Without pins: extent-fit.
  const scale = pinned ? 1 / width : 1 / Math.max(usedW, totalH, 1e-6);
  for (const p of placed) placeIsland(p.bx, p.x*scale, p.y*scale, p.w*scale, p.h*scale);
  return result.overflow;
}

/* SHAPE-AWARE -- packs each island's TRUE footprint (not its AABB) so concave /
   diagonal / L-shaped shells interlock and the dead space around a bounding box
   is reclaimed (backlog #10). Each island is rasterized into a small occupancy
   bitmap; a bottom-left scan drops each mask into the first free slot, trying the
   four 90-degree rotations so a concave notch can accept a neighbour's tab. Falls
   back to the box path per island only when a mask can't be built. A final global
   scale normalizes the packed extent into the 0-1 square.

   The grid resolution is derived from the box count so cost stays bounded: more
   islands -> a coarser per-island mask (fewer cells) but a wider shared grid. The
   masks keep each island's native aspect + weight (mask cell area ~ bw*bh, i.e.
   ~ weight), so importance still drives size exactly like measureBoxes. */
const SHAPE_GRID = 128;            // shared occupancy grid resolution (cells per side)
const SHAPE_MASK_MAX = 40;         // cap a single island's mask side (bounds cost)

/* rasterize one box's island footprint into a {w,h,bits} occupancy mask at the
   given cells-per-unit resolution. bits[y*w+x] = 1 where a fan triangle covers
   the cell centre. The mask spans the island's native UV bbox scaled by the box's
   weighted (bw,bh), so a weight-4 island rasterizes into ~4x the cells. */
function shapeIslandMask(box, cellsPerUnit) {
  const { isl, b, bw, bh } = box;
  let w = Math.max(1, Math.min(SHAPE_MASK_MAX, Math.round(bw * cellsPerUnit)));
  let h = Math.max(1, Math.min(SHAPE_MASK_MAX, Math.round(bh * cellsPerUnit)));
  const bits = new Uint8Array(w * h);
  const sx = w / (b.w || 1e-6), sy = h / (b.h || 1e-6);
  // map a vert's native UV into mask cell space
  const mx = v => (isl.uv[v][0] - b.mnU) * sx;
  const my = v => (isl.uv[v][1] - b.mnV) * sy;
  let filled = 0;
  for (const f of isl.faces) {
    for (const [a, c, d] of TOPO.faceTris[f]) {
      if (!isl.uv[a] || !isl.uv[c] || !isl.uv[d]) continue;
      const ax = mx(a), ay = my(a), cx = mx(c), cy = my(c), dx = mx(d), dy = my(d);
      const minX = Math.max(0, Math.floor(Math.min(ax, cx, dx)));
      const maxX = Math.min(w - 1, Math.ceil(Math.max(ax, cx, dx)));
      const minY = Math.max(0, Math.floor(Math.min(ay, cy, dy)));
      const maxY = Math.min(h - 1, Math.ceil(Math.max(ay, cy, dy)));
      const area = (cx - ax) * (dy - ay) - (dx - ax) * (cy - ay);
      if (Math.abs(area) < 1e-12) continue;
      const inv = 1 / area;
      for (let py = minY; py <= maxY; py++) {
        for (let px = minX; px <= maxX; px++) {
          if (bits[py * w + px]) continue;
          const qx = px + 0.5, qy = py + 0.5;
          // barycentric coverage of the cell centre
          const l1 = ((cx - qx) * (dy - qy) - (dx - qx) * (cy - qy)) * inv;
          const l2 = ((dx - qx) * (ay - qy) - (ax - qx) * (dy - qy)) * inv;
          const l3 = 1 - l1 - l2;
          if (l1 >= -1e-6 && l2 >= -1e-6 && l3 >= -1e-6) { bits[py * w + px] = 1; filled++; }
        }
      }
    }
  }
  // a degenerate/empty rasterization falls back to a solid box so the island
  // still packs (never drops it) -- guarantees every island is placed.
  if (!filled) bits.fill(1);
  return { w, h, bits };
}

/* rotate a mask by 90*steps degrees (steps 0..3), returning a new mask. Keeps
   shape-aware honest about orientation so a tall notch can accept a wide tab. */
function shapeRotateMask(mask, steps) {
  steps = ((steps % 4) + 4) % 4;
  if (steps === 0) return mask;
  const { w, h, bits } = mask;
  const rw = (steps % 2) ? h : w, rh = (steps % 2) ? w : h;
  const out = new Uint8Array(rw * rh);
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
    if (!bits[y * w + x]) continue;
    let nx, ny;
    if (steps === 1) { nx = h - 1 - y; ny = x; }
    else if (steps === 2) { nx = w - 1 - x; ny = h - 1 - y; }
    else { nx = y; ny = w - 1 - x; }
    out[ny * rw + nx] = 1;
  }
  return { w: rw, h: rh, bits: out };
}

function shapeMaskArea(mask) {
  let n = 0; for (let i = 0; i < mask.bits.length; i++) n += mask.bits[i];
  return n;
}

function packShapeAware(boxes, margin, obstacles = [], packOpts = {}) {
  if (!boxes.length) return 0;
  const pinned = obstacles.length > 0;
  const grid = SHAPE_GRID;
  // cells-per-unit sizes the masks so the summed mask area roughly fills the grid.
  // Σ(bw*bh) is the total weighted box area (~ Σ weight); solve for the scale that
  // maps it to a comfortable fraction of the grid so islands aren't microscopic.
  //
  // With pins present the pack runs in the pins' FIXED 0-1 frame (no post-rescale),
  // so cellsPerUnit must be exactly `grid` cells per UV unit -- then the free set's
  // grid cells map straight back to UV via scale = 1/cellsPerUnit and stay aligned
  // with the pinned masks seeded from the same UV->cell mapping.
  const boxArea = boxes.reduce((s, b) => s + Math.max(b.bw * b.bh, 1e-6), 0);
  const cellsPerUnit = pinned ? grid : Math.sqrt((grid * grid * 0.62) / boxArea);
  const pad = Math.max(1, Math.round(margin * cellsPerUnit));   // gap between masks, in cells
  // build each island's base mask, keep the largest first (bottom-left decreasing)
  const items = boxes.map(box => ({ box, mask: shapeIslandMask(box, cellsPerUnit) }));
  items.sort((a, b) => shapeMaskArea(b.mask) - shapeMaskArea(a.mask));
  // occupancy of the shared grid; skyline gives the bottom-left candidate y per column
  const occ = new Uint8Array(grid * grid);
  const colTop = new Int32Array(grid);          // lowest free y per column (skyline)
  const placed = [];
  let usedW = 0, usedH = 0;
  // pad-dilate a mask ONCE (Minkowski-grow the footprint by `pad`) so the per-slot
  // fit test is a plain occupancy overlap -- no inner pad loop in the hot path.
  const dilate = (mask) => {
    if (pad <= 0) return mask;
    const { w, h, bits } = mask;
    const out = new Uint8Array(w * h);
    for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
      if (!bits[y * w + x]) continue;
      for (let dy = -pad; dy <= pad; dy++) for (let dx = -pad; dx <= pad; dx++) {
        const nx = x + dx, ny = y + dy;
        if (nx >= 0 && ny >= 0 && nx < w && ny < h) out[ny * w + nx] = 1;
      }
    }
    return { w, h, bits: out };
  };
  // does the dilated `dm` overlap occupancy with its corner at (ox,oy)?
  const fits = (dm, ox, oy) => {
    if (ox + dm.w > grid || oy + dm.h > grid) return false;
    const b = dm.bits, w = dm.w, h = dm.h;
    for (let y = 0; y < h; y++) {
      const row = (oy + y) * grid + ox, mr = y * w;
      for (let x = 0; x < w; x++) if (b[mr + x] && occ[row + x]) return false;
    }
    return true;
  };
  const stamp = (mask, ox, oy) => {
    for (let y = 0; y < mask.h; y++) for (let x = 0; x < mask.w; x++) {
      if (mask.bits[y * mask.w + x]) occ[(oy + y) * grid + (ox + x)] = 1;
    }
  };
  // seed pinned shells: rasterize each pinned island's TRUE footprint directly
  // into the grid at its FIXED UV position (tile origin subtracted), then raise
  // colTop over its span. Free islands then pack around the real shell, not just
  // its box. Pins live in the same cell frame (cellsPerUnit = grid), so their
  // cell coords are UV*grid within the tile.
  const tileOrigin = (packOpts && packOpts.tile) ? packOpts.tile : { tu: 0, tv: 0 };
  if (pinned && packOpts && packOpts.pinned) {
    for (const isl of packOpts.pinned) {
      for (const f of isl.faces) {
        for (const [a, c, d] of TOPO.faceTris[f]) {
          if (!isl.uv[a] || !isl.uv[c] || !isl.uv[d]) continue;
          const ax = (isl.uv[a][0] - tileOrigin.tu) * grid, ay = (isl.uv[a][1] - tileOrigin.tv) * grid;
          const cx = (isl.uv[c][0] - tileOrigin.tu) * grid, cy = (isl.uv[c][1] - tileOrigin.tv) * grid;
          const dx = (isl.uv[d][0] - tileOrigin.tu) * grid, dy = (isl.uv[d][1] - tileOrigin.tv) * grid;
          const minX = Math.max(0, Math.floor(Math.min(ax, cx, dx) - pad));
          const maxX = Math.min(grid - 1, Math.ceil(Math.max(ax, cx, dx) + pad));
          const minY = Math.max(0, Math.floor(Math.min(ay, cy, dy) - pad));
          const maxY = Math.min(grid - 1, Math.ceil(Math.max(ay, cy, dy) + pad));
          const area = (cx - ax) * (dy - ay) - (dx - ax) * (cy - ay);
          if (Math.abs(area) < 1e-9) continue;
          const inv = 1 / area;
          for (let py = minY; py <= maxY; py++) for (let px = minX; px <= maxX; px++) {
            if (occ[py * grid + px]) continue;
            const qx = px + 0.5, qy = py + 0.5;
            const l1 = ((cx - qx) * (dy - qy) - (dx - qx) * (cy - qy)) * inv;
            const l2 = ((dx - qx) * (ay - qy) - (ax - qx) * (dy - qy)) * inv;
            const l3 = 1 - l1 - l2;
            // pad-dilate the pin footprint so free masks keep a `pad` gap from it.
            if (l1 >= -0.5 && l2 >= -0.5 && l3 >= -0.5) occ[py * grid + px] = 1;
          }
        }
      }
      // raise colTop across the pin's cell x-span to its top so free boxes don't
      // scan below it (a cheap skyline seed; occ still guards exact overlaps).
      const b = islandBounds(isl);
      const px0 = Math.max(0, Math.floor((b.mnU - tileOrigin.tu) * grid));
      const px1 = Math.min(grid - 1, Math.ceil((b.mxU - tileOrigin.tu) * grid));
      const top = Math.min(grid, Math.ceil((b.mxV - tileOrigin.tv) * grid) + pad);
      for (let x = px0; x <= px1; x++) if (top > colTop[x]) colTop[x] = top;
    }
  }
  let overflow = 0;
  for (const it of items) {
    // try 4 rotations; for each, scan bottom-left for the first free slot
    let best = null;
    for (let rot = 0; rot < 4; rot++) {
      const m = shapeRotateMask(it.mask, rot);
      if (m.w + pad > grid || m.h + pad > grid) continue;
      const dm = dilate(m);
      for (let ox = 0; ox + dm.w <= grid; ox++) {
        // bottom-left rule: nothing can fit below the current skyline under the
        // mask's span, so start the upward walk at the max colTop over [ox, ox+m.w).
        let oy0 = 0;
        for (let x = 0; x < dm.w; x++) if (colTop[ox + x] > oy0) oy0 = colTop[ox + x];
        if (best && oy0 * grid + ox >= best.score) continue;   // can't beat the incumbent
        let found = -1;
        for (let oy = oy0; oy + dm.h <= grid; oy++) {
          if (fits(dm, ox, oy)) { found = oy; break; }
        }
        if (found < 0) continue;
        const score = found * grid + ox;      // bottom-most, then left-most
        if (!best || score < best.score) best = { m, ox, oy: found, score, rot };
      }
    }
    if (!best) {                                // no fit anywhere -> stack on top
      const m = it.mask;
      best = { m, ox: 0, oy: Math.min(Math.max(0, grid - m.h - pad), usedH), score: 0, rot: 0 };
      if (pinned) overflow++;                    // couldn't clear the pinned shells
    }
    stamp(best.m, best.ox, best.oy);
    for (let x = 0; x < best.m.w; x++) colTop[best.ox + x] = Math.max(colTop[best.ox + x], best.oy + best.m.h + pad);
    // record placement in grid-cell space, tagged with the rotation so write-back
    // maps the island's native UV into the placed+rotated slot.
    placed.push({ box: it.box, ox: best.ox, oy: best.oy, mw: best.m.w, mh: best.m.h, rot: best.rot });
    usedW = Math.max(usedW, best.ox + best.m.w + pad);
    usedH = Math.max(usedH, best.oy + best.m.h + pad);
  }
  // with pins, keep the fixed frame (grid cells -> UV via 1/cellsPerUnit) so the
  // free set stays aligned with the seeded pinned shells; otherwise normalize the
  // packed extent into the 0-1 square as before.
  const scale = pinned ? (1 / cellsPerUnit) : (1 / Math.max(usedW, usedH, 1e-6));
  for (const p of placed) shapePlaceIsland(p, scale);
  return overflow;
}

/* write an island's verts into its placed grid slot [ox,oy] of size [mw,mh]
   (cells), scaled to 0-1 by `scale`, applying the chosen 90-degree rotation to
   the island's native UV bbox so the rendered shape matches the packed mask. */
function shapePlaceIsland(p, scale) {
  const { box, ox, oy, mw, mh, rot } = p;
  const { isl, b } = box;
  // target rectangle in unit space
  const rx = ox * scale, ry = oy * scale, rw = mw * scale, rh = mh * scale;
  // rot 0/2 keep the native aspect; 1/3 swap axes. Map native (u,v) in [0,1]^bbox
  // through the rotation, then into the target rect.
  for (const v of isl.verts) {
    const nu = (isl.uv[v][0] - b.mnU) / (b.w || 1e-6);   // 0..1 across native bbox
    const nv = (isl.uv[v][1] - b.mnV) / (b.h || 1e-6);
    let tu, tv;
    if (rot === 1) { tu = nv; tv = 1 - nu; }             // 90
    else if (rot === 2) { tu = 1 - nu; tv = 1 - nv; }    // 180
    else if (rot === 3) { tu = 1 - nv; tv = nu; }        // 270
    else { tu = nu; tv = nv; }                           // 0
    isl.uv[v] = [rx + tu * rw, ry + tv * rh];
  }
}

const PACKERS = { grid: packGrid, shelf: packShelf, skyline: packSkyline, shapeAware: packShapeAware };
let PACK_METHOD = "skyline";

/* ---- packing margin (backlog: px @ resolution) -------------------------
   DCC-style island gap: the user sets a pixel margin against a texture
   resolution, and the packer converts it to the fractional gap it already
   consumes (margin is in 0-1 UV units). marginPx = 0 means "untouched" -> fall
   back to the historical 0.01 default, so existing behaviour is unchanged until
   the control is used. These conversions are pure (exported for the smoketest). */
const PACK = { marginPx: 0, marginRes: 1024 };
/* pixels -> fractional UV margin at a texture resolution. */
function pixelsToMargin(px, res) { return res > 0 ? px / res : 0; }
/* fractional UV margin -> pixels at a texture resolution. */
function marginToPixels(frac, res) { return frac * res; }
/* set the pixel margin + resolution driving the pack gap. */
function setPackMargin(px, res) {
  PACK.marginPx = Math.max(0, px || 0);
  if (res > 0) PACK.marginRes = res;
}
/* the current fractional margin, or null when the px control is untouched
   (marginPx = 0) so packIslands can fall back to its 0.01 default. */
function packMarginFraction() {
  return PACK.marginPx > 0 ? pixelsToMargin(PACK.marginPx, PACK.marginRes) : null;
}

/* the 0-1-relative AABB an island currently occupies -- reused as a packer
   obstacle so free islands avoid a pinned shell's footprint. Offset by `origin`
   (its tile's (tu,tv)) so a per-tile pack works in the normalized 0-1 frame. */
function islandAABB(isl, origin) {
  const b = islandBounds(isl);
  const ou = origin ? origin.tu : 0, ov = origin ? origin.tv : 0;
  return { x: b.mnU - ou, y: b.mnV - ov, w: b.w, h: b.h };
}

/* which tile does an island BELONG to? This is its explicit assignment only --
   never the floor of its geometry. An untagged island defaults to (0,0) (the
   active tile at pack time). Using geometry here was wrong: freshly-unwrapped
   shells sit at varying v, so floor(bbox) scattered one shell per tile and a
   Repack laddered them diagonally across UDIMs instead of packing them together. */
function islandTile(isl) {
  if (isl.tile) return { tu: isl.tile.tu | 0, tv: isl.tile.tv | 0 };
  return { tu: 0, tv: 0 };
}

/* do two tiles address the same UDIM cell? */
function sameTile(a, b) { return a.tu === b.tu && a.tv === b.tv; }

/* pack island boxes into a target UDIM tile using the active algorithm, honoring
   per-island importance weights and PINNED islands. Pinned islands are never
   moved or resized: they are excluded from placement and seeded as obstacles so
   the free set packs AROUND them. Only islands belonging to the target tile
   (opts.tile, default the active UDIM tile / (0,0)) participate -- pins in other
   tiles never obstruct this one. Mutates island uv in place. Returns
   { islands, overflow } where overflow counts free islands that could not clear
   the pinned footprints (placed anyway, per the warn+allow-overlap policy). */
function packIslands(islands, opts = {}) {
  if (!islands.length) return { islands, overflow: 0 };
  const margin = opts.margin != null
    ? opts.margin
    : (packMarginFraction() != null ? packMarginFraction() : 0.01);
  const method = opts.method || PACK_METHOD;
  const activeTile = (typeof UDIM !== "undefined" && UDIM.activeTile) ? UDIM.activeTile : { tu: 0, tv: 0 };
  const tile = opts.tile || activeTile;
  // opts.subset -- when present, ONLY these islands are candidates to be packed
  // (selection-driven Repack). Unselected free islands are left untouched (they
  // stay put even if they overlap). Pinned islands in the target tile ALWAYS
  // obstruct, selected or not. When absent, the whole tile's free set packs.
  const subset = opts.subset ? new Set(opts.subset) : null;
  // pinned obstacles are the pins that BELONG to the target tile. Free candidates:
  //  - with a subset (selection): the selected unpinned islands, pulled INTO the
  //    target tile regardless of their prior tile (stamped below). Selecting +
  //    packing is an explicit "bring these here".
  //  - without a subset: the unpinned islands already belonging to the tile.
  const pinned = islands.filter(isl => isl.pinned && sameTile(islandTile(isl), tile));
  let free;
  if (subset) {
    free = islands.filter(isl => subset.has(isl) && !isl.pinned);
    for (const isl of free) isl.tile = { tu: tile.tu, tv: tile.tv };
  } else {
    free = islands.filter(isl => !isl.pinned && sameTile(islandTile(isl), tile));
  }
  const obstacles = pinned.map(isl => islandAABB(isl, tile));
  if (!free.length) return { islands, overflow: 0 };
  const boxes = measureBoxes(free);
  const overflow = (PACKERS[method] || packSkyline)(boxes, margin, obstacles, { tile, pinned }) || 0;
  // offset the freshly-packed free islands from the 0-1 pack frame into the tile.
  if (tile.tu || tile.tv) {
    for (const isl of free) for (const v of isl.verts) { isl.uv[v][0] += tile.tu; isl.uv[v][1] += tile.tv; }
  }
  return { islands, overflow };
}

/* ======================= PUBLIC ENTRY ======================= */
const PROJECTORS = {
  planar: projectPlanar,
  box: projectBox,
  cylindrical: projectCylindrical,
  spherical: projectSpherical,
};
/* Smart UV Project (Blender-style): partition faces into islands by walking
   adjacency and refusing to cross an edge whose two faces differ in normal by
   more than `angleLimit` (radians, Blender default 66deg). Each resulting patch
   is a near-developable chart that LSCM flattens with low distortion. */
function smartPartition(faces, angleLimit = 66 * Math.PI / 180) {
  const cosLimit = Math.cos(angleLimit);
  const inSet = new Set(faces);
  const visited = new Set();
  const patches = [];
  for (const seed of faces) {
    if (visited.has(seed)) continue;
    const grp = [], stack = [seed]; visited.add(seed);
    while (stack.length) {
      const f = stack.pop(); grp.push(f);
      const nf0 = TOPO.faceNormal[f];
      for (const k of TOPO.faceEdges[f]) {
        if (SEAMS.has(k)) continue;                       // honor user seams too
        for (const nf of TOPO.edgeFaces[k]) {
          if (!inSet.has(nf) || visited.has(nf)) continue;
          const n = TOPO.faceNormal[nf];
          const dot = nf0[0]*n[0] + nf0[1]*n[1] + nf0[2]*n[2];
          if (dot < cosLimit) continue;                   // fold crease -> chart boundary
          visited.add(nf); stack.push(nf);
        }
      }
    }
    patches.push(grp);
  }
  return patches;
}

function unwrap(method) {
  const faces = targetFaces();
  let islands;
  if (method === "unwrap") {
    // Blender's default Unwrap: LSCM per connected chart, cut at the chart's own
    // boundary. facesToIslands splits across marked seams, so each component here
    // is exactly one chart -- either a user-seamed piece or, for a bare selection
    // with no seams, the selected patch itself (whose selection edge IS its cut).
    //
    // A chart flattens as ONE island when it has ENOUGH open boundary to be a
    // disk that LSCM can lay flat without gross distortion:
    //   • a partial selection off a larger shell is open by construction, and
    //   • a seam-cut chart is open where the seam runs.
    // This matches Blender -- select a shell, press U, get ONE filled island that
    // fills the UV space, rather than the layout shattering into axis fragments.
    //
    // When the user has marked SEAMS, they ARE the intended cuts: honor them
    // literally. facesToIslands already split each chart at the seams, so any
    // component with an open boundary flattens as one island -- we never auto-cut
    // on top of a user's seams (that would over-fragment a deliberately-seamed
    // chart). Only a component still fully CLOSED after seaming (zero boundary)
    // needs a fallback cut.
    //
    // With NO seams, "cut at the chart's own boundary" is measured by
    // openBoundaryRatio -- the share of the chart's edges that lie on its open
    // boundary. A disk-like patch (a partial selection, or an eye shell) sits well
    // above DISK_BOUNDARY_RATIO and flattens as one island; a near-CLOSED blob (a
    // big shell with only a pinhole opening, e.g. Suzanne's head) sits below it and
    // would flatten to a collapsed sliver, so it falls back to the disk-heuristic
    // auto-cut. A flat sqrt(N) bar can't tell the two apart: a large closed shell
    // clears sqrt(N) on raw boundary count yet is nowhere near a disk -- the ratio
    // is scale-invariant and separates them cleanly. This is what makes "select a
    // shell, press U" yield ONE filled island instead of axis fragments.
    const haveSeams = SEAMS.size > 0;
    islands = [];
    for (const comp of facesToIslands(faces)) {
      const flattenAsOne = haveSeams
        ? boundaryEdgeCount(comp) > 0
        : openBoundaryRatio(comp) >= DISK_BOUNDARY_RATIO;
      if (flattenAsOne) islands.push(lscmIsland(comp));
      else for (const disk of autoCutForConformal(comp)) islands.push(lscmIsland(disk));
    }
  } else if (method === "smart") {
    // Smart UV Project: angle-based charts, each flattened conformally.
    islands = smartPartition(faces).map(lscmIsland);
  } else if (method === "conformal") {
    islands = [];
    for (const comp of facesToIslands(faces)) {
      for (const disk of autoCutForConformal(comp)) islands.push(lscmIsland(disk));
    }
  } else if (PROJECTORS[method]) {
    // projections may already return multiple islands (box). Split each by
    // connectivity, but fold sub-4-face slivers into the group's biggest patch
    // so the layout stays a handful of coherent shells rather than hundreds.
    islands = [];
    for (const grp of PROJECTORS[method](faces)) {
      const sub = facesToIslands(grp.faces).sort((a, b) => b.length - a.length);
      const keep = sub.filter(p => p.length >= 4);
      const strays = sub.filter(p => p.length < 4).flat();
      if (keep.length && strays.length) keep[0].push(...strays);
      else if (!keep.length && strays.length) keep.push(strays);
      for (const sf of keep) {
        const verts = islandVerts(sf), uv = {};
        for (const v of verts) uv[v] = grp.uv[v];
        islands.push({ faces: sf, verts, uv });
      }
    }
  } else {
    islands = facesToIslands(faces).map(lscmIsland);
  }
  islands = islands.filter(i => i.faces.length);
  // symmetric-unwrap parity: mirror-copy L/R pairs so both sides are identical
  // (backlog #5). Runs before packing -- the packer then places each island, but
  // their INTERNAL mirrored UVs stay pixel-identical relative to their own bounds.
  pairSymmetricIslands(islands);
  packIslands(islands);
  return islands;
}

/* set the active packing algorithm ("grid" | "shelf" | "skyline") */
function setPackMethod(m) { if (PACKERS[m]) PACK_METHOD = m; }
function packMethod() { return PACK_METHOD; }

if (typeof module !== "undefined") {
  module.exports = {
    unwrap, facesToIslands, lscmIsland, packIslands, targetFaces,
    projectPlanar, projectBox, projectCylindrical, projectSpherical,
    setPackMethod, packMethod, islandWeight, PACKERS,
    sliderToWeight, weightToSlider, percentToWeight, weightToPercent,
    WEIGHT_SLIDER_MIN, WEIGHT_SLIDER_MAX,
    pairSymmetricIslands, mirrorIslandUv,
    packShapeAware, shapeIslandMask, shapeRotateMask, shapeMaskArea,
    PACK, pixelsToMargin, marginToPixels, setPackMargin, packMarginFraction,
    islandAABB, islandTile, sameTile,
  };
}
