"use strict";
/* ==========================================================================
   uveditor2d.js -- the 2D UV pane. Draws the packed islands produced by
   unwrap.js on a 0-1 / UDIM canvas, with select / box / lasso, move / rotate /
   scale gizmo, mirror, snap and camera-lag pan-zoom (ported from the original
   UVEditor.html). Unlike the original it is driven by REAL geometry: each
   "shell" is an unwrap island; its polygons are the mesh faces flattened into
   UV space, so editing here maps back to actual faces in the 3D pane.
   ========================================================================== */
const UVED = {
  canvas: null, bg: null, cx: null, bgx: null,
  w: 0, h: 0, dpr: Math.min(window.devicePixelRatio || 1, 2),
  islands: [],                 // [{ id, name, color, faces:[fi], uv:{vi:[u,v]}, selected, verts:[vi] }]
  view: { x: 0, y: 0, zoom: 1, tx: 0, ty: 0, tzoom: 1 },
  tool: "select",
  settings: { grid: "L", showGrid: true, showBounds: true, showGizmo: true, snap: false, snapDiv: 16, showTexels: true, showAllIslands: true, packOnUnwrap: true },
  hoverU: 0.5, hoverV: 0.5,
  // UV coordinate the view is centred on. 0.5 keeps the classic single-tile
  // framing (0-1 box centred); the UDIM pan can move it to view other tiles.
  viewU0: 0.5, viewV0: 0.5,
  running: false,
  preview: null,               // ghost projection of selected faces not yet unwrapped
};
const UV_PALETTE = ['#ef4444','#eab308','#22c55e','#14b8a6','#3b82f6','#a855f7','#ec4899','#f97316','#84cc16','#06b6d4','#d946ef','#fde047'];

/* ---- ingest islands from unwrap ---- */
function uvedSetIslands(islands) {
  UVED.islands = islands.map((isl, i) => ({
    id: "isl" + i,
    name: "Island " + (i + 1),
    color: UV_PALETTE[i % UV_PALETTE.length],
    faces: isl.faces.slice(),
    verts: isl.verts.slice(),
    uv: Object.fromEntries(isl.verts.map(v => [v, isl.uv[v].slice()])),
    weight: isl.weight || 1,      // importance: >1 claims more atlas space
    pinned: !!isl.pinned,         // frozen in place: a repack never moves/resizes it
    tile: isl.tile ? { tu: isl.tile.tu | 0, tv: isl.tile.tv | 0 } : { tu: 0, tv: 0 },
    selected: false,
  }));
  // any preview whose faces now belong to a real island is stale -> recompute
  if (UVED.preview) uvedBuildPreview(selectedFaceSet());
  if (typeof vp3SetUvChannel === "function") vp3SetUvChannel(UVED.islands);
  uvedRefreshStretch();
  uvedRenderTree();
  uvedDraw();
}

/* ---- merge freshly-unwrapped islands into the existing layout ----
   Blender behaviour: unwrapping a SELECTION lays the NEW island(s) into the full
   0-1 space (they "fill" it), and previously-unwrapped islands STAY EXACTLY where
   they are — so the new shells may overlap old ones. That overlap is intentional
   and is only resolved when the user explicitly Packs. We therefore:
     • drop only the prior islands whose faces are being re-done (superseded),
     • keep every other prior island untouched (same UV coords, no reflow),
     • append the new islands as unwrap produced them (already 0-1 normalized).
   No repack here. Passing an empty/absent targetFaces (or unwrapping with no
   selection = whole mesh) falls back to a full replace. Returns the stored list. */
function uvedMergeIslands(newIslands, targetFaces) {
  const target = targetFaces instanceof Set ? targetFaces : new Set(targetFaces || []);
  const autoPack = UVED.settings.packOnUnwrap;
  // no target context, or the unwrap covered the whole mesh -> replace outright
  if (!target.size || target.size >= TOPO.faceCount) { uvedSetIslands(newIslands); return UVED.islands; }
  // keep prior islands that don't touch any re-done face, in place, with a new
  // color slot assigned after (so old + new stay visually distinct)
  const kept = UVED.islands.filter(isl => !isl.faces.some(f => target.has(f)));
  const combined = kept.map(isl => ({
    faces: isl.faces.slice(), verts: isl.verts.slice(),
    uv: Object.fromEntries(isl.verts.map(v => [v, isl.uv[v].slice()])),
    weight: isl.weight || 1, color: isl.color, selected: false,
    pinned: !!isl.pinned, tile: isl.tile ? { tu: isl.tile.tu | 0, tv: isl.tile.tv | 0 } : { tu: 0, tv: 0 },
  })).concat(newIslands.map(isl => ({
    faces: isl.faces.slice(), verts: isl.verts.slice(),
    uv: Object.fromEntries(isl.verts.map(v => [v, isl.uv[v].slice()])),
    weight: isl.weight || 1, selected: true,   // the just-unwrapped shells are the active selection
    pinned: !!isl.pinned, tile: isl.tile ? { tu: isl.tile.tu | 0, tv: isl.tile.tv | 0 } : { tu: 0, tv: 0 },
  })));
  // Auto-pack ON: organize the whole set together so nothing overlaps (kept
  // islands may move). OFF (Blender-style): leave prior islands exactly in place;
  // the new shells filled 0-1 already and are allowed to overlap until Pack.
  if (autoPack) packIslands(combined);
  uvedStoreIslands(combined);
  return UVED.islands;
}
/* store a pre-built island array (preserving any color/selected already set)
   without re-flattening or repacking — the shared ingest path for merge. */
function uvedStoreIslands(list) {
  UVED.islands = list.map((isl, i) => ({
    id: "isl" + i,
    name: "Island " + (i + 1),
    color: isl.color || UV_PALETTE[i % UV_PALETTE.length],
    faces: isl.faces.slice(),
    verts: isl.verts.slice(),
    uv: Object.fromEntries(isl.verts.map(v => [v, isl.uv[v].slice()])),
    weight: isl.weight || 1,
    pinned: !!isl.pinned,
    tile: isl.tile ? { tu: isl.tile.tu | 0, tv: isl.tile.tv | 0 } : { tu: 0, tv: 0 },
    selected: !!isl.selected,
  }));
  if (UVED.preview) uvedBuildPreview(selectedFaceSet());
  if (typeof vp3SetUvChannel === "function") vp3SetUvChannel(UVED.islands);
  uvedRefreshStretch();
  uvedRenderTree();
  uvedDraw();
}

/* recompute the stretch-distortion metrics for the current islands (backlog #2).
   Called wherever UVs change so the 2D/3D stretch channel stays accurate. Cheap
   to always run (a few thousand fan triangles); skipped only when the module is
   absent (headless load-order guard). */
