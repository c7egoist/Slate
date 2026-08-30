"use strict";
/* ==========================================================================
   stretch.js -- per-triangle UV distortion heatmap, shared by the 2D UV editor
   and the 3D viewport so both colour the SAME triangle identically. Compares
   each fan triangle's 3D area/shape against its UV-space area/shape and maps the
   distortion to a diverging blue->green->red ramp.

     • computeStretchMetrics(islands) walks every island's fan triangles, storing
       per-face per-tri distortion in STRETCH.triValue[faceIndex] plus a per-
       island aggregate (mean/max) and the model-wide reference scale. Recompute
       whenever the UVs change (after unwrap / pack / transform).
     • Two modes:
         "area"  = sqrt(uvArea / worldArea) scale, normalized against the model's
                   global mean scale so 1.0 = neutral texel density. <1 compressed
                   (blue), >1 stretched (red).
         "angle" = conformal distortion sigmaMax/sigmaMin of the per-triangle
                   Jacobian (1 = angle-preserving, >1 = sheared). Independent of
                   the global scale.
     • stretchColorRamp(t) -> [r,g,b] is pure: both renderers call it, so a
       triangle in the 2D editor and the same triangle on the 3D model match.
     • STRETCH holds the toolbar toggles (mode, whether the 2D/3D channel is on).
       Pure VIEW state -- never recorded in history.
   ========================================================================== */

/* toolbar-driven view state. channel2d = show stretch in the 2D editor;
   enabled3d = project it onto the 3D model. mode picks the metric. triValue is
   the recomputed table (faceIndex -> Float array, one normalized value per fan
   triangle); islandStats keys an island object -> {mean,max}; refScale is the
   model's global mean sqrt-area scale (the "neutral" the area ramp centres on). */
const STRETCH = {
    channel2d: false,       // stretch is the active 2D display channel
    enabled3d: false,       // project stretch onto the 3D viewport
    mode: "area",           // "area" | "angle"
    triValue: null,         // faceIndex -> Float64Array(normalized value per fan tri)
    islandStats: null,      // Map<island, {mean,max}>
    refScale: 1,            // model-wide mean sqrt-area scale (area-mode neutral)
    computedMode: null,     // which mode the current triValue table was built for
};

/* signed 2x area of a UV triangle. */
function stretchUvArea2(a, b, c) {
    return (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]);
}
/* 2x area of a 3D triangle (magnitude of the edge cross product). */
function stretchWorldArea2(p, q, r) {
    const ux = q[0] - p[0], uy = q[1] - p[1], uz = q[2] - p[2];
    const vx = r[0] - p[0], vy = r[1] - p[1], vz = r[2] - p[2];
    const cx = uy * vz - uz * vy, cy = uz * vx - ux * vz, cz = ux * vy - uy * vx;
    return Math.hypot(cx, cy, cz);
}

/* conformal distortion sigmaMax/sigmaMin of the affine map from a 3D triangle to
   its UV triangle. Build a local 2D orthonormal basis on the 3D triangle plane,
   express the two 3D edges in it, then the 2x2 Jacobian J takes those local 3D
   coords to UV. The singular values of J are the principal stretch factors;
   their ratio (>=1) is the shear/angle distortion (1 = perfectly conformal). */
