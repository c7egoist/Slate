"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   history.js — the History timeline pane (Properties ⇄ Features ⇄ History).
                A branching edit history for the single CAD part:

                  • A tree is an ordered list of events plus a cursor
                    ("you are here"). Clicking an event jumps the cursor to it;
                    events past the cursor become a greyed-out future.
                  • Recording a NEW event while jumped back forks the future into
                    a fresh sibling tree so nothing is lost.
                  • The branch-tab strip switches trees, spawns a manual branch
                    (+), and deletes a tree (× — at least one always remains).

                Other modules call pushHistory(...) to record an event.
   ════════════════════════════════════════════════════════════════════════════ */

/* event type → {icon, label, accent-class, verb}. layerEvt draws a ringed glyph. */
const HX_TYPES={
  start:    {icon:"box",                 label:"Start"},
  feature:  {icon:"git-commit-vertical", label:"Feature", cls:"param"},
  param:    {icon:"sliders-horizontal",  label:"Params",  cls:"param"},
  sketch:   {icon:"pen-tool",            label:"Sketch",  cls:"gen"},
  transform:{icon:"move",                label:"Transform",cls:"gen"},
  body:     {icon:"box",                 label:"Body",    cls:"mat"},
  // ringed-glyph node events
  add:   {icon:"plus",    layerEvt:true, verb:"Create"},
  edit:  {icon:"layers3", layerEvt:true, verb:"Edit"},
  drop:  {icon:"trash-2", layerEvt:true, verb:"Drop"},
};

/* single store for the CAD part: {trees:[tree,…], active:idx}. */
let _hxSeq=0, _hxTreeSeq=0;
let HX_STORE={trees:[],active:0};
function hxNewTree(name,events){
  return {id:"tree"+(++_hxTreeSeq), name:name||("Branch "+(_hxTreeSeq)), events:events||[], cursor:(events?events.length-1:-1)};
}
function hxState(){
  if(!HX_STORE.trees.length) HX_STORE={trees:[hxNewTree("Main")],active:0};
  return HX_STORE;
}
function hxActiveTree(){ const s=hxState(); return s.trees[s.active]; }

/* the body/part an event is attributed to (drives the rail chip colour) */
function hxActiveBody(){
  const n=findItem(selectedId)||flat(CAD_TREE).find(x=>x.kind==="solid");
  return n?{name:n.name,swatch:n.swatch||"#ffffff"}:{name:"Part01",swatch:"#ffffff"};
}
function hxTime(d){ return d.toLocaleTimeString([],{hour:"2-digit",minute:"2-digit",second:"2-digit"}); }
function hxMakeEvent(type,title,subtitle,bodyOverride){
  const b=bodyOverride||hxActiveBody();
  return {id:"h"+(++_hxSeq),at:new Date(),layerName:b.name,layerColor:b.swatch||"#ffffff",type,title,subtitle};
}

/* ─── RECORD AN EVENT ─── (forks if the cursor is behind the tip) */
function pushHistory(type,title,subtitle,bodyOverride){
  const s=hxState(), tree=s.trees[s.active];
  const ev=hxMakeEvent(type,title,subtitle,bodyOverride);
  const atTip=tree.cursor===tree.events.length-1;
  if(!atTip){
    const future=tree.events.slice(tree.cursor+1);
    tree.events=tree.events.slice(0,tree.cursor+1);
    const fork=hxNewTree("Branch "+(_hxTreeSeq+1),[...tree.events,...future]);
    s.trees.push(fork);
  }
  tree.events.push(ev);
  tree.cursor=tree.events.length-1;
  if(hxVisible()) renderHistory();
}
function hxVisible(){
  const pane=document.getElementById("paneHistory");
  return pane && pane.style.display!=="none" && typeof cadRightPane!=="undefined" && cadRightPane==="history";
}

/* ─── SEED the CAD timeline ─── empty scene: a single "Part created" event so the
   history rail isn't blank, then events are recorded as the tools are used. */
function seedHistory(){
  _hxSeq=0; _hxTreeSeq=0;
  const part={name:"Part01",swatch:"#ffffff"};
  const e=[hxMakeEvent("start","Part created","New part · mm · 0.01 tol",part)];
  HX_STORE={trees:[hxNewTree("Main",e)],active:0};
}

/* ─── JUMP: set the cursor to a clicked event ─── */
function hxJump(idx){
  const tree=hxActiveTree();
  tree.cursor=Math.max(0,Math.min(tree.events.length-1,idx));
  renderHistory();
  const ev=tree.events[tree.cursor];
  toast("Jumped to “"+(ev.title||ev.subtitle||HX_TYPES[ev.type].label)+"”","history");
}

