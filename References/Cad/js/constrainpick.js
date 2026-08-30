"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   constrainpick.js — the pick-and-apply workflow that drives the constraint
                      solver (constraints.js) from the Dimension / Constrain tool
                      groups.

     Arming a constraint tool (activateCadTool → armConstraintTool) enters a pick
     state: the user clicks 1 or 2 sub-entities in the viewport (reusing the
     auto-detect ssHitTest), a HUD toast tracks progress, and once enough entities
     are gathered the constraint is created via addConstraint(). Dimensional tools
     (linear / angle) then prompt for a value in a small floating input near the
     cursor before solving.

     Picking reuses subselect's hit-test; a vertex → a 'point' entity, an edge →
     a 'line' entity (segment cpts[i]→cpts[i+1]). Esc cancels the armed tool.
   ════════════════════════════════════════════════════════════════════════════ */

const CPICK={ active:null, type:null, need:0, picks:[] };   // active = tool id or null

/* map a subselect hit → a constraint entity ref (or null if unusable) */
function cpickEntityFromHit(hit){
  if(!hit) return null;
  const obj=hit.obj;
  if(hit.kind==="vertex") return {objId:obj.id,kind:"point",i:hit.i,_screenObj:obj};
  if(hit.kind==="edge"){
    // segment cpts[i]→cpts[i+1] on a curve; primitive corners aren't line entities yet
    if(typeof isCurveCategory==="function"&&isCurveCategory(obj.category))
      return {objId:obj.id,kind:"line",i:hit.i,_screenObj:obj};
    return {objId:obj.id,kind:"line",i:hit.i,_screenObj:obj};
  }
  if(hit.kind==="face") return {objId:obj.id,kind:"circle",i:-1,_screenObj:obj};
  return null;
}
/* expand a picked edge (line entity) into its two endpoint POINT entities, so a
   single-edge linear dimension measures that edge's length with the existing
   point-to-point solver residual (constraints.js). Returns null when unusable.

   A rectangle / polygon is picked as an edge too, but stores origin/w/h or
   center/rim rather than cpts, so the solver can't move it. Convert it to an
   editable corner polyline (line curve) first — then its corners ARE cpts the
   dimension can drive. This is what makes "pick one edge of a rectangle" work. */
function cpickExpandLineEnts(ent){
  if(!ent||ent.kind!=="line") return null;
  let o=(typeof cxObj==="function")?cxObj(ent.objId):null;
  if(o&&!o.cpts&&typeof sketchPrimitiveToCurve==="function") sketchPrimitiveToCurve(o);
  if(!o||!o.cpts||!o.cpts.length) return null;
  const n=o.cpts.length, i0=ent.i, i1=(ent.i+1)%n;
  if(i0<0||i0>=n||i0===i1) return null;
  return [ {objId:ent.objId,kind:"point",i:i0,_screenObj:o},
           {objId:ent.objId,kind:"point",i:i1,_screenObj:o} ];
}
/* short hint describing what to pick next */
function cpickHint(){
  const t=CPICK.type; if(!t) return "";
  const label=(typeof CX_LABEL!=="undefined"&&CX_LABEL[t])||t;
  const got=CPICK.picks.length;
  if(t==="smart"&&got===0) return "Smart Dimension — pick an edge, circle, or point";
  if((t==="linear"||t==="hdist"||t==="vdist")&&got===0) return label+" — pick an edge, or two points";
  if(CPICK.need===1) return label+" — pick an entity";
  return label+" — pick entity "+(got+1)+" of "+CPICK.need;
}

/* arm a Dimension/Constrain tool for picking (called from toolbar.activateCadTool). */
function armConstraintTool(toolId){
  const type=CPICK_TOOL_TYPE[toolId]; if(!type) return false;
  // dimension/constraint work happens in Sketch interaction-mode picking, gizmo off
  if(typeof setCadInteractionMode==="function") setCadInteractionMode("sketch",true);
  if(typeof sketchExitDrawMode==="function"&&typeof SKETCH!=="undefined"&&SKETCH.drawing) sketchExitDrawMode();
  CPICK.active=toolId; CPICK.type=type;
  CPICK.need=(typeof CX_ARITY!=="undefined"&&CX_ARITY[type])||1;
  CPICK.picks=[];
  if(typeof toast==="function") toast(cpickHint(),"mouse-pointer-2");
  if(typeof vp3Draw==="function") vp3Draw();
  return true;
}
/* which constraint type a tool id creates (radius/diameter map onto their dims) */
const CPICK_TOOL_TYPE={
  dimsmart:"smart", dimension:"linear", dimhorizontal:"hdist", dimvertical:"vdist",
  dimradius:"radius", dimdiameter:"diameter", dimangle:"angle",
  coincident:"coincident", horizontal:"horizontal", vertical:"vertical",
  parallel:"parallel", perpendicular:"perpendicular", tangent:"tangent",
  equal:"equal", concentric:"concentric", fix:"fix",
};
/* linear-family dimension types measure the same two endpoints of an edge (they
   differ only in the residual + witness direction), so all three drive the
   single-edge → drag-out gesture. */
