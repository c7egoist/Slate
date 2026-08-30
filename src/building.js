import * as THREE from "three";
import { RNG } from "./rng.js";
import { Builder } from "./builder.js";
import { STYLES } from "./styles.js";
import { createMaterials } from "./textures.js";
import { addColumn, addPediment, orderOf } from "./parts/columns.js";
import { addWindow, addDoor, addStoop, addBalcony } from "./parts/openings.js";
import { addRoof } from "./parts/roof.js";

export function generateBuilding(params) {
  const rng = new RNG(params.seed);
  const style = STYLES[params.style] || STYLES.georgian;
  const mats = createMaterials(style, rng);
  const b = new Builder();
  const dims = computeDims(params, style, rng);

  addFoundation(b, mats, dims, style);
  addFloors(b, mats, dims, style);

  for (const mass of dims.masses) {
    addMass(b, mats, dims, style, params, rng, mass);
  }

  addMainDoor(b, mats, dims, style, params, rng);
  if (params.columns) addPortico(b, mats, dims, style, params, rng);
  if (style.timber) addTimber(b, mats, dims, style, rng);
  if (style.quoins) addQuoins(b, mats, dims, style);
  if (style.cornice) addCornice(b, mats, dims, style);
  if (style.id === "deco") addDecoFins(b, mats, dims, style);
  if (style.id === "victorian") addBayWindow(b, mats, dims, style, params, rng);
  if (style.porch && style.id !== "mediterranean") addWrapPorch(b, mats, dims, style, params, rng);
  if (style.id === "mediterranean" && params.columns) addLoggia(b, mats, dims, style, params, rng);

  addRoof(b, mats, dims, style, params, rng);

  const group = b.build();
  group.userData = { mats, dims, style, stats: makeStats(dims, style, params) };
  return group;
}

function computeDims(params, style, rng) {
  const floorsN = params.floors;
  const baysX = params.baysX;
  const baysZ = params.baysZ;
  const bayW = style.bayWidth;
  const bayD = style.bayDepth;
  const heights = [];
  for (let i = 0; i < floorsN; i++) {
    if (i === 0) heights.push(style.baseHeight);
    else if (i === 1 && style.pianoNobile) heights.push(style.typicalHeight + 0.45);
    else if (i === floorsN - 1 && floorsN > 3) heights.push(style.atticHeight);
    else heights.push(style.typicalHeight);
  }
  const yOf = [0];
  for (let i = 0; i < heights.length; i++) yOf.push(yOf[i] + heights[i]);
  const width = baysX * bayW;
  const depth = baysZ * bayD;
  const totalH = yOf[floorsN];
  const masses = massing(style, width, depth, floorsN, heights, yOf, rng);
  return { floorsN, baysX, baysZ, bayW, bayD, heights, yOf, width, depth, totalH, masses, t: 0.32 };
}