function stretchAngleRatio(p, q, r, uvA, uvB, uvC) {
    // 3D edges
    const e1 = [q[0] - p[0], q[1] - p[1], q[2] - p[2]];
    const e2 = [r[0] - p[0], r[1] - p[1], r[2] - p[2]];
    // local orthonormal basis (x along e1, y in-plane perpendicular)
    const l1 = Math.hypot(e1[0], e1[1], e1[2]) || 1e-12;
    const xAxis = [e1[0] / l1, e1[1] / l1, e1[2] / l1];
    const d = e2[0] * xAxis[0] + e2[1] * xAxis[1] + e2[2] * xAxis[2];
    let yv = [e2[0] - d * xAxis[0], e2[1] - d * xAxis[1], e2[2] - d * xAxis[2]];
    const l2 = Math.hypot(yv[0], yv[1], yv[2]) || 1e-12;
    const yAxis = [yv[0] / l2, yv[1] / l2, yv[2] / l2];
    // local 3D coords of the two edges (p is the origin)
    const p1x = l1, p1y = 0;                                   // e1 in local basis
    const p2x = d, p2y = e2[0] * yAxis[0] + e2[1] * yAxis[1] + e2[2] * yAxis[2];
    // UV edges
    const q1u = uvB[0] - uvA[0], q1v = uvB[1] - uvA[1];
    const q2u = uvC[0] - uvA[0], q2v = uvC[1] - uvA[1];
    // solve J * [localEdge] = [uvEdge] for the 2x2 Jacobian J. The local-coord
    // matrix M = [[p1x,p2x],[p1y,p2y]] is upper-triangular (p1y=0) so invert directly.
    const det = p1x * p2y - p2x * p1y;
    if (Math.abs(det) < 1e-18) return 1;
    const inv = 1 / det;
    // M^-1
    const m00 = p2y * inv, m01 = -p2x * inv, m10 = -p1y * inv, m11 = p1x * inv;
    // J = UV * M^-1  (UV = [[q1u,q2u],[q1v,q2v]])
    const j00 = q1u * m00 + q2u * m10, j01 = q1u * m01 + q2u * m11;
    const j10 = q1v * m00 + q2v * m10, j11 = q1v * m01 + q2v * m11;
    // singular values of J via the eigenvalues of J^T J
    const a = j00 * j00 + j10 * j10;
    const b = j00 * j01 + j10 * j11;
    const c = j01 * j01 + j11 * j11;
    const tr = a + c, dt = a * c - b * b;
    const disc = Math.sqrt(Math.max(0, tr * tr / 4 - dt));
    const s2max = tr / 2 + disc, s2min = tr / 2 - disc;
    const smax = Math.sqrt(Math.max(0, s2max)), smin = Math.sqrt(Math.max(1e-18, s2min));
    return Math.max(1, smax / smin);                           // >=1 by construction, 1 = conformal
}

/* build (or rebuild) STRETCH.triValue for the given mode over all islands.
   AREA mode is a two-pass normalization: pass 1 accumulates every triangle's
   sqrt(uvArea/worldArea) scale to find the model mean (refScale), pass 2 stores
   each triangle's ratio-to-mean as log2 (so compressed and stretched are
   symmetric around 0). ANGLE mode stores log2(sigmaMax/sigmaMin) directly (0 =
   conformal). Both are consumed by stretchColorRamp via stretchNormalized. */