const CPICK_EDGE_DIM=new Set(["linear","hdist","vdist"]);

function cpickCancel(silent){
  const wasActive=CPICK.active;
  CPICK.active=null; CPICK.type=null; CPICK.need=0; CPICK.picks=[];
  if(!wasActive) return;
  if(!silent&&typeof toast==="function") toast("Cancelled","x");
  if(typeof vp3Draw==="function") vp3Draw();
}

/* ─── MOUSEDOWN HOOK ─── slotted into vp3AttachInput BEFORE ptMouseDown/ssMouseDown
   so an armed constraint tool owns the click. Returns true to consume it. */
function cpickMouseDown(e,mx,my){
  if(!CPICK.active||e.button!==0) return false;
  if(typeof ssHitTest!=="function") return false;
  const hit=ssHitTest(mx,my);
  const ent=cpickEntityFromHit(hit);
  if(!ent){ if(typeof toast==="function") toast("Nothing under cursor","x"); return true; }
  // no duplicate identical picks (same object + kind + index)
  if(CPICK.picks.some(p=>p.objId===ent.objId&&p.kind===ent.kind&&p.i===ent.i)){
    if(typeof toast==="function") toast("Already picked that entity","x"); return true;
  }
  // ── SMART DIMENSION ── resolve the concrete dim type from what was clicked, then
  // fall through into the normal linear/diameter paths (Fusion/SolidWorks feel):
  //   • edge  → that edge's length (linear), placed immediately;
  //   • circle/face → diameter;
  //   • a vertex, then a second vertex → point-to-point distance (linear).
  // Angle-between-two-edges stays on the dedicated Angle tool, matching P1/P2 (a smart
  // edge click is unambiguously a length, so it never waits for a second edge).
  if(CPICK.type==="smart"){
    if(ent.kind==="line"){ CPICK.type="linear"; CPICK.need=2; }
    else if(ent.kind==="circle"){ CPICK.type="diameter"; CPICK.need=1; }
    else if(ent.kind==="point"){ CPICK.type="linear"; CPICK.need=2; }
    else { if(typeof toast==="function") toast("Click an edge, circle, or two points","x"); return true; }
  }
  // Fusion/SolidWorks feel: one edge → measure THAT edge. Expand the line into its two
  // endpoints and go straight to placing (two-point picking still works below). Applies
  // to every linear-family dim (linear / horizontal / vertical distance).
  if(CPICK_EDGE_DIM.has(CPICK.type)&&ent.kind==="line"&&CPICK.picks.length===0){
    const pair=cpickExpandLineEnts(ent);
    if(pair){ CPICK.picks=pair; cpickApply(e); return true; }
  }
  CPICK.picks.push(ent);
  if(CPICK.picks.length<CPICK.need){
    if(typeof toast==="function") toast(cpickHint(),"mouse-pointer-2");
    if(typeof vp3Draw==="function") vp3Draw();
    return true;
  }
  cpickApply(e);
  return true;
}

/* enough entities gathered → create the constraint immediately (P2's model). A
   dimensional constraint is STORED at its current measured value the moment the edge /
   second point is picked; the user edits the exact value afterward by double-clicking
   the on-canvas label or its outliner row (cpickPromptValue → setConstraintValue). No
   drag-to-place, no value box on the create path — that gesture is what dropped the
   dimension before it was ever stored. `place` is a default annotation offset (pure
   render metadata; the solver never reads it). */
function cpickApply(e){
  const type=CPICK.type, ents=CPICK.picks.slice();
  const isDim=(typeof CX_DIMENSIONAL!=="undefined")&&CX_DIMENSIONAL.has(type);
  let created=null;
  if(isDim){
    let val=cpickSeedValue(type,ents);           // current measured quantity (deg for angle)
    if(type==="angle") val=val*Math.PI/180;      // solver wants radians
    const place=cplaceDefault(type,ents);        // default annotation offset (render-only)
    if(typeof addConstraint==="function") created=addConstraint(type,ents,val,place);
  } else if(typeof addConstraint==="function"){
    created=addConstraint(type,ents);
  }
  // STAY ARMED after a good create (P2 leaves the dimension tool active): clear only
  // the gathered picks and keep active/type/need so the very next edge stores its OWN
  // dimension — this is what makes it per-edge instead of "one per object". A rejected
  // constraint (addConstraint returned null: unreachable / fights another) disarms
  // instead, so the user isn't left with a silently-armed tool after a failure.
  if(created){ CPICK.picks=[]; if(typeof toast==="function") toast(cpickHint(),"mouse-pointer-2"); if(typeof vp3Draw==="function") vp3Draw(); }
  else cpickCancel(true);
}

/* the default annotation offset a stored dimension gets (where its line sits relative
   to the measured entity). Pure render metadata on c.place; the solver never reads it.
   Linear rides just off the edge; radius/diameter/angle carry their own descriptor. */
function cplaceDefault(type,ents){
  if(type==="angle") return {r:32};
  if(type==="radius"||type==="diameter") return {off:32,ang:-Math.PI/2};
  return {off:6};   // linear — plane-space [u,v] units (model mm), NOT screen px
}

