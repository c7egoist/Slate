"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   constraintdraw.js — the viewport overlay for the sketch solver: small glyphs
                       next to geometric constraints (∥ ⊥ = H V ⊙ lock) and full
                       dimension annotations (witness lines + arrow + value label)
                       for the linear / angle / radius / diameter dimensions.

     Hooked into vp3Draw right after ssDrawHover. Double-clicking a dimension's
     value label opens the floating value box (constrainpick.cpickPromptValue) and
     re-solves via setConstraintValue().

     Positions come from plane-space cpts → world (sketchPlanePoint) → screen
     (vp3World2Screen), so glyphs track the geometry as the solver moves it.
   ════════════════════════════════════════════════════════════════════════════ */

const CX_GLYPH_COL="#34d399";     // geometric-constraint glyph colour (green)
const CX_DIM_COL="#ffffff";       // dimension annotation colour (white)
const CX_SEL_COL="#38bdf8";       // selected-dimension accent (sky blue)
const _cxLabelRects=[];           // per-frame [{id, x,y,w,h}] for click/dbl-click hit-test
let CX_SELECTED=null;             // id of the currently-selected dimension, or null
let CX_DRAG=null;                 // active label-drag payload, or null (see cxMouseDown)

/* world point of a control point (plane-space → world) */
function cxWorldPt(objId,i){
  const o=cxObj(objId); if(!o||!o.cpts||!o.cpts[i]) return null;
  return sketchPlanePoint(o.plane,o.cpts[i][0],o.cpts[i][1]);
}
/* screen point of a control point, or null if off-plane/behind camera */
function cxScreenPt(mvp,objId,i){ const w=cxWorldPt(objId,i); return w?vp3World2Screen(mvp,w):null; }
/* midpoint (screen) of a line entity */
function cxLineMidScreen(mvp,ent){
  const o=cxObj(ent.objId); if(!o) return null;
  const a=cxScreenPt(mvp,ent.objId,ent.i), b=cxScreenPt(mvp,ent.objId,(ent.i+1)%o.cpts.length);
  if(!a||!b) return null;
  return [(a[0]+b[0])/2,(a[1]+b[1])/2];
}
/* a representative anchor screen point for any entity (for glyph placement) */
function cxEntityAnchorScreen(mvp,ent){
  if(ent.kind==="point") return cxScreenPt(mvp,ent.objId,ent.i);
  if(ent.kind==="line")  return cxLineMidScreen(mvp,ent);
  const o=cxObj(ent.objId);                                  // circle → center
  if(o&&o.center){ const [cu,cv]=sketchPlaneCoords(o.plane,o.center); return vp3World2Screen(mvp,sketchPlanePoint(o.plane,cu,cv)); }
  return null;
}

/* the short glyph text drawn for each geometric constraint type */
const CX_GLYPH_TEXT={
  coincident:"◎", horizontal:"H", vertical:"V", parallel:"∥", perpendicular:"⊥",
  equal:"=", concentric:"⊙", tangent:"◠", fix:"▲",
};

/* is the sketch object a constraint is attached to currently visible? A dimension
   rides with its geometry: it shows whenever that geometry shows and hides with it,
   regardless of interaction mode — so orbiting the camera never drops it (the earlier
   sketch-mode-only gate is why dimensions vanished the instant you left the pick tool
   to orbit; sketch EDGES render in every mode via vp3DrawSketches, so the annotations
   must too). Returns false only when the owning object is hidden or gone. */
function cxConstraintVisible(c){
  if(!c||!c.entities||!c.entities.length) return false;
  if(typeof cxObj!=="function") return true;
  const o=cxObj(c.entities[0].objId); if(!o) return false;
  if(o.nodeId&&typeof findItem==="function"){ const n=findItem(o.nodeId); if(n&&n.visible===false) return false; }
  return true;
}

