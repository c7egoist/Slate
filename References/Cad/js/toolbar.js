"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   toolbar.js — LEFT-dock Tools pane: a Sketch ⇄ Model environment pill, tool
                search, and the grouped collapsible tool list. Sketch draw tools
                become the active mode; dimension + constraint tools annotate the
                sketch; model feature tools fire cadAddFeature() and append to the
                timeline. Keyboard shortcuts mirror SolidWorks / Fusion.

                (The old vertex/edge/face/body selection-filter pill is gone —
                sketch picking is now auto-detect, so no element-type toggle.)

                Also owns the LEFT Browser ⇄ Tools carousel.
   ════════════════════════════════════════════════════════════════════════════ */

const CAD_ENV_LABEL={sketch:"Sketch",model:"Model"};

/* tool registry per environment */
const CAD_SKETCH_GROUPS=[
  {label:"Draw", tools:[
    {id:"line",      icon:"line",     name:"Line",       key:"L"},
    {id:"rectangle", icon:"rect",     name:"Rectangle",  key:"R"},
    {id:"polygon",   icon:"polygon",  name:"Polygon"},
    {id:"circle2d",  icon:"circle2d", name:"Circle",     key:"C"},
    {id:"arc",       icon:"arc",      name:"Arc",        key:"A"},
    {id:"bevel",     icon:"fillet",   name:"Fillet/Chamfer"},
  ]},
  {label:"Curve", tools:[
    {id:"bezier",    icon:"bezier",   name:"Bezier",     key:"B"},
    {id:"bspline",   icon:"bspline",  name:"B-spline"},
    {id:"nurbs",     icon:"nurbs",    name:"NURBS"},
  ]},
  {label:"Dimension", tools:[
    {id:"dimsmart",      icon:"dimension",   name:"Smart Dimension", key:"D"},
    {id:"dimension",     icon:"dimension",   name:"Dimension"},
    {id:"dimhorizontal", icon:"horizontal",  name:"Horizontal"},
    {id:"dimvertical",   icon:"vertical",    name:"Vertical"},
    {id:"dimradius",     icon:"dimradius",   name:"Radius"},
    {id:"dimdiameter",   icon:"dimdiameter", name:"Diameter"},
    {id:"dimangle",      icon:"dimangle",    name:"Angle"},
  ]},
  {label:"Constrain", tools:[
    {id:"coincident",    icon:"coincident",    name:"Coincident",    key:"K"},
    {id:"horizontal",    icon:"horizontal",    name:"Horizontal",    key:"H"},
    {id:"vertical",      icon:"vertical",      name:"Vertical",      key:"V"},
    {id:"parallel",      icon:"parallel",      name:"Parallel",      key:"P"},
    {id:"perpendicular", icon:"perpendicular", name:"Perpendicular"},
    {id:"tangent",       icon:"tangent",       name:"Tangent",       key:"T"},
    {id:"equal",         icon:"equal",         name:"Equal"},
    {id:"concentric",    icon:"concentric",    name:"Concentric"},
    {id:"fix",           icon:"fix",           name:"Fix"},
  ]},
  {label:"Boolean", tools:[
    {id:"union",     icon:"bunion",     name:"Union"},
    {id:"subtract",  icon:"bsubtract",  name:"Subtract"},
    {id:"intersect", icon:"bintersect", name:"Intersect"},
    {id:"join",      icon:"bunion",     name:"Join"},
  ]},
];
/* sketch draw tools enter viewport draw-mode; everything else in the Dimension /
   Constrain groups is an entity-pick annotation (stubbed in this prototype). */
const CAD_DRAW_TOOLS=new Set(["line","rectangle","polygon","circle2d","bezier","bspline","nurbs"]);
const CAD_SKETCH_ANNOTATE=new Set([
  "dimsmart","dimension","dimhorizontal","dimvertical","dimradius","dimdiameter","dimangle",
  "coincident","horizontal","vertical","parallel","perpendicular","tangent","equal","concentric","fix",
]);
/* 2D shape booleans — one-shot actions on the current ordered selection (base first),
   not pick-arm tools. Handled in activateCadTool → sketchRunBoolean. */
