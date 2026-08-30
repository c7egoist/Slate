"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   tabs.js — the Chrome-style trapezoid tab strip above the header. This is the
             standalone CAD app, so there is one open workspace tab (Part01);
             double-click renames it, the + is a placeholder (single-workspace).
             The trapezoid chrome + active styling mirror Studio's tab bar.
   ════════════════════════════════════════════════════════════════════════════ */

const CAD_TAB={ key:"cad", name:"CAD · Part01" };

function buildCadTab(){
  const el=document.createElement("button");
  el.className="tab active";
  el.dataset.key=CAD_TAB.key;
  el.style.setProperty("--tab-color",CAD_WS.color);

  const fill=document.createElement("span"); fill.className="tab-fill"; el.appendChild(fill);
  const dot=document.createElement("span"); dot.className="tab-dot"; el.appendChild(dot);

  const name=document.createElement("span");
  name.className="tab-name"; name.textContent=CAD_TAB.name; name.title="Double-click to rename";
  name.addEventListener("dblclick",e=>{ e.stopPropagation(); beginTabRename(name); });
  el.appendChild(name);

  const close=document.createElement("span");
  close.className="tab-close";
  close.innerHTML='<span class="lico" data-ic="x"></span>';
  close.addEventListener("click",e=>{ e.stopPropagation(); toast("The CAD workspace stays open","info"); });
  el.appendChild(close);
  return el;
}

function beginTabRename(nameEl){
  const input=document.createElement("input");
  input.className="tab-rename"; input.value=CAD_TAB.name;
  nameEl.replaceWith(input); input.focus(); input.select();
  let done=false;
  const commit=()=>{ if(done)return; done=true; CAD_TAB.name=input.value.trim()||CAD_TAB.name; renderTabs(); };
  input.addEventListener("keydown",ev=>{ ev.stopPropagation();
    if(ev.key==="Enter"){ev.preventDefault();commit();}
    else if(ev.key==="Escape"){ev.preventDefault();done=true;renderTabs();} });
  input.addEventListener("blur",commit);
}

function renderTabs(){
  const strip=document.getElementById("tabStrip"); if(!strip) return;
  strip.innerHTML="";
  strip.appendChild(buildCadTab());
  const add=document.createElement("button");
  add.className="tab-add"; add.title="Open workspace";
  add.innerHTML='<span class="lico" data-ic="plus"></span>';
  add.addEventListener("click",()=>toast("Standalone CAD workspace","layout-grid"));
  strip.appendChild(add);
  refreshIcons(strip);
}
