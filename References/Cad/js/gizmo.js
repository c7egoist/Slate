"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   gizmo.js — the in-scene transform gizmo, ported from the deprecated C++
              TransformGizmo (Engine-Deprecated/Internal/Interface/Gizmos). It
              shows on the selected sketch object and drives its transform:

                • Translate — axis arrows + translucent plane quads
                • Rotate    — three rotation rings
                • Scale     — axis handles ending in boxes + a uniform centre
                • Universal — everything shown together

              Palette is 1:1 with the C++ constants (X green / Y blue / Z red;
              planes cyan/magenta/yellow; centre white; hover amber). Handles are
              projected to the 2D canvas at fixed pixel sizes and hit-tested in
              screen space — a pragmatic port of the engine's world-per-pixel /
              ray-picking gizmo, keeping the look + feel identical.

              Hotkeys (Plasticity): G translate · R rotate · S scale · X/Y/Z lock
              an axis · Esc cancel drag · Enter confirm. Registered capture-phase
              so they win over the toolbar's letter shortcuts when the gizmo is up.
   ════════════════════════════════════════════════════════════════════════════ */

/* exact palette from TransformGizmo.cpp */
const GZ_PALETTE={
  x:"#31F50A", y:"#1C64FC", z:"#FA1414",
  pxy:"#1BF5E6", pyz:"#DF22F5", pxz:"#F7DB14",
  center:"#FFFFFF", hover:"#FFAE42",
};
/* pixel-unit handle sizes (Configuration defaults from the C++ header).
   axisLen is shared by translate arrows AND scale axes so the two modes present
   an identical footprint; the rotation ring sits just outside the axis tips. */
const GZ_CFG={ axisLen:82, axisThick:6, tipCone:18, scaleBox:11, ringR:96, ringThick:6,
               planeOff:28, planeSize:22, uniform:14, pickTol:5 };

const GZ={
  visible:false, mode:"translate", target:null, origin:[0,0,0],
  hover:null, active:null, axisLock:null, drag:null, _dim:1, snap:false,
};

/* attach/detach against a sketch object */
function gizmoAttach(obj){
  GZ.target=obj; GZ.visible=true; GZ.origin=sketchWorldCentroid(obj);
  vp3Draw();
}
function gizmoDetach(){ GZ.target=null; GZ.visible=false; GZ.active=null; GZ.hover=null; GZ.drag=null; if(typeof gizmoHudHide==="function") gizmoHudHide(); }
function gizmoSetMode(m){ GZ.mode=m; if(GZ.visible) vp3Draw(); }

/* ─── LAYOUT ─── project origin + per-axis screen directions once per paint. */
function gizmoLayout(mvp){
  if(!GZ.target) return null;
  GZ.origin=sketchWorldCentroid(GZ.target);
  const o=vp3World2Screen(mvp,GZ.origin); if(!o) return null;
  const AX={x:[1,0,0],y:[0,1,0],z:[0,0,1]};
  const dir={}, tip={};
  const eps=0.5;   // world offset to sample the axis screen direction
  for(const k in AX){
    const wp=[GZ.origin[0]+AX[k][0]*eps,GZ.origin[1]+AX[k][1]*eps,GZ.origin[2]+AX[k][2]*eps];
    const sp=vp3World2Screen(mvp,wp);
    if(!sp){ dir[k]=[0,0]; tip[k]=o; continue; }
    let dx=sp[0]-o[0], dy=sp[1]-o[1]; const len=Math.hypot(dx,dy)||1;
    dir[k]=[dx/len,dy/len];
    tip[k]=[o[0]+dir[k][0]*GZ_CFG.axisLen, o[1]+dir[k][1]*GZ_CFG.axisLen];
  }
  return {o,dir,tip};
}
function gizmoShows(op){ return GZ.mode==="universal"||GZ.mode===op; }

