import * as THREE from "three";

function offset(p, dist) {
  return {
    x: p.x + p.nx * dist,
    y: p.y,
    z: p.z + p.nz * dist,
    ry: p.ry,
    nx: p.nx,
    nz: p.nz,
  };
}

export function addWindow(b, mats, pos, w, h, style, params, rng, opts = {}) {
  const type = opts.type || style.window;
  const reveal = opts.reveal ?? style.reveal;
  const lit = opts.lit ?? rng.chance(0.55);
  const glass = lit ? mats.glassLit : mats.glass;
  const interior = lit ? mats.interiorLit : mats.interior;
  const frame = mats.frame;
  const sillD = Math.max(0.08, reveal * 0.5);
  const outer = offset(pos, 0.02);
  const inner = offset(pos, -reveal);
  const glassP = offset(pos, -reveal * 0.65);

  b.box(mats.dark, w + 0.04, h + 0.04, reveal + 0.02, inner.x, pos.y, inner.z, 0, pos.ry, 0, 0.2);
  b.box(interior, w * 0.92, h * 0.92, 0.02, offset(pos, -reveal - 0.04).x, pos.y, offset(pos, -reveal - 0.04).z, 0, pos.ry, 0, 0.3);

  b.box(mats.trim, w + 0.16, 0.07, sillD + 0.1, outer.x, pos.y - h / 2 - 0.02, outer.z + 0, 0, pos.ry, 0, 0.8);
  const sill = offset(pos, 0.06);
  b.box(mats.trim, w + 0.18, 0.06, 0.14, sill.x, pos.y - h / 2 - 0.03, sill.z, 0, pos.ry, 0);
  const lintel = offset(pos, 0.05);
  b.box(mats.trim, w + 0.2, 0.09, 0.12, lintel.x, pos.y + h / 2 + 0.05, lintel.z, 0, pos.ry, 0);

  if (type === "arched" || opts.arched) {
    addArchedWindow(b, mats, pos, w, h, glass, frame, reveal);
  } else if (type === "ribbon") {
    addRibbon(b, glass, frame, pos, w, h, glassP);
  } else if (type === "deep") {
    addDeep(b, glass, frame, pos, w, h, reveal, glassP);
  } else if (type === "metal") {
    addMetal(b, glass, frame, pos, w, h, glassP, params.muntins);
  } else if (type === "casement") {
    addCasement(b, glass, frame, pos, w, h, glassP, params.muntins);
  } else if (type === "french") {
    addFrench(b, glass, frame, pos, w, h, glassP, params.muntins);
  } else {
    addSash(b, glass, frame, pos, w, h, glassP, type === "sash2" ? 2 : 3, params.muntins);
  }

  if (style.shutters && !opts.noShutters && type !== "ribbon") addShutters(b, mats, pos, w, h, rng);
  if (opts.keystone) {
    const k = offset(pos, 0.04);
    b.box(mats.stone, 0.16, 0.22, 0.1, k.x, pos.y + h / 2 + 0.1, k.z, 0, pos.ry, 0);
  }
}

function addSash(b, glass, frame, pos, w, h, glassP, cols, muntins) {
  const t = 0.045;
  frameRect(b, frame, pos, w, h, t, 0.05);
  b.box(glass, w - t * 2, h - t * 2, 0.02, glassP.x, pos.y, glassP.z, 0, pos.ry, 0);
  const meet = 0.05;
  b.box(frame, w - t, meet, 0.04, pos.x + pos.nx * 0.04, pos.y, pos.z + pos.nz * 0.04, 0, pos.ry, 0);
  const rowsEach = Math.max(1, muntins);
  muntinGrid(b, frame, pos, w - t * 2, (h - t * 2) / 2, cols, rowsEach, 0, h / 4, 0.04);
  muntinGrid(b, frame, pos, w - t * 2, (h - t * 2) / 2, cols, rowsEach, 0, -h / 4, 0.04);
}

function addFrench(b, glass, frame, pos, w, h, glassP, muntins) {
  const t = 0.05;
  frameRect(b, frame, pos, w, h, t, 0.05);
  b.box(glass, w - t * 2, h - t * 2, 0.02, glassP.x, pos.y, glassP.z, 0, pos.ry, 0);
  b.box(frame, 0.05, h - t * 2, 0.04, pos.x + pos.nx * 0.04, pos.y, pos.z + pos.nz * 0.04, 0, pos.ry, 0);
  const cols = 2;
  const rows = Math.max(3, muntins + 2);
  muntinGrid(b, frame, pos, w - t * 2, h - t * 2, cols, rows, 0, 0, 0.04);
}

