"use strict";
/* ==========================================================================
   app.js -- top-level wiring for the split UV toolkit: builds topology, boots
   both viewports, wires the shared selection-mode pill, the selection-operation
   toolbar (grow/shrink/similar/island/perimeter/loop/invert), the unwrap menu,
   dock toggles + resizers, keyboard shortcuts and the shared status bar.
   ========================================================================== */
const APP = {
  spaceDown: false,
  mx: 0, my: 0,          // last cursor position (for the U context menu)
  propsWidth: 280,
  splitFrac: 0.5,        // fraction of the split width given to the 3D pane
  updateStatus() { updateStatusBar(); },
  syncModeUI() { syncSelModeUI(); },
};
function refreshIcons(){ if (window.lucide) lucide.createIcons(); }
function toast(txt){ const t=document.getElementById("vpToast"); if(!t)return; t.textContent=txt; t.classList.add("show"); clearTimeout(t._t); t._t=setTimeout(()=>t.classList.remove("show"),1200); }

window.addEventListener("DOMContentLoaded", () => {
  buildTopology(MESH);
  // selection change -> refresh 3D + mirror to 2D island highlight
  SEL.onChange = () => { if (typeof vp3Draw==="function") vp3Draw(); if (typeof uvedHighlightFromMesh==="function") uvedHighlightFromMesh(); updateStatusBar(); };

  vp3Init(document.getElementById("vp3Canvas"));
  uvedInit();

  wireSelModePill();
  wireSelectionOps();
  wireSymmetryToggle();
  wireUnwrapMenu();
  wireViewTools();
  wireMatcapPicker();
  wire2DTools();
  wireDocks();
  wireResizers();
  wireKeyboard();
  wireProps();
  wireHistoryUI();

  syncSelModeUI();
  // start with a face selection + a default unwrap so both panes show content
  SEL.mode = "face"; selClear();
  doUnwrap("conformal");
  vp3ResetView();
  updateStatusBar();
  // seed the timeline AFTER the default unwrap so step 0 restores a real layout
  seedHistoryUV();
  refreshIcons();

  window.addEventListener("resize", () => { vp3Resize(); uvedResize(); });
  // remember the last cursor position so the U menu can appear under it
  window.addEventListener("mousemove", e => { APP.mx = e.clientX; APP.my = e.clientY; }, true);
});

/* ---- selection-mode pill (vertex/edge/face/island) ---- */
function wireSelModePill() {
  document.querySelectorAll("#selModeBar .smbtn").forEach(b => b.onclick = () => setSelMode(b.dataset.mode));
}
function setSelMode(m) { SEL.mode = m; selClear(); syncSelModeUI(); if(typeof vp3Draw==="function")vp3Draw(); updateStatusBar(); }
function syncSelModeUI() { document.querySelectorAll("#selModeBar .smbtn[data-mode]").forEach(x => x.classList.toggle("active", x.dataset.mode===SEL.mode)); }

/* ---- symmetry / mirror-select toggle (bottom selection bar) ----
   Mirrors every selection op across the object's plane of symmetry. The map is
   built on first enable; an asymmetric mesh (match fraction below the SYM
   threshold) refuses to enable and reports why. */
function wireSymmetryToggle() {
  const btn = document.getElementById("opSymmetry");
  if (!btn) return;
  btn.onclick = () => toggleSymmetry();
}
function toggleSymmetry() {
  const btn = document.getElementById("opSymmetry");
  if (SYM.enabled) {
    symDisable();
    if (btn) btn.classList.remove("active");
    toast("Mirror select off");
  } else {
    const frac = symEnable();
    if (!SYM.enabled) {
      toast("Mesh isn't symmetric enough to mirror (" + Math.round(frac*100) + "% matched)");
      return;
    }
    if (btn) btn.classList.add("active");
    toast("Mirror select on · " + SYM.axisName + " axis (" + Math.round(frac*100) + "% matched)");
    if (typeof vp3Draw === "function") vp3Draw();
    updateStatusBar();
  }
}

/* ---- selection operations ---- */
function wireSelectionOps() {
  const ops = {
    opGrow: selGrow, opShrink: selShrink, opSimilar: selSimilar,
    opIsland: selIsland, opPerimeter: selPerimeter, opInvert: selInvert, opAll: selAll,
    opLoop: () => { if (SEL.mode==="edge" && SEL.edges.size) selLoopFromEdge([...SEL.edges][0]); else toast("Pick an edge first"); },
    opSeam: () => { const n=markSeamFromSelection(); toast(n?("Marked "+n+" seam edge"+(n===1?"":"s")+" ("+SEAMS.size+" total)"):"Nothing selected to mark"); if(n)recordHistory("seam","Mark "+n+" seam"+(n===1?"":"s")); },
    opClearSeam: () => { const had=SEAMS.size; clearSeams(); toast("Seams cleared"); if(had)recordHistory("seam","Clear seams"); },
    opNone: selClear,
  };
  for (const id of Object.keys(ops)) { const el=document.getElementById(id); if(el) el.onclick=()=>{ ops[id](); toast(el.dataset.tip||id); }; }
}