/* ─── MAIN OVERLAY PASS ─── */
function cxDrawOverlay(g,mvp){
  if(typeof CONSTRAINTS==="undefined"||!CONSTRAINTS.length) return;
  _cxLabelRects.length=0;
  g.save();
  g.font="600 11px 'General Sans',system-ui,sans-serif";
  g.textAlign="center"; g.textBaseline="middle";
  const stack={};   // stack multiple glyphs on the same anchor so they don't overlap
  for(const c of CONSTRAINTS){
    if(!cxConstraintVisible(c)) continue;   // hidden/removed owner → skip (persists through orbit otherwise)
    if(typeof CX_DIMENSIONAL!=="undefined"&&CX_DIMENSIONAL.has(c.type)){ cxDrawDimension(g,mvp,c); continue; }
    // geometric glyph near the (first) entity anchor
    const s=cxEntityAnchorScreen(mvp,c.entities[0]); if(!s) continue;
    const key=Math.round(s[0])+","+Math.round(s[1]);
    const n=stack[key]=(stack[key]||0)+1;
    const gx=s[0]+10, gy=s[1]-10-(n-1)*15;
    cxDrawGlyphBadge(g,gx,gy,CX_GLYPH_TEXT[c.type]||"?");
  }
  g.restore();
}
/* a rounded chip behind a glyph so it reads over any geometry */
function cxDrawGlyphBadge(g,x,y,text){
  g.save();
  g.fillStyle="rgba(6,10,14,0.85)"; g.strokeStyle=CX_GLYPH_COL; g.lineWidth=1;
  const r=8;
  g.beginPath(); g.arc(x,y,r,0,7); g.fill(); g.stroke();
  g.fillStyle=CX_GLYPH_COL; g.fillText(text,x,y+0.5);
  g.restore();
}

/* ─── DIMENSION ANNOTATION ─── witness lines + a value label offset from the
   measured entity. Linear: between two points. Angle: at the shared vertex.
   Radius/diameter: from a circle centre. The offset comes from the constraint's
   stored `place` (where the user dragged the dimension line to); when absent it
   falls back to the original fixed offsets so old dimensions still render. */
function cxDrawDimension(g,mvp,c){
  g.save();
  g.strokeStyle=CX_DIM_COL; g.lineWidth=1;
  let lx,ly,text;
  if(c.type==="linear"){
    const off=(c.place&&typeof c.place.off==="number")?c.place.off:6;
    const lbl=cxRenderLinear(g,mvp,c.entities[0],c.entities[1],off);
    if(lbl){ lx=lbl[0]; ly=lbl[1]; text=(Math.round(c.value*100)/100)+" mm"; }
  } else if(c.type==="hdist"||c.type==="vdist"){
    const off=(c.place&&typeof c.place.off==="number")?c.place.off:6;
    const lbl=cxRenderAxisDim(g,mvp,c.entities[0],c.entities[1],off,c.type==="hdist"?"u":"v");
    if(lbl){ lx=lbl[0]; ly=lbl[1]; text=(Math.round(c.value*100)/100)+" mm"; }
  } else if(c.type==="angle"){
    const s=cxEntityAnchorScreen(mvp,c.entities[0]);
    if(s){ const r=(c.place&&typeof c.place.r==="number")?c.place.r:24; lx=s[0]; ly=s[1]-r; text=(Math.round(c.value*180/Math.PI*10)/10)+"°"; }
  } else if(c.type==="radius"||c.type==="diameter"){
    const o=cxObj(c.entities[0].objId);
    if(o&&o.center){
      const cs=vp3World2Screen(mvp,o.center);
      if(cs){
        const off=(c.place&&typeof c.place.off==="number")?c.place.off:24;
        const ang=(c.place&&typeof c.place.ang==="number")?c.place.ang:-Math.PI/2;
        lx=cs[0]+Math.cos(ang)*off; ly=cs[1]+Math.sin(ang)*off;
        g.beginPath(); g.moveTo(cs[0],cs[1]); g.lineTo(lx,ly); g.stroke();   // leader from centre
        const val=(typeof sketchRadiusOf==="function")?sketchRadiusOf(o):(c.value||0);
        text=(c.type==="radius"?"R":"⌀")+(Math.round(val*100)/100);
      }
    }
  }
  if(text!=null&&lx!=null){
    const sel=(c.id===CX_SELECTED);
    const rect=cxDrawLabelChip(g,lx,ly,text,sel?CX_SEL_COL:CX_DIM_COL,sel);
    _cxLabelRects.push({id:c.id,x:rect[0],y:rect[1],w:rect[2],h:rect[3]});
  }
  g.restore();
}
/* draw a linear dimension (witness lines + offset dim line + arrows). The offset is
   a signed PLANE-SPACE distance: the two measured endpoints and their offset copies
   are all built in the sketch plane's [u,v] then projected world→screen, so the dim
   line stays perpendicular to the edge and rides straight out in the sketch plane —
   no screen-space skew on a tilted view. Returns the label anchor [lx,ly] (screen
   dim-line midpoint), or null if the object/points aren't projectable. */
