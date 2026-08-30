"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   boolean2d.js — correct 2D polygon boolean operations (Martinez–Rueda–Feito
                  plane sweep) for sketch shapes: Union / Subtract / Intersect,
                  plus a non-merging Join.

     Operates purely in plane-space [u,v] rings. Unlike the reference P3.html
     (Greiner–Hormann with a whole-polygon +1e-9 perturbation hack that corrupts
     coordinates and still fails on shared/coincident edges), Martinez–Rueda
     handles coincident + overlapping edges, self-touching, holes, and winding
     NATIVELY inside the sweep — no vertex is ever moved. Epsilon is used only in
     comparisons.

     Public entry points:
       boolean2d(subjectRings, clipRings, op)   → {rings:[{uv,hole}]}
       boolean2dMany(baseRings, toolRingsList, op)
       boolean2dJoin(ringsList)                 (composite, non-merging)
     plus the sketch integration (sketchRunBoolean) at the bottom.

     A "ring" is an array of [u,v] points with NO repeated closing vertex.
   ════════════════════════════════════════════════════════════════════════════ */

const B2_EPS=1e-9;                                   // comparison tolerance ONLY — never moves points
const B2_OP={UNION:0, INTERSECTION:1, DIFFERENCE:2, XOR:3};

/* edge classification for overlapping (collinear) segments */
const B2_NORMAL=0, B2_NON_CONTRIBUTING=1, B2_SAME_TRANSITION=2, B2_DIFFERENT_TRANSITION=3;
const B2_SUBJECT=0, B2_CLIP=1;

/* ─── RING HELPERS ─── (none of these exist elsewhere in the prototype) */
/* signed area: >0 for counter-clockwise, <0 for clockwise (shoelace / 2). */
function b2SignedArea(ring){
  let s=0; const n=ring.length;
  for(let i=0;i<n;i++){ const a=ring[i], b=ring[(i+1)%n]; s+=a[0]*b[1]-b[0]*a[1]; }
  return s/2;
}
function b2RingArea(ring){ return Math.abs(b2SignedArea(ring)); }
function b2IsCW(ring){ return b2SignedArea(ring)<0; }
function b2EnsureCCW(ring){ return b2SignedArea(ring)<0?ring.slice().reverse():ring; }
function b2EnsureCW(ring){ return b2SignedArea(ring)>0?ring.slice().reverse():ring; }
/* even-odd point-in-ring test (ray cast to +u). */
function b2PointInRing(pt,ring){
  let inside=false; const n=ring.length, x=pt[0], y=pt[1];
  for(let i=0,j=n-1;i<n;j=i++){
    const xi=ring[i][0], yi=ring[i][1], xj=ring[j][0], yj=ring[j][1];
    if(((yi>y)!==(yj>y)) && (x < (xj-xi)*(y-yi)/((yj-yi)||B2_EPS)+xi)) inside=!inside;
  }
  return inside;
}

/* ─── POINT COMPARE ─── total order by (x then y) within epsilon. */
function b2cmpPt(p,q){
  if(p[0]-q[0] >  B2_EPS) return 1;
  if(q[0]-p[0] >  B2_EPS) return -1;
  if(p[1]-q[1] >  B2_EPS) return 1;
  if(q[1]-p[1] >  B2_EPS) return -1;
  return 0;
}
function b2ptEq(p,q){ return Math.abs(p[0]-q[0])<B2_EPS && Math.abs(p[1]-q[1])<B2_EPS; }
/* signed area of triangle (a,b,c): >0 if c is left of a→b. Orientation primitive
   the sweep comparators lean on for deterministic ordering. */
function b2sTri(a,b,c){ return (a[0]-c[0])*(b[1]-c[1])-(b[0]-c[0])*(a[1]-c[1]); }

/* ─── SWEEP EVENT ─── one per segment endpoint (segments are cross-linked). */
function b2Event(p,polyType){
  return { p, left:false, otherEvent:null, polyType,
           inOut:false, otherInOut:false, type:B2_NORMAL,
           inResult:false, prevInResult:null, resultInOut:false,
           contourId:0, pos:0, otherPos:-1 };
}
/* isBelow(p): is point p above this event's segment? (differs for left vs right
   endpoints, per canonical SweepEvent.isBelow). */
