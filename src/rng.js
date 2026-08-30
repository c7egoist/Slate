export class RNG {
  constructor(seed) {
    this.seed = seed >>> 0 || 1;
    this._s = this.seed;
  }

  next() {
    this._s += 0x6d2b79f5;
    let t = this._s;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  }

  range(a, b) {
    return a + this.next() * (b - a);
  }

  int(a, b) {
    return Math.floor(this.range(a, b + 1));
  }

  chance(p) {
    return this.next() < p;
  }

  pick(arr) {
    return arr[Math.floor(this.next() * arr.length)];
  }

  sign() {
    return this.next() < 0.5 ? -1 : 1;
  }
}

export function hashString(str) {
  let h = 2166136261;
  for (let i = 0; i < str.length; i++) {
    h ^= str.charCodeAt(i);
    h = Math.imul(h, 16777619);
  }
  return h >>> 0;
}

export function parseSeed(value) {
  const s = String(value ?? "").trim();
  if (!s) return (Math.random() * 1e9) >>> 0;
  if (/^\d+$/.test(s)) return Number(s) >>> 0 || 1;
  return hashString(s) || 1;
}