/* ─── DRAW ─── (hooked into vp3Draw before the orientation gizmo) */
function gizmoModeAllows(){ return (typeof CAD==="undefined")||CAD.imode!=="sketch"; }
/* while dragging a single axis (or a plane's two axes) the other handles fade and
   a full-length guide line is drawn through the origin — Plasticity/Blender feel. */
function gizmoActiveAxes(){
  const id=GZ.active; if(!id) return null;                 // only during a drag
  if(id==="center") return null;                           // free / uniform: no lock
  if(GZ.mode==="rotate") return [gizmoAxisOf(id)];         // ring: constrained axis
  if(id.length===3) return [id[1],id[2]];                  // translate plane quad
  return [gizmoAxisOf(id)];                                // single axis (x / sx)
}
function gizmoDrawGuide(g,o,dir,ax,col){
  // a long line through the origin along the axis's screen direction
  const d=dir[ax], big=4000;
  g.save(); g.globalAlpha=0.5; g.strokeStyle=col; g.lineWidth=1; g.setLineDash([6,5]);
  g.beginPath();
  g.moveTo(o[0]-d[0]*big,o[1]-d[1]*big);
  g.lineTo(o[0]+d[0]*big,o[1]+d[1]*big);
  g.stroke(); g.restore();
}
function vp3DrawGizmo(g,mvp){
  if(!GZ.visible||!GZ.target||SKETCH.drawing||!gizmoModeAllows()) return;
  const L=gizmoLayout(mvp); if(!L) return;
  const {o,dir,tip}=L;
  const axisCol={x:GZ_PALETTE.x,y:GZ_PALETTE.y,z:GZ_PALETTE.z};
  const hoverCol=GZ_PALETTE.hover;
  const isHot=(id)=>GZ.active===id||(!GZ.active&&GZ.hover===id)||GZ.axisLock===id;
  // axis-constrain readout: which axes are engaged, and a dim factor for the rest
  const lockAxes=gizmoActiveAxes();
  const engaged=(id)=>{
    if(!lockAxes) return true;                     // nothing constrained → all bright
    if(id==="center") return false;
    return lockAxes.includes(gizmoAxisOf(id))||(id.length===3&&lockAxes.includes(id[1])&&lockAxes.includes(id[2]));
  };
  // primitives multiply GZ._dim into their own alpha so an inner globalAlpha=1
  // reset can't defeat the fade; 1 = full bright, <1 = dimmed non-active handle.
  const withDim=(id,fn)=>{ GZ._dim=(lockAxes&&!engaged(id))?0.16:1; fn(); GZ._dim=1; };

  // guide line(s) along the constrained axis/axes
  if(lockAxes) for(const ax of lockAxes) gizmoDrawGuide(g,o,dir,ax,axisCol[ax]);

  if(gizmoShows("translate")){
    withDim("pxy",()=>gizmoDrawPlane(g,o,dir,"x","y",GZ_PALETTE.pxy,isHot("pxy")));
    withDim("pyz",()=>gizmoDrawPlane(g,o,dir,"y","z",GZ_PALETTE.pyz,isHot("pyz")));
    withDim("pxz",()=>gizmoDrawPlane(g,o,dir,"x","z",GZ_PALETTE.pxz,isHot("pxz")));
    for(const k of ["x","y","z"]){
      const col=isHot(k)?hoverCol:axisCol[k];
      withDim(k,()=>gizmoArrow(g,o,tip[k],dir[k],col));
    }
    withDim("center",()=>gizmoCenter(g,o,isHot("center")?hoverCol:GZ_PALETTE.center,false));
  }
  if(gizmoShows("rotate")){
    for(const k of ["x","y","z"]){
      const col=isHot("r"+k)?hoverCol:axisCol[k];
      withDim("r"+k,()=>gizmoRing(g,o,dir,k,col));
    }
  }
  if(gizmoShows("scale")){
    for(const k of ["x","y","z"]){
      const col=isHot("s"+k)?hoverCol:axisCol[k];
      withDim("s"+k,()=>gizmoScaleAxis(g,o,tip[k],col));
    }
    withDim("center",()=>gizmoCenter(g,o,isHot("center")?hoverCol:GZ_PALETTE.center,true));
  }
}
function gizmoArrow(g,o,tip,d,col){
  g.save(); g.globalAlpha=GZ._dim;
  g.strokeStyle=col; g.lineWidth=3; g.lineCap="round";
  g.beginPath(); g.moveTo(o[0],o[1]); g.lineTo(tip[0],tip[1]); g.stroke();
  // cone tip
  const back=[tip[0]-d[0]*GZ_CFG.tipCone,tip[1]-d[1]*GZ_CFG.tipCone];
  const perp=[-d[1],d[0]], w=6;
  g.fillStyle=col; g.beginPath();
  g.moveTo(tip[0],tip[1]);
  g.lineTo(back[0]+perp[0]*w,back[1]+perp[1]*w);
  g.lineTo(back[0]-perp[0]*w,back[1]-perp[1]*w);
  g.closePath(); g.fill();
  g.restore();
}
function gizmoScaleAxis(g,o,tip,col){
  g.save(); g.globalAlpha=GZ._dim;
  g.strokeStyle=col; g.lineWidth=3; g.lineCap="round";
  g.beginPath(); g.moveTo(o[0],o[1]); g.lineTo(tip[0],tip[1]); g.stroke();
  const s=GZ_CFG.scaleBox; g.fillStyle=col;
  g.fillRect(tip[0]-s/2,tip[1]-s/2,s,s);
  g.strokeStyle="rgba(0,0,0,0.35)"; g.lineWidth=1; g.strokeRect(tip[0]-s/2,tip[1]-s/2,s,s);
  g.restore();
}
function gizmoPlaneCorner(o,dir,a,b,ka,kb){
  return [o[0]+dir[a][0]*ka+dir[b][0]*kb, o[1]+dir[a][1]*ka+dir[b][1]*kb];
}
function gizmoDrawPlane(g,o,dir,a,b,col,hot){
  const off=GZ_CFG.planeOff, sz=GZ_CFG.planeSize;
  const c0=gizmoPlaneCorner(o,dir,a,b,off,off);
  const c1=gizmoPlaneCorner(o,dir,a,b,off+sz,off);
  const c2=gizmoPlaneCorner(o,dir,a,b,off+sz,off+sz);
  const c3=gizmoPlaneCorner(o,dir,a,b,off,off+sz);
  g.globalAlpha=(hot?0.7:0.45)*GZ._dim; g.fillStyle=hot?GZ_PALETTE.hover:col;
  g.beginPath(); g.moveTo(c0[0],c0[1]); g.lineTo(c1[0],c1[1]); g.lineTo(c2[0],c2[1]); g.lineTo(c3[0],c3[1]); g.closePath(); g.fill();
  g.globalAlpha=GZ._dim; g.strokeStyle=hot?GZ_PALETTE.hover:col; g.lineWidth=1; g.stroke();
  g.globalAlpha=1;
}
function gizmoRing(g,o,dir,k,col){
  // ellipse spanned by the two in-plane screen axes (the axes NOT equal to k)
  const others=["x","y","z"].filter(a=>a!==k);
  const a=dir[others[0]], b=dir[others[1]], R=GZ_CFG.ringR;
  g.save(); g.globalAlpha=GZ._dim;
  g.strokeStyle=col; g.lineWidth=2.5; g.beginPath();
  for(let i=0;i<=48;i++){
    const th=i/48*Math.PI*2, ca=Math.cos(th)*R, sb=Math.sin(th)*R;
    const x=o[0]+a[0]*ca+b[0]*sb, y=o[1]+a[1]*ca+b[1]*sb;
    if(i===0) g.moveTo(x,y); else g.lineTo(x,y);
  }
  g.stroke(); g.restore();
}
function gizmoCenter(g,o,col,box){
  const s=GZ_CFG.uniform;
  g.save(); g.globalAlpha=GZ._dim;
  if(box){ g.fillStyle=col; g.globalAlpha=0.9*GZ._dim; g.fillRect(o[0]-s/2,o[1]-s/2,s,s); }
  else { g.strokeStyle=col; g.lineWidth=1.6; g.beginPath(); g.arc(o[0],o[1],s*0.5,0,7); g.stroke(); }
  g.restore();
}