function b2isBelow(ev,p){
  const p0=ev.p, p1=ev.otherEvent.p;
  return ev.left
    ? (p0[0]-p[0])*(p1[1]-p[1])-(p1[0]-p[0])*(p0[1]-p[1]) > 0
    : (p1[0]-p[0])*(p0[1]-p[1])-(p0[0]-p[0])*(p1[1]-p[1]) > 0;
}
function b2isAbove(ev,p){ return !b2isBelow(ev,p); }
function b2isVertical(ev){ return ev.p[0]===ev.otherEvent.p[0]; }

/* ─── EVENT QUEUE ORDER ─── x asc, y asc; at a shared point a RIGHT endpoint
   sorts before a LEFT one (segments close before they open); collinear ties are
   broken by orientation so the order is deterministic (removes P3's need to
   perturb geometry). Returns true when e1 should be processed AFTER e2. */
function b2queueAfter(e1,e2){ return b2compareEvents(e1,e2)>0; }

/* binary min-heap of events (the "ordered set" JS lacks). ~O(n log n). */
function b2Queue(){ this.h=[]; }
b2Queue.prototype.size=function(){ return this.h.length; };
b2Queue.prototype.push=function(e){
  const h=this.h; h.push(e); let i=h.length-1;
  while(i>0){ const par=(i-1)>>1; if(b2queueAfter(h[par],h[i])){ [h[par],h[i]]=[h[i],h[par]]; i=par; } else break; }
};
b2Queue.prototype.pop=function(){
  const h=this.h; const top=h[0], last=h.pop();
  if(h.length){ h[0]=last; let i=0; const n=h.length;
    for(;;){ let l=2*i+1, r=2*i+2, sm=i;
      if(l<n && b2queueAfter(h[sm],h[l])) sm=l;
      if(r<n && b2queueAfter(h[sm],h[r])) sm=r;
      if(sm===i) break; [h[sm],h[i]]=[h[i],h[sm]]; i=sm; } }
  return top;
};

/* ─── SWEEP STATUS ─── segments crossing the sweep line, kept in a sorted array
   ordered vertically by b2statusAfter. Insert/remove by ELEMENT IDENTITY so two
   segments that compare equal never evict the wrong neighbour. */
/* canonical compareSegments: -1 if le1 sorts below le2 in the status, else 1 (0 eq). */
function b2compareSegments(le1,le2){
  if(le1===le2) return 0;
  if(b2sTri(le1.p,le1.otherEvent.p,le2.p)!==0 || b2sTri(le1.p,le1.otherEvent.p,le2.otherEvent.p)!==0){
    if(b2ptEq(le1.p,le2.p)) return b2isBelow(le1,le2.otherEvent.p)?-1:1;
    if(le1.p[0]===le2.p[0]) return le1.p[1]<le2.p[1]?-1:1;
    if(b2compareEvents(le1,le2)===1) return b2isAbove(le2,le1.p)?-1:1;
    return b2isBelow(le1,le2.p)?-1:1;
  }
  if(le1.polyType===le2.polyType){
    let p1=le1.p, p2=le2.p;
    if(p1[0]===p2[0]&&p1[1]===p2[1]){
      p1=le1.otherEvent.p; p2=le2.otherEvent.p;
      if(p1[0]===p2[0]&&p1[1]===p2[1]) return 0;
      return (le1.contourId||0)>(le2.contourId||0)?1:-1;
    }
  } else {
    return le1.polyType===B2_SUBJECT?-1:1;
  }
  return b2compareEvents(le1,le2)===1?1:-1;
}
function b2StatusInsert(status,le){
  let lo=0, hi=status.length;
  while(lo<hi){ const mid=(lo+hi)>>1; if(b2compareSegments(le,status[mid])>0) lo=mid+1; else hi=mid; }
  status.splice(lo,0,le); return lo;
}
function b2StatusIndexOf(status,le){ return status.indexOf(le); }   // identity search

