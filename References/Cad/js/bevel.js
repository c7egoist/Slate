"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   bevel.js — Plasticity-style corner blend (fillet / chamfer) for line-curve
              sketches. Arm the tool, click a CORNER vertex shared by two
              segments, then drag: the distance from the corner sets the radius
              and the drag DIRECTION decides the blend — toward the interior
              (along the angle bisector) rounds it (fillet), away from it flattens
              it (chamfer). Release to bake the blend into the curve's control
              points.

     The prototype's curves are plane-space control-point polylines (obj.cpts)
     with NO arc entity, so a fillet is baked as a short tessellated arc of
     inserted control points; a chamfer is a single inserted segment. All blend
     math is 2D in plane space [u,v]; screen projection is only for hit-testing,
     the live preview, and translating the cursor drag into a radius.

     Line-category curves qualify directly; a rectangle or regular polygon is
     converted to an editable corner polyline (sketchPrimitiveToCurve) the moment
     you grab one of its corners, so the same cpts blend applies to it too. A
     circle is NOT blendable — its "corners" are just tessellation. A corner needs
     BOTH neighbours, so the endpoints of an OPEN curve are not blendable.
   ════════════════════════════════════════════════════════════════════════════ */

const BEVEL_SEG=12;                 // arc tessellation segments for a fillet
const BEVEL_COL="#34d399";          // preview colour (green, matches constraint glyphs)

/* live tool state. `blend` is the last computed {ok,mode,T1,T2,C,Reff,points}. */
const BEVEL={ active:false, obj:null, i:-1, R:0, mode:"fillet", blend:null, dragging:false };

/* ─── 2D PLANE-SPACE HELPERS ─── (V3 in mat4.js is 3D only) */
function bevelSub(a,b){ return [a[0]-b[0],a[1]-b[1]]; }
function bevelLen(a){ return Math.hypot(a[0],a[1]); }
function bevelDot(a,b){ return a[0]*b[0]+a[1]*b[1]; }
function bevelNorm(a){ const l=bevelLen(a)||1; return [a[0]/l,a[1]/l]; }

/* ─── BLEND GEOMETRY ─── compute the corner blend at cpts[i] for radius R. Pure;
   returns {ok:false} for any non-blendable corner. */
function bevelComputeBlend(obj,i,R,mode){
  if(!obj||obj.category!=="line"||!obj.cpts) return {ok:false};
  const cpts=obj.cpts, n=cpts.length;
  if(n<3) return {ok:false};
  if(!obj.closed&&(i===0||i===n-1)) return {ok:false};   // open-curve endpoint: one neighbour
  if(R<=1e-6) return {ok:false};
  const prev=(i-1+n)%n, next=(i+1)%n;
  const V=cpts[i], P=cpts[prev], Q=cpts[next];
  const e1=bevelSub(P,V), e2=bevelSub(Q,V);
  const L1=bevelLen(e1), L2=bevelLen(e2);
  if(L1<1e-6||L2<1e-6) return {ok:false};
  const d1=[e1[0]/L1,e1[1]/L1], d2=[e2[0]/L2,e2[1]/L2];
  let cosPhi=bevelDot(d1,d2); cosPhi=Math.max(-1,Math.min(1,cosPhi));
  const phi=Math.acos(cosPhi);                            // interior corner angle
  if(phi<1e-3||Math.PI-phi<1e-3) return {ok:false};       // collinear / spike
  const h=phi/2;
  const t=Math.min(R/Math.tan(h), 0.98*Math.min(L1,L2));  // setback, clamped to shorter edge
  const Reff=t*Math.tan(h);
  const T1=[V[0]+d1[0]*t, V[1]+d1[1]*t];
  const T2=[V[0]+d2[0]*t, V[1]+d2[1]*t];
  const bis=bevelNorm([d1[0]+d2[0], d1[1]+d2[1]]);        // into the corner
  const C=[V[0]+bis[0]*(Reff/Math.sin(h)), V[1]+bis[1]*(Reff/Math.sin(h))];
  let points;
  if(mode==="chamfer"){
    points=[T1,T2];
  } else {
    // short-way sweep from T1 to T2 about C, tessellated into inserted cpts
    const a1=Math.atan2(T1[1]-C[1],T1[0]-C[0]);
    const a2=Math.atan2(T2[1]-C[1],T2[0]-C[0]);
    let da=a2-a1; while(da<=-Math.PI) da+=2*Math.PI; while(da>Math.PI) da-=2*Math.PI;
    points=[T1];
    for(let k=1;k<BEVEL_SEG;k++){ const ang=a1+da*(k/BEVEL_SEG);
      points.push([C[0]+Reff*Math.cos(ang), C[1]+Reff*Math.sin(ang)]); }
    points.push(T2);
  }
  return {ok:true, mode, T1, T2, C, Reff, points};
}