/* ─── HIT-TEST ─── screen-space, priority: centre → planes → axes → rings. */
function gizmoHitTest(mx,my){
  if(!GZ.visible||!GZ.target) return null;
  const L=gizmoLayout(VP3.mvp); if(!L) return null;
  const {o,dir,tip}=L, tol=GZ_CFG.pickTol;
  const near=(p,q,r)=>((mx-p)*(mx-p)+(my-q)*(my-q))<=r*r;
  // centre
  if(near(o[0],o[1],GZ_CFG.uniform*0.6+tol)) return "center";
  if(gizmoShows("translate")){
    // planes
    for(const [id,a,b] of [["pxy","x","y"],["pyz","y","z"],["pxz","x","z"]]){
      if(gizmoInPlane(mx,my,o,dir,a,b)) return id;
    }
  }
  const axes=gizmoShows("translate")?["x","y","z"]:gizmoShows("scale")?["x","y","z"]:[];
  const axisPrefix=gizmoShows("translate")?"":"s";
  for(const k of axes){ if(gizmoNearSeg(mx,my,o,tip[k],GZ_CFG.axisThick+tol)) return axisPrefix+k; }
  if(gizmoShows("rotate")){
    for(const k of ["x","y","z"]){ if(gizmoNearRing(mx,my,o,dir,k)) return "r"+k; }
  }
  return null;
}
function gizmoNearSeg(px,py,a,b,tol){
  const vx=b[0]-a[0], vy=b[1]-a[1], wx=px-a[0], wy=py-a[1];
  const L2=vx*vx+vy*vy||1; let t=(wx*vx+wy*vy)/L2; t=Math.max(0,Math.min(1,t));
  const cx=a[0]+vx*t, cy=a[1]+vy*t;
  return (px-cx)*(px-cx)+(py-cy)*(py-cy)<=tol*tol;
}
function gizmoInPlane(mx,my,o,dir,a,b){
  const off=GZ_CFG.planeOff, sz=GZ_CFG.planeSize, c=off+sz/2;
  const cen=gizmoPlaneCorner(o,dir,a,b,c,c);
  return Math.abs(mx-cen[0])<=sz/2+GZ_CFG.pickTol && Math.abs(my-cen[1])<=sz/2+GZ_CFG.pickTol;
}
function gizmoNearRing(mx,my,o,dir,k){
  const others=["x","y","z"].filter(x=>x!==k), a=dir[others[0]], b=dir[others[1]], R=GZ_CFG.ringR;
  // approximate: distance from origin ≈ R along the ellipse — sample and test
  let best=1e9;
  for(let i=0;i<48;i++){
    const th=i/48*Math.PI*2, ca=Math.cos(th)*R, sb=Math.sin(th)*R;
    const x=o[0]+a[0]*ca+b[0]*sb, y=o[1]+a[1]*ca+b[1]*sb;
    const d=(mx-x)*(mx-x)+(my-y)*(my-y); if(d<best) best=d;
  }
  return best<=(GZ_CFG.ringThick+GZ_CFG.pickTol)*(GZ_CFG.ringThick+GZ_CFG.pickTol);
}