/* ─── SEGMENT INTERSECTION ─── canonical Schneider–Eberly port. Returns {n,p0,p1}
   with n=0/1/2 (2 ⇒ collinear overlap sub-segment endpoints). No perturbation. */
function b2segInt(a1,a2,b1,b2){
  const va=[a2[0]-a1[0],a2[1]-a1[1]], vb=[b2[0]-b1[0],b2[1]-b1[1]];
  const e=[b1[0]-a1[0],b1[1]-a1[1]];
  const toP=(p,s,d)=>[p[0]+s*d[0], p[1]+s*d[1]];
  let kross=va[0]*vb[1]-va[1]*vb[0];
  const sqrLenA=va[0]*va[0]+va[1]*va[1];
  if(kross*kross>0){
    const s=(e[0]*vb[1]-e[1]*vb[0])/kross;
    if(s<0||s>1) return {n:0};
    const t=(e[0]*va[1]-e[1]*va[0])/kross;
    if(t<0||t>1) return {n:0};
    if(s===0||s===1) return {n:1, p0:toP(a1,s,va)};
    if(t===0||t===1) return {n:1, p0:toP(b1,t,vb)};
    return {n:1, p0:toP(a1,s,va)};
  }
  kross=e[0]*va[1]-e[1]*va[0];
  if(kross*kross>0) return {n:0};                           // parallel, not collinear
  const sa=(va[0]*e[0]+va[1]*e[1])/sqrLenA;
  const sb=sa+(va[0]*vb[0]+va[1]*vb[1])/sqrLenA;
  const smin=Math.min(sa,sb), smax=Math.max(sa,sb);
  if(smin<=1 && smax>=0){
    if(smin===1) return {n:1, p0:toP(a1,smin>0?smin:0,va)};
    if(smax===0) return {n:1, p0:toP(a1,smax<1?smax:1,va)};
    return {n:2, p0:toP(a1,smin>0?smin:0,va), p1:toP(a1,smax<1?smax:1,va)};
  }
  return {n:0};
}

/* -1/0/1 comparator on events (canonical compareEvents). */
function b2compareEvents(e1,e2){
  const p1=e1.p, p2=e2.p;
  if(p1[0]-p2[0] >  B2_EPS) return 1;
  if(p2[0]-p1[0] >  B2_EPS) return -1;
  if(Math.abs(p1[1]-p2[1])>B2_EPS) return p1[1]>p2[1]?1:-1;
  // same point
  if(e1.left!==e2.left) return e1.left?1:-1;              // right endpoint first
  if(b2sTri(p1,e1.otherEvent.p,e2.otherEvent.p)!==0) return (!b2isBelow(e1,e2.otherEvent.p))?1:-1;
  return (e1.polyType===B2_SUBJECT && e2.polyType===B2_CLIP)?1:-1;
}

/* split a segment (given event se) at p: canonical divideSegment (re-orders the
   left/right flags on a rounding edge case). */
function b2divide(se,p,queue){
  const r=b2Event([p[0],p[1]], se.polyType); r.left=false; r.otherEvent=se;
  const l=b2Event([p[0],p[1]], se.polyType); l.left=true;  l.otherEvent=se.otherEvent;
  if(b2compareEvents(l, se.otherEvent) > 0){ se.otherEvent.left=true; l.left=false; }
  se.otherEvent.otherEvent=l;
  se.otherEvent=r;
  queue.push(l); queue.push(r);
}

