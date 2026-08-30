#!/usr/bin/env python3
# =============================================================================
# decimate_suzanne.py
#
# Build-time tool: reads the heavy Blender Suzanne reference OBJ (~31k quads),
# decimates it to a low-poly proxy (~600-1200 tris) via vertex clustering, then
# emits js/mesh-data.js for the UV-editor prototype to embed directly (no fetch,
# works over file://).
#
# The proxy keeps a recognizable Suzanne silhouette and has clean, welded,
# shared-vertex topology so grow/shrink/island/perimeter selection and LSCM
# unwrap all have real adjacency to work with.
#
# Run (from repo root, via the PowerShell tool per project rule):
#   python .retired/Prototypes/UVeditor/tools/decimate_suzanne.py
# =============================================================================

import os
import math

HERE = os.path.dirname(os.path.abspath(__file__))
OBJ = os.path.normpath(os.path.join(
    HERE, "..", "..", "..", "..",
    "Engine", "EngineContent", "ReferenceGeometry", "Suzzane.obj"))
OUT = os.path.normpath(os.path.join(HERE, "..", "js", "mesh-data.js"))

# Target grid resolution for clustering. Higher -> more faces retained.
# ~16 lands Suzanne around ~1400 tris, which stays smooth for JS picking while
# keeping the silhouette (eyes/brows/muzzle) recognizable.
GRID = 16


def parse_obj(path):
    positions = []
    tris = []  # each tri is (a, b, c) 0-based indices into positions
    with open(path, "r") as f:
        for line in f:
            if line.startswith("v "):
                _, x, y, z = line.split()[:4]
                positions.append((float(x), float(y), float(z)))
            elif line.startswith("f "):
                # face verts are "vi/vti/vni" - take the position index only
                idx = [int(tok.split("/")[0]) - 1 for tok in line.split()[1:]]
                # triangulate a polygon fan (Suzanne is all quads)
                for i in range(1, len(idx) - 1):
                    tris.append((idx[0], idx[i], idx[i + 1]))
    return positions, tris


def bounds(positions):
    mn = [1e30, 1e30, 1e30]
    mx = [-1e30, -1e30, -1e30]
    for p in positions:
        for k in range(3):
            mn[k] = min(mn[k], p[k])
            mx[k] = max(mx[k], p[k])
    return mn, mx


def cluster_decimate(positions, tris, grid):
    mn, mx = bounds(positions)
    span = [max(1e-6, mx[k] - mn[k]) for k in range(3)]

    # assign every source vertex to a grid cell
    def cell_of(p):
        c = []
        for k in range(3):
            t = (p[k] - mn[k]) / span[k]
            ci = min(grid - 1, int(t * grid))
            c.append(ci)
        return (c[0], c[1], c[2])

    # accumulate centroid per occupied cell
    cell_sum = {}
    cell_cnt = {}
    vert_cell = []
    for p in positions:
        c = cell_of(p)
        vert_cell.append(c)
        if c not in cell_sum:
            cell_sum[c] = [0.0, 0.0, 0.0]
            cell_cnt[c] = 0
        for k in range(3):
            cell_sum[c][k] += p[k]
        cell_cnt[c] += 1

    # each occupied cell becomes one output vertex
    cell_index = {}
    out_pos = []
    for c in cell_sum:
        cell_index[c] = len(out_pos)
        n = cell_cnt[c]
        out_pos.append([cell_sum[c][k] / n for k in range(3)])

    # remap tris; drop degenerate (two corners in same cell) and duplicates
    seen = set()
    out_tris = []
    for (a, b, c) in tris:
        ia = cell_index[vert_cell[a]]
        ib = cell_index[vert_cell[b]]
        ic = cell_index[vert_cell[c]]
        if ia == ib or ib == ic or ia == ic:
            continue
        key = tuple(sorted((ia, ib, ic)))
        if key in seen:
            continue
        seen.add(key)
        out_tris.append((ia, ib, ic))

    return out_pos, out_tris


def drop_unused(positions, tris):
    used = sorted({i for t in tris for i in t})
    remap = {old: new for new, old in enumerate(used)}
    out_pos = [positions[i] for i in used]
    out_tris = [(remap[a], remap[b], remap[c]) for (a, b, c) in tris]
    return out_pos, out_tris


def normalize(positions):
    mn, mx = bounds(positions)
    cen = [(mn[k] + mx[k]) / 2 for k in range(3)]
    height = max(1e-6, mx[1] - mn[1])
    scale = 2.0 / height  # fit roughly to [-1,1] in Y
    return [[(p[k] - cen[k]) * scale for k in range(3)] for p in positions]


def smooth_normals(positions, tris):
    normals = [[0.0, 0.0, 0.0] for _ in positions]
    for (a, b, c) in tris:
        pa, pb, pc = positions[a], positions[b], positions[c]
        u = [pb[k] - pa[k] for k in range(3)]
        v = [pc[k] - pa[k] for k in range(3)]
        n = [u[1] * v[2] - u[2] * v[1],
             u[2] * v[0] - u[0] * v[2],
             u[0] * v[1] - u[1] * v[0]]
        for idx in (a, b, c):
            for k in range(3):
                normals[idx][k] += n[k]
    for n in normals:
        ln = math.sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) or 1.0
        n[0] /= ln
        n[1] /= ln
        n[2] /= ln
    return normals


def fmt(v, p=5):
    return round(v, p)


def emit(positions, normals, tris, out_path):
    pos_flat = [fmt(c) for p in positions for c in p]
    nrm_flat = [fmt(c) for n in normals for c in n]
    face_flat = [i for t in tris for i in t]

    def arr(name, data):
        return "const %s = [%s];" % (name, ",".join(str(x) for x in data))

    lines = [
        "\"use strict\";",
        "/* ==========================================================================",
        "   mesh-data.js  --  GENERATED by tools/decimate_suzanne.py, do not hand-edit.",
        "   Decimated Blender Suzanne proxy for the UV-editor prototype.",
        "   POSITIONS: flat xyz (%d verts).  NORMALS: flat xyz (smooth)." % len(positions),
        "   FACES: flat triangle indices (%d tris)." % len(tris),
        "   ========================================================================== */",
        arr("MESH_POSITIONS", pos_flat),
        arr("MESH_NORMALS", nrm_flat),
        arr("MESH_FACES", face_flat),
        "const MESH = { positions: MESH_POSITIONS, normals: MESH_NORMALS, faces: MESH_FACES,",
        "               vertCount: %d, faceCount: %d };" % (len(positions), len(tris)),
    ]
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w") as f:
        f.write("\n".join(lines) + "\n")


def main():
    print("[decimate] reading", OBJ)
    positions, tris = parse_obj(OBJ)
    print("[decimate] source: %d verts, %d tris" % (len(positions), len(tris)))
    positions, tris = cluster_decimate(positions, tris, GRID)
    positions, tris = drop_unused(positions, tris)
    positions = normalize(positions)
    normals = smooth_normals(positions, tris)
    print("[decimate] proxy:  %d verts, %d tris" % (len(positions), len(tris)))
    emit(positions, normals, tris, OUT)
    print("[decimate] wrote", OUT)


if __name__ == "__main__":
    main()
