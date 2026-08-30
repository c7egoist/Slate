"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   subselect.js — Plasticity-style auto-detect sub-object selection for the sketch
                  environment. NO element-type toggling: in Sketch mode you hover
                  and the nearest element auto-highlights by proximity priority —

                    vertex (corner / control point) → edge (segment) → face (interior)

                  Click selects that element; drag moves it. Works on curves
                  (line / bezier / bspline / nurbs) AND primitives (rectangle /
                  polygon). Vertices are draggable on all; edges translate; a face
                  selects the whole object (translation stays with the gizmo).

                  OWNERSHIP: points.js owns the SELECTED curve's anchors + bezier
                  handles (its ptMouseDown runs first). subselect owns everything
                  else — edges + faces, plus vertices of primitives and non-selected
                  curves. The vertex pass skips the ptEditable() object's anchors so
                  the two never fight over the same dot.

                  The drag lifecycle mirrors points.js / the transform gizmo
                  (snapshot on down, delta on move, commit + history on up).
   ════════════════════════════════════════════════════════════════════════════ */

const SS_VTOL=9;                 // vertex pick radius (px)
const SS_ETOL=6;                 // edge pick tolerance (px)
const SS={ hover:null, drag:null };   // element = {obj, kind:'vertex'|'edge'|'face', i}

/* are we in a state where sub-object picking is live? (Sketch mode, not drawing) */
function ssActive(){
  return typeof CAD!=="undefined"&&CAD.imode==="sketch"&&
         !(typeof SKETCH!=="undefined"&&SKETCH.drawing);
}
/* the object points.js is currently editing (whose anchors it owns) — skip those. */
function ssPointsOwner(){
  return (typeof ptEditable==="function")?ptEditable():null;
}
/* a sketch object's world vertices: curve anchors, or the transformed primitive
   corners. These are the draggable "vertices" for each category. */
function ssVerts(obj){
  if(obj.category==="region") return [];   // regions are derived — no editable verts
  if(typeof isCurveCategory==="function"&&isCurveCategory(obj.category))
    return sketchWorldCpts(obj).anchors;
  return sketchApplyTransform(obj);
}
/* the world points that form the pickable EDGE segments — the control polygon for
   curves (so a hit maps to cpts[i]/cpts[i+1]), the corners for primitives. */
function ssEdgePoints(obj){ return ssVerts(obj); }

/* ─── HIT-TEST ─── three passes so a near vertex always beats a farther edge. */
function ssHitTest(mx,my){
  if(typeof SKETCHES==="undefined") return null;
  const owner=ssPointsOwner();
  // topmost (last drawn) object first
  const order=[];
  for(let i=SKETCHES.length-1;i>=0;i--){
    const obj=SKETCHES[i];
    const node=obj.nodeId?findItem(obj.nodeId):null;
    if(node&&node.visible===false) continue;
    order.push(obj);
  }
  // pass 1: vertices (skip the points.js-owned object's anchors)
  for(const obj of order){
    if(obj===owner) continue;
    const vs=ssVerts(obj);
    for(let k=0;k<vs.length;k++){
      const s=vp3World2Screen(VP3.mvp,vs[k]); if(!s) continue;
      if((mx-s[0])*(mx-s[0])+(my-s[1])*(my-s[1])<=SS_VTOL*SS_VTOL)
        return {obj,kind:"vertex",i:k};
    }
  }
  // pass 2: edges (control polygon / primitive corners)
  for(const obj of order){
    const ep=ssEdgePoints(obj);
    const sp=ep.map(p=>vp3World2Screen(VP3.mvp,p));
    const curve=typeof isCurveCategory==="function"&&isCurveCategory(obj.category);
    const n=sp.length;
    // curves: open polyline (segments 0..n-2, plus the closing seg when closed);
    // primitives: closed loop (wrap the last→first segment).
    const last=curve?(obj.closed?n:n-1):n;
    for(let k=0;k<last;k++){
      const a=sp[k], b=sp[(k+1)%n]; if(!a||!b) continue;
      if(gizmoNearSeg(mx,my,a,b,SS_ETOL)) return {obj,kind:"edge",i:k};
    }
  }
  // pass 3: faces (interior of a closed shape)
  for(const obj of order){
    if(!obj.closed) continue;
    const sp=sketchApplyTransform(obj).map(p=>vp3World2Screen(VP3.mvp,p));
    if(sp.some(p=>!p)||sp.length<3) continue;
    if(sketchPointInPoly(mx,my,sp)) return {obj,kind:"face",i:-1};
  }
  return null;
}

/* ─── DRAW ─── the hovered element highlight (hooked in vp3Draw after the curve
   control-point overlay). Amber ring / thick amber edge / faint face tint. */
