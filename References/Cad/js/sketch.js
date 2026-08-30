"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   sketch.js — the sketch draw tools: a plane-bound Rectangle/Square tool
               (click-drag corner box) and a radius-based regular Polygon tool
               (click centre, drag radius, N sides). Owns:

                 • the SKETCH draw-mode state machine (enter / mouse / commit),
                 • the geometry builders (rect + N-gon on an arbitrary plane),
                 • the viewport render pass (points / lines / translucent closed
                   fill — Plasticity-style three-colour readout),
                 • registration of a committed sketch into the outliner tree, the
                   Features timeline, and the History log.

               Each sketch is one OBJECT (a polygon selects as a whole, never per
               edge/vertex). Points are stored on the draw plane; a per-object
               transform is layered at render time so the gizmo mutates the
               transform, not the points.
   ════════════════════════════════════════════════════════════════════════════ */

/* Plasticity-like readout colours */
const SK_COLORS={ point:"#ffffff", line:"#e2e8f0", fill:"#38bdf8" };

/* per-category stroke + outliner colour — each shape type reads distinctly. */
const CAT_COLORS={
  line:"#e2e8f0", bezier:"#a855f7", bspline:"#3b82f6", nurbs:"#14b8a6",
  circle:"#38bdf8", rectangle:"#22c55e", polygon:"#f472b6",
};
/* the curve categories (open control-point curves), vs the closed primitives */
const CURVE_CATS=["line","bezier","bspline","nurbs"];
const CURVE_TOOLS=["line","bezier","bspline","nurbs"];   // tool ids that draw curves
function isCurveCategory(c){ return CURVE_CATS.indexOf(c)>=0; }

/* draw-mode state — `drawing` gates the viewport mousedown so we don't orbit.
   Curve tools accumulate `cpts` (plane-space) while `stage==="placing"`. */
const SKETCH={
  drawing:false, tool:null, sides:6, square:false, degree:3,
  plane:{ point:[0,0,0], normal:[0,1,0], name:"Top Plane" },   // default: ground y=0
  stage:"idle", p0:null, p1:null, preview:null,
  cpts:[], hoverPt:null,                                       // curve placement
  snapClose:false, snapScreen:null,                           // endpoint-snap-to-close
};
/* screen-space radius (px) for snapping the drawing cursor onto the first control
   point to close a curve into a loop. */
const SK_SNAP_PX=10;

/* pretty label for an N-gon (used in the cmd-bar + toasts) */
const SK_POLY_NAMES={3:"Triangle",4:"Quad",5:"Pentagon",6:"Hexagon",7:"Heptagon",8:"Octagon",9:"Nonagon",10:"Decagon"};
function sketchPolyName(n){ return SK_POLY_NAMES[n]||(n+"-gon"); }

/* ─── PLANE BASIS ─── an orthonormal (u,v) pair spanning the draw plane. */
function sketchPlaneBasis(plane){
  const n=V3.normalize(plane.normal);
  let ref=Math.abs(n[1])>0.9?[1,0,0]:[0,1,0];      // avoid parallel ref
  const u=V3.normalize(V3.cross(ref,n));
  const v=V3.cross(n,u);
  return {u,v,n};
}
function sketchPlanePoint(plane,su,sv){
  const {u,v}=sketchPlaneBasis(plane);
  return [plane.point[0]+u[0]*su+v[0]*sv,
          plane.point[1]+u[1]*su+v[1]*sv,
          plane.point[2]+u[2]*su+v[2]*sv];
}
/* project a world point onto the plane's (u,v) coordinates */
function sketchPlaneCoords(plane,p){
  const {u,v}=sketchPlaneBasis(plane);
  const d=V3.sub(p,plane.point);
  return [V3.dot(d,u), V3.dot(d,v)];
}

/* ─── GEOMETRY BUILDERS ─── */
/* corner box from p0→p1 (world points on the plane). square forces equal extent. */
function sketchRectPoints(p0,p1,plane,square){
  const [u0,v0]=sketchPlaneCoords(plane,p0);
  let [u1,v1]=sketchPlaneCoords(plane,p1);
  if(square){
    const du=u1-u0, dv=v1-v0, s=Math.max(Math.abs(du),Math.abs(dv));
    u1=u0+Math.sign(du||1)*s; v1=v0+Math.sign(dv||1)*s;
  }
  return [ sketchPlanePoint(plane,u0,v0), sketchPlanePoint(plane,u1,v0),
           sketchPlanePoint(plane,u1,v1), sketchPlanePoint(plane,u0,v1) ];
}
/* regular N-gon: centre + a rim point set the radius and the start angle. */
function sketchPolyPoints(centre,rim,plane,sides){
  const [cu,cv]=sketchPlaneCoords(plane,centre);
  const [ru,rv]=sketchPlaneCoords(plane,rim);
  const r=Math.hypot(ru-cu,rv-cv), a0=Math.atan2(rv-cv,ru-cu);
  const pts=[];
  for(let i=0;i<sides;i++){
    const a=a0+i/sides*Math.PI*2;
    pts.push(sketchPlanePoint(plane,cu+Math.cos(a)*r,cv+Math.sin(a)*r));
  }
  return pts;
}
/* rebuild an existing polygon object's points after its side count changes */
function sketchRebuildPoly(obj){
  if(obj.category!=="polygon"&&obj.category!=="circle") return;
  const c=obj.center, rim=obj.rim; if(!c||!rim) return;
  obj.points=sketchPolyPoints(c,rim,obj.plane,obj.category==="circle"?48:obj.sides);
}

/* the straight-edged primitives that can become an editable corner polyline. A
   circle stays a circle (its "edges" are just a 48-gon tessellation — it wants a
   radius/diameter dimension, not a linear one). */
function isPrimitiveCategory(c){ return c==="rectangle"||c==="polygon"; }

/* CONVERT a rectangle / regular-polygon into an editable closed "line" curve in
   place: its corners become plane-space control points (obj.cpts), so the same
   cpts-based tooling (per-edge dimensions, corner fillet/chamfer, point dragging)
   works on it. Preserves id / name / nodeId / plane / transform so every existing
   reference (outliner node, selection, constraints, history) stays valid. No-op on
   a category that is already a curve. Returns true when a conversion happened. */