function computeStretchMetrics(islands, mode) {
    mode = mode || STRETCH.mode || "area";
    const P = (typeof MESH !== "undefined") ? MESH.positions : null;
    const tris = {};                                           // faceIndex -> number[]
    const stats = new Map();
    // pass 1 (area): the reference "neutral" scale is the world-area-weighted
    // GEOMETRIC mean of the per-triangle sqrt(uvArea/worldArea) scale. A geometric
    // mean (mean of log2(scale)) makes the stored log2(scale/refScale) sum to zero
    // by construction, so compression and stretch are symmetric about neutral and
    // large faces set the reference while slivers don't skew it.
    let sumLogArea = 0, sumArea = 0;
    if (mode === "area" && P) {
        for (const isl of islands || []) {
            const uv = isl.uv; if (!uv) continue;
            for (const f of isl.faces) {
                for (const [a, b, c] of TOPO.faceTris[f]) {
                    if (!uv[a] || !uv[b] || !uv[c]) continue;
                    const wA = stretchWorldArea2([P[a*3],P[a*3+1],P[a*3+2]], [P[b*3],P[b*3+1],P[b*3+2]], [P[c*3],P[c*3+1],P[c*3+2]]) * 0.5;
                    const uA = Math.abs(stretchUvArea2(uv[a], uv[b], uv[c])) * 0.5;
                    // skip degenerate triangles (zero 3D or collapsed UV) — they carry
                    // no meaningful scale and a log2(0) would poison the reference mean.
                    if (wA < 1e-12 || uA < 1e-14) continue;
                    const scale = Math.sqrt(uA / wA);
                    sumLogArea += Math.log2(scale) * wA; sumArea += wA;
                }
            }
        }
    }
    const refScale = (sumArea > 0) ? Math.pow(2, sumLogArea / sumArea) : 1;
    STRETCH.refScale = refScale || 1;
    // pass 2: store the per-triangle normalized log-distortion + island aggregates.
    for (const isl of islands || []) {
        const uv = isl.uv; if (!uv) continue;
        let mean = 0, max = 0, n = 0;
        for (const f of isl.faces) {
            const ftris = TOPO.faceTris[f];
            const arr = new Float64Array(ftris.length);
            for (let ti = 0; ti < ftris.length; ti++) {
                const [a, b, c] = ftris[ti];
                let val = 0;
                if (uv[a] && uv[b] && uv[c] && P) {
                    if (mode === "angle") {
                        const ratio = stretchAngleRatio(
                            [P[a*3],P[a*3+1],P[a*3+2]], [P[b*3],P[b*3+1],P[b*3+2]], [P[c*3],P[c*3+1],P[c*3+2]],
                            uv[a], uv[b], uv[c]);
                        val = Math.log2(Math.max(1e-6, ratio));       // 0 = conformal
                    } else {
                        const wA = stretchWorldArea2([P[a*3],P[a*3+1],P[a*3+2]], [P[b*3],P[b*3+1],P[b*3+2]], [P[c*3],P[c*3+1],P[c*3+2]]) * 0.5;
                        const uA = Math.abs(stretchUvArea2(uv[a], uv[b], uv[c])) * 0.5;
                        // a collapsed-UV or degenerate triangle reads as fully compressed
                        // (max blue) but must not carry a wild -Infinity log; clamp it.
                        if (wA < 1e-12 || uA < 1e-14) val = -STRETCH_SPREAD;
                        else val = Math.log2(Math.sqrt(uA / wA) / refScale);   // 0 = neutral
                    }
                }
                arr[ti] = val;
                const mag = Math.abs(val);
                mean += mag; max = Math.max(max, mag); n++;
            }
            tris[f] = arr;
        }
        stats.set(isl, { mean: n ? mean / n : 0, max });
    }
    STRETCH.triValue = tris;
    STRETCH.islandStats = stats;
    STRETCH.computedMode = mode;
    return { triValue: tris, islandStats: stats, refScale: STRETCH.refScale };
}

/* the stored distortion is a log2 ratio (0 = neutral). SPREAD sets how many
   octaves of distortion saturate the ramp: +/-1 octave (2x / 0.5x) hits full
   blue / red. Map log-value -> t in [0,1] with 0.5 = neutral. */
const STRETCH_SPREAD = 1.0;                                     // octaves to full saturation
function stretchNormalized(logVal) {
    const t = 0.5 + 0.5 * (logVal / STRETCH_SPREAD);
    return t < 0 ? 0 : t > 1 ? 1 : t;
}

/* diverging ramp: t=0 compressed (blue) -> 0.5 neutral (green) -> 1 stretched
   (red). Returns [r,g,b] 0-255. Both renderers use it so colours match. */
function stretchColorRamp(t) {
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    // two linear segments through blue -> green -> red
    const blue = [56, 122, 232], green = [90, 200, 120], red = [232, 74, 60];
    let r, g, b, k;
    if (t < 0.5) { k = t / 0.5; r = blue[0]+(green[0]-blue[0])*k; g = blue[1]+(green[1]-blue[1])*k; b = blue[2]+(green[2]-blue[2])*k; }
    else { k = (t - 0.5) / 0.5; r = green[0]+(red[0]-green[0])*k; g = green[1]+(red[1]-green[1])*k; b = green[2]+(red[2]-green[2])*k; }
    return [Math.round(r), Math.round(g), Math.round(b)];
}

/* the stretch colour for one fan triangle of a face -> [r,g,b], or null if no
   value was computed (fall back to the flat fill / matcap). */
function stretchTriColor(faceIndex, triIndex) {
    const t = STRETCH.triValue;
    if (!t || !t[faceIndex]) return null;
    const arr = t[faceIndex];
    if (triIndex >= arr.length) return null;
    return stretchColorRamp(stretchNormalized(arr[triIndex]));
}

if (typeof module !== "undefined") {
    module.exports = {
        STRETCH, computeStretchMetrics, stretchColorRamp, stretchNormalized,
        stretchTriColor, stretchAngleRatio, stretchUvArea2, stretchWorldArea2, STRETCH_SPREAD,
    };
}