function cxRenderLinear(g,mvp,e0,e1,off){
  const o=cxObj(e0.objId); if(!o||!o.cpts||!o.plane) return null;
  const p=o.cpts[e0.i], q=o.cpts[e1.i]; if(!p||!q) return null;
  const ex=q[0]-p[0], ey=q[1]-p[1], L=Math.hypot(ex,ey)||1;
  const nx=-ey/L*off, ny=ex/L*off;                     // plane-space perpendicular offset
  const toScr=(uu,vv)=>{ const w=sketchPlanePoint(o.plane,uu,vv); return vp3World2Screen(mvp,w); };
  const a=toScr(p[0],p[1]),        b=toScr(q[0],q[1]);
  const a2=toScr(p[0]+nx,p[1]+ny), b2=toScr(q[0]+nx,q[1]+ny);
  if(!a||!b||!a2||!b2) return null;
  g.beginPath(); g.moveTo(a[0],a[1]); g.lineTo(a2[0],a2[1]); g.moveTo(b[0],b[1]); g.lineTo(b2[0],b2[1]); g.stroke(); // witness
  g.beginPath(); g.moveTo(a2[0],a2[1]); g.lineTo(b2[0],b2[1]); g.stroke();                                          // dim line
  cxArrow(g,a2,b2); cxArrow(g,b2,a2);
  return [(a2[0]+b2[0])/2,(a2[1]+b2[1])/2];
}
/* draw a horizontal/vertical distance dimension. `axis` = "u" (horizontal component)
   or "v" (vertical component) in plane space. The dim line runs along that axis at a
   perpendicular offset, with witness lines from each endpoint — everything built in
   plane [u,v] then projected, so it tracks the geometry on a tilted view. Returns the
   screen label anchor, or null if not projectable. */
function cxRenderAxisDim(g,mvp,e0,e1,off,axis){
  const o=cxObj(e0.objId); if(!o||!o.cpts||!o.plane) return null;
  const p=o.cpts[e0.i], q=o.cpts[e1.i]; if(!p||!q) return null;
  const toScr=(uu,vv)=>{ const w=sketchPlanePoint(o.plane,uu,vv); return vp3World2Screen(mvp,w); };
  // dim-line endpoints share the measured component; the offset pushes them out along
  // the OTHER axis. Horizontal: keep u, drop v to a common line below both points.
  let a2u,a2v,b2u,b2v;
  if(axis==="u"){
    const base=Math.min(p[1],q[1])-Math.abs(off);
    a2u=p[0]; a2v=base; b2u=q[0]; b2v=base;
  } else {
    const base=Math.max(p[0],q[0])+Math.abs(off);
    a2u=base; a2v=p[1]; b2u=base; b2v=q[1];
  }
  const a=toScr(p[0],p[1]), b=toScr(q[0],q[1]);
  const a2=toScr(a2u,a2v), b2=toScr(b2u,b2v);
  if(!a||!b||!a2||!b2) return null;
  g.beginPath(); g.moveTo(a[0],a[1]); g.lineTo(a2[0],a2[1]); g.moveTo(b[0],b[1]); g.lineTo(b2[0],b2[1]); g.stroke(); // witness
  g.beginPath(); g.moveTo(a2[0],a2[1]); g.lineTo(b2[0],b2[1]); g.stroke();                                          // dim line
  cxArrow(g,a2,b2); cxArrow(g,b2,a2);
  return [(a2[0]+b2[0])/2,(a2[1]+b2[1])/2];
}
/* a rounded chip behind a value label; returns its rect [x,y,w,h] for hit-testing.
   A selected dimension draws a thicker accent border + tinted fill so the pick reads. */
function cxDrawLabelChip(g,lx,ly,text,col,selected){
  g.font="600 11px 'General Sans',system-ui,sans-serif";
  g.textAlign="center"; g.textBaseline="middle";
  const w=g.measureText(text).width+14, h=18, rx=lx-w/2, ry=ly-h/2, rad=h/2;
  g.fillStyle=selected?"rgba(14,30,44,0.92)":"rgba(6,10,14,0.85)"; g.strokeStyle=col; g.lineWidth=selected?2:1;
  g.beginPath();
  if(typeof g.roundRect==="function") g.roundRect(rx,ry,w,h,rad);
  else { g.moveTo(rx+rad,ry); g.arcTo(rx+w,ry,rx+w,ry+h,rad); g.arcTo(rx+w,ry+h,rx,ry+h,rad); g.arcTo(rx,ry+h,rx,ry,rad); g.arcTo(rx,ry,rx+w,ry,rad); }
  g.fill(); g.stroke();
  g.fillStyle=col; g.fillText(text,lx,ly+0.5);
  return [rx,ry,w,h];
}