/* ---- unwrap menu ---- */
function wireUnwrapMenu() {
  // both the popup (2D pane header) and the properties-panel button grid
  document.querySelectorAll("[data-unwrap]").forEach(it => it.onclick = () => { doUnwrap(it.dataset.unwrap); closeAllPopups(); });
}
function doUnwrap(method) {
  // faces this unwrap will (re)flatten — a selection, or the whole mesh if none.
  // Merging against this set keeps islands the user already made for OTHER faces
  // instead of wiping the whole layout each time an unwrap runs.
  const target = new Set(targetFaces());
  const islands = unwrap(method);
  uvedMergeIslands(islands, target);
  uvedFit();
  const names = { unwrap:"Unwrap", smart:"Smart UV Project", planar:"Planar", box:"Box", cylindrical:"Cylindrical", spherical:"Spherical", conformal:"Conformal (auto-cut)" };
  toast((names[method]||method) + " → " + islands.length + " island" + (islands.length===1?"":"s"));
  updateStatusBar();
  // record once the timeline is seeded (the boot unwrap becomes step 0 via seed)
  recordHistory("unwrap", names[method]||method, islands.length + " island" + (islands.length===1?"":"s"));
}

/* record a history step iff the timeline exists yet (skips the pre-seed boot
   unwrap, whose result is captured as the seeded step 0). */
function recordHistory(type, title, subtitle) {
  if (typeof pushHistoryUV === "function" && HXU.events.length) pushHistoryUV(type, title, subtitle);
}

/* ---- 3D view tools ---- */
function wireViewTools() {
  document.getElementById("vp3Reset").onclick = () => vp3ResetView();
  document.getElementById("vp3ZoomIn").onclick = () => { VP3.tDist=Math.max(1.2,VP3.tDist*0.85); vp3Arm(); };
  document.getElementById("vp3ZoomOut").onclick = () => { VP3.tDist=Math.min(20,VP3.tDist*1.15); vp3Arm(); };
  const frameBtn = document.getElementById("vp3Frame");
  if (frameBtn) frameBtn.onclick = () => { vp3FrameSelection(); toast("Frame selection"); };
  // selection tools now live in a dropdown that pops UP from the bottom bar.
  // Picking one sets the tool, swaps the trigger's icon, marks the active row,
  // and closes the popup. (Replaces the old center-right marquee toolbar.)
  wireSelToolDropdown();
}
/* selection-tool dropdown (click/box/circle/lasso/paint) — opens upward */
const SEL_TOOL_ICON = { pick:"mouse-pointer-2", box:"square-dashed", circle:"circle-dashed", lasso:"lasso", paint:"brush" };
const SEL_TOOL_NAME = { pick:"Click select", box:"Box select", circle:"Circle / brush", lasso:"Lasso select", paint:"Paint faces" };
function setSelTool(tool) {
  vp3SetTool(tool);
  const pop = document.getElementById("selToolPopup");
  pop && pop.querySelectorAll("[data-vptool]").forEach(x => x.classList.toggle("active", x.dataset.vptool===tool));
  // swap the trigger's glyph by re-rendering its inner markup (lucide replaces
  // <i data-lucide> nodes on createIcons; mutating an already-rendered <svg>
  // won't re-process, so we rewrite the icon node + caret and re-run icons).
  const trig = document.getElementById("selToolTrigger");
  if (trig) {
    const name = SEL_TOOL_ICON[tool] || "mouse-pointer-2";
    trig.innerHTML = `<i data-lucide="${name}" class="lico" id="selToolIcon"></i><i data-lucide="chevron-up" class="lico sm-caret"></i>`;
    trig.dataset.tip = SEL_TOOL_NAME[tool] || "Select tool";
    refreshIcons();
  }
  toast(SEL_TOOL_NAME[tool] || tool);
}
function wireSelToolDropdown() {
  const trig = document.getElementById("selToolTrigger");
  const pop = document.getElementById("selToolPopup");
  if (!trig || !pop) return;
  pop.querySelectorAll("[data-vptool]").forEach(b => b.onclick = e => { e.stopPropagation(); setSelTool(b.dataset.vptool); closeSelToolPopup(); });
  pop.addEventListener("click", e => e.stopPropagation());
  trig.onclick = e => { e.stopPropagation(); toggleSelToolPopup(); };
  // default active row = the current tool ("pick")
  const cur = (VP3 && VP3.tool) || "pick";
  pop.querySelectorAll("[data-vptool]").forEach(x => x.classList.toggle("active", x.dataset.vptool===cur));
  document.addEventListener("click", closeSelToolPopup);
}
function closeSelToolPopup(){ const p=document.getElementById("selToolPopup"), t=document.getElementById("selToolTrigger"); if(p)p.classList.remove("open"); if(t)t.classList.remove("active"); }
function toggleSelToolPopup(){
  const pop=document.getElementById("selToolPopup"), trig=document.getElementById("selToolTrigger");
  if(!pop||!trig) return;
  const isOpen=pop.classList.contains("open");
  closeAllPopups(); closeSelToolPopup();
  if(isOpen) return;
  pop.classList.add("open"); trig.classList.add("active");
  // anchor ABOVE the trigger (this is a pop-UP), horizontally centered on it
  const r=trig.getBoundingClientRect();
  const pw=pop.offsetWidth||210, ph=pop.offsetHeight||220;
  let left=r.left + r.width/2 - pw/2;
  left=Math.max(8, Math.min(left, window.innerWidth-pw-12));
  pop.style.left=left+"px";
  pop.style.top=Math.max(8, r.top - ph - 8)+"px";
}
function wireMatcapPicker() {
  // matcap tiles live in the settings-grid popup (data-vpmc). Clicking one sets
  // the matcap and marks the active tile.
  const tiles = () => document.querySelectorAll("#vpDispPopup .mc-tile[data-vpmc]");
  tiles().forEach(tile => {
    const name = tile.dataset.vpmc;
    tile.onclick = () => { vp3SetMatcap(name); tiles().forEach(x=>x.classList.toggle("active",x===tile)); };
  });
  // tint each tile's chip from the matcap's centre pixel once matcaps are built
  requestAnimationFrame(() => {
    tiles().forEach(tile => {
      const mc = VP3.matcaps[tile.dataset.vpmc]; if(!mc)return;
      const c = mc.size, i=((c/2)*c+(c/2))*4;
      const chip = tile.querySelector(".mc-chip");
      if (chip) chip.style.background = `rgb(${mc.data[i]},${mc.data[i+1]},${mc.data[i+2]})`;
    });
  });
}