/* ─── PRECISE-VALUE HUD ─── a Plasticity-style numeric field that appears while a
   handle is being dragged. It shows the live delta for the active axis/mode and
   lets the user type an EXACT value (+ Enter) that is applied in realtime. */
let _gzHud=null;
function gizmoHudEl(){
  if(_gzHud) return _gzHud;
  const el=document.createElement("div"); el.className="gz-hud"; el.id="gzHud";
  el.innerHTML=`<span class="gz-hud-lbl" id="gzHudLbl">Move</span>
    <input class="gz-hud-inp" id="gzHudInp" type="text" autocomplete="off">
    <span class="gz-hud-unit" id="gzHudUnit">mm</span>`;
  document.body.appendChild(el); _gzHud=el;
  const inp=el.querySelector("#gzHudInp");
  inp.addEventListener("keydown",e=>{
    e.stopPropagation();                     // never trigger gizmo/tool hotkeys
    if(e.key==="Enter"){ gizmoApplyTypedValue(parseFloat(inp.value)); inp.blur(); }
    if(e.key==="Escape"){ gizmoHudHide(); }
  });
  return el;
}
/* which single scalar the HUD edits, given the active handle + mode */
function gizmoHudSpec(){
  if(!GZ.drag) return null;
  const mode=GZ.mode, id=GZ.drag.id;
  if(mode==="rotate")  return {label:"Rotate "+gizmoAxisOf(id).toUpperCase(), unit:"°",  kind:"rot", axis:gizmoAxisOf(id)};
  if(mode==="scale")   return {label:id==="center"?"Scale":"Scale "+gizmoAxisOf(id).toUpperCase(), unit:"×", kind:"scl", axis:id==="center"?null:gizmoAxisOf(id)};
  // translate: single axis / plane / centre
  const axis=(id.length===1)?id:null;
  return {label:"Move"+(axis?" "+axis.toUpperCase():""), unit:"mm", kind:"trn", axis};
}
function gizmoHudShow(){
  const el=gizmoHudEl(), spec=gizmoHudSpec(); if(!spec){ gizmoHudHide(); return; }
  el._spec=spec;
  el.querySelector("#gzHudLbl").textContent=spec.label;
  el.querySelector("#gzHudUnit").textContent=spec.unit;
  gizmoHudSync();
  el.classList.add("show");
}
/* reflect the current dragged value into the field (unless the user is typing) */
function gizmoHudSync(){
  if(!_gzHud||!_gzHud._spec||!GZ.target) return;
  const inp=_gzHud.querySelector("#gzHudInp");
  if(document.activeElement===inp) return;
  const tr=GZ.target.transform, s=_gzHud._spec;
  let v=0;
  if(s.kind==="trn") v=s.axis?tr.t[{x:0,y:1,z:2}[s.axis]]:V3.length(tr.t);
  else if(s.kind==="rot") v=(tr.r[{x:0,y:1,z:2}[s.axis]]||0)*180/Math.PI;
  else if(s.kind==="scl") v=s.axis?tr.s[{x:0,y:1,z:2}[s.axis]]:tr.s[0];
  inp.value=(Math.round(v*100)/100).toString();
  // park the HUD just above the gizmo origin
  const stage=document.getElementById("stage"); if(stage&&GZ.origin){
    const o=vp3World2Screen(VP3.mvp,GZ.origin), r=stage.getBoundingClientRect();
    if(o){ _gzHud.style.left=(r.left+o[0]+18)+"px"; _gzHud.style.top=(r.top+o[1]-14)+"px"; }
  }
}
function gizmoHudHide(){ if(_gzHud) _gzHud.classList.remove("show"); }
function gizmoHudFocus(){ const el=gizmoHudEl(); const inp=el.querySelector("#gzHudInp"); inp.focus(); inp.select(); }
/* apply an exact typed value to the active handle's axis/mode, then repaint */
function gizmoApplyTypedValue(v){
  if(isNaN(v)||!GZ.drag||!GZ.target){ return; }
  const tr=GZ.target.transform, s=_gzHud&&_gzHud._spec; if(!s) return;
  const d=GZ.drag;
  if(s.kind==="trn"){
    if(s.axis){ const i={x:0,y:1,z:2}[s.axis]; tr.t=d.t0.slice(); tr.t[i]=v; }
  }else if(s.kind==="rot"){
    const i={x:0,y:1,z:2}[s.axis]; tr.r=d.r0.slice(); tr.r[i]=v*Math.PI/180;
  }else if(s.kind==="scl"){
    if(s.axis){ const i={x:0,y:1,z:2}[s.axis]; tr.s=d.s0.slice(); tr.s[i]=Math.max(0.05,v); }
    else tr.s=[Math.max(0.05,v),Math.max(0.05,v),Math.max(0.05,v)];
  }
  vp3Draw();
  if(GZ.target&&typeof pushHistory==="function")
    pushHistory("transform",GZ.target.name,(s.label)+" = "+v+s.unit);
  if(typeof renderProps==="function") renderProps();
  gizmoHudHide(); GZ.drag=null;
}

