"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   joinmenu.js — the endpoint-JOIN confirm menu for editing a curve after it has
                 been drawn. When you drag one open endpoint of a curve onto the
                 other endpoint (within snap distance) and release, a small floating
                 panel asks whether to join the two points — and, if so, whether to
                 close the loop (which fills it with the translucent face).

                   Join & close   → merge the two endpoints + close the loop (fill)
                   Join (keep open)→ merge the two endpoints, leave it open
                   Cancel          → leave the dragged point where it was dropped

                 It reuses the .ctx-menu / .ctx-item / .ctx-sec styling of the
                 draw-tool context menu but lives in its OWN #joinMenu element so
                 the two never collide.

                 // The same "drag one end onto another" idea recurs when MODELLING
                 // solids, but that path needs a different (solids) menu — build it
                 // separately; joinApply here stays free of modelling assumptions.
   ════════════════════════════════════════════════════════════════════════════ */

/* the last known cursor position — ptDragUp fires without a MouseEvent, so we track
   the pointer here to place the menu where the user released. */
const _lastMouse={x:0,y:0};
window.addEventListener("mousemove",e=>{ _lastMouse.x=e.clientX; _lastMouse.y=e.clientY; });

/* called from points.js ptDragUp when an OPEN curve's first/last anchor was dragged.
   If it landed within snap distance of the other endpoint, pop the join menu. */
function joinMaybe(obj,draggedIndex){
  if(!obj||!obj.cpts||obj.closed) return;
  const n=obj.cpts.length; if(n<3) return;
  if(draggedIndex!==0&&draggedIndex!==n-1) return;
  const otherIndex=(draggedIndex===0)?n-1:0;
  const ds=joinScreenOf(obj,draggedIndex), os=joinScreenOf(obj,otherIndex);
  if(!ds||!os) return;
  const px=(typeof SK_SNAP_PX!=="undefined")?SK_SNAP_PX:10;
  if((ds[0]-os[0])*(ds[0]-os[0])+(ds[1]-os[1])*(ds[1]-os[1])>px*px) return;
  curveJoinMenu(_lastMouse.x,_lastMouse.y,obj,draggedIndex,otherIndex);
}
function joinScreenOf(obj,i){
  const w=sketchPlanePoint(obj.plane,obj.cpts[i][0],obj.cpts[i][1]);
  return vp3World2Screen(VP3.mvp,w);
}

let _joinEl=null;
function joinMenuEl(){
  if(_joinEl) return _joinEl;
  const el=document.createElement("div"); el.className="ctx-menu"; el.id="joinMenu";
  document.body.appendChild(el); _joinEl=el; return el;
}
/* build + open the 3-choice join menu at (clientX,clientY). Opened from a mouseup,
   so the outside-click dismiss handler won't instantly close it. */
function curveJoinMenu(clientX,clientY,obj,draggedIndex,otherIndex){
  const el=joinMenuEl();
  el.innerHTML="";
  const sec=document.createElement("div"); sec.className="ctx-sec"; sec.textContent="Join endpoints";
  el.appendChild(sec);
  const rows=[
    {label:"Join & close",     act:()=>joinApply(obj,draggedIndex,otherIndex,true)},
    {label:"Join (keep open)", act:()=>joinApply(obj,draggedIndex,otherIndex,false)},
    {label:"Cancel",           act:()=>{ if(typeof vp3Draw==="function") vp3Draw(); }},
  ];
  rows.forEach(r=>{
    const b=document.createElement("button"); b.className="ctx-item";
    b.innerHTML=`<span class="ctx-lbl">${r.label}</span>`;
    b.onclick=()=>{ r.act(); joinMenuClose(); };
    el.appendChild(b);
  });
  el.style.display="block";
  const vw=window.innerWidth, vh=window.innerHeight, rect=el.getBoundingClientRect();
  el.style.left=Math.min(clientX,vw-rect.width-8)+"px";
  el.style.top=Math.min(clientY,vh-rect.height-8)+"px";
  el.classList.add("open");
}
function joinMenuClose(){ if(_joinEl){ _joinEl.classList.remove("open"); _joinEl.style.display="none"; } }

/* collapse the two endpoints into a single shared control point. The dragged point
   was released on top of the other, so dropping it leaves the surviving endpoint in
   place — no coordinate averaging needed. Optionally close the loop (fills it). */
function joinApply(obj,draggedIndex,otherIndex,close){
  obj.cpts.splice(draggedIndex,1);
  // bezier: re-derive default handles for the new point ring (indices shifted).
  if(obj.category==="bezier"&&typeof curveDefaultHandles==="function")
    obj.handles=curveDefaultHandles(obj.cpts);
  // bspline / nurbs: drop the matching weight + clamp the degree to the new count.
  if(obj.weights) obj.weights.splice(draggedIndex,1);
  if(typeof obj.degree==="number") obj.degree=Math.min(obj.degree,Math.max(1,obj.cpts.length-1));
  obj.closed=!!close;
  if(typeof sketchRebuildCurve==="function") sketchRebuildCurve(obj);
  if(typeof pushHistory==="function") pushHistory("sketch",obj.name,close?"joined & closed":"joined endpoints");
  if(typeof renderProps==="function") renderProps();
  if(typeof vp3Draw==="function") vp3Draw();
}

/* dismiss on an outside click or Esc (scoped to #joinMenu so it never touches the
   draw-tool #ctxMenu). */
document.addEventListener("mousedown",e=>{
  if(_joinEl&&_joinEl.classList.contains("open")&&!e.target.closest("#joinMenu")) joinMenuClose();
});
window.addEventListener("keydown",e=>{ if(e.key==="Escape") joinMenuClose(); });
