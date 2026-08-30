"use strict";
/* ==========================================================================
   viewport3d.js -- software matcap rasterizer + orbit camera + element picking
   for the left pane. Draws the decimated Suzanne to a 2D <canvas> (no WebGL):
   transform -> backface cull -> depth sort -> flat matcap fill per triangle,
   with selection/hover overlays for verts / edges / faces on top.

   Matcaps are generated procedurally at load (small offscreen canvases) so the
   prototype needs no binary image assets and works over file://.
   ========================================================================== */
const VP3 = {
  canvas: null, ctx: null,
  w: 0, h: 0, dpr: Math.min(window.devicePixelRatio || 1, 2),
  // orbit camera (spherical) + eased targets. `pan` is the orbit PIVOT in world
  // space (the point the camera looks at); dragging slides it along the camera's
  // own right/up axes so the model tracks the cursor at any orbit angle.
  yaw: 0.6, pitch: 0.25, dist: 4.2, pan: [0, 0, 0],
  tYaw: 0.6, tPitch: 0.25, tDist: 4.2, tPan: [0, 0, 0],
  matcap: "clay",
  matcaps: {},              // name -> {canvas, ctx, data, size}
  showWire: true,
  // base display CHANNEL under the overlays: "solid" (matcap) | "checker" |
  // "stretch" (slot; stub until backlog #2). Element overlays draw on top of it.
  channel: "solid",
  showVerts: false,         // element overlays (drawn over the active channel)
  showEdges: false,
  showFaces: false,
  uv: null,                 // vertIndex -> [u,v], mirrored from UVED.islands for the checker projection
  fb: null,                 // reused ImageData scratch for the software checker raster
  gridMode: "dots",         // dots | lines | off  (3D ground grid style)
  selectThrough: false,     // X-ray select: reach occluded / back-facing geometry
  tool: "pick",             // pick | box | circle | lasso | paint
  brushRadius: 34,          // circle/paint brush radius (screen px)
  marquee: null,            // in-progress marquee overlay state
  // per-frame scratch: projected screen coords for every vertex
  scr: null, viewNormals: null, mvp: null,
  // per-frame depth buffer of the SOLID surface, so overlays (wire/verts/edges)
  // can be occlusion-tested — a front-facing element that sits behind nearer
  // geometry (eye-socket far rim, head interior) is hidden, like Blender.
  depth: null,              // { dw, dh, cell, buf:Float32Array } — min NDC z per cell
  depthBias: 0,             // slope/precision tolerance, recomputed per frame
  zSpan: 1e-3,              // model's on-screen NDC-z front-to-back span (per frame)
  hover: { kind: null, idx: null },
  // loop/ring PREVIEW: while a loop-modifier (ctrl/alt) is held and hovering, the
  // full set a click would grab is ghost-drawn so the user sees WHICH edges/faces
  // will be selected before committing (backlog #8). null when inactive.
  preview: null,            // { edges:Set, faces:Set, seed } | null
  running: false,
};

const SEAM_COLOR = "#ff3b30";     // distinct seam colour (bright red)

/* ---------- procedural matcaps ---------- */
function buildMatcaps() {
  const size = 128;
  const defs = {
    clay:   { base: [196, 154, 122], rim: [90, 60, 45], spec: 0.25 },
    chrome: { base: [150, 160, 172], rim: [20, 24, 30], spec: 0.9 },
    normal: { base: null, rim: null, spec: 0 },        // encodes the normal as colour
    red:    { base: [206, 90, 78], rim: [70, 20, 18], spec: 0.4 },
    // metals
    gold:   { base: [212, 175, 55], rim: [80, 58, 12], spec: 0.85 },
    copper: { base: [200, 117, 80], rim: [70, 34, 22], spec: 0.7 },
    steel:  { base: [150, 160, 172], rim: [34, 38, 44], spec: 0.95 },
    // studio greys
    grey:   { base: [140, 140, 145], rim: [40, 40, 44], spec: 0.2 },
    white:  { base: [220, 220, 224], rim: [110, 110, 116], spec: 0.15 },
    // coloured plastics
    blue:   { base: [70, 120, 220], rim: [18, 30, 70], spec: 0.55 },
    green:  { base: [70, 180, 110], rim: [16, 54, 34], spec: 0.5 },
    purple: { base: [150, 90, 210], rim: [44, 24, 68], spec: 0.55 },
    // skin / ceramic
    skin:   { base: [225, 170, 150], rim: [120, 70, 62], spec: 0.12 },
    ceramic:{ base: [235, 235, 238], rim: [70, 74, 82], spec: 0.9 },
    // cavity / ambient-occlusion studios: `cavity` darkens grazing normals so
    // crevices, edges and silhouettes read as shaded pits (fake AO -- a matcap
    // can't sample real mesh cavities, but the rim falloff mimics them well).
    cavityGrey: { base: [176, 176, 182], rim: [26, 26, 30], spec: 0.18, cavity: 1.0 },
    cavityClay: { base: [198, 158, 128], rim: [30, 20, 16], spec: 0.15, cavity: 1.3 },
  };
  for (const name of Object.keys(defs)) {
    const cv = document.createElement("canvas");
    cv.width = cv.height = size;
    const g = cv.getContext("2d");
    const img = g.createImageData(size, size);
    const d = defs[name];
    for (let y = 0; y < size; y++) {
      for (let x = 0; x < size; x++) {
        const nx = (x / size) * 2 - 1;
        const ny = 1 - (y / size) * 2;
        const r2 = nx*nx + ny*ny;
        const i = (y*size + x) * 4;
        if (r2 > 1) { img.data[i+3] = 0; continue; }
        const nz = Math.sqrt(1 - r2);
        let R, G, B;
        if (name === "normal") {
          R = (nx*0.5+0.5)*255; G = (ny*0.5+0.5)*255; B = nz*255;
        } else {
          // simple hemispheric light from upper-left + specular hotspot
          const l = Math.max(0, nx*(-0.4) + ny*0.6 + nz*0.7);
          const t = Math.pow(l, 1.2);
          R = d.rim[0] + (d.base[0]-d.rim[0])*t;
          G = d.rim[1] + (d.base[1]-d.rim[1])*t;
          B = d.rim[2] + (d.base[2]-d.rim[2])*t;
          // cavity/AO term: darken toward the rim (nz -> 0) so crevices and
          // grazing edges read as occluded. `cavity` scales the strength.
          if (d.cavity) {
            const ao = Math.pow(nz, 0.75 * d.cavity);   // 1 face-on, ->0 at rim
            const k = 0.15 + 0.85 * ao;                 // keep a little ambient
            R *= k; G *= k; B *= k;
          }
          // specular
          const sp = Math.pow(Math.max(0, nx*(-0.45)+ny*0.6+nz*0.66), 40) * d.spec * 255;
          R = Math.min(255, R+sp); G = Math.min(255, G+sp); B = Math.min(255, B+sp);
        }
        img.data[i] = R; img.data[i+1] = G; img.data[i+2] = B; img.data[i+3] = 255;
      }
    }
    g.putImageData(img, 0, 0);
    VP3.matcaps[name] = { canvas: cv, ctx: g, data: img.data, size };
  }
}

function vp3Init(canvas) {
  VP3.canvas = canvas;
  VP3.ctx = canvas.getContext("2d");
  buildMatcaps();
  vp3Resize();
  vp3AttachInput();
  vp3Draw();
}

function vp3Resize() {
  const r = VP3.canvas.parentElement.getBoundingClientRect();
  VP3.w = Math.max(1, r.width); VP3.h = Math.max(1, r.height);
  VP3.canvas.width = VP3.w * VP3.dpr; VP3.canvas.height = VP3.h * VP3.dpr;
  VP3.ctx.setTransform(VP3.dpr, 0, 0, VP3.dpr, 0, 0);
  vp3Draw();
}

