"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   constraints.js — a Fusion/SolidWorks-style 2D parametric sketch solver.

     • A document-level list of constraints (CONSTRAINTS) referencing sketch
       entities across objects — geometric relations (coincident, horizontal,
       vertical, parallel, perpendicular, equal, fix) and dimensional relations
       (linear distance, angle) that carry a target value.
     • A small numerical solver (Levenberg-Marquardt with a finite-difference
       Jacobian + dense Gaussian elimination on the normal equations) that moves
       the free control points until every residual is ~0.

   COORDINATE FRAME. A sketch is 2D. Every constrained object is assumed to share
   the SAME plane, so a curve control point's stored plane-space [u,v] (obj.cpts[i])
   IS the solver variable — no world round-trip needed. Points that never appear in
   a constraint are not variables (they don't move); "fix"-ed points are held as
   constants in the residuals.

   ENTITY REFS. { objId, kind:'point'|'line'|'circle', i }:
     • point → the curve control point cpts[i].
     • line  → the control-polygon segment cpts[i]→cpts[i+1] (its two endpoints).
     • circle→ the whole center/rim shape (used by Phase-3 primitive relations).
   ════════════════════════════════════════════════════════════════════════════ */

const CONSTRAINTS=[];            // document-level; element = {id,type,entities:[ref],value?}
let _cxUid=0;

const CX_TOL=1e-7;               // residual convergence tolerance
const CX_MAX_ITER=80;            // solver iteration cap
const CX_FD=1e-6;                // finite-difference step for the Jacobian

/* human labels + the glyph the render pass draws next to each constraint */
const CX_LABEL={
  coincident:"Coincident", horizontal:"Horizontal", vertical:"Vertical",
  parallel:"Parallel", perpendicular:"Perpendicular", equal:"Equal", fix:"Fix",
  concentric:"Concentric", tangent:"Tangent",
  linear:"Dimension", hdist:"Horizontal", vdist:"Vertical",
  angle:"Angle", radius:"Radius", diameter:"Diameter",
};
/* how many entities each constraint type consumes (dimensional ones carry a value) */
const CX_ARITY={
  coincident:2, horizontal:1, vertical:1, parallel:2, perpendicular:2, equal:2, fix:1,
  concentric:2, tangent:2, linear:2, hdist:2, vdist:2, angle:2, radius:1, diameter:1,
};
const CX_DIMENSIONAL=new Set(["linear","hdist","vdist","angle","radius","diameter"]);

/* ─── ENTITY RESOLUTION ─── */
function cxObj(objId){
  if(typeof SKETCHES==="undefined") return null;
  return SKETCHES.find(s=>s.id===objId)||null;
}
/* a stable key for one control point (object + anchor index) */
function cxPointKey(objId,i){ return objId+"#"+i; }
/* expand an entity ref into the control points it touches: [{objId,i}...] */
function cxEntityPoints(ent){
  if(ent.kind==="point") return [{objId:ent.objId,i:ent.i}];
  if(ent.kind==="line"){
    const obj=cxObj(ent.objId); const n=obj?obj.cpts.length:0;
    return [{objId:ent.objId,i:ent.i},{objId:ent.objId,i:(ent.i+1)%Math.max(n,1)}];
  }
  return [];   // circle handled directly by its own residuals (Phase 3)
}
/* current plane-space [u,v] of a control point straight from its object */
function cxReadUV(objId,i){
  const obj=cxObj(objId); if(!obj||!obj.cpts||!obj.cpts[i]) return [0,0];
  return [obj.cpts[i][0],obj.cpts[i][1]];
}

/* ─── VARIABLE SYSTEM ─── gather every point touched by a constraint; drop the
   ones pinned by a fix constraint; the rest become 2 solver variables each. */
function cxBuildSystem(pin){
  const fixed=new Set();
  for(const c of CONSTRAINTS){
    if(c.type!=="fix") continue;
    for(const p of cxEntityPoints(c.entities[0])) fixed.add(cxPointKey(p.objId,p.i));
  }
  // caller-pinned points (e.g. the one just dragged) are held constant this solve
  if(pin) for(const k of pin) fixed.add(k);
  const varmap={};        // pointKey → base slot in x
  const varkeys=[];       // slot/2 → {objId,i}
  const constMap={};      // pointKey → [u,v] snapshot (for fixed + read-only reads)
  const touch=(objId,i)=>{
    const key=cxPointKey(objId,i);
    if(!(key in constMap)) constMap[key]=cxReadUV(objId,i);
    if(fixed.has(key)||key in varmap) return;
    varmap[key]=varkeys.length*2; varkeys.push({objId,i});
  };
  for(const c of CONSTRAINTS)
    for(const ent of c.entities)
      for(const p of cxEntityPoints(ent)) touch(p.objId,p.i);
  return {varmap,varkeys,constMap,fixed};
}
/* [u,v] of a point inside the solver: from x if it's a variable, else the snapshot */
function cxUV(key,x,sys){
  if(key in sys.varmap){ const b=sys.varmap[key]; return [x[b],x[b+1]]; }
  return sys.constMap[key]||[0,0];
}
/* the two endpoints [P,Q] of a line entity as live [u,v] pairs */
function cxLineEnds(ent,x,sys){
  const ps=cxEntityPoints(ent);
  return [cxUV(cxPointKey(ps[0].objId,ps[0].i),x,sys),
          cxUV(cxPointKey(ps[1].objId,ps[1].i),x,sys)];
}
function cxPointUV(ent,x,sys){ return cxUV(cxPointKey(ent.objId,ent.i),x,sys); }

/* ─── RESIDUALS ─── one constraint contributes 1+ scalar residuals; the solver
   drives the whole stacked vector to zero. */
function cxResidual(c,x,sys,out){
  switch(c.type){
    case "coincident":{
      const p=cxPointUV(c.entities[0],x,sys), q=cxPointUV(c.entities[1],x,sys);
      out.push(p[0]-q[0], p[1]-q[1]); break;
    }
    case "horizontal":{                 // a line's endpoints share v
      const [a,b]=cxLineEnds(c.entities[0],x,sys); out.push(a[1]-b[1]); break;
    }
    case "vertical":{                    // a line's endpoints share u
      const [a,b]=cxLineEnds(c.entities[0],x,sys); out.push(a[0]-b[0]); break;
    }
    case "parallel":{                    // cross(d1,d2)=0
      const [a,b]=cxLineEnds(c.entities[0],x,sys), [p,q]=cxLineEnds(c.entities[1],x,sys);
      const d1=[b[0]-a[0],b[1]-a[1]], d2=[q[0]-p[0],q[1]-p[1]];
      out.push(d1[0]*d2[1]-d1[1]*d2[0]); break;
    }
    case "perpendicular":{               // dot(d1,d2)=0
      const [a,b]=cxLineEnds(c.entities[0],x,sys), [p,q]=cxLineEnds(c.entities[1],x,sys);
      const d1=[b[0]-a[0],b[1]-a[1]], d2=[q[0]-p[0],q[1]-p[1]];
      out.push(d1[0]*d2[0]+d1[1]*d2[1]); break;
    }
    case "equal":{                       // equal squared length (scale-stable)
      const [a,b]=cxLineEnds(c.entities[0],x,sys), [p,q]=cxLineEnds(c.entities[1],x,sys);
      const l1=(b[0]-a[0])**2+(b[1]-a[1])**2, l2=(q[0]-p[0])**2+(q[1]-p[1])**2;
      out.push(l1-l2); break;
    }
    case "linear":{                      // |P-Q| = value
      const p=cxPointUV(c.entities[0],x,sys), q=cxPointUV(c.entities[1],x,sys);
      out.push(Math.hypot(p[0]-q[0],p[1]-q[1])-(c.value||0)); break;
    }
    case "hdist":{                       // |Pu-Qu| = value (plane-space horizontal)
      const p=cxPointUV(c.entities[0],x,sys), q=cxPointUV(c.entities[1],x,sys);
      out.push(Math.abs(p[0]-q[0])-(c.value||0)); break;
    }
    case "vdist":{                       // |Pv-Qv| = value (plane-space vertical)
      const p=cxPointUV(c.entities[0],x,sys), q=cxPointUV(c.entities[1],x,sys);
      out.push(Math.abs(p[1]-q[1])-(c.value||0)); break;
    }
    case "angle":{                       // angle(d1,d2) = value (radians)
      const [a,b]=cxLineEnds(c.entities[0],x,sys), [p,q]=cxLineEnds(c.entities[1],x,sys);
      const d1=[b[0]-a[0],b[1]-a[1]], d2=[q[0]-p[0],q[1]-p[1]];
      const ang=Math.atan2(d1[0]*d2[1]-d1[1]*d2[0], d1[0]*d2[0]+d1[1]*d2[1]);
      out.push(ang-(c.value||0)); break;
    }
    // fix contributes no residual (its points are simply not variables)
    default: break;
  }
}
function cxResidualVec(x,sys){ const out=[]; for(const c of CONSTRAINTS) cxResidual(c,x,sys,out); return out; }
function cxNorm(v){ let s=0; for(const e of v) s+=e*e; return Math.sqrt(s); }

/* ─── LINEAR SOLVE ─── dense Gaussian elimination with partial pivoting on A y = b
   (A is the damped normal matrix JᵀJ+λI). Returns null on a singular system. */
function cxSolveLinear(A,b){
  const n=b.length;
  const M=A.map((row,r)=>row.concat([b[r]]));
  for(let col=0;col<n;col++){
    let piv=col;
    for(let r=col+1;r<n;r++) if(Math.abs(M[r][col])>Math.abs(M[piv][col])) piv=r;
    if(Math.abs(M[piv][col])<1e-12) continue;   // damping keeps it non-singular in practice
    [M[col],M[piv]]=[M[piv],M[col]];
    const d=M[col][col];
    for(let k=col;k<=n;k++) M[col][k]/=d;
    for(let r=0;r<n;r++){
      if(r===col) continue;
      const f=M[r][col];
      if(!f) continue;
      for(let k=col;k<=n;k++) M[r][k]-=f*M[col][k];
    }
  }
  return M.map(row=>row[n]);
}

/* ─── SOLVE ─── Levenberg-Marquardt over the current constraint set. Mutates the
   participating objects' cpts + rebuilds them on success. Returns a report. */
function solveSketch(opt){
  opt=opt||{};
  const sys=cxBuildSystem(opt.pin);
  const nv=sys.varkeys.length*2;
  if(!CONSTRAINTS.length||nv===0) return {ok:true,iters:0,residual:0,vars:nv};

  // seed x from the current geometry
  let x=new Array(nv);
  sys.varkeys.forEach((vk,idx)=>{ const uv=cxReadUV(vk.objId,vk.i); x[idx*2]=uv[0]; x[idx*2+1]=uv[1]; });

  let r=cxResidualVec(x,sys), rn=cxNorm(r);
  let lambda=1e-3, iters=0;
  for(;iters<CX_MAX_ITER && rn>CX_TOL;iters++){
    const m=r.length;
    // finite-difference Jacobian J (m×nv)
    const J=[]; for(let i=0;i<m;i++) J.push(new Array(nv).fill(0));
    for(let j=0;j<nv;j++){
      const save=x[j]; x[j]=save+CX_FD;
      const rp=cxResidualVec(x,sys);
      x[j]=save;
      for(let i=0;i<m;i++) J[i][j]=(rp[i]-r[i])/CX_FD;
    }
    // normal equations: (JᵀJ + λI) dx = -Jᵀr
    const JTJ=[]; const JTr=new Array(nv).fill(0);
    for(let a=0;a<nv;a++){
      JTJ.push(new Array(nv).fill(0));
      for(let b=0;b<nv;b++){ let s=0; for(let i=0;i<m;i++) s+=J[i][a]*J[i][b]; JTJ[a][b]=s; }
      let s=0; for(let i=0;i<m;i++) s+=J[i][a]*r[i]; JTr[a]=s;
    }
    for(let a=0;a<nv;a++) JTJ[a][a]+=lambda*(1+JTJ[a][a]);
    const dx=cxSolveLinear(JTJ,JTr.map(v=>-v));
    if(!dx){ lambda*=10; continue; }
    const xn=x.map((v,i)=>v+dx[i]);
    const rnew=cxResidualVec(xn,sys), rnn=cxNorm(rnew);
    if(rnn<rn){ x=xn; r=rnew; rn=rnn; lambda=Math.max(lambda*0.5,1e-9); }   // accept, less damping
    else lambda=Math.min(lambda*10,1e9);                                    // reject, more damping
  }

  const ok=rn<=Math.max(CX_TOL*100,1e-4);
  if(ok||opt.force){
    // write the solved variables back into their objects, then rebuild geometry
    const dirty=new Set();
    sys.varkeys.forEach((vk,idx)=>{
      const obj=cxObj(vk.objId); if(!obj) return;
      obj.cpts[vk.i]=[x[idx*2],x[idx*2+1]];
      dirty.add(obj);
    });
    for(const obj of dirty) if(typeof sketchRebuildCurve==="function") sketchRebuildCurve(obj);
  }
  return {ok,iters,residual:rn,vars:nv};
}

/* ─── PUBLIC API ─── add a constraint, solve, and record history. `place` is an
   optional per-dimension offset descriptor (where the user dragged the dimension
   line out to). It is PURE RENDER METADATA — the solver never reads it — so it
   only records how constraintdraw.js should position the annotation. */
function addConstraint(type,entities,value,place){
  // measure how far the sketch is from satisfying its constraints BEFORE adding the
  // new one, so we can tell "merely under-constrained" (fine — keep it) from "made
  // the sketch worse / geometrically impossible" (roll back).
  const before=(()=>{ const sys=cxBuildSystem(); const nv=sys.varkeys.length*2;
    if(!CONSTRAINTS.length||nv===0) return 0;
    const x=new Array(nv); sys.varkeys.forEach((vk,i)=>{ const uv=cxReadUV(vk.objId,vk.i); x[i*2]=uv[0]; x[i*2+1]=uv[1]; });
    return cxNorm(cxResidualVec(x,sys)); })();

  const c={id:"cx"+(++_cxUid),type,entities:entities.slice()};
  if(CX_DIMENSIONAL.has(type)){ c.value=value; if(place) c.place=place; }
  CONSTRAINTS.push(c);
  // force-write the solved variables even when the system is under-determined (a
  // single dimension on a free edge leaves DOF and never fully "converges", but it
  // is a valid, drivable constraint — Fusion/SolidWorks keep it). Only roll back if
  // the residual grew, i.e. the constraint fights an existing one / is unreachable.
  const rep=solveSketch({force:true});
  if(!rep.ok && rep.residual>before+1e-4){
    CONSTRAINTS.pop();
    solveSketch({force:true});        // restore the prior solved state
    if(typeof toast==="function") toast("Can't satisfy "+(CX_LABEL[type]||type),"x");
    if(typeof vp3Draw==="function") vp3Draw();
    return null;
  }
  cxAddOutlinerNode(c);
  if(typeof pushHistory==="function") pushHistory("sketch",CX_LABEL[type]||type,"constraint added");
  if(typeof renderProps==="function") renderProps();
  if(typeof renderTree==="function") renderTree();
  if(typeof vp3Draw==="function") vp3Draw();
  return c;
}
/* change a dimensional constraint's target value, then re-solve. */
function setConstraintValue(id,value){
  const c=CONSTRAINTS.find(k=>k.id===id); if(!c||!CX_DIMENSIONAL.has(c.type)) return false;
  const prev=c.value; c.value=value;
  const rep=solveSketch({force:true});
  if(!rep.ok && rep.residual>Math.max(CX_TOL*100,1e-4)){ c.value=prev; solveSketch({force:true}); if(typeof toast==="function") toast("Value unreachable","x"); return false; }
  cxRefreshOutlinerNode(c);
  if(typeof pushHistory==="function") pushHistory("sketch",CX_LABEL[c.type]||c.type,"dimension = "+value);
  if(typeof renderTree==="function") renderTree();
  if(typeof vp3Draw==="function") vp3Draw();
  return true;
}
/* remove one constraint by id: drop it, prune its outliner row, re-solve + redraw. */
function removeConstraint(id){
  const i=CONSTRAINTS.findIndex(k=>k.id===id); if(i<0) return;
  const c=CONSTRAINTS[i];
  cxRemoveOutlinerNode(c);
  CONSTRAINTS.splice(i,1);
  solveSketch({force:true});
  if(typeof renderTree==="function") renderTree();
  if(typeof renderProps==="function") renderProps();
  if(typeof vp3Draw==="function") vp3Draw();
}
/* drop every constraint that references an object (called when it's deleted); prune
   each one's outliner row too so a deleted sketch leaves no orphan dimension rows. */
function removeConstraintsFor(objId){
  for(let i=CONSTRAINTS.length-1;i>=0;i--)
    if(CONSTRAINTS[i].entities.some(e=>e.objId===objId)){ cxRemoveOutlinerNode(CONSTRAINTS[i]); CONSTRAINTS.splice(i,1); }
}

/* ─── OUTLINER INTEGRATION ─── each constraint shows as a CHILD row under the sketch
   object it belongs to (like P2's Dimensions/Constraints sections, but nested). The
   node carries { kind:"cxnode", cxId } so browser.js routes its click/delete to the
   constraint, not to geometry. */
function cxOwnerNode(c){
  const obj=(c&&c.entities&&c.entities[0])?cxObj(c.entities[0].objId):null;
  if(!obj||!obj.nodeId||typeof findItem!=="function") return null;
  return findItem(obj.nodeId);
}
/* icon key + display label (dimensions carry their measured value, like P2). */
const CX_NODE_ICON={
  linear:"dimension", hdist:"dimension", vdist:"dimension", angle:"dimangle",
  radius:"dimradius", diameter:"dimdiameter",
  coincident:"coincident", horizontal:"horizontal", vertical:"vertical",
  parallel:"parallel", perpendicular:"perpendicular", equal:"equal",
  concentric:"concentric", tangent:"tangent", fix:"fix",
};
function cxNodeLabel(c){
  const name=CX_LABEL[c.type]||c.type;
  if(!CX_DIMENSIONAL.has(c.type)) return name;
  if(c.type==="angle") return name+"  "+(Math.round((c.value||0)*180/Math.PI*10)/10)+"°";
  if(c.type==="radius"||c.type==="diameter"){
    const o=cxObj(c.entities[0].objId);
    const val=(typeof sketchRadiusOf==="function"&&o)?sketchRadiusOf(o):(c.value||0);
    return (c.type==="radius"?"R":"⌀")+(Math.round(val*100)/100)+" mm";
  }
  return name+"  "+(Math.round((c.value||0)*100)/100)+" mm";
}
function cxAddOutlinerNode(c){
  const owner=cxOwnerNode(c); if(!owner||typeof item!=="function") return;
  const node=item(cxNodeLabel(c),CX_NODE_ICON[c.type]||"dimension",{kind:"cxnode",cxId:c.id,swatch:"#ffffff"});
  owner.children.push(node); c.nodeId=node.id;
}
function cxRefreshOutlinerNode(c){
  if(!c||!c.nodeId||typeof findItem!=="function") return;
  const n=findItem(c.nodeId); if(n) n.name=cxNodeLabel(c);
}
function cxRemoveOutlinerNode(c){
  if(!c||!c.nodeId||typeof CAD_TREE==="undefined") return;
  const rec=(list)=>{ const i=list.findIndex(x=>x.id===c.nodeId); if(i>=0){list.splice(i,1);return true;}
    return list.some(x=>x.children&&rec(x.children)); };
  rec(CAD_TREE); c.nodeId=null;
}
/* re-solve after the user dragged a point/edge: pin the just-moved anchors so the
   solver pulls the REST of the sketch to keep constraints, not the dragged handle.
   `pinKeys` = array of "objId#i". No-op when nothing is constrained. */
function solveAfterDrag(pinKeys){
  if(!CONSTRAINTS.length) return;
  solveSketch({pin:pinKeys});
}