function sketchPrimitiveToCurve(obj){
  if(!obj||!isPrimitiveCategory(obj.category)) return false;
  // corner cpts straight from the primitive's own geometry, in plane-space [u,v].
  let cpts;
  if(obj.category==="rectangle"){
    const [u0,v0]=obj.origin, u1=u0+obj.w, v1=v0+obj.h;
    cpts=[[u0,v0],[u1,v0],[u1,v1],[u0,v1]];
  } else {   // polygon: sample its corners, then read each back into plane-space.
    cpts=sketchPolyPoints(obj.center,obj.rim,obj.plane,obj.sides)
         .map(w=>sketchPlaneCoords(obj.plane,w));
  }
  obj.category="line";
  obj.cpts=cpts;
  obj.handles=null; obj.weights=null; obj.degree=null;
  obj.center=null; obj.rim=null; obj.origin=null; obj.w=null; obj.h=null; obj.sides=null;
  obj.closed=true;
  if(obj.colors) obj.colors.line=CAT_COLORS.line;
  sketchRebuildCurve(obj);
  // reflect the new category on the outliner node so its glyph/behaviour update.
  const node=obj.nodeId&&typeof findItem==="function"?findItem(obj.nodeId):null;
  if(node) node.category="line";
  return true;
}

/* ─── DIMENSION HELPERS (for the typeable command-bar / properties fields) ───
   rectangle: plane-space corner + signed width/height. */
function sketchRectDims(p0,p1,plane,square){
  const [u0,v0]=sketchPlaneCoords(plane,p0);
  let [u1,v1]=sketchPlaneCoords(plane,p1);
  if(square){ const du=u1-u0, dv=v1-v0, s=Math.max(Math.abs(du),Math.abs(dv));
    u1=u0+Math.sign(du||1)*s; v1=v0+Math.sign(dv||1)*s; }
  return {origin:[u0,v0], w:u1-u0, h:v1-v0};
}
/* radius of a center/rim shape (polygon or circle) */
function sketchRadiusOf(obj){
  if(!obj.center||!obj.rim) return 0;
  const [cu,cv]=sketchPlaneCoords(obj.plane,obj.center);
  const [ru,rv]=sketchPlaneCoords(obj.plane,obj.rim);
  return Math.hypot(ru-cu,rv-cv);
}
/* rebuild a rectangle's points from its stored origin + w/h */
function sketchRebuildRect(obj){
  if(obj.category!=="rectangle"||!obj.origin) return;
  const [u0,v0]=obj.origin, u1=u0+obj.w, v1=v0+obj.h;
  obj.points=[ sketchPlanePoint(obj.plane,u0,v0), sketchPlanePoint(obj.plane,u1,v0),
               sketchPlanePoint(obj.plane,u1,v1), sketchPlanePoint(obj.plane,u0,v1) ];
}
/* set a center/rim shape's radius (keeps its start angle) and rebuild */
function sketchSetRadius(obj,r){
  if(!obj.center||!obj.rim||r<=0) return;
  const [cu,cv]=sketchPlaneCoords(obj.plane,obj.center);
  const [ru,rv]=sketchPlaneCoords(obj.plane,obj.rim);
  const a=Math.atan2(rv-cv,ru-cu);
  obj.rim=sketchPlanePoint(obj.plane,cu+Math.cos(a)*r,cv+Math.sin(a)*r);
  sketchRebuildPoly(obj);
}
/* the sketch object currently selected in the browser (or null) */
function sketchSelectedObject(){
  if(typeof selectedId==="undefined"||selectedId===null) return null;
  const n=(typeof findItem==="function")?findItem(selectedId):null;
  if(!n||n.kind!=="sketch"||!n.skId) return null;
  return SKETCHES.find(s=>s.id===n.skId)||null;
}

/* ═══════════════ CURVE SAMPLERS (plane-space cpts → world polyline) ═══════════════
   All curves live as control points in plane-space [u,v]; each sampler returns a
   dense WORLD polyline (via sketchPlanePoint) that vp3DrawSketches strokes and
   sketchPickAt hit-tests. Handles/weights are plane-space too. */

