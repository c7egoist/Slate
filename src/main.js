import "./style.css";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import { STYLES, STYLE_LIST } from "./styles.js";
import { generateBuilding } from "./building.js";
import { disposeObject } from "./builder.js";
import { setAnisotropy, setNightAmount } from "./textures.js";
import { createEnvironment, applyTimeOfDay, timeLabel } from "./environment.js";
import { parseSeed } from "./rng.js";

const canvas = document.getElementById("view");
const renderer = new THREE.WebGLRenderer({
  canvas,
  antialias: true,
  preserveDrawingBuffer: true,
  powerPreference: "high-performance",
});
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.setSize(innerWidth, innerHeight);
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.PCFSoftShadowMap;
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.05;
renderer.outputColorSpace = THREE.SRGBColorSpace;
setAnisotropy(renderer.capabilities.getMaxAnisotropy());

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(32, innerWidth / innerHeight, 0.1, 400);
camera.position.set(16, 9, 22);

const controls = new OrbitControls(camera, canvas);
controls.enableDamping = true;
controls.dampingFactor = 0.05;
controls.maxPolarAngle = Math.PI * 0.48;
controls.minDistance = 8;
controls.maxDistance = 60;
controls.target.set(0, 4, 0);

const env = createEnvironment(renderer, scene);

const params = {
  seed: (Math.random() * 90000 + 10000) | 0,
  style: "georgian",
  floors: 3,
  baysX: 5,
  baysZ: 3,
  winH: 1,
  muntins: 2,
  columns: true,
  balconies: false,
  dormers: true,
  order: "style",
  roof: "style",
  time: 0.32,
};

let building = null;
let generating = false;
let lastFrameKey = "";