function massing(style, width, depth, floorsN, heights, yOf, rng) {
  const main = { x: 0, z: 0, w: width, d: depth, floor0: 0, floor1: floorsN, primary: true };
  if (style.massing === "offset") {
    const split = width * rng.range(0.42, 0.58);
    const h2 = Math.max(1, floorsN - 1);
    return [
      { x: -width / 2 + split / 2, z: 0.2, w: split, d: depth * 0.92, floor0: 0, floor1: floorsN, primary: true },
      { x: width / 2 - (width - split) / 2, z: -0.35, w: width - split, d: depth * 0.78, floor0: 0, floor1: h2, primary: false },
    ];
  }
  if (style.massing === "setback") {
    const masses = [
      { ...main, floor1: floorsN >= 4 ? floorsN - 2 : floorsN },
    ];
    if (floorsN >= 4) {
      masses.push({
        x: 0,
        z: 0,
        w: width * 0.76,
        d: depth * 0.76,
        floor0: floorsN - 2,
        floor1: floorsN >= 6 ? floorsN - 1 : floorsN,
        primary: false,
      });
    }
    if (floorsN >= 6) {
      masses.push({
        x: 0,
        z: 0,
        w: width * 0.56,
        d: depth * 0.56,
        floor0: floorsN - 1,
        floor1: floorsN,
        primary: false,
      });
    }
    return masses;
  }
  if (style.massing === "wing") {
    const wingW = width * 0.38;
    return [
      main,
      {
        x: width / 2 + wingW / 2 - 0.2,
        z: -depth * 0.12,
        w: wingW,
        d: depth * 0.72,
        floor0: 0,
        floor1: Math.max(1, floorsN - (floorsN > 2 ? 1 : 0)),
        primary: false,
      },
    ];
  }
  if (style.massing === "chunk") {
    return [
      main,
      {
        x: width * 0.18,
        z: depth * 0.22,
        w: width * 0.55,
        d: depth * 0.42,
        floor0: Math.max(0, floorsN - 2),
        floor1: floorsN,
        primary: false,
      },
    ];
  }
  return [main];
}

function addFoundation(b, mats, dims, style) {
  const h = 0.38;
  b.box(mats.stone, dims.width + 0.35, h, dims.depth + 0.35, 0, h / 2 - 0.02, 0);
  b.box(mats.base, dims.width + 0.12, 0.22, dims.depth + 0.12, 0, h + 0.08, 0);
}

function addFloors(b, mats, dims, style) {
  for (let i = 0; i <= dims.floorsN; i++) {
    const y = dims.yOf[i];
    const slab = i === 0 ? mats.stone : mats.floor;
    const t = i === 0 ? 0.16 : 0.12;
    b.box(slab, dims.width - 0.4, t, dims.depth - 0.4, 0, y + t / 2 + 0.01, 0, 0, 0, 0, 0.35);
    if (style.id === "brutalist" || style.id === "modernist") {
      b.box(mats.slab, dims.width + 0.5, 0.2, dims.depth + 0.5, 0, y + 0.08, 0);
    }
    if (i > 0 && i < dims.floorsN && style.stringCourse) {
      b.box(mats.trim, dims.width + 0.1, 0.1, 0.14, 0, y + 0.04, dims.depth / 2 + 0.02);
      b.box(mats.trim, dims.width + 0.1, 0.1, 0.14, 0, y + 0.04, -dims.depth / 2 - 0.02);
      b.box(mats.trim, 0.14, 0.1, dims.depth + 0.1, dims.width / 2 + 0.02, y + 0.04, 0);
      b.box(mats.trim, 0.14, 0.1, dims.depth + 0.1, -dims.width / 2 - 0.02, y + 0.04, 0);
    }
  }
}

function addMass(b, mats, dims, style, params, rng, mass) {
  const t = dims.t;
  for (let f = mass.floor0; f < mass.floor1; f++) {
    if (mass.replaceUpper && f < mass.floor0) continue;
    const y0 = dims.yOf[f];
    const fh = dims.heights[f];
    const isBase = f === 0 && style.rusticatedBase;
    const wallMat = isBase ? mats.base : mats.wall;
    const sill = style.windowSill * (f === 0 ? 1.05 : 1);
    const winH = THREE.MathUtils.clamp(style.windowHeight * params.winH * (f === 1 && style.pianoNobile ? 1.12 : 1), 0.9, fh - sill - 0.35);
    const winW = style.windowWidth;

    addFacade(b, mats, dims, style, params, rng, mass, "front", f, y0, fh, wallMat, sill, winH, winW, t);
    addFacade(b, mats, dims, style, params, rng, mass, "back", f, y0, fh, wallMat, sill, winH, winW, t);
    addFacade(b, mats, dims, style, params, rng, mass, "left", f, y0, fh, wallMat, sill, winH, winW, t);
    addFacade(b, mats, dims, style, params, rng, mass, "right", f, y0, fh, wallMat, sill, winH, winW, t);
  }
}

