import * as THREE from "three";
import { RNG } from "./rng.js";

const cache = new Map();
let anisotropy = 8;

export function setAnisotropy(n) {
  anisotropy = n;
}

function texFromCanvas(canvas, wrap = true) {
  const t = new THREE.CanvasTexture(canvas);
  t.colorSpace = THREE.SRGBColorSpace;
  t.anisotropy = anisotropy;
  t.wrapS = t.wrapT = wrap ? THREE.RepeatWrapping : THREE.ClampToEdgeWrapping;
  t.needsUpdate = true;
  return t;
}

function rgb(c) {
  if (c.isColor) return { r: (c.r * 255) | 0, g: (c.g * 255) | 0, b: (c.b * 255) | 0 };
  const col = new THREE.Color(c);
  return { r: (col.r * 255) | 0, g: (col.g * 255) | 0, b: (col.b * 255) | 0 };
}

function mix(a, b, t) {
  return {
    r: a.r + (b.r - a.r) * t,
    g: a.g + (b.g - a.g) * t,
    b: a.b + (b.b - a.b) * t,
  };
}

function hash2(x, y) {
  const n = Math.sin(x * 127.1 + y * 311.7) * 43758.5453;
  return n - Math.floor(n);
}

function noise(x, y) {
  const xi = Math.floor(x);
  const yi = Math.floor(y);
  const xf = x - xi;
  const yf = y - yi;
  const u = xf * xf * (3 - 2 * xf);
  const v = yf * yf * (3 - 2 * yf);
  const n00 = hash2(xi, yi);
  const n10 = hash2(xi + 1, yi);
  const n01 = hash2(xi, yi + 1);
  const n11 = hash2(xi + 1, yi + 1);
  return n00 * (1 - u) * (1 - v) + n10 * u * (1 - v) + n01 * (1 - u) * v + n11 * u * v;
}

function fbm(x, y, oct = 4) {
  let a = 0.5;
  let f = 1;
  let s = 0;
  let n = 0;
  for (let i = 0; i < oct; i++) {
    s += a * noise(x * f, y * f);
    n += a;
    a *= 0.5;
    f *= 2;
  }
  return s / n;
}

function put(ctx, x, y, w, h, col, a = 1) {
  ctx.fillStyle = `rgba(${col.r | 0},${col.g | 0},${col.b | 0},${a})`;
  ctx.fillRect(x, y, w, h);
}

function canvas(size = 512) {
  const c = document.createElement("canvas");
  c.width = c.height = size;
  const ctx = c.getContext("2d", { willReadFrequently: true });
  return { c, ctx, size };
}

function bumpFrom(src, size, lift = 1) {
  const { c, ctx } = canvas(size);
  ctx.drawImage(src, 0, 0);
  const img = ctx.getImageData(0, 0, size, size);
  const d = img.data;
  for (let i = 0; i < d.length; i += 4) {
    const l = (d[i] * 0.3 + d[i + 1] * 0.5 + d[i + 2] * 0.2) * lift;
    d[i] = d[i + 1] = d[i + 2] = l;
  }
  ctx.putImageData(img, 0, 0);
  const t = texFromCanvas(c);
  t.colorSpace = THREE.NoColorSpace;
  return t;
}

function pair(mapCanvas, bump = true) {
  const map = texFromCanvas(mapCanvas);
  return { map, bump: bump ? bumpFrom(mapCanvas, mapCanvas.width) : null };
}

