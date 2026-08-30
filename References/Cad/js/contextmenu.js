"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   contextmenu.js — the viewport right-click menu (ContextMenu). Lets the user
                    pick the exact draw tool / variant: Rectangle, Square,
                    Polygon (Triangle…Decagon, radius-based), or Circle. Picking a
                    row swaps to the Sketch environment, arms the tool, sets the
                    side count / square flag, and enters draw-mode.

                    A floating dark panel built to match the theme. It is
                    suppressed after an RMB orbit-drag (RMB doubles as orbit) so a
                    camera spin doesn't pop the menu on release.
   ════════════════════════════════════════════════════════════════════════════ */

/* menu rows — the draw tools. The Polygon row is a single entry: its exact side
   count comes from the live "Sides" number field below (not a fixed 3…10 list),
   so the user types whatever they want and it updates in realtime. */
const CTX_ROWS=[
  {sec:"Rectangle"},
  {label:"Rectangle", icon:"rect",    tool:"rectangle", opts:{square:false}},
  {label:"Square",    icon:"square",  tool:"rectangle", opts:{square:true}},
  {sec:"Polygon"},
  {label:"Polygon",   icon:"polygon", tool:"polygon", opts:{}, sidesField:true},
  {label:"Circle",    icon:"circle2d", tool:"circle2d", opts:{}},
  {sec:"Curve"},
  {label:"Line",      icon:"line",    tool:"line",    opts:{}},
  {label:"Bezier",    icon:"bezier",  tool:"bezier",  opts:{}},
  {label:"B-spline",  icon:"bspline", tool:"bspline", opts:{}},
  {label:"NURBS",     icon:"nurbs",   tool:"nurbs",   opts:{}},
];
const CTX_SIDES_MIN=3, CTX_SIDES_MAX=64;

let _ctxEl=null;
function ctxMenuEl(){
  if(_ctxEl) return _ctxEl;
  const el=document.createElement("div"); el.className="ctx-menu"; el.id="ctxMenu";
  document.body.appendChild(el); _ctxEl=el; return el;
}
/* the live sides stepper — sets SKETCH.sides and re-previews immediately */
function ctxSidesField(){
  const row=document.createElement("div"); row.className="ctx-field";
  const lbl=document.createElement("span"); lbl.className="ctx-field-lbl"; lbl.textContent="Sides";
  const box=document.createElement("div"); box.className="ctx-step";
  const dec=document.createElement("button"); dec.className="ctx-step-btn"; dec.textContent="–";
  const inp=document.createElement("input"); inp.className="ctx-step-inp"; inp.type="text"; inp.value=SKETCH.sides;
  const inc=document.createElement("button"); inc.className="ctx-step-btn"; inc.textContent="+";
  // if a polygon object is selected, the field edits THAT object; otherwise it
  // sets the next-draw default. (Fixes "changes the next shape, not the current".)
  const sel=(typeof sketchSelectedObject==="function")?sketchSelectedObject():null;
  const target=(sel&&sel.category==="polygon")?sel:null;
  if(target){ lbl.textContent="Sides"; inp.value=target.sides; }
  const apply=(n)=>{
    n=Math.max(CTX_SIDES_MIN,Math.min(CTX_SIDES_MAX,Math.round(n)||(target?target.sides:SKETCH.sides)));
    inp.value=n;
    if(target){ target.sides=n; if(typeof sketchRebuildPoly==="function") sketchRebuildPoly(target); if(typeof vp3Draw==="function") vp3Draw(); if(typeof renderProps==="function") renderProps(); }
    else { SKETCH.sides=n; ctxApplySidesLive(); }   // realtime: rebuild preview / active tool
  };
  dec.onclick=(e)=>{e.stopPropagation();apply(SKETCH.sides-1);};
  inc.onclick=(e)=>{e.stopPropagation();apply(SKETCH.sides+1);};
  inp.onclick=(e)=>e.stopPropagation();
  inp.onkeydown=(e)=>{ e.stopPropagation(); if(e.key==="Enter"){ apply(parseInt(inp.value,10)); inp.blur(); } };
  inp.onchange=()=>apply(parseInt(inp.value,10));
  box.appendChild(dec); box.appendChild(inp); box.appendChild(inc);
  row.appendChild(lbl); row.appendChild(box);
  return row;
}
/* push a live sides change through: refresh the cmd-bar chips, re-preview a
   mid-draw polygon, and re-title the active-tool footer. */