function addFacade(b, mats, dims, style, params, rng, mass, face, floor, y0, fh, wallMat, sill, winH, winW, t) {
  const alongX = face === "front" || face === "back";
  const len = alongX ? mass.w : mass.d;
  const bays = alongX ? Math.max(1, Math.round(mass.w / dims.bayW)) : Math.max(1, Math.round(mass.d / dims.bayD));
  const bay = len / bays;
  const cx = mass.x;
  const cz = mass.z;
  const skipDoorBay = mass.primary && face === "front" && floor === 0;
  const doorBay = style.symmetry ? Math.floor(bays / 2) : 0;

  const wallSlab = (a0, a1, y, h) => {
    const pw = a1 - a0;
    if (pw < 0.03 || h < 0.03) return;
    const mid = (a0 + a1) / 2;
    if (alongX) {
      const z = face === "front" ? cz + mass.d / 2 - t / 2 : cz - mass.d / 2 + t / 2;
      b.box(wallMat, pw, h, t, cx + mid, y + h / 2, z, 0, 0, 0, 0.45);
    } else {
      const x = face === "left" ? cx - mass.w / 2 + t / 2 : cx + mass.w / 2 - t / 2;
      b.box(wallMat, t, h, pw, x, y + h / 2, cz + mid, 0, 0, 0, 0.45);
    }
  };

  for (let i = 0; i < bays; i++) {
    const leftAlong = -len / 2 + i * bay;
    const rightAlong = leftAlong + bay;
    const along = leftAlong + bay / 2;
    const isDoor = skipDoorBay && i === doorBay;
    const useRibbon = style.window === "ribbon";
    const w = useRibbon ? Math.min(winW * 1.15, bay - 0.25) : Math.min(winW, bay - 0.28);

    if (isDoor) {
      const openingW = style.door === "carriage" ? 1.55 : 1.28;
      const pier = Math.max(0.16, (bay - openingW) / 2);
      const doorH = Math.min(fh * 0.78, 2.95);
      wallSlab(leftAlong, leftAlong + pier, y0, fh);
      wallSlab(rightAlong - pier, rightAlong, y0, fh);
      wallSlab(leftAlong + pier, rightAlong - pier, y0 + doorH, fh - doorH);
      continue;
    }

    const openingW = Math.min(w, bay - 0.28);
    const pier = Math.max(0.14, (bay - openingW) / 2);
    wallSlab(leftAlong, rightAlong, y0, sill);
    wallSlab(leftAlong, leftAlong + pier, y0 + sill, winH);
    wallSlab(rightAlong - pier, rightAlong, y0 + sill, winH);
    const above = fh - sill - winH;
    wallSlab(leftAlong, rightAlong, y0 + sill + winH, above);

    const real = {
      x: alongX ? cx + along : face === "left" ? cx - mass.w / 2 : cx + mass.w / 2,
      y: y0 + sill + winH / 2,
      z: alongX ? (face === "front" ? cz + mass.d / 2 : cz - mass.d / 2) : cz + along,
      ...{
        front: { ry: 0, nx: 0, nz: 1 },
        back: { ry: Math.PI, nx: 0, nz: -1 },
        left: { ry: Math.PI / 2, nx: -1, nz: 0 },
        right: { ry: -Math.PI / 2, nx: 1, nz: 0 },
      }[face],
      face,
    };

    const grouped = style.window === "casement";
    if (grouped) {
      const n = 3;
      const gw = openingW * 0.3;
      for (let k = 0; k < n; k++) {
        const off = (k - 1) * (gw + 0.05);
        const p2 = { ...real };
        p2.x += Math.cos(real.ry) * off;
        p2.z -= Math.sin(real.ry) * off;
        addWindow(b, mats, p2, gw, winH * 0.92, style, params, rng, {
          type: floor === 0 && style.window === "arched" ? "arched" : style.window,
        });
      }
    } else {
      const type =
        floor === 0 && style.window === "arched"
          ? "arched"
          : floor === 0 && style.id === "haussmann"
            ? "arched"
            : style.window;
      addWindow(b, mats, real, openingW, winH, style, params, rng, {
        type,
        keystone: style.id === "georgian" && floor === 0 && rng.chance(0.2),
      });
    }

    if (params.balconies && shouldBalcony(style, floor, i, bays, dims.floorsN, face, mass)) {
      addBalcony(b, mats, { ...real, y: y0 + sill - 0.08 }, Math.min(bay * 0.92, openingW + 0.7), style);
    }
  }
}

