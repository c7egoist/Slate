"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   browser.js — LEFT-dock Browser pane: the feature/body tree (Origin datums,
                Sketches, Bodies), row selection / visibility / delete / rename,
                the search filter, and the browser footer counts.
   ════════════════════════════════════════════════════════════════════════════ */

const tree=document.getElementById("tree");

/* flatten the tree into a list; find a node by id */
function flat(list,arr=[]){ list.forEach(n=>{arr.push(n); if(n.children&&n.children.length) flat(n.children,arr);}); return arr; }
function findItem(id){ return flat(CAD_TREE).find(n=>n.id===id)||null; }

/* count the solid/surface bodies, for the footer body count */
function bodyCount(){ return flat(CAD_TREE).filter(n=>n.kind==="solid"||n.kind==="surface").length; }

function renderTree(){
  tree.innerHTML="";
  const matches=(n)=>(!searchTerm||n.name.toLowerCase().includes(searchTerm));
  function renderNode(n,depth){
    const visibleSelf=matches(n);
    const childEls=[];
    if(n.children) n.children.forEach(c=>{const el=renderNode(c,depth+1); if(el) childEls.push(el);});
    if(!visibleSelf && childEls.length===0) return null;

    const wrap=document.createElement("div"); wrap.className="node";
    const row=document.createElement("div");
    row.className="row"+(n.selected?" selected":"")+(n.visible?"":" hidden-row");

    const hasKids=n.children&&n.children.length;
    const twisty = hasKids
      ? `<span class="twisty ${n.collapsed?'collapsed':''}"><span class="lico" data-ic="chevron-down"></span></span>`
      : `<span class="twisty empty"></span>`;
    // show the type icon AND (when present) a small colour dot, so the category
    // reads by both glyph and colour. The icon is tinted to the swatch colour too.
    const dot = n.swatch?`<span class="type-dot" style="background:${n.swatch}"></span>`:"";
    const ico = `<span class="r-ico"${n.swatch?` style="color:${n.swatch}"`:""}><span class="lico" data-ic="${n.icon}"></span></span>`;
    const cnt = hasKids?`<span class="count">${n.children.length}</span>`:"";

    row.innerHTML=`<span class="indent" style="width:${depth*14}px"></span>${twisty}${ico}${dot}<span class="label">${n.name}</span>${cnt}
      <span class="row-actions">
        <button class="act ${n.visible?'':'off'}" data-act="vis"><span class="lico" data-ic="${n.visible?'eye':'eye-off'}"></span></button>
        <button class="act" data-act="del"><span class="lico" data-ic="trash-2"></span></button>
      </span>`;

    row.onclick=(e)=>{ if(e.target.closest(".act")||e.target.closest(".twisty"))return; selectItem(n.id, e.shiftKey||e.ctrlKey||e.metaKey); };
    // a dimension/constraint row (kind "cxnode") double-clicks to edit a dimensional
    // value (via the same floating box the on-canvas label uses) rather than rename.
    row.ondblclick=(e)=>{ if(e.target.closest(".act"))return;
      if(n.kind==="cxnode"){ cxNodeEditValue(n); return; }
      renameItem(row,n); };
    if(hasKids) row.querySelector(".twisty").onclick=(e)=>{e.stopPropagation();n.collapsed=!n.collapsed;renderTree();};
    row.querySelector('[data-act="vis"]').onclick=(e)=>{e.stopPropagation();n.visible=!n.visible;renderTree();if(n.id===selectedId)renderProps();};
    row.querySelector('[data-act="del"]').onclick=(e)=>{e.stopPropagation();deleteItem(n.id);};

    wrap.appendChild(row);
    if(!n.collapsed) childEls.forEach(c=>wrap.appendChild(c));
    return wrap;
  }
  CAD_TREE.forEach(n=>{const el=renderNode(n,0); if(el) tree.appendChild(el);});
  if(!tree.children.length) tree.innerHTML='<div class="empty-state">No items.<br>Use New Sketch to add one.</div>';
  syncBrowserFooter();
  refreshIcons(tree);
}