/* ---------- camera matrices ---------- */
function cameraEye() {
  const cy = Math.cos(VP3.pitch), sy = Math.sin(VP3.pitch);
  const cx = Math.cos(VP3.yaw), sx = Math.sin(VP3.yaw);
  return [VP3.dist*cy*sx, VP3.dist*sy, VP3.dist*cy*cx];
}
function buildMVP() {
  // the orbit pivot IS the pan offset (a world-space point); the eye orbits
  // around it, so panning the pivot slides the whole view rigidly.
  const target = VP3.pan.slice();
  const eye = V3.add(cameraEye(), target);
  const view = M4.lookAt(eye, target, [0, 1, 0]);
  const proj = M4.perspective(45*Math.PI/180, VP3.w/VP3.h, 0.05, 100);
  return { mvp: M4.multiply(proj, view), view };
}
/* camera right/up basis in world space (rows of the view rotation), so a screen
   drag can move the pivot along exactly what the viewer sees as horizontal /
   vertical -- this is what makes pan feel camera-relative, not object-relative. */
function cameraBasis() {
  const eye = V3.add(cameraEye(), VP3.pan);
  const forward = V3.normalize(V3.sub(VP3.pan, eye));
  const right = V3.normalize(V3.cross(forward, [0, 1, 0]));
  const up = V3.cross(right, forward);
  return { right, up };
}

/* ---------- project all verts ---------- */
function vp3Project() {
  const { mvp, view } = buildMVP();
  VP3.mvp = mvp;
  const P = MESH.positions, N = MESH.normals, nV = MESH.vertCount;
  const scr = new Float32Array(nV * 3);       // x, y, invW/depth
  const vn = new Float32Array(nV * 3);        // view-space normal
  // track the model's on-screen NDC-z span this frame. Perspective NDC z is
  // non-linear: zooming out pushes the whole model toward the far plane and
  // COLLAPSES its front-to-back z span. The overlay depth bias must scale with
  // this span (not a fixed constant) or it becomes larger than the model's own
  // thickness when zoomed out and lets every back-face bleed through.
  let zMin = Infinity, zMax = -Infinity;
  for (let v = 0; v < nV; v++) {
    const p = [P[v*3], P[v*3+1], P[v*3+2]];
    const c = M4.transformPoint(mvp, p);
    const w = c[3] || 1e-6;
    scr[v*3]   = ( c[0]/w * 0.5 + 0.5) * VP3.w;
    scr[v*3+1] = (-c[1]/w * 0.5 + 0.5) * VP3.h;
    const z = c[2]/w;
    scr[v*3+2] = z;                            // NDC depth
    if (w > 1e-4) { if (z < zMin) zMin = z; if (z > zMax) zMax = z; }
    // normals in the source mesh face inward, so negate to light the outer skin
    const n = M4.transformDir(view, [-N[v*3], -N[v*3+1], -N[v*3+2]]);
    const ln = Math.hypot(n[0], n[1], n[2]) || 1;
    vn[v*3] = n[0]/ln; vn[v*3+1] = n[1]/ln; vn[v*3+2] = n[2]/ln;
  }
  VP3.scr = scr; VP3.viewNormals = vn;
  VP3.zSpan = (zMax > zMin) ? (zMax - zMin) : 1e-3;
}

/* ---------- surface depth buffer (for overlay occlusion) ----------
   A coarse screen-space grid holding the NEAREST NDC z of the solid surface per
   cell. Overlays test their own z against it: an element behind the stored depth
   (by more than a small bias) is occluded and skipped — so a front-facing edge
   inside an eye socket or on the far interior wall no longer draws through the
   model. Coarse cells (a few px) keep it cheap and forgiving at silhouettes. */
function vp3DepthReset() {
  const cell = 4;                               // px per depth cell
  const dw = Math.max(1, Math.ceil(VP3.w / cell));
  const dh = Math.max(1, Math.ceil(VP3.h / cell));
  let d = VP3.depth;
  if (!d || d.dw !== dw || d.dh !== dh) { d = { dw, dh, cell, buf: new Float32Array(dw*dh), tol: new Float32Array(dw*dh) }; VP3.depth = d; }
  d.buf.fill(Infinity);                         // Infinity = nothing drawn yet
  d.tol.fill(0);                                // per-cell slope-scaled tolerance
  // depth-compare tolerance is now SLOPE-SCALED and stored PER CELL at stamp
  // time (see vp3DepthStampTri): tolerance = |dz/dscreen| · cell, i.e. how much
  // the surface z can differ across one depth cell. That is exactly the
  // quantization error the coincident overlay must forgive — tight on flat
  // camera-facing skin (so back-faces stay rejected), loose only where the
  // surface is steep. It self-scales with zoom, so no span/constant term is
  // needed. A tiny floor absorbs pure numerical coincidence at shared verts.
  VP3.depthBias = 0;                            // (legacy field; per-cell tol used)
}
/* stamp a triangle's per-cell min depth + slope tolerance into the buffer
   (bounding-box barycentric scan; the mesh is small so this stays cheap). */
function vp3DepthStampTri(scr, a, b, c) {
  const d = VP3.depth; if (!d) return;
  const cell = d.cell, dw = d.dw, dh = d.dh, buf = d.buf, tolBuf = d.tol;
  const ax=scr[a*3],ay=scr[a*3+1],az=scr[a*3+2];
  const bx=scr[b*3],by=scr[b*3+1],bz=scr[b*3+2];
  const cx=scr[c*3],cy=scr[c*3+1],cz=scr[c*3+2];
  let minX=Math.min(ax,bx,cx), maxX=Math.max(ax,bx,cx);
  let minY=Math.min(ay,by,cy), maxY=Math.max(ay,by,cy);
  minX=Math.max(0,Math.floor(minX/cell)); maxX=Math.min(dw-1,Math.floor(maxX/cell));
  minY=Math.max(0,Math.floor(minY/cell)); maxY=Math.min(dh-1,Math.floor(maxY/cell));
  const area=(bx-ax)*(cy-ay)-(cx-ax)*(by-ay);
  if (Math.abs(area)<1e-9) return;
  const inv=1/area;
  // screen-space z gradient of this triangle's plane (constant per tri): how
  // much NDC z changes per screen pixel. tol per cell = |grad| · cell = the
  // largest z step the stored cell-centre sample can be off by within the cell.
  const dzdx = ((cy-ay)*(bz-az) - (by-ay)*(cz-az)) * inv;
  const dzdy = ((bx-ax)*(cz-az) - (cx-ax)*(bz-az)) * inv;
  const slopeTol = Math.hypot(dzdx, dzdy) * cell + 1e-5;
  for (let gy=minY; gy<=maxY; gy++) {
    const py=gy*cell+cell*0.5;
    for (let gx=minX; gx<=maxX; gx++) {
      const px=gx*cell+cell*0.5;
      // barycentric weights (same winding as the fill)
      const w0=((bx-px)*(cy-py)-(cx-px)*(by-py))*inv;
      const w1=((cx-px)*(ay-py)-(ax-px)*(cy-py))*inv;
      const w2=1-w0-w1;
      if (w0<-0.02||w1<-0.02||w2<-0.02) continue;
      const z=w0*az+w1*bz+w2*cz;
      const i=gy*dw+gx;
      // keep the NEAREST surface's depth and ITS slope tolerance for the cell.
      if (z<buf[i]) { buf[i]=z; tolBuf[i]=slopeTol; }
    }
  }
}
/* is screen point (sx,sy) at NDC depth z visible, i.e. NOT behind the stored
   surface? True when no surface is in that cell, or the point is nearer/equal
   (within bias). Used to occlusion-test every overlay element. */