function ssDrawHover(g,mvp){
  if(!ssActive()||!SS.hover||SS.drag) return;
  const h=SS.hover, obj=h.obj;
  g.save();
  if(h.kind==="vertex"){
    const vs=ssVerts(obj), s=vp3World2Screen(mvp,vs[h.i]);
    if(s){ g.strokeStyle="#f59e0b"; g.lineWidth=2; g.beginPath(); g.arc(s[0],s[1],7,0,7); g.stroke(); }
  } else if(h.kind==="edge"){
    const ep=ssEdgePoints(obj), n=ep.length;
    const a=vp3World2Screen(mvp,ep[h.i]), b=vp3World2Screen(mvp,ep[(h.i+1)%n]);
    if(a&&b){ g.strokeStyle="#f59e0b"; g.lineWidth=3; g.globalAlpha=0.9;
      g.beginPath(); g.moveTo(a[0],a[1]); g.lineTo(b[0],b[1]); g.stroke(); g.globalAlpha=1; }
  } else if(h.kind==="face"){
    const sp=sketchApplyTransform(obj).map(p=>vp3World2Screen(mvp,p));
    if(!sp.some(p=>!p)&&sp.length>=3){
      g.globalAlpha=0.10; g.fillStyle=(obj.colors&&obj.colors.fill)||"#38bdf8";
      g.beginPath(); g.moveTo(sp[0][0],sp[0][1]);
      for(let i=1;i<sp.length;i++) g.lineTo(sp[i][0],sp[i][1]);
      g.closePath(); g.fill(); g.globalAlpha=1;
    }
  }
  g.restore();
}

/* ─── GRAB ─── snapshot the base geometry so a drag can apply a clean delta. */
function ssGrabBase(obj,kind,i){
  if(obj.category==="region") return null;   // regions have no cpts/origin/center to drag
  const curve=typeof isCurveCategory==="function"&&isCurveCategory(obj.category);
  if(curve){
    if(kind==="vertex") return {cpt:obj.cpts[i].slice(), handles:ssCloneHandle(obj,i)};
    if(kind==="edge") return {a:obj.cpts[i].slice(), b:obj.cpts[i+1].slice(),
                              ha:ssCloneHandle(obj,i), hb:ssCloneHandle(obj,i+1), ai:i, bi:i+1};
  } else if(obj.category==="rectangle"){
    return {origin:obj.origin.slice(), w:obj.w, h:obj.h};
  } else {  // polygon / circle
    return {center:obj.center.slice(), rim:obj.rim.slice(), sides:obj.sides};
  }
  return {};
}
function ssCloneHandle(obj,i){
  if(!obj.handles||!obj.handles[i]) return null;
  return [obj.handles[i][0].slice(),obj.handles[i][1].slice()];
}

/* ─── MOUSEDOWN HOOK ─── slotted into vp3AttachInput after ptMouseDown, before the
   orientation gizmo. MUST return false on no-hit so orbit/pan still works. */
function ssMouseDown(e,mx,my){
  if(e.button!==0||!ssActive()) return false;
  const hit=ssHitTest(mx,my); if(!hit) return false;
  const additive=!!(e.shiftKey||e.ctrlKey||e.metaKey);
  if(typeof selectItem==="function") selectItem(hit.obj.nodeId, additive);
  // whole-object select (face) OR an additive multi-pick never starts a vertex/edge
  // drag — it just adds to the selection (so booleans can gather shapes).
  if(hit.kind==="face"||additive){ vp3Draw(); return true; }
  const p=vp3ScreenToPlane(mx,my,hit.obj.plane.point,hit.obj.plane.normal,e.ctrlKey?true:undefined);
  const baseUV=p?sketchPlaneCoords(hit.obj.plane,p):(hit.obj.cpts?hit.obj.cpts[hit.i].slice():[0,0]);
  SS.drag={ obj:hit.obj, kind:hit.kind, i:hit.i, plane:hit.obj.plane,
            baseUV, base:ssGrabBase(hit.obj,hit.kind,hit.i) };
  SS.hover=null;
  if(typeof renderProps==="function") renderProps();
  vp3Draw();
  return true;
}

