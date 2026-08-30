"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   viewport3d.js — a real software-rendered 3D viewport for the CAD stage. No
                   WebGL: the procedural PART is transformed → backface-culled →
                   depth-sorted → flat matcap-shaded to a 2D <canvas>, with a
                   ground grid (grid / dots / off), a sharp-edge wireframe, and a
                   Blender-style orientation gizmo overlay (top-left).

                   • Orbit  — RMB drag  (or LMB drag on empty space)
                   • Pan    — MMB drag  / Space+drag
                   • Zoom   — wheel
                   • Views  — viewcube buttons + gizmo axis balls snap orientation

                   The camera is eased (spherical yaw/pitch/dist around a pan
                   pivot) exactly like the UV-editor viewport it is ported from.
   ════════════════════════════════════════════════════════════════════════════ */

const VP3={
  canvas:null, ctx:null, w:0, h:0, dpr:Math.min(window.devicePixelRatio||1,2),
  yaw:0.9, pitch:0.42, dist:6.4, pan:[0,0.7,0],
  tYaw:0.9, tPitch:0.42, tDist:6.4, tPan:[0,0.7,0],
  matcap:null, showWire:true, shaded:true,
  gridMode:"dots",                 // dots | lines | off
  scr:null, viewNormals:null, mvp:null,
  running:false, spaceDown:false,
};
const ACCENT="#ffffff";            // white accent, per design
const ACCENT_RGB="255,255,255";

/* ─── procedural clay matcap (one, neutral — CAD wants a readable surface) ─── */
function vp3BuildMatcap(){
  const size=128, cv=document.createElement("canvas"); cv.width=cv.height=size;
  const g=cv.getContext("2d"), img=g.createImageData(size,size);
  const base=[178,184,196], rim=[26,30,38];      // cool machined grey
  for(let y=0;y<size;y++)for(let x=0;x<size;x++){
    const nx=(x/size)*2-1, ny=1-(y/size)*2, r2=nx*nx+ny*ny, i=(y*size+x)*4;
    if(r2>1){ img.data[i+3]=0; continue; }
    const nz=Math.sqrt(1-r2);
    const l=Math.max(0,nx*(-0.4)+ny*0.6+nz*0.7), t=Math.pow(l,1.15);
    let R=rim[0]+(base[0]-rim[0])*t, G=rim[1]+(base[1]-rim[1])*t, B=rim[2]+(base[2]-rim[2])*t;
    const sp=Math.pow(Math.max(0,nx*(-0.45)+ny*0.6+nz*0.66),36)*0.6*255;
    R=Math.min(255,R+sp); G=Math.min(255,G+sp); B=Math.min(255,B+sp);
    img.data[i]=R; img.data[i+1]=G; img.data[i+2]=B; img.data[i+3]=255;
  }
  g.putImageData(img,0,0);
  VP3.matcap={data:img.data,size};
}

function vp3Init(canvas){
  VP3.canvas=canvas; VP3.ctx=canvas.getContext("2d");
  vp3BuildMatcap(); vp3Resize(); vp3AttachInput();
  window.addEventListener("resize",vp3Resize);
  vp3Draw();
}
function vp3Resize(){
  const r=VP3.canvas.parentElement.getBoundingClientRect();
  VP3.w=Math.max(1,r.width); VP3.h=Math.max(1,r.height);
  VP3.canvas.width=VP3.w*VP3.dpr; VP3.canvas.height=VP3.h*VP3.dpr;
  VP3.ctx.setTransform(VP3.dpr,0,0,VP3.dpr,0,0);
  vp3Draw();
}

/* ─── camera ─── */
function vp3Eye(){
  const cy=Math.cos(VP3.pitch), sy=Math.sin(VP3.pitch), cx=Math.cos(VP3.yaw), sx=Math.sin(VP3.yaw);
  return [VP3.dist*cy*sx, VP3.dist*sy, VP3.dist*cy*cx];
}
function vp3MVP(){
  const target=VP3.pan.slice(), eye=V3.add(vp3Eye(),target);
  const view=M4.lookAt(eye,target,[0,1,0]);
  const proj=M4.perspective(42*Math.PI/180, VP3.w/VP3.h, 0.05, 100);
  return {mvp:M4.multiply(proj,view),view};
}
function vp3Basis(){
  const eye=V3.add(vp3Eye(),VP3.pan);
  const forward=V3.normalize(V3.sub(VP3.pan,eye));
  const right=V3.normalize(V3.cross(forward,[0,1,0]));
  const up=V3.cross(right,forward);
  return {right,up,forward};
}
function vp3World2Screen(mvp,p){
  const c=M4.transformPoint(mvp,p), w=c[3];
  if(w<=1e-4) return null;
  return [(c[0]/w*0.5+0.5)*VP3.w, (-c[1]/w*0.5+0.5)*VP3.h];
}