function vp3DepthVisible(sx, sy, z) {
  const d = VP3.depth; if (!d) return true;
  const gx = Math.floor(sx/d.cell), gy = Math.floor(sy/d.cell);
  if (gx<0||gy<0||gx>=d.dw||gy>=d.dh) return true;
  const i = gy*d.dw+gx;
  const s = d.buf[i];
  if (s === Infinity) return true;              // nothing drawn here
  // forgive only the stored cell's own slope-quantization error — far smaller
  // than the model's front-to-back thickness, so occluded/back geometry (a full
  // thickness behind) is still rejected at every zoom level.
  return z <= s + d.tol[i];
}
/* depth-visibility for a whole vertex: sample the buffer at its screen point. */
function vertDepthVisible(v) {
  const s = VP3.scr;
  return vp3DepthVisible(s[v*3], s[v*3+1], s[v*3+2]);
}

/* ---------- draw ---------- */
function vp3Draw() {
  const g = VP3.ctx;
  g.clearRect(0, 0, VP3.w, VP3.h);
  g.fillStyle = "#0d0f12"; g.fillRect(0, 0, VP3.w, VP3.h);
  if (VP3.gridMode !== "off") vp3GroundGrid(g);
  vp3Project();
  const scr = VP3.scr, vn = VP3.viewNormals, mc = VP3.matcaps[VP3.matcap];
  const selFaces = selectedFaceSet();

  // build a depth-sorted list of the fan triangles (painter's, back-to-front).
  // Polygons are fan-triangulated in topology; each tri remembers its parent
  // face so selection tint keys on the original polygon.
  const order = [];
  const T = TOPO.tris;
  for (let t = 0; t < T.length; t++) {
    const [a, b, c] = T[t].v;
    // backface cull in screen space (CCW check)
    const ax = scr[a*3], ay = scr[a*3+1], bx = scr[b*3], by = scr[b*3+1], cx = scr[c*3], cy = scr[c*3+1];
    const area = (bx-ax)*(cy-ay) - (cx-ax)*(by-ay);
    if (area >= 0) continue;                    // cull backfaces (winding flipped with normals)
    const z = (scr[a*3+2] + scr[b*3+2] + scr[c*3+2]) / 3;
    order.push([t, z]);
  }
  order.sort((p, q) => q[1] - p[1]);

  // reset the surface depth buffer for this frame (front-to-back min-z per cell)
  vp3DepthReset();

  // fill each visible triangle. Three paths, all stamping the surface depth
  // buffer so overlays stay occlusion-gated:
  //   • checker CHANNEL — per-pixel software raster projecting the same UV
  //     checker the 2D editor shows (only when UVs exist);
  //   • stretch CHANNEL — flat per-triangle distortion colour (precomputed in
  //     STRETCH.triValue) multiplied by the matcap shade;
  //   • everything else — the fast flat matcap vector fill.
  const checkerOn = VP3.channel === "checker" && typeof CHECKER !== "undefined" && VP3.uv;
  const rastered = checkerOn && vp3RasterCheckerPass(g, scr, order, T, vn, mc, selFaces);
  const stretchOn = !rastered && VP3.channel === "stretch" && typeof STRETCH !== "undefined" && STRETCH.triValue;
  if (!rastered) {
    for (const [t] of order) {
      const [a, b, c] = T[t].v;
      // face view normal (avg of corners)
      const nx = (vn[a*3]+vn[b*3]+vn[c*3])/3;
      const ny = (vn[a*3+1]+vn[b*3+1]+vn[c*3+1])/3;
      // stretch channel: distortion colour for THIS fan triangle multiplied by
      // the matcap shade (keeps form reading), else the flat matcap colour.
      let col;
      if (stretchOn) {
        const sc = stretchTriColor(T[t].face, triLocalIndex(t));
        if (sc) {
          const [mr, mg, mb] = sampleMatcapRGB(mc, nx, ny);
          col = Math.round(sc[0]*mr/255) + "," + Math.round(sc[1]*mg/255) + "," + Math.round(sc[2]*mb/255);
        } else col = sampleMatcap(mc, nx, ny);
      } else col = sampleMatcap(mc, nx, ny);
      g.beginPath();
      g.moveTo(scr[a*3], scr[a*3+1]);
      g.lineTo(scr[b*3], scr[b*3+1]);
      g.lineTo(scr[c*3], scr[c*3+1]);
      g.closePath();
      if (selFaces.has(T[t].face)) {
        g.fillStyle = "rgba(80,150,255,0.55)";
        g.fill();
        g.fillStyle = "rgba(" + col + ",0.35)"; g.fill();
      } else {
        g.fillStyle = "rgb(" + col + ")"; g.fill();
      }
      vp3DepthStampTri(scr, a, b, c);
    }
  }

  vp3DrawOverlays(g, scr);
  vp3DrawMarquee(g);
}

/* the in-progress marquee overlay (box rect / lasso path / brush disc). */
function vp3DrawMarquee(g) {
  const m = VP3.marquee; if (!m) return;
  g.save();
  const stroke = m.sub ? "rgba(255,90,90,0.9)" : "rgba(120,190,255,0.95)";
  const fill = m.sub ? "rgba(255,90,90,0.10)" : "rgba(120,190,255,0.12)";
  g.lineWidth = 1.4; g.strokeStyle = stroke; g.fillStyle = fill; g.setLineDash([5,4]);
  if (m.tool === "box") {
    const x=Math.min(m.x0,m.x1), y=Math.min(m.y0,m.y1), w=Math.abs(m.x1-m.x0), h=Math.abs(m.y1-m.y0);
    g.fillRect(x,y,w,h); g.strokeRect(x,y,w,h);
  } else if (m.tool === "lasso" && m.pts.length) {
    g.beginPath(); g.moveTo(m.pts[0][0], m.pts[0][1]);
    for (const p of m.pts) g.lineTo(p[0], p[1]);
    g.closePath(); g.fill(); g.stroke();
  } else if (m.tool === "circle") {
    g.beginPath(); g.arc(m.cx, m.cy, m.r, 0, 7); g.fill(); g.stroke();
  }
  g.restore();
}

function sampleMatcap(mc, nx, ny) {
  const s = mc.size;
  const x = Math.max(0, Math.min(s-1, Math.floor((nx*0.5+0.5)*s)));
  const y = Math.max(0, Math.min(s-1, Math.floor((1-(ny*0.5+0.5))*s)));
  const i = (y*s + x)*4;
  return mc.data[i] + "," + mc.data[i+1] + "," + mc.data[i+2];
}
/* numeric matcap sample -> [r,g,b] (0-255). Used by the software checker raster,
   which multiplies the checker colour by the matcap shade to keep shape reading. */
function sampleMatcapRGB(mc, nx, ny) {
  const s = mc.size;
  const x = Math.max(0, Math.min(s-1, Math.floor((nx*0.5+0.5)*s)));
  const y = Math.max(0, Math.min(s-1, Math.floor((1-(ny*0.5+0.5))*s)));
  const i = (y*s + x)*4;
  return [mc.data[i], mc.data[i+1], mc.data[i+2]];
}

/* flat-triangle index (into TOPO.tris) -> its fan index WITHIN its parent face,
   so the stretch channel can look up STRETCH.triValue[face][localIndex]. TOPO.tris
   is built face-by-face in fan order, so a running per-face counter recovers it.
   Memoized against the current TOPO.tris identity (rebuilt if the mesh changes). */
let _triLocalIdx = null, _triLocalFor = null;
function triLocalIndex(t) {
  if (_triLocalFor !== TOPO.tris) {
    const map = new Int32Array(TOPO.tris.length);
    const seen = {};
    for (let i = 0; i < TOPO.tris.length; i++) {
      const f = TOPO.tris[i].face;
      const n = seen[f] || 0; map[i] = n; seen[f] = n + 1;
    }
    _triLocalIdx = map; _triLocalFor = TOPO.tris;
  }
  return _triLocalIdx[t];
}

/* ---------- UV checker projection (backlog #1) ----------
   The viewport has no UVs of its own; the 2D editor owns them per island. Mirror
   them into a flat vertIndex -> [u,v] map so the raster can interpolate a UV per
   pixel and sample the SAME checker the 2D editor draws. Call whenever the island
   set changes (after unwrap / pack). A vertex with no island UV is simply absent
   (its triangles fall back to matcap). */