/* line / polyline: control points are the vertices (optionally closed). */
function curveSampleLine(cpts,plane,closed){
  const pts=cpts.map(c=>sketchPlanePoint(plane,c[0],c[1]));
  if(closed&&pts.length>2) pts.push(pts[0].slice());
  return pts;
}
/* default bezier in/out handles from neighbours (Catmull-Rom tangents), plane-space */
function curveDefaultHandles(cpts){
  const H={};
  for(let i=0;i<cpts.length;i++){
    const p=cpts[i], prev=cpts[i-1]||cpts[i], next=cpts[i+1]||cpts[i];
    const tx=(next[0]-prev[0])*0.25, ty=(next[1]-prev[1])*0.25;
    H[i]=[[p[0]-tx,p[1]-ty],[p[0]+tx,p[1]+ty]];   // [in, out]
  }
  return H;
}
/* cubic bezier through the anchors using per-anchor out/in handles. */
function curveSampleBezier(cpts,handles,plane,steps){
  steps=steps||24;
  if(cpts.length<2) return cpts.map(c=>sketchPlanePoint(plane,c[0],c[1]));
  const H=handles||curveDefaultHandles(cpts);
  const out=[];
  for(let i=0;i<cpts.length-1;i++){
    const a=cpts[i], b=cpts[i+1];
    const c1=(H[i]&&H[i][1])||a, c2=(H[i+1]&&H[i+1][0])||b;   // a.out, b.in
    for(let s=(i===0?0:1);s<=steps;s++){
      const t=s/steps, mt=1-t;
      const u=mt*mt*mt*a[0]+3*mt*mt*t*c1[0]+3*mt*t*t*c2[0]+t*t*t*b[0];
      const v=mt*mt*mt*a[1]+3*mt*mt*t*c1[1]+3*mt*t*t*c2[1]+t*t*t*b[1];
      out.push(sketchPlanePoint(plane,u,v));
    }
  }
  return out;
}
/* open uniform B-spline via De Boor with a clamped knot vector. */
function curveDeBoorKnots(n,degree){
  // n = control-point count; clamped uniform knots, length n+degree+1
  const p=degree, m=n+p+1, knots=[];
  for(let i=0;i<m;i++){
    if(i<=p) knots.push(0);
    else if(i>=n) knots.push(n-p);
    else knots.push(i-p);
  }
  return knots;
}
function curveEvalBSpline(cpts,degree,weights,tt){
  // rational De Boor at parameter tt∈[0, n-p]; weights optional (NURBS). 2D [u,v].
  const n=cpts.length, p=Math.min(degree,n-1);
  if(p<1) return cpts[0].slice();
  const knots=curveDeBoorKnots(n,p);
  // find knot span
  let k=p; while(k<n-1 && tt>=knots[k+1]) k++;
  const w=weights||cpts.map(()=>1);
  // homogeneous control points d = [u*w, v*w, w]
  const d=[];
  for(let j=0;j<=p;j++){ const idx=k-p+j; const wj=w[idx]; d.push([cpts[idx][0]*wj,cpts[idx][1]*wj,wj]); }
  for(let r=1;r<=p;r++){
    for(let j=p;j>=r;j--){
      const idx=k-p+j;
      const denom=(knots[idx+p-r+1]-knots[idx])||1e-9;
      const alpha=(tt-knots[idx])/denom;
      d[j]=[ (1-alpha)*d[j-1][0]+alpha*d[j][0],
             (1-alpha)*d[j-1][1]+alpha*d[j][1],
             (1-alpha)*d[j-1][2]+alpha*d[j][2] ];
    }
  }
  const hw=d[p][2]||1e-9;
  return [d[p][0]/hw, d[p][1]/hw];
}
function curveSampleBSpline(cpts,degree,plane,steps){
  steps=steps||Math.max(24,cpts.length*12);
  const n=cpts.length, p=Math.min(degree||3,n-1);
  if(n<2) return cpts.map(c=>sketchPlanePoint(plane,c[0],c[1]));
  if(p<1) return curveSampleLine(cpts,plane,false);
  const tMax=n-p, out=[];
  for(let s=0;s<=steps;s++){
    const tt=s/steps*tMax*0.999999;
    const uv=curveEvalBSpline(cpts,p,null,tt);
    out.push(sketchPlanePoint(plane,uv[0],uv[1]));
  }
  return out;
}
function curveSampleNurbs(cpts,degree,weights,plane,steps){
  steps=steps||Math.max(24,cpts.length*12);
  const n=cpts.length, p=Math.min(degree||3,n-1);
  if(n<2) return cpts.map(c=>sketchPlanePoint(plane,c[0],c[1]));
  if(p<1) return curveSampleLine(cpts,plane,false);
  const tMax=n-p, out=[];
  for(let s=0;s<=steps;s++){
    const tt=s/steps*tMax*0.999999;
    const uv=curveEvalBSpline(cpts,p,weights,tt);
    out.push(sketchPlanePoint(plane,uv[0],uv[1]));
  }
  return out;
}
/* rebuild a curve object's world `points` from its plane-space control data. */
function sketchRebuildCurve(obj){
  if(!isCurveCategory(obj.category)) return;
  const pl=obj.plane, c=obj.cpts;
  if(obj.category==="line")        obj.points=curveSampleLine(c,pl,obj.closed);
  else if(obj.category==="bezier") obj.points=curveSampleBezier(c,obj.handles,pl,24);
  else if(obj.category==="bspline")obj.points=curveSampleBSpline(c,obj.degree||3,pl);
  else if(obj.category==="nurbs")  obj.points=curveSampleNurbs(c,obj.degree||3,obj.weights,pl);
  // close ANY curve category by looping the sampled polyline back to its first
  // point (curveSampleLine already does this for "line"; the distance guard stops
  // a double-append there). This is what drives the translucent loop fill.
  if(obj.closed&&obj.points.length>2&&
     V3.length(V3.sub(obj.points[0],obj.points[obj.points.length-1]))>1e-6)
    obj.points.push(obj.points[0].slice());
}
/* control points (and handles) as WORLD coords, through the object transform —
   used by the point-editing overlay + hit-test. Returns {anchors:[world],
   handles:{i:[worldIn,worldOut]}}. */
function sketchWorldCpts(obj){
  if(!isCurveCategory(obj.category)||!obj.cpts) return {anchors:[],handles:{}};
  const anchors=obj.cpts.map(c=>sketchPlanePoint(obj.plane,c[0],c[1]));
  const handles={};
  if(obj.category==="bezier"&&obj.handles){
    for(const i in obj.handles){
      const h=obj.handles[i];
      handles[i]=[sketchPlanePoint(obj.plane,h[0][0],h[0][1]),sketchPlanePoint(obj.plane,h[1][0],h[1][1])];
    }
  }
  return {anchors,handles};
}

/* apply an object's transform (translate only for now — rotate/scale about
   centroid) to its stored points, returning transformed world points. */
function sketchApplyTransform(obj){
  const t=obj.transform||{t:[0,0,0],r:[0,0,0],s:[1,1,1]};
  const c=sketchCentroid(obj.points);
  const rx=t.r[0]||0, ry=t.r[1]||0, rz=t.r[2]||0;
  const sx=t.s[0]||1, sy=t.s[1]||1, sz=t.s[2]||1;
  const cx=Math.cos(rx),snx=Math.sin(rx),cy=Math.cos(ry),sny=Math.sin(ry),cz=Math.cos(rz),snz=Math.sin(rz);
  return obj.points.map(p=>{
    let x=(p[0]-c[0])*sx, y=(p[1]-c[1])*sy, z=(p[2]-c[2])*sz;
    // Rz, Ry, Rx
    let x1=x*cz-y*snz, y1=x*snz+y*cz; x=x1; y=y1;
    let x2=x*cy+z*sny, z2=-x*sny+z*cy; x=x2; z=z2;
    let y3=y*cx-z*snx, z3=y*snx+z*cx; y=y3; z=z3;
    return [x+c[0]+t.t[0], y+c[1]+t.t[1], z+c[2]+t.t[2]];
  });
}
function sketchCentroid(pts){
  const c=[0,0,0]; pts.forEach(p=>{c[0]+=p[0];c[1]+=p[1];c[2]+=p[2];});
  const n=pts.length||1; return [c[0]/n,c[1]/n,c[2]/n];
}
/* world centroid including transform — the gizmo origin */
function sketchWorldCentroid(obj){ return sketchCentroid(sketchApplyTransform(obj)); }

