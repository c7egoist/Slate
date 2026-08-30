"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   points.js — editable control-point / handle overlay for the curve sketches
               (line / bezier / b-spline / nurbs). When a curve object is the
               current selection it draws, on top of the viewport:

                 • the control polygon (thin grey line through the anchors),
                 • bezier tangent handle lines + hollow handle dots,
                 • filled anchor dots,
                 • the selected / hovered point highlighted amber.

               Points and handles are pickable and draggable. The drag lifecycle
               mirrors the transform gizmo (snapshot on down, delta on move, commit
               on up) but writes into the curve's plane-space control data and
               rebuilds the sampled world polyline through sketchRebuildCurve().

               Ctrl held during a point drag snaps to the grid (like the draw tools).
   ════════════════════════════════════════════════════════════════════════════ */

/* per-element colours — anchors, handles, control polygon, and the amber hot state */
const PT_COLORS={ anchor:"#ffffff", handle:"#22d3ee", poly:"#5a5a5a", hot:"#f59e0b" };
const PT_TOL=8;                 // screen-space pick radius (px)

const PT={ obj:null, sel:null, hover:null, drag:null };

/* the curve object we should be editing right now (selected + a curve category) */
function ptEditable(){
  const obj=(typeof sketchSelectedObject==="function")?sketchSelectedObject():null;
  if(!obj||typeof isCurveCategory!=="function"||!isCurveCategory(obj.category)) return null;
  return obj;
}

/* ─── DRAW ─── (hooked into vp3Draw after the transform gizmo) */
function vp3DrawControlPoints(g,mvp){
  const obj=ptEditable(); PT.obj=obj;
  if(!obj) return;
  const {anchors,handles}=sketchWorldCpts(obj);
  const sa=anchors.map(p=>vp3World2Screen(mvp,p));
  // control polygon
  g.save();
  g.strokeStyle=PT_COLORS.poly; g.lineWidth=1; g.setLineDash([4,4]);
  g.beginPath(); let started=false;
  for(const s of sa){ if(!s) continue; if(!started){ g.moveTo(s[0],s[1]); started=true; } else g.lineTo(s[0],s[1]); }
  g.stroke(); g.setLineDash([]);
  // bezier handle lines + hollow handle dots
  if(obj.category==="bezier"){
    for(const i in handles){
      const a=sa[i]; if(!a) continue;
      const hin=vp3World2Screen(mvp,handles[i][0]), hout=vp3World2Screen(mvp,handles[i][1]);
      g.strokeStyle=PT_COLORS.handle; g.lineWidth=1;
      for(const h of [hin,hout]){
        if(!h) continue;
        g.beginPath(); g.moveTo(a[0],a[1]); g.lineTo(h[0],h[1]); g.stroke();
      }
      const hotIn=ptIsHot(+i,"in"), hotOut=ptIsHot(+i,"out");
      if(hin) ptRing(g,hin,hotIn?PT_COLORS.hot:PT_COLORS.handle);
      if(hout) ptRing(g,hout,hotOut?PT_COLORS.hot:PT_COLORS.handle);
    }
  }
  // anchor dots — skip baked arc-tessellation points (obj.arcPts) so a filleted
  // corner reads as a smooth arc, not a scatter of markers.
  for(let i=0;i<sa.length;i++){
    const s=sa[i]; if(!s) continue;
    if(obj.arcPts&&obj.arcPts.has(i)) continue;
    const hot=ptIsHot(i,"anchor");
    g.fillStyle=hot?PT_COLORS.hot:PT_COLORS.anchor;
    g.beginPath(); g.arc(s[0],s[1],hot?5:4,0,7); g.fill();
    g.strokeStyle="rgba(0,0,0,0.5)"; g.lineWidth=1; g.stroke();
  }
  g.restore();
}
function ptRing(g,s,col){
  g.fillStyle="#0b0d11"; g.beginPath(); g.arc(s[0],s[1],4,0,7); g.fill();
  g.strokeStyle=col; g.lineWidth=1.5; g.beginPath(); g.arc(s[0],s[1],4,0,7); g.stroke();
}
function ptIsHot(i,part){
  const a=PT.drag||PT.sel||PT.hover;
  return !!a && a.i===i && a.part===part;
}

