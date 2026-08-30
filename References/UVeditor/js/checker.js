"use strict";
/* ==========================================================================
   checker.js -- procedural checker/reference texture, shared by the 2D UV
   editor and the 3D viewport so both sample IDENTICAL colours from the same
   per-vertex UVs. Stateless colour function + a small view-state object.

     • checkerColor(u,v,opts) -> [r,g,b] is pure: given a UV and options it
       returns the tile colour. Both renderers call it, so a texel in the 2D
       editor and the same texel projected on the 3D model always match.
     • Two modes: "twoTone" (classic A/B parity board) and "oriented" (adds a
       per-tile hue ramp + faint intra-tile gradient so mirror / rotation flips
       in the unwrap read at a glance).
     • CHECKER holds the toggles the toolbar drives -- resolution (tiles), mode,
       whether the 2D channel is active, and whether it projects onto 3D. Pure
       VIEW state: never recorded in history.
   ========================================================================== */

/* default tile colours (0-255 rgb triples). Kept muted so overlays read on top. */
const CHECKER_COLOR_A = [210, 214, 220];    // light square
const CHECKER_COLOR_B = [70, 78, 92];       // dark square

/* toolbar-driven view state. tiles is the number of squares across the 0-1 UV
   box (so tiles*tiles squares total). channel2d = show the checker in the 2D
   editor; enabled3d = project it onto the 3D model (else the viewport stays
   matcap-only). */
const CHECKER = {
    channel2d: false,       // checker is the active 2D display channel
    enabled3d: false,       // project the checker onto the 3D viewport
    tiles: 8,               // squares across the 0-1 box (4 | 8 | 16 | 32)
    mode: "twoTone",        // "twoTone" | "oriented"
    colorA: CHECKER_COLOR_A,
    colorB: CHECKER_COLOR_B,
};

/* wrap a coordinate into [0,1) so tiling repeats outside the 0-1 box (UDIM-safe). */
function checkerWrap(t) { const w = t - Math.floor(t); return w < 0 ? w + 1 : w; }

/* small HSV->RGB (h in [0,1)) used by the oriented mode's per-tile hue ramp. */
function checkerHsv(h, s, v) {
    const i = Math.floor(h * 6), f = h * 6 - i;
    const p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
    let r, g, b;
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return [Math.round(r * 255), Math.round(g * 255), Math.round(b * 255)];
}

/* the checker colour at UV (u,v). opts overrides CHECKER fields per-call; when
   omitted it reads the shared CHECKER state so 2D and 3D stay in lockstep. */
function checkerColor(u, v, opts) {
    const o = opts || CHECKER;
    const tiles = o.tiles || 8;
    const uu = checkerWrap(u), vv = checkerWrap(v);
    const tu = Math.floor(uu * tiles), tv = Math.floor(vv * tiles);
    const parity = (tu + tv) & 1;
    const a = o.colorA || CHECKER_COLOR_A, b = o.colorB || CHECKER_COLOR_B;
    if ((o.mode || "twoTone") !== "oriented") {
        return parity ? a.slice() : b.slice();
    }
    // oriented: a hue that ramps across U (per-tile) and brightens up V, with the
    // parity board kept as a value step and a faint intra-tile gradient so a
    // mirrored or rotated island is visually distinct from a correct one.
    const hue = (tu / tiles);
    const val = 0.45 + 0.4 * (tv / Math.max(1, tiles - 1));
    const [hr, hg, hb] = checkerHsv(hue, 0.55, val);
    const step = parity ? 1.0 : 0.6;                       // parity darkens B tiles
    // intra-tile gradient: brighten toward the tile's +U,+V corner
    const fu = uu * tiles - tu, fv = vv * tiles - tv;
    const grad = 0.85 + 0.15 * ((fu + fv) * 0.5);
    const k = step * grad;
    return [Math.round(hr * k), Math.round(hg * k), Math.round(hb * k)];
}

if (typeof module !== "undefined") {
    module.exports = { checkerColor, checkerWrap, checkerHsv, CHECKER, CHECKER_COLOR_A, CHECKER_COLOR_B };
}
