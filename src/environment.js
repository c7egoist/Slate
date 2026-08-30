import * as THREE from "three";
import { Sky } from "three/examples/jsm/objects/Sky.js";
import { RoomEnvironment } from "three/examples/jsm/environments/RoomEnvironment.js";
import { Builder } from "./builder.js";
import { makeGrass, makeFlagstone, makeHedge } from "./textures.js";

export function createEnvironment(renderer, scene) {
  const sky = new Sky();
  sky.scale.setScalar(450000);
  scene.add(sky);

  const sun = new THREE.Vector3();
  const hemi = new THREE.HemisphereLight(0xb8d0ea, 0x3d3328, 0.55);
  scene.add(hemi);

  const dir = new THREE.DirectionalLight(0xfff1d6, 2.2);
  dir.castShadow = true;
  dir.shadow.mapSize.set(2048, 2048);
  dir.shadow.camera.near = 1;
  dir.shadow.camera.far = 80;
  dir.shadow.camera.left = -30;
  dir.shadow.camera.right = 30;
  dir.shadow.camera.top = 30;
  dir.shadow.camera.bottom = -30;
  dir.shadow.bias = -0.0004;
  scene.add(dir);
  scene.add(dir.target);

  const fill = new THREE.DirectionalLight(0x8fb4d8, 0.35);
  scene.add(fill);

  const pmrem = new THREE.PMREMGenerator(renderer);
  scene.environment = pmrem.fromScene(new RoomEnvironment(), 0.04).texture;

  const site = buildSite();
  scene.add(site);

  scene.fog = new THREE.FogExp2(0xc5d4e0, 0.012);

  return { sky, sun, hemi, dir, fill, site };
}

function buildSite() {
  const b = new Builder();
  const grass = makeGrass(7);
  const path = makeFlagstone(11);
  const hedge = makeHedge(3);
  const grassMat = new THREE.MeshStandardMaterial({ map: grass.map, roughness: 0.95 });
  const pathMat = new THREE.MeshStandardMaterial({
    map: path.map,
    bumpMap: path.bump,
    bumpScale: 0.03,
    roughness: 0.9,
  });
  const hedgeMat = new THREE.MeshStandardMaterial({ map: hedge.map, roughness: 0.95 });
  const curbMat = new THREE.MeshStandardMaterial({ color: 0x9a958c, roughness: 0.85 });
  const roadMat = new THREE.MeshStandardMaterial({ color: 0x3a3c40, roughness: 0.92 });
  const bark = new THREE.MeshStandardMaterial({ color: 0x4a3426, roughness: 0.9 });
  const leaf = new THREE.MeshStandardMaterial({ color: 0x3d6a42, roughness: 0.85 });

  b.box(grassMat, 48, 0.08, 36, 0, -0.04, -2, 0, 0, 0, 0.15);
  b.box(roadMat, 48, 0.06, 8, 0, -0.02, 16, 0, 0, 0, 0.08);
  b.box(curbMat, 48, 0.14, 0.35, 0, 0.04, 12.1);
  b.box(pathMat, 2.2, 0.05, 8.5, 0, 0.03, 7.2, 0, 0, 0, 0.4);
  b.box(pathMat, 18, 0.05, 2.4, 0, 0.03, 11.2, 0, 0, 0, 0.4);

  b.box(hedgeMat, 7.5, 0.85, 0.45, -6.2, 0.45, 5.4);
  b.box(hedgeMat, 7.5, 0.85, 0.45, 6.2, 0.45, 5.4);
  b.box(hedgeMat, 0.45, 0.7, 4.2, -10.5, 0.38, 2.2);
  b.box(hedgeMat, 0.45, 0.7, 4.2, 10.5, 0.38, 2.2);

  addTree(b, bark, leaf, -9.5, -6.5, 1.1);
  addTree(b, bark, leaf, 10.2, -5.2, 0.95);
  addTree(b, bark, leaf, -11.5, 3.5, 0.75);
  addTree(b, bark, leaf, 12.4, 4.2, 0.8);

  const group = b.build();
  group.name = "site";
  return group;
}

function addTree(b, bark, leaf, x, z, s) {
  b.cyl(bark, 0.14 * s, 0.22 * s, 1.6 * s, x, 0.8 * s, z, 8);
  b.sphere(leaf, 1.15 * s, x, 2.1 * s, z, 10);
  b.sphere(leaf, 0.75 * s, x + 0.55 * s, 2.0 * s, z + 0.2 * s, 8);
  b.sphere(leaf, 0.7 * s, x - 0.45 * s, 2.25 * s, z - 0.15 * s, 8);
}

export function applyTimeOfDay(env, renderer, scene, t) {
  const hour = 6 + t * 16;
  const elev = THREE.MathUtils.degToRad(Math.max(2, Math.sin(t * Math.PI) * 62));
  const azim = THREE.MathUtils.degToRad(-70 + t * 160);
  env.sun.setFromSphericalCoords(1, Math.PI / 2 - elev, azim);
  const u = env.sky.material.uniforms;
  u["sunPosition"].value.copy(env.sun);
  u["turbidity"].value = 2 + t * 6;
  u["rayleigh"].value = 1.1 + (1 - Math.sin(t * Math.PI)) * 1.4;
  u["mieCoefficient"].value = 0.005;
  u["mieDirectionalG"].value = 0.8;

  env.dir.position.copy(env.sun).multiplyScalar(40);
  env.dir.target.position.set(0, 2, 0);
  env.fill.position.set(-env.sun.x * 20, 8, -env.sun.z * 12);

  const night = Math.max(0, (t - 0.72) / 0.28);
  const dusk = Math.max(0, 1 - Math.sin(t * Math.PI));
  env.dir.color.setHSL(0.12 - night * 0.05, 0.35 + dusk * 0.35, 0.95 - night * 0.4);
  env.dir.intensity = 2.4 * Math.sin(t * Math.PI) + 0.08;
  env.hemi.intensity = 0.55 * Math.sin(t * Math.PI) + 0.08;
  env.hemi.color.set(night > 0.4 ? 0x1a2438 : 0xb8d0ea);
  env.fill.intensity = 0.25 * (1 - night);

  const fogDay = new THREE.Color(0xc5d4e0);
  const fogNight = new THREE.Color(0x0b1020);
  scene.fog.color.lerpColors(fogDay, fogNight, night);
  scene.fog.density = 0.011 + night * 0.01;
  renderer.toneMappingExposure = 1.05 - night * 0.45 + dusk * 0.1;

  return night;
}

export const TIME_LABELS = [
  [0, "Dawn"],
  [0.12, "Morning"],
  [0.28, "Afternoon"],
  [0.5, "Golden hour"],
  [0.68, "Dusk"],
  [0.84, "Night"],
];

export function timeLabel(t) {
  let label = TIME_LABELS[0][1];
  for (const [k, v] of TIME_LABELS) if (t >= k) label = v;
  return label;
}