/* while placing a curve, is the cursor within snapping distance of the FIRST
   control point? Returns that point's screen position (to snap onto + highlight),
   or null. Only once at least two points exist (so there's a loop to close). */
function sketchDrawSnapTarget(mx,my){
  if(!SKETCH.cpts||SKETCH.cpts.length<2) return null;
  const w=sketchPlanePoint(SKETCH.plane,SKETCH.cpts[0][0],SKETCH.cpts[0][1]);
  const s=vp3World2Screen(VP3.mvp,w); if(!s) return null;
  return ((mx-s[0])*(mx-s[0])+(my-s[1])*(my-s[1]))<=SK_SNAP_PX*SK_SNAP_PX?s:null;
}

/* ─── DRAW-MODE STATE MACHINE ─── */
const SK_CURVE_NAMES={line:"Line",bezier:"Bezier",bspline:"B-spline",nurbs:"NURBS"};
function sketchEnterDrawMode(toolId){
  if(CAD.env!=="sketch") return;
  SKETCH.drawing=true; SKETCH.tool=toolId; SKETCH.p0=SKETCH.p1=SKETCH.preview=null;
  SKETCH.cpts=[]; SKETCH.hoverPt=null;
  SKETCH.stage=CURVE_TOOLS.indexOf(toolId)>=0?"placing":"idle";
  if(typeof gizmoDetach==="function") gizmoDetach();
  sketchShowCmdBar();
  const label=CURVE_TOOLS.indexOf(toolId)>=0?SK_CURVE_NAMES[toolId]
             :toolId==="rectangle"?(SKETCH.square?"Square":"Rectangle")
             :toolId==="circle2d"?"Circle":sketchPolyName(SKETCH.sides);
  toast(label+" — draw on "+SKETCH.plane.name,"pen-tool");
}
function sketchExitDrawMode(){
  SKETCH.drawing=false; SKETCH.stage="idle"; SKETCH.p0=SKETCH.p1=SKETCH.preview=null;
  SKETCH.cpts=[]; SKETCH.hoverPt=null; SKETCH.snapClose=false; SKETCH.snapScreen=null;
  if(typeof hideCmdBar==="function") hideCmdBar();
  // leaving the draw tool drops back to Object mode so the gizmo is reachable
  if(typeof CAD!=="undefined"&&CAD.imode==="sketch"&&typeof setCadInteractionMode==="function")
    setCadInteractionMode("object",true);
  vp3Draw();
}

/* mousedown hook (called from vp3AttachInput before orbit). Returns true to
   consume the event. LMB starts a drag; RMB is left for the context menu. */
function sketchMouseDown(e,mx,my){
  if(!SKETCH.drawing||e.button!==0) return false;
  const snap=e&&e.ctrlKey?true:undefined;
  const p=vp3ScreenToPlane(mx,my,SKETCH.plane.point,SKETCH.plane.normal,snap);
  if(!p) return true;                      // consumed, but nothing to place
  // CURVE tools: each click appends a control point (plane-space); no commit yet.
  if(CURVE_TOOLS.indexOf(SKETCH.tool)>=0){
    // clicking on the first point (within snap radius) closes the curve into a loop.
    if(sketchDrawSnapTarget(mx,my)){ sketchFinishCurve(true); return true; }
    SKETCH.cpts.push(sketchPlaneCoords(SKETCH.plane,p));
    SKETCH.hoverPt=p; SKETCH.stage="placing";
    sketchBuildPreview(); vp3Draw(); sketchSyncCmdFields();
    return true;
  }
  // PRIMITIVES: click-drag corner/radius box.
  SKETCH.p0=p; SKETCH.p1=p; SKETCH.stage="dragging"; sketchBuildPreview();
  vp3Draw(); sketchSyncCmdFields();
  return true;
}
function sketchOnMove(mx,my,snap){
  if(SKETCH.stage==="placing"){             // curve rubber-band to the cursor
    // near the first point? flag the snap-to-close and pin the hover onto it.
    const snapS=sketchDrawSnapTarget(mx,my);
    SKETCH.snapClose=!!snapS; SKETCH.snapScreen=snapS;
    if(snapS){
      SKETCH.hoverPt=sketchPlanePoint(SKETCH.plane,SKETCH.cpts[0][0],SKETCH.cpts[0][1]);
      sketchBuildPreview(); vp3Draw(); return;
    }
    const p=vp3ScreenToPlane(mx,my,SKETCH.plane.point,SKETCH.plane.normal,snap);
    if(p){ SKETCH.hoverPt=p; sketchBuildPreview(); vp3Draw(); }
    return;
  }
  if(SKETCH.stage!=="dragging") return;
  const p=vp3ScreenToPlane(mx,my,SKETCH.plane.point,SKETCH.plane.normal,snap);
  if(p){ SKETCH.p1=p; sketchBuildPreview(); vp3Draw(); sketchSyncCmdFields(); }
}
/* refresh the command-bar readouts to the live drag values — but never while the
   user is typing in one of those fields (that would blur mid-entry). */
function sketchSyncCmdFields(){
  const opts=document.getElementById("cmdOpts"); if(!opts) return;
  if(document.activeElement&&document.activeElement.closest&&document.activeElement.closest("#cmdOpts")) return;
  sketchFillCmdOpts();
}
function sketchOnUp(){
  // Enter routes here for curves: finish the placed curve.
  if(SKETCH.stage==="placing"){ sketchFinishCurve(); return; }
  if(SKETCH.stage!=="dragging") return;
  SKETCH.stage="idle";
  sketchCommit();
}
/* finish a click-per-point curve (Enter / double-click / Finish button). A truthy
   closeLoop (snap-to-first-point) commits the curve as a closed loop. */
