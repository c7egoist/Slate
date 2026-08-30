"use strict";
/* ==========================================================================
   history.js -- snapshot-based linear undo / redo for the UV toolkit, with a
   timeline pane modelled on the CAD prototype's History panel (rail + event
   rows + jump-to-cursor) minus branching.

     • A snapshot captures everything an undo must restore: island UV layouts,
       the seam set, and the current selection.
     • pushHistoryUV(type,title,subtitle) records a snapshot AFTER an op ran,
       discarding any redo tail (linear history).
     • Clicking an event jumps the cursor to it and restores its snapshot;
       events past the cursor render as a greyed-out future until overwritten.
   ========================================================================== */

/* event type -> { icon (lucide), label, accent-class }. Mirrors HX_TYPES from
   the CAD panel but keyed to UV-editor operations. */
const HXU_TYPES = {
  start:     { icon: "flag",              label: "Start" },
  unwrap:    { icon: "unfold-horizontal", label: "Unwrap",    cls: "gen" },
  transform: { icon: "move",              label: "Transform", cls: "param" },
  mirror:    { icon: "flip-horizontal-2", label: "Mirror",    cls: "param" },
  pack:      { icon: "layout-grid",       label: "Pack",      cls: "mat" },
  stack:     { icon: "layers",            label: "Stack",     cls: "mat" },
  weight:    { icon: "scaling",           label: "Weight",    cls: "param" },
  color:     { icon: "palette",           label: "Recolor",   cls: "gen" },
  seam:      { icon: "scissors",          label: "Seam",      cls: "gen" },
  select:    { icon: "box-select",        label: "Select",    cls: "gen" },
};

/* single linear store: an ordered list of events + a cursor ("you are here"). */
let _hxuSeq = 0;
const HXU = { events: [], cursor: -1 };

/* ─── SNAPSHOT ─── capture / restore the full UV-editing state ─── */
function hxuCaptureSnapshot() {
  return {
    islands: (typeof UVED !== "undefined" ? UVED.islands : []).map(isl => ({
      id: isl.id, name: isl.name, color: isl.color,
      faces: isl.faces.slice(), verts: isl.verts.slice(),
      uv: Object.fromEntries(isl.verts.map(v => [v, isl.uv[v].slice()])),
      weight: isl.weight || 1, selected: !!isl.selected,
      pinned: !!isl.pinned, tile: isl.tile ? { tu: isl.tile.tu | 0, tv: isl.tile.tv | 0 } : { tu: 0, tv: 0 },
    })),
    seams: (typeof SEAMS !== "undefined") ? [...SEAMS] : [],
    sel: (typeof SEL !== "undefined") ? {
      mode: SEL.mode,
      verts: [...SEL.verts], edges: [...SEL.edges], faces: [...SEL.faces],
    } : null,
  };
}
function hxuRestoreSnapshot(snap) {
  if (!snap) return;
  // islands (rebuild fresh objects so later edits don't alias the snapshot)
  UVED.islands = snap.islands.map(isl => ({
    id: isl.id, name: isl.name, color: isl.color,
    faces: isl.faces.slice(), verts: isl.verts.slice(),
    uv: Object.fromEntries(isl.verts.map(v => [v, isl.uv[v].slice()])),
    weight: isl.weight || 1, selected: !!isl.selected,
    pinned: !!isl.pinned, tile: isl.tile ? { tu: isl.tile.tu | 0, tv: isl.tile.tv | 0 } : { tu: 0, tv: 0 },
  }));
  // seams
  SEAMS.clear();
  for (const k of snap.seams) SEAMS.add(k);
  // selection
  if (snap.sel) {
    SEL.mode = snap.sel.mode;
    SEL.verts = new Set(snap.sel.verts);
    SEL.edges = new Set(snap.sel.edges);
    SEL.faces = new Set(snap.sel.faces);
    SEL.lastPick = null;
  }
  // repaint every dependent view
  if (typeof uvedRenderTree === "function") uvedRenderTree();
  if (typeof uvedDraw === "function") uvedDraw();
  if (typeof vp3Draw === "function") vp3Draw();
  if (typeof syncSelModeUI === "function") syncSelModeUI();
  if (typeof updateStatusBar === "function") updateStatusBar();
}

/* ─── RECORD ─── snapshot AFTER an op; drop any redo tail (linear) ─── */
function pushHistoryUV(type, title, subtitle) {
  const ev = {
    id: "h" + (++_hxuSeq), at: new Date(),
    type, title, subtitle,
    snapshot: hxuCaptureSnapshot(),
  };
  // overwrite the future: anything past the cursor is discarded
  if (HXU.cursor < HXU.events.length - 1) HXU.events.length = HXU.cursor + 1;
  HXU.events.push(ev);
  HXU.cursor = HXU.events.length - 1;
  hxuSyncButtons();
  if (hxuVisible()) renderHistoryUV();
}

