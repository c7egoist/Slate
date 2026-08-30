"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   viewport.js — the centre stage chrome that wraps the real 3D viewport (VP3):
                 the display-mode bar, the viewcube orientation buttons wired to
                 the camera, the context command bar (OK / Cancel), the Sketch ⇄
                 Model captions + status bar, the snap toggle, the Measure tool,
                 the header document buttons, and the top-header ground-grid
                 dropdown. The 3D canvas itself lives in viewport3d.js.
   ════════════════════════════════════════════════════════════════════════════ */

/* ─── DISPLAY-MODE BAR (top-right) — drives the real viewport ─── */
const DISP_MODES=[
  {id:"shaded", icon:"box",                 tip:"Shaded",   on:true},
  {id:"wire",   icon:"git-commit-vertical", tip:"Wireframe",on:true},
  {id:"xray",   icon:"layers",              tip:"X-Ray"},
];
function buildDispBar(){
  const bar=document.getElementById("dispBar"); if(!bar) return;
  bar.innerHTML="";
  DISP_MODES.forEach(m=>{
    const b=document.createElement("button");
    b.dataset.tip=m.tip; b.dataset.mode=m.id;
    b.className=m.on?"active":"";
    b.innerHTML=`<span class="lico" data-ic="${m.icon}"></span>`;
    b.onclick=()=>{
      const nowOn=!b.classList.contains("active");
      b.classList.toggle("active",nowOn);
      if(m.id==="shaded") vp3SetShaded(nowOn);
      else if(m.id==="wire") vp3SetWire(nowOn);
      else if(m.id==="xray"){ VP3.shaded=nowOn?false:true; bar.querySelector('[data-mode="shaded"]').classList.toggle("active",!nowOn); vp3Draw(); }
      toast(m.tip+(nowOn?" on":" off"),m.icon);
    };
    bar.appendChild(b);
  });
  refreshIcons(bar);
}

/* ─── VIEWPORT SETTINGS POPUP (top-right gear) — ground-grid style ─── */
let gridMode="dots";
function bindViewportSettings(){
  const box=document.getElementById("vpSettings"), trig=document.getElementById("vpSetTrig"), seg=document.getElementById("gridSeg");
  if(!box||!trig||!seg) return;
  function paint(){
    seg.querySelectorAll(".vp-seg-btn").forEach(o=>o.classList.toggle("active",o.dataset.grid===gridMode));
    trig.classList.toggle("active",box.classList.contains("open"));
  }
  trig.onclick=e=>{ e.stopPropagation(); box.classList.toggle("open"); paint(); };
  seg.querySelectorAll(".vp-seg-btn").forEach(o=>{
    o.onclick=e=>{ e.stopPropagation(); gridMode=o.dataset.grid; vp3SetGrid(gridMode); paint();
      toast("Grid: "+({dots:"Dots",lines:"Grid",off:"None"}[gridMode]),"grid-3x3"); };
  });
  document.addEventListener("click",e=>{ if(!e.target.closest("#vpSettings")){ box.classList.remove("open"); paint(); } });
  paint();
}

/* ─── CONTEXT COMMAND BAR ─── */
function showCmdBar(name,hint,icon,chips){
  const bar=document.getElementById("cmdBar"); if(!bar) return;
  document.getElementById("cmdName").textContent=name;
  document.getElementById("cmdHint").textContent=hint||"";
  document.getElementById("cmdIco").innerHTML=CAD_ICONS[icon]||CAD_ICONS.extrude;
  const opts=document.getElementById("cmdOpts"); opts.innerHTML="";
  (chips||[]).forEach(c=>{
    const chip=document.createElement("button"); chip.className="vp-cmd-chip"+(c.on?" on":"");
    chip.textContent=c.label; chip.onclick=()=>chip.classList.toggle("on");
    opts.appendChild(chip);
  });
  bar.classList.add("show");
}
function hideCmdBar(){ const bar=document.getElementById("cmdBar"); if(bar) bar.classList.remove("show"); }
function bindCmdBar(){
  const ok=document.getElementById("cmdOk"), cancel=document.getElementById("cmdCancel");
  if(ok) ok.onclick=()=>{ hideCmdBar(); toast("Applied","check"); };
  if(cancel) cancel.onclick=()=>{ hideCmdBar(); toast("Cancelled","x"); };
}

/* ─── ENV → VIEWPORT / STATUS SYNC ─── */
function syncViewportEnv(){
  const sketch=CAD.env==="sketch";
  const env=document.getElementById("vpEnv"); if(env) env.textContent=sketch?"SKETCH":"MODEL";
  const st=document.getElementById("stEnv"); if(st) st.textContent=sketch?"Sketch":"Model";
}

/* ─── STATUS-BAR SNAP TOGGLE ─── */
function bindSnap(){
  const chip=document.getElementById("stSnap"); if(!chip) return;
  const host=chip.closest(".stat"); if(host) host.style.cursor="pointer";
  (host||chip).onclick=()=>{ cadSnap=!cadSnap; chip.textContent=cadSnap?"On":"Off"; toast("Snap "+(cadSnap?"on":"off"),"grid-3x3"); };
}

/* ─── HEADER document buttons (data-toast) ─── */
function bindHeaderButtons(){
  document.querySelectorAll('[data-toast]').forEach(b=>{
    b.onclick=()=>toast(b.dataset.toast,b.dataset.ic||"info");
  });
}

/* one-time wiring of every viewport-chrome control + the 3D canvas */
function bindViewport(){
  const canvas=document.getElementById("vpCanvas");
  if(canvas&&typeof vp3Init==="function") vp3Init(canvas);
  buildDispBar();
  bindViewportSettings();
  bindCmdBar();
  bindSnap();
  bindHeaderButtons();
  bindModeToggle();
  syncViewportEnv();
}

/* the Object ⇄ Sketch pill in the viewport top-left */
function bindModeToggle(){
  const pill=document.getElementById("vpMode");
  if(pill&&typeof toggleCadInteractionMode==="function") pill.onclick=()=>toggleCadInteractionMode();
  if(typeof syncInteractionMode==="function") syncInteractionMode();
}