function uvedRefreshStretch() {
  if (typeof computeStretchMetrics === "function") computeStretchMetrics(UVED.islands, STRETCH.mode);
}
/* is the stretch channel visible anywhere (2D or projected on 3D)? gates the
   live recompute during scale/rotate so we don't pay for it when it's off. */
function stretchChannelLive() {
  return typeof STRETCH !== "undefined" && (STRETCH.channel2d || STRETCH.enabled3d);
}

/* re-run the active packing algorithm over the CURRENT islands in place,
   honoring each island's importance weight. Called after a weight change or a
   packer switch so the atlas re-lays out without re-flattening the geometry. */
function uvedRepack() {
  if (!UVED.islands.length) return;
  // Selection-driven, single-tile. Pack ONLY the selected islands (fall back to
  // ALL islands when nothing is selected) into the ACTIVE UDIM tile. Unselected
  // islands stay exactly where they are, even if they overlap. Pinned islands in
  // the active tile always seed as obstacles (frozen), so any free selected
  // islands pack AROUND them. Overflow = free shells that couldn't clear a pin.
  const sel = UVED.islands.filter(i => i.selected);
  const subset = sel.length ? sel : null;   // null => pack the whole active tile
  const tile = (typeof UDIM !== "undefined" && UDIM.activeTile) ? UDIM.activeTile : { tu: 0, tv: 0 };
  const r = packIslands(UVED.islands, { tile, subset });   // verts+uv+weight+pinned+tile
  const overflow = (r && r.overflow) || 0;
  if (overflow > 0 && typeof toast === "function") toast(overflow + " shell(s) overlap a pinned island");
  if (typeof vp3SetUvChannel === "function") vp3SetUvChannel(UVED.islands);
  uvedRefreshStretch();
  uvedDraw();
}

/* pin / unpin the selected islands (or all if none selected). Pinned islands are
   frozen: a repack packs around them and never moves or resizes them. */
function uvedSetPinned(on) {
  const sel = UVED.islands.filter(i => i.selected);
  const target = sel.length ? sel : UVED.islands;
  for (const isl of target) isl.pinned = !!on;
  uvedRenderTree();
  uvedDraw();
}
/* assign the selected islands (or all if none) to UDIM tile (tu,tv): translate
   each island's verts by the delta between its current tile and the target,
   preserving intra-tile layout, and stamp isl.tile. Distinct from a repack -- a
   pure translate, so a laid-out shell keeps its arrangement in the new tile. */
function uvedAssignToTile(tu, tv) {
  const sel = UVED.islands.filter(i => i.selected);
  const target = sel.length ? sel : UVED.islands;
  for (const isl of target) {
    const cur = (typeof islandTile === "function") ? islandTile(isl) : (isl.tile || { tu: 0, tv: 0 });
    const du = tu - cur.tu, dv = tv - cur.tv;
    if (du || dv) islandMove(isl, du, dv);
    isl.tile = { tu, tv };
  }
  if (typeof vp3SetUvChannel === "function") vp3SetUvChannel(UVED.islands);
  uvedRefreshStretch();
  uvedRenderTree();
  uvedDraw();
}

/* toggle pin on the selection (keybind path). */
function uvedTogglePin() {
  const sel = UVED.islands.filter(i => i.selected);
  const target = sel.length ? sel : UVED.islands;
  const anyUnpinned = target.some(i => !i.pinned);
  for (const isl of target) isl.pinned = anyUnpinned;   // pin all if any is loose
  if (typeof toast === "function") toast(anyUnpinned ? "Pinned selected shells" : "Unpinned selected shells");
  if (typeof recordHistory === "function") recordHistory("pin", anyUnpinned ? "Pin selected shells" : "Unpin selected shells");
  uvedRenderTree();
  uvedDraw();
}

/* upper limit on an island's importance weight. Held very high (not the old 32)
   so the %-atlas-share control can drive one island toward ~100% of the box:
   share = w / (w + restSum), so reaching 99% with many other islands needs a
   large weight. The ×N field still presents a sane 0.1..32 range; this ceiling
   only bounds the share solve. */
const WEIGHT_MAX = 1e6;

/* set the importance weight of the selected islands, then repack. */
function uvedSetWeight(w) {
  const sel = UVED.islands.filter(i => i.selected);
  const target = sel.length ? sel : UVED.islands;
  for (const isl of target) isl.weight = Math.max(0.1, Math.min(WEIGHT_MAX, w));
  uvedRepack();
  uvedRenderTree();
}
function uvedNudgeWeight(mult) {
  const sel = UVED.islands.filter(i => i.selected);
  const target = sel.length ? sel : UVED.islands;
  for (const isl of target) isl.weight = Math.max(0.1, Math.min(WEIGHT_MAX, (isl.weight || 1) * mult));
  uvedRepack();
  uvedRenderTree();
}

function uvedInit() {
  UVED.bg = document.getElementById("uvBg"); UVED.bgx = UVED.bg.getContext("2d");
  UVED.canvas = document.getElementById("uvFg"); UVED.cx = UVED.canvas.getContext("2d");
  uvedResize();
  uvedAttachInput();
}
function uvedResize() {
  const r = UVED.bg.parentElement.getBoundingClientRect();
  UVED.w = Math.max(1, r.width); UVED.h = Math.max(1, r.height);
  [UVED.bg, UVED.canvas].forEach(c => { c.width = UVED.w*UVED.dpr; c.height = UVED.h*UVED.dpr; });
  UVED.bgx.setTransform(UVED.dpr,0,0,UVED.dpr,0,0);
  UVED.cx.setTransform(UVED.dpr,0,0,UVED.dpr,0,0);
  uvedDraw();
}

/* ---- coordinate transforms ---- */
function uvGridPx() { return ({ S:300, M:420, L:560, XL:760 })[UVED.settings.grid] || 560; }
function uvOX() { return UVED.w/2 + UVED.view.x; }
function uvOY() { return UVED.h/2 + UVED.view.y; }
function uvSize() { return uvGridPx() * UVED.view.zoom; }
function u2sx(u) { return uvOX() + (u-UVED.viewU0)*uvSize(); }
function v2sy(v) { return uvOY() + (UVED.viewV0-v)*uvSize(); }   // V up
function sx2u(x) { return (x-uvOX())/uvSize() + UVED.viewU0; }
function sy2v(y) { return UVED.viewV0 - (y-uvOY())/uvSize(); }