export function makeBrick(color, mortar, seed, bond = "running") {
  const key = `brick:${color}:${mortar}:${seed}:${bond}`;
  if (cache.has(key)) return cache.get(key);
  const { c, ctx, size } = canvas(512);
  const rng = new RNG(seed);
  const base = rgb(color);
  const mort = rgb(mortar);
  put(ctx, 0, 0, size, size, mort);
  const bw = 58;
  const bh = 18;
  const m = 4;
  let row = 0;
  for (let y = 0; y < size + bh; y += bh + m) {
    const off = bond === "running" && row % 2 ? (bw + m) / 2 : 0;
    for (let x = -bw; x < size + bw; x += bw + m) {
      const n = rng.range(-18, 18);
      const shade = mix(base, { r: base.r + n, g: base.g + n * 0.7, b: base.b + n * 0.5 }, 1);
      const burnt = rng.chance(0.08);
      const col = burnt ? mix(shade, { r: 40, g: 22, b: 18 }, 0.35) : shade;
      put(ctx, x + off, y, bw, bh, col);
      ctx.fillStyle = `rgba(255,255,255,${rng.range(0.02, 0.08)})`;
      ctx.fillRect(x + off, y, bw, 2);
      ctx.fillStyle = `rgba(0,0,0,${rng.range(0.05, 0.14)})`;
      ctx.fillRect(x + off, y + bh - 2, bw, 2);
    }
    row++;
  }
  const img = ctx.getImageData(0, 0, size, size);
  const d = img.data;
  for (let i = 0; i < d.length; i += 4) {
    const g = (fbm((i / 4) % size * 0.08, ((i / 4) / size) * 0.08) - 0.5) * 16;
    d[i] = clamp(d[i] + g);
    d[i + 1] = clamp(d[i + 1] + g);
    d[i + 2] = clamp(d[i + 2] + g);
  }
  ctx.putImageData(img, 0, 0);
  const p = pair(c);
  cache.set(key, p);
  return p;
}

export function makeAshlar(color, seed) {
  const key = `ashlar:${color}:${seed}`;
  if (cache.has(key)) return cache.get(key);
  const { c, ctx, size } = canvas(512);
  const rng = new RNG(seed);
  const base = rgb(color);
  put(ctx, 0, 0, size, size, mix(base, { r: 80, g: 75, b: 68 }, 0.25));
  let y = 0;
  while (y < size) {
    const ch = rng.int(28, 56);
    let x = 0;
    while (x < size) {
      const cw = rng.int(48, 120);
      const n = rng.range(-14, 14);
      const col = { r: clamp(base.r + n), g: clamp(base.g + n * 0.9), b: clamp(base.b + n * 0.75) };
      put(ctx, x + 2, y + 2, cw - 3, ch - 3, col);
      ctx.fillStyle = "rgba(255,255,255,0.08)";
      ctx.fillRect(x + 2, y + 2, cw - 3, 3);
      ctx.fillRect(x + 2, y + 2, 3, ch - 3);
      ctx.fillStyle = "rgba(0,0,0,0.16)";
      ctx.fillRect(x + 2, y + ch - 4, cw - 3, 3);
      ctx.fillRect(x + cw - 4, y + 2, 3, ch - 3);
      x += cw;
    }
    y += ch;
  }
  const p = pair(c);
  cache.set(key, p);
  return p;
}

