"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   icons.js — the whole prototype's icon set as self-contained coloured inline
              SVG (no network / Lucide dependency). Three groups:

                • CAD_ICONS / CAD_ENV_ICONS — the vibrant multi-tone tool and
                  environment glyphs (ported from Studio's cad-toolbar.js, then
                  extended with the dimension + constraint sets).
                • WS_GLYPH_CAD — the single-silhouette CAD workspace glyph.
                • UI_ICONS — a coloured line-icon for every data-ic="…" name the
                  markup and the render code reference.

              hydrateIcons(root) walks the DOM and injects the SVG for each
              [data-ic] span; refreshIcons() (controls.js) delegates to it so
              every render path stays a one-liner.
   ════════════════════════════════════════════════════════════════════════════ */

/* ─── COLOURED TOOL ICON SET (sketch curves, solid features, modify, boolean) ─── */
const CAD_ICONS={
  line:`<svg viewBox="0 0 24 24" fill="none"><path d="M4 20L20 4" stroke="#38bdf8" stroke-width="2" stroke-linecap="round"/><circle cx="4" cy="20" r="2.3" fill="#0ea5e9"/><circle cx="20" cy="4" r="2.3" fill="#0ea5e9"/></svg>`,
  rect:`<svg viewBox="0 0 24 24" fill="none"><rect x="4" y="6" width="16" height="12" rx="1" fill="#3b82f6" fill-opacity=".18" stroke="#3b82f6" stroke-width="1.7"/><circle cx="4" cy="6" r="1.8" fill="#60a5fa"/><circle cx="20" cy="18" r="1.8" fill="#60a5fa"/></svg>`,
  circle2d:`<svg viewBox="0 0 24 24" fill="none"><circle cx="12" cy="12" r="8.3" fill="#22d3ee" fill-opacity=".16" stroke="#22d3ee" stroke-width="1.7"/><circle cx="12" cy="12" r="1.9" fill="#67e8f9"/><path d="M12 12L20.3 12" stroke="#67e8f9" stroke-width="1.3" stroke-dasharray="2 2"/></svg>`,
  polygon:`<svg viewBox="0 0 24 24" fill="none"><path d="M12 3l7.8 5.7-3 9.2H7.2l-3-9.2z" fill="#f472b6" fill-opacity=".18" stroke="#f472b6" stroke-width="1.7" stroke-linejoin="round"/><circle cx="12" cy="12" r="1.7" fill="#f9a8d4"/><path d="M12 12L12 3" stroke="#f9a8d4" stroke-width="1.2" stroke-dasharray="2 2"/></svg>`,
  square:`<svg viewBox="0 0 24 24" fill="none"><rect x="5" y="5" width="14" height="14" rx="1" fill="#3b82f6" fill-opacity=".18" stroke="#3b82f6" stroke-width="1.7"/><circle cx="5" cy="5" r="1.8" fill="#60a5fa"/><circle cx="19" cy="19" r="1.8" fill="#60a5fa"/></svg>`,
  arc:`<svg viewBox="0 0 24 24" fill="none"><path d="M4 19A11 11 0 0 1 20 8" stroke="#818cf8" stroke-width="2" stroke-linecap="round" fill="none"/><circle cx="4" cy="19" r="2.1" fill="#6366f1"/><circle cx="20" cy="8" r="2.1" fill="#6366f1"/></svg>`,
  spline:`<svg viewBox="0 0 24 24" fill="none"><path d="M3 17c4 0 4-10 8-10s4 10 10 4" stroke="#a855f7" stroke-width="2" stroke-linecap="round" fill="none"/><circle cx="3" cy="17" r="1.9" fill="#c084fc"/><circle cx="11" cy="7" r="1.9" fill="#c084fc"/><circle cx="21" cy="11" r="1.9" fill="#c084fc"/></svg>`,
  bezier:`<svg viewBox="0 0 24 24" fill="none"><path d="M4 18C8 18 8 6 12 6s4 12 8 12" stroke="#a855f7" stroke-width="2" stroke-linecap="round" fill="none"/><path d="M4 18l4-6M20 18l-4-6" stroke="#c084fc" stroke-width="1.1" opacity=".7"/><circle cx="4" cy="18" r="1.9" fill="#a855f7"/><circle cx="20" cy="18" r="1.9" fill="#a855f7"/><circle cx="8" cy="12" r="1.5" fill="#0b0d11" stroke="#c084fc" stroke-width="1.2"/><circle cx="16" cy="12" r="1.5" fill="#0b0d11" stroke="#c084fc" stroke-width="1.2"/></svg>`,
  bspline:`<svg viewBox="0 0 24 24" fill="none"><path d="M3 18c3 0 5-12 9-12s6 12 9 12" stroke="#3b82f6" stroke-width="2" stroke-linecap="round" fill="none"/><path d="M3 18l4-8 5-2 5 2 4 8" stroke="#60a5fa" stroke-width="1" opacity=".55" fill="none"/><circle cx="7" cy="10" r="1.7" fill="#60a5fa"/><circle cx="12" cy="8" r="1.7" fill="#60a5fa"/><circle cx="17" cy="10" r="1.7" fill="#60a5fa"/></svg>`,
  nurbs:`<svg viewBox="0 0 24 24" fill="none"><path d="M3 18c3 0 5-12 9-12s6 12 9 12" stroke="#14b8a6" stroke-width="2" stroke-linecap="round" fill="none"/><circle cx="7" cy="10" r="2.1" fill="#2dd4bf"/><circle cx="12" cy="7" r="1.5" fill="#2dd4bf" fill-opacity=".6"/><circle cx="17" cy="10" r="2.4" fill="#2dd4bf"/></svg>`,
  dimension:`<svg viewBox="0 0 24 24" fill="none"><path d="M4 6v12M20 6v12M4 12h16" stroke="#f59e0b" stroke-width="1.7" stroke-linecap="round"/><path d="M4 12l3-2.2v4.4L4 12zM20 12l-3-2.2v4.4L20 12z" fill="#fbbf24"/></svg>`,
  dimradius:`<svg viewBox="0 0 24 24" fill="none"><circle cx="11" cy="13" r="8" fill="none" stroke="#f59e0b" stroke-width="1.5" opacity=".55"/><path d="M11 13L20 5" stroke="#fbbf24" stroke-width="1.7" stroke-linecap="round"/><circle cx="11" cy="13" r="1.7" fill="#fbbf24"/><path d="M20 5l-3 .4 1.6 2.6z" fill="#fbbf24"/></svg>`,
  dimdiameter:`<svg viewBox="0 0 24 24" fill="none"><circle cx="12" cy="12" r="8" fill="none" stroke="#f59e0b" stroke-width="1.5" opacity=".55"/><path d="M5 19L19 5" stroke="#fbbf24" stroke-width="1.7" stroke-linecap="round"/><path d="M5 19l.3-3 2.6 1.6zM19 5l-.3 3-2.6-1.6z" fill="#fbbf24"/></svg>`,
  dimangle:`<svg viewBox="0 0 24 24" fill="none"><path d="M4 20h16M4 20L18 6" stroke="#fbbf24" stroke-width="1.7" stroke-linecap="round"/><path d="M4 20a12 12 0 0 1 8-4" fill="none" stroke="#f59e0b" stroke-width="1.5"/></svg>`,
  coincident:`<svg viewBox="0 0 24 24" fill="none"><circle cx="12" cy="12" r="6" fill="#34d399" fill-opacity=".22" stroke="#34d399" stroke-width="1.6"/><circle cx="12" cy="12" r="2" fill="#6ee7b7"/></svg>`,
  horizontal:`<svg viewBox="0 0 24 24" fill="none"><path d="M4 12h16" stroke="#34d399" stroke-width="2.2" stroke-linecap="round"/><circle cx="4" cy="12" r="2.1" fill="#6ee7b7"/><circle cx="20" cy="12" r="2.1" fill="#6ee7b7"/></svg>`,
  vertical:`<svg viewBox="0 0 24 24" fill="none"><path d="M12 4v16" stroke="#34d399" stroke-width="2.2" stroke-linecap="round"/><circle cx="12" cy="4" r="2.1" fill="#6ee7b7"/><circle cx="12" cy="20" r="2.1" fill="#6ee7b7"/></svg>`,
  parallel:`<svg viewBox="0 0 24 24" fill="none"><path d="M6 20L12 4M13 20L19 4" stroke="#34d399" stroke-width="2" stroke-linecap="round"/></svg>`,
  perpendicular:`<svg viewBox="0 0 24 24" fill="none"><path d="M6 4v16h14" stroke="#34d399" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" fill="none"/><path d="M6 15h5v5" stroke="#6ee7b7" stroke-width="1.3" fill="none"/></svg>`,
  tangent:`<svg viewBox="0 0 24 24" fill="none"><circle cx="9" cy="15" r="5.5" fill="none" stroke="#34d399" stroke-width="1.6"/><path d="M3 4l18 6" stroke="#6ee7b7" stroke-width="1.8" stroke-linecap="round"/></svg>`,
  equal:`<svg viewBox="0 0 24 24" fill="none"><path d="M5 9h14M5 15h14" stroke="#34d399" stroke-width="2.2" stroke-linecap="round"/></svg>`,
  concentric:`<svg viewBox="0 0 24 24" fill="none"><circle cx="12" cy="12" r="8" fill="none" stroke="#34d399" stroke-width="1.6"/><circle cx="12" cy="12" r="3.5" fill="none" stroke="#6ee7b7" stroke-width="1.6"/><circle cx="12" cy="12" r="1" fill="#6ee7b7"/></svg>`,
  fix:`<svg viewBox="0 0 24 24" fill="none"><path d="M12 3v11" stroke="#34d399" stroke-width="1.8" stroke-linecap="round"/><path d="M6 14h12l-2.5 6h-7z" fill="#34d399" fill-opacity=".28" stroke="#34d399" stroke-width="1.5" stroke-linejoin="round"/><circle cx="12" cy="4" r="1.9" fill="#6ee7b7"/></svg>`,
  extrude:`<svg viewBox="0 0 24 24" fill="none"><rect x="3" y="9" width="12" height="12" rx="1.5" fill="#22c55e" fill-opacity=".2" stroke="#22c55e" stroke-width="1.5"/><path d="M9 9V3h12v12h-6" stroke="#4ade80" stroke-width="1.5" stroke-linejoin="round" fill="none"/><path d="M9 9l12-6" stroke="#4ade80" stroke-width="1.2" opacity=".6"/></svg>`,
  revolve:`<svg viewBox="0 0 24 24" fill="none"><path d="M12 2v20" stroke="#94a3b8" stroke-width="1.5" stroke-dasharray="3 2"/><ellipse cx="12" cy="12" rx="7" ry="9" fill="#a855f7" fill-opacity=".22" stroke="#a855f7" stroke-width="1.6"/><ellipse cx="12" cy="12" rx="2.6" ry="9" fill="none" stroke="#c084fc" stroke-width="1.2" opacity=".7"/></svg>`,
  sweep:`<svg viewBox="0 0 24 24" fill="none"><path d="M3 18c5 0 6-12 12-12" stroke="#f97316" stroke-width="1.7" stroke-linecap="round" fill="none"/><ellipse cx="15" cy="6" rx="2.4" ry="3.4" fill="#fb923c" fill-opacity=".5" stroke="#fb923c" stroke-width="1.3"/><circle cx="3" cy="18" r="2" fill="#fdba74"/></svg>`,
  loft:`<svg viewBox="0 0 24 24" fill="none"><ellipse cx="6" cy="6" rx="3.2" ry="2.1" fill="#22d3ee" fill-opacity=".4" stroke="#22d3ee" stroke-width="1.4"/><rect x="15" y="15" width="6" height="6" rx="1" fill="#22d3ee" fill-opacity=".3" stroke="#22d3ee" stroke-width="1.4"/><path d="M7.6 7.4L15 15M8.4 5.2L21 15" stroke="#67e8f9" stroke-width="1.2" opacity=".7"/></svg>`,
  fillet:`<svg viewBox="0 0 24 24" fill="none"><path d="M5 20V11A6 6 0 0 1 11 5h9" stroke="#60a5fa" stroke-width="1.9" stroke-linecap="round" fill="none"/><path d="M5 20h-1M20 5v-1" stroke="#60a5fa" stroke-width="1.5" stroke-linecap="round"/><circle cx="11" cy="11" r="1.6" fill="#93c5fd"/></svg>`,
  chamfer:`<svg viewBox="0 0 24 24" fill="none"><path d="M5 20V10l5-5h10" stroke="#fbbf24" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round" fill="none"/><circle cx="10" cy="5" r="1.5" fill="#fcd34d"/><circle cx="5" cy="10" r="1.5" fill="#fcd34d"/></svg>`,
  shell:`<svg viewBox="0 0 24 24" fill="none"><path d="M4 8l8-4.5L20 8v8l-8 4.5L4 16z" fill="none" stroke="#2dd4bf" stroke-width="1.6" stroke-linejoin="round"/><path d="M8 10l4-2.2 4 2.2v4l-4 2.2-4-2.2z" fill="#2dd4bf" fill-opacity=".22" stroke="#5eead4" stroke-width="1.2" stroke-linejoin="round"/></svg>`,
  draft:`<svg viewBox="0 0 24 24" fill="none"><path d="M6 20L10 4h4l4 16z" fill="#c084fc" fill-opacity=".22" stroke="#c084fc" stroke-width="1.5" stroke-linejoin="round"/><path d="M10 4l-4 16M14 4l4 16" stroke="#d8b4fe" stroke-width="1.1" opacity=".7"/></svg>`,
  hole:`<svg viewBox="0 0 24 24" fill="none"><rect x="3.5" y="3.5" width="17" height="17" rx="2.5" fill="#1e293b" fill-opacity=".5" stroke="#64748b" stroke-width="1.4"/><circle cx="12" cy="12" r="4.4" fill="#0b0b0b" stroke="#f87171" stroke-width="1.6"/><path d="M12 5.5v2M12 16.5v2M5.5 12h2M16.5 12h2" stroke="#f87171" stroke-width="1.3" stroke-linecap="round"/></svg>`,
  pattern:`<svg viewBox="0 0 24 24" fill="none"><rect x="3" y="3" width="6.5" height="6.5" rx="1.2" fill="#a855f7"/><rect x="14.5" y="3" width="6.5" height="6.5" rx="1.2" fill="#a855f7" fill-opacity=".6"/><rect x="3" y="14.5" width="6.5" height="6.5" rx="1.2" fill="#a855f7" fill-opacity=".6"/><rect x="14.5" y="14.5" width="6.5" height="6.5" rx="1.2" fill="#a855f7" fill-opacity=".3"/></svg>`,
  bunion:`<svg viewBox="0 0 24 24" fill="none"><circle cx="9" cy="12" r="6.5" fill="#22c55e" fill-opacity=".3" stroke="#22c55e" stroke-width="1.4"/><circle cx="15" cy="12" r="6.5" fill="#22c55e" fill-opacity=".3" stroke="#22c55e" stroke-width="1.4"/></svg>`,
  bsubtract:`<svg viewBox="0 0 24 24" fill="none"><circle cx="9" cy="12" r="6.5" fill="#ef4444" fill-opacity=".28" stroke="#ef4444" stroke-width="1.4"/><circle cx="15" cy="12" r="6.5" fill="#0b0b0b" stroke="#f87171" stroke-width="1.4" stroke-dasharray="3 2"/></svg>`,
  bintersect:`<svg viewBox="0 0 24 24" fill="none"><circle cx="9" cy="12" r="6.5" fill="none" stroke="#f59e0b" stroke-width="1.4"/><circle cx="15" cy="12" r="6.5" fill="none" stroke="#f59e0b" stroke-width="1.4"/><path d="M12 6.6a6.5 6.5 0 0 1 0 10.8 6.5 6.5 0 0 1 0-10.8z" fill="#fbbf24" fill-opacity=".55"/></svg>`,
  measure:`<svg viewBox="0 0 24 24" fill="none"><rect x="2.5" y="8" width="19" height="8" rx="1.5" fill="#38bdf8" fill-opacity=".18" stroke="#38bdf8" stroke-width="1.5" transform="rotate(-12 12 12)"/><path d="M7 9.5v2M11 8.6v2.6M15 7.7v2M19 6.8v2.6" stroke="#7dd3fc" stroke-width="1.3" stroke-linecap="round"/></svg>`,
  section:`<svg viewBox="0 0 24 24" fill="none"><path d="M12 3l8 4.6v8.8L12 21l-8-4.6V7.6z" fill="none" stroke="#64748b" stroke-width="1.4" stroke-linejoin="round"/><path d="M4 12h16" stroke="#f59e0b" stroke-width="1.8" stroke-dasharray="3 2.4"/><circle cx="20" cy="12" r="1.8" fill="#fbbf24"/></svg>`,
};