/* ---- draw ---- */
function uvedDraw() { uvedDrawBG(); uvedDrawFG(); }
function uvedDrawBG() {
  const g = UVED.bgx, st = UVED.settings, s = uvSize();
  g.clearRect(0,0,UVED.w,UVED.h); g.fillStyle = "#101010"; g.fillRect(0,0,UVED.w,UVED.h);
  // dotted background
  const gap = 22 * Math.max(.6, Math.min(1.6, UVED.view.zoom));
  g.fillStyle = "#181818";
  const offx = uvOX()%gap, offy = uvOY()%gap;
  for (let x = offx; x < UVED.w; x += gap) for (let y = offy; y < UVED.h; y += gap) { g.beginPath(); g.arc(x,y,1,0,7); g.fill(); }
  const x0 = u2sx(0), y0 = v2sy(1);   // top-left of tile (0,0) in screen (V up -> v=1 is top)
  // UDIM tiles to draw. When udim.js is absent (headless / older load order) fall
  // back to a single 0-1 tile so behaviour is unchanged.
  const cols = (typeof UDIM !== "undefined") ? UDIM.cols : 1;
  const rows = (typeof UDIM !== "undefined") ? UDIM.rows : 1;
  const active = (typeof UDIM !== "undefined") ? UDIM.activeTile : { tu: 0, tv: 0 };
  // per-tile top-left in screen space (each tile is `s` px on a side).
  const tileTL = (tu, tv) => ({ tx: u2sx(tu), ty: v2sy(tv + 1) });
  // checker / texel guide per visible tile that intersects the viewport.
  const onScreen = (tx, ty) => tx + s > 0 && tx < UVED.w && ty + s > 0 && ty < UVED.h;
  for (let tv = 0; tv < rows; tv++) for (let tu = 0; tu < cols; tu++) {
    const { tx, ty } = tileTL(tu, tv);
    if (!onScreen(tx, ty)) continue;
    if (typeof CHECKER !== "undefined" && CHECKER.channel2d) {
      uvedDrawChecker(g, tx, ty, s);
    } else if (st.showTexels) {
      const n = 8, c = s/n;
      for (let i=0;i<n;i++) for (let j=0;j<n;j++){ g.fillStyle=((i+j)&1)?"rgba(255,255,255,.03)":"rgba(255,255,255,.012)"; g.fillRect(tx+i*c, ty+j*c, c+0.5, c+0.5); }
    }
  }
  // fine sub-grid only inside the active tile (keeps the multi-tile view readable).
  if (st.showGrid) {
    const div = st.snapDiv>0?st.snapDiv:16, step = s/div;
    if (step >= 6) {
      const { tx, ty } = tileTL(active.tu, active.tv);
      g.strokeStyle="rgba(255,255,255,.05)"; g.lineWidth=1;
      for (let i=1;i<div;i++){ const t=i/div; g.beginPath();g.moveTo(tx+t*s,ty);g.lineTo(tx+t*s,ty+s);g.stroke(); g.beginPath();g.moveTo(tx,ty+t*s);g.lineTo(tx+s,ty+t*s);g.stroke(); }
    }
  }
  // axes (through the origin of tile 0,0)
  g.strokeStyle="rgba(255,255,255,.18)"; g.lineWidth=1.4;
  g.beginPath();g.moveTo(0,v2sy(0));g.lineTo(UVED.w,v2sy(0));g.stroke();
  g.beginPath();g.moveTo(u2sx(0),0);g.lineTo(u2sx(0),UVED.h);g.stroke();
  // per-tile bounds box + UDIM number label; the active tile reads brighter.
  if (st.showBounds) {
    g.font="11px system-ui"; g.textBaseline="alphabetic";
    for (let tv = 0; tv < rows; tv++) for (let tu = 0; tu < cols; tu++) {
      const { tx, ty } = tileTL(tu, tv);
      if (!onScreen(tx, ty)) continue;
      const isActive = tu === active.tu && tv === active.tv;
      g.strokeStyle = isActive ? "rgba(59,130,246,.85)" : "rgba(59,130,246,.28)";
      g.lineWidth = isActive ? 1.9 : 1.1;
      g.strokeRect(tx, ty, s, s);
      const num = (typeof tileToUdim === "function") ? tileToUdim(tu, tv) : (1001 + tu + tv*10);
      g.fillStyle = isActive ? "rgba(255,255,255,.6)" : "rgba(255,255,255,.28)";
      g.fillText(String(num), tx+5, ty+15);
    }
  }
}
/* paint the procedural checker board across the 0-1 UV box as a reference layer.
   `x0,y0` = screen top-left of the box, `s` = its on-screen size. Colours come
   from the shared checkerColor() so this matches the 3D projection exactly. */
function uvedDrawChecker(g, x0, y0, s) {
    const tiles = CHECKER.tiles || 8, c = s / tiles;
    for (let i = 0; i < tiles; i++) for (let j = 0; j < tiles; j++) {
        // tile (i,j): i across U, j down the screen -> v = 1 - (j+0.5)/tiles
        const u = (i + 0.5) / tiles, v = 1 - (j + 0.5) / tiles;
        const [r, gg, b] = checkerColor(u, v, CHECKER);
        g.fillStyle = "rgb(" + r + "," + gg + "," + b + ")";
        g.fillRect(x0 + i * c, y0 + j * c, c + 0.5, c + 0.5);
    }
}

/* draw ONE island. `filled` fills the polygon interiors; when false only the
   boundary loop is stroked (used so overlapping shells stay readable as
   outlines instead of stacking into one opaque blob). */