/* ─── DRAG ─── mousedown pre-empts orbit; move integrates; up finalizes. */
function gizmoMouseDown(e,mx,my){
  if(!GZ.visible||!GZ.target||SKETCH.drawing||!gizmoModeAllows()||e.button!==0) return false;
  const hit=gizmoHitTest(mx,my); if(!hit) return false;
  const L=gizmoLayout(VP3.mvp);
  const tr=GZ.target.transform;
  GZ.active=hit;
  GZ.drag={
    id:hit, sx:mx, sy:my,
    t0:tr.t.slice(), r0:tr.r.slice(), s0:tr.s.slice(),
    o:L.o.slice(), dir:{x:L.dir.x.slice(),y:L.dir.y.slice(),z:L.dir.z.slice()},
    a0:Math.atan2(my-L.o[1],mx-L.o[0]),
  };
  gizmoHudShow();
  return true;
}
function gizmoAxisOf(id){ return id.replace(/^[rs]/,""); }
function gizmoDragMove(mx,my){
  if(!GZ.active||!GZ.drag) return;
  const d=GZ.drag, tr=GZ.target.transform, dx=mx-d.sx, dy=my-d.sy;
  const k=VP3.dist*0.0026;                       // world units per pixel
  const axisWorld={x:[1,0,0],y:[0,1,0],z:[0,0,1]};
  if(GZ.mode==="translate"){
    if(d.id==="center"){                          // screen-plane translate
      const {right,up}=vp3Basis();
      tr.t=[d.t0[0]+(right[0]*dx-up[0]*dy)*k, d.t0[1]+(right[1]*dx-up[1]*dy)*k, d.t0[2]+(right[2]*dx-up[2]*dy)*k];
    } else if(d.id.length===3){                   // plane quad (pxy/pyz/pxz)
      const a=d.id[1], b=d.id[2];
      const amt=(sd,axis)=>((dx*sd[0]+dy*sd[1])*k);
      tr.t=d.t0.slice();
      [a,b].forEach(ax=>{ const along=amt(d.dir[ax],ax); axisWorld[ax].forEach((c,i)=>tr.t[i]+=c*along); });
    } else {                                       // single axis
      const ax=d.id, along=(dx*d.dir[ax][0]+dy*d.dir[ax][1])*k;
      tr.t=d.t0.slice(); axisWorld[ax].forEach((c,i)=>tr.t[i]+=c*along);
    }
    if(GZ.snap){ const q=0.25; tr.t=tr.t.map(v=>Math.round(v/q)*q); }  // Ctrl-snap translate
  } else if(GZ.mode==="rotate"){
    const ax=gizmoAxisOf(d.id), idx={x:0,y:1,z:2}[ax]||0;
    const ang=Math.atan2(my-d.o[1],mx-d.o[0])-d.a0;
    tr.r=d.r0.slice(); tr.r[idx]=d.r0[idx]+ang;
  } else if(GZ.mode==="scale"){
    if(d.id==="center"){
      const f=1+(-dy)*0.01; tr.s=[Math.max(0.05,d.s0[0]*f),Math.max(0.05,d.s0[1]*f),Math.max(0.05,d.s0[2]*f)];
    } else {
      const ax=gizmoAxisOf(d.id), idx={x:0,y:1,z:2}[ax]||0;
      const along=(dx*d.dir[ax][0]+dy*d.dir[ax][1]);
      tr.s=d.s0.slice(); tr.s[idx]=Math.max(0.05,d.s0[idx]*(1+along*0.01));
    }
  }
  gizmoHudSync();
  vp3Draw();
}
function gizmoDragUp(){
  if(!GZ.active) return;
  const mode=GZ.mode;
  GZ.active=null;                            // keep GZ.drag snapshot so the HUD can
                                             // still apply a typed exact value.
  if(GZ.target&&typeof pushHistory==="function")
    pushHistory("transform",GZ.target.name,mode.charAt(0).toUpperCase()+mode.slice(1)+" applied");
  gizmoHudSync(); gizmoHudFocus();           // hand focus to the value field
  if(typeof renderProps==="function") renderProps();
  vp3Draw();
}