function readHash() {
  const h = new URLSearchParams(location.hash.replace(/^#/, ""));
  if (h.get("s")) params.seed = parseSeed(h.get("s"));
  if (h.get("style") && STYLES[h.get("style")]) params.style = h.get("style");
  if (h.get("floors")) params.floors = THREE.MathUtils.clamp(+h.get("floors"), 1, 8);
  if (h.get("bays")) params.baysX = THREE.MathUtils.clamp(+h.get("bays"), 3, 7);
}

function writeHash() {
  const h = new URLSearchParams();
  h.set("s", String(params.seed));
  h.set("style", params.style);
  h.set("floors", String(params.floors));
  h.set("bays", String(params.baysX));
  history.replaceState(null, "", `#${h.toString()}`);
}

function applyStyleDefaults(id, keepSeed = true) {
  const s = STYLES[id];
  params.style = id;
  params.floors = s.defaults.floors;
  params.baysX = s.defaults.baysX;
  params.baysZ = s.defaults.baysZ;
  params.columns = s.defaults.columns;
  params.balconies = s.defaults.balconies;
  params.dormers = s.defaults.dormers;
  syncInputs();
}

function rebuild() {
  if (generating) return;
  generating = true;
  if (building) {
    scene.remove(building);
    disposeObject(building);
    building = null;
  }
  let group;
  try {
    group = generateBuilding(params);
  } catch (err) {
    console.error(err);
    generating = false;
    return;
  }
  building = group;
  scene.add(building);
  const d = group.userData.dims;
  controls.target.set(0, d.totalH * 0.42, 0);
  const span = Math.max(d.width, d.depth, d.totalH);
  const dist = span * 2.15;
  const frameKey = `${params.style}-${params.floors}-${params.baysX}-${params.baysZ}-${params.seed}`;
  if (frameKey !== lastFrameKey) {
    camera.position.set(dist * 0.78, Math.max(6, d.totalH * 0.7), dist * 1.05);
    lastFrameKey = frameKey;
  } else {
    const cur = camera.position.clone().sub(controls.target);
    cur.setLength(THREE.MathUtils.clamp(cur.length(), dist * 0.65, dist * 1.8));
    camera.position.copy(controls.target).add(cur);
  }
  setNightAmount(group.userData.mats, params.time);
  renderSpecs(group.userData);
  writeHash();
  generating = false;
}

function renderSpecs(data) {
  const st = data.stats;
  const el = document.getElementById("specSheet");
  const cells = [
    ["Typology", st.style],
    ["Storeys", String(st.storeys)],
    ["Height", `${st.height.toFixed(1)} m`],
    ["GFA", `${Math.round(st.gfa)} m²`],
    ["Bays", st.bays],
    ["Seed", String(st.seed)],
  ];
  el.innerHTML = cells
    .map(([k, v]) => `<div class="spec"><div class="k">${k}</div><div class="v">${v}</div></div>`)
    .join("");
  document.getElementById("styleCaption").innerHTML = `
    <h1>${data.style.name}</h1>
    <div class="origin">${data.style.origin} · ${data.style.period}</div>
    <p>${data.style.summary}</p>
  `;
}

function syncInputs() {
  document.getElementById("seed").value = String(params.seed);
  document.getElementById("floors").value = params.floors;
  document.getElementById("floorsVal").textContent = params.floors;
  document.getElementById("baysX").value = params.baysX;
  document.getElementById("baysXVal").textContent = params.baysX;
  document.getElementById("baysZ").value = params.baysZ;
  document.getElementById("baysZVal").textContent = params.baysZ;
  document.getElementById("winH").value = params.winH;
  document.getElementById("winHVal").textContent = params.winH.toFixed(2);
  document.getElementById("muntins").value = params.muntins;
  document.getElementById("muntinVal").textContent = params.muntins;
  document.getElementById("columns").checked = params.columns;
  document.getElementById("balconies").checked = params.balconies;
  document.getElementById("dormers").checked = params.dormers;
  document.getElementById("order").value = params.order;
  document.getElementById("roof").value = params.roof;
  document.getElementById("time").value = Math.round(params.time * 100);
  document.getElementById("timeVal").textContent = timeLabel(params.time);
  document.querySelectorAll(".style-btn").forEach((btn) => {
    btn.classList.toggle("active", btn.dataset.id === params.style);
  });
}

function bind() {
  const list = document.getElementById("styles");
  for (const s of STYLE_LIST) {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "style-btn";
    btn.dataset.id = s.id;
    btn.innerHTML = `<span class="chip" style="--c:${s.colors.wall[0]}"></span><span class="name">${s.name}</span><span class="period">${s.period.split("–")[0]}</span>`;
    btn.addEventListener("click", () => {
      applyStyleDefaults(s.id);
      rebuild();
    });
    list.appendChild(btn);
  }

  const num = (id, key, map = Number) => {
    document.getElementById(id).addEventListener("input", (e) => {
      params[key] = map(e.target.value);
      syncInputs();
      rebuild();
    });
  };
  num("floors", "floors", (v) => +v);
  num("baysX", "baysX", (v) => +v);
  num("baysZ", "baysZ", (v) => +v);
  num("winH", "winH", (v) => +v);
  num("muntins", "muntins", (v) => +v);

  document.getElementById("columns").addEventListener("change", (e) => {
    params.columns = e.target.checked;
    rebuild();
  });
  document.getElementById("balconies").addEventListener("change", (e) => {
    params.balconies = e.target.checked;
    rebuild();
  });
  document.getElementById("dormers").addEventListener("change", (e) => {
    params.dormers = e.target.checked;
    rebuild();
  });
  document.getElementById("order").addEventListener("change", (e) => {
    params.order = e.target.value;
    rebuild();
  });
  document.getElementById("roof").addEventListener("change", (e) => {
    params.roof = e.target.value;
    rebuild();
  });
  document.getElementById("time").addEventListener("input", (e) => {
    params.time = +e.target.value / 100;
    document.getElementById("timeVal").textContent = timeLabel(params.time);
    applyTimeOfDay(env, renderer, scene, params.time);
    if (building) setNightAmount(building.userData.mats, params.time);
  });
  document.getElementById("seed").addEventListener("change", (e) => {
    params.seed = parseSeed(e.target.value);
    syncInputs();
    rebuild();
  });
  document.getElementById("seedRand").addEventListener("click", () => {
    params.seed = (Math.random() * 90000 + 10000) | 0;
    syncInputs();
    rebuild();
  });
  document.getElementById("randomize").addEventListener("click", () => {
    params.seed = (Math.random() * 90000 + 10000) | 0;
    const s = STYLE_LIST[(Math.random() * STYLE_LIST.length) | 0];
    applyStyleDefaults(s.id);
    params.floors = THREE.MathUtils.clamp(s.defaults.floors + (((Math.random() * 3) | 0) - 1), 1, 8);
    params.baysX = THREE.MathUtils.clamp(s.defaults.baysX + (((Math.random() * 3) | 0) - 1), 3, 7);
    syncInputs();
    rebuild();
  });
  document.getElementById("autorotate").addEventListener("change", (e) => {
    controls.autoRotate = e.target.checked;
    controls.autoRotateSpeed = 0.6;
  });
  document.getElementById("shot").addEventListener("click", () => {
    renderer.render(scene, camera);
    const a = document.createElement("a");
    a.download = `slate-${params.style}-${params.seed}.png`;
    a.href = renderer.domElement.toDataURL("image/png");
    a.click();
  });
  addEventListener("keydown", (e) => {
    if (e.target.matches("input, select, textarea")) return;
    if (e.key === "r") {
      params.seed = (Math.random() * 90000 + 10000) | 0;
      syncInputs();
      rebuild();
    }
  });
}

readHash();
bind();
if (!location.hash) applyStyleDefaults(params.style);
else {
  const s = STYLES[params.style];
  params.columns = s.defaults.columns;
  params.balconies = s.defaults.balconies;
  params.dormers = s.defaults.dormers;
}
syncInputs();
applyTimeOfDay(env, renderer, scene, params.time);
rebuild();

addEventListener("resize", () => {
  camera.aspect = innerWidth / innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight);
});

renderer.setAnimationLoop(() => {
  controls.update();
  renderer.render(scene, camera);
});