function sketchFinishCurve(closeLoop){
  if(SKETCH.stage!=="placing") return;
  const need=2;
  if(SKETCH.cpts.length<need){ toast("Need at least "+need+" points","pen-tool"); return; }
  SKETCH.stage="idle";
  sketchCommitCurve(closeLoop);
}
/* build the live preview object off the current p0/p1 (primitives) or cpts (curves) */
function sketchBuildPreview(){
  const t=SKETCH.tool;
  if(CURVE_TOOLS.indexOf(t)>=0){
    // provisional cpts = placed points + the live cursor as a trailing anchor.
    // when snapping to close, drop the trailing cursor and build a closed loop.
    const close=SKETCH.snapClose;
    const cc=SKETCH.cpts.slice();
    if(SKETCH.hoverPt&&!close) cc.push(sketchPlaneCoords(SKETCH.plane,SKETCH.hoverPt));
    let pts;
    if(cc.length<2) pts=cc.map(c=>sketchPlanePoint(SKETCH.plane,c[0],c[1]));
    else if(t==="line")    pts=curveSampleLine(cc,SKETCH.plane,close);
    else if(t==="bezier")  pts=curveSampleBezier(cc,null,SKETCH.plane,18);
    else if(t==="bspline") pts=curveSampleBSpline(cc,SKETCH.degree||3,SKETCH.plane);
    else                   pts=curveSampleNurbs(cc,SKETCH.degree||3,null,SKETCH.plane);
    // loop non-line previews so the translucent closed fill shows the snap intent.
    if(close&&t!=="line"&&pts.length>2) pts.push(pts[0].slice());
    SKETCH.preview={points:pts,closed:close,colors:{...SK_COLORS,line:CAT_COLORS[t]},transform:{t:[0,0,0],r:[0,0,0],s:[1,1,1]},cpts:cc};
    return;
  }
  let pts;
  if(t==="rectangle") pts=sketchRectPoints(SKETCH.p0,SKETCH.p1,SKETCH.plane,SKETCH.square);
  else pts=sketchPolyPoints(SKETCH.p0,SKETCH.p1,SKETCH.plane,t==="circle2d"?48:SKETCH.sides);
  SKETCH.preview={points:pts,closed:true,colors:SK_COLORS,transform:{t:[0,0,0],r:[0,0,0],s:[1,1,1]}};
}

/* commit the in-progress drag into a real SKETCHES object (+ registration). */
function sketchCommit(){
  if(!SKETCH.p0||!SKETCH.p1){ SKETCH.preview=null; return; }
  const t=SKETCH.tool;
  const dragLen=V3.length(V3.sub(SKETCH.p1,SKETCH.p0));
  if(dragLen<0.05){ SKETCH.preview=null; vp3Draw(); return; }   // ignore a click with no drag

  const category=t==="rectangle"?"rectangle":t==="circle2d"?"circle":"polygon";
  const sides=t==="rectangle"?4:t==="circle2d"?48:SKETCH.sides;
  const points=t==="rectangle"?sketchRectPoints(SKETCH.p0,SKETCH.p1,SKETCH.plane,SKETCH.square)
                              :sketchPolyPoints(SKETCH.p0,SKETCH.p1,SKETCH.plane,sides);
  const namebase=category==="rectangle"?(SKETCH.square?"Square":"Rectangle")
                :category==="circle"?"Circle":sketchPolyName(SKETCH.sides);
  // rectangle keeps its plane-space corner (p0) + width/height so the command-bar
  // and Properties can resize it by exact value; radius shapes keep center/rim.
  const rectDims=(category==="rectangle")?sketchRectDims(SKETCH.p0,SKETCH.p1,SKETCH.plane,SKETCH.square):null;
  const obj={
    id:"sk"+(++_skUid), nodeId:null, name:namebase+"_"+(SKETCHES.length+1),
    category, sides,
    plane:{point:SKETCH.plane.point.slice(),normal:SKETCH.plane.normal.slice(),name:SKETCH.plane.name},
    center:(category!=="rectangle")?SKETCH.p0.slice():null,
    rim:(category!=="rectangle")?SKETCH.p1.slice():null,
    origin:rectDims?rectDims.origin:null,          // rect: plane-space corner [u,v]
    w:rectDims?rectDims.w:null, h:rectDims?rectDims.h:null,
    points, closed:true,
    transform:{t:[0,0,0],r:[0,0,0],s:[1,1,1]},
    colors:{point:SK_COLORS.point,line:SK_COLORS.line,fill:SK_COLORS.fill},
  };
  obj.colors.line=CAT_COLORS[category]||SK_COLORS.line;
  SKETCH.preview=null;
  SKETCHES.push(obj);
  sketchRegister(obj);
  sketchSyncCmdFields();                    // fields fall back to next-draw defaults
}

/* commit a click-per-point curve into a real SKETCHES object (+ registration).
   closeLoop closes the curve into a filled loop (endpoint snapped onto the start). */
function sketchCommitCurve(closeLoop){
  const t=SKETCH.tool, cpts=SKETCH.cpts.slice();
  SKETCH.preview=null; SKETCH.hoverPt=null;
  SKETCH.snapClose=false; SKETCH.snapScreen=null;
  if(cpts.length<2){ SKETCH.cpts=[]; vp3Draw(); return; }
  const category=t;   // line | bezier | bspline | nurbs
  const namebase=SK_CURVE_NAMES[category]||"Curve";
  const obj={
    id:"sk"+(++_skUid), nodeId:null, name:namebase+"_"+(SKETCHES.length+1),
    category, sides:null,
    plane:{point:SKETCH.plane.point.slice(),normal:SKETCH.plane.normal.slice(),name:SKETCH.plane.name},
    cpts,
    handles:(category==="bezier")?curveDefaultHandles(cpts):null,
    weights:(category==="nurbs")?cpts.map(()=>1):null,
    degree:(category==="bspline"||category==="nurbs")?Math.min(SKETCH.degree||3,cpts.length-1):null,
    points:[], closed:!!closeLoop,
    transform:{t:[0,0,0],r:[0,0,0],s:[1,1,1]},
    colors:{point:SK_COLORS.point,line:CAT_COLORS[category]||SK_COLORS.line,fill:SK_COLORS.fill},
  };
  sketchRebuildCurve(obj);
  SKETCH.cpts=[];
  SKETCHES.push(obj);
  sketchRegister(obj);
  // curve tool stays armed for the next curve
  sketchSyncCmdFields();
}