/* ---- 2D tools + popups ---- */
function wire2DTools() {
  document.querySelectorAll(".pane-uv .tool[data-uvtool]").forEach(b => b.onclick = () => uvedSetTool(b.dataset.uvtool));
  document.getElementById("uvFlipH").onclick = () => uvedMirrorSel("h");
  document.getElementById("uvFlipV").onclick = () => uvedMirrorSel("v");
  document.getElementById("uvSnap").onclick = function(){ UVED.settings.snap=!UVED.settings.snap; this.classList.toggle("active",UVED.settings.snap); };
  document.getElementById("uvFit").onclick = () => uvedFit();
  document.getElementById("uvZoomIn").onclick = () => uvedZoomAt(UVED.w/2,UVED.h/2,1.2);
  document.getElementById("uvZoomOut").onclick = () => uvedZoomAt(UVED.w/2,UVED.h/2,1/1.2);
  // popups (2D unwrap/display + 3D unwrap/display). The 3D "unwrap" button
  // opens the SAME unwrap popup as the U key.
  const popups = {
    unwrap:[document.getElementById("unwrapBtn"),document.getElementById("unwrapPopup")],
    disp:[document.getElementById("dispBtn"),document.getElementById("dispPopup")],
    vpDisp:[document.getElementById("vpDispBtn"),document.getElementById("vpDispPopup")],
    checker:[document.getElementById("checkerBtn"),document.getElementById("checkerPopup")],
    stretch:[document.getElementById("stretchBtn"),document.getElementById("stretchPopup")],
    elem:[document.getElementById("elemBtn"),document.getElementById("elemPopup")],
  };
  window.__popups = popups;
  // NOTE: the 3D-pane "UV mapping" button was removed from the top header — UV
  // mapping is reached via the U hotkey (openPopupAt) and the Unwrap property
  // section. The unwrap popup itself is still shared by both.
  Object.entries(popups).forEach(([k,[btn]]) => btn && (btn.onclick = e => { e.stopPropagation(); togglePopup(k); }));
  Object.values(popups).forEach(([,pop]) => pop && pop.addEventListener("click", e=>e.stopPropagation()));
  document.addEventListener("click", closeAllPopups);
  // display popup toggles
  document.querySelectorAll("#dispPopup [data-disp]").forEach(it => it.onclick = () => { const k=it.dataset.disp; UVED.settings[k]=!UVED.settings[k]; it.classList.toggle("checked",UVED.settings[k]); uvedDraw(); });
  // 3D viewport display toggle-tiles (wireframe, etc.)
  document.querySelectorAll("#vpDispPopup .toggle-tile[data-vpdisp]").forEach(it => it.onclick = () => { const k=it.dataset.vpdisp; VP3[k]=!VP3[k]; it.classList.toggle("checked",VP3[k]); vp3Draw(); });
  // ground-grid style segmented control (dots / lines / off)
  document.querySelectorAll("#vpGridSeg .seg-btn[data-grid]").forEach(b => b.onclick = () => {
    document.querySelectorAll("#vpGridSeg .seg-btn[data-grid]").forEach(x=>x.classList.toggle("active",x===b));
    VP3.gridMode = b.dataset.grid; vp3Draw();
  });
  document.querySelectorAll("#dispPopup .seg-btn[data-s]").forEach(b => b.onclick = () => { document.querySelectorAll("#dispPopup .seg-btn[data-s]").forEach(x=>x.classList.toggle("active",x===b)); UVED.settings.grid=b.dataset.s; uvedDraw(); });
  wireVizToolbar();
}