/* test + process a possible intersection between two left-events (canonical). */
function b2possibleIntersection(se1,se2,queue){
  const inter=b2segInt(se1.p, se1.otherEvent.p, se2.p, se2.otherEvent.p);
  const n=inter.n;
  if(n===0) return 0;
  if(n===1 && (b2ptEq(se1.p,se2.p) || b2ptEq(se1.otherEvent.p,se2.otherEvent.p))) return 0;
  if(n===2 && se1.polyType===se2.polyType) return 0;      // edges of same polygon overlap → ignore

  if(n===1){
    if(!b2ptEq(se1.p,inter.p0) && !b2ptEq(se1.otherEvent.p,inter.p0)) b2divide(se1,inter.p0,queue);
    if(!b2ptEq(se2.p,inter.p0) && !b2ptEq(se2.otherEvent.p,inter.p0)) b2divide(se2,inter.p0,queue);
    return 1;
  }

  // overlap (collinear). Order the four endpoints; mark same/different transition.
  const events=[];
  let leftCoincide=false, rightCoincide=false;
  if(b2ptEq(se1.p,se2.p)) leftCoincide=true;
  else if(b2compareEvents(se1,se2)===1) events.push(se2,se1); else events.push(se1,se2);
  if(b2ptEq(se1.otherEvent.p,se2.otherEvent.p)) rightCoincide=true;
  else if(b2compareEvents(se1.otherEvent,se2.otherEvent)===1) events.push(se2.otherEvent,se1.otherEvent);
  else events.push(se1.otherEvent,se2.otherEvent);

  if((leftCoincide&&rightCoincide) || leftCoincide){
    se2.type=B2_NON_CONTRIBUTING;
    se1.type=(se2.inOut===se1.inOut)?B2_SAME_TRANSITION:B2_DIFFERENT_TRANSITION;
    if(leftCoincide && !rightCoincide) b2divide(events[1].otherEvent, events[0].p, queue);
    return 2;
  }
  if(rightCoincide){ b2divide(events[0], events[1].p, queue); return 3; }
  if(events[0]!==events[3].otherEvent){
    b2divide(events[0], events[1].p, queue);
    b2divide(events[1], events[2].p, queue);
    return 3;
  }
  b2divide(events[0], events[1].p, queue);
  b2divide(events[3].otherEvent, events[2].p, queue);
  return 3;
}

/* canonical computeFields: set inOut/otherInOut/prevInResult, then inResult +
   resultTransition (the signed in/out transition used for hole nesting). */
function b2computeFields(le,prev){
  if(!prev){ le.inOut=false; le.otherInOut=true; }
  else if(le.polyType===prev.polyType){ le.inOut=!prev.inOut; le.otherInOut=prev.otherInOut; }
  else { le.inOut=!prev.otherInOut; le.otherInOut=b2isVertical(prev)?!prev.inOut:prev.inOut; }
  if(prev) le.prevInResult=(!b2inResult(prev)||prev.type!==B2_NORMAL)?prev.prevInResult:prev;
  le.inResult=b2inResult(le);
  le.resultTransition=le.inResult?b2resultTransition(le):0;
}
function b2resultTransition(le){
  const thisIn=!le.inOut, thatIn=!le.otherInOut; let isIn=false;
  switch(b2_OP){
    case B2_OP.INTERSECTION: isIn=thisIn&&thatIn; break;
    case B2_OP.UNION:        isIn=thisIn||thatIn; break;
    case B2_OP.XOR:          isIn=thisIn!==thatIn; break;
    case B2_OP.DIFFERENCE:   isIn=(le.polyType===B2_SUBJECT)?(thisIn&&!thatIn):(thatIn&&!thisIn); break;
  }
  return isIn?1:-1;
}
/* per-operation edge selection (canonical Martinez inResult table). */
function b2inResult(le){
  switch(le.type){
    case B2_NORMAL:
      switch(b2_OP){
        case B2_OP.INTERSECTION: return !le.otherInOut;
        case B2_OP.UNION:        return le.otherInOut;
        case B2_OP.DIFFERENCE:   return (le.polyType===B2_SUBJECT)?le.otherInOut:!le.otherInOut;
        case B2_OP.XOR:          return true;
      }
      return false;
    case B2_SAME_TRANSITION:      return b2_OP===B2_OP.INTERSECTION || b2_OP===B2_OP.UNION;
    case B2_DIFFERENT_TRANSITION: return b2_OP===B2_OP.DIFFERENCE;
    case B2_NON_CONTRIBUTING:     return false;
  }
  return false;
}
let b2_OP=B2_OP.UNION;   // current op, read by b2inResult during a run