/* environment glyphs — Sketch (2D) ⇄ Model (3D solid) */
const CAD_ENV_ICONS={
  sketch:`<svg viewBox="0 0 24 24" fill="none"><path d="M3 17c4 0 4-10 8-10s4 10 10 4" stroke="#a855f7" stroke-width="2" stroke-linecap="round" fill="none"/><circle cx="3" cy="17" r="1.9" fill="#c084fc"/><circle cx="21" cy="11" r="1.9" fill="#c084fc"/></svg>`,
  model:`<svg viewBox="0 0 24 24" fill="none"><path d="M12 3l8 4.6v8.8L12 21l-8-4.6V7.6L12 3z" fill="#38bdf8" fill-opacity=".22" stroke="#38bdf8" stroke-width="1.4" stroke-linejoin="round"/><path d="M4 7.6L12 12l8-4.4M12 12v9" stroke="#7dd3fc" stroke-width="1.2" opacity=".75"/></svg>`,
};

/* single-silhouette CAD workspace glyph (drafting compass) — takes --ws-color */
const WS_GLYPH_CAD=`<svg viewBox="0 0 24 24" fill="currentColor" xmlns="http://www.w3.org/2000/svg"><path d="M16.04 3.34a1.6 1.6 0 0 1 2.27 0l2.35 2.35a1.6 1.6 0 0 1 0 2.27L8.96 19.66a1.6 1.6 0 0 1-2.27 0l-2.35-2.35a1.6 1.6 0 0 1 0-2.27L16.04 3.34Zm-.8 3.07L13.6 8.05l1.3 1.3 1.05-1.05-1.3-1.3.6-.6 1.3 1.3 1.06-1.05-1.3-1.3.74-.73 1.84 1.84-9.1 9.1-1.84-1.84.73-.74 1.3 1.3 1.05-1.05-1.3-1.3.6-.6 1.3 1.3Z"/></svg>`;