function uvedDrawIsland(isl, filled) {
  const g = UVED.cx;
  // per-triangle fill CHANNEL: the checker samples checkerColor at each fan
  // triangle's UV centroid; the stretch heatmap looks up the precomputed
  // distortion colour for that triangle. Either shows THROUGH the actual islands
  // (only where UVs land), not just as the background box. Otherwise the flat
  // island tint is used.
  const checkerFill = filled && typeof CHECKER !== "undefined" && CHECKER.channel2d;
  const stretchFill = filled && typeof STRETCH !== "undefined" && STRETCH.channel2d && STRETCH.triValue;
  if (filled) {
    g.fillStyle = hexA(isl.color, isl.selected ? 0.34 : 0.16);
    for (const f of isl.faces) {
      const loop = TOPO.faceVerts[f];
      if (loop.some(v => !isl.uv[v])) continue;
      const ftris = TOPO.faceTris[f];
      for (let ti = 0; ti < ftris.length; ti++) {
        const [a,b,c] = ftris[ti];
        const pa = isl.uv[a], pb = isl.uv[b], pc = isl.uv[c];
        if (checkerFill) {
          const cu = (pa[0]+pb[0]+pc[0])/3, cv = (pa[1]+pb[1]+pc[1])/3;
          const [r,gg,bb] = checkerColor(cu, cv, CHECKER);
          g.fillStyle = "rgba(" + r + "," + gg + "," + bb + "," + (isl.selected ? 0.92 : 0.8) + ")";
        } else if (stretchFill) {
          const col = stretchTriColor(f, ti);
          if (col) g.fillStyle = "rgba(" + col[0] + "," + col[1] + "," + col[2] + "," + (isl.selected ? 0.9 : 0.78) + ")";
          else g.fillStyle = hexA(isl.color, isl.selected ? 0.34 : 0.16);
        }
        g.beginPath();
        g.moveTo(u2sx(pa[0]), v2sy(pa[1]));
        g.lineTo(u2sx(pb[0]), v2sy(pb[1]));
        g.lineTo(u2sx(pc[0]), v2sy(pc[1]));
        g.closePath(); g.fill();
      }
    }
  }
  // stroke the polygon LOOP of every face (real boundary edges only, no diagonal)
  g.strokeStyle = isl.selected ? "#ffffff" : hexA(isl.color, filled ? 0.85 : 0.6);
  g.lineWidth = isl.selected ? 1.2 : 0.7;
  for (const f of isl.faces) {
    const loop = TOPO.faceVerts[f];
    if (loop.some(v => !isl.uv[v])) continue;
    g.beginPath();
    const p0 = isl.uv[loop[0]]; g.moveTo(u2sx(p0[0]), v2sy(p0[1]));
    for (let i = 1; i < loop.length; i++) { const p = isl.uv[loop[i]]; g.lineTo(u2sx(p[0]), v2sy(p[1])); }
    g.closePath(); g.stroke();
  }
  // pinned shells read as frozen: a solid amber bbox so it's clear a repack won't
  // touch them (distinct from the white dashed selection emphasis below).
  if (isl.pinned) {
    const b = islandUVBounds(isl);
    const bx = u2sx(b.mnU), by = v2sy(b.mxV), bw = (b.mxU-b.mnU)*uvSize(), bh = (b.mxV-b.mnV)*uvSize();
    g.strokeStyle = "rgba(245,158,11,.9)"; g.lineWidth = 1.6; g.setLineDash([]);
    g.strokeRect(bx, by, bw, bh);
    g.fillStyle = "rgba(245,158,11,.95)"; g.font = "10px system-ui"; g.textBaseline = "top";
    g.fillText("📌", bx + 2, by + 2);   // pushpin, top-left corner
  }
  // dashed bbox emphasis when selected so a selected shell reads through overlaps
  if (isl.selected) {
    const b = islandUVBounds(isl);
    g.strokeStyle = "rgba(255,255,255,.35)"; g.setLineDash([4,4]); g.lineWidth=1;
    g.strokeRect(u2sx(b.mnU), v2sy(b.mxV), (b.mxU-b.mnU)*uvSize(), (b.mxV-b.mnV)*uvSize());
    g.setLineDash([]);
  }
}
function uvedDrawFG() {
  const g = UVED.cx; g.clearRect(0,0,UVED.w,UVED.h);
  const showAll = UVED.settings.showAllIslands;
  const unsel = UVED.islands.filter(i => !i.selected);
  const sel   = UVED.islands.filter(i =>  i.selected);
  // pass 1: unselected islands underneath. In "All" mode they're filled; in
  // "Selected-only" mode they're outline-only so selected shells stand out and
  // overlaps stay legible. Selected islands ALWAYS draw last (never hidden).
  for (const isl of unsel) uvedDrawIsland(isl, showAll);
  for (const isl of sel)   uvedDrawIsland(isl, true);
  // selection preview: selected faces not yet unwrapped, drawn as a dashed ghost
  if (UVED.preview) {
    const pv = UVED.preview;
    g.save();
    g.fillStyle = "rgba(59,130,246,.14)";
    g.strokeStyle = "rgba(120,170,255,.9)";
    g.lineWidth = 1; g.setLineDash([3,3]);
    for (const f of pv.faces) {
      const loop = TOPO.faceVerts[f];
      if (loop.some(v => !pv.uv[v])) continue;
      for (const [a,b,c] of TOPO.faceTris[f]) {
        const pa=pv.uv[a], pb=pv.uv[b], pc=pv.uv[c];
        g.beginPath(); g.moveTo(u2sx(pa[0]),v2sy(pa[1])); g.lineTo(u2sx(pb[0]),v2sy(pb[1])); g.lineTo(u2sx(pc[0]),v2sy(pc[1])); g.closePath(); g.fill();
      }
      g.beginPath();
      const p0=pv.uv[loop[0]]; g.moveTo(u2sx(p0[0]),v2sy(p0[1]));
      for (let i=1;i<loop.length;i++){ const p=pv.uv[loop[i]]; g.lineTo(u2sx(p[0]),v2sy(p[1])); }
      g.closePath(); g.stroke();
    }
    g.restore();
    // label so it reads as a not-yet-committed preview
    g.save(); g.setLineDash([]); g.fillStyle="rgba(160,190,255,.85)"; g.font="10px system-ui";
    const lb = islandUVBounds({ verts: pv.verts, uv: pv.uv });
    g.fillText("selection · press U to unwrap", u2sx(lb.mnU), v2sy(lb.mxV)-4);
    g.restore();
  }
  if (UVED.settings.showGizmo && sel.length) uvedGizmo(sel);
}
function islandUVBounds(isl) {
  let mnU=1e9,mnV=1e9,mxU=-1e9,mxV=-1e9;
  for (const v of isl.verts){ const p=isl.uv[v]; mnU=Math.min(mnU,p[0]);mnV=Math.min(mnV,p[1]);mxU=Math.max(mxU,p[0]);mxV=Math.max(mxV,p[1]); }
  return {mnU,mnV,mxU,mxV,cu:(mnU+mxU)/2,cv:(mnV+mxV)/2};
}
function uvedGizmo(sel) {
  let mnU=1e9,mnV=1e9,mxU=-1e9,mxV=-1e9;
  for (const isl of sel){ const b=islandUVBounds(isl); mnU=Math.min(mnU,b.mnU);mnV=Math.min(mnV,b.mnV);mxU=Math.max(mxU,b.mxU);mxV=Math.max(mxV,b.mxV); }
  const g=UVED.cx, x0=u2sx(mnU),y0=v2sy(mxV),x1=u2sx(mxU),y1=v2sy(mnV);
  const ccx=(x0+x1)/2, ccy=(y0+y1)/2, tool=UVED.tool;
  g.save(); g.strokeStyle="rgba(255,255,255,.4)"; g.setLineDash([4,4]); g.lineWidth=1; g.strokeRect(x0,y0,x1-x0,y1-y0); g.setLineDash([]);
  if (tool==="move"||tool==="select"){ uvArrow(ccx,ccy,ccx+48,ccy,"#ef4444"); uvArrow(ccx,ccy,ccx,ccy-48,"#22c55e"); g.fillStyle="#3b82f6"; g.fillRect(ccx-5,ccy-5,10,10); }
  if (tool==="rotate"){ g.strokeStyle="#a855f7"; g.lineWidth=2; g.beginPath(); g.arc(ccx,ccy,Math.max(40,(x1-x0)/2+18),0,7); g.stroke(); }
  if (tool==="scale"){ [[x0,y0],[x1,y0],[x0,y1],[x1,y1]].forEach(([px,py])=>{ g.fillStyle="#eab308"; g.fillRect(px-4,py-4,8,8); }); }
  g.restore();
}
function uvArrow(x1,y1,x2,y2,c){ const g=UVED.cx; g.strokeStyle=c;g.fillStyle=c;g.lineWidth=2.4; g.beginPath();g.moveTo(x1,y1);g.lineTo(x2,y2);g.stroke(); const a=Math.atan2(y2-y1,x2-x1); g.beginPath();g.moveTo(x2,y2);g.lineTo(x2-9*Math.cos(a-.4),y2-9*Math.sin(a-.4));g.lineTo(x2-9*Math.cos(a+.4),y2-9*Math.sin(a+.4));g.closePath();g.fill(); }
function hexA(hex,a){ hex=hex.replace("#",""); if(hex.length===3)hex=hex.split("").map(c=>c+c).join(""); const r=parseInt(hex.substr(0,2),16),g=parseInt(hex.substr(2,2),16),b=parseInt(hex.substr(4,2),16); return `rgba(${r},${g},${b},${a})`; }

