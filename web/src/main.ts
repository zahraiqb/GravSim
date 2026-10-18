import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import { BodyMeta, Gravsim, Kind, ScenarioDefaults } from "./sim";

const MAX_STEPS_PER_FRAME = 400;   // cost cap: keeps the UI responsive at high speed
const TRAIL_POINTS = 1500;
const SUBFRAMES = 4;               // trail samples per rendered frame

const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;

const NAME_COLORS: Record<string, number> = {
  Sun: 0xffd27a, Mercury: 0xb5b5b5, Venus: 0xe8c98a, Earth: 0x4f8cff, Mars: 0xd4603a,
  Jupiter: 0xd9a066, Saturn: 0xe6d28a, Uranus: 0x9fe0e8, Neptune: 0x4b6cff, Pluto: 0xc9b8a0, Halley: 0xa8f0ff,
};
const STAR_PALETTE = [0xffe3a3, 0xffb27a, 0xa3c4ff];

interface Visual { mesh: THREE.Object3D; trail: THREE.Line; trailPos: Float32Array; trailCount: number; label: HTMLDivElement; color: number }

class App {
  renderer = new THREE.WebGLRenderer({ canvas: $("view"), antialias: true });
  scene = new THREE.Scene();
  camera = new THREE.PerspectiveCamera(50, 1, 0.01, 5000);
  controls: OrbitControls;
  grid: THREE.GridHelper;
  visuals: Visual[] = [];
  bodies: BodyMeta[] = [];
  defaults!: ScenarioDefaults;
  scale = 1;           // scene units per length unit
  paused = false;
  last = performance.now();
  frame = 0;

  constructor(private sim: Gravsim) {
    this.camera.up.set(0, 0, 1);               // ecliptic north is +z
    this.camera.position.set(0, -14, 9);
    this.controls = new OrbitControls(this.camera, this.renderer.domElement);
    this.controls.enableDamping = true;
    this.scene.background = new THREE.Color(0x05060a);
    this.grid = new THREE.GridHelper(20, 20, 0x2a3350, 0x161b2b);
    this.grid.rotation.x = Math.PI / 2;        // GridHelper is XZ by default; we want XY
    this.scene.add(this.grid);
    this.addStars();
    window.addEventListener("resize", () => this.resize());
    this.resize();
    this.bindUI();
    this.loadScenario(0);
    requestAnimationFrame((t) => this.tick(t));
  }

  addStars() {
    const g = new THREE.BufferGeometry();
    const n = 1500, p = new Float32Array(n * 3);
    for (let i = 0; i < n; i++) {
      const v = new THREE.Vector3().randomDirection().multiplyScalar(2000);
      p.set([v.x, v.y, v.z], i * 3);
    }
    g.setAttribute("position", new THREE.BufferAttribute(p, 3));
    this.scene.add(new THREE.Points(g, new THREE.PointsMaterial({ color: 0x8890a8, size: 1.2, sizeAttenuation: false })));
  }

  resize() {
    const w = window.innerWidth, h = window.innerHeight;
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    this.renderer.setSize(w, h, false);
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();
  }

  bindUI() {
    const sel = $<HTMLSelectElement>("scenario");
    this.sim.scenarios.forEach((s) => sel.add(new Option(s.title, String(s.index))));
    sel.onchange = () => this.loadScenario(Number(sel.value));
    $<HTMLSelectElement>("method").onchange = (e) => this.sim.setMethod(Number((e.target as HTMLSelectElement).value));
    $<HTMLInputElement>("dt").oninput = () => this.applyDt();
    $<HTMLInputElement>("warp").oninput = () => this.updateWarpLabel();
    $<HTMLInputElement>("pn").onchange = (e) => { this.sim.setPN((e.target as HTMLInputElement).checked); };
    $<HTMLInputElement>("trails").onchange = () => this.updateTrailVisibility();
    $<HTMLInputElement>("labelsOn").onchange = () => this.updateLabelVisibility();
    $<HTMLButtonElement>("pause").onclick = () => {
      this.paused = !this.paused;
      $("pause").textContent = this.paused ? "Resume" : "Pause";
    };
    $<HTMLButtonElement>("reset").onclick = () => { this.sim.reset(); this.clearTrails(); };
  }