/* ─── REGISTRATION: outliner + features timeline + history ─── */
function sketchRegister(obj){
  // 1) outliner node under the Sketches folder. Icon + swatch reflect the category
  //    (curve types get the spline glyph; primitives keep their own glyph).
  const folder=flat(CAD_TREE).find(n=>n.name==="Sketches");
  const catIcon={line:"line",bezier:"spline",bspline:"spline",nurbs:"spline",
                 rectangle:"rect",circle:"circle2d",polygon:"polygon"}[obj.category]||"spline";
  const catColor=(typeof CAT_COLORS!=="undefined"&&CAT_COLORS[obj.category])||obj.colors.fill;
  const node=item(obj.name,catIcon,{kind:"sketch",swatch:catColor,plane:obj.plane.name,skId:obj.id,category:obj.category});
  obj.nodeId=node.id;
  if(folder){ folder.collapsed=false; folder.children.push(node); }
  // 2) features timeline entry (manual append; keeps history push as a "sketch")
  if(typeof CAD_TIMELINE!=="undefined"&&typeof cadFeatInstance==="function"){
    const inst=cadFeatInstance("sketch",obj.name); inst.skId=obj.id;
    const at=Math.min(cadRollback,CAD_TIMELINE.length);
    CAD_TIMELINE.splice(at,0,inst); cadRollback=at+1;
    if(cadRightPane==="features") renderCadFeatures();
  }
  // 3) history event
  if(typeof pushHistory==="function")
    pushHistory("sketch",obj.name,obj.category+" · "+obj.points.length+" pts");
  // 4) reflect in the UI + select it (drives properties + gizmo)
  renderTree();
  if(typeof selectItem==="function") selectItem(obj.nodeId);
  syncBrowserFooter();
  toast("Drew "+obj.name,"pen-tool");
  vp3Draw();
}

/* ─── COMMAND BAR (bottom-centre) — sides selector / square toggle ─── */
function sketchShowCmdBar(){
  const t=SKETCH.tool;
  const curve=CURVE_TOOLS.indexOf(t)>=0;
  const name=curve?SK_CURVE_NAMES[t]
            :t==="rectangle"?(SKETCH.square?"Square":"Rectangle"):t==="circle2d"?"Circle":"Polygon";
  const hint=curve?"Click to place points · Enter or double-click to finish · Esc cancels"
            :t==="rectangle"?"Click and drag a corner box"
            :t==="circle2d"?"Click centre, drag radius"
            :"Click centre, drag radius · pick sides";
  const icon=curve?(t==="line"?"line":t):t==="rectangle"?"rect":t==="circle2d"?"circle2d":"spline";
  if(typeof showCmdBar==="function") showCmdBar(name,hint,icon,[]);
  sketchFillCmdOpts();
}
/* a single typeable numeric field for the command bar: label + input + unit. */
function sketchCmdField(label,value,unit,onCommit,opt){
  const wrap=document.createElement("div"); wrap.className="cmd-field";
  const lbl=document.createElement("span"); lbl.className="cmd-field-lbl"; lbl.textContent=label;
  const inp=document.createElement("input"); inp.className="cmd-field-inp"; inp.type="text";
  inp.value=(Math.round(value*100)/100).toString();
  if(unit){ inp.dataset.unit=unit; }
  const push=()=>{ const v=parseFloat(inp.value); if(!isNaN(v)) onCommit(v); };
  inp.onkeydown=(e)=>{ e.stopPropagation(); if(e.key==="Enter"){ push(); inp.blur(); } };
  inp.oninput=push;                          // realtime while typing
  inp.onchange=push;
  wrap.appendChild(lbl); wrap.appendChild(inp);
  if(unit){ const u=document.createElement("span"); u.className="cmd-field-unit"; u.textContent=unit; wrap.appendChild(u); }
  wrap.querySelector("input")._opt=opt;
  return wrap;
}
/* write the tool's inline options into #cmdOpts as TYPEABLE fields (no long chip
   list). The fields edit the selected object if one is picked, otherwise the live
   preview / the next-draw defaults — all in realtime. */
