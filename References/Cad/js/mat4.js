"use strict";
/* ════════════════════════════════════════════════════════════════════════════
   mat4.js — minimal column-major 4×4 matrix + vec3 helpers for the software 3D
             viewport. No dependencies. Matrices are Array[16], column-major
             (m[col*4+row]) to match the usual GL convention.
   ════════════════════════════════════════════════════════════════════════════ */
const M4={
  identity(){ return [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]; },
  multiply(a,b){
    const o=new Array(16);
    for(let c=0;c<4;c++){
      for(let r=0;r<4;r++){
        o[c*4+r]=a[0*4+r]*b[c*4+0]+a[1*4+r]*b[c*4+1]+a[2*4+r]*b[c*4+2]+a[3*4+r]*b[c*4+3];
      }
    }
    return o;
  },
  perspective(fovy,aspect,near,far){
    const f=1/Math.tan(fovy/2), nf=1/(near-far);
    return [f/aspect,0,0,0, 0,f,0,0, 0,0,(far+near)*nf,-1, 0,0,2*far*near*nf,0];
  },
  lookAt(eye,center,up){
    const z=V3.normalize(V3.sub(eye,center));
    const x=V3.normalize(V3.cross(up,z));
    const y=V3.cross(z,x);
    return [x[0],y[0],z[0],0, x[1],y[1],z[1],0, x[2],y[2],z[2],0,
            -V3.dot(x,eye),-V3.dot(y,eye),-V3.dot(z,eye),1];
  },
  transformPoint(m,p){
    return [m[0]*p[0]+m[4]*p[1]+m[8]*p[2]+m[12],
            m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+m[13],
            m[2]*p[0]+m[6]*p[1]+m[10]*p[2]+m[14],
            m[3]*p[0]+m[7]*p[1]+m[11]*p[2]+m[15]];
  },
  transformDir(m,v){
    return [m[0]*v[0]+m[4]*v[1]+m[8]*v[2],
            m[1]*v[0]+m[5]*v[1]+m[9]*v[2],
            m[2]*v[0]+m[6]*v[1]+m[10]*v[2]];
  },
  /* full column-major 4×4 inverse (cofactor method). Returns identity on a
     singular matrix so screen→world unproject degrades gracefully. */
  invert(m){
    const
      m00=m[0],m01=m[1],m02=m[2],m03=m[3],
      m10=m[4],m11=m[5],m12=m[6],m13=m[7],
      m20=m[8],m21=m[9],m22=m[10],m23=m[11],
      m30=m[12],m31=m[13],m32=m[14],m33=m[15];
    const
      b00=m00*m11-m01*m10, b01=m00*m12-m02*m10, b02=m00*m13-m03*m10,
      b03=m01*m12-m02*m11, b04=m01*m13-m03*m11, b05=m02*m13-m03*m12,
      b06=m20*m31-m21*m30, b07=m20*m32-m22*m30, b08=m20*m33-m23*m30,
      b09=m21*m32-m22*m31, b10=m21*m33-m23*m31, b11=m22*m33-m23*m32;
    let det=b00*b11-b01*b10+b02*b09+b03*b08-b04*b07+b05*b06;
    if(!det) return M4.identity();
    det=1/det;
    return [
      (m11*b11-m12*b10+m13*b09)*det, (m02*b10-m01*b11-m03*b09)*det, (m31*b05-m32*b04+m33*b03)*det, (m22*b04-m21*b05-m23*b03)*det,
      (m12*b08-m10*b11-m13*b07)*det, (m00*b11-m02*b08+m03*b07)*det, (m32*b02-m30*b05-m33*b01)*det, (m20*b05-m22*b02+m23*b01)*det,
      (m10*b10-m11*b08+m13*b06)*det, (m01*b08-m00*b10-m03*b06)*det, (m30*b04-m31*b02+m33*b00)*det, (m21*b02-m20*b04-m23*b00)*det,
      (m11*b07-m10*b09-m12*b06)*det, (m00*b09-m01*b07+m02*b06)*det, (m31*b01-m30*b03-m32*b00)*det, (m20*b03-m21*b01+m22*b00)*det];
  },
};
const V3={
  sub(a,b){ return [a[0]-b[0],a[1]-b[1],a[2]-b[2]]; },
  add(a,b){ return [a[0]+b[0],a[1]+b[1],a[2]+b[2]]; },
  scale(a,s){ return [a[0]*s,a[1]*s,a[2]*s]; },
  dot(a,b){ return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; },
  cross(a,b){ return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]; },
  length(a){ return Math.sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]); },
  normalize(a){ const l=V3.length(a)||1; return [a[0]/l,a[1]/l,a[2]/l]; },
};