/* ─── ARM ─── enter corner-blend picking (sketch imode, draw mode off). */
function bevelArm(){
  if(typeof setCadInteractionMode==="function") setCadInteractionMode("sketch",true);
  if(typeof sketchExitDrawMode==="function"&&typeof SKETCH!=="undefined"&&SKETCH.drawing) sketchExitDrawMode();
  BEVEL.active=true; BEVEL.obj=null; BEVEL.i=-1; BEVEL.blend=null; BEVEL.dragging=false; BEVEL.mode="fillet";
  if(typeof toast==="function") toast("Pick a corner, drag to bevel","spline");
  if(typeof vp3Draw==="function") vp3Draw();
  return true;
}

/* a corner is blendable if it's an editable line-curve corner, OR a straight-edged
   primitive (rectangle / polygon) corner — those convert to a line curve on demand
   so the same cpts blend math applies. A circle's "corners" are tessellation and
   are NOT blendable. */
function bevelCornerObj(obj){
  if(!obj) return null;
  if(obj.category==="line") return obj;
  if(typeof isPrimitiveCategory==="function"&&isPrimitiveCategory(obj.category)) return obj;
  return null;
}
/* resolve the corner {obj,i} under the cursor. Prefer the sub-object hit-test's
   vertex pass; fall back to the point-editor's anchor hit-test, since ssHitTest
   skips the anchors of the currently SELECTED curve (points.js owns those) — and
   the curve you want to bevel is usually the selected one. A rectangle / polygon
   corner is converted to an editable line curve here so cpts[i] exists. */
function bevelResolveCorner(mx,my){
  if(typeof ssHitTest==="function"){
    const hit=ssHitTest(mx,my);
    if(hit&&hit.kind==="vertex"&&bevelCornerObj(hit.obj)){
      const i=hit.i;
      if(hit.obj.category!=="line"&&typeof sketchPrimitiveToCurve==="function") sketchPrimitiveToCurve(hit.obj);
      return {obj:hit.obj,i};
    }
  }
  if(typeof ptEditable==="function"&&typeof ptHitTest==="function"){
    const obj=ptEditable();
    if(obj&&obj.category==="line"){ const a=ptHitTest(mx,my); if(a&&a.part==="anchor") return {obj,i:a.i}; }
  }
  return null;
}
/* ─── MOUSEDOWN ─── slotted into vp3AttachInput before ptMouseDown. A corner hit
   starts the drag; returns true to consume the click. */
function bevelMouseDown(e,mx,my){
  if(BEVEL.dragging&&e.button===0) return true;           // guard: already dragging
  if(!BEVEL.active||e.button!==0) return false;
  const corner=bevelResolveCorner(mx,my);
  if(!corner){ if(typeof toast==="function") toast("Pick a corner of a line","x"); return true; }
  const probe=bevelComputeBlend(corner.obj,corner.i,1,BEVEL.mode||"fillet");
  if(!probe.ok){ if(typeof toast==="function") toast("Corner needs two edges","x"); return true; }
  BEVEL.obj=corner.obj; BEVEL.i=corner.i; BEVEL.dragging=true;
  BEVEL.R=probe.Reff||1; BEVEL.blend=probe;
  if(typeof vp3Draw==="function") vp3Draw();
  return true;
}