function sketchFillCmdOpts(){
  const opts=document.getElementById("cmdOpts"); if(!opts) return;
  opts.innerHTML="";
  const sel=(typeof sketchSelectedObject==="function")?sketchSelectedObject():null;
  const tool=SKETCH.tool||(sel?sel.category:null);
  const target=(sel&&sel.category===(tool==="circle2d"?"circle":tool))?sel:null;

  // CURVE placement: point-count chip, degree field (bspline/nurbs), Finish/Cancel
  if(SKETCH.drawing&&CURVE_TOOLS.indexOf(tool)>=0){
    const chip=document.createElement("span"); chip.className="vp-cmd-chip";
    chip.textContent=SKETCH.cpts.length+" pt"+(SKETCH.cpts.length===1?"":"s"); opts.appendChild(chip);
    if(tool==="bspline"||tool==="nurbs"){
      opts.appendChild(sketchCmdField("Degree", SKETCH.degree||3, "", v=>{
        SKETCH.degree=Math.max(1,Math.min(5,Math.round(v))); sketchBuildPreview(); vp3Draw();
      }));
    }
    const fin=document.createElement("button"); fin.className="vp-cmd-chip on"; fin.textContent="Finish";
    fin.dataset.tip="Enter / double-click"; fin.onclick=()=>sketchFinishCurve(); opts.appendChild(fin);
    const can=document.createElement("button"); can.className="vp-cmd-chip"; can.textContent="Cancel";
    can.onclick=()=>sketchExitDrawMode(); opts.appendChild(can);
    return;
  }

  if(tool==="polygon"){
    // Sides field
    opts.appendChild(sketchCmdField("Sides", target?target.sides:SKETCH.sides, "", v=>{
      const n=Math.max(3,Math.min(64,Math.round(v)));
      if(target){ target.sides=n; sketchRebuildPoly(target); vp3Draw(); }
      else { SKETCH.sides=n; sketchApplyPreviewOpts(); }
      const foot=document.getElementById("activeToolFoot");
      if(foot&&CAD.active==="polygon"&&!target) foot.textContent=sketchPolyName(n);
    }));
    // Radius field (edits a selected/preview N-gon; blank on a fresh tool)
    if(target||sketchPreviewRadius()!=null){
      opts.appendChild(sketchCmdField("Radius", target?sketchRadiusOf(target):sketchPreviewRadius(), "mm", v=>{
        if(target){ sketchSetRadius(target,Math.max(0.05,v)); vp3Draw(); }
        else sketchSetPreviewRadius(Math.max(0.05,v));
      }));
    }
  } else if(tool==="circle2d"||tool==="circle"){
    if(target||sketchPreviewRadius()!=null){
      opts.appendChild(sketchCmdField("Radius", target?sketchRadiusOf(target):sketchPreviewRadius(), "mm", v=>{
        if(target){ sketchSetRadius(target,Math.max(0.05,v)); vp3Draw(); }
        else sketchSetPreviewRadius(Math.max(0.05,v));
      }));
    }
  } else if(tool==="rectangle"){
    const dims=target?{w:target.w,h:target.h}:sketchPreviewRectDims();
    if(dims){
      opts.appendChild(sketchCmdField("W", Math.abs(dims.w), "mm", v=>{
        if(target){ target.w=Math.sign(target.w||1)*Math.max(0.05,v); sketchRebuildRect(target); vp3Draw(); }
        else sketchSetPreviewRect("w",v);
      }));
      opts.appendChild(sketchCmdField("H", Math.abs(dims.h), "mm", v=>{
        if(target){ target.h=Math.sign(target.h||1)*Math.max(0.05,v); sketchRebuildRect(target); vp3Draw(); }
        else sketchSetPreviewRect("h",v);
      }));
    }
    // Square toggle stays as a small chip
    const chip=document.createElement("button");
    chip.className="vp-cmd-chip"+(SKETCH.square?" on":"");
    chip.textContent="Square"; chip.dataset.tip="Constrain to a square";
    chip.onclick=()=>{ SKETCH.square=!SKETCH.square; if(SKETCH.stage==="dragging"){ sketchBuildPreview(); vp3Draw(); } sketchFillCmdOpts(); };
    opts.appendChild(chip);
  }
}
/* re-apply preview after a default change (sides), rebuilding a mid-draw shape */
function sketchApplyPreviewOpts(){
  if(SKETCH.drawing&&SKETCH.stage==="dragging"){ sketchBuildPreview(); vp3Draw(); }
}
/* current preview radius (center/rim shapes) or null when not mid-drag */
function sketchPreviewRadius(){
  if(!SKETCH.preview||!SKETCH.p0||!SKETCH.p1) return null;
  if(SKETCH.tool!=="polygon"&&SKETCH.tool!=="circle2d") return null;
  const [cu,cv]=sketchPlaneCoords(SKETCH.plane,SKETCH.p0);
  const [ru,rv]=sketchPlaneCoords(SKETCH.plane,SKETCH.p1);
  return Math.hypot(ru-cu,rv-cv);
}
/* set the preview radius by moving p1 along its current angle from p0 */
function sketchSetPreviewRadius(r){
  if(!SKETCH.p0||!SKETCH.p1) return;
  const [cu,cv]=sketchPlaneCoords(SKETCH.plane,SKETCH.p0);
  const [ru,rv]=sketchPlaneCoords(SKETCH.plane,SKETCH.p1);
  const a=Math.atan2(rv-cv,ru-cu);
  SKETCH.p1=sketchPlanePoint(SKETCH.plane,cu+Math.cos(a)*r,cv+Math.sin(a)*r);
  sketchBuildPreview(); vp3Draw();
}
/* current preview rectangle w/h or null */
function sketchPreviewRectDims(){
  if(!SKETCH.preview||SKETCH.tool!=="rectangle"||!SKETCH.p0||!SKETCH.p1) return null;
  return sketchRectDims(SKETCH.p0,SKETCH.p1,SKETCH.plane,SKETCH.square);
}
/* set preview rect width or height by nudging p1 in plane space */
function sketchSetPreviewRect(which,v){
  if(!SKETCH.p0||!SKETCH.p1) return;
  const d=sketchRectDims(SKETCH.p0,SKETCH.p1,SKETCH.plane,false);
  let w=d.w, h=d.h;
  if(which==="w") w=Math.sign(w||1)*Math.max(0.05,v); else h=Math.sign(h||1)*Math.max(0.05,v);
  SKETCH.p1=sketchPlanePoint(SKETCH.plane,d.origin[0]+w,d.origin[1]+h);
  sketchBuildPreview(); vp3Draw();
}

/* ─── RENDER PASS ─── points / lines / translucent closed fill. */
/* draw a boolean Region: all rings into one path, even-odd fill so holes cut out,
   then each ring stroked. Rings hold plane-space [u,v]; identity transform. */
