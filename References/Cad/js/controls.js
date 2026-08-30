"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   controls.js — reusable UI builders (sections, switches, sliders, segmented
                 buttons, colour strips), the themed dropdown, the toast, and
                 refreshIcons(). No workspace logic lives here — pure widgets.

                 refreshIcons() delegates to icons.js's hydrateIcons() so every
                 render path that ends with refreshIcons() keeps its icons live.
   ════════════════════════════════════════════════════════════════════════════ */

function refreshIcons(root){ hydrateIcons(root||document); }

/* a data-ic span (hydrated by refreshIcons) */
function icoSpan(name,cls){ return `<span class="lico${cls?" "+cls:""}" data-ic="${name}"></span>`; }

/* control builders */
function section(title){
  const el=document.createElement("div");el.className="prop-section";
  const t=document.createElement("div");t.className="prop-title";t.innerHTML=`${title} ${icoSpan("chevron-down","chev")}`;
  const c=document.createElement("div");c.className="prop-content";
  t.onclick=()=>t.classList.toggle("collapsed");
  el.appendChild(t);el.appendChild(c);return {el,content:c};
}
function switchRow(label,val,onChange){
  const r=document.createElement("div");r.className="switch-row";
  r.innerHTML=`<span class="sr-label">${label}</span><div class="switch ${val?'on':''}"><div class="knob"></div></div>`;
  r.querySelector(".switch").onclick=function(){this.classList.toggle("on");onChange(this.classList.contains("on"));};
  return r;
}

/* ─── THEMED DROPDOWN ───
   A custom select matching the dark pill theme, with a header + footer. `opts` is
   an array of {value,label,dot?} (or plain strings). Closes on outside click. */
let _ddSeq=0;
function themedDropdown({value,opts,onChange,head,headIcon,foot,footIcon,dotFor}){
  const norm=opts.map(o=>typeof o==="string"?{value:o,label:o}:o);
  const cur=()=>norm.find(o=>o.value===value)||norm[0];
  const dd=document.createElement("div");dd.className="dd";dd.dataset.dd=++_ddSeq;
  const dotHtml=(o)=>{const col=o&&o.dot!==undefined?o.dot:(dotFor?dotFor(o):null);return col?`<span class="dd-opt-dot" style="background:${col}"></span>`:"";};
  function paintTrigger(){
    const o=cur();const col=o&&o.dot!==undefined?o.dot:(dotFor?dotFor(o):null);
    trig.innerHTML=`${col?`<span class="dd-dot" style="background:${col}"></span>`:""}<span class="dd-val">${o?o.label:"—"}</span>${icoSpan("chevron-down","dd-chev")}`;
    refreshIcons(trig);
  }
  const trig=document.createElement("button");trig.className="dd-trigger";
  const menu=document.createElement("div");menu.className="dd-menu";
  menu.innerHTML=`<div class="dd-menu-head">${headIcon?icoSpan(headIcon):""}${head||"Options"}</div>
    <div class="dd-menu-list"></div>
    <div class="dd-menu-foot">${footIcon?icoSpan(footIcon):""}<span>${foot||(norm.length+" options")}</span></div>`;
  const list=menu.querySelector(".dd-menu-list");
  norm.forEach(o=>{
    const b=document.createElement("button");b.className="dd-opt"+(o.value===value?" sel":"");
    b.innerHTML=`${dotHtml(o)}<span class="dd-opt-lbl">${o.label}</span>${icoSpan("check","dd-check")}`;
    b.onclick=(e)=>{e.stopPropagation();value=o.value;list.querySelectorAll(".dd-opt").forEach(x=>x.classList.remove("sel"));b.classList.add("sel");paintTrigger();dd.classList.remove("open");onChange(o.value);};
    list.appendChild(b);
  });
  trig.onclick=(e)=>{e.stopPropagation();const open=dd.classList.contains("open");closeDropdowns();if(!open)dd.classList.add("open");};
  dd.appendChild(trig);dd.appendChild(menu);paintTrigger();refreshIcons(menu);
  return dd;
}
function closeDropdowns(){document.querySelectorAll(".dd.open").forEach(d=>d.classList.remove("open"));}
document.addEventListener("click",e=>{if(!e.target.closest(".dd"))closeDropdowns();});
/* a labelled themed-dropdown control */
function ddCtl(label,value,opts,onChange,meta={}){
  const c=document.createElement("div");c.className="ctl";
  const lbl=document.createElement("div");lbl.className="ctl-label";lbl.textContent=label;c.appendChild(lbl);
  c.appendChild(themedDropdown(Object.assign({value,opts,onChange},meta)));
  return c;
}
function segCtl(label,val,opts,onChange){
  const c=document.createElement("div");c.className="ctl";c.innerHTML=`<div class="ctl-label">${label}</div>`;
  const seg=document.createElement("div");seg.className="seg";
  opts.forEach(([v,l])=>{const b=document.createElement("button");b.textContent=l;if(v===val)b.classList.add("active");
    b.onclick=()=>{seg.querySelectorAll("button").forEach(x=>x.classList.remove("active"));b.classList.add("active");onChange(v);};seg.appendChild(b);});
  c.appendChild(seg);return c;
}
function sliderCtl(label,val,min,max,step,unit,onChange){
  const c=document.createElement("div");c.className="ctl";c.innerHTML=`<div class="ctl-label">${label}</div>`;
  const host=document.createElement("div");c.appendChild(host);
  makeSlider(host,{min,max,step,value:val,unit,onInput:onChange});return c;
}
function infoRow(label,val){const c=document.createElement("div");c.className="ctl-label";c.style.margin="11px 0";c.innerHTML=`<span>${label}</span><span class="hint">${val}</span>`;return c;}
/* an editable numeric-vector row: label + one number field per component. `vals`
   is a number array; onChange(index,newValue) fires on Enter/blur. `hot` marks the
   row (e.g. the selected control point) amber. */