function addCasement(b, glass, frame, pos, w, h, glassP, muntins) {
  const t = 0.04;
  frameRect(b, frame, pos, w, h, t, 0.04);
  b.box(glass, w - t * 2, h - t * 2, 0.02, glassP.x, pos.y, glassP.z, 0, pos.ry, 0);
  const cols = Math.max(2, muntins);
  const rows = Math.max(2, muntins);
  muntinGrid(b, frame, pos, w - t * 2, h - t * 2, cols, rows, 0, 0, 0.035);
}

function addRibbon(b, glass, frame, pos, w, h, glassP) {
  const t = 0.04;
  frameRect(b, frame, pos, w, h, t, 0.04);
  b.box(glass, w - t * 2, h - t * 2, 0.02, glassP.x, pos.y, glassP.z, 0, pos.ry, 0);
  const n = Math.max(2, Math.round(w / 0.7));
  for (let i = 1; i < n; i++) {
    const along = -w / 2 + (i / n) * w;
    const x = pos.x + Math.cos(pos.ry) * along;
    const z = pos.z + -Math.sin(pos.ry) * along;
    b.box(frame, 0.04, h - t, 0.04, x + pos.nx * 0.03, pos.y, z + pos.nz * 0.03, 0, pos.ry, 0);
  }
}

function addMetal(b, glass, frame, pos, w, h, glassP, muntins) {
  const t = 0.035;
  frameRect(b, frame, pos, w, h, t, 0.04);
  b.box(glass, w - t * 2, h - t * 2, 0.02, glassP.x, pos.y, glassP.z, 0, pos.ry, 0);
  muntinGrid(b, frame, pos, w - t * 2, h - t * 2, 2, Math.max(3, muntins + 1), 0, 0, 0.03);
}

function addDeep(b, glass, frame, pos, w, h, reveal, glassP) {
  b.box(glass, w * 0.9, h * 0.9, 0.02, glassP.x, pos.y, glassP.z, 0, pos.ry, 0);
  frameRect(b, frame, offset(pos, -reveal * 0.3), w * 0.92, h * 0.92, 0.04, 0.03);
}

function addArchedWindow(b, mats, pos, w, h, glass, frame, reveal) {
  const t = 0.05;
  const rectH = Math.max(h * 0.58, h - w / 2);
  const archR = Math.min(w / 2, h * 0.45);
  const glassP = offset(pos, -reveal * 0.65);
  b.box(glass, w - t * 2, rectH, 0.02, glassP.x, pos.y - h / 2 + rectH / 2, glassP.z, 0, pos.ry, 0);
  frameRect(b, frame, { ...pos, y: pos.y - h / 2 + rectH / 2 }, w, rectH, t, 0.05);

  const archY = pos.y - h / 2 + rectH;
  const glassArch = new THREE.Shape();
  glassArch.moveTo(-archR + 0.02, 0);
  glassArch.absarc(0, 0, archR - 0.02, Math.PI, 0, true);
  glassArch.closePath();
  b.shape(glass, glassArch, 0.02, glassP.x, archY, glassP.z, 0, pos.ry, 0);

  const segs = 8;
  for (let i = 0; i <= segs; i++) {
    const a = Math.PI * (i / segs);
    const along = Math.cos(a) * archR;
    const yy = archY + Math.sin(a) * archR;
    const x = pos.x + Math.cos(pos.ry) * along + pos.nx * 0.04;
    const z = pos.z - Math.sin(pos.ry) * along + pos.nz * 0.04;
    b.box(frame, 0.08, 0.08, 0.06, x, yy, z, 0, pos.ry, 0);
  }
  b.box(frame, 0.045, rectH, 0.04, pos.x + pos.nx * 0.04, pos.y - h / 2 + rectH / 2, pos.z + pos.nz * 0.04, 0, pos.ry, 0);
}

function frameRect(b, mat, pos, w, h, t, d) {
  const o = 0.03;
  b.box(mat, w, t, d, pos.x + pos.nx * o, pos.y + h / 2 - t / 2, pos.z + pos.nz * o, 0, pos.ry, 0);
  b.box(mat, w, t, d, pos.x + pos.nx * o, pos.y - h / 2 + t / 2, pos.z + pos.nz * o, 0, pos.ry, 0);
  b.box(mat, t, h, d, pos.x + pos.nx * o + Math.cos(pos.ry) * (w / 2 - t / 2), pos.y, pos.z + pos.nz * o - Math.sin(pos.ry) * (w / 2 - t / 2), 0, pos.ry, 0);
  b.box(mat, t, h, d, pos.x + pos.nx * o + Math.cos(pos.ry) * -(w / 2 - t / 2), pos.y, pos.z + pos.nz * o - Math.sin(pos.ry) * -(w / 2 - t / 2), 0, pos.ry, 0);
}