/* ─── UI LINE-ICON SET ───
   Coloured stroke icons keyed by the data-ic names used across index.html and the
   render code. `currentColor` lets host context tint them where wanted; a --ic-cad
   accent is baked where the design calls for the CAD blue. */
const IC=(paths,opt={})=>`<svg viewBox="0 0 24 24" fill="none" stroke="${opt.stroke||"currentColor"}" stroke-width="${opt.sw||2}" stroke-linecap="round" stroke-linejoin="round">${paths}</svg>`;
const CAD_BLUE="#ffffff";   // chrome-icon accent — white, per design (tool glyphs above keep their colour)
const UI_ICONS={
  search:IC(`<circle cx="11" cy="11" r="7"/><path d="M21 21l-4.3-4.3"/>`),
  plus:IC(`<path d="M12 5v14M5 12h14"/>`),
  x:IC(`<path d="M18 6L6 18M6 6l12 12"/>`),
  check:IC(`<path d="M20 6L9 17l-5-5"/>`),
  "chevron-down":IC(`<path d="M6 9l6 6 6-6"/>`),
  "chevron-up":IC(`<path d="M18 15l-6-6-6 6"/>`),
  "chevron-right":IC(`<path d="M9 6l6 6-6 6"/>`),
  "chevrons-down":IC(`<path d="M7 6l5 5 5-5M7 13l5 5 5-5"/>`),
  "chevrons-down-up":IC(`<path d="M7 15l5 5 5-5M7 9l5-5 5 5"/>`),
  box:IC(`<path d="M12 3l8 4.6v8.8L12 21l-8-4.6V7.6z" stroke="${CAD_BLUE}"/><path d="M4 7.6L12 12l8-4.4M12 12v9" stroke="${CAD_BLUE}" opacity=".7"/>`,{sw:1.6}),
  "git-commit-vertical":IC(`<path d="M12 3v6M12 15v6" stroke="${CAD_BLUE}"/><circle cx="12" cy="12" r="3.2" stroke="${CAD_BLUE}"/>`,{sw:1.8}),
  "mouse-pointer-2":IC(`<path d="M5 3l7 17 2.4-7L21 11z" stroke="${CAD_BLUE}" fill="rgba(255,255,255,.18)"/>`,{sw:1.6}),
  "mouse-pointer-square-dashed":IC(`<path d="M5 3h3M11 3h3M17 3h2a1 1 0 0 1 1 1v2M20 10v3M4 6V4a1 1 0 0 1 1-1M4 10v4a1 1 0 0 0 1 1h3"/><path d="M12 12l7 3-3 1.5L14.5 20z" stroke="${CAD_BLUE}" fill="rgba(255,255,255,.18)"/>`,{sw:1.5}),
  "layout-grid":IC(`<rect x="3" y="3" width="7" height="7" rx="1" stroke="${CAD_BLUE}"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/>`,{sw:1.7}),
  ruler:IC(`<rect x="2.5" y="8" width="19" height="8" rx="1.5" transform="rotate(-12 12 12)" stroke="${CAD_BLUE}" fill="rgba(255,255,255,.12)"/><path d="M7 9.5v2M11 8.6v2.6M15 7.7v2M19 6.8v2.6" stroke="#7dd3fc"/>`,{sw:1.4}),
  "grid-3x3":IC(`<rect x="3" y="3" width="18" height="18" rx="2"/><path d="M9 3v18M15 3v18M3 9h18M3 15h18"/>`,{sw:1.5}),
  "undo-2":IC(`<path d="M9 14L4 9l5-5"/><path d="M4 9h11a6 6 0 0 1 0 12h-3"/>`,{sw:1.8}),
  "redo-2":IC(`<path d="M15 14l5-5-5-5"/><path d="M20 9H9a6 6 0 0 0 0 12h3"/>`,{sw:1.8}),
  "file-plus":IC(`<path d="M14 3v5h5"/><path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M12 11v6M9 14h6" stroke="${CAD_BLUE}"/>`,{sw:1.6}),
  "folder-open":IC(`<path d="M4 8V6a2 2 0 0 1 2-2h4l2 2h6a2 2 0 0 1 2 2v1"/><path d="M2.5 12.5L4 20a1 1 0 0 0 1 .8h13a1 1 0 0 0 1-.8l1.5-7.5a1 1 0 0 0-1-1.2H3.5a1 1 0 0 0-1 1.2z" stroke="${CAD_BLUE}"/>`,{sw:1.6}),
  save:IC(`<path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z" stroke="${CAD_BLUE}"/><path d="M17 21v-8H7v8M7 3v5h8"/>`,{sw:1.6}),
  palette:IC(`<circle cx="13.5" cy="6.5" r="1.5" fill="#f87171" stroke="none"/><circle cx="17.5" cy="10.5" r="1.5" fill="#fbbf24" stroke="none"/><circle cx="8.5" cy="7.5" r="1.5" fill="#60a5fa" stroke="none"/><circle cx="6.5" cy="12.5" r="1.5" fill="#4ade80" stroke="none"/><path d="M12 2a10 10 0 0 0 0 20c1.6 0 2-1.3 1.2-2.3-.9-1.1-.3-2.7 1.1-2.7H17a5 5 0 0 0 5-5c0-5.5-4.5-10-10-10z"/>`,{sw:1.5}),
  "pencil-ruler":IC(`<path d="M14 3l7 7-9 9-7-7z" stroke="${CAD_BLUE}"/><path d="M9 8l2 2M12 5l2 2"/>`,{sw:1.6}),
  "sliders-horizontal":IC(`<path d="M4 8h10M18 8h2M4 16h4M12 16h8"/><circle cx="15" cy="8" r="2" fill="rgba(255,255,255,.25)" stroke="${CAD_BLUE}"/><circle cx="9" cy="16" r="2" fill="rgba(255,255,255,.25)" stroke="${CAD_BLUE}"/>`,{sw:1.7}),
  "trash-2":IC(`<path d="M3 6h18M8 6V4a1 1 0 0 1 1-1h6a1 1 0 0 1 1 1v2M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6M10 11v6M14 11v6"/>`,{sw:1.6}),
  rewind:IC(`<path d="M11 19L2 12l9-7zM22 19l-9-7 9-7z" fill="rgba(255,255,255,.2)" stroke="${CAD_BLUE}"/>`,{sw:1.6}),
  "grip-horizontal":IC(`<circle cx="7" cy="9" r="1.3" fill="currentColor" stroke="none"/><circle cx="12" cy="9" r="1.3" fill="currentColor" stroke="none"/><circle cx="17" cy="9" r="1.3" fill="currentColor" stroke="none"/><circle cx="7" cy="15" r="1.3" fill="currentColor" stroke="none"/><circle cx="12" cy="15" r="1.3" fill="currentColor" stroke="none"/><circle cx="17" cy="15" r="1.3" fill="currentColor" stroke="none"/>`),
  eye:IC(`<path d="M2 12s3.5-7 10-7 10 7 10 7-3.5 7-10 7-10-7-10-7z"/><circle cx="12" cy="12" r="3"/>`,{sw:1.7}),
  "eye-off":IC(`<path d="M9.9 5.2A9.9 9.9 0 0 1 12 5c6.5 0 10 7 10 7a15 15 0 0 1-3.3 4M6.6 6.6A15 15 0 0 0 2 12s3.5 7 10 7a9.7 9.7 0 0 0 4.4-1M3 3l18 18M9.6 9.6a3 3 0 0 0 4.2 4.2"/>`,{sw:1.7}),
  history:IC(`<path d="M3 12a9 9 0 1 0 3-6.7L3 8"/><path d="M3 3v5h5"/><path d="M12 7v5l3.5 2" stroke="${CAD_BLUE}"/>`,{sw:1.7}),
  "list-tree":IC(`<path d="M10 6h11M10 12h11M14 18h7"/><path d="M4 4v14a1 1 0 0 0 1 1h4M4 11h5" stroke="${CAD_BLUE}"/>`,{sw:1.7}),
  wrench:IC(`<path d="M14.7 6.3a4 4 0 0 0-5.2 5.2l-6 6a1.5 1.5 0 0 0 2 2l6-6a4 4 0 0 0 5.2-5.2l-2.4 2.4-2-2z" stroke="${CAD_BLUE}" fill="rgba(255,255,255,.12)"/>`,{sw:1.6}),
  "git-branch":IC(`<circle cx="6" cy="6" r="2.5" stroke="${CAD_BLUE}"/><circle cx="6" cy="18" r="2.5"/><circle cx="18" cy="8" r="2.5" stroke="${CAD_BLUE}"/><path d="M6 8.5v7M18 10.5a6 6 0 0 1-6 6H8.5"/>`,{sw:1.6}),
  "check-check":IC(`<path d="M18 6L7 17l-4-4" stroke="${CAD_BLUE}"/><path d="M22 8l-6 6"/>`,{sw:1.8}),
  "pen-tool":IC(`<path d="M12 19l7-7 3 3-7 7-3-3z" stroke="${CAD_BLUE}"/><path d="M18 13l-1.5-7.5L2 2l3.5 14.5L13 18z"/><path d="M2 2l7.6 7.6"/>`,{sw:1.5}),
  compass:IC(`<circle cx="12" cy="12" r="9" stroke="${CAD_BLUE}"/><path d="M16 8l-2 6-6 2 2-6z" fill="rgba(255,255,255,.25)" stroke="${CAD_BLUE}"/>`,{sw:1.6}),
  square:IC(`<rect x="4" y="4" width="16" height="16" rx="2"/>`,{sw:1.7}),
  layers:IC(`<path d="M12 3l9 5-9 5-9-5z"/><path d="M3 12l9 5 9-5M3 17l9 5 9-5"/>`,{sw:1.6}),
  folder:IC(`<path d="M4 6a2 2 0 0 1 2-2h4l2 2h6a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2z" stroke="${CAD_BLUE}"/>`,{sw:1.6}),
  move:IC(`<path d="M12 2v20M2 12h20M9 5l3-3 3 3M9 19l3 3 3-3M5 9l-3 3 3 3M19 9l3 3-3 3"/>`,{sw:1.6}),
  info:IC(`<circle cx="12" cy="12" r="9"/><path d="M12 16v-5M12 8h.01"/>`,{sw:1.7}),
  "circle-dashed":IC(`<path d="M10.1 3.2a9 9 0 0 0-3.5 1.8M4.9 6.6a9 9 0 0 0-1.7 3.5M3.1 13.9a9 9 0 0 0 1.8 3.5M6.6 19.1a9 9 0 0 0 3.5 1.7M13.9 20.8a9 9 0 0 0 3.5-1.8M19.1 17.4a9 9 0 0 0 1.7-3.5M20.8 10.1a9 9 0 0 0-1.8-3.5M17.4 4.9a9 9 0 0 0-3.5-1.7"/>`,{sw:1.7}),
  layers3:IC(`<path d="M12 3l9 5-9 5-9-5z"/><path d="M3 12l9 5 9-5M3 17l9 5 9-5"/>`,{sw:1.6}),
  "settings-2":IC(`<path d="M20 7h-9M14 17H5"/><circle cx="17" cy="17" r="3"/><circle cx="7" cy="7" r="3"/>`,{sw:1.8}),
};

/* inject the SVG for every un-hydrated [data-ic] under `root`. Idempotent. */
function hydrateIcons(root){
  const scope=root&&root.querySelectorAll?root:document;
  scope.querySelectorAll("[data-ic]").forEach(el=>{
    if(el.dataset.icDone==="1") return;
    const svg=UI_ICONS[el.dataset.ic];
    if(svg){ el.innerHTML=svg; el.dataset.icDone="1"; }
  });
}