/* ─── DRAG ─── cursor → radius + fillet/chamfer decision. Own window listener
   (the viewport mousemove only fires during an orbit/pan drag). */
function bevelMove(mx,my){
  if(!BEVEL.dragging||!BEVEL.obj) return;
  const obj=BEVEL.obj, i=BEVEL.i, cpts=obj.cpts, n=cpts.length;
  const world=(typeof vp3ScreenToPlane==="function")
    ? vp3ScreenToPlane(mx,my,obj.plane.point,obj.plane.normal,false) : null;
  if(!world) return;                                       // keep prior R/mode
  const cuv=sketchPlaneCoords(obj.plane,world);
  const V=cpts[i], prev=(i-1+n)%n, next=(i+1)%n;
  const d1=bevelNorm(bevelSub(cpts[prev],V)), d2=bevelNorm(bevelSub(cpts[next],V));
  const bis=bevelNorm([d1[0]+d2[0], d1[1]+d2[1]]);
  const drag=bevelSub(cuv,V);
  BEVEL.R=Math.max(bevelLen(drag),1e-3);
  BEVEL.mode=(bevelDot(bevelNorm(drag),bis)>0)?"fillet":"chamfer";   // toward interior = round
  BEVEL.blend=bevelComputeBlend(obj,i,BEVEL.R,BEVEL.mode);
  if(typeof vp3Draw==="function") vp3Draw();
}

/* ─── COMMIT ─── bake the blend into the curve's control points. Direct geometry
   edit (like a draw commit): the splice shifts cpts indices, so we do NOT run the
   constraint solve-after-drag (index-based constraint refs would go stale). */
function bevelCommit(){
  if(!BEVEL.dragging) return;
  const obj=BEVEL.obj, i=BEVEL.i, blend=BEVEL.blend, mode=BEVEL.mode;
  const Reff=blend&&blend.Reff||0;
  BEVEL.dragging=false;
  if(!obj||!blend||!blend.ok||!blend.points.length){ BEVEL.blend=null; if(typeof vp3Draw==="function") vp3Draw(); return; }
  const inserted=blend.points.map(p=>[p[0],p[1]]);
  obj.cpts.splice(i,1,...inserted);
  if(typeof sketchRebuildCurve==="function") sketchRebuildCurve(obj);
  // mark the INTERIOR arc points (everything but the two tangent endpoints T1/T2) as
  // baked tessellation so the point editor draws just the arc, not a scatter of
  // draggable anchors. Indices shift by the splice, so rebuild the set from scratch.
  bevelMarkArcPoints(obj,i,inserted.length,mode);
  bevelRegister(obj,mode,Reff);
  if(typeof toast==="function") toast((mode==="fillet"?"Fillet":"Chamfer")+" applied","check");
  BEVEL.obj=null; BEVEL.i=-1; BEVEL.blend=null;
  if(typeof vp3Draw==="function") vp3Draw();
}

/* rebuild obj.arcPts (a Set of cpt indices that are baked arc tessellation, not
   hand-editable anchors). A fillet inserts T1 + interior arc pts + T2 at [i .. i+len-1];
   the interior ones (all but the first + last) are marked. A chamfer is just [T1,T2]
   — both are real corners, nothing to hide. Existing marks at indices ≥ i shift by
   the splice delta (len-1). */
function bevelMarkArcPoints(obj,i,len,mode){
  const delta=len-1;
  const next=new Set();
  if(obj.arcPts) for(const idx of obj.arcPts) next.add(idx<i?idx:idx+delta);
  if(mode==="fillet") for(let k=1;k<len-1;k++) next.add(i+k);   // interior arc pts only
  obj.arcPts=next;
}

/* register the blend as a real modify feature: timeline instance (Fillet/Chamfer
   with its measured radius), history event, and a properties refresh. Mirrors
   sketchRegister's timeline/history steps so the operation shows up like any other. */
