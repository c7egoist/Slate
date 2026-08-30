import * as THREE from "three";
import { mergeGeometries } from "three/examples/jsm/utils/BufferGeometryUtils.js";

function normalizeGeo(geo) {
  const g = geo.index ? geo.toNonIndexed() : geo.clone();
  const keep = new Set(["position", "normal", "uv"]);
  for (const name of Object.keys(g.attributes)) {
    if (!keep.has(name)) g.deleteAttribute(name);
  }
  if (!g.attributes.normal) g.computeVertexNormals();
  if (!g.attributes.uv) {
    const n = g.attributes.position.count;
    g.setAttribute("uv", new THREE.Float32BufferAttribute(new Float32Array(n * 2), 2));
  }
  return g;
}

function scaleBoxUVs(geo, w, h, d, uScale) {
  const uv = geo.attributes.uv;
  const faces = [
    [d, h],
    [d, h],
    [w, d],
    [w, d],
    [w, h],
    [w, h],
  ];
  for (let f = 0; f < 6; f++) {
    for (let i = 0; i < 4; i++) {
      const idx = f * 4 + i;
      uv.setX(idx, uv.getX(idx) * faces[f][0] * uScale);
      uv.setY(idx, uv.getY(idx) * faces[f][1] * uScale);
    }
  }
  uv.needsUpdate = true;
}

export class Builder {
  constructor() {
    this.buckets = new Map();
    this.loose = [];
  }

  _bucket(mat) {
    let arr = this.buckets.get(mat);
    if (!arr) {
      arr = [];
      this.buckets.set(mat, arr);
    }
    return arr;
  }

  box(mat, w, h, d, x, y, z, rx = 0, ry = 0, rz = 0, uvScale = 0.45) {
    const geo = new THREE.BoxGeometry(w, h, d);
    scaleBoxUVs(geo, w, h, d, uvScale);
    const m = new THREE.Matrix4();
    const q = new THREE.Quaternion().setFromEuler(new THREE.Euler(rx, ry, rz));
    m.compose(new THREE.Vector3(x, y, z), q, new THREE.Vector3(1, 1, 1));
    geo.applyMatrix4(m);
    this._bucket(mat).push(geo);
    return this;
  }

  cyl(mat, rTop, rBot, h, x, y, z, segs = 14, rx = 0, ry = 0, rz = 0) {
    const geo = new THREE.CylinderGeometry(rTop, rBot, h, segs);
    const m = new THREE.Matrix4();
    const q = new THREE.Quaternion().setFromEuler(new THREE.Euler(rx, ry, rz));
    m.compose(new THREE.Vector3(x, y, z), q, new THREE.Vector3(1, 1, 1));
    geo.applyMatrix4(m);
    this._bucket(mat).push(geo);
    return this;
  }

  torus(mat, r, tube, x, y, z, rx = 0, ry = 0, rz = 0, radial = 10, tubular = 16) {
    const geo = new THREE.TorusGeometry(r, tube, radial, tubular);
    const m = new THREE.Matrix4();
    const q = new THREE.Quaternion().setFromEuler(new THREE.Euler(rx, ry, rz));
    m.compose(new THREE.Vector3(x, y, z), q, new THREE.Vector3(1, 1, 1));
    geo.applyMatrix4(m);
    this._bucket(mat).push(geo);
    return this;
  }

  sphere(mat, r, x, y, z, seg = 10) {
    const geo = new THREE.SphereGeometry(r, seg, seg);
    geo.translate(x, y, z);
    this._bucket(mat).push(geo);
    return this;
  }

  shape(mat, shape, depth, x, y, z, rx = 0, ry = 0, rz = 0, uvScale = 0.45) {
    const geo = new THREE.ExtrudeGeometry(shape, {
      depth,
      bevelEnabled: false,
    });
    geo.translate(0, 0, -depth / 2);
    const uv = geo.attributes.uv;
    if (uv) {
      for (let i = 0; i < uv.count; i++) {
        uv.setX(i, uv.getX(i) * uvScale);
        uv.setY(i, uv.getY(i) * uvScale);
      }
    }
    const m = new THREE.Matrix4();
    const q = new THREE.Quaternion().setFromEuler(new THREE.Euler(rx, ry, rz));
    m.compose(new THREE.Vector3(x, y, z), q, new THREE.Vector3(1, 1, 1));
    geo.applyMatrix4(m);
    this._bucket(mat).push(geo);
    return this;
  }

  poly(mat, vertices, indices, uvs) {
    const geo = new THREE.BufferGeometry();
    geo.setAttribute("position", new THREE.Float32BufferAttribute(vertices, 3));
    if (uvs) geo.setAttribute("uv", new THREE.Float32BufferAttribute(uvs, 2));
    geo.setIndex(indices);
    geo.computeVertexNormals();
    this._bucket(mat).push(geo);
    return this;
  }

  add(object) {
    object.castShadow = true;
    object.receiveShadow = true;
    this.loose.push(object);
    return object;
  }

  build() {
    const group = new THREE.Group();
    for (const [mat, geoms] of this.buckets) {
      if (!geoms.length) continue;
      const normalized = geoms.map(normalizeGeo);
      geoms.forEach((g) => g.dispose());
      let merged = null;
      try {
        merged = mergeGeometries(normalized, false);
      } catch (err) {
        merged = null;
      }
      if (merged) {
        normalized.forEach((g) => g.dispose());
        merged.computeVertexNormals();
        const mesh = new THREE.Mesh(merged, mat);
        mesh.castShadow = true;
        mesh.receiveShadow = true;
        group.add(mesh);
      } else {
        for (const g of normalized) {
          const mesh = new THREE.Mesh(g, mat);
          mesh.castShadow = true;
          mesh.receiveShadow = true;
          group.add(mesh);
        }
      }
    }
    for (const o of this.loose) group.add(o);
    return group;
  }
}

export function disposeObject(root) {
  if (!root) return;
  root.traverse((obj) => {
    if (obj.geometry) obj.geometry.dispose();
    const mats = obj.material
      ? Array.isArray(obj.material)
        ? obj.material
        : [obj.material]
      : [];
    for (const m of mats) {
      if (!m) continue;
      m.dispose();
    }
  });
}

export function facingOf(name) {
  switch (name) {
    case "front":
      return { ry: 0, nx: 0, nz: 1 };
    case "back":
      return { ry: Math.PI, nx: 0, nz: -1 };
    case "left":
      return { ry: Math.PI / 2, nx: -1, nz: 0 };
    case "right":
      return { ry: -Math.PI / 2, nx: 1, nz: 0 };
    default:
      return { ry: 0, nx: 0, nz: 1 };
  }
}

export function onWall(face, along, y, width, depth) {
  const f = facingOf(face);
  let x = 0;
  let z = 0;
  if (face === "front") {
    x = along;
    z = depth / 2;
  } else if (face === "back") {
    x = along;
    z = -depth / 2;
  } else if (face === "left") {
    x = -width / 2;
    z = along;
  } else {
    x = width / 2;
    z = along;
  }
  return { x, y, z, ...f, face };
}
