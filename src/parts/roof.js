import * as THREE from "three";

function addFace(positions, uvs, indices, pts, uvScale = 0.45) {
  const origin = pts[0];
  const ax = pts[1][0] - origin[0];
  const ay = pts[1][1] - origin[1];
  const az = pts[1][2] - origin[2];
  const alen = Math.hypot(ax, ay, az) || 1;
  const ux = ax / alen;
  const uy = ay / alen;
  const uz = az / alen;
  const bx = pts[pts.length - 1][0] - origin[0];
  const by = pts[pts.length - 1][1] - origin[1];
  const bz = pts[pts.length - 1][2] - origin[2];
  const nx = uy * bz - uz * by;
  const ny = uz * bx - ux * bz;
  const nz = ux * by - uy * bx;
  const nlen = Math.hypot(nx, ny, nz) || 1;
  const vx = (ny * uz - nz * uy) / nlen;
  const vy = (nz * ux - nx * uz) / nlen;
  const vz = (nx * uy - ny * ux) / nlen;
  const base = positions.length / 3;
  for (const p of pts) {
    positions.push(p[0], p[1], p[2]);
    const dx = p[0] - origin[0];
    const dy = p[1] - origin[1];
    const dz = p[2] - origin[2];
    uvs.push((dx * ux + dy * uy + dz * uz) * uvScale, (dx * vx + dy * vy + dz * vz) * uvScale);
  }
  for (let i = 1; i < pts.length - 1; i++) {
    indices.push(base, base + i, base + i + 1);
  }
}

export function addRoof(b, mats, dims, style, params, rng) {
  const kind = params.roof === "style" ? style.roof : params.roof;
  const y = dims.totalH;
  const w = dims.width;
  const d = dims.depth;
  const over = kind === "flat" ? 0.15 : style.id === "mediterranean" ? 0.7 : 0.45;

  if (kind === "flat") addFlat(b, mats, w, d, y, style);
  else if (kind === "gable" || kind === "steep-gable") addGable(b, mats, w, d, y, over, kind === "steep-gable" ? 0.85 : 0.55, style);
  else if (kind === "mansard") addMansard(b, mats, w, d, y, style, params, rng);
  else addHip(b, mats, w, d, y, over, style.id === "mediterranean" ? 0.42 : 0.38, style);

  if (params.dormers && kind !== "flat") {
    if (kind === "mansard") addMansardDormers(b, mats, dims, style, params, rng, y);
    else if (kind === "gable" || kind === "steep-gable") addGableDormers(b, mats, dims, style, params, rng, y, over);
    else addHipDormers(b, mats, dims, style, params, rng, y, over);
  }

  if (params.dormers && style.chimney !== "none") addChimneys(b, mats, dims, style, rng, y, kind);
}

function addHip(b, mats, w, d, y, over, pitch, style) {
  const W = w + over * 2;
  const D = d + over * 2;
  const hw = W / 2;
  const hd = D / 2;
  const alongX = W >= D;
  const rise = (alongX ? D : W) * 0.5 * pitch * 2.1;
  const positions = [];
  const uvs = [];
  const indices = [];
  if (alongX) {
    const r = Math.max(0.25, (W - D) / 2);
    addFace(positions, uvs, indices, [[-hw, y, hd], [hw, y, hd], [r, y + rise, 0], [-r, y + rise, 0]]);
    addFace(positions, uvs, indices, [[hw, y, -hd], [-hw, y, -hd], [-r, y + rise, 0], [r, y + rise, 0]]);
    addFace(positions, uvs, indices, [[-hw, y, -hd], [-hw, y, hd], [-r, y + rise, 0]]);
    addFace(positions, uvs, indices, [[hw, y, hd], [hw, y, -hd], [r, y + rise, 0]]);
  } else {
    const r = Math.max(0.25, (D - W) / 2);
    addFace(positions, uvs, indices, [[-hw, y, hd], [hw, y, hd], [0, y + rise, r]]);
    addFace(positions, uvs, indices, [[hw, y, -hd], [-hw, y, -hd], [0, y + rise, -r]]);
    addFace(positions, uvs, indices, [[-hw, y, -hd], [-hw, y, hd], [0, y + rise, r], [0, y + rise, -r]]);
    addFace(positions, uvs, indices, [[hw, y, hd], [hw, y, -hd], [0, y + rise, -r], [0, y + rise, r]]);
  }
  b.poly(mats.roof, positions, indices, uvs);
  b.box(mats.trim, W + 0.04, 0.1, D + 0.04, 0, y + 0.02, 0);
  const ridge = alongX ? Math.max(0.6, W - D) : Math.max(0.6, D - W);
  if (alongX) b.box(mats.trim, ridge + 0.3, 0.08, 0.14, 0, y + rise + 0.02, 0);
  else b.box(mats.trim, 0.14, 0.08, ridge + 0.3, 0, y + rise + 0.02, 0);
  if (style.id === "mediterranean") addRafterTails(b, mats, w, d, y, over);
}