/* ─── CONNECT RESULT EDGES INTO RINGS ─── build an undirected multigraph on the
   endpoints of the selected result segments, then walk closed loops. Each result
   segment is a left-event with inResult=true, spanning le.p → le.otherEvent.p.
   Robust to shared vertices (multiple segments meeting at a point) because each
   segment is consumed exactly once. */
/* canonical orderEvents: keep contributing result events, sort, link otherPos. */
function b2orderEvents(sortedEvents){
  const resultEvents=[];
  for(const e of sortedEvents){
    if((e.left&&e.inResult) || (!e.left&&e.otherEvent.inResult)) resultEvents.push(e);
  }
  // bubble to a stable full order (overlaps can leave it slightly unsorted)
  let sorted=false;
  while(!sorted){ sorted=true;
    for(let i=0;i+1<resultEvents.length;i++){
      if(b2compareEvents(resultEvents[i],resultEvents[i+1])===1){
        const t=resultEvents[i]; resultEvents[i]=resultEvents[i+1]; resultEvents[i+1]=t; sorted=false;
      }
    }
  }
  for(let i=0;i<resultEvents.length;i++) resultEvents[i].otherPos=i;
  for(let i=0;i<resultEvents.length;i++){
    const e=resultEvents[i];
    if(!e.left){ const t=e.otherPos; e.otherPos=e.otherEvent.otherPos; e.otherEvent.otherPos=t; }
  }
  return resultEvents;
}
function b2nextPos(pos,resultEvents,processed,origPos){
  let newPos=pos+1; const len=resultEvents.length; const p=resultEvents[pos].p;
  let p1=(newPos<len)?resultEvents[newPos].p:null;
  while(newPos<len && p1 && b2ptEq(p1,p)){
    if(!processed[newPos]) return newPos;
    newPos++; if(newPos<len) p1=resultEvents[newPos].p;
  }
  newPos=pos-1;
  while(newPos>origPos && processed[newPos]) newPos--;
  return newPos;
}
/* canonical connectEdges → contours with hole nesting via prevInResult / resultTransition. */
function b2connectEdges(sortedEvents){
  const resultEvents=b2orderEvents(sortedEvents);
  const processed={}; const contours=[];
  for(let i=0;i<resultEvents.length;i++){
    if(processed[i]) continue;
    const contourId=contours.length;
    const contour={points:[], holeOf:null, holeIds:[], depth:0};
    // hole nesting from the segment below (prevInResult)
    const ev=resultEvents[i];
    if(ev.prevInResult!=null){
      const lower=ev.prevInResult, lowerId=lower.outputContourId, lowerTrans=lower.resultTransition;
      if(lowerTrans>0){
        const lowerContour=contours[lowerId];
        if(lowerContour.holeOf!=null){
          contours[lowerContour.holeOf].holeIds.push(contourId);
          contour.holeOf=lowerContour.holeOf; contour.depth=contours[lowerId].depth;
        } else {
          contours[lowerId].holeIds.push(contourId);
          contour.holeOf=lowerId; contour.depth=contours[lowerId].depth+1;
        }
      } else { contour.holeOf=null; contour.depth=contours[lowerId].depth; }
    }
    let pos=i; const origPos=i;
    const mark=(pp)=>{ processed[pp]=true; if(resultEvents[pp]) resultEvents[pp].outputContourId=contourId; };
    contour.points.push([resultEvents[i].p[0],resultEvents[i].p[1]]);
    for(;;){
      mark(pos);
      pos=resultEvents[pos].otherPos;
      mark(pos);
      contour.points.push([resultEvents[pos].p[0],resultEvents[pos].p[1]]);
      pos=b2nextPos(pos,resultEvents,processed,origPos);
      if(pos===origPos || pos>=resultEvents.length || !resultEvents[pos]) break;
    }
    contours.push(contour);
  }
  return contours;
}
function b2dedupRing(ring){
  const out=[];
  for(const p of ring){ if(!out.length || !b2ptEq(out[out.length-1],p)) out.push(p); }
  if(out.length>1 && b2ptEq(out[0],out[out.length-1])) out.pop();
  return out;
}