function shouldBalcony(style, floor, bay, bays, floorsN, face, mass) {
  if (!mass.primary || face !== "front") return false;
  if (style.balcony === "none") return false;
  if (style.balcony === "haussmann") {
    if (floor === 1) return true;
    if (floor === floorsN - 2 && floorsN > 3) return true;
    return floor > 0 && floor < floorsN - 1 && (bay === 0 || bay === bays - 1);
  }
  if (style.balcony === "iron") return floor === 1 || (floor > 0 && bay % 2 === 1);
  if (style.balcony === "modern") return floor === floorsN - 1 || (floor === 1 && bay === bays - 1);
  if (style.balcony === "chunk") return floor > 0 && bay % 2 === 0;
  return floor === 1;
}

function addMainDoor(b, mats, dims, style, params, rng) {
  const mass = dims.masses.find((m) => m.primary) || dims.masses[0];
  const bays = Math.max(1, Math.round(mass.w / dims.bayW));
  const bayI = style.symmetry ? Math.floor(bays / 2) : 0;
  const along = -mass.w / 2 + (bayI + 0.5) * (mass.w / bays);
  const doorW = style.door === "carriage" ? 1.45 : 1.15;
  const doorH = Math.min(dims.heights[0] * 0.72, 2.6);
  const pos = {
    x: mass.x + along,
    y: doorH / 2 + 0.38,
    z: mass.z + mass.d / 2,
    ry: 0,
    nx: 0,
    nz: 1,
    face: "front",
  };
  addDoor(b, mats, pos, doorW, doorH, style, rng);
  addStoop(b, mats, { ...pos, y: 0 }, doorW, style);
  dims._door = { pos, doorW, doorH, along };
}

function addPortico(b, mats, dims, style, params, rng) {
  const order = orderOf(style, params.order);
  if (order === "pilotis") {
    addGroundPilotis(b, mats, dims, style, params);
    return;
  }
  if (style.id === "brutalist") {
    addGroundPilotis(b, mats, dims, style, params);
    return;
  }
  if (style.porch && style.id === "victorian") return;
  if (style.id === "mediterranean") return;
  if (style.id === "tudor") return;

  const door = dims._door;
  if (!door) return;
  const h = Math.min(dims.heights[0] + 0.15, 3.6);
  const depth = 1.35;
  const width = Math.min(dims.width * 0.42, Math.max(3.2, door.doorW + 2.2));
  const z = dims.depth / 2 + depth * 0.45;
  const cols = width > 4.2 ? 4 : 2;
  for (let i = 0; i < cols; i++) {
    const x = -width / 2 + 0.28 + (cols === 1 ? width / 2 : (i / (cols - 1)) * (width - 0.56));
    addColumn(b, mats, door.pos.x + (style.symmetry ? 0 : 0) + (x - door.pos.x + door.pos.x), 0.35, z + 0.15, h - 0.45, style, params.order);
  }
  const xs = [];
  for (let i = 0; i < cols; i++) xs.push(-width / 2 + 0.28 + (i / Math.max(1, cols - 1)) * (width - 0.56));
  b.loose.length;
  const yEnt = h - 0.05;
  b.box(mats.trim, width + 0.2, 0.14, depth + 0.25, door.pos.x, yEnt, dims.depth / 2 + depth * 0.4);
  b.box(mats.trim, width + 0.35, 0.12, depth + 0.4, door.pos.x, yEnt + 0.18, dims.depth / 2 + depth * 0.4);
  if (style.dentils) {
    const n = Math.floor(width / 0.16);
    for (let i = 0; i < n; i++) {
      const x = door.pos.x - width / 2 + (i + 0.5) * (width / n);
      b.box(mats.trim, 0.07, 0.1, 0.1, x, yEnt + 0.08, dims.depth / 2 + depth * 0.4 + depth * 0.28);
    }
  }
  if (style.door === "classical" || style.id === "georgian") {
    addPediment(b, mats.trim, width + 0.2, 0.22, yEnt + 0.28, dims.depth / 2 + depth * 0.4, 0.85);
  }

  for (let i = 0; i < cols; i++) {
    const x = door.pos.x - width / 2 + 0.28 + (i / Math.max(1, cols - 1)) * (width - 0.56);
    addColumn(b, mats, x, 0.32, dims.depth / 2 + depth * 0.55, h - 0.5, style, params.order);
  }
}