/* screen pixel → the world point where its view ray meets a plane.
   `planePoint`/`planeNormal` define the plane; returns [x,y,z] or null when the
   ray is parallel to (or points away from) the plane. Reads VP3.mvp from the last
   vp3Draw, inverting it to unproject two NDC depths into a ray. */
function vp3ScreenToPlane(px,py,planePoint,planeNormal,snapOverride){
  const inv=M4.invert(VP3.mvp);
  const nx=px/VP3.w*2-1, ny=1-py/VP3.h*2;         // NDC (y-flip mirrors vp3World2Screen)
  const un=(z)=>{ const c=M4.transformPoint(inv,[nx,ny,z]); const w=c[3]||1e-6; return [c[0]/w,c[1]/w,c[2]/w]; };
  const near=un(-1), far=un(1);
  const dir=V3.normalize(V3.sub(far,near));
  const denom=V3.dot(dir,planeNormal);
  if(Math.abs(denom)<1e-6) return null;
  const t=V3.dot(V3.sub(planePoint,near),planeNormal)/denom;
  if(t<0) return null;
  const hit=V3.add(near,V3.scale(dir,t));
  // snap: explicit override (Ctrl held) wins; otherwise fall back to the toggle.
  const doSnap=(snapOverride!==undefined)?snapOverride:(typeof cadSnap!=="undefined"&&cadSnap);
  if(doSnap){ const q=0.25; return [Math.round(hit[0]/q)*q, hit[1], Math.round(hit[2]/q)*q]; }
  return hit;
}

/* ─── draw ─── */
function vp3Draw(){
  const g=VP3.ctx; if(!g) return;
  g.clearRect(0,0,VP3.w,VP3.h);
  g.fillStyle="#050608"; g.fillRect(0,0,VP3.w,VP3.h);
  const {mvp,view}=vp3MVP(); VP3.mvp=mvp;
  if(VP3.gridMode!=="off") vp3Grid(g,mvp);

  const mc=VP3.matcap;
  // project + depth-sort every triangle (painter's, back-to-front)
  const order=[];
  for(let i=0;i<PART.tris.length;i++){
    const t=PART.tris[i], sp=[];
    let behind=false, zsum=0;
    for(const p of t.p){
      const c=M4.transformPoint(mvp,p), w=c[3]||1e-6;
      if(w<=1e-4){ behind=true; break; }
      sp.push([(c[0]/w*0.5+0.5)*VP3.w, (-c[1]/w*0.5+0.5)*VP3.h]);
      zsum+=c[2]/w;
    }
    if(behind) continue;
    // backface cull (screen winding)
    const[a,b,c]=sp;
    const area=(b[0]-a[0])*(c[1]-a[1])-(c[0]-a[0])*(b[1]-a[1]);
    if(area>=0) continue;
    // view-space normal for matcap lookup
    const vn=M4.transformDir(view,t.n), ln=Math.hypot(vn[0],vn[1],vn[2])||1;
    order.push({sp,z:zsum/3,nx:vn[0]/ln,ny:vn[1]/ln});
  }
  order.sort((p,q)=>q.z-p.z);
  for(const o of order){
    const col=VP3.shaded?vp3Sample(mc,o.nx,o.ny):"150,156,168";
    g.beginPath(); g.moveTo(o.sp[0][0],o.sp[0][1]); g.lineTo(o.sp[1][0],o.sp[1][1]); g.lineTo(o.sp[2][0],o.sp[2][1]); g.closePath();
    g.fillStyle="rgb("+col+")"; g.fill();
  }
  if(VP3.showWire) vp3Wire(g,mvp);
  if(typeof vp3DrawSketches==="function") vp3DrawSketches(g,mvp);   // sketch geometry pass
  if(typeof vp3DrawGizmo==="function") vp3DrawGizmo(g,mvp);         // transform gizmo pass
  if(typeof vp3DrawControlPoints==="function") vp3DrawControlPoints(g,mvp); // curve point overlay
  if(typeof ssDrawHover==="function") ssDrawHover(g,mvp);            // sub-object hover highlight
  if(typeof cxDrawOverlay==="function") cxDrawOverlay(g,mvp);        // constraint glyphs + dimensions
  if(typeof cxDrawPlacing==="function") cxDrawPlacing(g,mvp);        // live drag-out dimension preview
  if(typeof bevelDrawPreview==="function") bevelDrawPreview(g,mvp);  // live corner blend preview
  vp3Gizmo(g);                                                       // orientation gizmo (top-left)
}
function vp3Sample(mc,nx,ny){
  const s=mc.size;
  const x=Math.max(0,Math.min(s-1,Math.floor((nx*0.5+0.5)*s)));
  const y=Math.max(0,Math.min(s-1,Math.floor((1-(ny*0.5+0.5))*s)));
  const i=(y*s+x)*4;
  return mc.data[i]+","+mc.data[i+1]+","+mc.data[i+2];
}