function addGable(b, mats, w, d, y, over, pitch, style) {
  const W = w + over * 2;
  const D = d + over * 2;
  const rise = (W / 2) * pitch;
  const positions = [];
  const uvs = [];
  const indices = [];
  const hw = W / 2;
  const hd = D / 2;
  addFace(positions, uvs, indices, [[-hw, y, -hd], [-hw, y, hd], [0, y + rise, hd], [0, y + rise, -hd]]);
  addFace(positions, uvs, indices, [[hw, y, hd], [hw, y, -hd], [0, y + rise, -hd], [0, y + rise, hd]]);
  b.poly(mats.roof, positions, indices, uvs);

  const gable = new THREE.Shape();
  gable.moveTo(-w / 2, 0);
  gable.lineTo(w / 2, 0);
  gable.lineTo(0, rise);
  gable.closePath();
  const wallMat = style.wallAlt ? mats.wallAlt : mats.wall;
  b.shape(wallMat, gable, 0.28, 0, y, d / 2 - 0.14, 0, 0, 0);
  b.shape(wallMat, gable, 0.28, 0, y, -d / 2 + 0.14, 0, 0, 0);
  const barge = Math.hypot(w / 2, rise);
  const ang = Math.atan2(rise, w / 2);
  b.box(mats.trim, 0.08, barge, 0.08, w / 4, y + rise / 2, d / 2 + 0.02, 0, 0, ang);
  b.box(mats.trim, 0.08, barge, 0.08, -w / 4, y + rise / 2, d / 2 + 0.02, 0, 0, -ang);
  b.box(mats.trim, 0.08, barge, 0.08, w / 4, y + rise / 2, -d / 2 - 0.02, 0, 0, ang);
  b.box(mats.trim, 0.08, barge, 0.08, -w / 4, y + rise / 2, -d / 2 - 0.02, 0, 0, -ang);
  b.box(mats.trim, 0.12, 0.1, D + 0.08, 0, y + rise + 0.02, 0);
}

function addMansard(b, mats, w, d, y, style, params, rng) {
  const over = 0.2;
  const W = w + over * 2;
  const D = d + over * 2;
  const lower = 2.15;
  const inset = 1.15;
  const positions = [];
  const uvs = [];
  const indices = [];
  const hw = W / 2;
  const hd = D / 2;
  const iw = hw - inset;
  const id = hd - inset;
  addFace(positions, uvs, indices, [[-hw, y, hd], [hw, y, hd], [iw, y + lower, id], [-iw, y + lower, id]]);
  addFace(positions, uvs, indices, [[hw, y, -hd], [-hw, y, -hd], [-iw, y + lower, -id], [iw, y + lower, -id]]);
  addFace(positions, uvs, indices, [[-hw, y, -hd], [-hw, y, hd], [-iw, y + lower, id], [-iw, y + lower, -id]]);
  addFace(positions, uvs, indices, [[hw, y, hd], [hw, y, -hd], [iw, y + lower, -id], [iw, y + lower, id]]);
  b.poly(mats.roof, positions, indices, uvs);

  const topH = 0.55;
  const positions2 = [];
  const uvs2 = [];
  const indices2 = [];
  addFace(positions2, uvs2, indices2, [[-iw, y + lower, id], [iw, y + lower, id], [iw * 0.2, y + lower + topH, 0], [-iw * 0.2, y + lower + topH, 0]]);
  addFace(positions2, uvs2, indices2, [[iw, y + lower, -id], [-iw, y + lower, -id], [-iw * 0.2, y + lower + topH, 0], [iw * 0.2, y + lower + topH, 0]]);
  addFace(positions2, uvs2, indices2, [[-iw, y + lower, -id], [-iw, y + lower, id], [-iw * 0.2, y + lower + topH, 0]]);
  addFace(positions2, uvs2, indices2, [[iw, y + lower, id], [iw, y + lower, -id], [iw * 0.2, y + lower + topH, 0]]);
  b.poly(mats.roof, positions2, indices2, uvs2);
  b.box(mats.trim, W + 0.1, 0.16, D + 0.1, 0, y + 0.05, 0);
}

