"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   properties.js — RIGHT-dock Properties pane: the header (icon / name / tag),
                   the empty state, and the per-kind property router for the CAD
                   browser selection (solid / surface / sketch / plane / datums /
                   folder). Mirrors Studio's renderProps() cad branch 1:1.
   ════════════════════════════════════════════════════════════════════════════ */

const propEmpty=document.getElementById("propEmpty");
const propsBody=document.getElementById("propsBody");
const propName=document.getElementById("propName");
const propSub=document.getElementById("propSub");
const propIco=document.getElementById("propIco");

/* icon key per body kind, for the header chip */
const CAD_KIND_ICON={solid:"box",surface:"layers",sketch:"pen-tool",plane:"square",datums:"compass",folder:"folder"};
const CAD_KIND_LABEL={solid:"Solid body",surface:"Surface",sketch:"Sketch",plane:"Datum plane",datums:"Datums",folder:"Group"};

function clearProps(){
  if(typeof gizmoDetach==="function") gizmoDetach();
  propEmpty.style.display="flex"; propsBody.style.display="none"; propsBody.innerHTML="";
  propName.textContent="Properties";
  propSub.innerHTML=`<span class="rdh-tag"><span class="rdh-tagdot"></span> No selection</span>`;
  propIco.innerHTML=`<span class="lico" data-ic="sliders-horizontal"></span>`;
  propIco.style.background="";propIco.style.color="";propIco.style.borderColor="";
  refreshIcons(propIco);
}

/* subtle swatch tint on the header icon */
function paintPropHeadIcon(n){
  propIco.innerHTML=`<span class="lico" data-ic="${CAD_KIND_ICON[n.kind]||"box"}"></span>`;
  if(n.swatch){propIco.style.background=n.swatch+"14";propIco.style.borderColor=n.swatch+"33";propIco.style.color="var(--text)";}
  else{propIco.style.background="var(--panel-3)";propIco.style.borderColor="var(--border)";propIco.style.color="var(--text-dim)";}
  refreshIcons(propIco);
}

/* inline-rename from the header, committing to the same node the browser edits */
function beginPropRename(n){
  if(propName.querySelector("input")) return;
  const old=n.name;
  const inp=document.createElement("input");inp.className="label-input";inp.value=old;
  propName.textContent="";propName.appendChild(inp);inp.focus();inp.select();
  const commit=()=>{n.name=inp.value.trim()||old;renderTree();renderProps();};
  inp.onblur=commit;
  inp.onkeydown=(e)=>{e.stopPropagation();if(e.key==="Enter")inp.blur();if(e.key==="Escape"){inp.value=old;inp.blur();}};
}

/* colour strip for a drawn sketch — writes BOTH the node swatch and the object
   fill colour (the shared colorStrip only touches n.swatch). */
function sketchColorStrip(n,obj){
  const wrap=document.createElement("div");
  const lbl=document.createElement("div");lbl.className="ctl-label";lbl.style.marginBottom="2px";lbl.textContent="Fill colour";wrap.appendChild(lbl);
  const strip=document.createElement("div");strip.className="swatch-strip";
  PALETTE.forEach(c=>{const s=document.createElement("div");s.className="sw";s.style.background=c;
    s.onclick=()=>{n.swatch=c;obj.colors.fill=c;renderTree();renderProps();vp3Draw();};strip.appendChild(s);});
  wrap.appendChild(strip);return wrap;
}

/* attach the transform gizmo to a drawn-sketch selection, detach otherwise */
function syncGizmoToSelection(n){
  if(typeof gizmoAttach!=="function") return;
  // the gizmo only lives in Object mode; Sketch mode has no transform gizmo
  const objectMode=(typeof CAD==="undefined")||CAD.imode!=="sketch";
  const obj=(objectMode&&n&&n.kind==="sketch"&&n.skId&&typeof SKETCHES!=="undefined")?SKETCHES.find(s=>s.id===n.skId):null;
  if(obj) gizmoAttach(obj); else gizmoDetach();
}