function coordCtl(label,vals,onChange,hot){
  const c=document.createElement("div");c.className="coord-ctl"+(hot?" hot":"");
  const lbl=document.createElement("span");lbl.className="coord-lbl";lbl.textContent=label;c.appendChild(lbl);
  const box=document.createElement("div");box.className="coord-fields";
  vals.forEach((v,i)=>{
    const inp=document.createElement("input");inp.className="coord-inp";inp.type="text";
    inp.value=(Math.round(v*100)/100).toString();
    const push=()=>{const x=parseFloat(inp.value);if(!isNaN(x))onChange(i,x);};
    inp.onkeydown=(e)=>{e.stopPropagation();if(e.key==="Enter"){push();inp.blur();}};
    inp.onchange=push;
    box.appendChild(inp);
  });
  c.appendChild(box);return c;
}
function btn(label,icon,onClick){const b=document.createElement("button");b.className="act-btn";b.innerHTML=`${icoSpan(icon)} ${label}`;b.onclick=onClick;refreshIcons(b);return b;}
function colorStrip(n){
  const wrap=document.createElement("div");
  const lbl=document.createElement("div");lbl.className="ctl-label";lbl.style.marginBottom="2px";lbl.textContent="Colour label";wrap.appendChild(lbl);
  const strip=document.createElement("div");strip.className="swatch-strip";
  PALETTE.forEach(c=>{const s=document.createElement("div");s.className="sw";s.style.background=c;
    s.onclick=()=>{n.swatch=c;renderTree();renderProps();};strip.appendChild(s);});
  wrap.appendChild(strip);return wrap;
}

/* CUSTOM SLIDER — draggable fill/knob + editable numbox */
function makeSlider(host,{min,max,step=1,value,unit="",onInput}){
  host.innerHTML="";
  const row=document.createElement("div");row.className="vw-row";
  const numbox=document.createElement("div");numbox.className="vw-numbox";
  const inp=document.createElement("input");const u=document.createElement("span");u.className="vw-unit";u.textContent=unit;
  numbox.appendChild(inp);numbox.appendChild(u);
  const sl=document.createElement("div");sl.className="vw-slider";
  const fill=document.createElement("div");fill.className="vw-fill";const knob=document.createElement("div");knob.className="vw-knob";
  sl.appendChild(fill);sl.appendChild(knob);row.appendChild(numbox);row.appendChild(sl);host.appendChild(row);
  let val=value;
  function fmt(v){return step<1?(+v).toFixed(1):Math.round(v);}
  function setVisual(){const pct=(val-min)/(max-min);fill.style.width=(pct*100)+"%";knob.style.left=(pct*100)+"%";inp.value=fmt(val);}
  function setVal(v,fire=true){v=Math.max(min,Math.min(max,v));if(step)v=Math.round(v/step)*step;val=v;setVisual();if(fire&&onInput)onInput(val);}
  setVisual();
  function fromX(cx){const r=sl.getBoundingClientRect();setVal(min+Math.max(0,Math.min(1,(cx-r.left)/r.width))*(max-min));}
  let drag=false;
  sl.addEventListener("pointerdown",e=>{drag=true;sl.setPointerCapture(e.pointerId);fromX(e.clientX);});
  sl.addEventListener("pointermove",e=>{if(drag)fromX(e.clientX);});
  sl.addEventListener("pointerup",()=>drag=false);
  inp.addEventListener("change",()=>{const x=parseFloat(inp.value);if(!isNaN(x))setVal(x);else setVisual();});
  return {set:v=>setVal(v,false),get:()=>val};
}

/* ═════════ TOAST ═════════ */
const toastEl=document.getElementById("toast");
function toast(msg,icon){toastEl.innerHTML=`${icoSpan(icon||'info')} ${msg}`;refreshIcons(toastEl);toastEl.classList.add("show");clearTimeout(toastEl._t);toastEl._t=setTimeout(()=>toastEl.classList.remove("show"),1500);}
