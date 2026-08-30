"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   dock.js — left/right dock collapse toggles + edge-drag resizers. Both docks
             carry a drag handle on their inner edge; dragging it widens/narrows
             the dock (the viewport takes up the slack).
   ════════════════════════════════════════════════════════════════════════════ */

const leftDock=document.getElementById("leftDock"),rightDock=document.getElementById("rightDock");
const dockLeftBtn=document.getElementById("dockLeftBtn"),dockRightBtn=document.getElementById("dockRightBtn");
const resizerRight=document.getElementById("resizerRight"),resizerLeft=document.getElementById("resizerLeft");
let leftOpen=true,rightOpen=true,leftW=322,rightW=326;

/* the collapse buttons have no dedicated open/close icon set in UI_ICONS, so we
   reuse the panel arrows via inline SVG; state is reflected by rotating them. */
function panelIcon(open,side){
  const dir=side==="left"?(open?"◀":"▶"):(open?"▶":"◀");
  return `<span class="lico" style="font-size:12px;line-height:1">${dir}</span>`;
}
function paintDockButtons(){
  if(dockLeftBtn) dockLeftBtn.innerHTML=panelIcon(leftOpen,"left");
  if(dockRightBtn) dockRightBtn.innerHTML=panelIcon(rightOpen,"right");
}

if(dockLeftBtn) dockLeftBtn.onclick=()=>{
  leftOpen=!leftOpen;
  if(leftOpen){leftDock.style.width=leftW+"px";leftDock.classList.remove("collapsed");resizerLeft.style.display="block";}
  else{leftDock.style.width="";leftDock.classList.add("collapsed");resizerLeft.style.display="none";}
  paintDockButtons();
};
if(dockRightBtn) dockRightBtn.onclick=()=>{
  rightOpen=!rightOpen;
  if(rightOpen){rightDock.style.width=rightW+"px";rightDock.classList.remove("collapsed");resizerRight.style.display="block";}
  else{rightDock.style.width="";rightDock.classList.add("collapsed");resizerRight.style.display="none";}
  paintDockButtons();
};

/* shared edge-drag: clamps width to [240, 520] */
function bindResizer(handle,dock,side){
  if(!handle||!dock) return;
  let rz=false;
  handle.addEventListener("mousedown",()=>{
    if(side==="left"?!leftOpen:!rightOpen)return;
    rz=true;document.body.style.cursor="col-resize";dock.classList.add("resizing");
  });
  window.addEventListener("mousemove",e=>{
    if(!rz)return;
    const w=Math.max(240,Math.min(520, side==="left"?e.clientX-dock.getBoundingClientRect().left:innerWidth-e.clientX));
    dock.style.width=w+"px";
    if(side==="left")leftW=w; else rightW=w;
  });
  window.addEventListener("mouseup",()=>{if(!rz)return;rz=false;document.body.style.cursor="";dock.classList.remove("resizing");});
}
bindResizer(resizerLeft,leftDock,"left");
bindResizer(resizerRight,rightDock,"right");
