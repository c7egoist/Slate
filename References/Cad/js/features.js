"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   features.js — RIGHT-dock Features pane: a parametric feature timeline in the
                 SolidWorks / Fusion style. An ordered list of feature cards,
                 each with a suppress toggle, move up/down, delete, roll-back, and
                 its own parameters; a rollback bar marks the rebuild point;
                 features below it are rolled back (greyed). The "Add feature"
                 button opens a categorised picker.

                 Renders into the pre-existing #featList; drives the header
                 (#featHeadSub) + footer buttons (#featCollapseAll / #featRollEnd).
   ════════════════════════════════════════════════════════════════════════════ */

/* ─── FEATURE TYPE REGISTRY ───
   cat: create | modify | boolean. params is the ordered list of editable fields.
   icon keys reference the shared CAD_ICONS set from icons.js. */
const CAD_FEAT_TYPES=[
  /* a drawn sketch (cat:"sketch" is intentionally NOT in CAD_FEAT_CATS, so it
     shows in the timeline but stays out of the Add-Feature picker) */
  {type:"sketch", name:"Sketch", icon:"spline", cat:"sketch",
    params:[{k:"plane",t:"dd",label:"Plane",opts:["Top Plane","Front Plane","Right Plane"],def:"Top Plane"}]},
  {type:"extrude", name:"Extrude", icon:"extrude", cat:"create",
    params:[{k:"profile",t:"dd",label:"Profile",opts:["Sketch_Base","Sketch_Boss","— pick —"],def:"Sketch_Base"},
            {k:"end",t:"dd",label:"End condition",opts:["Blind","Through All","To Next","Symmetric"],def:"Blind"},
            {k:"distance",t:"slider",label:"Distance",min:0,max:200,step:0.5,unit:"mm",def:25},
            {k:"taper",t:"slider",label:"Draft angle",min:-30,max:30,step:0.5,unit:"°",def:0},
            {k:"merge",t:"switch",label:"Merge result",def:true}]},
  {type:"revolve", name:"Revolve", icon:"revolve", cat:"create",
    params:[{k:"profile",t:"dd",label:"Profile",opts:["Sketch_Base","Sketch_Boss","— pick —"],def:"Sketch_Base"},
            {k:"axis",t:"dd",label:"Axis",opts:["X Axis","Y Axis","Z Axis","Edge"],def:"Y Axis"},
            {k:"angle",t:"slider",label:"Angle",min:0,max:360,step:1,unit:"°",def:360}]},
  {type:"sweep", name:"Sweep", icon:"sweep", cat:"create",
    params:[{k:"profile",t:"dd",label:"Profile",opts:["Sketch_Boss","Sketch_Base"],def:"Sketch_Boss"},
            {k:"path",t:"dd",label:"Path",opts:["Sketch_Base","Edge Chain"],def:"Sketch_Base"},
            {k:"twist",t:"slider",label:"Twist",min:0,max:360,step:1,unit:"°",def:0},
            {k:"orient",t:"seg",label:"Orientation",opts:[["follow","Follow"],["keep","Keep"]],def:"follow"}]},
  {type:"loft", name:"Loft", icon:"loft", cat:"create",
    params:[{k:"sections",t:"slider",label:"Sections",min:2,max:8,step:1,def:2},
            {k:"start",t:"dd",label:"Start tangency",opts:["None","Normal","Direction"],def:"None"},
            {k:"end",t:"dd",label:"End tangency",opts:["None","Normal","Direction"],def:"None"},
            {k:"closed",t:"switch",label:"Closed loft",def:false}]},
  {type:"fillet", name:"Fillet", icon:"fillet", cat:"modify",
    params:[{k:"radius",t:"slider",label:"Radius",min:0,max:50,step:0.1,unit:"mm",def:2},
            {k:"edges",t:"dd",label:"Edges",opts:["Selected (4)","Tangent chain","All convex","— pick —"],def:"Selected (4)"},
            {k:"profile",t:"seg",label:"Profile",opts:[["circular","Circular"],["conic","Conic"]],def:"circular"}]},
  {type:"chamfer", name:"Chamfer", icon:"chamfer", cat:"modify",
    params:[{k:"distance",t:"slider",label:"Distance",min:0,max:30,step:0.1,unit:"mm",def:1},
            {k:"angle",t:"slider",label:"Angle",min:5,max:85,step:1,unit:"°",def:45},
            {k:"mode",t:"seg",label:"Mode",opts:[["da","Dist·Angle"],["dd","Dist·Dist"]],def:"da"}]},
  {type:"shell", name:"Shell", icon:"shell", cat:"modify",
    params:[{k:"thickness",t:"slider",label:"Thickness",min:0.1,max:20,step:0.1,unit:"mm",def:2},
            {k:"direction",t:"seg",label:"Direction",opts:[["inward","Inward"],["outward","Outward"]],def:"inward"},
            {k:"faces",t:"dd",label:"Faces to remove",opts:["Top (1)","Selected (2)","— pick —"],def:"Top (1)"}]},
  {type:"draft", name:"Draft", icon:"draft", cat:"modify",
    params:[{k:"angle",t:"slider",label:"Angle",min:0,max:30,step:0.5,unit:"°",def:3},
            {k:"neutral",t:"dd",label:"Neutral plane",opts:["Top Plane","Bottom Face","— pick —"],def:"Top Plane"}]},
  {type:"hole", name:"Hole", icon:"hole", cat:"modify",
    params:[{k:"htype",t:"dd",label:"Type",opts:["Simple","Counterbore","Countersink","Tapped"],def:"Simple"},
            {k:"diameter",t:"slider",label:"Diameter",min:0.5,max:50,step:0.1,unit:"mm",def:6},
            {k:"depth",t:"dd",label:"End condition",opts:["Through All","Blind","To Next"],def:"Through All"},
            {k:"count",t:"slider",label:"Instances",min:1,max:24,step:1,def:1}]},
  {type:"pattern", name:"Pattern", icon:"pattern", cat:"modify",
    params:[{k:"ptype",t:"seg",label:"Type",opts:[["linear","Linear"],["circular","Circular"]],def:"linear"},
            {k:"count",t:"slider",label:"Count",min:1,max:48,step:1,def:4},
            {k:"spacing",t:"slider",label:"Spacing",min:0,max:200,step:0.5,unit:"mm",def:20},
            {k:"symmetric",t:"switch",label:"Symmetric",def:false}]},
  {type:"bunion", name:"Combine", icon:"bunion", cat:"boolean",
    params:[{k:"tool",t:"dd",label:"Tool body",opts:["Lid","Sweep_Surface","— pick —"],def:"Lid"},
            {k:"keep",t:"switch",label:"Keep tools",def:false}]},
  {type:"bsubtract", name:"Cut", icon:"bsubtract", cat:"boolean",
    params:[{k:"tool",t:"dd",label:"Tool body",opts:["Lid","Sweep_Surface","— pick —"],def:"Lid"},
            {k:"keep",t:"switch",label:"Keep tools",def:false}]},
  {type:"bintersect", name:"Intersect", icon:"bintersect", cat:"boolean",
    params:[{k:"tool",t:"dd",label:"Tool body",opts:["Lid","Housing","— pick —"],def:"Lid"}]},
];
const CAD_FEAT_BY_TYPE=Object.fromEntries(CAD_FEAT_TYPES.map(f=>[f.type,f]));
const CAD_FEAT_CATS=[
  {key:"create",  label:"Create"},
  {key:"modify",  label:"Modify"},
  {key:"boolean", label:"Boolean"},
];
function cadEsc(s){ return (s+"").replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c])); }