function addGroundPilotis(b, mats, dims, style, params) {
  const inset = 0.55;
  const h = dims.heights[0] - 0.15;
  const nx = Math.max(2, dims.baysX);
  const nz = Math.max(2, dims.baysZ);
  for (let i = 0; i < nx; i++) {
    for (let j = 0; j < nz; j++) {
      if (i > 0 && i < nx - 1 && j > 0 && j < nz - 1) continue;
      const x = -dims.width / 2 + inset + (i / (nx - 1)) * (dims.width - inset * 2);
      const z = -dims.depth / 2 + inset + (j / (nz - 1)) * (dims.depth - inset * 2);
      addColumn(b, mats, x, 0.2, z, h, style, params.order);
    }
  }
}

function addTimber(b, mats, dims, style, rng) {
  const t = 0.12;
  const d = 0.08;
  const z = dims.depth / 2 + 0.02;
  for (let f = 1; f <= dims.floorsN; f++) {
    b.box(mats.timber, dims.width + 0.04, t, d, 0, dims.yOf[f] - 0.02, z);
  }
  for (let i = 0; i <= dims.baysX; i++) {
    const x = -dims.width / 2 + (i / dims.baysX) * dims.width;
    b.box(mats.timber, t, dims.totalH - dims.heights[0] + 0.1, d, x, dims.heights[0] + (dims.totalH - dims.heights[0]) / 2, z);
  }
  for (let i = 0; i < dims.baysX; i++) {
    if (!rng.chance(0.65)) continue;
    const x0 = -dims.width / 2 + (i / dims.baysX) * dims.width;
    const x1 = -dims.width / 2 + ((i + 1) / dims.baysX) * dims.width;
    const y0 = dims.heights[0];
    const y1 = dims.totalH;
    const len = Math.hypot(x1 - x0, y1 - y0);
    const ang = Math.atan2(y1 - y0, x1 - x0);
    b.box(mats.timber, len, t, d, (x0 + x1) / 2, (y0 + y1) / 2, z, 0, 0, -ang + Math.PI / 2);
    const ang2 = Math.atan2(y1 - y0, x0 - x1);
    if (rng.chance(0.5)) b.box(mats.timber, len, t, d, (x0 + x1) / 2, (y0 + y1) / 2, z, 0, 0, -ang2 + Math.PI / 2);
  }
  b.box(mats.timber, t * 1.4, dims.totalH, d * 1.2, -dims.width / 2, dims.totalH / 2, z);
  b.box(mats.timber, t * 1.4, dims.totalH, d * 1.2, dims.width / 2, dims.totalH / 2, z);
}