function vp3SetUvChannel(islands) {
  const map = {};
  if (islands) for (const isl of islands) {
    const uv = isl.uv; if (!uv) continue;
    for (const v of isl.verts) if (uv[v]) map[v] = uv[v];
  }
  VP3.uv = map;
}

/* software-rasterize the depth-sorted triangles with a per-pixel UV checker,
   compositing over a snapshot of the current canvas (grid/clear already drawn).
   Runs ONLY when the checker channel is active AND UVs exist; otherwise the
   caller uses the vector matcap fill. Depth uses the painter's back-to-front
   `order` (no z-buffer), and each triangle still stamps the overlay depth buffer.
   All work is in PHYSICAL device pixels (scr is logical -> scale by dpr), because
   ImageData ignores the ctx dpr transform. */
function vp3RasterCheckerPass(g, scr, order, T, vn, mc, selFaces) {
  const uv = VP3.uv; if (!uv) return false;
  const dpr = VP3.dpr, W = Math.max(1, Math.round(VP3.w * dpr)), H = Math.max(1, Math.round(VP3.h * dpr));
  // start from the current canvas so the ground grid / background survive
  const img = g.getImageData(0, 0, W, H);
  const px = img.data;
  for (const [t] of order) {
    const [a, b, c] = T[t].v;
    const ua = uv[a], ub = uv[b], uc = uv[c];
    // matcap shade for this triangle (flat, as the vector path does)
    const nx = (vn[a*3]+vn[b*3]+vn[c*3])/3, ny = (vn[a*3+1]+vn[b*3+1]+vn[c*3+1])/3;
    const [mr, mg, mb] = sampleMatcapRGB(mc, nx, ny);
    const sel = selFaces.has(T[t].face);
    // triangle screen coords in device pixels
    const ax = scr[a*3]*dpr, ay = scr[a*3+1]*dpr;
    const bx = scr[b*3]*dpr, by = scr[b*3+1]*dpr;
    const cxp = scr[c*3]*dpr, cyp = scr[c*3+1]*dpr;
    // any corner missing a UV -> flat matcap fill (still stamp depth below)
    const haveUv = ua && ub && uc;
    const minX = Math.max(0, Math.floor(Math.min(ax, bx, cxp)));
    const maxX = Math.min(W-1, Math.ceil(Math.max(ax, bx, cxp)));
    const minY = Math.max(0, Math.floor(Math.min(ay, by, cyp)));
    const maxY = Math.min(H-1, Math.ceil(Math.max(ay, by, cyp)));
    const denom = (by-cyp)*(ax-cxp) + (cxp-bx)*(ay-cyp);
    if (Math.abs(denom) < 1e-9) continue;
    const invDen = 1/denom;
    for (let y = minY; y <= maxY; y++) {
      for (let x = minX; x <= maxX; x++) {
        const w0 = ((by-cyp)*(x-cxp) + (cxp-bx)*(y-cyp)) * invDen;
        const w1 = ((cyp-ay)*(x-cxp) + (ax-cxp)*(y-cyp)) * invDen;
        const w2 = 1 - w0 - w1;
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;      // outside the triangle
        let r, gg, bb;
        if (haveUv) {
          const u = w0*ua[0] + w1*ub[0] + w2*uc[0];
          const v = w0*ua[1] + w1*ub[1] + w2*uc[1];
          const ck = checkerColor(u, v, CHECKER);
          // multiply checker by matcap shade (normalized) to keep form shading
          r = ck[0]*mr/255; gg = ck[1]*mg/255; bb = ck[2]*mb/255;
        } else { r = mr; gg = mg; bb = mb; }
        if (sel) { r = r*0.45 + 80*0.55; gg = gg*0.45 + 150*0.55; bb = bb*0.45 + 255*0.55; }
        const i = (y*W + x)*4;
        px[i] = r; px[i+1] = gg; px[i+2] = bb; px[i+3] = 255;
      }
    }
    vp3DepthStampTri(scr, a, b, c);
  }
  g.putImageData(img, 0, 0);
  return true;
}

/* project a world point to screen; returns [x,y] or null if behind the eye. */
function vp3WorldToScreen(mvp, p) {
  const c = M4.transformPoint(mvp, p);
  const w = c[3];
  if (w <= 1e-4) return null;                    // behind / on the near plane
  return [(c[0]/w*0.5+0.5)*VP3.w, (-c[1]/w*0.5+0.5)*VP3.h];
}
/* A real ground grid on the y=0 plane (not flat screen lines). Two styles:
   `lines` draws the grid segments; `dots` marks only the lattice intersections.
   Lines that cross behind the eye are skipped so nothing smears across screen. */
function vp3GroundGrid(g) {
  const { mvp } = buildMVP();
  const half = 6, step = 0.5;                    // 12x12 units, half-unit cells
  if (VP3.gridMode === "lines") {
    g.lineWidth = 1;
    for (let i = -half; i <= half; i += step) {
      const axis = Math.abs(i) < 1e-6;
      g.strokeStyle = axis ? "rgba(255,255,255,0.14)" : "rgba(255,255,255,0.055)";
      // line parallel to X (varying z=i), then parallel to Z (varying x=i)
      const a1 = vp3WorldToScreen(mvp, [-half, 0, i]), b1 = vp3WorldToScreen(mvp, [half, 0, i]);
      if (a1 && b1) { g.beginPath(); g.moveTo(a1[0], a1[1]); g.lineTo(b1[0], b1[1]); g.stroke(); }
      const a2 = vp3WorldToScreen(mvp, [i, 0, -half]), b2 = vp3WorldToScreen(mvp, [i, 0, half]);
      if (a2 && b2) { g.beginPath(); g.moveTo(a2[0], a2[1]); g.lineTo(b2[0], b2[1]); g.stroke(); }
    }
  } else {                                        // "dots" — lattice intersections
    g.fillStyle = "rgba(255,255,255,0.20)";
    for (let x = -half; x <= half; x += step) {
      for (let z = -half; z <= half; z += step) {
        const s = vp3WorldToScreen(mvp, [x, 0, z]);
        if (!s) continue;
        const r = (Math.abs(x) < 1e-6 || Math.abs(z) < 1e-6) ? 1.6 : 1.0;
        g.beginPath(); g.arc(s[0], s[1], r, 0, 7); g.fill();
      }
    }
  }
}

/* ---------- depth-clipped line drawing ----------
   Append a line to the current path, but ONLY the spans that pass the surface
   depth test — the segment is walked in ~cell-sized steps, interpolating screen
   x/y AND ndc-z, and each step where z clears the stored surface depth is drawn.
   This clips an edge AT the silhouette: the camera-facing half of an edge that
   dips behind nearer geometry draws, the occluded half doesn't (per-pixel, not
   per-edge). X-ray / no depth buffer -> the whole edge draws. Contiguous visible
   samples form one polyline so line caps/joins stay smooth. */
function pathEdgeClipped(g, scr, v0, v1, xray) {
  const x0 = scr[v0*3], y0 = scr[v0*3+1], z0 = scr[v0*3+2];
  const x1 = scr[v1*3], y1 = scr[v1*3+1], z1 = scr[v1*3+2];
  if (xray || !VP3.depth) { g.moveTo(x0, y0); g.lineTo(x1, y1); return; }
  const len = Math.hypot(x1-x0, y1-y0);
  const n = Math.max(1, Math.ceil(len / VP3.depth.cell));
  let inRun = false;
  for (let i = 0; i <= n; i++) {
    const t = i / n;
    const x = x0 + (x1-x0)*t, y = y0 + (y1-y0)*t, z = z0 + (z1-z0)*t;
    if (vp3DepthVisible(x, y, z)) {
      if (!inRun) { g.moveTo(x, y); inRun = true; } else g.lineTo(x, y);
    } else inRun = false;
  }
}
/* stroke a face's polygon boundary with per-pixel depth clipping (quads/ngons
   show true edges, no fan diagonal; occluded spans are cut). */