/* ---- visualization toolbar (backlog #1): display CHANNEL + checker + elements.
   The channel (solid | checker | stretch) is the base fill shared by 2D and 3D;
   checker resolution/mode/project-on-3D live in the Checker dropdown; element
   overlays (faces/verts/edges/wireframe) draw on top of whatever channel is on. */
function setVizChannel(ch) {
  VP3.channel = ch;
  // channels are mutually exclusive in the 2D editor: only one per-triangle fill
  // channel is active at a time (checker OR stretch OR the flat island tint).
  CHECKER.channel2d = (ch === "checker");
  if (typeof STRETCH !== "undefined") {
    STRETCH.channel2d = (ch === "stretch");
    // make sure the distortion table exists the moment stretch becomes visible
    if (ch === "stretch" && !STRETCH.triValue && typeof uvedRefreshStretch === "function") uvedRefreshStretch();
  }
  // keep the dropdown "show" ticks in sync with the channel
  syncCheckerUI();
  if (typeof syncStretchUI === "function") syncStretchUI();
  redrawAll();
}
function redrawAll() { if (typeof vp3Draw==="function") vp3Draw(); if (typeof uvedDraw==="function") uvedDraw(); }
function syncCheckerUI() {
  const tog = document.querySelector('#checkerPopup [data-checker="toggle"]');
  if (tog) tog.classList.toggle("checked", CHECKER.channel2d || VP3.channel==="checker");
  const p3 = document.querySelector('#checkerPopup [data-checker="project3d"]');
  if (p3) p3.classList.toggle("checked", CHECKER.enabled3d);
  document.querySelectorAll("#channelSeg .seg-btn[data-channel]").forEach(x => x.classList.toggle("active", x.dataset.channel===VP3.channel));
}
function syncStretchUI() {
  if (typeof STRETCH === "undefined") return;
  const tog = document.querySelector('#stretchPopup [data-stretch="toggle"]');
  if (tog) tog.classList.toggle("checked", STRETCH.channel2d || VP3.channel==="stretch");
  const p3 = document.querySelector('#stretchPopup [data-stretch="project3d"]');
  if (p3) p3.classList.toggle("checked", STRETCH.enabled3d);
  document.querySelectorAll("#stretchModeSeg .seg-btn[data-smode]").forEach(x => x.classList.toggle("active", x.dataset.smode===STRETCH.mode));
}
function wireVizToolbar() {
  // channel segmented control
  document.querySelectorAll("#channelSeg .seg-btn[data-channel]").forEach(b => b.onclick = () => {
    if (b.disabled) return;
    setVizChannel(b.dataset.channel);
  });
  // checker: show toggle == select the checker channel; project-on-3D gates the 3D raster
  document.querySelectorAll('#checkerPopup [data-checker]').forEach(it => it.onclick = () => {
    const k = it.dataset.checker;
    if (k === "toggle") {
      setVizChannel(VP3.channel === "checker" ? "solid" : "checker");
    } else if (k === "project3d") {
      CHECKER.enabled3d = !CHECKER.enabled3d;
      // projecting on 3D implies the checker channel is what the viewport shows
      VP3.channel = CHECKER.enabled3d ? "checker" : (CHECKER.channel2d ? "checker" : "solid");
      syncCheckerUI(); redrawAll();
    }
  });
  // checker resolution + mode segmented controls
  document.querySelectorAll("#checkerResSeg .seg-btn[data-ctiles]").forEach(b => b.onclick = () => {
    document.querySelectorAll("#checkerResSeg .seg-btn[data-ctiles]").forEach(x=>x.classList.toggle("active",x===b));
    CHECKER.tiles = +b.dataset.ctiles; redrawAll();
  });
  document.querySelectorAll("#checkerModeSeg .seg-btn[data-cmode]").forEach(b => b.onclick = () => {
    document.querySelectorAll("#checkerModeSeg .seg-btn[data-cmode]").forEach(x=>x.classList.toggle("active",x===b));
    CHECKER.mode = b.dataset.cmode; redrawAll();
  });
  // stretch: show toggle == select the stretch channel; project-on-3D gates the
  // 3D per-triangle heatmap. Mode switch (area/angle) recomputes the metric.
  document.querySelectorAll('#stretchPopup [data-stretch]').forEach(it => it.onclick = () => {
    const k = it.dataset.stretch;
    if (k === "toggle") {
      setVizChannel(VP3.channel === "stretch" ? "solid" : "stretch");
    } else if (k === "project3d") {
      STRETCH.enabled3d = !STRETCH.enabled3d;
      VP3.channel = STRETCH.enabled3d ? "stretch" : (STRETCH.channel2d ? "stretch" : "solid");
      if (STRETCH.enabled3d && !STRETCH.triValue && typeof uvedRefreshStretch==="function") uvedRefreshStretch();
      syncStretchUI(); redrawAll();
    }
  });
  document.querySelectorAll("#stretchModeSeg .seg-btn[data-smode]").forEach(b => b.onclick = () => {
    document.querySelectorAll("#stretchModeSeg .seg-btn[data-smode]").forEach(x=>x.classList.toggle("active",x===b));
    STRETCH.mode = b.dataset.smode;
    if (typeof uvedRefreshStretch==="function") uvedRefreshStretch();   // recompute for the new metric
    redrawAll();
  });
  // element overlays -> VP3 flags
  document.querySelectorAll('#elemPopup .toggle-tile[data-elem]').forEach(it => it.onclick = () => {
    const k = it.dataset.elem; VP3[k] = !VP3[k]; it.classList.toggle("checked", VP3[k]); if (typeof vp3Draw==="function") vp3Draw();
  });
  syncCheckerUI();
  syncStretchUI();
}
function togglePopup(key){ const [btn,pop]=window.__popups[key]; const open=pop.classList.contains("open"); closeAllPopups(); if(open)return; pop.classList.add("open"); btn.classList.add("active"); const r=btn.getBoundingClientRect(); const pw=pop.offsetWidth||230; pop.style.left=Math.max(8,Math.min(r.left,window.innerWidth-pw-12))+"px"; pop.style.top=(r.bottom+6)+"px"; }
/* open a popup at an arbitrary screen point (used by the Blender-style U menu,
   which appears under the cursor rather than anchored to a toolbar button). */