/* ---- island hit test + transforms ---- */
function islandAt(u, v) {
  for (let i = UVED.islands.length-1; i>=0; i--) {
    const isl = UVED.islands[i];
    for (const f of isl.faces) {
      // point-in-polygon via the face's fan triangles
      for (const [a,b,c] of TOPO.faceTris[f]) {
        if (ptInTri(u,v, isl.uv[a], isl.uv[b], isl.uv[c])) return isl;
      }
    }
  }
  return null;
}
function ptInTri(u,v,A,B,C){ if(!A||!B||!C)return false; const d1=(u-B[0])*(A[1]-B[1])-(A[0]-B[0])*(v-B[1]); const d2=(u-C[0])*(B[1]-C[1])-(B[0]-C[0])*(v-C[1]); const d3=(u-A[0])*(C[1]-A[1])-(C[0]-A[0])*(v-A[1]); const neg=d1<0||d2<0||d3<0, pos=d1>0||d2>0||d3>0; return !(neg&&pos); }
function islandMove(isl,du,dv){ for(const v of isl.verts){ isl.uv[v][0]+=du; isl.uv[v][1]+=dv; } }
function islandScale(isl,f,cu,cv){ for(const v of isl.verts){ isl.uv[v][0]=cu+(isl.uv[v][0]-cu)*f; isl.uv[v][1]=cv+(isl.uv[v][1]-cv)*f; } }
function islandRotate(isl,da,cu,cv){ const co=Math.cos(da),si=Math.sin(da); for(const v of isl.verts){ const dx=isl.uv[v][0]-cu,dy=isl.uv[v][1]-cv; isl.uv[v][0]=cu+dx*co-dy*si; isl.uv[v][1]=cv+dx*si+dy*co; } }
function islandMirror(isl,axis,cu,cv){ for(const v of isl.verts){ if(axis==="h")isl.uv[v][0]=2*cu-isl.uv[v][0]; else isl.uv[v][1]=2*cv-isl.uv[v][1]; } }
function selCenter() { const sel=UVED.islands.filter(i=>i.selected); if(!sel.length)return{u:.5,v:.5}; let a=0,b=0,n=0; for(const isl of sel){const bd=islandUVBounds(isl);a+=bd.cu;b+=bd.cv;n++;} return {u:a/n,v:b/n}; }

/* ---- input ----
   Mouse coords are measured against the FOREGROUND CANVAS's own live rect (the
   element actually drawn into), not the wrapper. This keeps click-space and
   draw-space identical even if the wrapper has padding/borders or the canvas
   backing size drifted while a dock/split transition was still animating. If a
   drift is detected we resync the backing store on the spot. */
