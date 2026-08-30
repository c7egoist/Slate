"use strict";
/* ==========================================================================
   mat4.js -- minimal column-major 4x4 matrix + vec3 helpers for the software
   3D viewport. No dependencies. All matrices are Float-array[16], column-major
   (m[col*4+row]) to match the usual GL convention.
   ========================================================================== */
const M4 = {
  identity() { return [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]; },

  multiply(a, b) {
    const o = new Array(16);
    for (let c = 0; c < 4; c++) {
      for (let r = 0; r < 4; r++) {
        o[c*4+r] = a[0*4+r]*b[c*4+0] + a[1*4+r]*b[c*4+1] +
                   a[2*4+r]*b[c*4+2] + a[3*4+r]*b[c*4+3];
      }
    }
    return o;
  },

  perspective(fovy, aspect, near, far) {
    const f = 1 / Math.tan(fovy / 2);
    const nf = 1 / (near - far);
    return [
      f/aspect, 0, 0, 0,
      0, f, 0, 0,
      0, 0, (far+near)*nf, -1,
      0, 0, 2*far*near*nf, 0,
    ];
  },

  lookAt(eye, center, up) {
    const z = V3.normalize(V3.sub(eye, center));
    const x = V3.normalize(V3.cross(up, z));
    const y = V3.cross(z, x);
    return [
      x[0], y[0], z[0], 0,
      x[1], y[1], z[1], 0,
      x[2], y[2], z[2], 0,
      -V3.dot(x, eye), -V3.dot(y, eye), -V3.dot(z, eye), 1,
    ];
  },

  // transform a point (w=1); returns [x,y,z,w] clip/eye coords
  transformPoint(m, p) {
    return [
      m[0]*p[0] + m[4]*p[1] + m[8]*p[2]  + m[12],
      m[1]*p[0] + m[5]*p[1] + m[9]*p[2]  + m[13],
      m[2]*p[0] + m[6]*p[1] + m[10]*p[2] + m[14],
      m[3]*p[0] + m[7]*p[1] + m[11]*p[2] + m[15],
    ];
  },

  // transform a direction (w=0), for normals with a rotation-only matrix
  transformDir(m, v) {
    return [
      m[0]*v[0] + m[4]*v[1] + m[8]*v[2],
      m[1]*v[0] + m[5]*v[1] + m[9]*v[2],
      m[2]*v[0] + m[6]*v[1] + m[10]*v[2],
    ];
  },
};

const V3 = {
  sub(a, b) { return [a[0]-b[0], a[1]-b[1], a[2]-b[2]]; },
  add(a, b) { return [a[0]+b[0], a[1]+b[1], a[2]+b[2]]; },
  scale(a, s) { return [a[0]*s, a[1]*s, a[2]*s]; },
  dot(a, b) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; },
  cross(a, b) {
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]];
  },
  length(a) { return Math.sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]); },
  normalize(a) {
    const l = V3.length(a) || 1;
    return [a[0]/l, a[1]/l, a[2]/l];
  },
};

if (typeof module !== "undefined") module.exports = { M4, V3 };