/* the measured quantity a dimension is created at (and the value box seeds with when
   editing). Length / |Δu| / |Δv| in plane [u,v]; angle returned in DEGREES. */
function cpickSeedValue(type,ents){
  const uvOf=(ent)=>{ const o=cxObj(ent.objId); return o&&o.cpts&&o.cpts[ent.i]?o.cpts[ent.i]:[0,0]; };
  if(type==="linear"){
    const p=uvOf(ents[0]), q=uvOf(ents[1]); return Math.hypot(p[0]-q[0],p[1]-q[1]);
  }
  if(type==="hdist"){ const p=uvOf(ents[0]), q=uvOf(ents[1]); return Math.abs(p[0]-q[0]); }
  if(type==="vdist"){ const p=uvOf(ents[0]), q=uvOf(ents[1]); return Math.abs(p[1]-q[1]); }
  if(type==="angle"){
    const o=cxObj(ents[0].objId), o2=cxObj(ents[1].objId);
    if(!o||!o2) return 90;
    const a=o.cpts[ents[0].i], b=o.cpts[(ents[0].i+1)%o.cpts.length];
    const p=o2.cpts[ents[1].i], q=o2.cpts[(ents[1].i+1)%o2.cpts.length];
    const d1=[b[0]-a[0],b[1]-a[1]], d2=[q[0]-p[0],q[1]-p[1]];
    return Math.abs(Math.atan2(d1[0]*d2[1]-d1[1]*d2[0],d1[0]*d2[0]+d1[1]*d2[1]))*180/Math.PI;
  }
  return 0;
}

/* ─── FLOATING VALUE INPUT ─── a tiny box near the cursor; Enter commits, Esc
   cancels. Reuses the .ctx-menu look so no new CSS is needed. `onDone(value|null)`
   receives the value in solver units (radians for angle). */
function cpickPromptValue(e,type,seed,onDone){
  cpickCloseValue();
  const box=document.createElement("div");
  box.className="ctx-menu open"; box.id="cpickValue";
  box.style.position="fixed"; box.style.padding="8px"; box.style.zIndex=400;
  const unit=(type==="angle")?"°":"mm";
  const row=document.createElement("div"); row.style.display="flex"; row.style.gap="6px"; row.style.alignItems="center";
  const lbl=document.createElement("span"); lbl.className="ctx-sec"; lbl.textContent=(CX_LABEL[type]||type);
  const inp=document.createElement("input"); inp.type="text"; inp.className="cmd-field-inp";
  inp.value=(Math.round((seed||0)*100)/100).toString(); inp.style.width="72px";
  const u=document.createElement("span"); u.className="cmd-field-unit"; u.textContent=unit;
  row.appendChild(lbl); row.appendChild(inp); row.appendChild(u);
  box.appendChild(row);
  document.body.appendChild(box);
  const vw=window.innerWidth, vh=window.innerHeight, r=box.getBoundingClientRect();
  box.style.left=Math.min((e?e.clientX:vw/2)+8,vw-r.width-8)+"px";
  box.style.top =Math.min((e?e.clientY:vh/2)+8,vh-r.height-8)+"px";
  // Focus the input so typing goes straight into it + Enter commits. A synchronous
  // focus() here is undone because this runs INSIDE the mouseup that dropped the
  // dimension — when that event finishes settling the browser returns focus to the
  // canvas/body, leaving the box unfocused (keystrokes then never reach it, so the
  // value never commits and the dimension is never stored). Defer focus to the next
  // tick, AFTER the drop event fully settles, and retry once on the following frame
  // so the box reliably owns the keyboard.
  const grab=()=>{ const b=document.getElementById("cpickValue"); if(b){ inp.focus(); inp.select(); } };
  setTimeout(grab,0);
  requestAnimationFrame(grab);
  // Enter, blur, and Esc can all race to finish the box; `done` makes commit/cancel
  // fire exactly once so a value is never applied twice (Enter then the resulting
  // blur, say) and onDone is called a single time.
  let done=false;
  const commit=()=>{
    if(done) return; done=true;
    let v=parseFloat(inp.value);
    cpickCloseValue();
    if(isNaN(v)){ onDone(null); return; }
    if(type==="angle") v=v*Math.PI/180;      // degrees → radians for the solver
    onDone(v);
  };
  const cancel=()=>{ if(done) return; done=true; cpickCloseValue(); onDone(null); };
  inp.onkeydown=(ev)=>{ ev.stopPropagation(); if(ev.key==="Enter") commit(); else if(ev.key==="Escape") cancel(); };
  // committing on blur too (click elsewhere) matches Fusion/SolidWorks — the box is a
  // transient value entry, not a modal; losing focus with a valid number applies it.
  inp.onblur=()=>{ commit(); };
  box._cancel=cancel;
}
function cpickCloseValue(){ const b=document.getElementById("cpickValue"); if(b) b.remove(); }

/* Esc anywhere cancels an armed tool or an open value box. */
window.addEventListener("keydown",e=>{
  if(e.key!=="Escape") return;
  const box=document.getElementById("cpickValue");
  if(box&&box._cancel){ box._cancel(); return; }
  if(CPICK.active) cpickCancel();
});