/* ─── LIVE PLACING PREVIEW ─── drawn while the user is dragging the dimension line
   out (CPLACE.active), before the value is typed. Dashed + dimmer so it reads as a
   preview; uses the seeded (measured) value as placeholder text. */
function cxDrawPlacing(g,mvp){
  if(typeof CPLACE==="undefined"||!CPLACE.active||!CPLACE.off) return;
  const type=CPLACE.type, ents=CPLACE.ents;
  const seed=(typeof cpickSeedValue==="function")?cpickSeedValue(type,ents):0;
  g.save();
  g.strokeStyle=CX_DIM_COL; g.globalAlpha=0.65; g.lineWidth=1; g.setLineDash([4,3]);
  let lx,ly,text;
  if(type==="linear"){
    const lbl=cxRenderLinear(g,mvp,ents[0],ents[1],CPLACE.off.off||0);
    if(lbl){ lx=lbl[0]; ly=lbl[1]; text=(Math.round(seed*100)/100)+" mm"; }
  } else if(type==="hdist"||type==="vdist"){
    const lbl=cxRenderAxisDim(g,mvp,ents[0],ents[1],CPLACE.off.off||0,type==="hdist"?"u":"v");
    if(lbl){ lx=lbl[0]; ly=lbl[1]; text=(Math.round(seed*100)/100)+" mm"; }
  } else if(type==="radius"||type==="diameter"){
    const s=cxEntityAnchorScreen(mvp,ents[0]);
    if(s){ const off=CPLACE.off.off||24, ang=CPLACE.off.ang||-Math.PI/2;
      lx=s[0]+Math.cos(ang)*off; ly=s[1]+Math.sin(ang)*off;
      g.beginPath(); g.moveTo(s[0],s[1]); g.lineTo(lx,ly); g.stroke();
      text=(type==="radius"?"R":"⌀")+(Math.round(seed*100)/100); }
  } else if(type==="angle"){
    const s=cxEntityAnchorScreen(mvp,ents[0]);
    if(s){ const r=CPLACE.off.r||24; lx=s[0]; ly=s[1]-r; text=(Math.round(seed*10)/10)+"°"; }
  }
  g.setLineDash([]);
  if(text!=null&&lx!=null) cxDrawLabelChip(g,lx,ly,text,CX_DIM_COL);
  g.restore();
}
/* a small arrowhead at `to`, pointing away from `from` */
function cxArrow(g,from,to){
  const dx=to[0]-from[0], dy=to[1]-from[1], a=Math.atan2(dy,dx), s=5;
  g.beginPath(); g.moveTo(to[0],to[1]);
  g.lineTo(to[0]-Math.cos(a-0.4)*s,to[1]-Math.sin(a-0.4)*s);
  g.lineTo(to[0]-Math.cos(a+0.4)*s,to[1]-Math.sin(a+0.4)*s);
  g.closePath(); g.fillStyle=CX_DIM_COL; g.fill();
}

/* ─── SELECT + DRAG A DIMENSION ─── single-click a value label to select it (accent
   highlight), then drag it to reposition the annotation. Dragging writes ONLY the
   render offset (c.place) — never the solver value — so the geometry stays put while
   the dimension line slides in/out (matches P2's moveDim). */

/* which stored dimension's label rect is under (mx,my), or null. Rects are in the
   same stage-relative coords vp3AttachInput hands us, so no extra transform needed. */
function cxHitLabel(mx,my){
  for(let i=_cxLabelRects.length-1;i>=0;i--){
    const r=_cxLabelRects[i];
    if(mx>=r.x&&mx<=r.x+r.w&&my>=r.y&&my<=r.y+r.h) return r;
  }
  return null;
}

/* build a drag payload for the picked dimension (mirrors P2 makeDimDrag): linear /
   axis dims slide their perpendicular offset; radius/diameter swing their leader
   angle; angle dims auto-place (selectable but no drag). */
function cxMakeDimDrag(c,mx,my){
  if(c.type==="radius"||c.type==="diameter"){
    const o=cxObj(c.entities[0].objId), cs=(o&&o.center)?vp3World2Screen(VP3.mvp,o.center):null;
    return {id:c.id,mode:"radial",cx:cs?cs[0]:mx,cy:cs?cs[1]:my};
  }
  if(c.type==="angle") return {id:c.id,mode:"angular"};
  return {id:c.id,mode:"linear",sx:mx,sy:my,off0:(c.place&&typeof c.place.off==="number")?c.place.off:6};
}