export function makeStucco(color, seed) {
  const key = `stucco:${color}:${seed}`;
  if (cache.has(key)) return cache.get(key);
  const { c, ctx, size } = canvas(512);
  const base = rgb(color);
  const img = ctx.createImageData(size, size);
  const d = img.data;
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const n = fbm(x * 0.035, y * 0.035, 5);
      const s = (n - 0.5) * 28;
      const blot = fbm(x * 0.01 + 20, y * 0.01, 3);
      const i = (y * size + x) * 4;
      d[i] = clamp(base.r + s + blot * 8);
      d[i + 1] = clamp(base.g + s + blot * 6);
      d[i + 2] = clamp(base.b + s * 0.8);
      d[i + 3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
  const p = pair(c);
  cache.set(key, p);
  return p;
}

export function makeClapboard(color, seed) {
  const key = `clap:${color}:${seed}`;
  if (cache.has(key)) return cache.get(key);
  const { c, ctx, size } = canvas(512);
  const rng = new RNG(seed);
  const base = rgb(color);
  const board = 22;
  for (let y = 0; y < size; y += board) {
    const n = rng.range(-10, 10);
    put(ctx, 0, y, size, board, { r: base.r + n, g: base.g + n, b: base.b + n * 0.8 });
    ctx.fillStyle = "rgba(0,0,0,0.22)";
    ctx.fillRect(0, y + board - 3, size, 3);
    ctx.fillStyle = "rgba(255,255,255,0.06)";
    ctx.fillRect(0, y, size, 2);
    for (let x = 0; x < size; x += rng.int(80, 160)) {
      ctx.fillStyle = "rgba(0,0,0,0.08)";
      ctx.fillRect(x, y, 2, board);
    }
  }
  const img = ctx.getImageData(0, 0, size, size);
  const d = img.data;
  for (let i = 0; i < d.length; i += 4) {
    const x = (i / 4) % size;
    const y = ((i / 4) / size) | 0;
    const g = (fbm(x * 0.2, y * 0.04) - 0.5) * 12;
    d[i] = clamp(d[i] + g);
    d[i + 1] = clamp(d[i + 1] + g);
    d[i + 2] = clamp(d[i + 2] + g);
  }
  ctx.putImageData(img, 0, 0);
  const p = pair(c);
  cache.set(key, p);
  return p;
}

export function makeSlate(color, seed) {
  const key = `slate:${color}:${seed}`;
  if (cache.has(key)) return cache.get(key);
  const { c, ctx, size } = canvas(512);
  const rng = new RNG(seed);
  const base = rgb(color);
  put(ctx, 0, 0, size, size, mix(base, { r: 20, g: 20, b: 24 }, 0.4));
  const tw = 46;
  const th = 28;
  let row = 0;
  for (let y = -th; y < size + th; y += th * 0.72) {
    const off = row % 2 ? tw / 2 : 0;
    for (let x = -tw; x < size + tw; x += tw) {
      const n = rng.range(-16, 16);
      const col = { r: clamp(base.r + n), g: clamp(base.g + n), b: clamp(base.b + n + rng.range(-6, 10)) };
      ctx.beginPath();
      ctx.moveTo(x + off + 2, y + th);
      ctx.lineTo(x + off + tw * 0.5, y + 2);
      ctx.lineTo(x + off + tw - 2, y + th);
      ctx.closePath();
      ctx.fillStyle = `rgb(${col.r},${col.g},${col.b})`;
      ctx.fill();
    }
    row++;
  }
  const p = pair(c);
  cache.set(key, p);
  return p;
}

export function makeTerracotta(color, seed) {
  const key = `terra:${color}:${seed}`;
  if (cache.has(key)) return cache.get(key);
  const { c, ctx, size } = canvas(512);
  const rng = new RNG(seed);
  const base = rgb(color);
  const rowH = 28;
  for (let y = 0; y < size; y += rowH) {
    const off = ((y / rowH) | 0) % 2 ? 18 : 0;
    for (let x = -20; x < size + 20; x += 36) {
      const n = rng.range(-20, 16);
      const col = { r: clamp(base.r + n), g: clamp(base.g + n * 0.6), b: clamp(base.b + n * 0.3) };
      const px = x + off;
      const grd = ctx.createLinearGradient(px, y, px + 18, y);
      grd.addColorStop(0, `rgb(${col.r * 0.55 | 0},${col.g * 0.55 | 0},${col.b * 0.55 | 0})`);
      grd.addColorStop(0.5, `rgb(${clamp(col.r + 20)},${clamp(col.g + 10)},${col.b})`);
      grd.addColorStop(1, `rgb(${col.r * 0.55 | 0},${col.g * 0.55 | 0},${col.b * 0.55 | 0})`);
      ctx.fillStyle = grd;
      ctx.fillRect(px, y + 2, 20, rowH - 4);
    }
  }
  const p = pair(c);
  cache.set(key, p);
  return p;
}

export function makeShingle(color, seed) {
  const key = `shingle:${color}:${seed}`;
  if (cache.has(key)) return cache.get(key);
  const { c, ctx, size } = canvas(512);
  const rng = new RNG(seed);
  const base = rgb(color);
  put(ctx, 0, 0, size, size, mix(base, { r: 20, g: 16, b: 12 }, 0.4));
  const tw = 36;
  const th = 20;
  let row = 0;
  for (let y = 0; y < size + th; y += th * 0.7) {
    const off = row % 2 ? tw / 2 : 0;
    for (let x = -tw; x < size; x += tw) {
      const n = rng.range(-14, 14);
      put(ctx, x + off + 1, y, tw - 2, th - 2, { r: clamp(base.r + n), g: clamp(base.g + n), b: clamp(base.b + n) });
      ctx.fillStyle = "rgba(0,0,0,0.25)";
      ctx.fillRect(x + off + 1, y + th - 4, tw - 2, 3);
    }
    row++;
  }
  const p = pair(c);
  cache.set(key, p);
  return p;
}

export function makeFishScale(color, seed) {
  const key = `fish:${color}:${seed}`;
  if (cache.has(key)) return cache.get(key);
  const { c, ctx, size } = canvas(512);
  const rng = new RNG(seed);
  const base = rgb(color);
  put(ctx, 0, 0, size, size, base);
  const r = 16;
  let row = 0;
  for (let y = 0; y < size + r; y += r * 1.2) {
    const off = row % 2 ? r : 0;
    for (let x = -r; x < size + r; x += r * 2) {
      const n = rng.range(-12, 12);
      ctx.beginPath();
      ctx.arc(x + off, y, r, 0, Math.PI);
      ctx.fillStyle = `rgb(${clamp(base.r + n)},${clamp(base.g + n)},${clamp(base.b + n)})`;
      ctx.fill();
      ctx.strokeStyle = "rgba(0,0,0,0.25)";
      ctx.stroke();
    }
    row++;
  }
  const p = pair(c);
  cache.set(key, p);
  return p;
}

export function makeConcrete(color, seed) {
  const key = `conc:${color}:${seed}`;
  if (cache.has(key)) return cache.get(key);
  const { c, ctx, size } = canvas(512);
  const base = rgb(color);
  const img = ctx.createImageData(size, size);
  const d = img.data;
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const n = fbm(x * 0.04, y * 0.04, 5);
      const stain = fbm(x * 0.008 + 9, y * 0.02, 3);
      const i = (y * size + x) * 4;
      let r = base.r + (n - 0.5) * 30 + stain * 10;
      let g = base.g + (n - 0.5) * 28 + stain * 8;
      let b = base.b + (n - 0.5) * 24;
      if (x % 128 < 2 || y % 96 < 2) {
        r -= 18;
        g -= 18;
        b -= 16;
      }
      d[i] = clamp(r);
      d[i + 1] = clamp(g);
      d[i + 2] = clamp(b);
      d[i + 3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
  ctx.fillStyle = "rgba(30,30,28,0.35)";
  for (let y = 24; y < size; y += 96) {
    for (let x = 24; x < size; x += 128) {
      ctx.beginPath();
      ctx.arc(x, y, 3, 0, Math.PI * 2);
      ctx.fill();
    }
  }
  const p = pair(c);
  cache.set(key, p);
  return p;
}

export function makeZinc(color, seed) {
  const key = `zinc:${color}:${seed}`;
  if (cache.has(key)) return cache.get(key);
  const { c, ctx, size } = canvas(512);
  const base = rgb(color);
  put(ctx, 0, 0, size, size, base);
  const img = ctx.getImageData(0, 0, size, size);
  const d = img.data;
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const n = fbm(x * 0.08, y * 0.02, 4);
      const seam = x % 42 < 3 ? -30 : 0;
      const i = (y * size + x) * 4;
      d[i] = clamp(base.r + (n - 0.5) * 22 + seam);
      d[i + 1] = clamp(base.g + (n - 0.5) * 22 + seam);
      d[i + 2] = clamp(base.b + (n - 0.5) * 24 + seam);
    }
  }
  ctx.putImageData(img, 0, 0);
  const p = pair(c);
  cache.set(key, p);
  return p;
}