function ctxApplySidesLive(){
  if(typeof sketchFillCmdOpts==="function") sketchFillCmdOpts();
  if(typeof SKETCH!=="undefined"&&SKETCH.drawing&&SKETCH.stage==="dragging"&&typeof sketchBuildPreview==="function"){
    sketchBuildPreview(); vp3Draw();
  }
  const foot=document.getElementById("activeToolFoot");
  if(foot&&CAD.active==="polygon") foot.textContent=sketchPolyName(SKETCH.sides);
}
function ctxOpen(clientX,clientY){
  const el=ctxMenuEl();
  el.innerHTML="";
  CTX_ROWS.forEach(r=>{
    if(r.sec){ const s=document.createElement("div"); s.className="ctx-sec"; s.textContent=r.sec; el.appendChild(s); return; }
    const b=document.createElement("button"); b.className="ctx-item";
    const tag=r.tool==="polygon"?`<span class="ctx-tag">${sketchPolyName(SKETCH.sides)}</span>`:"";
    b.innerHTML=`<span class="ctx-ico">${CAD_ICONS[r.icon]||""}</span><span class="ctx-lbl">${r.label}</span>${tag}`;
    b.onclick=()=>{ ctxPick(r.tool,r.opts); ctxClose(); };
    el.appendChild(b);
    if(r.sidesField) el.appendChild(ctxSidesField());
  });
  el.style.display="block";
  // clamp to viewport
  const vw=window.innerWidth, vh=window.innerHeight, r=el.getBoundingClientRect();
  el.style.left=Math.min(clientX,vw-r.width-8)+"px";
  el.style.top=Math.min(clientY,vh-r.height-8)+"px";
  el.classList.add("open");
  if(typeof refreshIcons==="function") refreshIcons(el);
}
function ctxClose(){ if(_ctxEl){ _ctxEl.classList.remove("open"); _ctxEl.style.display="none"; } }

function ctxPick(toolId,opts){
  if(CAD.env!=="sketch"&&typeof setCadEnv==="function") setCadEnv("sketch");
  if(opts){ if(opts.sides) SKETCH.sides=opts.sides; if("square" in opts) SKETCH.square=opts.square; }
  CAD.active=toolId; if(typeof markCadTool==="function") markCadTool();
  const foot=document.getElementById("activeToolFoot");
  if(foot){
    const curveName={line:"Line",bezier:"Bezier",bspline:"B-spline",nurbs:"NURBS"}[toolId];
    foot.textContent=curveName||(toolId==="polygon"?sketchPolyName(SKETCH.sides):toolId==="rectangle"?(SKETCH.square?"Square":"Rectangle"):"Circle");
  }
  if(typeof setCadInteractionMode==="function") setCadInteractionMode("sketch",true);  // draw mode
  if(typeof sketchEnterDrawMode==="function") sketchEnterDrawMode(toolId);
}

/* ─── wiring: right-click on the viewport stage opens the menu ───
   Track RMB movement so an orbit-drag (RMB) suppresses the menu on release. */
let _rmbDownX=0,_rmbDownY=0,_rmbMoved=false;
window.addEventListener("mousedown",e=>{ if(e.button===2){ _rmbDownX=e.clientX; _rmbDownY=e.clientY; _rmbMoved=false; } });
window.addEventListener("mousemove",e=>{ if(e.buttons&2){ if(Math.hypot(e.clientX-_rmbDownX,e.clientY-_rmbDownY)>5) _rmbMoved=true; } });

function bindContextMenu(){
  const stage=document.getElementById("stage"); if(!stage) return;
  stage.addEventListener("contextmenu",e=>{
    e.preventDefault();
    if(_rmbMoved) return;                 // that RMB was an orbit, not a menu click
    ctxOpen(e.clientX,e.clientY);
  });
  document.addEventListener("mousedown",e=>{ if(_ctxEl&&_ctxEl.classList.contains("open")&&!e.target.closest("#ctxMenu")) ctxClose(); });
  window.addEventListener("keydown",e=>{ if(e.key==="Escape") ctxClose(); });
}