function uvedPointer(e) {
  const cv = UVED.canvas, r = cv.getBoundingClientRect();
  // the canvas fills the wrap (CSS 100%); if its displayed size no longer
  // matches the backing store we recorded, refresh so transforms stay valid.
  if (Math.abs(r.width - UVED.w) > 0.5 || Math.abs(r.height - UVED.h) > 0.5) uvedResize();
  return { mx: e.clientX - r.left, my: e.clientY - r.top };
}
let uvedDrag = null;
function uvedAttachInput() {
  const wrap = UVED.bg.parentElement;
  wrap.addEventListener("mousedown", e => {
    if (e.target.closest(".floaty")||e.target.closest(".bottom-bar")||e.target.closest(".vr-bar")||e.target.closest(".uv-tools")) return;
    // a modal transform swallows the next click: LMB confirms, RMB cancels
    if (uvedModalActive()) { e.preventDefault(); if (e.button===2) uvedModalCancel(); else uvedModalConfirm(); return; }
    const { mx, my } = uvedPointer(e);
    if (e.button===1 || APP.spaceDown) { uvedDrag={type:"pan",sx:mx,sy:my,vx:UVED.view.tx,vy:UVED.view.ty}; return; }
    if (e.button!==0) return;
    const u=sx2u(mx), v=sy2v(my), hit=islandAt(u,v), tool=UVED.tool;
    if (tool==="box") { uvedDrag={type:"box",sx:mx,sy:my}; document.getElementById("uvSelBox").style.display="block"; return; }
    if (hit) {
      if (!e.shiftKey && !hit.selected) UVED.islands.forEach(i=>i.selected=false);
      hit.selected = true;
      uvedSyncToMesh();
      const c = selCenter();
      uvedDrag = { type: (tool==="select"?"move":tool), sx:mx, sy:my, u, v, c,
        snap: UVED.islands.filter(i=>i.selected).map(i=>({i,uv:Object.fromEntries(i.verts.map(v=>[v,i.uv[v].slice()]))})) };
    } else {
      if (!e.shiftKey) { UVED.islands.forEach(i=>i.selected=false); uvedSyncToMesh(); }
      uvedDrag = { type:"box", sx:mx, sy:my }; document.getElementById("uvSelBox").style.display="block";
    }
    uvedRenderTree(); uvedDraw();
  });
  window.addEventListener("mousemove", e => {
    const { mx, my } = uvedPointer(e);
    UVED.hoverU=sx2u(mx); UVED.hoverV=sy2v(my); APP.updateStatus();
    if (uvedModalActive()) { uvedModalUpdate(); return; }
    if (!uvedDrag) return;
    if (uvedDrag.type==="pan"){ UVED.view.tx=uvedDrag.vx+(mx-uvedDrag.sx); UVED.view.ty=uvedDrag.vy+(my-uvedDrag.sy); uvedArm(); return; }
    if (uvedDrag.type==="box"){ const box=document.getElementById("uvSelBox"); const x=Math.min(mx,uvedDrag.sx),y=Math.min(my,uvedDrag.sy),w=Math.abs(mx-uvedDrag.sx),h=Math.abs(my-uvedDrag.sy); box.style.left=x+"px";box.style.top=y+"px";box.style.width=w+"px";box.style.height=h+"px"; return; }
    const du=sx2u(mx)-uvedDrag.u, dv=sy2v(my)-uvedDrag.v, c=uvedDrag.c;
    // restore snapshot then re-apply the transform so it composes cleanly
    for (const s of uvedDrag.snap) for (const v of Object.keys(s.uv)) UVED.islands.find(i=>i===s.i).uv[v]=s.uv[v].slice();
    const sel = uvedDrag.snap.map(s=>s.i);
    if (uvedDrag.type==="move"){ for(const isl of sel){ islandMove(isl,du,dv); } if(UVED.settings.snap)uvedSnapSel(); }
    if (uvedDrag.type==="scale"){ const dist=Math.hypot(mx-u2sx(c.u),my-v2sy(c.v)); const base=Math.hypot(uvedDrag.sx-u2sx(c.u),uvedDrag.sy-v2sy(c.v))||1; const f=Math.max(.05,dist/base); for(const isl of sel)islandScale(isl,f,c.u,c.v); }
    if (uvedDrag.type==="rotate"){ const a0=Math.atan2(uvedDrag.sy-v2sy(c.v),uvedDrag.sx-u2sx(c.u)); const a1=Math.atan2(my-v2sy(c.v),mx-u2sx(c.u)); for(const isl of sel)islandRotate(isl,-(a1-a0),c.u,c.v); }
    // scale/rotate change per-triangle area/shape, so refresh the stretch heatmap
    // live while it's the active channel (move is a rigid translate -> no change).
    if ((uvedDrag.type==="scale"||uvedDrag.type==="rotate") && stretchChannelLive()) uvedRefreshStretch();
    uvedDraw();
  });
  window.addEventListener("mouseup", e => {
    if (!uvedDrag) return;
    const { mx, my } = uvedPointer(e);
    const dragType = uvedDrag.type;
    if (dragType==="box"){
      const u0=sx2u(Math.min(mx,uvedDrag.sx)),u1=sx2u(Math.max(mx,uvedDrag.sx));
      const v0=sy2v(Math.max(my,uvedDrag.sy)),v1=sy2v(Math.min(my,uvedDrag.sy));
      if (!e.shiftKey) UVED.islands.forEach(i=>i.selected=false);
      for (const isl of UVED.islands){ const b=islandUVBounds(isl); if(b.cu>=u0&&b.cu<=u1&&b.cv>=v0&&b.cv<=v1)isl.selected=true; }
      document.getElementById("uvSelBox").style.display="none";
      uvedSyncToMesh(); uvedRenderTree();
    }
    // committing a drag-transform records a history step (box/pan are not edits)
    const moved = uvedDrag.snap && (mx!==uvedDrag.sx || my!==uvedDrag.sy);
    uvedDrag=null; uvedDraw();
    if ((dragType==="move"||dragType==="scale"||dragType==="rotate") && moved && typeof recordHistory==="function") {
      const label = { move:"Move island", scale:"Scale island", rotate:"Rotate island" }[dragType];
      recordHistory("transform", label);
    }
  });
  wrap.addEventListener("wheel", e => {
    e.preventDefault();
    const { mx, my } = uvedPointer(e);
    uvedZoomAt(mx,my, e.deltaY<0?1.1:0.9);
  }, {passive:false});
  // RMB cancels a modal transform without opening the browser menu
  wrap.addEventListener("contextmenu", e => { if (uvedModalActive()) e.preventDefault(); });
}
function uvedSnapSel(){ const d=UVED.settings.snapDiv; if(d<=0)return; for(const isl of UVED.islands.filter(i=>i.selected)){ const b=islandUVBounds(isl); const su=Math.round(b.cu*d)/d-b.cu, sv=Math.round(b.cv*d)/d-b.cv; islandMove(isl,su,sv); } }
function uvedZoomAt(mx,my,factor){ const vw=UVED.view; const u=sx2u(mx),v=sy2v(my); vw.tzoom=Math.max(.2,Math.min(8,vw.tzoom*factor)); const sp=uvGridPx()*vw.tzoom; vw.tx=mx-UVED.w/2-(u-0.5)*sp; vw.ty=my-UVED.h/2+(v-0.5)*sp; uvedArm(); }
function uvedFit(){ const vw=UVED.view; vw.tx=0;vw.ty=0;vw.tzoom=1; uvedArm(); }

/* ---- camera lag ---- */
function uvedArm(){ if(!UVED.running){UVED.running=true;requestAnimationFrame(uvedTick);} }
function uvedTick(){ const v=UVED.view; const editing=uvedDrag&&uvedDrag.type!=="pan"; if(editing){v.x=v.tx;v.y=v.ty;v.zoom=v.tzoom;uvedDraw();UVED.running=false;return;} const k=0.2; v.x+=(v.tx-v.x)*k;v.y+=(v.ty-v.y)*k;v.zoom+=(v.tzoom-v.zoom)*k; uvedDraw(); if(Math.abs(v.tx-v.x)<.01&&Math.abs(v.ty-v.y)<.01&&Math.abs(v.tzoom-v.zoom)<.01){v.x=v.tx;v.y=v.ty;v.zoom=v.tzoom;uvedDraw();UVED.running=false;return;} requestAnimationFrame(uvedTick); }

/* ---- island tree (left dock reused for islands) ---- */
function uvedRenderTree() {
  const tree = document.getElementById("islTree"); if(!tree) return;
  tree.innerHTML="";
  document.getElementById("islCount").textContent = UVED.islands.length;
  if (!UVED.islands.length){ tree.innerHTML='<div class="empty-state">No islands yet.<br>Pick faces in the 3D view and run an unwrap.</div>'; refreshIcons&&refreshIcons(); return; }
  for (const isl of UVED.islands) {
    const row=document.createElement("div"); row.className="row"+(isl.selected?" selected":"");
    const wBadge = (isl.weight && isl.weight!==1) ? `<span class="count" style="background:#3b2f10;color:#eab308">×${(+isl.weight).toFixed(isl.weight%1?1:0)}</span>` : "";
    row.innerHTML=`<span class="swatch" style="background:${isl.color}"></span><span class="label">${isl.name}</span>${wBadge}<span class="count">${isl.faces.length}f</span>`;
    row.onclick=(e)=>{ if(!e.shiftKey)UVED.islands.forEach(i=>i.selected=false); isl.selected=!e.shiftKey?true:!isl.selected; uvedSyncToMesh(); uvedRenderTree(); uvedDraw(); };
    tree.appendChild(row);
  }
  refreshIcons&&refreshIcons();
  if (typeof syncWeightPreset === "function") syncWeightPreset();  // keep weight views fresh on selection change
}