/* the single parametric timeline. Each entry is
   {uid,type,name,suppressed,collapsed,values{...}}. cadRollback is the index the
   model is rebuilt up to (features at index >= it are rolled back). */
let cadFeatUid=0;
let cadRollback=0;
const CAD_TIMELINE=[];
function cadFeatDefaults(type){ const def={}; CAD_FEAT_BY_TYPE[type].params.forEach(p=>def[p.k]=p.def); return def; }
function cadFeatInstance(type,name){
  return {uid:"cf"+(++cadFeatUid),type,name:name||CAD_FEAT_BY_TYPE[type].name,suppressed:false,collapsed:true,values:cadFeatDefaults(type)};
}
/* empty scene — the timeline fills as the draw/feature tools are used */
function cadSeedTimeline(){
  cadRollback=CAD_TIMELINE.length;
}

/* ════════════════════════ RENDER ════════════════════════ */
function renderCadFeatures(){
  cadSeedTimeline();
  const list=document.getElementById("featList"); if(!list) return;

  // header sub-count
  const active=CAD_TIMELINE.filter(f=>!f.suppressed).length;
  const sub=document.getElementById("featHeadSub");
  if(sub) sub.textContent=`${CAD_TIMELINE.length} feature${CAD_TIMELINE.length!==1?"s":""} · ${active} active`;
  const hIco=document.getElementById("featHeadIco");
  if(hIco && !hIco.dataset.done){ hIco.innerHTML=CAD_ENV_ICONS.model; hIco.dataset.done="1"; }

  // the timeline body
  list.innerHTML="";
  if(!CAD_TIMELINE.length){
    list.innerHTML=`<div class="cdf-empty">
      <span class="cdf-empty-ico">${CAD_ICONS.extrude}</span>
      <div class="cdf-empty-t">Empty timeline</div>
      <div class="cdf-empty-s">Add a feature to build the part parametrically. Order is top-to-bottom rebuild order — drag the rollback bar to step back in history.</div>
    </div>`;
  } else {
    CAD_TIMELINE.forEach((inst,idx)=>{
      list.appendChild(cadFeatCard(inst,idx));
      if(idx+1===cadRollback) list.appendChild(cadRollbackBar());
    });
    if(cadRollback>=CAD_TIMELINE.length) list.appendChild(cadRollbackBar());
  }
  syncBrowserFooter();
  refreshIcons(list);
}