/* sharp-edge wireframe overlay (white, faint) */
function vp3Wire(g,mvp){
  g.lineWidth=1; g.strokeStyle="rgba(255,255,255,0.22)";
  for(const[a,b]of PART.edges){
    const sa=vp3World2Screen(mvp,a), sb=vp3World2Screen(mvp,b);
    if(sa&&sb){ g.beginPath(); g.moveTo(sa[0],sa[1]); g.lineTo(sb[0],sb[1]); g.stroke(); }
  }
}

/* real ground grid on y=0 — `lines` draws segments, `dots` marks intersections.
   Axis lines through the origin get the coloured X (red) / Z (blue) tint. */
function vp3Grid(g,mvp){
  const half=6, step=0.5;
  if(VP3.gridMode==="lines"){
    g.lineWidth=1;
    for(let i=-half;i<=half;i+=step){
      const axis=Math.abs(i)<1e-6;
      g.strokeStyle=axis?"rgba(255,255,255,0.16)":"rgba(255,255,255,0.05)";
      const a1=vp3World2Screen(mvp,[-half,0,i]), b1=vp3World2Screen(mvp,[half,0,i]);
      if(a1&&b1){ g.beginPath(); g.moveTo(a1[0],a1[1]); g.lineTo(b1[0],b1[1]); g.stroke(); }
      const a2=vp3World2Screen(mvp,[i,0,-half]), b2=vp3World2Screen(mvp,[i,0,half]);
      if(a2&&b2){ g.beginPath(); g.moveTo(a2[0],a2[1]); g.lineTo(b2[0],b2[1]); g.stroke(); }
    }
  }else{
    for(let x=-half;x<=half;x+=step)for(let z=-half;z<=half;z+=step){
      const s=vp3World2Screen(mvp,[x,0,z]); if(!s) continue;
      const onAxis=Math.abs(x)<1e-6||Math.abs(z)<1e-6;
      g.fillStyle=onAxis?"rgba(255,255,255,0.30)":"rgba(255,255,255,0.16)";
      const r=onAxis?1.6:1.0;
      g.beginPath(); g.arc(s[0],s[1],r,0,7); g.fill();
    }
  }
}