function renderProps(){
  const n=findItem(selectedId);
  syncGizmoToSelection(n);
  if(!n){ clearProps(); return; }
  propEmpty.style.display="none"; propsBody.style.display="block"; propsBody.innerHTML="";
  propName.textContent=n.name; propName.title="Double-click to rename";
  propName.ondblclick=()=>beginPropRename(n);
  propSub.innerHTML=`<span class="rdh-tag"><span class="rdh-tagdot" style="background:${n.swatch||'#ffffff'}"></span>${CAD_KIND_LABEL[n.kind]||"Item"}</span>`;
  paintPropHeadIcon(n);

  // common: visibility
  const gen=section("General");
  gen.content.appendChild(switchRow("Visible",n.visible,v=>{n.visible=v;renderTree();}));
  propsBody.appendChild(gen.el);

  if(n.kind==="solid"||n.kind==="surface"){
    const g=section("Body");
    g.content.appendChild(infoRow("Faces",n.faces));
    g.content.appendChild(infoRow("Edges",n.edges));
    if(n.kind==="solid") g.content.appendChild(infoRow("Volume",n.volume.toFixed(1)+" cm³"));
    else g.content.appendChild(infoRow("Area",n.area.toFixed(1)+" cm²"));
    g.content.appendChild(colorStrip(n));
    propsBody.appendChild(g.el);
    if(n.kind==="solid"){
      const m=section("Material");
      m.content.appendChild(ddCtl("Appearance",n.material,
        ["Aluminium 6061","ABS Plastic","Stainless Steel","Brass","Titanium","Carbon Fibre"],
        v=>{n.material=v;renderProps();},
        {head:"Material",headIcon:"layers",foot:"Drives mass & render look",footIcon:"info"}));
      propsBody.appendChild(m.el);
    }
    const d=section("Display");
    d.content.appendChild(segCtl("Shading",n._shade||"shaded",[["shaded","Shaded"],["wire","Wire"],["xray","X-Ray"]],v=>n._shade=v));
    propsBody.appendChild(d.el);
  } else if(n.kind==="sketch"&&n.skId&&typeof SKETCHES!=="undefined"&&SKETCHES.find(s=>s.id===n.skId)){
    // a DRAWN sketch object — the live geometry. Properties expand per category so
    // the whole shape is editable: circle=radius, rect=W/H, polygon=sides+radius,
    // curves=control-point list (+degree/weights).
    const obj=SKETCHES.find(s=>s.id===n.skId);
    const curve=(typeof isCurveCategory==="function")&&isCurveCategory(obj.category);
    const g=section("Sketch");
    g.content.appendChild(infoRow("Category",obj.category.charAt(0).toUpperCase()+obj.category.slice(1)));
    g.content.appendChild(infoRow("On plane",obj.plane.name));
    g.content.appendChild(infoRow(curve?"Control points":"Points",curve?obj.cpts.length:obj.points.length));
    g.content.appendChild(infoRow("Closed",obj.closed?"Yes":"No"));
    if(obj.category==="polygon"){
      g.content.appendChild(sliderCtl("Sides",obj.sides,3,64,1,"",v=>{obj.sides=Math.round(v);sketchRebuildPoly(obj);vp3Draw();}));
      g.content.appendChild(sliderCtl("Radius",sketchRadiusOf(obj),0.1,20,0.1,"mm",v=>{sketchSetRadius(obj,v);vp3Draw();}));
    } else if(obj.category==="circle"){
      g.content.appendChild(sliderCtl("Radius",sketchRadiusOf(obj),0.1,20,0.1,"mm",v=>{sketchSetRadius(obj,v);vp3Draw();}));
    } else if(obj.category==="rectangle"){
      g.content.appendChild(sliderCtl("Width",Math.abs(obj.w),0.1,20,0.1,"mm",v=>{obj.w=Math.sign(obj.w||1)*Math.max(0.05,v);sketchRebuildRect(obj);vp3Draw();}));
      g.content.appendChild(sliderCtl("Height",Math.abs(obj.h),0.1,20,0.1,"mm",v=>{obj.h=Math.sign(obj.h||1)*Math.max(0.05,v);sketchRebuildRect(obj);vp3Draw();}));
    }
    g.content.appendChild(sketchColorStrip(n,obj));
    propsBody.appendChild(g.el);
    // curve-specific: degree, closed toggle, and an editable control-point list
    if(curve){
      const cg=section("Curve");
      if(obj.category==="bspline"||obj.category==="nurbs"){
        cg.content.appendChild(sliderCtl("Degree",obj.degree||3,1,Math.max(1,obj.cpts.length-1),1,"",v=>{obj.degree=Math.round(v);sketchRebuildCurve(obj);vp3Draw();}));
      }
      cg.content.appendChild(switchRow("Closed",obj.closed,v=>{obj.closed=v;sketchRebuildCurve(obj);vp3Draw();}));
      propsBody.appendChild(cg.el);
      const pg=section("Control Points");
      obj.cpts.forEach((c,i)=>{
        const hot=(typeof PT!=="undefined"&&PT.sel&&PT.obj===obj&&PT.sel.i===i&&PT.sel.part==="anchor");
        pg.content.appendChild(coordCtl("P"+i,[c[0],c[1]],(k,val)=>{obj.cpts[i][k]=val;sketchRebuildCurve(obj);vp3Draw();},hot));
        if(obj.category==="nurbs"&&obj.weights){
          pg.content.appendChild(sliderCtl("W"+i,obj.weights[i],0.1,10,0.1,"",v=>{obj.weights[i]=v;sketchRebuildCurve(obj);vp3Draw();}));
        }
      });
      propsBody.appendChild(pg.el);
    }
    const t=section("Transform");
    t.content.appendChild(segCtl("Gizmo",GZ.mode,[["translate","Move"],["rotate","Rotate"],["scale","Scale"]],v=>gizmoSetMode(v)));
    t.content.appendChild(infoRow("Move",obj.transform.t.map(x=>x.toFixed(2)).join(", ")));
    propsBody.appendChild(t.el);
  } else if(n.kind==="sketch"){
    const g=section("Sketch");
    g.content.appendChild(infoRow("On plane",n.plane));
    g.content.appendChild(infoRow("Constraints",n.constraints));
    g.content.appendChild(infoRow("Status",n.fullyDefined?"Fully defined":"Under-defined"));
    g.content.appendChild(colorStrip(n));
    propsBody.appendChild(g.el);
    const t=section("Edit");
    t.content.appendChild(btn("Edit Sketch","pen-tool",()=>{setCadEnv("sketch");toast("Editing "+n.name,"pen-tool");}));
    t.content.appendChild(btn("Auto-Dimension","ruler",()=>toast("Auto-dimensioned "+n.name,"ruler")));
    propsBody.appendChild(t.el);
  } else if(n.kind==="plane"){
    const g=section("Datum Plane");
    g.content.appendChild(infoRow("Orientation",n.axis));
    g.content.appendChild(colorStrip(n));
    propsBody.appendChild(g.el);
    const t=section("New Sketch");
    t.content.appendChild(btn("Start Sketch Here","pen-tool",()=>{setCadEnv("sketch");toast("New sketch on "+n.name,"pen-tool");}));
    propsBody.appendChild(t.el);
  } else {
    const g=section(n.kind==="datums"?"Datums":"Group");
    g.content.appendChild(infoRow("Children",(n.children||[]).length));
    propsBody.appendChild(g.el);
  }
  refreshIcons(propsBody);
}