/* ─── SEED ─── one "Session started" event so the rail isn't blank ─── */
function seedHistoryUV() {
  _hxuSeq = 0;
  HXU.events = [{
    id: "h" + (++_hxuSeq), at: new Date(),
    type: "start", title: "Session started", subtitle: "Default conformal unwrap",
    snapshot: hxuCaptureSnapshot(),
  }];
  HXU.cursor = 0;
  hxuSyncButtons();
  if (hxuVisible()) renderHistoryUV();
}

/* is the History pane the one currently shown in the right dock? */
function hxuVisible() {
  const pane = document.getElementById("hxuPane");
  return pane && pane.style.display !== "none";
}

/* ─── JUMP / UNDO / REDO ─── */
function hxuJump(idx) {
  HXU.cursor = Math.max(0, Math.min(HXU.events.length - 1, idx));
  hxuRestoreSnapshot(HXU.events[HXU.cursor].snapshot);
  renderHistoryUV();
  const ev = HXU.events[HXU.cursor];
  toast("Jumped to “" + (ev.title || (HXU_TYPES[ev.type] || {}).label || "step") + "”");
}
function hxuUndo() {
  if (HXU.cursor > 0) { HXU.cursor--; hxuRestoreSnapshot(HXU.events[HXU.cursor].snapshot); renderHistoryUV(); toast("Undo"); }
  else toast("Nothing to undo");
}
function hxuRedo() {
  if (HXU.cursor < HXU.events.length - 1) { HXU.cursor++; hxuRestoreSnapshot(HXU.events[HXU.cursor].snapshot); renderHistoryUV(); toast("Redo"); }
  else toast("Nothing to redo");
}

/* ─── RENDER ─── the rail timeline (event rows with a connecting rail) ─── */
function hxuTime(d) { return d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" }); }
function hxuSyncButtons() {
  const u = document.querySelector('[data-hist="undo"]'), r = document.querySelector('[data-hist="redo"]');
  if (u) u.classList.toggle("disabled", HXU.cursor <= 0);
  if (r) r.classList.toggle("disabled", HXU.cursor >= HXU.events.length - 1);
}
function renderHistoryUV() {
  hxuSyncButtons();
  const scroll = document.getElementById("hxuScroll");
  if (!scroll) return;
  scroll.innerHTML = "";
  const events = HXU.events;
  if (!events.length) {
    scroll.innerHTML = '<div class="hxu-empty">No history yet.<br>Unwrap, transform, pack or mark seams to record steps.</div>';
    return;
  }
  events.forEach((ev, i) => {
    const t = HXU_TYPES[ev.type] || HXU_TYPES.transform;
    const isFirst = i === 0, isLast = i === events.length - 1;
    const future = i > HXU.cursor;
    const atCursor = i === HXU.cursor;

    const row = document.createElement("div");
    row.className = "hxu-ev" + (future ? " future" : "") + (atCursor ? " at-cursor" : "");

    // rail: top line · node · bottom line
    const rail = document.createElement("div"); rail.className = "hxu-rail";
    const top = document.createElement("div"); top.className = "hxu-line top" + (isFirst ? " hidden" : "");
    const node = document.createElement("div"); node.className = "hxu-node";
    node.innerHTML = `<i data-lucide="${t.icon}" class="lico"></i>`;
    const bot = document.createElement("div"); bot.className = "hxu-line" + (isLast ? " hidden" : "");
    rail.appendChild(top); rail.appendChild(node); rail.appendChild(bot);

    // body: title · type chip + subtitle · time
    const body = document.createElement("div"); body.className = "hxu-body";
    const main = document.createElement("div"); main.className = "hxu-main";
    main.innerHTML = `<div class="hxu-evt-title">${ev.title || t.label}</div>
      <div class="hxu-evt-meta">
        <span class="hxu-type ${t.cls || ""}"><i data-lucide="${t.icon}" class="lico"></i>${t.label}</span>
        ${ev.subtitle ? `<span class="hxu-sub">${ev.subtitle}</span>` : ""}
      </div>`;
    const time = document.createElement("div"); time.className = "hxu-time"; time.textContent = hxuTime(ev.at);
    body.appendChild(main); body.appendChild(time);

    row.appendChild(rail); row.appendChild(body);
    row.title = "Jump to this step";
    row.onclick = () => hxuJump(i);
    scroll.appendChild(row);
  });
  refreshIcons();
  const cur = scroll.querySelector(".hxu-ev.at-cursor");
  if (cur) cur.scrollIntoView({ block: "nearest" });
}

if (typeof module !== "undefined") {
  module.exports = {
    HXU, HXU_TYPES, pushHistoryUV, seedHistoryUV,
    hxuCaptureSnapshot, hxuRestoreSnapshot,
    hxuJump, hxuUndo, hxuRedo, renderHistoryUV, hxuVisible, hxuSyncButtons,
  };
}