/* ─── Blender-style orientation gizmo (top-left) ─── */
const GIZMO={cx:0,cy:0,r:34};
const GIZMO_AXES=[
  {v:[1,0,0], col:"#fc5a5a", pos:true,  label:"X", view:"Right"},
  {v:[-1,0,0],col:"#fc5a5a", pos:false, label:"",  view:"Left"},
  {v:[0,1,0], col:"#7bd66a", pos:true,  label:"Y", view:"Top"},
  {v:[0,-1,0],col:"#7bd66a", pos:false, label:"",  view:"Bottom"},
  {v:[0,0,1], col:"#5a8bfc", pos:true,  label:"Z", view:"Front"},
  {v:[0,0,-1],col:"#5a8bfc", pos:false, label:"",  view:"Back"},
];
function vp3Gizmo(g){
  const pad=52; GIZMO.cx=pad; GIZMO.cy=pad+6;
  const {right,up,forward}=vp3Basis();
  // project each axis dir into the gizmo's 2D screen basis (right = +x, up = -y)
  const pts=GIZMO_AXES.map(ax=>{
    const sx=V3.dot(ax.v,right), sy=V3.dot(ax.v,up), depth=V3.dot(ax.v,forward);
    return {ax, x:GIZMO.cx+sx*GIZMO.r, y:GIZMO.cy-sy*GIZMO.r, depth};
  }).sort((a,b)=>a.depth-b.depth);   // far axes first
  // connector spokes (subtle)
  g.lineWidth=2; g.strokeStyle="rgba(255,255,255,0.14)";
  for(const p of pts){ if(p.ax.pos){ g.beginPath(); g.moveTo(GIZMO.cx,GIZMO.cy); g.lineTo(p.x,p.y); g.stroke(); } }
  for(const p of pts){
    const front=p.depth>0.0, R=p.ax.pos?9:7;
    g.beginPath(); g.arc(p.x,p.y,R,0,7);
    if(p.ax.pos){
      g.fillStyle=p.ax.col; g.globalAlpha=front?1:0.75; g.fill(); g.globalAlpha=1;
      g.fillStyle="rgba(0,0,0,0.85)"; g.font="700 10px 'General Sans',sans-serif";
      g.textAlign="center"; g.textBaseline="middle"; g.fillText(p.ax.label,p.x,p.y+0.5);
    }else{
      g.fillStyle="rgba(10,12,16,0.9)"; g.fill();
      g.lineWidth=1.6; g.strokeStyle=p.ax.col; g.globalAlpha=0.85; g.stroke(); g.globalAlpha=1;
    }
  }
}
/* hit-test the gizmo balls; returns the view name or null. */
function vp3GizmoHit(mx,my){
  const {right,up}=vp3Basis();
  let best=null,bd=13*13;
  for(const ax of GIZMO_AXES){
    const x=GIZMO.cx+V3.dot(ax.v,right)*GIZMO.r, y=GIZMO.cy-V3.dot(ax.v,up)*GIZMO.r;
    const d=(mx-x)*(mx-x)+(my-y)*(my-y);
    if(d<bd){ bd=d; best=ax.view; }
  }
  return best;
}

/* ─── orientation presets (viewcube + gizmo) ─── */
const VP3_VIEWS={
  Top:   {yaw:0,        pitch:1.4},
  Bottom:{yaw:0,        pitch:-1.4},
  Front: {yaw:0,        pitch:0},
  Back:  {yaw:Math.PI,  pitch:0},
  Right: {yaw:Math.PI/2,pitch:0},
  Left:  {yaw:-Math.PI/2,pitch:0},
  Iso:   {yaw:0.9,      pitch:0.42},
};
function vp3SetView(name){
  const v=VP3_VIEWS[name]; if(!v) return;
  VP3.tYaw=v.yaw; VP3.tPitch=v.pitch; vp3Arm();
}
function vp3ResetView(){ VP3.tYaw=0.9; VP3.tPitch=0.42; VP3.tDist=6.4; VP3.tPan=[0,0.7,0]; vp3Arm(); }
function vp3SetGrid(mode){ VP3.gridMode=mode; vp3Draw(); }
function vp3SetShaded(on){ VP3.shaded=on; vp3Draw(); }
function vp3SetWire(on){ VP3.showWire=on; vp3Draw(); }