/* the plane-units-per-screen-pixel scale along a linear dim's edge, so a screen-space
   drag distance maps back to the plane-space `off` the renderer consumes. Falls back
   to 1 when the edge isn't projectable this frame. */
function cxEdgePlanePerPixel(c){
  const o=cxObj(c.entities[0].objId); if(!o||!o.cpts) return 1;
  const p=o.cpts[c.entities[0].i], q=o.cpts[c.entities[1].i]; if(!p||!q) return 1;
  const planeLen=Math.hypot(p[0]-q[0],p[1]-q[1])||1;
  const a=cxScreenPt(VP3.mvp,c.entities[0].objId,c.entities[0].i);
  const b=cxScreenPt(VP3.mvp,c.entities[1].objId,c.entities[1].i);
  if(!a||!b) return 1;
  const scrLen=Math.hypot(a[0]-b[0],a[1]-b[1])||1;
  return planeLen/scrLen;                                  // plane units per screen px
}

/* ─── MOUSEDOWN HOOK ─── slotted into vp3AttachInput (gated by !CPICK.active so it
   never steals a create-pick). Returns true to consume the click. */
function cxMouseDown(e,mx,my){
  if(e.button!==0) return false;
  const hit=cxHitLabel(mx,my);
  if(hit){
    const c=CONSTRAINTS.find(k=>k.id===hit.id); if(!c) return false;
    CX_SELECTED=c.id;
    CX_DRAG=cxMakeDimDrag(c,mx,my);
    if(typeof vp3Draw==="function") vp3Draw();
    return true;                                           // consume — don't orbit
  }
  if(CX_SELECTED!=null){ CX_SELECTED=null; if(typeof vp3Draw==="function") vp3Draw(); }
  return false;                                            // empty click → let orbit run
}

window.addEventListener("mousemove",e=>{
  if(!CX_DRAG) return;
  const stage=document.getElementById("stage"); if(!stage) return;
  const r=stage.getBoundingClientRect(), mx=e.clientX-r.left, my=e.clientY-r.top;
  const c=CONSTRAINTS.find(k=>k.id===CX_DRAG.id); if(!c) return;
  if(!c.place) c.place={};
  if(CX_DRAG.mode==="radial"){
    c.place.ang=Math.atan2(my-CX_DRAG.cy,mx-CX_DRAG.cx);
  } else if(CX_DRAG.mode==="linear"){
    // perpendicular screen distance from the edge → plane-space offset the renderer reads.
    const a=cxScreenPt(VP3.mvp,c.entities[0].objId,c.entities[0].i);
    const b=cxScreenPt(VP3.mvp,c.entities[1].objId,c.entities[1].i);
    if(a&&b){
      const ex=b[0]-a[0], ey=b[1]-a[1], L=Math.hypot(ex,ey)||1;
      const nx=-ey/L, ny=ex/L;                             // screen-space edge normal
      const projPx=(mx-a[0])*nx+(my-a[1])*ny;              // signed perpendicular px
      const off=projPx*cxEdgePlanePerPixel(c);             // px → plane units
      c.place.off=Math.max(2,Math.abs(off))*(off<0?-1:1);  // keep off the edge, preserve side
    }
  }
  // reposition only touches render metadata — no solve, geometry stays put.
  if(typeof vp3Draw==="function") vp3Draw();
});
window.addEventListener("mouseup",()=>{ if(CX_DRAG) CX_DRAG=null; });

/* ─── DOUBLE-CLICK A DIMENSION LABEL → EDIT ITS VALUE ─── */
function cxDblClick(e){
  if(!_cxLabelRects.length) return;
  const stage=document.getElementById("stage"); if(!stage) return;
  const r=stage.getBoundingClientRect(), mx=e.clientX-r.left, my=e.clientY-r.top;
  const hit=_cxLabelRects.find(q=>mx>=q.x&&mx<=q.x+q.w&&my>=q.y&&my<=q.y+q.h);
  if(!hit) return;
  const c=CONSTRAINTS.find(k=>k.id===hit.id); if(!c) return;
  e.preventDefault(); e.stopPropagation();
  const seed=(c.type==="angle")?c.value*180/Math.PI:c.value;
  if(typeof cpickPromptValue==="function")
    cpickPromptValue(e,c.type,seed,(val)=>{ if(val!=null&&typeof setConstraintValue==="function") setConstraintValue(c.id,val); });
}
window.addEventListener("dblclick",cxDblClick,true);