function strokeFaceLoopClipped(g, scr, f, xray) {
  const loop = TOPO.faceVerts[f];
  g.beginPath();
  for (let i = 0; i < loop.length; i++) pathEdgeClipped(g, scr, loop[i], loop[(i+1)%loop.length], xray);
  g.stroke();
}

/* ---------- selection + hover overlays ----------
   Every overlay is depth-tested against the solid surface, PER PIXEL: an edge is
   drawn only along the spans that sit in front of the mesh, so a silhouette edge
   is clipped exactly where it crosses behind nearer geometry — matching Blender's
   default (non-X-ray) edit mode. Verts are single-point depth tested. A cheap
   backface pre-reject skips clearly-back elements so we don't sample them. Select
   -through (VP3.selectThrough) bypasses both facing and depth, revealing all.
   Each selection mode gets its own look:
     vertex : dots at every visible vertex, selected ones brightened
     edge   : thin depth-clipped wireframe, selected edges bright white
     face   : thin depth-clipped wireframe + blue-filled selected faces
     island : selected islands read as a solid coloured region (silhouette only) */
function vp3DrawOverlays(g, scr) {
  const mode = SEL.mode;
  const xray = VP3.selectThrough;
  // cheap pre-rejects (skip clearly back-facing elements before we sample depth);
  // silhouette (front-back) edges/faces are NOT rejected, they get clipped instead.
  const faceMaybe = (f) => xray || faceFrontFacing(f);
  const edgeMaybe = (k) => xray || edgeFrontFacing(k);
  // a vertex is a single point: front-facing AND clears the surface depth.
  const showVert = (v) => xray || (frontFacing(v) && vertDepthVisible(v));

  // --- toolbar ELEMENT overlays (backlog #1): faces / edges / vertices drawn ON
  //     TOP of the active display channel (solid/checker/stretch), independent of
  //     the selection mode. Toggled from the Elements dropdown. Depth-clipped so
  //     they follow the surface like the selection-mode overlays do. ---
  if (VP3.showEdges) {
    g.lineWidth = 1; g.strokeStyle = "rgba(150,150,160,0.34)"; g.beginPath();
    for (const k of TOPO.edgeKeys) { if (!edgeMaybe(k)) continue; const [v0,v1] = TOPO.edgeVerts[k]; pathEdgeClipped(g, scr, v0, v1, xray); }
    g.stroke();
  }
  if (VP3.showFaces) {
    g.lineWidth = 0.8; g.strokeStyle = "rgba(150,205,255,0.5)";
    for (let f = 0; f < TOPO.faceCount; f++) if (faceMaybe(f)) strokeFaceLoopClipped(g, scr, f, xray);
  }
  if (VP3.showVerts) {
    g.fillStyle = "rgba(230,235,245,0.9)";
    for (let v = 0; v < MESH.vertCount; v++) { if (!showVert(v)) continue; g.beginPath(); g.arc(scr[v*3], scr[v*3+1], 1.6, 0, 7); g.fill(); }
  }

  // --- base wireframe: vertex/edge/face modes (never island). Depth-clipped. ---
  if (VP3.showWire && mode !== "island") {
    g.lineWidth = 1; g.strokeStyle = "rgba(150,150,160,0.34)";
    g.beginPath();
    for (const k of TOPO.edgeKeys) {
      if (!edgeMaybe(k)) continue;
      const [v0, v1] = TOPO.edgeVerts[k];
      pathEdgeClipped(g, scr, v0, v1, xray);
    }
    g.stroke();
  }

  // --- selected faces: the solid blue tint is already laid down (depth-sorted,
  //     backface-culled) by the raster fill loop. Here we only add crisp outlines:
  //     a per-face loop in face mode, the island silhouette in island mode. All
  //     outlines are depth-clipped so occluded spans are cut. ---
  const selFaces = selectedFaceSet();
  if (mode === "face" && selFaces.size) {
    g.lineWidth = 1.3; g.strokeStyle = "rgba(150,205,255,0.9)";
    for (const f of selFaces) if (faceMaybe(f)) strokeFaceLoopClipped(g, scr, f, xray);
  } else if (mode === "island" && selFaces.size) {
    g.lineWidth = 1.8; g.strokeStyle = "rgba(150,205,255,0.95)";
    g.beginPath();
    for (const f of selFaces) {
      if (!faceMaybe(f)) continue;
      for (const k of TOPO.faceEdges[f]) {
        if (TOPO.edgeFaces[k].filter(nf => selFaces.has(nf)).length === 1) {
          const [v0, v1] = TOPO.edgeVerts[k];
          pathEdgeClipped(g, scr, v0, v1, xray);
        }
      }
    }
    g.stroke();
  }

  // --- seams: bright red with a soft halo. Two depth-clipped passes. ---
  for (const pass of [{ w: 5, c: "rgba(255,59,48,0.28)" }, { w: 2.4, c: SEAM_COLOR }]) {
    g.lineWidth = pass.w; g.strokeStyle = pass.c; g.lineCap = "round";
    g.beginPath();
    for (const k of SEAMS) {
      if (!edgeMaybe(k)) continue;
      const [v0, v1] = TOPO.edgeVerts[k];
      pathEdgeClipped(g, scr, v0, v1, xray);
    }
    g.stroke();
  }
  g.lineCap = "butt";

  // --- edge mode: selected edges bright white over the base wire. Depth-clipped. ---
  if (mode === "edge") {
    g.lineWidth = 2.4; g.strokeStyle = "#ffffff";
    g.beginPath();
    for (const k of SEL.edges) {
      if (!edgeMaybe(k)) continue;
      const [v0, v1] = TOPO.edgeVerts[k];
      pathEdgeClipped(g, scr, v0, v1, xray);
    }
    g.stroke();
  }
  // --- vertex mode: dots at every visible vertex, selected ones brightened ---
  if (mode === "vertex") {
    for (let v = 0; v < MESH.vertCount; v++) {
      if (!showVert(v)) continue;
      const sel = SEL.verts.has(v);
      g.beginPath(); g.arc(scr[v*3], scr[v*3+1], sel ? 3.6 : 2, 0, 7);
      g.fillStyle = sel ? "#ffffff" : "rgba(120,190,255,0.7)"; g.fill();
    }
  }

  // --- loop/ring preview ghost: the full set a Ctrl+click would grab, drawn in
  //     a soft amber UNDER the crisp single-element hover highlight so the user
  //     reads WHICH edges/faces the loop selects before committing (backlog #8). ---
  if (VP3.preview) {
    if (VP3.preview.edges) {
      g.lineWidth = 3; g.strokeStyle = "rgba(255,210,63,0.5)";
      g.beginPath();
      for (const k of VP3.preview.edges) {
        if (!edgeMaybe(k)) continue;
        const [v0, v1] = TOPO.edgeVerts[k];
        pathEdgeClipped(g, scr, v0, v1, xray);
      }
      g.stroke();
    } else if (VP3.preview.faces) {
      g.lineWidth = 1.4; g.strokeStyle = "rgba(255,210,63,0.55)";
      for (const f of VP3.preview.faces) if (faceMaybe(f)) strokeFaceLoopClipped(g, scr, f, xray);
    }
  }

  // --- hover highlight (depth-clipped too) ---
  if (VP3.hover.kind === "vertex" && VP3.hover.idx != null && showVert(VP3.hover.idx)) {
    const v = VP3.hover.idx;
    g.beginPath(); g.arc(scr[v*3], scr[v*3+1], 5, 0, 7);
    g.strokeStyle = "#ffd23f"; g.lineWidth = 2; g.stroke();
  } else if (VP3.hover.kind === "edge" && VP3.hover.idx != null && edgeMaybe(VP3.hover.idx)) {
    const [v0, v1] = TOPO.edgeVerts[VP3.hover.idx];
    g.strokeStyle = "#ffd23f"; g.lineWidth = 3;
    g.beginPath(); pathEdgeClipped(g, scr, v0, v1, xray); g.stroke();
  } else if (VP3.hover.kind === "face" && VP3.hover.idx != null && faceMaybe(VP3.hover.idx)) {
    g.strokeStyle = "#ffd23f"; g.lineWidth = 1.6;
    strokeFaceLoopClipped(g, scr, VP3.hover.idx, xray);
  }
}
/* stroke a face's polygon boundary (unclipped) — kept for any non-occluded use. */
function strokeFaceLoop(g, scr, f) {
  const loop = TOPO.faceVerts[f];
  g.beginPath();
  g.moveTo(scr[loop[0]*3], scr[loop[0]*3+1]);
  for (let i = 1; i < loop.length; i++) g.lineTo(scr[loop[i]*3], scr[loop[i]*3+1]);
  g.closePath(); g.stroke();
}
/* a polygon faces the camera when its fan-summed signed screen area is
   front-wound (< 0, since the source normals flip the winding). Summing the
   whole fan makes it robust for quads / ngons, not just the first triangle. */