  get dt() { return this.defaults.dt * 10 ** Number($<HTMLInputElement>("dt").value); }
  get warp() { return this.defaults.warp * 10 ** Number($<HTMLInputElement>("warp").value); }
  get unit() { return this.defaults.timeUnit === "day" ? "d" : "t"; }

  applyDt() { this.sim.setDt(this.dt); $("dtOut").textContent = `${this.dt.toPrecision(3)} ${this.unit}`; }
  updateWarpLabel() { $("warpOut").textContent = `${this.warp.toPrecision(3)} ${this.unit}/s`; }

  loadScenario(index: number) {
    for (const v of this.visuals) { this.scene.remove(v.mesh); this.scene.remove(v.trail); v.label.remove(); }
    this.visuals = [];
    const { bodies, defaults } = this.sim.load(index);
    this.bodies = bodies;
    this.defaults = defaults;
    this.scale = 10 / defaults.viewScale;
    $("desc").textContent = this.sim.scenarios[index].description;
    $<HTMLSelectElement>("method").value = String(defaults.method);
    $<HTMLInputElement>("pn").checked = defaults.pn;
    $<HTMLInputElement>("dt").value = "0";
    $<HTMLInputElement>("warp").value = "0";
    this.applyDt();
    this.updateWarpLabel();

    let starIdx = 0;
    bodies.forEach((b, i) => {
      const color = NAME_COLORS[b.name] ?? (b.kind === Kind.Star ? STAR_PALETTE[starIdx++ % 3] : b.kind === Kind.Planet ? 0x6aa9ff : 0xffffff);
      const r = 0.06 * (1 + 0.25 * Math.log10(1 + b.mass / 1e-6));
      let mesh: THREE.Object3D;
      if (b.kind === Kind.BlackHole) {
        const g = new THREE.Group();
        g.add(new THREE.Mesh(new THREE.SphereGeometry(r, 24, 16), new THREE.MeshBasicMaterial({ color: 0x000000 })));
        const ring = new THREE.Mesh(new THREE.TorusGeometry(r * 1.9, r * 0.18, 8, 48), new THREE.MeshBasicMaterial({ color: 0xff9a3c }));
        g.add(ring);
        mesh = g;
      } else {
        mesh = new THREE.Mesh(new THREE.SphereGeometry(r, 20, 14), new THREE.MeshBasicMaterial({ color }));
      }
      this.scene.add(mesh);
      const trailPos = new Float32Array(TRAIL_POINTS * 3);
      const tg = new THREE.BufferGeometry();
      tg.setAttribute("position", new THREE.BufferAttribute(trailPos, 3));
      tg.setDrawRange(0, 0);
      const trail = new THREE.Line(tg, new THREE.LineBasicMaterial({ color, transparent: true, opacity: 0.55 }));
      trail.frustumCulled = false;
      this.scene.add(trail);
      const label = document.createElement("div");
      label.className = "label";
      label.textContent = b.name;
      $("labels").appendChild(label);
      this.visuals.push({ mesh, trail, trailPos, trailCount: 0, label, color });
      void i;
    });

    const follow = $<HTMLSelectElement>("follow");
    follow.innerHTML = "";
    follow.add(new Option("Barycenter", "-1"));
    bodies.forEach((b, i) => follow.add(new Option(b.name, String(i))));
    this.camera.position.set(0, -14, 9);
    this.controls.target.set(0, 0, 0);
    this.syncBodies();
    this.updateTrailVisibility();
    this.updateLabelVisibility();
  }

  syncBodies() {
    const p = this.sim.positions();
    this.visuals.forEach((v, i) => v.mesh.position.set(p[3 * i] * this.scale, p[3 * i + 1] * this.scale, p[3 * i + 2] * this.scale));
    return p;
  }