/* ─── DRAG MOVE ─── uses the OBJECT'S OWN plane (never SKETCH.plane). */
function ssDragMove(mx,my,snap){
  const d=SS.drag; if(!d) return;
  const obj=d.obj;
  const p=vp3ScreenToPlane(mx,my,d.plane.point,d.plane.normal,snap); if(!p) return;
  const uv=sketchPlaneCoords(d.plane,p);
  const du=uv[0]-d.baseUV[0], dv=uv[1]-d.baseUV[1];
  const curve=typeof isCurveCategory==="function"&&isCurveCategory(obj.category);
  if(curve&&d.kind==="vertex"){
    obj.cpts[d.i]=[d.base.cpt[0]+du,d.base.cpt[1]+dv];
    if(d.base.handles&&obj.handles) obj.handles[d.i]=ssShiftHandle(d.base.handles,du,dv);
    sketchRebuildCurve(obj);
  } else if(curve&&d.kind==="edge"){
    obj.cpts[d.base.ai]=[d.base.a[0]+du,d.base.a[1]+dv];
    obj.cpts[d.base.bi]=[d.base.b[0]+du,d.base.b[1]+dv];
    if(obj.handles){
      if(d.base.ha) obj.handles[d.base.ai]=ssShiftHandle(d.base.ha,du,dv);
      if(d.base.hb) obj.handles[d.base.bi]=ssShiftHandle(d.base.hb,du,dv);
    }
    sketchRebuildCurve(obj);
  } else if(obj.category==="rectangle"){
    ssRectCorner(obj,d.base,d.i,uv);
  } else if(d.kind==="vertex"){                 // polygon corner: radius + start angle
    obj.rim=sketchPlanePoint(obj.plane,uv[0],uv[1]);
    sketchRebuildPoly(obj);
  } else {                                       // polygon edge: translate whole shape
    const cuv=sketchPlaneCoords(obj.plane,d.base.center), ruv=sketchPlaneCoords(obj.plane,d.base.rim);
    obj.center=sketchPlanePoint(obj.plane,cuv[0]+du,cuv[1]+dv);
    obj.rim=sketchPlanePoint(obj.plane,ruv[0]+du,ruv[1]+dv);
    sketchRebuildPoly(obj);
  }
  vp3Draw();
}
function ssShiftHandle(h,du,dv){
  return [[h[0][0]+du,h[0][1]+dv],[h[1][0]+du,h[1][1]+dv]];
}
/* rectangle corner drag: the diagonally-opposite corner stays anchored; recompute
   the plane-space origin + signed width/height from it and the dropped corner.
   corners order: 0=origin, 1=(u0+w,v0), 2=(u0+w,v0+h), 3=(u0,v0+h). */
function ssRectCorner(obj,base,corner,uv){
  const o=base.origin, w=base.w, h=base.h;
  const cx=[o[0], o[0]+w, o[0]+w, o[0]][corner];   // dragged corner's current u
  const cy=[o[1], o[1], o[1]+h, o[1]+h][corner];   // (unused, documents layout)
  void cx; void cy;
  const opp=(corner+2)%4;                          // anchored (opposite) corner
  const ax=[o[0], o[0]+w, o[0]+w, o[0]][opp];
  const ay=[o[1], o[1], o[1]+h, o[1]+h][opp];
  const u0=Math.min(ax,uv[0]), v0=Math.min(ay,uv[1]);
  obj.origin=[u0,v0];
  obj.w=Math.max(0.05,Math.abs(uv[0]-ax));
  obj.h=Math.max(0.05,Math.abs(uv[1]-ay));
  sketchRebuildRect(obj);
}

/* ─── DRAG UP ─── commit + history (mirrors ptDragUp). */
function ssDragUp(){
  const d=SS.drag; if(!d) return;
  const obj=d.obj; SS.drag=null;
  // re-solve: pin the anchors the user just moved so constraints pull the rest.
  if(typeof solveAfterDrag==="function"&&typeof isCurveCategory==="function"&&isCurveCategory(obj.category)){
    const pins=[];
    if(d.kind==="vertex") pins.push(obj.id+"#"+d.i);
    else if(d.kind==="edge"){ pins.push(obj.id+"#"+d.base.ai,obj.id+"#"+d.base.bi); }
    if(pins.length) solveAfterDrag(pins);
  }
  if(obj&&typeof pushHistory==="function")
    pushHistory("sketch",obj.name,d.kind==="edge"?"moved edge":"moved vertex");
  if(typeof renderProps==="function") renderProps();
  vp3Draw();
}

/* ─── window listeners: hover + drag ─── */
window.addEventListener("mousemove",e=>{
  const stage=document.getElementById("stage"); if(!stage) return;
  const r=stage.getBoundingClientRect(), mx=e.clientX-r.left, my=e.clientY-r.top;
  if(SS.drag){ ssDragMove(mx,my,e.ctrlKey?true:undefined); return; }
  if(ssActive()){
    const h=ssHitTest(mx,my);
    const changed=(!!h!==!!SS.hover)||(h&&SS.hover&&(h.obj!==SS.hover.obj||h.kind!==SS.hover.kind||h.i!==SS.hover.i));
    if(changed){ SS.hover=h; vp3Draw(); }
  } else if(SS.hover){ SS.hover=null; vp3Draw(); }
});
window.addEventListener("mouseup",()=>{ if(SS.drag) ssDragUp(); });