function openPopupAt(key, x, y){ const [btn,pop]=window.__popups[key]||[]; if(!pop)return; closeAllPopups(); pop.classList.add("open"); if(btn)btn.classList.add("active"); pop.style.left=Math.min(x,window.innerWidth-230)+"px"; pop.style.top=Math.min(y,window.innerHeight-320)+"px"; }
function closeAllPopups(){ if(window.__popups) Object.values(window.__popups).forEach(([btn,pop])=>{btn&&btn.classList.remove("active");pop&&pop.classList.remove("open");}); if(typeof closeSelToolPopup==="function")closeSelToolPopup(); }

/* ---- dock toggles ---- */
function wireDocks() {
  const dock=document.getElementById("leftDock"), props=document.getElementById("propsDock");
  const lBtn=document.getElementById("dockLeftBtn"), rBtn=document.getElementById("dockRightBtn");
  // the islands dock starts collapsed (see index.html), so left starts closed.
  let lo=false, ro=true;
  lBtn.innerHTML=`<i data-lucide="panel-left-open" class="lico"></i>`;
  lBtn.onclick=()=>{ lo=!lo; dock.classList.toggle("collapsed",!lo); lBtn.innerHTML=`<i data-lucide="${lo?'panel-left-close':'panel-left-open'}" class="lico"></i>`; refreshIcons(); setTimeout(()=>{vp3Resize();uvedResize();},300); };
  rBtn.onclick=()=>{ ro=!ro; props.classList.toggle("collapsed",!ro); document.getElementById("propsResizer").style.display=ro?"block":"none"; rBtn.innerHTML=`<i data-lucide="${ro?'panel-right-close':'panel-right-open'}" class="lico"></i>`; refreshIcons(); setTimeout(()=>{vp3Resize();uvedResize();},300); };
}

/* ---- resizers: split (3D|2D) + props ---- */
function wireResizers() {
  const split=document.getElementById("splitResizer"), pane3d=document.getElementById("pane3d"), paneUv=document.getElementById("paneUv");
  const splitHost=document.getElementById("split");
  let rz=false;
  split.addEventListener("mousedown",()=>{ rz=true; document.body.style.cursor="col-resize"; pane3d.style.transition="none"; });
  window.addEventListener("mousemove",e=>{ if(!rz)return; const r=splitHost.getBoundingClientRect(); let f=(e.clientX-r.left)/r.width; f=Math.max(.2,Math.min(.8,f)); pane3d.style.flex="none"; pane3d.style.width=(f*100)+"%"; paneUv.style.flex="1"; vp3Resize(); uvedResize(); });
  window.addEventListener("mouseup",()=>{ if(rz){rz=false;document.body.style.cursor="";pane3d.style.transition="";} });

  const pr=document.getElementById("propsResizer"), props=document.getElementById("propsDock");
  let prz=false;
  pr.addEventListener("mousedown",()=>{ prz=true; document.body.style.cursor="col-resize"; props.classList.add("resizing"); });
  window.addEventListener("mousemove",e=>{ if(!prz)return; let w=Math.max(230,Math.min(460,window.innerWidth-e.clientX)); props.style.width=w+"px"; vp3Resize(); uvedResize(); });
  window.addEventListener("mouseup",()=>{ if(prz){prz=false;document.body.style.cursor="";props.classList.remove("resizing");} });
}

/* ---- keyboard (Blender-style) ----
   A modal transform (G/R/S) captures keys while active: X/Y lock the axis,
   Enter confirms, Esc cancels. Outside a modal, the usual selection + tool
   shortcuts apply. Most selection ops (A, Alt+A, L, Ctrl+E, U) mirror Blender's
   UV/Image editor defaults. */