function muntinGrid(b, mat, pos, w, h, cols, rows, dx, dy, depth) {
  const o = 0.035;
  for (let i = 1; i < cols; i++) {
    const along = -w / 2 + (i / cols) * w + dx;
    const x = pos.x + Math.cos(pos.ry) * along;
    const z = pos.z - Math.sin(pos.ry) * along;
    b.box(mat, 0.03, h, depth, x + pos.nx * o, pos.y + dy, z + pos.nz * o, 0, pos.ry, 0);
  }
  for (let j = 1; j < rows; j++) {
    const yy = pos.y + dy - h / 2 + (j / rows) * h;
    b.box(mat, w, 0.03, depth, pos.x + pos.nx * o + Math.cos(pos.ry) * dx, yy, pos.z + pos.nz * o - Math.sin(pos.ry) * dx, 0, pos.ry, 0);
  }
}

function addShutters(b, mats, pos, w, h, rng) {
  const sw = w * 0.42;
  const mat = rng.chance(0.5) ? mats.wood : mats.wallAlt || mats.wood;
  const along = w / 2 + sw / 2 + 0.04;
  for (const s of [-1, 1]) {
    const x = pos.x + Math.cos(pos.ry) * along * s;
    const z = pos.z - Math.sin(pos.ry) * along * s;
    b.box(mat, sw, h * 0.96, 0.04, x + pos.nx * 0.03, pos.y, z + pos.nz * 0.03, 0, pos.ry, 0);
    b.box(mats.frame, 0.02, h * 0.9, 0.045, x + pos.nx * 0.04, pos.y, z + pos.nz * 0.04, 0, pos.ry, 0);
  }
}

export function addDoor(b, mats, pos, w, h, style, rng) {
  const type = style.door;
  const reveal = 0.16;
  const inner = offset(pos, -reveal);
  b.box(mats.dark, w + 0.08, h + 0.08, reveal, inner.x, pos.y, inner.z, 0, pos.ry, 0);
  frameRect(b, mats.trim, pos, w + 0.12, h + 0.1, 0.07, 0.08);

  if (type === "classical") addPaneledDoor(b, mats, pos, w, h, true);
  else if (type === "victorian") addPaneledDoor(b, mats, pos, w, h, false);
  else if (type === "arched") addArchedDoor(b, mats, pos, w, h);
  else if (type === "board") addBoardDoor(b, mats, pos, w, h);
  else if (type === "deco") addDecoDoor(b, mats, pos, w, h);
  else if (type === "brutal") addBrutalDoor(b, mats, pos, w, h);
  else if (type === "carriage") addCarriageDoor(b, mats, pos, w, h);
  else addSlabDoor(b, mats, pos, w, h);

  const handleX = pos.x + Math.cos(pos.ry) * (w * 0.32);
  const handleZ = pos.z - Math.sin(pos.ry) * (w * 0.32);
  b.box(mats.metal, 0.03, 0.12, 0.05, handleX + pos.nx * 0.06, pos.y - 0.05, handleZ + pos.nz * 0.06, 0, pos.ry, 0);

  if (type === "classical" || type === "carriage") {
    const tw = w * 0.7;
    const th = 0.45;
    const ty = pos.y + h / 2 + 0.32;
    const tp = { ...pos, y: ty };
    addWindow(b, mats, tp, tw, th, style, { muntins: 2 }, rng, { type: "sash", noShutters: true, lit: false });
  }
}

function addPaneledDoor(b, mats, pos, w, h, fancy) {
  b.box(mats.door, w, h, 0.07, pos.x + pos.nx * -0.02, pos.y, pos.z + pos.nz * -0.02, 0, pos.ry, 0);
  const cols = 2;
  const rows = fancy ? 3 : 2;
  const pw = (w - 0.18) / cols;
  const ph = (h - 0.2) / rows;
  for (let i = 0; i < cols; i++) {
    for (let j = 0; j < rows; j++) {
      const along = -w / 2 + 0.1 + pw * (i + 0.5);
      const yy = pos.y - h / 2 + 0.12 + ph * (j + 0.5);
      const x = pos.x + Math.cos(pos.ry) * along;
      const z = pos.z - Math.sin(pos.ry) * along;
      b.box(mats.wood, pw * 0.78, ph * 0.72, 0.03, x + pos.nx * 0.03, yy, z + pos.nz * 0.03, 0, pos.ry, 0);
    }
  }
}