function addQuoins(b, mats, dims, style) {
  const s = 0.32;
  const d = 0.1;
  const n = Math.max(6, Math.floor(dims.totalH / 0.38));
  const corners = [
    [dims.width / 2, dims.depth / 2, 0],
    [-dims.width / 2, dims.depth / 2, 0],
    [dims.width / 2, -dims.depth / 2, Math.PI],
    [-dims.width / 2, -dims.depth / 2, Math.PI],
  ];
  for (const [x, z] of corners) {
    for (let i = 0; i < n; i++) {
      const long = i % 2 === 0;
      const y = 0.45 + i * 0.38;
      if (y > dims.totalH - 0.3) break;
      b.box(mats.stone, long ? s * 1.7 : s, 0.34, long ? s : s * 1.7, x, y, z);
    }
  }
}

function addCornice(b, mats, dims, style) {
  const y = dims.totalH;
  const extra = 0.28;
  b.box(mats.trim, dims.width + extra, 0.12, dims.depth + extra, 0, y + 0.04, 0);
  b.box(mats.trim, dims.width + extra + 0.18, 0.1, dims.depth + extra + 0.18, 0, y + 0.14, 0);
  if (style.dentils) {
    const step = 0.16;
    const n = Math.floor(dims.width / step);
    for (let i = 0; i < n; i++) {
      const x = -dims.width / 2 + (i + 0.5) * (dims.width / n);
      b.box(mats.trim, 0.08, 0.1, 0.12, x, y - 0.02, dims.depth / 2 + 0.08);
      b.box(mats.trim, 0.08, 0.1, 0.12, x, y - 0.02, -dims.depth / 2 - 0.08);
    }
  }
}

function addDecoFins(b, mats, dims, style) {
  for (let i = 0; i <= dims.baysX; i++) {
    const x = -dims.width / 2 + (i / dims.baysX) * dims.width;
    const h = dims.totalH + 0.45 + (i % 2 === 0 ? 0.35 : 0);
    b.box(mats.trim, 0.16, h, 0.22, x, h / 2, dims.depth / 2 + 0.04);
    b.box(mats.metal, 0.05, 0.2, 0.05, x, h + 0.05, dims.depth / 2 + 0.08);
  }
  const bandY = dims.yOf[Math.min(1, dims.floorsN - 1)];
  for (let i = 0; i < dims.baysX; i++) {
    const x = -dims.width / 2 + (i + 0.5) * dims.bayW;
    for (let k = 0; k < 3; k++) {
      b.box(mats.metal, 0.18, 0.06, 0.06, x + (k - 1) * 0.16, bandY + 0.2, dims.depth / 2 + 0.08, 0, 0, Math.PI / 4);
    }
  }
}

function addBayWindow(b, mats, dims, style, params, rng) {
  if (dims.baysX < 3) return;
  const mass = dims.masses[0];
  const bayI = dims.baysX - 1;
  const along = -dims.width / 2 + (bayI + 0.5) * dims.bayW;
  const proj = 0.7;
  const w = dims.bayW * 0.92;
  const h = Math.min(dims.totalH - 0.4, dims.yOf[Math.min(2, dims.floorsN)] - 0.2);
  const z = dims.depth / 2 + proj / 2;
  b.box(mats.wall, w, h, proj, along, h / 2 + 0.2, z);
  b.box(mats.roof, w + 0.15, 0.12, proj + 0.15, along, h + 0.26, z);
  for (let f = 0; f < Math.min(2, dims.floorsN); f++) {
    const y = dims.yOf[f] + style.windowSill + style.windowHeight * params.winH * 0.5;
    const pos = { x: along, y, z: dims.depth / 2 + proj, ry: 0, nx: 0, nz: 1, face: "front" };
    addWindow(b, mats, pos, 0.85, style.windowHeight * params.winH * 0.9, style, params, rng, { noShutters: true });
    const sideW = 0.55;
    addWindow(b, mats, { x: along - w / 2, y, z: dims.depth / 2 + proj * 0.5, ry: Math.PI / 2, nx: -1, nz: 0, face: "left" }, sideW, style.windowHeight * params.winH * 0.85, style, params, rng, { noShutters: true });
    addWindow(b, mats, { x: along + w / 2, y, z: dims.depth / 2 + proj * 0.5, ry: -Math.PI / 2, nx: 1, nz: 0, face: "right" }, sideW, style.windowHeight * params.winH * 0.85, style, params, rng, { noShutters: true });
  }
}