/* the rollback bar — a divider marking the rebuild point */
function cadRollbackBar(){
  const bar=document.createElement("div"); bar.className="cdf-rollbar";
  bar.innerHTML=`<span class="cdf-rollbar-grip"><span class="lico" data-ic="grip-horizontal"></span></span>
    <span class="cdf-rollbar-lbl">Rebuilt to here</span>
    <span class="cdf-rollbar-hint">drag / roll a card</span>`;
  return bar;
}

/* one feature card: header (icon/name/suppress/move/delete/roll) + params */
function cadFeatCard(inst,idx){
  const def=CAD_FEAT_BY_TYPE[inst.type];
  const rolledBack=idx>=cadRollback;
  const card=document.createElement("div");
  card.className="cdf-card"+(inst.collapsed?" collapsed":"")+(inst.suppressed?" off":"")+(rolledBack?" rolled":"");

  const head=document.createElement("div"); head.className="cdf-card-head";
  head.innerHTML=`<span class="cdf-num">${idx+1}</span>
    <span class="lico cdf-chev" data-ic="chevron-down"></span>
    <span class="cdf-card-ico">${CAD_ICONS[def.icon]||""}</span>
    <span class="cdf-card-name">${cadEsc(inst.name)}</span>
    <div class="cdf-card-acts">
      <button class="cdf-act" data-act="up" data-tip="Move up"${idx===0?" disabled":""}><span class="lico" data-ic="chevron-up"></span></button>
      <button class="cdf-act" data-act="down" data-tip="Move down"${idx===CAD_TIMELINE.length-1?" disabled":""}><span class="lico" data-ic="chevron-down"></span></button>
      <button class="cdf-act" data-act="roll" data-tip="Roll back to here"><span class="lico" data-ic="rewind"></span></button>
      <button class="cdf-act" data-act="del" data-tip="Delete"><span class="lico" data-ic="trash-2"></span></button>
      <div class="cdf-toggle ${inst.suppressed?'':'on'}" data-tip="Suppress"><div class="cdf-knob"></div></div>
    </div>`;
  head.onclick=(e)=>{ if(e.target.closest(".cdf-act")||e.target.closest(".cdf-toggle"))return; inst.collapsed=!inst.collapsed; renderCadFeatures(); };
  head.querySelector('[data-act="up"]').onclick=(e)=>{e.stopPropagation();cadFeatMove(idx,-1);};
  head.querySelector('[data-act="down"]').onclick=(e)=>{e.stopPropagation();cadFeatMove(idx,1);};
  head.querySelector('[data-act="roll"]').onclick=(e)=>{e.stopPropagation();cadRollback=idx;renderCadFeatures();toast("Rolled back to "+inst.name,"rewind");};
  head.querySelector('[data-act="del"]').onclick=(e)=>{e.stopPropagation();cadFeatDelete(inst.uid);};
  head.querySelector(".cdf-toggle").onclick=(e)=>{e.stopPropagation();inst.suppressed=!inst.suppressed;renderCadFeatures();toast((inst.suppressed?"Suppressed ":"Unsuppressed ")+inst.name,inst.suppressed?"eye-off":"eye");};
  card.appendChild(head);

  if(!inst.collapsed){
    const body=document.createElement("div"); body.className="cdf-card-body";
    def.params.forEach(p=>body.appendChild(cadFeatParam(inst,p)));
    card.appendChild(body);
  }
  return card;
}