function addFlat(b, mats, w, d, y, style) {
  const t = 0.22;
  b.box(mats.roof, w + 0.3, t, d + 0.3, 0, y + t / 2, 0);
  const p = 0.32;
  const ph = style.id === "brutalist" ? 0.9 : style.id === "deco" ? 1.15 : 0.55;
  b.box(mats.wall, w + 0.36, ph, p, 0, y + ph / 2, d / 2 + 0.05);
  b.box(mats.wall, w + 0.36, ph, p, 0, y + ph / 2, -d / 2 - 0.05);
  b.box(mats.wall, p, ph, d + 0.36, w / 2 + 0.05, y + ph / 2, 0);
  b.box(mats.wall, p, ph, d + 0.36, -w / 2 - 0.05, y + ph / 2, 0);
  if (style.id === "deco") {
    for (let i = 0; i < 5; i++) {
      const x = -w / 2 + ((i + 0.5) / 5) * w;
      b.box(mats.metal, 0.12, 0.35, 0.12, x, y + ph + 0.1, d / 2 + 0.05);
    }
  }
  if (style.id === "modernist") {
    b.box(mats.wall, 1.6, 0.9, 0.2, w / 2 - 1.2, y + 0.7, -d / 2 + 0.8);
  }
}

function addRafterTails(b, mats, w, d, y, over) {
  const n = Math.max(6, Math.round(w / 0.7));
  for (let i = 0; i < n; i++) {
    const x = -w / 2 + (i / (n - 1)) * w;
    b.box(mats.wood, 0.08, 0.1, over + 0.2, x, y - 0.04, d / 2 + over * 0.35);
    b.box(mats.wood, 0.08, 0.1, over + 0.2, x, y - 0.04, -d / 2 - over * 0.35);
  }
}

function addMansardDormers(b, mats, dims, style, params, rng, y) {
  const count = Math.max(2, dims.baysX - 1);
  for (let i = 0; i < count; i++) {
    const x = -dims.width / 2 + ((i + 1) / (count + 1)) * dims.width;
    const z = dims.depth / 2 - 0.15;
    const dw = 0.95;
    const dh = 1.35;
    const dd = 0.7;
    b.box(mats.wall, dw, dh, dd, x, y + 1.05, z + dd / 2 - 0.1);
    const g = new THREE.Shape();
    g.moveTo(-dw / 2, 0);
    g.lineTo(dw / 2, 0);
    g.lineTo(0, 0.45);
    g.closePath();
    b.shape(mats.wall, g, dd, x, y + 1.05 + dh / 2, z + dd / 2 - 0.1);
    const positions = [];
    const uvs = [];
    const indices = [];
    const yy = y + 1.05 + dh / 2;
    addFace(positions, uvs, indices, [[x - dw / 2 - 0.05, yy, z + dd], [x + dw / 2 + 0.05, yy, z + dd], [x, yy + 0.5, z + dd / 2]]);
    addFace(positions, uvs, indices, [[x + dw / 2 + 0.05, yy, z], [x - dw / 2 - 0.05, yy, z], [x, yy + 0.5, z + dd / 2]]);
    b.poly(mats.roof, positions, indices, uvs);
    b.box(mats.glass, 0.55, 0.9, 0.04, x, y + 1.0, z + dd - 0.05);
    b.box(mats.frame, 0.65, 1.0, 0.06, x, y + 1.0, z + dd - 0.03);
  }
}