export function makeWood(color, seed) {
  const key = `wood:${color}:${seed}`;
  if (cache.has(key)) return cache.get(key);
  const { c, ctx, size } = canvas(512);
  const base = rgb(color);
  const img = ctx.createImageData(size, size);
  const d = img.data;
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const grain = Math.sin(y * 0.12 + fbm(x * 0.08, y * 0.02) * 8) * 10;
      const n = fbm(x * 0.15, y * 0.03, 4);
      const i = (y * size + x) * 4;
      d[i] = clamp(base.r + grain + (n - 0.5) * 20);
      d[i + 1] = clamp(base.g + grain * 0.8 + (n - 0.5) * 16);
      d[i + 2] = clamp(base.b + grain * 0.5 + (n - 0.5) * 10);
      d[i + 3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
  const p = pair(c);
  cache.set(key, p);
  return p;
}

export function makeGrass(seed) {
  const key = `grass:${seed}`;
  if (cache.has(key)) return cache.get(key);
  const { c, ctx, size } = canvas(512);
  const img = ctx.createImageData(size, size);
  const d = img.data;
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const n = fbm(x * 0.06, y * 0.06, 5);
      const i = (y * size + x) * 4;
      d[i] = clamp(48 + n * 40);
      d[i + 1] = clamp(78 + n * 55);
      d[i + 2] = clamp(32 + n * 22);
      d[i + 3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
  const p = pair(c);
  cache.set(key, p);
  return p;
}

export function makeFlagstone(seed) {
  const key = `flag:${seed}`;
  if (cache.has(key)) return cache.get(key);
  const { c, ctx, size } = canvas(512);
  const rng = new RNG(seed);
  put(ctx, 0, 0, size, size, { r: 92, g: 90, b: 84 });
  for (let y = 0; y < size; y += 48) {
    const off = ((y / 48) | 0) % 2 ? 32 : 0;
    for (let x = -20; x < size; x += 64) {
      const n = rng.range(-12, 12);
      put(ctx, x + off + 2, y + 2, 58, 42, { r: 140 + n, g: 136 + n, b: 126 + n });
      ctx.fillStyle = "rgba(255,255,255,0.07)";
      ctx.fillRect(x + off + 2, y + 2, 58, 3);
    }
  }
  const p = pair(c);
  cache.set(key, p);
  return p;
}

export function makeHedge(seed) {
  const key = `hedge:${seed}`;
  if (cache.has(key)) return cache.get(key);
  const { c, ctx, size } = canvas(256);
  const img = ctx.createImageData(size, size);
  const d = img.data;
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const n = fbm(x * 0.12, y * 0.12, 4);
      const i = (y * size + x) * 4;
      d[i] = clamp(28 + n * 50);
      d[i + 1] = clamp(62 + n * 70);
      d[i + 2] = clamp(22 + n * 30);
      d[i + 3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
  const p = pair(c);
  cache.set(key, p);
  return p;
}

export function makeFlutes(seed) {
  const key = `flute:${seed}`;
  if (cache.has(key)) return cache.get(key);
  const { c, ctx, size } = canvas(256);
  const img = ctx.createImageData(size, size);
  const d = img.data;
  const flutes = 20;
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const u = x / size;
      const wave = Math.pow(Math.abs(Math.sin(u * Math.PI * flutes)), 0.55);
      const v = 210 - wave * 90;
      const i = (y * size + x) * 4;
      d[i] = d[i + 1] = d[i + 2] = v;
      d[i + 3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
  const map = texFromCanvas(c);
  map.colorSpace = THREE.NoColorSpace;
  const p = { map, bump: map };
  cache.set(key, p);
  return p;
}

function clamp(n) {
  return n < 0 ? 0 : n > 255 ? 255 : n;
}

function wallByKind(kind, color, mortar, seed) {
  switch (kind) {
    case "brick":
      return makeBrick(color, mortar, seed);
    case "ashlar":
      return makeAshlar(color, seed);
    case "stucco":
      return makeStucco(color, seed);
    case "clapboard":
      return makeClapboard(color, seed);
    case "concrete":
      return makeConcrete(color, seed);
    case "fish":
      return makeFishScale(color, seed);
    default:
      return makeStucco(color, seed);
  }
}

function roofByKind(kind, color, seed) {
  switch (kind) {
    case "slate":
      return makeSlate(color, seed);
    case "terracotta":
      return makeTerracotta(color, seed);
    case "shingle":
      return makeShingle(color, seed);
    case "zinc":
      return makeZinc(color, seed);
    case "concrete":
      return makeConcrete(color, seed);
    default:
      return makeSlate(color, seed);
  }
}

export function createMaterials(style, rng) {
  const seed = rng.int(1, 1e9);
  const wallCol = rng.pick(style.colors.wall);
  const wall = wallByKind(style.wall, wallCol, style.colors.mortar, seed);
  const base = wallByKind(style.base, style.colors.base, style.colors.mortar, seed + 11);
  const roof = roofByKind(style.roofTex, style.colors.roof, seed + 23);
  const wood = makeWood(style.colors.wood || "#5a3b24", seed + 41);
  const trimTex = makeStucco(style.colors.trim, seed + 7);
  const floorWood = makeWood(style.colors.floor || "#6b5340", seed + 61);
  const stone = makeAshlar(style.colors.base, seed + 77);
  const flute = makeFlutes(seed);

  const std = (opts) => new THREE.MeshStandardMaterial(opts);

  const mats = {
    wall: std({
      map: wall.map,
      bumpMap: wall.bump,
      bumpScale: 0.035,
      roughness: 0.92,
      metalness: 0.02,
      color: 0xffffff,
    }),
    wallAlt: std({
      map: style.wallAlt ? wallByKind(style.wallAlt, style.colors.accent, style.colors.mortar, seed + 3).map : wall.map,
      bumpMap: wall.bump,
      bumpScale: 0.03,
      roughness: 0.9,
    }),
    base: std({
      map: base.map,
      bumpMap: base.bump,
      bumpScale: 0.05,
      roughness: 0.88,
    }),
    trim: std({
      map: trimTex.map,
      color: style.colors.trim,
      roughness: 0.55,
      metalness: 0.02,
    }),
    roof: std({
      map: roof.map,
      bumpMap: roof.bump,
      bumpScale: 0.06,
      roughness: style.roofTex === "zinc" ? 0.35 : 0.86,
      metalness: style.roofTex === "zinc" ? 0.45 : 0.04,
    }),
    wood: std({
      map: wood.map,
      bumpMap: wood.bump,
      bumpScale: 0.02,
      roughness: 0.72,
    }),
    floor: std({
      map: floorWood.map,
      roughness: 0.65,
    }),
    stone: std({
      map: stone.map,
      bumpMap: stone.bump,
      bumpScale: 0.04,
      roughness: 0.86,
    }),
    frame: std({
      color: style.colors.frame,
      roughness: 0.4,
      metalness: style.window === "ribbon" || style.window === "metal" ? 0.55 : 0.05,
    }),
    door: std({
      map: wood.map,
      color: style.colors.door,
      roughness: 0.55,
    }),
    column: std({
      color: style.colors.column || style.colors.trim,
      map: flute.map,
      bumpMap: flute.bump,
      bumpScale: 0.08,
      roughness: 0.42,
    }),
    columnPlain: std({
      color: style.colors.column || style.colors.trim,
      roughness: 0.45,
    }),
    glass: std({
      color: 0x8fb0c4,
      roughness: 0.08,
      metalness: 0.35,
      transparent: true,
      opacity: 0.38,
      envMapIntensity: 1.4,
    }),
    glassLit: std({
      color: 0xffd7a1,
      emissive: 0xffc56b,
      emissiveIntensity: 0,
      roughness: 0.2,
      metalness: 0.1,
      transparent: true,
      opacity: 0.55,
    }),
    interior: std({
      color: style.colors.interior || "#cbb89a",
      roughness: 0.9,
      emissive: 0xffcc77,
      emissiveIntensity: 0,
      polygonOffset: true,
      polygonOffsetFactor: 1,
      polygonOffsetUnits: 1,
    }),
    interiorLit: std({
      color: "#e8c27a",
      roughness: 0.8,
      emissive: 0xffb24a,
      emissiveIntensity: 0,
    }),
    iron: std({ color: 0x1c1d20, roughness: 0.4, metalness: 0.7 }),
    metal: std({ color: style.colors.accent || "#b8943e", roughness: 0.3, metalness: 0.75 }),
    slab: std({ color: 0x8d877c, roughness: 0.85, map: stone.map }),
    plaster: std({ color: style.colors.trim, roughness: 0.7 }),
    dark: std({ color: 0x1a1816, roughness: 0.9 }),
    hedge: std({
      map: makeHedge(seed).map,
      roughness: 0.95,
    }),
    grass: std({
      map: makeGrass(seed).map,
      roughness: 0.95,
    }),
    path: std({
      map: makeFlagstone(seed).map,
      bumpMap: makeFlagstone(seed).bump,
      bumpScale: 0.03,
      roughness: 0.9,
    }),
    timber: std({
      map: wood.map,
      color: style.colors.timber || "#2a2118",
      roughness: 0.8,
    }),
  };

  mats._lit = [mats.glassLit, mats.interiorLit];
  mats._all = Object.values(mats).filter((m) => m && m.isMaterial);
  return mats;
}

export function setNightAmount(mats, t) {
  const night = Math.max(0, (t - 0.62) / 0.38);
  mats.glassLit.emissiveIntensity = night * 1.4;
  mats.interiorLit.emissiveIntensity = night * 1.1;
  mats.interior.emissiveIntensity = night * 0.15;
}