/* build one parameter control off its descriptor, wired to inst.values */
function cadFeatParam(inst,p){
  const v=inst.values;
  switch(p.t){
    case "slider": return sliderCtl(p.label,v[p.k],p.min,p.max,p.step,p.unit||"",val=>v[p.k]=val);
    case "switch": return switchRow(p.label,v[p.k],val=>{v[p.k]=val;});
    case "seg":    return segCtl(p.label,v[p.k],p.opts,val=>v[p.k]=val);
    case "dd":     return ddCtl(p.label,v[p.k],p.opts,val=>v[p.k]=val,{head:p.label,headIcon:"sliders-horizontal",foot:p.opts.length+" options"});
    default:       return document.createElement("div");
  }
}

/* ════════════════════════ TIMELINE OPS ════════════════════════ */
function cadFeatMove(idx,dir){
  const j=idx+dir;
  if(j<0||j>=CAD_TIMELINE.length) return;
  [CAD_TIMELINE[idx],CAD_TIMELINE[j]]=[CAD_TIMELINE[j],CAD_TIMELINE[idx]];
  renderCadFeatures();
}
function cadFeatDelete(uid){
  const i=CAD_TIMELINE.findIndex(x=>x.uid===uid);
  if(i<0) return;
  const name=CAD_TIMELINE[i].name;
  CAD_TIMELINE.splice(i,1);
  if(cadRollback>CAD_TIMELINE.length) cadRollback=CAD_TIMELINE.length;
  renderCadFeatures();
  toast("Removed "+name,"trash-2");
}
/* called by the toolbar when a model feature tool fires: appends at the rollback
   point, rebuilds forward, and switches the right dock to Features. */
function cadAddFeature(type){
  const inst=cadFeatInstance(type);
  inst.collapsed=false;
  const at=Math.min(cadRollback,CAD_TIMELINE.length);
  CAD_TIMELINE.splice(at,0,inst);
  cadRollback=at+1;
  cadRightPane="features"; renderCadRightCarousel();
  renderCadFeatures();
  pushHistory("feature","Added "+CAD_FEAT_BY_TYPE[type].name,CAD_FEAT_BY_TYPE[type].name);
  toast("Added "+CAD_FEAT_BY_TYPE[type].name,"plus");
}

