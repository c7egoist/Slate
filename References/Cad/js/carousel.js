"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   carousel.js — the two Dynamic-Island dock switchers.
     • LEFT dock  : Browser ⇄ Tools
     • RIGHT dock : Properties ⇄ Features ⇄ History
   Each renders a morphing pill and swaps its active pane. Generic builders live
   here; the CAD-specific segment lists + handlers live in toolbar.js / features.js.
   ════════════════════════════════════════════════════════════════════════════ */

const leftPill=document.getElementById("leftPill");
const rightPill=document.getElementById("rightPill");

/* generic pill renderer: `segs` = [{key,label,icon,pane}], `active` = key */
function renderPill(pill,segs,active,onPick){
  if(!pill) return;
  pill.innerHTML="";
  segs.forEach(s=>{
    const b=document.createElement("button");
    b.className="di-seg"+(s.key===active?" active":"");
    b.dataset.pane=s.key;
    b.innerHTML=`<span class="lico" data-ic="${s.icon}"></span><span class="di-label">${s.label}</span>`;
    b.onclick=()=>onPick(s.key);
    pill.appendChild(b);
  });
  refreshIcons(pill);
}
/* show exactly the pane whose key matches, hide the others; replay the enter anim */
function showPane(segs,key){
  segs.forEach(s=>{const el=document.getElementById(s.pane);if(el)el.style.display=(s.key===key)?"flex":"none";});
  const shown=document.getElementById(segs.find(s=>s.key===key).pane);
  if(shown){shown.style.animation="none";void shown.offsetWidth;shown.style.animation="";}
}