function bevelRegister(obj,mode,Reff){
  const type=(mode==="fillet")?"fillet":"chamfer";
  const val=Math.round(Reff*100)/100;
  if(typeof CAD_TIMELINE!=="undefined"&&typeof cadFeatInstance==="function"&&typeof CAD_FEAT_BY_TYPE!=="undefined"&&CAD_FEAT_BY_TYPE[type]){
    const inst=cadFeatInstance(type); inst.skId=obj.id;
    if(inst.values){ if(type==="fillet") inst.values.radius=val; else inst.values.distance=val; }
    const at=(typeof cadRollback==="number")?Math.min(cadRollback,CAD_TIMELINE.length):CAD_TIMELINE.length;
    CAD_TIMELINE.splice(at,0,inst); if(typeof cadRollback==="number") cadRollback=at+1;
    if(typeof cadRightPane!=="undefined"&&cadRightPane==="features"&&typeof renderCadFeatures==="function") renderCadFeatures();
  }
  if(typeof pushHistory==="function")
    pushHistory(type==="fillet"?"fillet":"chamfer",obj.name,(mode==="fillet"?"Fillet R":"Chamfer ")+val+" mm");
  if(typeof renderProps==="function") renderProps();
}

/* ─── CANCEL ─── drop the whole armed tool (Esc) or an in-flight drag. */
function bevelCancel(silent){
  const was=BEVEL.active||BEVEL.dragging;
  BEVEL.active=false; BEVEL.dragging=false; BEVEL.obj=null; BEVEL.i=-1; BEVEL.blend=null;
  if(was&&!silent&&typeof toast==="function") toast("Cancelled","x");
  if(typeof vp3Draw==="function") vp3Draw();
}

/* ─── PREVIEW ─── dashed blend polyline + a value chip, drawn while dragging. */
function bevelDrawPreview(g,mvp){
  if(!BEVEL.dragging||!BEVEL.blend||!BEVEL.blend.ok) return;
  const obj=BEVEL.obj, pts=BEVEL.blend.points;
  const scr=pts.map(p=>{ const w=sketchPlanePoint(obj.plane,p[0],p[1]); return vp3World2Screen(mvp,w); });
  if(scr.some(s=>!s)) return;
  g.save();
  // just the blend arc/segment — a clean solid stroke, no tangent dots (the user
  // wants to see the arc only, not a scatter of control-point markers).
  g.strokeStyle=BEVEL_COL; g.lineWidth=2;
  g.beginPath(); g.moveTo(scr[0][0],scr[0][1]);
  for(let k=1;k<scr.length;k++) g.lineTo(scr[k][0],scr[k][1]);
  g.stroke();
  // value chip at the blend midpoint
  const mid=scr[Math.floor(scr.length/2)];
  const val=Math.round(BEVEL.blend.Reff*100)/100;
  const text=(BEVEL.mode==="fillet"?"Fillet R":"Chamfer ")+val+" mm";
  if(typeof cxDrawLabelChip==="function") cxDrawLabelChip(g,mid[0],mid[1]-18,text,BEVEL_COL);
  g.restore();
}

/* Esc anywhere cancels the armed tool / in-flight drag. */
window.addEventListener("keydown",e=>{
  if(e.key!=="Escape") return;
  if(BEVEL.active||BEVEL.dragging) bevelCancel(false);
});
/* the corner blend follows the cursor while dragging (own listener — the
   viewport's mousemove only fires during an orbit/pan drag). */
window.addEventListener("mousemove",e=>{
  if(!BEVEL.dragging) return;
  const stage=document.getElementById("stage"); if(!stage) return;
  const r=stage.getBoundingClientRect();
  bevelMove(e.clientX-r.left,e.clientY-r.top);
});
/* release bakes the blend. */
window.addEventListener("mouseup",e=>{
  if(BEVEL.dragging&&e.button===0) bevelCommit();
});