  pushTrails(p: Float64Array) {
    this.visuals.forEach((v, i) => {
      if (v.trailCount === TRAIL_POINTS) v.trailPos.copyWithin(0, 3);
      else v.trailCount++;
      const o = (v.trailCount - 1) * 3;
      v.trailPos[o] = p[3 * i] * this.scale;
      v.trailPos[o + 1] = p[3 * i + 1] * this.scale;
      v.trailPos[o + 2] = p[3 * i + 2] * this.scale;
      const attr = v.trail.geometry.getAttribute("position") as THREE.BufferAttribute;
      attr.needsUpdate = true;
      v.trail.geometry.setDrawRange(0, v.trailCount);
    });
  }

  clearTrails() { for (const v of this.visuals) { v.trailCount = 0; v.trail.geometry.setDrawRange(0, 0); } this.syncBodies(); }
  updateTrailVisibility() { const on = $<HTMLInputElement>("trails").checked; for (const v of this.visuals) v.trail.visible = on; }
  updateLabelVisibility() { const on = $<HTMLInputElement>("labelsOn").checked; for (const v of this.visuals) v.label.style.display = on ? "" : "none"; }

  fmtTime(t: number) {
    return this.defaults.timeUnit === "day" ? `${(t / 365.25).toFixed(2)} yr` : `t = ${t.toFixed(3)}`;
  }

  tick(now: number) {
    const frameSec = Math.min(0.1, (now - this.last) / 1000);
    this.last = now;
    let limited = false;
    if (!this.paused) {
      const wanted = this.warp * frameSec;
      const cap = this.dt * MAX_STEPS_PER_FRAME;
      limited = wanted > cap;
      const adv = Math.min(wanted, cap);
      for (let k = 0; k < SUBFRAMES; k++) {
        this.sim.advance(adv / SUBFRAMES);
        if ($<HTMLInputElement>("trails").checked) this.pushTrails(this.sim.positions());
      }
    }
    const p = this.syncBodies();

    // camera follow
    const f = Number($<HTMLSelectElement>("follow").value);
    const target = f >= 0 ? new THREE.Vector3(p[3 * f] * this.scale, p[3 * f + 1] * this.scale, p[3 * f + 2] * this.scale) : new THREE.Vector3();
    const delta = target.clone().sub(this.controls.target);
    this.controls.target.add(delta);
    this.camera.position.add(delta);
    this.controls.update();

    // labels
    if ($<HTMLInputElement>("labelsOn").checked) {
      const w = window.innerWidth, h = window.innerHeight, v = new THREE.Vector3();
      for (const vis of this.visuals) {
        v.copy(vis.mesh.position).project(this.camera);
        const visible = v.z < 1;
        vis.label.style.display = visible ? "" : "none";
        vis.label.style.left = `${(v.x * 0.5 + 0.5) * w}px`;
        vis.label.style.top = `${(-v.y * 0.5 + 0.5) * h}px`;
      }
    }

    if (this.frame++ % 6 === 0) {
      const d = this.sim.diagnostics();
      $("dTime").textContent = this.fmtTime(d.time);
      $("dE").textContent = d.dE.toExponential(2);
      $("dL").textContent = d.dL.toExponential(2);
      $("dEv").textContent = d.evals.toExponential(2);
      $("limited").textContent = limited ? "Speed limited by CPU budget; lower the time step cost or speed." : "";
    }
    this.renderer.render(this.scene, this.camera);
    requestAnimationFrame((t) => this.tick(t));
  }
}

async function main() {
  const status = $("status");
  try {
    const sim = await Gravsim.create();
    status.textContent = "Physics core: C++20 → WebAssembly";
    new App(sim);
  } catch (err) {
    status.textContent =
      "Could not load the WebAssembly module. Build it first (scripts/build-wasm.sh) – see README. " + String(err);
  }
}
main();