function addGableDormers(b, mats, dims, style, params, rng, y, over) {
  const n = Math.min(3, Math.max(1, dims.baysZ - 1));
  for (let i = 0; i < n; i++) {
    const z = -dims.depth / 4 + (n === 1 ? 0 : (i / (n - 1) - 0.5) * dims.depth * 0.4);
    const x = dims.width * 0.22 * (i % 2 === 0 ? 1 : -0.2);
    addSmallDormer(b, mats, x, y, z + dims.depth / 2 * 0.15);
  }
}

function addHipDormers(b, mats, dims, style, params, rng, y, over) {
  const n = Math.min(dims.baysX - 2, 3);
  if (n < 1) return;
  for (let i = 0; i < n; i++) {
    const x = -dims.width * 0.28 + (n === 1 ? 0 : (i / (n - 1)) * dims.width * 0.56);
    addSmallDormer(b, mats, x, y, dims.depth / 2 - 0.35);
  }
}

function addSmallDormer(b, mats, x, y, z) {
  const dw = 0.9;
  const dh = 1.1;
  const dd = 0.85;
  b.box(mats.wall, dw, dh, dd, x, y + dh / 2 + 0.15, z);
  const g = new THREE.Shape();
  g.moveTo(-dw / 2, 0);
  g.lineTo(dw / 2, 0);
  g.lineTo(0, 0.4);
  g.closePath();
  b.shape(mats.wall, g, 0.2, x, y + dh + 0.15, z + dd / 2 - 0.1);
  const positions = [];
  const uvs = [];
  const indices = [];
  const yy = y + dh + 0.15;
  addFace(positions, uvs, indices, [[x - dw / 2 - 0.08, yy, z + dd / 2], [x + dw / 2 + 0.08, yy, z + dd / 2], [x, yy + 0.45, z]]);
  addFace(positions, uvs, indices, [[x + dw / 2 + 0.08, yy, z - dd / 2], [x - dw / 2 - 0.08, yy, z - dd / 2], [x, yy + 0.45, z]]);
  b.poly(mats.roof, positions, indices, uvs);
  b.box(mats.glass, 0.5, 0.7, 0.04, x, y + 0.7, z + dd / 2 - 0.02);
  b.box(mats.frame, 0.6, 0.8, 0.05, x, y + 0.7, z + dd / 2);
}

export function addChimneys(b, mats, dims, style, rng, y, kind) {
  const h = style.chimney === "cluster" ? 2.8 : style.chimney === "tall" ? 2.4 : 1.7;
  const w = style.chimney === "cluster" ? 0.85 : 0.55;
  const d = style.chimney === "cluster" ? 1.15 : 0.55;
  const positions = [];
  if (style.chimney === "pair") {
    positions.push([dims.width * 0.32, dims.depth * 0.18], [-dims.width * 0.32, -dims.depth * 0.18]);
  } else if (style.chimney === "cluster") {
    positions.push([dims.width * 0.28, -dims.depth * 0.12]);
  } else {
    positions.push([dims.width * 0.3 * rng.sign(), dims.depth * 0.15]);
  }
  const mat = style.base === "brick" || style.wall === "brick" ? mats.base : mats.wall;
  for (const [cx, cz] of positions) {
    const rise = kind === "flat" ? 0.2 : kind === "mansard" ? 2.4 : kind === "steep-gable" ? 2.2 : 1.6;
    b.box(mat, w, h + rise, d, cx, y + (h + rise) / 2, cz);
    b.box(mats.trim, w + 0.12, 0.1, d + 0.12, cx, y + h + rise - 0.08, cz);
    const pots = style.chimney === "cluster" ? 3 : 2;
    for (let i = 0; i < pots; i++) {
      const ox = (i - (pots - 1) / 2) * 0.22;
      b.cyl(mats.trim, 0.07, 0.08, 0.28, cx + ox, y + h + rise + 0.12, cz, 8);
    }
  }
}