/* classify each raw result ring as outer/hole by nesting depth, orient winding. */
function b2classifyRings(rawRings){
  const rings=rawRings.filter(r=>r.length>=3 && b2RingArea(r)>1e-9);
  const out=[];
  rings.forEach((r)=>{
    let depth=0; const test=r[0];
    rings.forEach((o)=>{ if(o!==r && b2PointInRing(test,o)) depth++; });
    const hole=(depth%2===1);
    out.push({ uv: hole?b2EnsureCW(r):b2EnsureCCW(r), hole });
  });
  out.sort((a,b)=> b2RingArea(b.uv)-b2RingArea(a.uv));   // outers (larger) first
  return out;
}

/* ─── PUBLIC: single boolean of two ring-sets ─── */
function boolean2d(subjectRings, clipRings, op){
  b2_OP=op;
  const subj=(subjectRings||[]).map(b2dedupRing).filter(r=>r.length>=3);
  const clip=(clipRings||[]).map(b2dedupRing).filter(r=>r.length>=3);
  // trivial empties
  if(!subj.length && !clip.length) return {rings:[]};
  if(!clip.length){ if(op===B2_OP.INTERSECTION) return {rings:[]}; return {rings:b2classifyRings(subj)}; }
  if(!subj.length){ if(op===B2_OP.UNION||op===B2_OP.XOR) return {rings:b2classifyRings(clip)}; return {rings:[]}; }

  const queue=new b2Queue();
  const addRing=(ring,polyType)=>{
    const n=ring.length;
    for(let i=0;i<n;i++){
      const a=ring[i], b=ring[(i+1)%n];
      if(b2ptEq(a,b)) continue;
      const e1=b2Event([a[0],a[1]],polyType), e2=b2Event([b[0],b[1]],polyType);
      e1.otherEvent=e2; e2.otherEvent=e1;
      if(b2compareEvents(e1,e2)>0){ e2.left=true; } else { e1.left=true; }
      queue.push(e1); queue.push(e2);
    }
  };
  subj.forEach(r=>addRing(r,B2_SUBJECT));
  clip.forEach(r=>addRing(r,B2_CLIP));

  const status=[];               // sweep-line status (sorted array, identity-keyed)
  const sortedEvents=[];         // all processed events, in sweep order
  let guard=0, GMAX=1e6;
  while(queue.size()){
    if(++guard>GMAX) break;      // runaway backstop
    const e=queue.pop();
    sortedEvents.push(e);
    if(e.left){
      const i=b2StatusInsert(status,e);
      const prev=(i>0)?status[i-1]:null;
      const next=(i+1<status.length)?status[i+1]:null;
      b2computeFields(e,prev);
      if(next && b2possibleIntersection(e,next,queue)===2){ b2computeFields(e,prev); b2computeFields(next,e); }
      if(prev && b2possibleIntersection(prev,e,queue)===2){
        const pi=b2StatusIndexOf(status,prev);
        b2computeFields(prev, (pi>0)?status[pi-1]:null);
        b2computeFields(e,prev);
      }
    } else {
      const le=e.otherEvent;
      const i=b2StatusIndexOf(status,le);
      if(i<0) continue;
      const prev=(i>0)?status[i-1]:null;
      const next=(i+1<status.length)?status[i+1]:null;
      status.splice(i,1);
      if(prev&&next) b2possibleIntersection(prev,next,queue);
    }
  }
  const contours=b2connectEdges(sortedEvents);
  return {rings:b2contoursToRings(contours)};
}
/* contours (with holeOf/depth) → oriented {uv,hole} rings. */
function b2contoursToRings(contours){
  const out=[];
  for(const c of contours){
    const ring=b2dedupRing(c.points);
    if(ring.length<3 || b2RingArea(ring)<1e-9) continue;
    const hole=(c.holeOf!=null) || (c.depth%2===1);
    out.push({ uv: hole?b2EnsureCW(ring):b2EnsureCCW(ring), hole });
  }
  out.sort((a,b)=> b2RingArea(b.uv)-b2RingArea(a.uv));
  return out;
}