function vp3DrawRegion(g,mvp,obj,sel){
  const ringsScr=obj.rings.map(r=>r.uv.map(uv=>{
    const w=sketchPlanePoint(obj.plane,uv[0],uv[1]); return vp3World2Screen(mvp,w);
  }));
  if(ringsScr.some(r=>r.some(p=>!p))) return;              // any point behind camera → skip frame
  // even-odd fill across every ring
  g.globalAlpha=0.28; g.fillStyle=obj.colors.fill;
  g.beginPath();
  for(const sp of ringsScr){
    if(sp.length<3) continue;
    g.moveTo(sp[0][0],sp[0][1]);
    for(let i=1;i<sp.length;i++) g.lineTo(sp[i][0],sp[i][1]);
    g.closePath();
  }
  g.fill("evenodd"); g.globalAlpha=1;
  // stroke each ring
  g.lineWidth=sel?2:1.5; g.strokeStyle=sel?"#ffffff":obj.colors.line;
  for(const sp of ringsScr){
    if(sp.length<2) continue;
    g.beginPath(); g.moveTo(sp[0][0],sp[0][1]);
    for(let i=1;i<sp.length;i++) g.lineTo(sp[i][0],sp[i][1]);
    g.closePath(); g.stroke();
  }
}
function vp3DrawSketches(g,mvp){
  const list=[];
  for(const obj of SKETCHES){
    const node=obj.nodeId?findItem(obj.nodeId):null;
    if(node&&node.visible===false) continue;
    const sel=node?!!node.selected:(obj.nodeId===selectedId);
    // a boolean Region is multi-ring (outer + holes) — drawn on its own path with an
    // even-odd fill so the holes punch through; each ring stroked separately.
    if(obj.category==="region"&&obj.rings){ vp3DrawRegion(g,mvp,obj,sel); continue; }
    list.push({pts:sketchApplyTransform(obj),colors:obj.colors,closed:obj.closed,sel,category:obj.category});
  }
  if(SKETCH.preview) list.push({pts:SKETCH.preview.points,colors:SK_COLORS,closed:true,sel:true,preview:true,category:SKETCH.tool==="circle2d"?"circle":SKETCH.tool});
  for(const s of list){
    const sp=s.pts.map(p=>vp3World2Screen(mvp,p));
    if(sp.some(p=>!p)) continue;
    // closed translucent fill
    if(s.closed&&sp.length>=3){
      g.globalAlpha=s.preview?0.18:0.28; g.fillStyle=s.colors.fill;
      g.beginPath(); g.moveTo(sp[0][0],sp[0][1]);
      for(let i=1;i<sp.length;i++) g.lineTo(sp[i][0],sp[i][1]);
      g.closePath(); g.fill(); g.globalAlpha=1;
    }
    // edges
    g.lineWidth=s.sel?2:1.5; g.strokeStyle=s.sel?"#ffffff":s.colors.line;
    g.beginPath(); g.moveTo(sp[0][0],sp[0][1]);
    for(let i=1;i<sp.length;i++) g.lineTo(sp[i][0],sp[i][1]);
    if(s.closed) g.closePath();
    g.stroke();
    // vertices — corner dots only for the straight-edged primitives (rectangle /
    // polygon). Circles and curves are smooth: their editable points come from the
    // dedicated control-point overlay (points.js), not one dot per sampled point.
    if(s.category==="rectangle"||s.category==="polygon"){
      g.fillStyle=s.colors.point;
      for(const p of sp){ g.beginPath(); g.arc(p[0],p[1],s.sel?3:2.4,0,7); g.fill(); }
    }
  }
  // while placing a curve, mark the control points dropped so far
  if(SKETCH.stage==="placing"&&SKETCH.cpts.length){
    g.fillStyle=CAT_COLORS[SKETCH.tool]||"#ffffff";
    for(const c of SKETCH.cpts){
      const w=sketchPlanePoint(SKETCH.plane,c[0],c[1]), s=vp3World2Screen(mvp,w);
      if(s){ g.beginPath(); g.arc(s[0],s[1],3.4,0,7); g.fill(); }
    }
    // snap-to-close indicator: an amber ring over the first point when the cursor
    // is close enough to loop the curve.
    if(SKETCH.snapClose&&SKETCH.snapScreen){
      const s=SKETCH.snapScreen;
      g.strokeStyle="#f59e0b"; g.lineWidth=2.5;
      g.beginPath(); g.arc(s[0],s[1],8,0,7); g.stroke();
    }
  }
}

/* ─── PICK (screen-space) ─── the topmost sketch object under the cursor, or
   null. A filled/closed sketch hits anywhere inside its outline; an open one
   hits near an edge. Used for click-to-select in the viewport. */
function sketchPickAt(mx,my){
  for(let i=SKETCHES.length-1;i>=0;i--){       // topmost (last drawn) first
    const obj=SKETCHES[i];
    const node=obj.nodeId?findItem(obj.nodeId):null;
    if(node&&node.visible===false) continue;
    const sp=sketchApplyTransform(obj).map(p=>vp3World2Screen(VP3.mvp,p));
    if(sp.some(p=>!p)) continue;
    if(obj.closed&&sp.length>=3&&sketchPointInPoly(mx,my,sp)) return obj;
    // also allow an edge-proximity hit (thin shapes / near the outline)
    for(let k=0;k<sp.length;k++){
      const a=sp[k], b=sp[(k+1)%sp.length];
      if(gizmoNearSeg(mx,my,a,b,6)) return obj;
    }
  }
  return null;
}
function sketchPointInPoly(px,py,poly){
  let inside=false;
  for(let i=0,j=poly.length-1;i<poly.length;j=i++){
    const xi=poly[i][0],yi=poly[i][1],xj=poly[j][0],yj=poly[j][1];
    if(((yi>py)!==(yj>py))&&(px<(xj-xi)*(py-yi)/((yj-yi)||1e-9)+xi)) inside=!inside;
  }
  return inside;
}
/* viewport LMB click (no drag): select the sketch under the cursor and attach the
   gizmo there, or click empty space to deselect + exit the gizmo. Returns true
   when it handled the click (an object was hit) so orbit is suppressed. */
function sketchClickSelect(mx,my,e){
  if(SKETCH.drawing) return false;             // draw-mode owns the click
  const additive=!!(e&&(e.shiftKey||e.ctrlKey||e.metaKey));
  const obj=sketchPickAt(mx,my);
  if(obj){ if(typeof selectItem==="function") selectItem(obj.nodeId, additive); return true; }
  // empty space → deselect (this is how you get out of the gizmo), unless adding
  if(!additive && selectedId!==null){ if(typeof selectItem==="function") selectItem(null); }
  return false;
}

/* ─── window move/up listeners for the active drag ─── */
window.addEventListener("mousemove",e=>{
  if(SKETCH.stage!=="dragging"&&SKETCH.stage!=="placing") return;
  const stage=document.getElementById("stage"); if(!stage) return;
  const r=stage.getBoundingClientRect();
  const snap=e.ctrlKey?true:undefined;               // Ctrl forces grid snap
  sketchOnMove(e.clientX-r.left,e.clientY-r.top,snap);
});
window.addEventListener("mouseup",()=>{
  if(SKETCH.stage==="dragging") sketchOnUp();          // primitives commit on up
  // curves do NOT commit on up (they finish on Enter / double-click)
});
/* double-click finishes a click-per-point curve. The dblclick fires after the
   2nd mousedown already appended a point, so drop that duplicate trailing point. */
window.addEventListener("dblclick",e=>{
  if(SKETCH.stage!=="placing") return;
  if(e.target&&e.target.closest&&e.target.closest(".vp-pill,.vp-viewtools,.vp-cmdbar,.gz-hud,.ctx-menu")) return;
  if(SKETCH.cpts.length>2) SKETCH.cpts.pop();          // remove the dbl-click duplicate
  sketchFinishCurve();
});
