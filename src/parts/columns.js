import * as THREE from "three";

function orderOf(style, override) {
  if (override && override !== "style") return override;
  return style.column || "doric";
}

export function addColumn(b, mats, x, yBase, z, height, style, override) {
  const order = orderOf(style, override);
  if (order === "pilotis") return addPilotis(b, mats, x, yBase, z, height);
  if (order === "craftsman" || order === "timber") return addCraftsmanPost(b, mats, x, yBase, z, height, order);
  if (order === "pier" || order === "square") return addSquarePier(b, mats, x, yBase, z, height);
  if (order === "deco") return addDecoPier(b, mats, x, yBase, z, height);

  const dia = THREE.MathUtils.clamp(height / 9.2, 0.28, 0.48);
  const plinth = dia * 1.35;
  const baseH = height * 0.08;
  const capH = height * (order === "corinthian" ? 0.12 : 0.09);
  const shaftH = height - baseH - capH;
  const yPlinth = yBase + 0.06;
  const colMat = mats.column;
  const plain = mats.columnPlain;

  b.box(plain, plinth, 0.12, plinth, x, yPlinth, z, 0, 0, 0, 0.8);
  if (order !== "tuscan") {
    b.torus(plain, dia * 0.52, dia * 0.07, x, yBase + baseH * 0.55, z, Math.PI / 2, 0, 0);
  } else {
    b.cyl(plain, dia * 0.58, dia * 0.58, baseH * 0.5, x, yBase + baseH * 0.45, z, 12);
  }

  const yShaft = yBase + baseH + shaftH / 2;
  b.cyl(colMat, dia * 0.42, dia * 0.5, shaftH, x, yShaft, z, 16);

  const yCap = yBase + height - capH;
  addCapital(b, plain, mats, x, yCap, z, dia, capH, order);
}

function addCapital(b, mat, mats, x, y, z, dia, capH, order) {
  const neck = y + capH * 0.15;
  b.cyl(mat, dia * 0.44, dia * 0.44, capH * 0.12, x, neck, z, 16);

  if (order === "ionic") {
    b.cyl(mat, dia * 0.55, dia * 0.46, capH * 0.28, x, y + capH * 0.38, z, 16);
    b.torus(mat, dia * 0.22, dia * 0.11, x - dia * 0.38, y + capH * 0.55, z, 0, 0, Math.PI / 2, 8, 14);
    b.torus(mat, dia * 0.22, dia * 0.11, x + dia * 0.38, y + capH * 0.55, z, 0, 0, Math.PI / 2, 8, 14);
    b.box(mat, dia * 1.35, capH * 0.16, dia * 0.85, x, y + capH * 0.88, z);
  } else if (order === "corinthian") {
    const leafR = dia * 0.58;
    for (let i = 0; i < 8; i++) {
      const a = (i / 8) * Math.PI * 2;
      b.sphere(mat, dia * 0.12, x + Math.cos(a) * leafR, y + capH * 0.28, z + Math.sin(a) * leafR, 8);
    }
    for (let i = 0; i < 8; i++) {
      const a = (i / 8) * Math.PI * 2 + 0.4;
      b.sphere(mat, dia * 0.1, x + Math.cos(a) * leafR * 0.85, y + capH * 0.52, z + Math.sin(a) * leafR * 0.85, 8);
    }
    b.cyl(mat, dia * 0.5, dia * 0.42, capH * 0.2, x, y + capH * 0.7, z, 12);
    b.box(mat, dia * 1.4, capH * 0.14, dia * 1.4, x, y + capH * 0.92, z);
    b.box(mats.metal, dia * 0.2, capH * 0.08, dia * 0.2, x, y + capH, z);
  } else {
    b.cyl(mat, dia * 0.62, dia * 0.46, capH * 0.42, x, y + capH * 0.45, z, 16);
    b.box(mat, dia * 1.28, capH * 0.16, dia * 1.28, x, y + capH * 0.86, z);
  }
}

function addSquarePier(b, mats, x, yBase, z, height) {
  const w = 0.48;
  b.box(mats.base, w + 0.12, 0.16, w + 0.12, x, yBase + 0.08, z);
  b.box(mats.wall, w, height - 0.28, w, x, yBase + height / 2, z);
  b.box(mats.trim, w + 0.1, 0.14, w + 0.1, x, yBase + height - 0.08, z);
}

function addDecoPier(b, mats, x, yBase, z, height) {
  const w = 0.38;
  b.box(mats.base, w + 0.08, 0.2, w + 0.08, x, yBase + 0.1, z);
  b.box(mats.columnPlain, w, height - 0.4, w, x, yBase + height / 2, z);
  b.box(mats.metal, w * 0.25, height - 0.5, 0.04, x, yBase + height / 2, z + w * 0.52);
  b.box(mats.trim, w + 0.16, 0.1, w + 0.16, x, yBase + height - 0.12, z);
  b.box(mats.metal, w + 0.06, 0.04, w + 0.06, x, yBase + height - 0.04, z);
}

function addPilotis(b, mats, x, yBase, z, height) {
  b.cyl(mats.columnPlain, 0.16, 0.18, height, x, yBase + height / 2, z, 12);
}

function addCraftsmanPost(b, mats, x, yBase, z, height, order) {
  const mat = order === "timber" ? mats.timber : mats.wood;
  b.box(mats.stone, 0.55, Math.min(0.7, height * 0.28), 0.55, x, yBase + 0.35, z);
  const shaft = Math.max(0.4, height - 0.8);
  b.box(mat, 0.22, shaft, 0.22, x, yBase + 0.7 + shaft / 2, z);
  b.box(mat, 0.4, 0.1, 0.4, x, yBase + height - 0.08, z);
}

export function addPilaster(b, mats, x, yBase, z, height, ry, style) {
  const w = 0.28;
  const d = 0.12;
  const mat = mats.columnPlain;
  b.box(mat, w, height, d, x, yBase + height / 2, z, 0, ry, 0);
  b.box(mat, w + 0.06, 0.1, d + 0.04, x, yBase + 0.08, z, 0, ry, 0);
  b.box(mat, w + 0.08, 0.12, d + 0.05, x, yBase + height - 0.08, z, 0, ry, 0);
}

export function addEntablature(b, mat, w, d, y, h = 0.42, extra = 0.18) {
  b.box(mat, w, h * 0.28, d, 0, y + h * 0.14, 0);
  b.box(mat, w * 0.985, h * 0.38, d * 0.985, 0, y + h * 0.48, 0);
  b.box(mat, w + extra, h * 0.34, d + extra, 0, y + h * 0.84, 0);
}

export function addPediment(b, mat, w, depth, y, z, h) {
  const shape = new THREE.Shape();
  shape.moveTo(-w / 2, 0);
  shape.lineTo(w / 2, 0);
  shape.lineTo(0, h);
  shape.closePath();
  b.shape(mat, shape, depth, 0, y, z, 0, 0, 0, 0.4);
  b.box(mat, w + 0.08, 0.08, depth + 0.04, 0, y + 0.04, z);
}

export { orderOf };