function wireKeyboard() {
  window.addEventListener("keydown", e => {
    if (e.code==="Space") APP.spaceDown=true;
    if (e.target.tagName==="INPUT") return;

    // --- while a modal transform is running, keys drive it ---
    if (uvedModalActive()) {
      if (e.code==="KeyX") { uvedModalSetAxis("x"); toast("X axis"); e.preventDefault(); return; }
      if (e.code==="KeyY") { uvedModalSetAxis("y"); toast("Y axis"); e.preventDefault(); return; }
      if (e.code==="Enter"||e.code==="NumpadEnter") { uvedModalConfirm(); e.preventDefault(); return; }
      if (e.code==="Escape") { uvedModalCancel(); toast("Cancelled"); e.preventDefault(); return; }
      // re-press G/R/S switches the modal kind
      if (e.code==="KeyG") { uvedBeginModal("move"); return; }
      if (e.code==="KeyR") { uvedBeginModal("rotate"); return; }
      if (e.code==="KeyS" && !e.ctrlKey) { uvedBeginModal("scale"); return; }
      return;   // swallow everything else mid-modal
    }

    // --- undo / redo (Ctrl+Z, Ctrl+Shift+Z or Ctrl+Y) ---
    if ((e.ctrlKey||e.metaKey) && e.code==="KeyZ" && !e.shiftKey) { hxuUndo(); e.preventDefault(); return; }
    if ((e.ctrlKey||e.metaKey) && (e.code==="KeyY" || (e.shiftKey && e.code==="KeyZ"))) { hxuRedo(); e.preventDefault(); return; }

    // --- selection mode 1/2/3/4 (Blender uses 1/2/3 for vert/edge/face) ---
    const sm={Digit1:"vertex",Digit2:"edge",Digit3:"face",Digit4:"island"};
    if (sm[e.code]) { setSelMode(sm[e.code]); return; }

    // --- 3D selection-tool hotkeys (mirror the pop-up dropdown) : W click,
    //     B box, Q circle/brush, K lasso, C paint. B & C match the user's ask;
    //     the rest fill in the remaining tools on free, unshadowed keys. ---
    const st={KeyW:"pick",KeyB:"box",KeyQ:"circle",KeyK:"lasso",KeyC:"paint"};
    if (st[e.code] && !e.ctrlKey && !e.altKey && !e.metaKey && !e.shiftKey) { setSelTool(st[e.code]); return; }

    // --- symmetry / mirror-select toggle (M) ---
    if (e.code==="KeyM" && !e.ctrlKey && !e.altKey && !e.metaKey && !e.shiftKey) { toggleSymmetry(); return; }

    // --- pin / unpin selected shells (P): a repack packs around pinned islands ---
    if (e.code==="KeyP" && !e.ctrlKey && !e.altKey && !e.metaKey && !e.shiftKey) { uvedTogglePin(); return; }

    // --- Blender modal transforms ---
    if (e.code==="KeyG") { uvedBeginModal("move"); return; }
    if (e.code==="KeyR") { uvedBeginModal("rotate"); return; }
    if (e.code==="KeyS" && !e.ctrlKey) { uvedBeginModal("scale"); return; }

    // --- unwrap context menu (U) : Blender opens a menu under the cursor;
    //     pick Unwrap / Smart UV Project / a projection from it. ---
    if (e.code==="KeyU") { openPopupAt("unwrap", APP.mx, APP.my); e.preventDefault(); return; }

    // --- select linked (Blender Ctrl+L): grow the current 3D selection to its
    //     connected component (welded/shared-vertex shell) -- head stays head,
    //     eyes stay eyes. Plain L selects the UV island under the cursor. ---
    if (e.code==="KeyL" && e.ctrlKey) {
      const n = selLinked();
      toast(n ? ("Select linked ("+n+" verts)") : "Nothing selected to link from");
      e.preventDefault(); return;
    }
    if (e.code==="KeyL") { uvedSelectLinked(e.shiftKey); toast("Select linked"); return; }

    // --- mark seam (Ctrl+E), clear seam (Alt+E) ---
    if (e.code==="KeyE" && e.ctrlKey) { const n=markSeamFromSelection(); toast(n?("Marked "+n+" seam"+(n===1?"":"s")+" ("+SEAMS.size+" total)"):"Nothing selected to mark"); if(n)recordHistory("seam","Mark "+n+" seam"+(n===1?"":"s")); e.preventDefault(); return; }
    if (e.code==="KeyE" && e.altKey) { const had=SEAMS.size; clearSeams(); toast("Seams cleared"); if(had)recordHistory("seam","Clear seams"); e.preventDefault(); return; }

    // --- select all (A or Ctrl+A) / deselect all (Alt+A) ---
    if (e.code==="KeyA" && e.altKey) { uvedSelectAll(false); selClear(); toast("Deselect all"); e.preventDefault(); return; }
    if (e.code==="KeyA") {
      if (UVED.islands.some(i=>i.selected)) uvedSelectAll(true); else { selAll(); }
      toast("Select all"); if (e.ctrlKey) e.preventDefault(); return;
    }

    // --- grow / shrink (Ctrl +/-, or +/-) ---
    if (e.code==="Equal"||e.code==="NumpadAdd") { selGrow(); toast("Grow"); return; }
    if (e.code==="Minus"||e.code==="NumpadSubtract") { selShrink(); toast("Shrink"); return; }
    if (e.code==="KeyI") { selInvert(); toast("Invert"); return; }

    // --- flip / mirror selection (Blender M then axis; here Shift+X / Shift+Y) ---
    if (e.code==="KeyX" && e.shiftKey) { uvedMirrorSel("h"); toast("Mirror X"); return; }
    if (e.code==="KeyY" && e.shiftKey) { uvedMirrorSel("v"); toast("Mirror Y"); return; }

    // --- view: Home reset 3D, F frame selection (3D) + fit (2D) ---
    if (e.key==="Home") { vp3ResetView(); return; }
    if (e.code==="KeyF") { vp3FrameSelection(); uvedFit(); toast("Frame selection"); return; }
  });
  window.addEventListener("keyup", e => { if(e.code==="Space") APP.spaceDown=false; });
}