const CAD_SKETCH_BOOLEAN=new Set(["union","subtract","intersect","join"]);
const CAD_MODEL_GROUPS=[
  {label:"Create", tools:[
    {id:"extrude", icon:"extrude", name:"Extrude",  key:"E"},
    {id:"revolve", icon:"revolve", name:"Revolve",  key:"V"},
    {id:"sweep",   icon:"sweep",   name:"Sweep"},
    {id:"loft",    icon:"loft",    name:"Loft"},
  ]},
  {label:"Modify", tools:[
    {id:"fillet",  icon:"fillet",  name:"Fillet",   key:"F"},
    {id:"chamfer", icon:"chamfer", name:"Chamfer"},
    {id:"shell",   icon:"shell",   name:"Shell"},
    {id:"draft",   icon:"draft",   name:"Draft"},
    {id:"hole",    icon:"hole",    name:"Hole",     key:"H"},
    {id:"pattern", icon:"pattern", name:"Pattern"},
  ]},
  {label:"Boolean", tools:[
    {id:"bunion",     icon:"bunion",     name:"Combine"},
    {id:"bsubtract",  icon:"bsubtract",  name:"Cut"},
    {id:"bintersect", icon:"bintersect", name:"Intersect"},
  ]},
  {label:"Inspect", tools:[
    {id:"measure", icon:"measure", name:"Measure",  key:"M"},
    {id:"section", icon:"section", name:"Section View"},
  ]},
];
/* model tools that map to a timeline feature (rest fire a plain action) */
const CAD_FEATURE_FOR_TOOL={
  extrude:"extrude", revolve:"revolve", sweep:"sweep", loft:"loft",
  fillet:"fillet", chamfer:"chamfer", shell:"shell", draft:"draft",
  hole:"hole", pattern:"pattern",
  bunion:"bunion", bsubtract:"bsubtract", bintersect:"bintersect",
};

const CAD={
  env:"model",                      // active environment: sketch | model
  active:"line",                    // active sketch draw tool
  imode:"object",                   // interaction mode: object (select+gizmo) | sketch (draw)
  toolFilter:"",                    // tool-search term
  built:false,
};

/* ─── INTERACTION MODE (Blender-style Object ⇄ Sketch) ───
   Object mode: select + transform gizmo; draw tools are inert.
   Sketch mode: draw tools active; the gizmo is hidden. Tab toggles. This is
   separate from the Sketch/Model *feature* env above — it decides whether a
   viewport click drives the gizmo or a draw tool, so the two never interfere. */
const CAD_IMODE_LABEL={object:"Object",sketch:"Sketch"};
function setCadInteractionMode(m,silent){
  if(CAD.imode===m) return;
  CAD.imode=m;
  if(m==="object"){
    // leaving draw: drop any armed draw-mode, gizmo re-attaches to the selection
    if(typeof SKETCH!=="undefined"&&SKETCH.drawing&&typeof sketchExitDrawMode==="function") sketchExitDrawMode();
    if(typeof renderProps==="function") renderProps();     // re-attach gizmo to selection
  } else {
    // entering sketch: the gizmo has no place here
    if(typeof gizmoDetach==="function") gizmoDetach();
  }
  syncInteractionMode();
  if(!silent) toast(CAD_IMODE_LABEL[m]+" mode",m==="sketch"?"pen-tool":"move");
  if(typeof vp3Draw==="function") vp3Draw();
}
function toggleCadInteractionMode(){ setCadInteractionMode(CAD.imode==="object"?"sketch":"object"); }
/* reflect the mode in the viewport pill + status bar + the pill toggle chip */
function syncInteractionMode(){
  const pill=document.getElementById("vpMode");
  if(pill){ pill.textContent=CAD_IMODE_LABEL[CAD.imode].toUpperCase(); pill.dataset.mode=CAD.imode; }
  const st=document.getElementById("stMode"); if(st) st.textContent=CAD_IMODE_LABEL[CAD.imode];
}

/* populate the env pill + filter pill + tool list into the existing DOM nodes */
function buildCadToolbarPane(){
  if(CAD.built) return;
  const envEl=document.getElementById("cdtEnv");
  if(!envEl) return;

  // environment pill (Sketch / Model) — single-select
  envEl.innerHTML="";
  Object.entries(CAD_ENV_ICONS).forEach(([m,svg])=>{
    const b=document.createElement("div");
    b.className="cdt-env-seg"+(CAD.env===m?" active":"");
    b.dataset.env=m;
    b.innerHTML=`${svg}<span>${CAD_ENV_LABEL[m]}</span>`;
    b.onclick=()=>setCadEnv(m);
    envEl.appendChild(b);
  });

  buildCadToolList();
  const ts=document.getElementById("toolSearch");
  if(ts) ts.oninput=function(){CAD.toolFilter=this.value.trim().toLowerCase();buildCadToolList();};
  CAD.built=true;
}

/* (re)build the grouped tool list for the active environment (+ tool-search) */
function buildCadToolList(){
  const scroll=document.getElementById("cdtScroll"); if(!scroll) return;
  scroll.innerHTML="";
  const groups=CAD.env==="sketch"?CAD_SKETCH_GROUPS:CAD_MODEL_GROUPS;
  const term=CAD.toolFilter;
  groups.forEach(grp=>{
    const tools=grp.tools.filter(t=>!term||t.name.toLowerCase().includes(term));
    if(!tools.length) return;
    const sec=document.createElement("div");sec.className="cdt-section";
    const title=document.createElement("div");title.className="cdt-sec-title";
    title.innerHTML=`<span>${grp.label}</span><span class="sec-count">${tools.length}</span><span class="lico chev" data-ic="chevron-down"></span>`;
    const body=document.createElement("div");body.className="cdt-sec-content";
    title.onclick=()=>{title.classList.toggle("collapsed");body.classList.toggle("collapsed");};
    tools.forEach(t=>{
      const it=document.createElement("div");
      it.className="cdt-item"+(CAD.env==="sketch"&&t.id===CAD.active?" active":"");
      it.dataset.tool=t.id;
      it.innerHTML=`<span class="cdt-ico">${CAD_ICONS[t.icon]||""}</span><span class="cdt-text">${t.name}</span>${t.key?`<span class="cdt-key">${t.key}</span>`:""}`;
      it.onclick=()=>activateCadTool(t);
      body.appendChild(it);
    });
    sec.appendChild(title);sec.appendChild(body);scroll.appendChild(sec);
  });
  if(!scroll.children.length) scroll.innerHTML='<div class="empty-state">No tool matches.</div>';
  refreshIcons(scroll);
}