/* fold N tools into a base. */
function boolean2dMany(baseRings, toolRingsList, op){
  let acc=(baseRings||[]).slice();
  for(const tool of (toolRingsList||[])){
    const r=boolean2d(acc, tool, op);
    acc=r.rings.map(x=>x.uv);
  }
  return {rings:b2classifyRings(acc)};
}

/* JOIN — non-merging composite: gather every ring into one region, dedupe exact
   duplicates, resolve nesting/winding. Touching / overlapping outlines are NOT
   dissolved (that is what Union is for) — a ring fully inside another becomes a
   hole. Gives Join independent value (disjoint profiles as one region, or an
   annulus from concentric circles). */
function boolean2dJoin(ringsList){
  const rings=[];
  for(const r of (ringsList||[])){ const dd=b2dedupRing(r); if(dd.length>=3&&b2RingArea(dd)>1e-9) rings.push(dd); }
  // drop exact-equal duplicate rings
  const uniq=[];
  for(const r of rings){
    if(!uniq.some(u=>u.length===r.length && u.every((p,k)=>b2ptEq(p,r[k])))) uniq.push(r);
  }
  return {rings:b2classifyRings(uniq)};
}

/* ════════════════════════════════════════════════════════════════════════════
   SKETCH INTEGRATION — extract rings, build/register a Region, drive from a tool
   ════════════════════════════════════════════════════════════════════════════ */

/* a closed sketch shape → one plane-space [u,v] ring on the base plane. Curves /
   circles are already densified into obj.points, so they arrive as polylines. */
function sketchShapeToRings(obj, basePlane){
  const world=(typeof sketchApplyTransform==="function")?sketchApplyTransform(obj):obj.points;
  const ring=world.map(p=>sketchPlaneCoords(basePlane,p));
  if(ring.length>1 && b2ptEq(ring[0],ring[ring.length-1])) ring.pop();   // drop closing dup
  return ring;
}
/* a shape (region → all its rings; else its single ring). */
function sketchGatherRings(obj, basePlane){
  if(obj.category==="region"&&obj.rings) return obj.rings.map(r=>r.uv.slice());
  return [sketchShapeToRings(obj, basePlane)];
}
/* is obj coplanar with the base plane? (reject rather than silently project). */
function sketchCoplanarWithBase(obj, basePlane){
  const na=obj.plane.normal, nb=basePlane.normal;
  const la=Math.hypot(na[0],na[1],na[2])||1, lb=Math.hypot(nb[0],nb[1],nb[2])||1;
  const dot=(na[0]*nb[0]+na[1]*nb[1]+na[2]*nb[2])/(la*lb);
  if(Math.abs(dot)<0.9999) return false;
  // centroid distance to the base plane along its normal
  const c=(typeof sketchCentroid==="function")?sketchCentroid(obj.points):obj.points[0];
  const d=[c[0]-basePlane.point[0], c[1]-basePlane.point[1], c[2]-basePlane.point[2]];
  const dist=Math.abs((d[0]*nb[0]+d[1]*nb[1]+d[2]*nb[2])/lb);
  return dist<1e-4;
}

/* build a Region object from oriented rings (multi-ring; identity transform). */
let _b2RegionCount=0;
function sketchMakeRegion(rings, basePlane, opId){
  const id="sk"+(++_skUid);
  const plane={ point:basePlane.point.slice(), normal:basePlane.normal.slice(), name:basePlane.name };
  // world points of the outer ring(s) for legacy centroid / pick / transform
  const outer=rings.filter(r=>!r.hole);
  const pts=[];
  (outer[0]?outer[0].uv:(rings[0]?rings[0].uv:[])).forEach(uv=>pts.push(sketchPlanePoint(plane,uv[0],uv[1])));
  return {
    id, nodeId:null, name:"Region "+(++_b2RegionCount),
    category:"region", plane,
    rings: rings.map(r=>({uv:r.uv.map(p=>p.slice()), hole:!!r.hole})),
    points: pts, closed:true,
    transform:{t:[0,0,0],r:[0,0,0],s:[1,1,1]},
    colors:{ point:(typeof SK_COLORS!=="undefined"?SK_COLORS.point:"#fff"), line:"#f59e0b", fill:"#f59e0b" },
    _boolOp:opId
  };
}
/* outliner + timeline + history for a region (parallel to sketchRegister; region
   has no cpts and a distinct category/icon). */