function addWrapPorch(b, mats, dims, style, params, rng) {
  const depth = 1.55;
  const y = 0.32;
  const h = dims.heights[0] * 0.72;
  const w = dims.width * 0.72;
  const x0 = -dims.width * 0.08;
  const z = dims.depth / 2 + depth / 2;
  b.box(mats.wood, w, 0.12, depth, x0, y, z);
  const posts = 4;
  for (let i = 0; i < posts; i++) {
    const x = x0 - w / 2 + 0.2 + (i / (posts - 1)) * (w - 0.4);
    addColumn(b, mats, x, y, z + depth / 2 - 0.12, h - 0.2, style, "craftsman");
  }
  b.box(mats.roof, w + 0.25, 0.1, depth + 0.2, x0, y + h, z);
  const positions = [];
  const uvs = [];
  const indices = [];
  const yy = y + h;
  const pts = [
    [x0 - w / 2 - 0.1, yy, z + depth / 2 + 0.1],
    [x0 + w / 2 + 0.1, yy, z + depth / 2 + 0.1],
    [x0 + w / 2 + 0.1, yy + 0.45, z - depth / 2],
    [x0 - w / 2 - 0.1, yy + 0.45, z - depth / 2],
  ];
  const addFace = (a) => {
    const base = positions.length / 3;
    for (const p of a) {
      positions.push(...p);
      uvs.push(p[0] * 0.4, p[2] * 0.4);
    }
    indices.push(base, base + 1, base + 2, base, base + 2, base + 3);
  };
  addFace(pts);
  b.poly(mats.roof, positions, indices, uvs);
  b.box(mats.iron, w - 0.3, 0.04, 0.04, x0, y + 0.7, z + depth / 2 - 0.08);
}

function addLoggia(b, mats, dims, style, params, rng) {
  const n = Math.min(3, dims.baysX);
  const z = dims.depth / 2 + 0.15;
  const y = 0.4;
  const h = dims.heights[0] * 0.85;
  const span = dims.bayW * n * 0.9;
  const x0 = style.symmetry ? 0 : -dims.width * 0.15;
  for (let i = 0; i < n + 1; i++) {
    const x = x0 - span / 2 + (i / n) * span;
    addColumn(b, mats, x, y, z + 0.55, h - 0.2, style, params.order);
  }
  b.box(mats.trim, span + 0.4, 0.18, 1.3, x0, y + h, z + 0.4);
  for (let i = 0; i < n; i++) {
    const x = x0 - span / 2 + (i + 0.5) * (span / n);
    const shape = new THREE.Shape();
    const r = (span / n) * 0.38;
    shape.absarc(0, 0, r, 0, Math.PI, false);
    b.shape(mats.wall, shape, 0.2, x, y + h * 0.55, z + 0.5);
  }
}

function makeStats(dims, style, params) {
  const footprint = dims.width * dims.depth;
  const gfa = footprint * dims.floorsN * 0.88;
  const windows = dims.baysX * dims.floorsN * 2 + dims.baysZ * dims.floorsN * 2 - 1;
  return {
    style: style.name,
    period: style.period,
    origin: style.origin,
    storeys: dims.floorsN,
    height: dims.totalH,
    footprint,
    gfa,
    windows,
    seed: params.seed,
    bays: `${params.baysX} × ${params.baysZ}`,
  };
}
