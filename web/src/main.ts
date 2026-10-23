import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import { BodyMeta, Gravsim, Kind, ScenarioDefaults } from "./sim";
import { renderScenarios } from "./scenarios";
import { Pulsar } from "./pulsar";
import { glow, skin } from "./skins";

const MAX_STEPS_PER_FRAME = 400;   // cost cap: keeps the UI responsive at high speed
const TRAIL_POINTS = 1500;
const SUBFRAMES = 4;               // trail samples per rendered frame

const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;

const NAME_COLORS: Record<string, number> = {
  Sun: 0xffd27a, Mercury: 0xb5b5b5, Venus: 0xe8c98a, Earth: 0x4f8cff, Mars: 0xd4603a,
  Jupiter: 0xd9a066, Saturn: 0xe6d28a, Uranus: 0x9fe0e8, Neptune: 0x4b6cff, Pluto: 0xc9b8a0, Halley: 0xa8f0ff,
};
const STAR_PALETTE = [0xffe3a3, 0xffb27a, 0xa3c4ff];

interface Visual { mesh: THREE.Object3D; trail: THREE.Line; trailPos: Float32Array; trailCount: number; label: HTMLDivElement; color: number; hidden: boolean }

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
  active = false;
  fps = 60;
  cur = 0;
  pts?: THREE.Points;
  masses: number[] = [];
  lightIdx = -1;
  rs = 0;  // black-hole horizon radius in scenario length units (G=1 scenarios only)
  applyWells: () => void = () => {};
  light = new THREE.PointLight(0xffffff, 3, 0, 0);
  wells = new THREE.Mesh(new THREE.PlaneGeometry(28, 28, 56, 56), new THREE.MeshBasicMaterial({ color: 0x4f7fd0, wireframe: true, transparent: true, opacity: 0.45 }));
  bary = new THREE.LineSegments(new THREE.BufferGeometry().setFromPoints([[-.3,0,0],[.3,0,0],[0,-.3,0],[0,.3,0],[0,0,-.3],[0,0,.3]].map((v) => new THREE.Vector3(...v))), new THREE.LineBasicMaterial({ color: 0xffb454 }));

  constructor(private sim: Gravsim) {
    this.camera.up.set(0, 0, 1);               // ecliptic north is +z
    this.camera.position.set(0, -14, 9);
    this.controls = new OrbitControls(this.camera, this.renderer.domElement);
    this.controls.enableDamping = true;
    this.scene.background = new THREE.Color(0x05060a);
    this.grid = new THREE.GridHelper(20, 20, 0x2a3350, 0x161b2b);
    this.grid.rotation.x = Math.PI / 2;        // GridHelper is XZ by default; we want XY
    this.scene.add(this.grid);
    this.bary.visible = false;
    this.scene.add(this.bary);
    this.wells.visible = false;
    this.scene.add(this.wells, this.light, new THREE.AmbientLight(0x404868, 1.4));
    this.addStars();
    window.addEventListener("resize", () => this.resize());
    this.resize();
    this.bindUI();
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
    const flys = document.querySelectorAll<HTMLElement>(".fly"), tools = document.querySelectorAll<HTMLElement>("[data-tool]");
    tools.forEach((b) => (b.onclick = () => {
      const f = $(`fly-${b.dataset.tool}`), open = !f.classList.contains("open");
      flys.forEach((x) => x.classList.remove("open")); tools.forEach((x) => x.classList.remove("on"));
      if (open) { f.classList.add("open"); b.classList.add("on"); }
    }));
    const gw = () => {
      const w = $<HTMLInputElement>("wellsOn").checked;
      this.wells.visible = w && !this.pts;
      this.grid.visible = $<HTMLInputElement>("gridOn").checked && !w;
    };
    this.applyWells = gw;
    $<HTMLInputElement>("gridOn").onchange = gw;
    $<HTMLInputElement>("wellsOn").onchange = gw;
    $<HTMLInputElement>("baryOn").onchange = (e) => { this.bary.visible = (e.target as HTMLInputElement).checked; };
    $("viewTop").onclick = () => this.setView(0, -0.01, 1);
    $("viewSide").onclick = () => this.setView(0, -1, 0.05);
    $("viewReset").onclick = () => this.setView(0, -14 / 16.6, 9 / 16.6);
    $<HTMLSelectElement>("method").onchange = (e) => this.sim.setMethod(Number((e.target as HTMLSelectElement).value));
    $<HTMLInputElement>("dt").oninput = () => this.applyDt();
    $<HTMLInputElement>("warp").oninput = () => this.updateWarpLabel();
    $<HTMLInputElement>("pn").onchange = (e) => { this.sim.setPN((e.target as HTMLInputElement).checked); };
    $<HTMLInputElement>("trails").onchange = () => this.updateTrailVisibility();
    $<HTMLInputElement>("labelsOn").onchange = () => this.updateLabelVisibility();
    $<HTMLButtonElement>("pause").onclick = () => {
      this.setPaused(!this.paused);
    };
    $<HTMLButtonElement>("reset").onclick = () => this.loadScenario(this.cur);
  }

  setPaused(p: boolean) { this.paused = p; $("pause").classList.toggle("is-paused", p); }
  setView(x: number, y: number, z: number) {
    const d = Math.max(this.camera.position.distanceTo(this.controls.target), 1), t = this.controls.target;
    this.camera.position.set(t.x + x * d, t.y + y * d, t.z + z * d);
  }
  enter(i: number) { this.active = true; this.setPaused(false); this.loadScenario(i); }

  get dt() { return this.defaults.dt * 10 ** Number($<HTMLInputElement>("dt").value); }
  get warp() { return this.defaults.warp * 10 ** Number($<HTMLInputElement>("warp").value); }
  get unit() { return this.defaults.timeUnit === "day" ? "d" : "t"; }

  applyDt() { this.sim.setDt(this.dt); $("dtOut").textContent = `${this.dt.toPrecision(3)} ${this.unit}`; }
  updateWarpLabel() { $("warpOut").textContent = `${this.warp.toPrecision(3)} ${this.unit}/s`; }

  loadScenario(index: number) {
    for (const v of this.visuals) { this.scene.remove(v.mesh); this.scene.remove(v.trail); v.label.remove(); }
    this.visuals = [];
    if (this.pts) { this.scene.remove(this.pts); this.pts = undefined; }
    const { bodies, defaults } = this.sim.load(index);
    this.bodies = bodies;
    this.cur = index;
    this.masses = bodies.map((b) => b.mass);
    const many = bodies.length > 40;  // swarm mode: draw as points, no meshes/trails/labels
    const shown = many ? [] : bodies;
    const bh = bodies.find((b) => b.kind === Kind.BlackHole);
    this.rs = defaults.timeUnit === "G=1" && bh ? bh.radiusKm : 0;
    this.lightIdx = -1;
    bodies.forEach((b, i) => { if (b.kind === Kind.Star && (this.lightIdx < 0 || b.mass > bodies[this.lightIdx].mass)) this.lightIdx = i; });
    $("wellsRow").hidden = many;
    this.defaults = defaults;
    this.scale = 10 / defaults.viewScale;
    $("desc").textContent = this.sim.scenarios[index].description;
    $("title").textContent = this.sim.scenarios[index].title;
    const msun = defaults.timeUnit === "day" ? " M☉" : "";
    $("massList").innerHTML = "";
    shown.forEach((b, i) => {
      const li = document.createElement("li");
      li.innerHTML = `<input type="checkbox" checked title="Show / hide" /><b></b><span></span>`;
      li.querySelector("b")!.textContent = b.name;
      li.querySelector("span")!.textContent = b.mass.toExponential(2) + msun;
      li.querySelector("input")!.onchange = (e) => {
        const v = this.visuals[i]; v.hidden = !(e.target as HTMLInputElement).checked; v.mesh.visible = !v.hidden;
        this.updateTrailVisibility(); this.updateLabelVisibility();
      };
      li.querySelector("b")!.onclick = () => { $<HTMLSelectElement>("follow").value = String(i); };
      if (b.mass > 0) {
        const r = document.createElement("input");
        r.type = "range"; r.min = "-1"; r.max = "1.5"; r.step = "0.05"; r.value = "0"; r.title = "Scale this body's mass (x0.1 to x30)";
        r.style.gridColumn = "1 / -1";
        r.oninput = () => {
          const m = b.mass * 10 ** Number(r.value);
          this.masses[i] = m; this.sim.setMass(i, m);
          li.querySelector("span")!.textContent = m.toExponential(2) + msun + ` (×${(10 ** Number(r.value)).toFixed(2)})`;
        };
        li.appendChild(r);
      }
      $("massList").appendChild(li);
    });
    $<HTMLSelectElement>("method").value = String(defaults.method);
    $<HTMLInputElement>("pn").checked = defaults.pn;
    $<HTMLInputElement>("dt").value = "0";
    $<HTMLInputElement>("warp").value = "0";
    this.applyDt();
    this.updateWarpLabel();

    let starIdx = 0;
    shown.forEach((b, i) => {
      const color = NAME_COLORS[b.name] ?? (b.kind === Kind.Star ? STAR_PALETTE[starIdx++ % 3] : b.kind === Kind.Planet ? 0x6aa9ff : 0xffffff);
      const r = b.kind === Kind.Star ? 0.07 : b.kind === Kind.BlackHole ? (this.rs > 0 ? this.rs * this.scale : 0.07)
        : Math.min(0.11, 0.035 + 0.012 * Math.log10(1 + b.mass / 1e-7));
      const mesh = new THREE.Group();
      if (b.kind === Kind.BlackHole) {
        mesh.add(new THREE.Mesh(new THREE.SphereGeometry(r, 32, 20), new THREE.MeshBasicMaterial({ color: 0x000000 })));
        const disc = new THREE.Mesh(new THREE.RingGeometry(r * 1.4, r * 3.2, 64), new THREE.MeshBasicMaterial({ color: 0xff9a3c, side: THREE.DoubleSide, transparent: true, opacity: 0.8 }));
        disc.rotation.x = 0.35;  // accretion disc, tilted toward the viewer
        mesh.add(disc, glow(0xff8a3c, r * 9, 0.5));
      } else if (b.kind === Kind.Star) {
        const map = b.name === "Sun" ? skin("Sun") : null;
        mesh.add(new THREE.Mesh(new THREE.SphereGeometry(r, 32, 20), new THREE.MeshBasicMaterial(map ? { map } : { color })), glow(color, r * 11, 0.9));
      } else {
        mesh.add(new THREE.Mesh(new THREE.SphereGeometry(r, 32, 20), new THREE.MeshStandardMaterial({ map: skin(b.name), roughness: 1 })));
        if (b.name === "Saturn") for (const [a0, a1, col] of [[1.45, 1.95, 0xd9c7a0], [2.05, 2.5, 0xb7a27a]] as const) {
          const ring = new THREE.Mesh(new THREE.RingGeometry(r * a0, r * a1, 64), new THREE.MeshBasicMaterial({ color: col, side: THREE.DoubleSide, transparent: true, opacity: 0.75 }));
          ring.rotation.x = 0.47;  // 26.7 degree ring tilt
          mesh.add(ring);
        }
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
      this.visuals.push({ mesh, trail, trailPos, trailCount: 0, label, color, hidden: false });
      void i;
    });

    if (many) {
      const g = new THREE.BufferGeometry();
      g.setAttribute("position", new THREE.BufferAttribute(new Float32Array(bodies.length * 3), 3));
      this.pts = new THREE.Points(g, new THREE.PointsMaterial({ size: 2.4, sizeAttenuation: false, color: 0xffe0b0, transparent: true, opacity: 0.9, blending: THREE.AdditiveBlending, depthWrite: false }));
      this.pts.frustumCulled = false;
      this.scene.add(this.pts);
      $("massList").innerHTML = `<li><span>${bodies.length} particles (swarm mode)</span></li>`;
    }
    $<HTMLInputElement>("labelsOn").checked = bodies.length <= 12;
    const follow = $<HTMLSelectElement>("follow");
    follow.innerHTML = "";
    follow.add(new Option("Barycenter", "-1"));
    shown.forEach((b, i) => follow.add(new Option(b.name, String(i))));
    this.camera.position.set(0, -14, 9);
    this.controls.target.set(0, 0, 0);
    this.applyWells();
    this.syncBodies();
    this.updateTrailVisibility();
    this.updateLabelVisibility();
  }

  syncBodies() {
    const p = this.sim.positions(), k = this.scale;
    this.visuals.forEach((v, i) => v.mesh.position.set(p[3 * i] * k, p[3 * i + 1] * k, p[3 * i + 2] * k));
    if (this.pts) {
      const a = this.pts.geometry.getAttribute("position") as THREE.BufferAttribute;
      for (let j = 0; j < a.count * 3; j++) (a.array as Float32Array)[j] = p[j] * k;
      a.needsUpdate = true;
    }
    return p;
  }

  /** Per-frame extras that depend on fresh positions: lighting, horizon absorption, gravity wells. */
  postSync(p: Float64Array) {
    const k = this.scale, L = this.lightIdx;
    if (L >= 0) this.light.position.set(p[3 * L] * k, p[3 * L + 1] * k, p[3 * L + 2] * k);
    else this.light.position.copy(this.camera.position);
    if (this.rs > 0) {
      const b = this.bodies.findIndex((x) => x.kind === Kind.BlackHole);
      this.visuals.forEach((v, i) => {
        if (v.hidden || this.bodies[i].mass > 0 || i === b) return;
        const d = Math.hypot(p[3 * i] - p[3 * b], p[3 * i + 1] - p[3 * b + 1], p[3 * i + 2] - p[3 * b + 2]);
        if (d < this.rs) { v.hidden = true; v.mesh.visible = false; v.trail.visible = false; }  // crossed the horizon
      });
    }
    if (this.wells.visible) this.updateWells(p);
  }

  /** Rubber-sheet picture of the potential: depth ~ -sum_i w_i / r_i, w_i = (m_i/m_max)^0.35 (illustrative scaling). */
  updateWells(p: Float64Array) {
    const n = this.bodies.length, k = this.scale, mx = Math.max(...this.masses, 1e-30);
    const w = this.masses.map((m) => Math.pow(m / mx, 0.35));
    const a = this.wells.geometry.getAttribute("position") as THREE.BufferAttribute;
    for (let v = 0; v < a.count; v++) {
      const x = a.getX(v), y = a.getY(v);
      let z = 0;
      for (let i = 0; i < n; i++) { const dx = x - p[3 * i] * k, dy = y - p[3 * i + 1] * k; z -= w[i] / Math.sqrt(dx * dx + dy * dy + 0.09); }
      a.setZ(v, Math.max(-6, 1.1 * z));
    }
    a.needsUpdate = true;
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
  updateTrailVisibility() { const on = $<HTMLInputElement>("trails").checked; for (const v of this.visuals) v.trail.visible = on && !v.hidden; }
  updateLabelVisibility() { const on = $<HTMLInputElement>("labelsOn").checked; for (const v of this.visuals) v.label.style.display = on && !v.hidden ? "" : "none"; }

  fmtTime(t: number) {
    return this.defaults.timeUnit === "day" ? `${(t / 365.25).toFixed(2)} yr` : `t = ${t.toFixed(3)}`;
  }

  tick(now: number) {
    const frameSec = Math.min(0.1, (now - this.last) / 1000);
    this.last = now;
    if (!this.active) { requestAnimationFrame((t) => this.tick(t)); return; }
    this.fps += (1 / Math.max(frameSec, 1e-3) - this.fps) * 0.05;
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
    this.postSync(p);

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
        const visible = v.z < 1 && !vis.hidden;
        vis.label.style.display = visible ? "" : "none";
        vis.label.style.left = `${(v.x * 0.5 + 0.5) * w}px`;
        vis.label.style.top = `${(-v.y * 0.5 + 0.5) * h}px`;
      }
    }

    if (this.frame++ % 6 === 0) {
      const d = this.sim.diagnostics();
      $("dTime").textContent = this.fmtTime(d.time);
      $("fps").textContent = `${Math.round(this.fps)} fps`;
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
  const show = (v: "home" | "list" | "sim") => { $("homeView").hidden = v !== "home"; $("listView").hidden = v !== "list"; $("simView").hidden = v !== "sim"; };
  const pulsar = new Pulsar($<HTMLCanvasElement>("pulsar"));
  let sim: Gravsim | undefined;
  let app: App | undefined;
  const route = () => {
    const h = location.hash, m = h.match(/^#\/sim\/(.+)$/);
    const i = m && sim ? sim.scenarios.findIndex((s) => s.name === m[1]) : -1;
    const view = i >= 0 ? "sim" : m || h.startsWith("#/scenarios") ? "list" : "home";
    show(view);
    pulsar.active = view === "home";
    if (app) app.active = view === "sim";
    if (i >= 0 && app) app.enter(i);
  };
  addEventListener("hashchange", route);
  route();
  try {
    sim = await Gravsim.create();
    $("status").hidden = true;
    renderScenarios(sim.scenarios);
    app = new App(sim);
    route();
  } catch (err) {
    $("status").textContent = "Could not load the WebAssembly module. Build it first (scripts/build-wasm.sh). " + String(err);
  }
}
main();