function faceFrontFacing(f) {
  const s = VP3.scr;
  let area = 0;
  for (const [a, b, c] of TOPO.faceTris[f]) {
    area += (s[b*3]-s[a*3])*(s[c*3+1]-s[a*3+1]) - (s[c*3]-s[a*3])*(s[b*3+1]-s[a*3+1]);
  }
  return area < 0;
}
/* an edge is on the visible skin when either incident face faces the camera:
   a front-front edge and a silhouette (front-back) edge show; a back-back edge
   (hidden behind the solid surface) is culled. */
function edgeFrontFacing(k) {
  for (const f of TOPO.edgeFaces[k]) if (faceFrontFacing(f)) return true;
  return false;
}
function frontFacing(v) {
  // a vertex is drawn if any incident face faces the camera.
  for (const f of TOPO.vertFaces[v]) if (faceFrontFacing(f)) return true;
  return false;
}

/* ---------- picking ----------
   `ctrl` requests loop/path selection: on an edge it grows the full loop, or —
   if a previous pick exists on the same loop — highlights the run between them,
   falling back to the shortest edge path when they don't share a loop. On a
   face it walks a face ring; on a vertex it connects to the previous pick via
   shortest path. */
function vp3Pick(mx, my, additive, ctrl) {
  const scr = VP3.scr;
  const xray = VP3.selectThrough;   // reach occluded / back-facing geometry
  if (SEL.mode === "vertex") {
    let best = -1, bd = 12*12;
    for (let v = 0; v < MESH.vertCount; v++) {
      if (!xray && !frontFacing(v)) continue;
      const dx = scr[v*3]-mx, dy = scr[v*3+1]-my, d = dx*dx+dy*dy;
      if (d < bd) { bd = d; best = v; }
    }
    if (best < 0) { if (!additive) selClear(); return 0; }
    if (ctrl && SEL.lastPick && SEL.lastPick.mode === "vertex") selConnectPath(best);
    else selectVert(best, additive);
    return 1;
  } else if (SEL.mode === "edge") {
    let best = null, bd = 10*10;
    for (const k of TOPO.edgeKeys) {
      const [v0, v1] = TOPO.edgeVerts[k];
      if (!xray && !frontFacing(v0) && !frontFacing(v1)) continue;
      const d = distToSeg(mx, my, scr[v0*3], scr[v0*3+1], scr[v1*3], scr[v1*3+1]);
      if (d < bd) { bd = d; best = k; }
    }
    if (!best) { if (!additive) selClear(); return 0; }
    if (ctrl && SEL.lastPick && SEL.lastPick.mode === "edge" && SEL.lastPick.id !== best) selConnectPath(best);
    else if (ctrl) selLoopFromEdge(best);          // no anchor yet -> whole loop
    else selectEdge(best, additive);
    return 1;
  } else {
    // face / island: nearest fan triangle under the cursor (by depth), reported
    // as its parent polygon face. Backfaces are culled unless X-ray select is on,
    // in which case a back-facing tri can win if it is the closest under the click.
    let best = -1, bestZ = 2;
    for (const tri of TOPO.tris) {
      const [a, b, c] = tri.v;
      const ax=scr[a*3],ay=scr[a*3+1],bx=scr[b*3],by=scr[b*3+1],cx=scr[c*3],cy=scr[c*3+1];
      const area=(bx-ax)*(cy-ay)-(cx-ax)*(by-ay);
      if (!xray && area >= 0) continue;
      if (!pointInTri(mx, my, ax, ay, bx, by, cx, cy)) continue;
      const z = (scr[a*3+2]+scr[b*3+2]+scr[c*3+2])/3;
      if (z < bestZ) { bestZ = z; best = tri.face; }
    }
    if (best < 0) { if (!additive) selClear(); return 0; }
    if (ctrl && SEL.mode === "face") {
      // second Ctrl+click on a different face -> path between the two picks;
      // first Ctrl+click (no anchor yet) -> face ring from the seed.
      if (SEL.lastPick && SEL.lastPick.mode === "face" && SEL.lastPick.id !== best) selConnectPath(best);
      else selRingFromFace(best);
    }
    else selectFace(best, additive);
    return 1;
  }
}
/* screen position of a selectable element (vertex / edge midpoint / face
   centre) from the current projection scratch — feeds selectRegion. */
function vp3ElementAt(kind, id) {
  const scr = VP3.scr;
  if (kind === "vertex") return [scr[id*3], scr[id*3+1]];
  if (kind === "edge") { const [v0, v1] = TOPO.edgeVerts[id]; return [(scr[v0*3]+scr[v1*3])/2, (scr[v0*3+1]+scr[v1*3+1])/2]; }
  const loop = TOPO.faceVerts[id];
  let sx = 0, sy = 0;
  for (const v of loop) { sx += scr[v*3]; sy += scr[v*3+1]; }
  return [sx/loop.length, sy/loop.length];
}
/* front-facing test per selectable element, so a marquee never grabs occluded
   back-side geometry. With X-ray select (VP3.selectThrough) on, every element is
   reachable regardless of facing — a box/circle/lasso/paint drag then grabs the
   back side too, matching Blender's "Select Through". */
function vp3ElementVisible(kind, id) {
  if (VP3.selectThrough) return true;
  if (kind === "vertex") return frontFacing(id);
  if (kind === "edge") { const [v0, v1] = TOPO.edgeVerts[id]; return frontFacing(v0) || frontFacing(v1); }
  const s = VP3.scr, [a, b, c] = TOPO.faceTris[id][0];
  const area = (s[b*3]-s[a*3])*(s[c*3+1]-s[a*3+1]) - (s[c*3]-s[a*3])*(s[b*3+1]-s[a*3+1]);
  return area < 0;
}
/* run a marquee region selection given a screen-space inside() predicate.
   Returns how many elements the marquee touched (0 => the drag hit empty space,
   which the caller uses to clear the selection on an empty brush stroke). */
function vp3ApplyRegion(inside, additive, subtract) {
  return selectRegion(inside, vp3ElementAt, vp3ElementVisible, additive, subtract);
}
/* close out a finished selection gesture (click / box / lasso / circle / paint):
     • empty space, no add/subtract modifier -> clear the selection, whatever the
       active tool is (Blender: click-into-the-void deselects all);
     • if the selection actually changed since mousedown, record a history step
       tagged with the tool that made it, so undo/redo walks selection edits too.
   `drag.sig0` is the pre-gesture fingerprint; `touched` is how many elements the
   gesture hit (0 => it landed on empty space). */