/* ─── window listeners: drag + hover ─── */
window.addEventListener("mousemove",e=>{
  const stage=document.getElementById("stage"); if(!stage) return;
  const r=stage.getBoundingClientRect(), mx=e.clientX-r.left, my=e.clientY-r.top;
  if(GZ.active){ GZ.snap=!!e.ctrlKey; gizmoDragMove(mx,my); return; }
  if(GZ.visible&&!SKETCH.drawing&&gizmoModeAllows()){
    const h=gizmoHitTest(mx,my);
    if(h!==GZ.hover){ GZ.hover=h; vp3Draw(); }
  }
});
window.addEventListener("mouseup",()=>{ if(GZ.active) gizmoDragUp(); });
/* dismiss the value HUD when the pointer goes down anywhere that is not the HUD
   and not on a live gizmo handle (a fresh handle press re-opens it). */
window.addEventListener("mousedown",e=>{
  if(!GZ.drag||GZ.active) return;
  if(e.target&&e.target.closest&&e.target.closest("#gzHud")) return;
  gizmoHudHide(); GZ.drag=null;
},true);

/* ─── HOTKEYS (capture-phase so they win over toolbar letter shortcuts) ─── */
window.addEventListener("keydown",e=>{
  if(e.target&&(e.target.tagName==="INPUT"||e.target.tagName==="SELECT"||e.target.tagName==="TEXTAREA")) return;
  if(e.ctrlKey||e.metaKey||e.altKey) return;
  const active=GZ.visible||SKETCH.drawing;
  // Esc / Enter are handled even mid-draw
  if(e.key==="Escape"){
    if(GZ.active||GZ.drag){ // cancel drag / dismiss the value HUD: restore snapshot
      const d=GZ.drag, tr=GZ.target&&GZ.target.transform; if(d&&tr){ tr.t=d.t0.slice(); tr.r=d.r0.slice(); tr.s=d.s0.slice(); }
      GZ.active=null; GZ.drag=null; gizmoHudHide(); vp3Draw(); e.stopImmediatePropagation(); return;
    }
    if(SKETCH.drawing){ sketchExitDrawMode(); e.stopImmediatePropagation(); return; }
    return;
  }
  if(e.key==="Enter"){
    if(SKETCH.stage==="dragging"||SKETCH.stage==="placing"){ sketchOnUp(); e.stopImmediatePropagation(); return; }
    return;
  }
  if(!active) return;                     // let tool letters work normally
  const key=e.key.toLowerCase();
  if(key==="g"){ gizmoSetMode("translate"); toast("Move","move"); e.stopImmediatePropagation(); return; }
  if(key==="r"){ gizmoSetMode("rotate");    toast("Rotate","rewind"); e.stopImmediatePropagation(); return; }
  if(key==="s"){ gizmoSetMode("scale");     toast("Scale","move"); e.stopImmediatePropagation(); return; }
  if(key==="x"||key==="y"||key==="z"){
    GZ.axisLock=GZ.axisLock===key?null:key; vp3Draw();
    toast(GZ.axisLock?("Lock "+key.toUpperCase()):"Unlock axis","move"); e.stopImmediatePropagation(); return;
  }
},true);