/* ─── BRANCH TABS ─── (inserted before #hxScroll on first render) */
function hxTreesStrip(){
  let strip=document.getElementById("hxTrees");
  if(!strip){
    const scroll=document.getElementById("hxScroll"); if(!scroll) return null;
    strip=document.createElement("div"); strip.id="hxTrees"; strip.className="hx-trees";
    scroll.parentNode.insertBefore(strip,scroll);
  }
  return strip;
}
function renderTrees(){
  const strip=hxTreesStrip(); if(!strip) return;
  const s=hxState();
  strip.innerHTML="";
  s.trees.forEach((t,i)=>{
    const tab=document.createElement("button");
    tab.className="hx-tree"+(i===s.active?" active":"");
    tab.innerHTML=`<span class="lico" data-ic="git-branch"></span><span class="hx-tree-nm">${t.name}</span>
      <span class="hx-tree-ct">${t.events.length}</span>
      ${s.trees.length>1?`<span class="hx-tree-x" data-del="${i}"><span class="lico" data-ic="x"></span></span>`:""}`;
    tab.onclick=(e)=>{ if(e.target.closest(".hx-tree-x")){hxDeleteTree(i);return;} s.active=i; renderHistory(); };
    strip.appendChild(tab);
  });
  const add=document.createElement("button");
  add.className="hx-tree-add"; add.dataset.tip="New branch from here";
  add.innerHTML=`<span class="lico" data-ic="plus"></span>`;
  add.onclick=hxBranchHere;
  strip.appendChild(add);
  refreshIcons(strip);
}
function hxBranchHere(){
  const s=hxState(), tree=s.trees[s.active];
  const upto=tree.events.slice(0,tree.cursor+1);
  const fork=hxNewTree("Branch "+(_hxTreeSeq+1),upto.map(e=>Object.assign({},e)));
  s.trees.push(fork); s.active=s.trees.length-1;
  renderHistory();
  toast("Created "+fork.name,"git-branch");
}
function hxDeleteTree(i){
  const s=hxState();
  if(s.trees.length<=1) return;
  const gone=s.trees[i].name;
  s.trees.splice(i,1);
  if(s.active>=s.trees.length) s.active=s.trees.length-1;
  else if(i<s.active) s.active--;
  renderHistory();
  toast("Deleted "+gone,"trash-2");
}

/* ─── RENDER ─── */
function renderHistory(){
  renderTrees();
  const scroll=document.getElementById("hxScroll"); if(!scroll) return;
  scroll.innerHTML="";
  const tree=hxActiveTree();
  const events=tree.events;
  if(!events.length){ scroll.innerHTML='<div class="hx-empty">No history yet.<br>Add features or edit the part to record events.</div>'; return; }
  events.forEach((ev,i)=>{
    const t=HX_TYPES[ev.type]||HX_TYPES.feature;
    const isFirst=i===0, isLast=i===events.length-1;
    const future=i>tree.cursor;
    const atCursor=i===tree.cursor;
    const prevColor=isFirst?ev.layerColor:events[i-1].layerColor;

    const row=document.createElement("div");
    row.className="hx-ev"+(future?" future":"")+(atCursor?" at-cursor":"");
    row.style.setProperty("--lcolor",ev.layerColor);

    // rail
    const rail=document.createElement("div"); rail.className="hx-rail";
    const top=document.createElement("div"); top.className="hx-line top"+(isFirst?" hidden":"");
    top.style.background=prevColor;
    const node=document.createElement("div");
    if(t.layerEvt){ node.className="hx-node glyph"; node.innerHTML=`<span class="lico" data-ic="${t.icon}"></span>`; }
    else node.className="hx-node";
    const bot=document.createElement("div"); bot.className="hx-line"+(isLast?" hidden":"");
    rail.appendChild(top); rail.appendChild(node); rail.appendChild(bot);

    // body
    const body=document.createElement("div"); body.className="hx-body";
    const main=document.createElement("div"); main.className="hx-main";
    if(t.layerEvt){
      main.innerHTML=`<div class="hx-layerline">
        <span class="hx-verb">${t.verb}</span>
        <span class="hx-chip" style="background:${ev.layerColor}">${ev.subtitle||ev.layerName}</span>
        <span class="hx-lname${ev.type==="drop"?" gone":""}">${ev.layerName}</span></div>`;
    }else{
      main.innerHTML=`<div class="hx-evt-title">${ev.title||t.label}</div>
        <div class="hx-evt-meta">
          <span class="hx-type ${t.cls||""}"><span class="lico" data-ic="${t.icon}"></span>${t.label}</span>
          ${ev.subtitle?`<span class="hx-sub">${ev.subtitle}</span>`:""}
        </div>`;
    }
    const time=document.createElement("div"); time.className="hx-time"; time.textContent=hxTime(ev.at);
    body.appendChild(main); body.appendChild(time);

    row.appendChild(rail); row.appendChild(body);
    row.title="Jump to this point";
    row.onclick=()=>hxJump(i);
    scroll.appendChild(row);
  });
  refreshIcons(scroll);
  const cur=scroll.querySelector(".hx-ev.at-cursor");
  if(cur) cur.scrollIntoView({block:"nearest"});
}

/* undo / redo — wired to every [data-hist] button (header + history pane) */
function hxUndo(){ const t=hxActiveTree(); if(t.cursor>0){ t.cursor--; renderHistory(); } else toast("At the start","undo-2"); }
function hxRedo(){ const t=hxActiveTree(); if(t.cursor<t.events.length-1){ t.cursor++; renderHistory(); } else toast("Nothing to redo","redo-2"); }
function bindHistoryButtons(){
  document.querySelectorAll('[data-hist]').forEach(b=>{
    b.onclick=()=>{ b.dataset.hist==="undo"?hxUndo():hxRedo(); };
  });
  window.addEventListener("keydown",e=>{
    if(e.target.matches("input,textarea")) return;
    if((e.ctrlKey||e.metaKey)&&!e.shiftKey&&e.key.toLowerCase()==="z"){ e.preventDefault(); hxUndo(); }
    else if((e.ctrlKey||e.metaKey)&&(e.key.toLowerCase()==="y"||(e.shiftKey&&e.key.toLowerCase()==="z"))){ e.preventDefault(); hxRedo(); }
  });
}