function vp3FinishSelectGesture(drag, touched) {
  if (touched === 0 && !drag.shift && !drag.sub) {
    if (SEL.faces.size || SEL.verts.size || SEL.edges.size) selClear();
  }
  const sig1 = (typeof selSignature === "function") ? selSignature() : null;
  if (drag.sig0 != null && sig1 != null && sig1 !== drag.sig0 && typeof recordHistory === "function") {
    const toolName = (typeof SEL_TOOL_NAME !== "undefined" && SEL_TOOL_NAME[VP3.tool]) || VP3.tool;
    const n = activeSet().size;
    recordHistory("select", toolName, n + " " + SEL.mode + (n === 1 ? "" : "s"));
  }
}
function pointInPolygon(px, py, poly) {
  let hit = false;
  for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
    const xi = poly[i][0], yi = poly[i][1], xj = poly[j][0], yj = poly[j][1];
    if (((yi > py) !== (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) hit = !hit;
  }
  return hit;
}
function vp3HoverAt(mx, my, loopMod) {
  const scr = VP3.scr;
  const before = VP3.hover.kind + ":" + VP3.hover.idx + ":" + previewSig();
  VP3.hover = { kind: null, idx: null };
  VP3.preview = null;
  if (SEL.mode === "vertex") {
    let best=-1,bd=12*12; for (let v=0;v<MESH.vertCount;v++){ if(!frontFacing(v))continue; const dx=scr[v*3]-mx,dy=scr[v*3+1]-my,d=dx*dx+dy*dy; if(d<bd){bd=d;best=v;} }
    if (best>=0) VP3.hover={kind:"vertex",idx:best};
  } else if (SEL.mode === "edge") {
    let best=null,bd=10*10; for(const k of TOPO.edgeKeys){const[v0,v1]=TOPO.edgeVerts[k]; if(!frontFacing(v0)&&!frontFacing(v1))continue; const d=distToSeg(mx,my,scr[v0*3],scr[v0*3+1],scr[v1*3],scr[v1*3+1]); if(d<bd){bd=d;best=k;}}
    if (best) VP3.hover={kind:"edge",idx:best};
  } else {
    let best=-1,bestZ=2; for(const tri of TOPO.tris){const[a,b,c]=tri.v;const ax=scr[a*3],ay=scr[a*3+1],bx=scr[b*3],by=scr[b*3+1],cx=scr[c*3],cy=scr[c*3+1];const area=(bx-ax)*(cy-ay)-(cx-ax)*(by-ay);if(area>=0)continue;if(!pointInTri(mx,my,ax,ay,bx,by,cx,cy))continue;const z=(scr[a*3+2]+scr[b*3+2]+scr[c*3+2])/3;if(z<bestZ){bestZ=z;best=tri.face;}}
    if (best>=0) VP3.hover={kind:"face",idx:best};
  }
  // while the loop-modifier is held, ghost the full set a click would grab so the
  // user can see which loop/ring is picked before committing (backlog #8). Only
  // in edge/face modes with an anchorless hover — mirrors vp3Pick's ctrl path.
  if (loopMod) VP3.preview = buildLoopPreview();
  if (before !== VP3.hover.kind + ":" + VP3.hover.idx + ":" + previewSig()) vp3Draw();
}
/* the loop/ring a Ctrl+click on the hovered element would select, as a ghost
   set — no SEL mutation. Matches the anchorless branch of vp3Pick: edge -> the
   quad edge loop; face -> the face band. Vertex mode has no anchorless loop
   preview (its ctrl path is a two-pick path), so it returns null. */
function buildLoopPreview() {
  const h = VP3.hover;
  if (h.kind === "edge" && h.idx != null) {
    return { seed: h.idx, edges: loopEdgeSet(h.idx), faces: null };
  }
  if (h.kind === "face" && h.idx != null) {
    return { seed: h.idx, edges: null, faces: computeFaceBand(h.idx) };
  }
  return null;
}
/* a compact fingerprint of the current preview so hover redraws only fire when
   the ghosted set actually changes. */
function previewSig() {
  const p = VP3.preview;
  if (!p) return "";
  const set = p.edges || p.faces;
  return (p.edges ? "e" : "f") + ":" + p.seed + ":" + (set ? set.size : 0);
}
function pointInTri(px,py,ax,ay,bx,by,cx,cy){
  const d1=(px-bx)*(ay-by)-(ax-bx)*(py-by);
  const d2=(px-cx)*(by-cy)-(bx-cx)*(py-cy);
  const d3=(px-ax)*(cy-ay)-(cx-ax)*(py-ay);
  const neg=(d1<0)||(d2<0)||(d3<0), pos=(d1>0)||(d2>0)||(d3>0);
  return !(neg&&pos);
}
function distToSeg(px,py,x0,y0,x1,y1){
  const dx=x1-x0,dy=y1-y0,l2=dx*dx+dy*dy||1e-6;
  let t=((px-x0)*dx+(py-y0)*dy)/l2; t=Math.max(0,Math.min(1,t));
  const qx=x0+t*dx,qy=y0+t*dy; return Math.hypot(px-qx,py-qy);
}

/* ---------- input (orbit / pan / zoom / pick) with eased camera ---------- */
let vp3Drag = null;
function vp3AttachInput() {
  const cv = VP3.canvas, wrap = cv.parentElement;
  wrap.addEventListener("mousedown", e => {
    if (e.target.closest(".floaty")) return;   // don't orbit when clicking bottom bar / view chrome
    const r = wrap.getBoundingClientRect(), mx = e.clientX-r.left, my = e.clientY-r.top;
    if (e.button === 1 || e.shiftKey && e.button === 0 && APP.spaceDown || APP.spaceDown) {
      vp3Drag = { type:"pan", sx:mx, sy:my, p0:VP3.tPan.slice(), basis:cameraBasis() }; return;
    }
    if (e.button === 2) { vp3Drag = { type:"orbit", sx:mx, sy:my, y0:VP3.yaw, p0:VP3.pitch }; return; }
    if (e.button === 0) {
      // marquee tools take over LMB-drag; "pick" keeps the orbit-or-click path.
      // ctrl / alt / cmd are interchangeable loop-modifiers: Alt+click and
      // Ctrl+Alt+click both trace a loop just like Ctrl+click.
      const loopMod = e.ctrlKey || e.altKey || e.metaKey;
      const sub = loopMod;    // ctrl/alt-drag subtracts from a marquee selection
      // fingerprint the selection before the gesture so mouseup can record a
      // history step iff it actually changed. `shift` is the plain additive
      // modifier (kept apart from the brush's internal always-add), used to
      // decide whether an empty-space gesture should clear the selection.
      const sig0 = (typeof selSignature === "function") ? selSignature() : null;
      if (VP3.tool === "box" || VP3.tool === "lasso") {
        vp3Drag = { type: VP3.tool, sx:mx, sy:my, add:e.shiftKey, sub, shift:e.shiftKey, sig0, pts:[[mx,my]] };
        VP3.marquee = { tool: VP3.tool, x0:mx, y0:my, x1:mx, y1:my, pts:[[mx,my]], sub };
        vp3Draw(); return;
      }
      if (VP3.tool === "circle" || VP3.tool === "paint") {
        vp3Drag = { type: VP3.tool, add:true, sub, shift:e.shiftKey, sig0, touched:0 };
        VP3.marquee = { tool:"circle", cx:mx, cy:my, r:VP3.brushRadius, sub };
        vp3Drag.touched += vp3ApplyRegion((x,y)=>Math.hypot(x-mx,y-my)<=VP3.brushRadius, true, sub);
        vp3Draw(); return;
      }
      vp3Drag = { type:"maybe-orbit", sx:mx, sy:my, y0:VP3.yaw, p0:VP3.pitch, add:e.shiftKey, ctrl:loopMod, shift:e.shiftKey, sig0 };
    }
  });
  window.addEventListener("mousemove", e => {
    const r = wrap.getBoundingClientRect(), mx = e.clientX-r.left, my = e.clientY-r.top;
    if (!vp3Drag) { if (mx>=0&&my>=0&&mx<=VP3.w&&my<=VP3.h) vp3HoverAt(mx, my, e.ctrlKey||e.altKey||e.metaKey); return; }
    // --- marquee tools ---
    if (vp3Drag.type === "box") {
      VP3.marquee.x1 = mx; VP3.marquee.y1 = my; vp3Draw(); return;
    }
    if (vp3Drag.type === "lasso") {
      vp3Drag.pts.push([mx,my]); VP3.marquee.pts.push([mx,my]); vp3Draw(); return;
    }
    if (vp3Drag.type === "circle" || vp3Drag.type === "paint") {
      VP3.marquee.cx = mx; VP3.marquee.cy = my;
      vp3Drag.touched += vp3ApplyRegion((x,y)=>Math.hypot(x-mx,y-my)<=VP3.brushRadius, true, vp3Drag.sub);
      vp3Draw(); return;
    }
    const dx = mx-vp3Drag.sx, dy = my-vp3Drag.sy;
    if (vp3Drag.type === "maybe-orbit" && (Math.abs(dx)>4||Math.abs(dy)>4)) vp3Drag.type = "orbit";
    if (vp3Drag.type === "orbit") {
      VP3.tYaw = vp3Drag.y0 - dx*0.01;
      VP3.tPitch = Math.max(-1.4, Math.min(1.4, vp3Drag.p0 + dy*0.01));
      vp3Arm();
    } else if (vp3Drag.type === "pan") {
      // slide the pivot along the camera's right/up axes: drag-right moves the
      // model right on screen at any orbit angle. scaled by dist so the grab
      // point stays roughly under the cursor.
      const k = 0.0024 * VP3.dist, { right, up } = vp3Drag.basis;
      VP3.tPan = [
        vp3Drag.p0[0] - (right[0]*dx - up[0]*dy) * k,
        vp3Drag.p0[1] - (right[1]*dx - up[1]*dy) * k,
        vp3Drag.p0[2] - (right[2]*dx - up[2]*dy) * k,
      ];
      vp3Arm();
    }
  });
  window.addEventListener("mouseup", e => {
    const drag = vp3Drag;
    // how many elements the gesture touched (null = not a selection gesture, so
    // no empty-space clear); the pick path reports its own via vp3Pick's return.
    let touched = null;
    if (drag && drag.type === "maybe-orbit") {
      const r = wrap.getBoundingClientRect();
      touched = vp3Pick(e.clientX-r.left, e.clientY-r.top, drag.add, drag.ctrl);
    } else if (drag && drag.type === "box") {
      const { x0, y0, x1, y1 } = { x0:drag.sx, y0:drag.sy, x1:VP3.marquee.x1, y1:VP3.marquee.y1 };
      const lo=[Math.min(x0,x1),Math.min(y0,y1)], hi=[Math.max(x0,x1),Math.max(y0,y1)];
      touched = vp3ApplyRegion((x,y)=>x>=lo[0]&&x<=hi[0]&&y>=lo[1]&&y<=hi[1], drag.add, drag.sub);
      VP3.marquee = null;
    } else if (drag && drag.type === "lasso") {
      const poly = drag.pts;
      touched = (poly.length >= 3) ? vp3ApplyRegion((x,y)=>pointInPolygon(x,y,poly), drag.add, drag.sub) : 0;
      VP3.marquee = null;
    } else if (drag && (drag.type === "circle" || drag.type === "paint")) {
      touched = drag.touched;
      VP3.marquee = null;
    }
    if (drag && touched !== null) vp3FinishSelectGesture(drag, touched);
    vp3Drag = null;
    VP3.preview = null;   // a committed pick replaces the ghost with the real selection
    vp3Draw();
  });
  wrap.addEventListener("wheel", e => {
    e.preventDefault();
    VP3.tDist = Math.max(1.2, Math.min(20, VP3.tDist * (e.deltaY<0 ? 0.9 : 1.1)));
    vp3Arm();
  }, { passive:false });
  wrap.addEventListener("contextmenu", e => e.preventDefault());
  // clear hover + loop preview when the cursor leaves the viewport
  wrap.addEventListener("mouseleave", () => {
    if (VP3.hover.kind || VP3.preview) { VP3.hover = { kind: null, idx: null }; VP3.preview = null; vp3Draw(); }
  });
  // releasing the loop-modifier drops the preview even without a mouse move
  window.addEventListener("keyup", e => {
    if (VP3.preview && !(e.ctrlKey || e.altKey || e.metaKey)) { VP3.preview = null; vp3Draw(); }
  });
}
function vp3Arm() { if (!VP3.running) { VP3.running = true; requestAnimationFrame(vp3Tick); } }
function vp3Tick() {
  const k = 0.25;
  VP3.yaw += (VP3.tYaw-VP3.yaw)*k;
  VP3.pitch += (VP3.tPitch-VP3.pitch)*k;
  VP3.dist += (VP3.tDist-VP3.dist)*k;
  VP3.pan[0] += (VP3.tPan[0]-VP3.pan[0])*k;
  VP3.pan[1] += (VP3.tPan[1]-VP3.pan[1])*k;
  VP3.pan[2] += (VP3.tPan[2]-VP3.pan[2])*k;
  vp3Draw();
  const done = Math.abs(VP3.tYaw-VP3.yaw)<1e-4 && Math.abs(VP3.tPitch-VP3.pitch)<1e-4 &&
               Math.abs(VP3.tDist-VP3.dist)<1e-3 && Math.abs(VP3.tPan[0]-VP3.pan[0])<1e-3 &&
               Math.abs(VP3.tPan[1]-VP3.pan[1])<1e-3 && Math.abs(VP3.tPan[2]-VP3.pan[2])<1e-3;
  if (done) { VP3.running = false; vp3Draw(); } else requestAnimationFrame(vp3Tick);
}
function vp3ResetView() { VP3.tYaw=0.6; VP3.tPitch=0.25; VP3.tDist=4.2; VP3.tPan=[0,0,0]; vp3Arm(); }
/* collect the world-space vertices covered by the current selection (mode-aware);
   empty selection -> the whole mesh, so "frame" with nothing selected fits all. */
function vp3SelectionVerts() {
  const verts = new Set();
  if (SEL.mode === "vertex") for (const v of SEL.verts) verts.add(v);
  else if (SEL.mode === "edge") for (const k of SEL.edges) { const [a,b]=TOPO.edgeVerts[k]; verts.add(a); verts.add(b); }
  else for (const f of selectedFaceSet()) for (const v of TOPO.faceVerts[f]) verts.add(v);
  if (!verts.size) for (let v=0; v<MESH.vertCount; v++) verts.add(v);
  return verts;
}
/* Frame the selection (Blender F/View-Selected): recenter the orbit pivot on the
   selection's bounding box and pull the camera in so it fills the view. */
function vp3FrameSelection() {
  const verts = vp3SelectionVerts(), P = MESH.positions;
  let mn = [1e9,1e9,1e9], mx = [-1e9,-1e9,-1e9];
  for (const v of verts) for (let a=0; a<3; a++) {
    const c = P[v*3+a]; if (c<mn[a]) mn[a]=c; if (c>mx[a]) mx[a]=c;
  }
  const centre = [(mn[0]+mx[0])/2, (mn[1]+mx[1])/2, (mn[2]+mx[2])/2];
  const radius = Math.max(0.15, 0.5*Math.hypot(mx[0]-mn[0], mx[1]-mn[1], mx[2]-mn[2]));
  VP3.tPan = centre;
  // distance so the bounding sphere fits the 45deg vertical FOV, with headroom
  VP3.tDist = Math.min(20, Math.max(1.2, radius / Math.tan(22.5*Math.PI/180) * 1.35));
  vp3Arm();
}
function vp3SetMatcap(name) { VP3.matcap = name; vp3Draw(); }
function vp3SetTool(t) { VP3.tool = t; VP3.marquee = null; vp3Draw(); }