/* ---- properties panel ---- */
function wireProps() {
  document.querySelectorAll(".prop-title").forEach(t => { const c=t.nextElementSibling; if(c&&c.classList.contains("prop-content"))c.style.maxHeight="800px"; t.onclick=()=>t.classList.toggle("collapsed"); });
  document.querySelectorAll("#snapMode .pill").forEach(p => p.onclick=()=>{ document.querySelectorAll("#snapMode .pill").forEach(x=>x.classList.remove("active")); p.classList.add("active"); UVED.settings.snapDiv=+p.dataset.snap; uvedDraw(); });
  const rc=document.getElementById("recolor");
  if (rc) UV_PALETTE.forEach(c=>{ const s=document.createElement("div"); s.className="sw"; s.style.background=c; s.onclick=()=>{ const sel=UVED.islands.filter(i=>i.selected); if(!sel.length)return; sel.forEach(i=>i.color=c); uvedRenderTree(); uvedDraw(); recordHistory("color","Recolor "+sel.length+" island"+(sel.length===1?"":"s")); }; rc.appendChild(s); });

  // shell tools: select similar / same perimeter / stack (select + stack are
  // deliberately separate actions)
  const bind = (id, fn) => { const el=document.getElementById(id); if(el) el.onclick=fn; };
  bind("btnSelSimilar", () => uvedSelectSimilar());
  bind("btnSelPerimeter", () => uvedSelectSamePerimeter());
  bind("btnStack", () => { uvedStackSelected(); recordHistory("stack","Stack selected shells"); });

  // packing algorithm selector + repack
  document.querySelectorAll("#packMode .pill").forEach(p => p.onclick=()=>{
    document.querySelectorAll("#packMode .pill").forEach(x=>x.classList.remove("active")); p.classList.add("active");
    setPackMethod(p.dataset.pack); uvedRepack(); toast("Pack: "+p.textContent);
    recordHistory("pack","Pack: "+p.textContent);
  });
  const repackBtn=document.getElementById("btnRepack");
  if (repackBtn) repackBtn.onclick=()=>{ uvedRepack(); toast("Repacked "+UVED.islands.length+" islands"); recordHistory("pack","Repack "+UVED.islands.length+" islands"); };

  // margin: px @ resolution -> fractional pack gap. Live-repack on change.
  const marginPx=document.getElementById("marginPx"), marginRes=document.getElementById("marginRes");
  const applyMargin=()=>{ setPackMargin(+marginPx.value||0, +marginRes.value||1024); uvedRepack(); };
  if (marginPx) marginPx.oninput=applyMargin;
  if (marginRes) marginRes.onchange=()=>{ applyMargin(); recordHistory("pack","Margin "+marginPx.value+"px @ "+marginRes.value); };

  // pin / unpin selected islands: a repack packs AROUND pinned shells.
  bind("btnPin", ()=>{ uvedSetPinned(true); toast("Pinned selected shells"); recordHistory("pin","Pin selected shells"); });
  bind("btnUnpin", ()=>{ uvedSetPinned(false); toast("Unpinned selected shells"); recordHistory("pin","Unpin selected shells"); });

  // UDIM: active tile field drives repack target; assign moves selection to it.
  const udimActive=document.getElementById("udimActive");
  if (udimActive) udimActive.onchange=()=>{ const t=udimToTile(+udimActive.value||1001); UDIM.activeTile={tu:t.tu,tv:t.tv}; uvedDraw(); toast("Active tile "+(+udimActive.value)); };
  bind("btnAssignTile", ()=>{ const t=udimToTile(+((udimActive&&udimActive.value)||1001)); uvedAssignToTile(t.tu,t.tv); toast("Assigned to tile "+tileToUdim(t.tu,t.tv)); recordHistory("udim","Assign to tile "+tileToUdim(t.tu,t.tv)); });

  // island importance (weight) controls
  const wUp=document.getElementById("wUp"), wDown=document.getElementById("wDown");
  if (wUp) wUp.onclick=()=>{ uvedNudgeWeight(2); syncWeightPreset(); toast("More space"); recordHistory("weight","More atlas space"); };
  if (wDown) wDown.onclick=()=>{ uvedNudgeWeight(0.5); syncWeightPreset(); toast("Less space"); recordHistory("weight","Less atlas space"); };
  document.querySelectorAll("#weightPreset .pill").forEach(p => p.onclick=()=>{
    uvedSetWeight(+p.dataset.weight); syncWeightPreset();
    toast("Weight ×"+p.dataset.weight); recordHistory("weight","Weight ×"+p.dataset.weight);
  });

  // fine weight controls -- the slider + % field are a direct 0..100% atlas-
  // SHARE view (100% = this island fills the box, others shrink toward nothing);
  // the ×N field is the raw-multiplier escape hatch. All three write isl.weight.
  const wSlider=document.getElementById("weightSlider"),
        wMult=document.getElementById("weightMult"),
        wPct=document.getElementById("weightPct");
  const SLIDER_STEPS=1000;                 // slider 0..1000 == 0.0..100.0 %
  const PCT_MAX=99;                         // 100% needs infinite weight -> cap at 99
  // apply an atlas-share (0..1) to the active island(s) via percentToWeight
  const applyShare=(share)=>{
    const sel=UVED.islands.filter(i=>i.selected);
    const target=sel.length?sel[0]:UVED.islands[0];
    if (!target) return null;
    const s=Math.max(0, Math.min(PCT_MAX/100, share));
    uvedSetWeight(percentToWeight(s, UVED.islands, target));
    syncWeightPreset();
    return s;
  };
  if (wSlider) wSlider.oninput=()=>{ applyShare(+wSlider.value/SLIDER_STEPS); };
  if (wSlider) wSlider.onchange=()=>{
    const s=applyShare(+wSlider.value/SLIDER_STEPS);
    if (s!=null) { toast("Atlas share "+Math.round(s*100)+"%"); recordHistory("weight","Atlas share "+Math.round(s*100)+"%"); }
  };
  if (wPct) wPct.onchange=()=>{
    const s=applyShare((+wPct.value||0)/100);
    if (s!=null) { toast("Atlas share "+Math.round(s*100)+"%"); recordHistory("weight","Atlas share "+Math.round(s*100)+"%"); }
  };
  if (wMult) wMult.onchange=()=>{
    const w=Math.max(0.1, +wMult.value||1);
    uvedSetWeight(w); syncWeightPreset();
    toast("Weight ×"+w.toFixed(2)); recordHistory("weight","Weight ×"+w.toFixed(2));
  };
}