/* environment swap rebuilds the tool list + viewport hint */
function setCadEnv(env){
  if(CAD.env===env) return;
  CAD.env=env;
  const envEl=document.getElementById("cdtEnv");
  if(envEl) envEl.querySelectorAll(".cdt-env-seg").forEach(x=>x.classList.toggle("active",x.dataset.env===env));
  buildCadToolList();
  syncViewportEnv();
  toast(CAD_ENV_LABEL[env]+" environment",env==="sketch"?"pen-tool":"box");
}

/* tool press: sketch draw tools become the active mode; dimension + constraint
   tools arm an annotation pick; model features fire a feature (+ append to the
   timeline); the rest fire a one-shot action. */
function activateCadTool(t){
  const foot=document.getElementById("activeToolFoot");
  if(CAD.env==="sketch"){
    CAD.active=t.id; markCadTool();
    if(foot) foot.textContent=t.name;
    // boolean ops fire immediately on the current ordered selection (not a pick tool)
    if(CAD_SKETCH_BOOLEAN.has(t.id)){
      if(typeof sketchExitDrawMode==="function"&&typeof SKETCH!=="undefined"&&SKETCH.drawing) sketchExitDrawMode();
      if(typeof sketchRunBoolean==="function") sketchRunBoolean(t.id);
      return;
    }
    // corner-blend tool arms its own pick-then-drag gesture
    if(t.id==="bevel"){ if(typeof bevelArm==="function"&&bevelArm()) return; }
    // draw tools enter viewport draw-mode
    if(CAD_DRAW_TOOLS.has(t.id)&&typeof sketchEnterDrawMode==="function"){
      setCadInteractionMode("sketch",true);       // arming a draw tool = enter sketch mode
      sketchEnterDrawMode(t.id); return;
    }
    // dimension / constraint tools arm an entity-pick that drives the solver
    if(CAD_SKETCH_ANNOTATE.has(t.id)){
      if(typeof armConstraintTool==="function"&&armConstraintTool(t.id)) return;
      if(typeof sketchExitDrawMode==="function"&&SKETCH.drawing) sketchExitDrawMode();
      toast(t.name+" — pick entities","check");
      return;
    }
    if(typeof sketchExitDrawMode==="function"&&SKETCH.drawing) sketchExitDrawMode();
    toast(t.name,"mouse-pointer-2");
    return;
  }
  const feat=CAD_FEATURE_FOR_TOOL[t.id];
  if(feat){ if(foot) foot.textContent=t.name; cadAddFeature(feat); return; }
  if(foot) foot.textContent=t.name;
  toast(t.name,"check");
}
function markCadTool(){
  const scroll=document.getElementById("cdtScroll"); if(!scroll) return;
  scroll.querySelectorAll(".cdt-item").forEach(x=>x.classList.toggle("active",CAD.env==="sketch"&&x.dataset.tool===CAD.active));
}

/* ─── LEFT-DOCK CAROUSEL: Outliner ⇄ Tools ─── */
const CAD_LEFT=[
  {key:"browser", label:"Outliner", icon:"list-tree", pane:"paneBrowser"},
  {key:"tools",   label:"Tools",    icon:"wrench",    pane:"paneTools"},
];
let cadLeftPane="browser";
function renderCadLeftCarousel(){ renderPill(leftPill,CAD_LEFT,cadLeftPane,selectCadLeftPane); }
function selectCadLeftPane(key){
  cadLeftPane=key;
  showPane(CAD_LEFT,key);
  leftPill.querySelectorAll(".di-seg").forEach(el=>el.classList.toggle("active",el.dataset.pane===key));
  if(key==="tools"){ buildCadToolbarPane(); markCadTool(); }
}

/* keyboard shortcuts (never while typing in an input) */
window.addEventListener("keydown",e=>{
  if(e.target&&(e.target.tagName==="INPUT"||e.target.tagName==="SELECT")) return;
  if(e.ctrlKey||e.metaKey||e.altKey) return;
  if(e.code==="Tab"){ e.preventDefault(); toggleCadInteractionMode(); return; }
  const groups=CAD.env==="sketch"?CAD_SKETCH_GROUPS:CAD_MODEL_GROUPS;
  const hit=groups.flatMap(g=>g.tools).find(t=>t.key&&t.key.length===1&&("Key"+t.key)===e.code);
  if(hit) activateCadTool(hit);
});