/* footer: body count + feature count (features.js owns CAD_TIMELINE length) */
function syncBrowserFooter(){
  const bc=document.getElementById("bodyCount"); if(bc) bc.textContent=bodyCount();
  const fc=document.getElementById("featCountFoot");
  if(fc) fc.textContent=(typeof CAD_TIMELINE!=="undefined"?CAD_TIMELINE.length:0);
}

/* select a node. `additive` (shift/ctrl) toggles it into an ordered multi-selection
   (selectionOrder[0] = the boolean base); non-additive replaces the selection with
   just this node. selectedId tracks the LAST selected (drives props/gizmo). */
function selectItem(id,additive){
  if(additive && id!=null){
    const at=selectionOrder.indexOf(id);
    if(at>=0){ selectionOrder.splice(at,1); const n=findItem(id); if(n) n.selected=false; }
    else { selectionOrder.push(id); const n=findItem(id); if(n) n.selected=true; }
    selectedId=selectionOrder.length?selectionOrder[selectionOrder.length-1]:null;
  } else {
    flat(CAD_TREE).forEach(n=>n.selected=false);
    selectionOrder = (id!=null)?[id]:[];
    const n=findItem(id); if(n)n.selected=true;
    selectedId=id;
  }
  const cur=selectedId!=null?findItem(selectedId):null;
  const stSel=document.getElementById("stSel");
  if(stSel) stSel.textContent=selectionOrder.length>1?(selectionOrder.length+" selected"):(cur?cur.name:"Nothing selected");
  renderTree();
  renderProps();
}
function deleteItem(id){
  const gone=findItem(id);
  // a dimension/constraint row removes its constraint (which prunes its own tree node),
  // NOT the sketch geometry — route it before the generic geometry removal below.
  if(gone&&gone.kind==="cxnode"){
    if(typeof removeConstraint==="function") removeConstraint(gone.cxId);
    if(selectedId===id){ selectedId=null; clearProps(); }
    if(typeof toast==="function") toast("Removed "+gone.name,"trash-2");
    return;
  }
  // drop any sketch constraints that referenced the object being removed
  if(typeof SKETCHES!=="undefined"&&typeof removeConstraintsFor==="function"){
    const obj=SKETCHES.find(s=>s.nodeId===id);
    if(obj) removeConstraintsFor(obj.id);
  }
  function rec(list){const i=list.findIndex(x=>x.id===id);if(i>=0){list.splice(i,1);return true;}return list.some(x=>x.children&&rec(x.children));}
  rec(CAD_TREE);
  if(selectedId===id){selectedId=null;clearProps();}
  renderTree();
  if(gone) toast("Removed "+gone.name,"trash-2");
}
/* double-clicking a dimension row opens the floating value box (same one the on-canvas
   label uses) and re-solves via setConstraintValue. Geometric constraints have no value
   to edit, so we just select + toast. */
function cxNodeEditValue(n){
  if(typeof CONSTRAINTS==="undefined") return;
  const c=CONSTRAINTS.find(k=>k.id===n.cxId); if(!c) return;
  if(typeof CX_DIMENSIONAL==="undefined"||!CX_DIMENSIONAL.has(c.type)){
    if(typeof toast==="function") toast((CX_LABEL[c.type]||c.type)+" constraint","info"); return;
  }
  const seed=(c.type==="angle")?c.value*180/Math.PI:c.value;
  if(typeof cpickPromptValue==="function")
    cpickPromptValue(null,c.type,seed,(val)=>{ if(val!=null&&typeof setConstraintValue==="function") setConstraintValue(c.id,val); });
}
function renameItem(row,n){
  const lab=row.querySelector(".label"); const old=n.name;
  const inp=document.createElement("input"); inp.className="label-input"; inp.value=old;
  lab.replaceWith(inp); inp.focus(); inp.select();
  const commit=()=>{n.name=inp.value.trim()||old;renderTree();if(n.id===selectedId)renderProps();};
  inp.onblur=commit;
  inp.onkeydown=(e)=>{e.stopPropagation();if(e.key==="Enter")inp.blur();if(e.key==="Escape"){inp.value=old;inp.blur();}};
}

/* search box */
const _olSearch=document.getElementById("olSearch");
if(_olSearch) _olSearch.oninput=function(){searchTerm=this.value.trim().toLowerCase();renderTree();};
