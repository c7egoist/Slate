"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   data.js — shared constants, the seeded CAD part browser tree, and the global
             UI state. Load FIRST after icons.js (every other module reads these).

             This is the standalone CAD workspace: `currentWs` is always "cad".
             The browser tree mirrors Studio's DATA.cad seed exactly — Origin
             datums + 3 planes, a Sketches group, and the Bodies the feature
             timeline builds.
   ════════════════════════════════════════════════════════════════════════════ */

const PALETTE=['#ef4444','#eab308','#22c55e','#3b82f6','#a855f7','#ec4899','#f97316','#14b8a6'];

/* the CAD workspace descriptor (label / accent / viewport captions) */
const CAD_WS={ label:"CAD", icon:"ruler", color:"#ffffff", olContext:"Outliner", vpLabel:"CAD Modeler" };

/* per-item factory — every browser node carries a `kind` that drives the
   properties router and an icon key resolved against UI_ICONS. */
let uid=0; const nid=()=>"n"+(++uid);
function item(name,icon,extra={}){return Object.assign({id:nid(),name,icon,visible:true,selected:false,children:[],collapsed:false},extra);}

/* ─── SEED: the parametric B-rep part browser ───
   origin datums + planes, the sketch group, and the solid/surface bodies. */
const CAD_TREE=(()=>{
  const origin=item("Origin","compass",{kind:"datums",locked:true});
  origin.children=[
    item("Front Plane","square",{kind:"plane",swatch:"#ef4444",axis:"XY"}),
    item("Top Plane","square",  {kind:"plane",swatch:"#22c55e",axis:"XZ"}),
    item("Right Plane","square",{kind:"plane",swatch:"#3b82f6",axis:"YZ"}),
  ];
  const sketches=item("Sketches","pen-tool",{kind:"folder"});
  sketches.children=[];   // empty scene — sketches are added by the draw tools
  const bodies=item("Bodies","box",{kind:"folder"});
  bodies.children=[];   // empty scene — no seeded demo bodies
  return [origin, sketches, bodies];
})();

/* ─── GLOBAL STATE ─── */
let currentWs="cad";     // fixed: this prototype is CAD-only
let selectedId=null;
let selectionOrder=[];   // node ids in selection order; [0] = boolean base (multi-select)
let searchTerm="";
let cadSnap=true;        // status-bar snap toggle