/* ---- sync: selecting an island highlights its faces in 3D ---- */
function uvedSyncToMesh() {
  const faces = new Set();
  for (const isl of UVED.islands) if (isl.selected) for (const f of isl.faces) faces.add(f);
  SEL.mode = "face"; SEL.verts.clear(); SEL.edges.clear(); SEL.faces = faces;
  APP.syncModeUI(); if (typeof vp3Draw==="function") vp3Draw();
  APP.updateStatus();
}
/* ---- sync the other way: mesh selection -> highlight matching islands ---- */
function uvedHighlightFromMesh() {
  const faces = selectedFaceSet();
  for (const isl of UVED.islands) isl.selected = isl.faces.some(f => faces.has(f));
  uvedBuildPreview(faces);
  uvedRenderTree(); uvedDraw();
}

/* ---- selection preview -------------------------------------------------
   Faces selected in the 3D view that are NOT yet part of any island still get
   shown in the UV pane as a ghost: we planar-project each connected group of
   such faces (facing its own average normal) into a small box so the user sees
   WHAT is selected and roughly where it maps, before committing an unwrap. The
   preview is display-only — it isn't editable and isn't packed into the atlas;
   running an unwrap replaces it with a real island. */
function uvedBuildPreview(faceSet) {
  const inIsland = new Set();
  for (const isl of UVED.islands) for (const f of isl.faces) inIsland.add(f);
  const pending = [...faceSet].filter(f => !inIsland.has(f));
  if (!pending.length) { UVED.preview = null; return; }
  // project the whole pending set against its average normal, then fit into a
  // dashed ghost box in the lower-left so it never collides with real islands.
  const n = _avgNormalLocal(pending);
  const up = Math.abs(n[1]) > 0.99 ? [0,0,1] : [0,1,0];
  const t = V3.normalize(V3.cross(up, n)), b = V3.cross(n, t), P = MESH.positions;
  const verts = new Set(); for (const f of pending) for (const v of TOPO.faceVerts[f]) verts.add(v);
  const uv = {}; let mnU=1e9,mnV=1e9,mxU=-1e9,mxV=-1e9;
  for (const v of verts) {
    const p = [P[v*3], P[v*3+1], P[v*3+2]];
    const cu = V3.dot(p, t), cv = V3.dot(p, b);
    uv[v] = [cu, cv];
    mnU=Math.min(mnU,cu); mnV=Math.min(mnV,cv); mxU=Math.max(mxU,cu); mxV=Math.max(mxV,cv);
  }
  const w = (mxU-mnU)||1e-6, h = (mxV-mnV)||1e-6, s = Math.max(w,h);
  const box = 0.28, ox = 0.02, oy = 0.02;   // ghost box footprint in the 0-1 square
  for (const v of verts) uv[v] = [ox + (uv[v][0]-mnU)/s*box, oy + (uv[v][1]-mnV)/s*box];
  UVED.preview = { faces: pending, verts: [...verts], uv };
}
/* avgNormal lives in unwrap.js; guard for headless/test load order */
function _avgNormalLocal(faces){ let n=[0,0,0]; for(const f of faces){const fn=TOPO.faceNormal[f];n[0]+=fn[0];n[1]+=fn[1];n[2]+=fn[2];} const l=Math.hypot(n[0],n[1],n[2])||1; return [n[0]/l,n[1]/l,n[2]/l]; }

function uvedSetTool(t){ UVED.tool=t; document.querySelectorAll(".pane-uv .tool[data-uvtool]").forEach(x=>x.classList.toggle("active",x.dataset.uvtool===t)); uvedDraw(); }
function uvedMirrorSel(axis){ const sel=UVED.islands.filter(i=>i.selected); if(!sel.length)return; const c=selCenter(); for(const isl of sel)islandMirror(isl,axis,c.u,c.v); uvedDraw(); if(typeof recordHistory==="function")recordHistory("mirror","Mirror "+(axis==="h"?"U":"V")); }

/* ==========================================================================
   Blender-style MODAL transform. Press G/R/S -> the selected islands follow
   the mouse live from the cursor's current position; X/Y lock the move axis;
   LMB or Enter confirms; Esc or RMB cancels (reverts to the pre-modal state).
   No drag needed -- mouse motion alone drives it, exactly like Blender.
   ========================================================================== */
const UVMODAL = { active: null };   // { type, snapshot, c, startU, startV, axis }
function uvedModalActive(){ return !!UVMODAL.active; }

function uvedBeginModal(type) {
  const sel = UVED.islands.filter(i => i.selected);
  if (!sel.length) { toast("Select an island first"); return; }
  UVMODAL.active = {
    type,
    c: selCenter(),
    startU: UVED.hoverU, startV: UVED.hoverV,
    axis: null,
    snapshot: sel.map(i => ({ i, uv: Object.fromEntries(i.verts.map(v => [v, i.uv[v].slice()])) })),
  };
  toast(({ move:"Move", rotate:"Rotate", scale:"Scale" })[type] + " — X/Y lock · LMB/Enter confirm · Esc cancel");
}
function uvedModalRestore() {
  for (const s of UVMODAL.active.snapshot) for (const v of Object.keys(s.uv)) s.i.uv[v] = s.uv[v].slice();
}
function uvedModalUpdate() {
  const m = UVMODAL.active; if (!m) return;
  uvedModalRestore();
  const sel = m.snapshot.map(s => s.i);
  const du = UVED.hoverU - m.startU, dv = UVED.hoverV - m.startV;
  if (m.type === "move") {
    const ax = m.axis;
    for (const isl of sel) islandMove(isl, ax==="y"?0:du, ax==="x"?0:dv);
    if (UVED.settings.snap) uvedSnapSel();
  } else if (m.type === "scale") {
    const base = Math.hypot(m.startU - m.c.u, m.startV - m.c.v) || 1e-4;
    const cur = Math.hypot(UVED.hoverU - m.c.u, UVED.hoverV - m.c.v);
    const f = Math.max(0.01, cur / base);
    for (const isl of sel) islandScale(isl, f, m.c.u, m.c.v);
  } else if (m.type === "rotate") {
    const a0 = Math.atan2(m.startV - m.c.v, m.startU - m.c.u);
    const a1 = Math.atan2(UVED.hoverV - m.c.v, UVED.hoverU - m.c.u);
    for (const isl of sel) islandRotate(isl, a1 - a0, m.c.u, m.c.v);
  }
  if ((m.type === "scale" || m.type === "rotate") && stretchChannelLive()) uvedRefreshStretch();
  uvedDraw();
}
function uvedModalSetAxis(axis) {
  if (!UVMODAL.active || UVMODAL.active.type !== "move") return;
  UVMODAL.active.axis = (UVMODAL.active.axis === axis) ? null : axis;
  uvedModalUpdate();
}
function uvedModalConfirm() { if (!UVMODAL.active) return; const type=UVMODAL.active.type; UVMODAL.active = null; uvedSyncToMesh(); uvedDraw(); if(typeof recordHistory==="function"){ const label={move:"Move island",scale:"Scale island",rotate:"Rotate island"}[type]; recordHistory("transform",label); } }
function uvedModalCancel() { if (!UVMODAL.active) return; uvedModalRestore(); UVMODAL.active = null; uvedDraw(); }