/* ─── HIT-TEST ─── handles first (they sit on top), then anchors. */
function ptHitTest(mx,my){
  const obj=PT.obj||ptEditable(); if(!obj) return null;
  const {anchors,handles}=sketchWorldCpts(obj);
  const near=(w)=>{ const s=vp3World2Screen(VP3.mvp,w); return s&&((mx-s[0])*(mx-s[0])+(my-s[1])*(my-s[1]))<=PT_TOL*PT_TOL; };
  if(obj.category==="bezier"){
    for(const i in handles){
      if(near(handles[i][1])) return {i:+i,part:"out"};
      if(near(handles[i][0])) return {i:+i,part:"in"};
    }
  }
  for(let i=0;i<anchors.length;i++){
    if(obj.arcPts&&obj.arcPts.has(i)) continue;   // baked arc points aren't editable anchors
    if(near(anchors[i])) return {i,part:"anchor"};
  }
  return null;
}

/* ─── DRAG ─── (called first in vp3AttachInput when a curve is selected) */
function ptMouseDown(e,mx,my){
  if(e.button!==0) return false;
  const obj=ptEditable(); if(!obj) return false;
  if(typeof SKETCH!=="undefined"&&SKETCH.drawing) return false;   // draw-mode owns clicks
  const hit=ptHitTest(mx,my);
  if(!hit){ if(PT.sel){ PT.sel=null; vp3Draw(); } return false; }  // let empty click fall through
  PT.obj=obj; PT.sel=hit;
  PT.drag={ i:hit.i, part:hit.part, sx:mx, sy:my,
            base:ptPlaneValue(obj,hit) };
  if(typeof renderProps==="function") renderProps();
  vp3Draw();
  return true;
}
/* read the plane-space [u,v] for the dragged element */
function ptPlaneValue(obj,hit){
  if(hit.part==="anchor") return obj.cpts[hit.i].slice();
  const h=obj.handles&&obj.handles[hit.i];
  return h?(hit.part==="in"?h[0].slice():h[1].slice()):obj.cpts[hit.i].slice();
}
function ptDragMove(mx,my,snap){
  if(!PT.drag||!PT.obj) return;
  const obj=PT.obj;
  const p=vp3ScreenToPlane(mx,my,obj.plane.point,obj.plane.normal,snap);
  if(!p) return;
  const uv=sketchPlaneCoords(obj.plane,p);
  const d=PT.drag;
  if(d.part==="anchor"){
    // move the anchor and carry its handles by the same delta (bezier)
    const du=uv[0]-obj.cpts[d.i][0], dv=uv[1]-obj.cpts[d.i][1];
    obj.cpts[d.i]=uv;
    if(obj.handles&&obj.handles[d.i]){
      obj.handles[d.i][0]=[obj.handles[d.i][0][0]+du,obj.handles[d.i][0][1]+dv];
      obj.handles[d.i][1]=[obj.handles[d.i][1][0]+du,obj.handles[d.i][1][1]+dv];
    }
  } else if(obj.handles&&obj.handles[d.i]){
    const slot=d.part==="in"?0:1;
    obj.handles[d.i][slot]=uv;
    // mirror the opposite handle around the anchor for smoothness
    const a=obj.cpts[d.i], other=slot===0?1:0;
    obj.handles[d.i][other]=[2*a[0]-uv[0],2*a[1]-uv[1]];
  }
  sketchRebuildCurve(obj);
  vp3Draw();
}
function ptDragUp(){
  if(!PT.drag) return;
  const obj=PT.obj, drag=PT.drag; PT.drag=null;
  // re-solve constraints, pinning the anchor the user just dragged so the rest moves.
  if(obj&&drag.part==="anchor"&&typeof solveAfterDrag==="function") solveAfterDrag([obj.id+"#"+drag.i]);
  if(obj&&typeof pushHistory==="function") pushHistory("sketch",obj.name,"moved control point");
  // dragged an OPEN curve's endpoint anchor? offer to join it to the other end.
  if(obj&&drag.part==="anchor"&&!obj.closed&&obj.cpts.length>=3&&
     (drag.i===0||drag.i===obj.cpts.length-1)&&typeof joinMaybe==="function")
    joinMaybe(obj,drag.i);
  if(typeof renderProps==="function") renderProps();
  vp3Draw();
}

/* ─── window listeners: hover + drag ─── */
window.addEventListener("mousemove",e=>{
  const stage=document.getElementById("stage"); if(!stage) return;
  const r=stage.getBoundingClientRect(), mx=e.clientX-r.left, my=e.clientY-r.top;
  if(PT.drag){ ptDragMove(mx,my,e.ctrlKey?true:undefined); return; }
  if(ptEditable()){
    const h=ptHitTest(mx,my);
    const changed=(!!h!==!!PT.hover)||(h&&PT.hover&&(h.i!==PT.hover.i||h.part!==PT.hover.part));
    if(changed){ PT.hover=h; vp3Draw(); }
  } else if(PT.hover){ PT.hover=null; }
});
window.addEventListener("mouseup",()=>{ if(PT.drag) ptDragUp(); });