function addArchedDoor(b, mats, pos, w, h) {
  b.box(mats.door, w, h * 0.7, 0.07, pos.x + pos.nx * -0.02, pos.y - h * 0.12, pos.z + pos.nz * -0.02, 0, pos.ry, 0);
  const shape = new THREE.Shape();
  shape.absarc(0, 0, w / 2, 0, Math.PI, false);
  b.shape(mats.door, shape, 0.07, pos.x + pos.nx * -0.02, pos.y + h * 0.2, pos.z + pos.nz * -0.02, 0, pos.ry, 0);
}

function addBoardDoor(b, mats, pos, w, h) {
  b.box(mats.timber, w, h, 0.07, pos.x + pos.nx * -0.02, pos.y, pos.z + pos.nz * -0.02, 0, pos.ry, 0);
  for (let i = 1; i < 5; i++) {
    const along = -w / 2 + (i / 5) * w;
    const x = pos.x + Math.cos(pos.ry) * along;
    const z = pos.z - Math.sin(pos.ry) * along;
    b.box(mats.dark, 0.02, h, 0.08, x + pos.nx * -0.01, pos.y, z + pos.nz * -0.01, 0, pos.ry, 0);
  }
  b.box(mats.iron, w * 0.7, 0.04, 0.05, pos.x + pos.nx * 0.04, pos.y + h * 0.15, pos.z + pos.nz * 0.04, 0, pos.ry, 0);
  b.box(mats.iron, w * 0.7, 0.04, 0.05, pos.x + pos.nx * 0.04, pos.y - h * 0.15, pos.z + pos.nz * 0.04, 0, pos.ry, 0);
}

function addDecoDoor(b, mats, pos, w, h) {
  b.box(mats.door, w, h, 0.07, pos.x + pos.nx * -0.02, pos.y, pos.z + pos.nz * -0.02, 0, pos.ry, 0);
  b.box(mats.metal, w * 0.15, h, 0.08, pos.x + pos.nx * 0.02, pos.y, pos.z + pos.nz * 0.02, 0, pos.ry, 0);
  for (let i = 0; i < 3; i++) {
    b.box(mats.metal, w * 0.7, 0.03, 0.05, pos.x + pos.nx * 0.03, pos.y + h * 0.3 - i * 0.18, pos.z + pos.nz * 0.03, 0, pos.ry, 0);
  }
}

function addBrutalDoor(b, mats, pos, w, h) {
  b.box(mats.iron, w, h, 0.08, pos.x + pos.nx * -0.02, pos.y, pos.z + pos.nz * -0.02, 0, pos.ry, 0);
  b.box(mats.dark, w * 0.4, h * 0.35, 0.04, pos.x + pos.nx * 0.04 + Math.cos(pos.ry) * -w * 0.15, pos.y + h * 0.2, pos.z + pos.nz * 0.04, 0, pos.ry, 0);
}

function addCarriageDoor(b, mats, pos, w, h) {
  addPaneledDoor(b, mats, pos, w, h, true);
  b.box(mats.iron, w + 0.04, 0.05, 0.06, pos.x + pos.nx * 0.04, pos.y, pos.z + pos.nz * 0.04, 0, pos.ry, 0);
}

function addSlabDoor(b, mats, pos, w, h) {
  b.box(mats.door, w, h, 0.06, pos.x + pos.nx * -0.02, pos.y, pos.z + pos.nz * -0.02, 0, pos.ry, 0);
}

export function addStoop(b, mats, pos, w, style) {
  const steps = style.id === "brutalist" || style.id === "modernist" ? 2 : 4;
  const stepH = 0.16;
  const stepD = 0.3;
  const stepW = w + 0.9;
  for (let i = 0; i < steps; i++) {
    const d = stepD;
    const out = 0.08 + i * stepD + stepD / 2;
    const y = (steps - i) * stepH * 0.5;
    const h = (steps - i) * stepH;
    b.box(mats.stone, stepW, h, d, pos.x + pos.nx * out, h / 2, pos.z + pos.nz * out);
  }
  const cheek = steps * stepH;
  const cheekD = steps * stepD + 0.1;
  const along = stepW / 2 + 0.06;
  for (const s of [-1, 1]) {
    const x = pos.x + Math.cos(pos.ry) * along * s + pos.nx * (cheekD / 2);
    const z = pos.z - Math.sin(pos.ry) * along * s + pos.nz * (cheekD / 2);
    b.box(mats.stone, 0.12, cheek, cheekD, x, cheek / 2, z, 0, pos.ry, 0);
  }
}