/* select the island(s) under the cursor's linked geometry (Blender "L") */
function uvedSelectLinked(additive) {
  const hit = islandAt(UVED.hoverU, UVED.hoverV);
  if (!hit) return;
  if (!additive) UVED.islands.forEach(i => i.selected = false);
  hit.selected = true;
  uvedSyncToMesh(); uvedRenderTree(); uvedDraw();
}
function uvedSelectAll(on) {
  UVED.islands.forEach(i => i.selected = on);
  uvedSyncToMesh(); uvedRenderTree(); uvedDraw();
}

/* ==========================================================================
   SHELL SIMILARITY + STACKING. A shell's signature is invariant to rigid
   motion and mirroring — face count, total UV area, boundary perimeter, and
   bbox aspect — so two islands that are the same shape (e.g. Suzanne's mirror-
   pair ears / eyes) match even when packed to different spots/orientations.
   "Select Similar" and "Same Perimeter" only SELECT; "Stack Selected" is the
   separate action that overlaps them onto the first selected island.
   ========================================================================== */
function islandSignature(isl) {
  let area = 0;
  // sum UV areas over each polygon's fan triangles
  for (const f of isl.faces) {
    for (const [a, b, c] of TOPO.faceTris[f]) {
      const pa = isl.uv[a], pb = isl.uv[b], pc = isl.uv[c];
      if (!pa || !pb || !pc) continue;
      area += Math.abs((pb[0]-pa[0])*(pc[1]-pa[1]) - (pc[0]-pa[0])*(pb[1]-pa[1])) * 0.5;
    }
  }
  // boundary perimeter: polygon LOOP edges used by exactly one face of this
  // island (loop edges only — no fan diagonals).
  const count = new Map();
  const key = (u, v) => u < v ? u+"_"+v : v+"_"+u;
  for (const f of isl.faces) {
    const loop = TOPO.faceVerts[f];
    for (let i = 0; i < loop.length; i++) {
      const k = key(loop[i], loop[(i+1)%loop.length]); count.set(k, (count.get(k)||0)+1);
    }
  }
  let perimeter = 0;
  for (const [k, n] of count) {
    if (n !== 1) continue;
    const [u, v] = k.split("_").map(Number);
    const pu = isl.uv[u], pv = isl.uv[v]; if (!pu || !pv) continue;
    perimeter += Math.hypot(pu[0]-pv[0], pu[1]-pv[1]);
  }
  const b = islandUVBounds(isl);
  const aspect = (b.mxU-b.mnU) / ((b.mxV-b.mnV) || 1e-6);
  return { faceCount: isl.faces.length, area, perimeter, aspect };
}
/* two signatures match within a relative tolerance (aspect compared to itself
   or its reciprocal so orientation/mirroring is ignored). */
function signaturesMatch(a, b, tol = 0.05) {
  if (a.faceCount !== b.faceCount) return false;
  const near = (x, y) => Math.abs(x - y) <= Math.max(x, y, 1e-6) * tol;
  const aspectOk = near(a.aspect, b.aspect) || near(a.aspect, 1/b.aspect);
  return near(a.area, b.area) && near(a.perimeter, b.perimeter) && aspectOk;
}
/* select every island matching any currently-selected reference island by full
   signature (shape + perimeter). Selection only — no movement. */
function uvedSelectSimilar() {
  const refs = UVED.islands.filter(i => i.selected).map(islandSignature);
  if (!refs.length) { toast("Select a reference shell first"); return; }
  let n = 0;
  for (const isl of UVED.islands) {
    if (isl.selected) { n++; continue; }
    const s = islandSignature(isl);
    if (refs.some(r => signaturesMatch(r, s))) { isl.selected = true; n++; }
  }
  uvedSyncToMesh(); uvedRenderTree(); uvedDraw();
  toast("Selected " + n + " similar shell" + (n===1?"":"s"));
}
/* match on boundary perimeter alone (looser — same outline length). */
function uvedSelectSamePerimeter() {
  const refs = UVED.islands.filter(i => i.selected).map(i => islandSignature(i).perimeter);
  if (!refs.length) { toast("Select a reference shell first"); return; }
  const near = (x, y) => Math.abs(x - y) <= Math.max(x, y, 1e-6) * 0.05;
  let n = 0;
  for (const isl of UVED.islands) {
    const p = islandSignature(isl).perimeter;
    if (isl.selected || refs.some(r => near(r, p))) { isl.selected = true; n++; }
  }
  uvedSyncToMesh(); uvedRenderTree(); uvedDraw();
  toast("Selected " + n + " same-perimeter shell" + (n===1?"":"s"));
}
/* overlap the selected shells: the FIRST selected island is the anchor; every
   other selected island is translated (and bbox-scaled) so it lands on top of
   the anchor. This is the explicit "stack identical shells" action. */
function uvedStackSelected() {
  const sel = UVED.islands.filter(i => i.selected);
  if (sel.length < 2) { toast("Select 2+ shells to stack"); return; }
  const anchor = sel[0], ab = islandUVBounds(anchor);
  const anchorW = (ab.mxU-ab.mnU)||1e-6, anchorH = (ab.mxV-ab.mnV)||1e-6;
  for (let i = 1; i < sel.length; i++) {
    const isl = sel[i], b = islandUVBounds(isl);
    const w = (b.mxU-b.mnU)||1e-6, h = (b.mxV-b.mnV)||1e-6;
    // uniform scale to match the anchor's bbox size, then centre onto it
    const f = Math.min(anchorW / w, anchorH / h);
    islandScale(isl, f, b.cu, b.cv);
    const nb = islandUVBounds(isl);
    islandMove(isl, ab.cu - nb.cu, ab.cv - nb.cv);
  }
  uvedDraw();
  toast("Stacked " + sel.length + " shells");
}