/* ─── input ─── */
let vp3Drag=null;
function vp3AttachInput(){
  const cv=VP3.canvas, wrap=cv.parentElement;
  wrap.addEventListener("mousedown",e=>{
    if(e.target.closest(".vp-pill,.vp-viewtools,.vp-cmdbar,.toast")) return;
    const r=wrap.getBoundingClientRect(), mx=e.clientX-r.left, my=e.clientY-r.top;
    const objectMode=(typeof CAD==="undefined")||CAD.imode!=="sketch";
    // OBJECT mode → transform gizmo gets first refusal; SKETCH mode → draw tool.
    if(objectMode){
      if(typeof gizmoMouseDown==="function" && gizmoMouseDown(e,mx,my)) return;
    } else {
      if(typeof sketchMouseDown==="function" && sketchMouseDown(e,mx,my)) return;
    }
    // clicking an EXISTING dimension label selects / drags it (before arm-pick and
    // before orbit) — but only when NOT arming a create tool, so a create-pick click
    // still reaches cpickMouseDown below.
    if(typeof CPICK!=="undefined" && !CPICK.active && typeof cxMouseDown==="function" && cxMouseDown(e,mx,my)) return;
    // an armed dimension/constraint tool owns the click (pick entities → solver).
    if(typeof cpickMouseDown==="function" && cpickMouseDown(e,mx,my)) return;
    // an armed corner-blend tool owns the click (pick a corner → drag to bevel).
    if(typeof bevelMouseDown==="function" && bevelMouseDown(e,mx,my)) return;
    // editable control points/handles of a selected curve (both modes, after the
    // gizmo/draw hook so they don't fight a live tool).
    if(typeof ptMouseDown==="function" && ptMouseDown(e,mx,my)) return;
    // auto-detect sub-object pick (Sketch mode): vertex / edge / face of any sketch
    // object. Returns false on no-hit so orbit/pan below still runs.
    if(typeof ssMouseDown==="function" && ssMouseDown(e,mx,my)) return;
    // orientation gizmo click → snap view (both modes)
    if(e.button===0){ const hit=vp3GizmoHit(mx,my); if(hit){ vp3SetView(hit); toast(hit+" view","box"); return; } }
    // OBJECT mode LMB pick: select a sketch object (attaches the gizmo) or clear on
    // empty. A hit consumes the click; an empty click deselects but still orbits.
    if(objectMode && e.button===0 && typeof sketchClickSelect==="function" && sketchClickSelect(mx,my,e)) return;
    if(e.button===1||VP3.spaceDown){ vp3Drag={type:"pan",sx:mx,sy:my,p0:VP3.tPan.slice(),basis:vp3Basis()}; e.preventDefault(); return; }
    if(e.button===2){ vp3Drag={type:"orbit",sx:mx,sy:my,y0:VP3.tYaw,p0:VP3.tPitch}; return; }
    if(e.button===0){ vp3Drag={type:"orbit",sx:mx,sy:my,y0:VP3.tYaw,p0:VP3.tPitch}; }
  });
  window.addEventListener("mousemove",e=>{
    if(!vp3Drag) return;
    const r=wrap.getBoundingClientRect(), mx=e.clientX-r.left, my=e.clientY-r.top;
    const dx=mx-vp3Drag.sx, dy=my-vp3Drag.sy;
    if(vp3Drag.type==="orbit"){
      VP3.tYaw=vp3Drag.y0-dx*0.01;
      VP3.tPitch=Math.max(-1.45,Math.min(1.45,vp3Drag.p0+dy*0.01));
      vp3Arm();
    }else if(vp3Drag.type==="pan"){
      const k=0.0026*VP3.dist, {right,up}=vp3Drag.basis;
      VP3.tPan=[vp3Drag.p0[0]-(right[0]*dx-up[0]*dy)*k,
                vp3Drag.p0[1]-(right[1]*dx-up[1]*dy)*k,
                vp3Drag.p0[2]-(right[2]*dx-up[2]*dy)*k];
      vp3Arm();
    }
  });
  window.addEventListener("mouseup",()=>{ vp3Drag=null; });
  wrap.addEventListener("wheel",e=>{
    e.preventDefault();
    VP3.tDist=Math.max(1.8,Math.min(22,VP3.tDist*(e.deltaY<0?0.9:1.1)));
    vp3Arm();
  },{passive:false});
  wrap.addEventListener("contextmenu",e=>e.preventDefault());
  window.addEventListener("keydown",e=>{ if(e.code==="Space"&&!e.repeat){ VP3.spaceDown=true; } });
  window.addEventListener("keyup",e=>{ if(e.code==="Space"){ VP3.spaceDown=false; } });
}
function vp3Arm(){ if(!VP3.running){ VP3.running=true; requestAnimationFrame(vp3Tick); } }
function vp3Tick(){
  const k=0.28;
  VP3.yaw+=(VP3.tYaw-VP3.yaw)*k; VP3.pitch+=(VP3.tPitch-VP3.pitch)*k; VP3.dist+=(VP3.tDist-VP3.dist)*k;
  for(let a=0;a<3;a++) VP3.pan[a]+=(VP3.tPan[a]-VP3.pan[a])*k;
  vp3Draw();
  const done=Math.abs(VP3.tYaw-VP3.yaw)<1e-4&&Math.abs(VP3.tPitch-VP3.pitch)<1e-4&&
             Math.abs(VP3.tDist-VP3.dist)<1e-3&&Math.abs(VP3.tPan[0]-VP3.pan[0])<1e-3&&
             Math.abs(VP3.tPan[1]-VP3.pan[1])<1e-3&&Math.abs(VP3.tPan[2]-VP3.pan[2])<1e-3;
  if(done){ VP3.running=false; vp3Draw(); } else requestAnimationFrame(vp3Tick);
}