export function addBalcony(b, mats, pos, w, style) {
  const kind = style.balcony;
  const depth = kind === "chunk" ? 1.15 : kind === "modern" ? 1.0 : 0.72;
  const thick = kind === "chunk" ? 0.18 : 0.1;
  const y = pos.y;
  const cx = pos.x + pos.nx * (depth / 2);
  const cz = pos.z + pos.nz * (depth / 2);
  const slab = kind === "chunk" ? mats.wall : kind === "modern" ? mats.slab : mats.stone;
  b.box(slab, w, thick, depth, cx, y, cz, 0, pos.ry, 0);

  if (kind === "modern") {
    b.box(mats.iron, w, 0.06, depth, cx, y + 0.9, cz, 0, pos.ry, 0);
    b.box(mats.iron, 0.05, 0.9, 0.05, pos.x + pos.nx * 0.08 + Math.cos(pos.ry) * (w / 2), y + 0.45, pos.z + pos.nz * 0.08 - Math.sin(pos.ry) * (w / 2), 0, pos.ry, 0);
    b.box(mats.iron, 0.05, 0.9, 0.05, pos.x + pos.nx * 0.08 + Math.cos(pos.ry) * (-w / 2), y + 0.45, pos.z + pos.nz * 0.08 - Math.sin(pos.ry) * (-w / 2), 0, pos.ry, 0);
    b.box(mats.iron, 0.05, 0.9, 0.05, pos.x + pos.nx * (depth - 0.05) + Math.cos(pos.ry) * (w / 2), y + 0.45, pos.z + pos.nz * (depth - 0.05) - Math.sin(pos.ry) * (w / 2), 0, pos.ry, 0);
    b.box(mats.iron, 0.05, 0.9, 0.05, pos.x + pos.nx * (depth - 0.05) + Math.cos(pos.ry) * (-w / 2), y + 0.45, pos.z + pos.nz * (depth - 0.05) - Math.sin(pos.ry) * (-w / 2), 0, pos.ry, 0);
  } else if (kind === "chunk") {
    b.box(mats.wall, w, 0.7, 0.12, pos.x + pos.nx * depth, y + 0.45, pos.z + pos.nz * depth, 0, pos.ry, 0);
    b.box(mats.wall, 0.12, 0.7, depth, cx + Math.cos(pos.ry) * (w / 2), y + 0.45, cz - Math.sin(pos.ry) * (w / 2), 0, pos.ry, 0);
    b.box(mats.wall, 0.12, 0.7, depth, cx + Math.cos(pos.ry) * (-w / 2), y + 0.45, cz - Math.sin(pos.ry) * (-w / 2), 0, pos.ry, 0);
  } else {
    addIronRail(b, mats, pos, w, depth, y);
  }
}

function addIronRail(b, mats, pos, w, depth, y) {
  const h = 0.85;
  const posts = Math.max(4, Math.round(w / 0.22));
  for (let i = 0; i <= posts; i++) {
    const along = -w / 2 + (i / posts) * w;
    const x = pos.x + pos.nx * (depth - 0.04) + Math.cos(pos.ry) * along;
    const z = pos.z + pos.nz * (depth - 0.04) - Math.sin(pos.ry) * along;
    b.box(mats.iron, 0.025, h, 0.025, x, y + h / 2, z, 0, pos.ry, 0);
  }
  b.box(mats.iron, w, 0.03, 0.03, pos.x + pos.nx * (depth - 0.04), y + h, pos.z + pos.nz * (depth - 0.04), 0, pos.ry, 0);
  b.box(mats.iron, w, 0.03, 0.03, pos.x + pos.nx * (depth - 0.04), y + h * 0.45, pos.z + pos.nz * (depth - 0.04), 0, pos.ry, 0);
  for (const s of [-1, 1]) {
    b.box(mats.iron, 0.03, h, depth, pos.x + pos.nx * (depth / 2) + Math.cos(pos.ry) * (w / 2) * s, y + h / 2, pos.z + pos.nz * (depth / 2) - Math.sin(pos.ry) * (w / 2) * s, 0, pos.ry, 0);
  }
}