/* ════════════════════════ ADD-FEATURE PICKER ════════════════════════ */
let cadPickerFilter="";
function openCadFeatPicker(){
  const ov=document.getElementById("picker"); if(!ov) return;
  cadPickerFilter=""; const si=document.getElementById("pickerSearch"); if(si) si.value="";
  renderCadPickerGrid();
  ov.classList.add("show");
  setTimeout(()=>{const s=document.getElementById("pickerSearch");if(s)s.focus();},60);
}
function renderCadPickerGrid(){
  const grid=document.getElementById("pickerGrid"); if(!grid) return;
  grid.innerHTML="";
  const term=cadPickerFilter.trim().toLowerCase();
  CAD_FEAT_CATS.forEach(cat=>{
    const items=CAD_FEAT_TYPES.filter(m=>m.cat===cat.key&&(!term||m.name.toLowerCase().includes(term)));
    if(!items.length) return;
    const lbl=document.createElement("div"); lbl.className="cdf-pk-cat"; lbl.textContent=cat.label;
    grid.appendChild(lbl);
    items.forEach(m=>{
      const b=document.createElement("button"); b.className="cdf-pk-item";
      b.innerHTML=`<span class="cdf-pk-ico">${CAD_ICONS[m.icon]}</span>
        <span class="cdf-pk-txt"><span class="cdf-pk-name">${m.name}</span><span class="cdf-pk-desc">${cat.label} feature</span></span>`;
      b.onclick=()=>{ closeCadFeatPicker(); cadAddFeature(m.type); };
      grid.appendChild(b);
    });
  });
  if(!grid.children.length) grid.innerHTML='<div class="cdf-pk-cat">No feature matches.</div>';
  refreshIcons(grid);
}
function closeCadFeatPicker(){ const ov=document.getElementById("picker"); if(ov)ov.classList.remove("show"); }

/* ════════════════════════ RIGHT-DOCK CAROUSEL ════════════════════════
   Properties ⇄ Features ⇄ History — the three right-dock panes. */
const CAD_RIGHT=[
  {key:"props",    label:"Properties", icon:"sliders-horizontal", pane:"paneProps"},
  {key:"features", label:"Features",   icon:"git-commit-vertical",pane:"paneFeatures"},
  {key:"history",  label:"History",    icon:"history",            pane:"paneHistory"},
];
let cadRightPane="props";
function renderCadRightCarousel(){ renderPill(rightPill,CAD_RIGHT,cadRightPane,selectCadRightPane); }
function selectCadRightPane(key){
  cadRightPane=key;
  showPane(CAD_RIGHT,key);
  rightPill.querySelectorAll(".di-seg").forEach(el=>el.classList.toggle("active",el.dataset.pane===key));
  if(key==="features") renderCadFeatures();
  else if(key==="history") renderHistory();
  else if(key==="props") renderProps();
}

/* wire the pre-existing header/footer buttons + picker controls (once) */
function bindCadFeatures(){
  const addBtn=document.getElementById("featAddBtn"); if(addBtn) addBtn.onclick=(e)=>{e.stopPropagation();openCadFeatPicker();};
  const coll=document.getElementById("featCollapseAll"); if(coll) coll.onclick=()=>{CAD_TIMELINE.forEach(i=>i.collapsed=true);renderCadFeatures();};
  const roll=document.getElementById("featRollEnd"); if(roll) roll.onclick=()=>{cadRollback=CAD_TIMELINE.length;renderCadFeatures();toast("Rebuilt to end","check-check");};
  const scrim=document.getElementById("pickerScrim"); if(scrim) scrim.onclick=closeCadFeatPicker;
  const search=document.getElementById("pickerSearch"); if(search) search.oninput=function(){cadPickerFilter=this.value;renderCadPickerGrid();};
  window.addEventListener("keydown",e=>{ if(e.key==="Escape") closeCadFeatPicker(); });
}