/* ---- right-dock tab strip (Properties ⇄ History) + header undo/redo ---- */
function wireHistoryUI() {
  const panes = { props: document.getElementById("propsPane"), history: document.getElementById("hxuPane") };
  document.querySelectorAll(".rd-tab").forEach(tab => tab.onclick = () => {
    const which = tab.dataset.rdpane;
    document.querySelectorAll(".rd-tab").forEach(x => x.classList.toggle("active", x===tab));
    if (panes.props) panes.props.style.display = which==="props" ? "" : "none";
    if (panes.history) panes.history.style.display = which==="history" ? "flex" : "none";
    if (which==="history") renderHistoryUV();
  });
  document.querySelectorAll("[data-hist]").forEach(b => b.onclick = () => { b.dataset.hist==="undo" ? hxuUndo() : hxuRedo(); });
}
/* reflect the selected islands' weight back onto ALL four views (pills,
   slider, xN field, % field) so the three fine controls stay in sync -- they
   are just interchangeable views of the one stored isl.weight (backlog #9). */
function syncWeightPreset() {
  const sel = UVED.islands.filter(i=>i.selected);
  const target = sel.length ? sel[0] : UVED.islands[0];
  const w = target ? (target.weight||1) : 1;
  document.querySelectorAll("#weightPreset .pill").forEach(p=>p.classList.toggle("active", +p.dataset.weight===Math.round(w)));
  const wSlider=document.getElementById("weightSlider"),
        wMult=document.getElementById("weightMult"),
        wPct=document.getElementById("weightPct");
  const share = target ? weightToPercent(UVED.islands, target) : 0;   // 0..1 atlas share
  if (wSlider && document.activeElement!==wSlider) wSlider.value = Math.round(share*1000);
  if (wMult && document.activeElement!==wMult) wMult.value = +w.toFixed(2);
  if (wPct && document.activeElement!==wPct) wPct.value = Math.round(share*100);
}

/* ---- status bar ---- */
function updateStatusBar() {
  const set = (id,val)=>{ const e=document.getElementById(id); if(e)e.textContent=val; };
  let count = SEL.mode==="vertex"?SEL.verts.size : SEL.mode==="edge"?SEL.edges.size : SEL.faces.size;
  set("stMode", SEL.mode[0].toUpperCase()+SEL.mode.slice(1));
  set("stSel", count);
  set("stIslands", UVED.islands.length);
  set("stSeams", SEAMS.size);
  set("stU", (UVED.hoverU||0).toFixed(3));
  set("stV", (UVED.hoverV||0).toFixed(3));
  set("stFaces", TOPO.faceCount);
}