function sketchRegisterRegion(obj){
  if(typeof item==="function" && typeof CAD_TREE!=="undefined"){
    const folder=flat(CAD_TREE).find(n=>n.name==="Sketches");
    const node=item(obj.name,"polygon",{kind:"sketch",swatch:obj.colors.fill,plane:obj.plane.name,skId:obj.id,category:"region"});
    obj.nodeId=node.id;
    if(folder){ folder.collapsed=false; folder.children.push(node); }
  }
  if(typeof CAD_TIMELINE!=="undefined"&&typeof cadFeatInstance==="function"){
    const inst=cadFeatInstance("sketch",obj.name); inst.skId=obj.id;
    const at=Math.min(typeof cadRollback!=="undefined"?cadRollback:CAD_TIMELINE.length,CAD_TIMELINE.length);
    CAD_TIMELINE.splice(at,0,inst); if(typeof cadRollback!=="undefined") cadRollback=at+1;
    if(typeof cadRightPane!=="undefined"&&cadRightPane==="features"&&typeof renderCadFeatures==="function") renderCadFeatures();
  }
  if(typeof pushHistory==="function") pushHistory("boolean",obj.name,(obj._boolOp||"boolean")+" · "+obj.rings.length+" rings");
  if(typeof renderTree==="function") renderTree();
  if(typeof selectItem==="function") selectItem(obj.nodeId);
  if(typeof syncBrowserFooter==="function") syncBrowserFooter();
  if(typeof toast==="function") toast("Created "+obj.name,"check");
  if(typeof vp3Draw==="function") vp3Draw();
}

/* ─── DRIVER ─── fired from the Boolean toolbar group; one-shot on the current
   ordered selection (selectionOrder[0] = base). Inputs are kept (non-destructive). */
function sketchRunBoolean(opId){
  if(typeof selectionOrder==="undefined"){ if(typeof toast==="function") toast("Select 2+ shapes","alert"); return null; }
  const objs=selectionOrder
    .map(nid=>{ const n=(typeof findItem==="function")?findItem(nid):null; return (n&&n.skId&&typeof SKETCHES!=="undefined")?SKETCHES.find(s=>s.id===n.skId):null; })
    .filter(Boolean);
  if(objs.length<2){ if(typeof toast==="function") toast("Select 2+ shapes (base first)","alert"); return null; }
  const base=objs[0], basePlane=base.plane;
  for(const o of objs){ if(!sketchCoplanarWithBase(o,basePlane)){ if(typeof toast==="function") toast(o.name+" not coplanar","alert"); return null; } }
  const baseRings=sketchGatherRings(base,basePlane);
  const toolRingsList=objs.slice(1).map(o=>sketchGatherRings(o,basePlane));
  let result;
  if(opId==="union")          result=boolean2dMany(baseRings,toolRingsList,B2_OP.UNION);
  else if(opId==="subtract")  result=boolean2dMany(baseRings,toolRingsList,B2_OP.DIFFERENCE);
  else if(opId==="intersect") result=boolean2dMany(baseRings,toolRingsList,B2_OP.INTERSECTION);
  else /* join */             result=boolean2dJoin([].concat(baseRings, ...toolRingsList));
  if(!result.rings.length){ if(typeof toast==="function") toast("Empty result","alert"); return null; }
  const region=sketchMakeRegion(result.rings,basePlane,opId);
  SKETCHES.push(region);
  sketchRegisterRegion(region);
  return region;
}
