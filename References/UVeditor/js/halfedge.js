"use strict";
/* ==========================================================================
   halfedge.js -- topology adjacency over the polygon mesh (MESH.faces).
   Built once at load. Provides the neighbour queries that grow / shrink /
   select-similar / select-island / select-perimeter / edge-loop all rely on.

   Faces are ORIGINAL polygons (tris / quads / ngons): MESH.faces is a jagged
   array of vertex loops, faceVerts[f] carries the loop of arity N. Edges are
   the N loop edges (consecutive pairs, wrap-around) -- no diagonals. For the
   CPU rasterizer / picker, each polygon also gets a fan triangulation:
   faceTris[f] (N-2 triangles) plus a flat TOPO.tris list that maps every
   triangle back to its parent face.

   Vertices are shared indices into MESH.positions. An "edge" is an unordered
   vertex pair identified by a stable string key. A "face" is a polygon index
   into MESH.faces.
   ========================================================================== */
const TOPO = {
  faceCount: 0,
  faceVerts: [],       // [ [v0,v1,..], ... ] original polygon loops (arity N)
  faceTris: [],        // faceIndex -> [ [a,b,c], ... ] fan triangulation (N-2)
  tris: [],            // flat [ {v:[a,b,c], face}, ... ] for raster/pick
  faceNormal: [],      // [ [nx,ny,nz], ... ] geometric per-face normal (Newell)
  faceCenter: [],      // [ [x,y,z], ... ] loop centroid
  vertFaces: [],       // vertIndex -> [faceIndex, ...]
  vertNeighbors: [],   // vertIndex -> Set(vertIndex) one-ring (real mesh edges)
  edgeKeys: [],        // list of unique edge keys "min_max"
  edgeVerts: {},       // key -> [v0, v1]
  edgeFaces: {},       // key -> [faceIndex, ...] (1 = boundary, 2 = interior)
  faceEdges: [],       // faceIndex -> [key, ...] (N loop edges)
};

function edgeKey(a, b) { return a < b ? a + "_" + b : b + "_" + a; }

function buildTopology(mesh) {
  const P = mesh.positions, F = mesh.faces;
  const fc = F.length;                 // F is a jagged array-of-loops
  TOPO.faceCount = fc;
  TOPO.faceVerts = [];
  TOPO.faceTris = [];
  TOPO.tris = [];
  TOPO.faceNormal = [];
  TOPO.faceCenter = [];
  TOPO.vertFaces = Array.from({ length: mesh.vertCount }, () => []);
  TOPO.vertNeighbors = Array.from({ length: mesh.vertCount }, () => new Set());
  TOPO.edgeVerts = {};
  TOPO.edgeFaces = {};
  TOPO.faceEdges = [];

  for (let f = 0; f < fc; f++) {
    const loop = F[f].slice();
    const N = loop.length;
    TOPO.faceVerts.push(loop);

    // Newell's method -> robust normal for any planar/near-planar polygon,
    // and centroid = average of the loop verts.
    let nx = 0, ny = 0, nz = 0, cx = 0, cy = 0, cz = 0;
    for (let i = 0; i < N; i++) {
      const cur = loop[i], nxt = loop[(i + 1) % N];
      const p0x = P[cur*3], p0y = P[cur*3+1], p0z = P[cur*3+2];
      const p1x = P[nxt*3], p1y = P[nxt*3+1], p1z = P[nxt*3+2];
      nx += (p0y - p1y) * (p0z + p1z);
      ny += (p0z - p1z) * (p0x + p1x);
      nz += (p0x - p1x) * (p0y + p1y);
      cx += p0x; cy += p0y; cz += p0z;
    }
    const ln = Math.hypot(nx, ny, nz) || 1;
    TOPO.faceNormal.push([nx/ln, ny/ln, nz/ln]);
    TOPO.faceCenter.push([cx/N, cy/N, cz/N]);

    // fan triangulation (v0, vi, vi+1) for raster/pick; parent = f
    const tris = [];
    for (let i = 1; i + 1 < N; i++) {
      const t = [loop[0], loop[i], loop[i + 1]];
      tris.push(t);
      TOPO.tris.push({ v: t, face: f });
    }
    TOPO.faceTris.push(tris);

    // per-vertex incidence + real (loop-edge) one-ring adjacency
    const es = [];
    for (let i = 0; i < N; i++) {
      const a = loop[i], b = loop[(i + 1) % N];
      TOPO.vertFaces[a].push(f);
      TOPO.vertNeighbors[a].add(b);
      TOPO.vertNeighbors[b].add(a);
      const k = edgeKey(a, b);
      es.push(k);
      if (!TOPO.edgeFaces[k]) { TOPO.edgeFaces[k] = []; TOPO.edgeVerts[k] = [a, b]; }
      TOPO.edgeFaces[k].push(f);
    }
    TOPO.faceEdges.push(es);
  }
  TOPO.edgeKeys = Object.keys(TOPO.edgeFaces);
  return TOPO;
}

// faces sharing an edge with face f
function faceNeighbors(f) {
  const out = new Set();
  for (const k of TOPO.faceEdges[f]) {
    for (const nf of TOPO.edgeFaces[k]) if (nf !== f) out.add(nf);
  }
  return [...out];
}

if (typeof module !== "undefined") {
  module.exports = { TOPO, buildTopology, faceNeighbors, edgeKey };
}
