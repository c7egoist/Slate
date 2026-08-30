"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   part-geometry.js — a procedural CAD part for the 3D viewport. No mesh assets:
                      the "Housing" solid is assembled from primitives (a rounded
                      base block, a cylindrical boss, and a bored hole) into flat
                      triangle soup with per-triangle normals. Everything is
                      centred on the origin sitting on the y=0 ground plane so the
                      grid reads as the build plate.

                      PART = { tris:[ {p:[3 verts],n:[3]}... ], edges:[[a,b]...] }
                      where verts are world-space [x,y,z]. Edges are the sharp
                      boundary lines used for the wireframe overlay.
   ════════════════════════════════════════════════════════════════════════════ */

/* push one quad (a,b,c,d CCW) as two triangles with a shared face normal. */
function _quad(tris,a,b,c,d){
  const n=V3.normalize(V3.cross(V3.sub(b,a),V3.sub(c,a)));
  tris.push({p:[a,b,c],n},{p:[a,c,d],n});
}

/* a box from min→max corner; optional face filter (skip faces that get covered). */
function _box(tris,edges,mn,mx,skip={}){
  const[x0,y0,z0]=mn,[x1,y1,z1]=mx;
  const V=[[x0,y0,z0],[x1,y0,z0],[x1,y0,z1],[x0,y0,z1],   // bottom 0..3
           [x0,y1,z0],[x1,y1,z0],[x1,y1,z1],[x0,y1,z1]];  // top    4..7
  if(!skip.bottom) _quad(tris,V[0],V[3],V[2],V[1]);
  if(!skip.top)    _quad(tris,V[4],V[5],V[6],V[7]);
  if(!skip.front)  _quad(tris,V[0],V[1],V[5],V[4]);       // -z
  if(!skip.back)   _quad(tris,V[3],V[7],V[6],V[2]);       // +z
  if(!skip.left)   _quad(tris,V[0],V[4],V[7],V[3]);       // -x
  if(!skip.right)  _quad(tris,V[1],V[2],V[6],V[5]);       // +x
  // silhouette edges of the box
  const E=[[0,1],[1,2],[2,3],[3,0],[4,5],[5,6],[6,7],[7,4],[0,4],[1,5],[2,6],[3,7]];
  for(const[a,b]of E) edges.push([V[a],V[b]]);
}

/* a cylinder (or annular ring) about the +y axis, centred at cx,cz, from y0→y1.
   `seg` segments; if `rInner>0` it's a tube. Adds cap rings to the wireframe. */
function _cylinder(tris,edges,cx,cz,rOuter,rInner,y0,y1,seg){
  const ring=(r,y)=>Array.from({length:seg},(_,i)=>{
    const a=i/seg*Math.PI*2; return [cx+Math.cos(a)*r, y, cz+Math.sin(a)*r];
  });
  const oB=ring(rOuter,y0),oT=ring(rOuter,y1);
  for(let i=0;i<seg;i++){
    const j=(i+1)%seg;
    _quad(tris,oB[i],oB[j],oT[j],oT[i]);                  // outer wall
    if(rInner>0){
      // top annulus ring quad + bore wall (inner wall faces inward)
      const iB=ring(rInner,y0),iT=ring(rInner,y1);
      _quad(tris,oT[i],oT[j],iT[j],iT[i]);               // top ring
      _quad(tris,iB[j],iB[i],iT[i],iT[j]);               // inner wall (flipped)
    }else{
      // solid top cap fan
      const ctr=[cx,y1,cz];
      const n=[0,1,0];
      tris.push({p:[ctr,oT[i],oT[j]],n});
    }
  }
  // ring wireframe (top rim only — reads as the machined circle)
  for(let i=0;i<seg;i++){ const j=(i+1)%seg; edges.push([oT[i],oT[j]]); }
}

/* Empty scene: no demo geometry. The viewport shows just the ground grid and the
   orientation gizmo until real bodies are built. The primitive helpers above stay
   available for when geometry is added. */
const PART={tris:[],edges:[]};

/* ─── SKETCH GEOMETRY ───
   The drawn 2D sketches live here, separate from PART's shaded solids. Each entry
   is a plane-bound polygon the sketch module (sketch.js) builds, renders, and
   registers into the outliner / timeline / history. */
const SKETCHES=[];
let _skUid=0;
