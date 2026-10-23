import * as THREE from "three";
import { glow } from "./skins";

/** Cinematic home-page backdrop: a spinning pulsar sweeping twin beams across a starfield. */
export class Pulsar {
  active = false;
  private r: THREE.WebGLRenderer;
  private scene = new THREE.Scene();
  private cam = new THREE.PerspectiveCamera(50, 1, 0.1, 1000);
  private spin = new THREE.Group();
  private mag = new THREE.Group();
  private flash = glow(0x9fd0ff, 4, 0.9);
  private stars: THREE.Points;
  private beams: THREE.Mesh[] = [];
  private last = 0;
  private ang = 0;

  constructor(canvas: HTMLCanvasElement) {
    this.r = new THREE.WebGLRenderer({ canvas, antialias: true });
    this.r.setPixelRatio(Math.min(devicePixelRatio, 2));
    this.scene.background = new THREE.Color(0x03050d);

    const n = 2600, pos = new Float32Array(n * 3), col = new Float32Array(n * 3);
    for (let i = 0; i < n; i++) {
      const d = new THREE.Vector3(Math.random() - 0.5, Math.random() - 0.5, Math.random() - 0.5).normalize().multiplyScalar(120 + Math.random() * 180);
      pos.set([d.x, d.y, d.z], 3 * i);
      const t = Math.random(), c = new THREE.Color().setHSL(t < 0.6 ? 0.6 : t < 0.85 ? 0.08 : 0.95, 0.5, 0.65 + Math.random() * 0.3);
      col.set([c.r, c.g, c.b], 3 * i);
    }
    const sg = new THREE.BufferGeometry();
    sg.setAttribute("position", new THREE.BufferAttribute(pos, 3));
    sg.setAttribute("color", new THREE.BufferAttribute(col, 3));
    this.stars = new THREE.Points(sg, new THREE.PointsMaterial({ size: 1.6, sizeAttenuation: false, vertexColors: true }));
    this.scene.add(this.stars);

    for (const [c, x, y, z, s] of [[0x3a2a8a, -90, 30, -120, 260], [0x1d4e8a, 110, -40, -140, 300], [0x6a2a6a, 20, 80, -160, 220]] as const) {
      const g = glow(c, s, 0.35); g.position.set(x, y, z); this.scene.add(g);
    }

    // Neutron star + two beams along the magnetic axis, which is tilted from the spin axis (Y).
    const star = new THREE.Mesh(new THREE.SphereGeometry(0.55, 32, 20), new THREE.MeshBasicMaterial({ color: 0xdff0ff }));
    this.mag.add(star, this.flash, glow(0x5a9aff, 3.2, 0.8));
    const cone = new THREE.ConeGeometry(1.1, 18, 40, 1, true).rotateX(Math.PI).translate(0, 9, 0);
    for (const flip of [false, true]) {
      const m = new THREE.Mesh(flip ? cone.clone().rotateX(Math.PI) : cone,
        new THREE.MeshBasicMaterial({ color: 0x7fb8ff, transparent: true, opacity: 0.2, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide }));
      this.beams.push(m); this.mag.add(m);
    }
    this.mag.rotation.z = 0.62;
    this.spin.add(this.mag);
    this.scene.add(this.spin);
    addEventListener("resize", () => this.resize());
    this.resize();
    requestAnimationFrame((t) => this.frame(t));
  }

  private resize() {
    const w = innerWidth, h = innerHeight;
    this.r.setSize(w, h, false);
    this.cam.aspect = w / h; this.cam.updateProjectionMatrix();
  }

  private frame(now: number) {
    requestAnimationFrame((t) => this.frame(t));
    if (!this.active) { this.last = now; return; }
    const dt = Math.min((now - this.last) / 1000, 0.05); this.last = now;
    this.spin.rotation.y += dt * 2.1;
    this.ang += dt * 0.045;
    this.cam.position.set(Math.sin(this.ang) * 17, 4 + Math.sin(this.ang * 1.7) * 2, Math.cos(this.ang) * 17);
    this.cam.lookAt(0, 0, 0);
    this.stars.rotation.y += dt * 0.004;

    // Lighthouse flash: brightest when a beam points at the camera.
    const dir = new THREE.Vector3(0, 1, 0).applyQuaternion(this.mag.getWorldQuaternion(new THREE.Quaternion()));
    const f = Math.abs(dir.dot(this.cam.position.clone().normalize()));
    this.flash.scale.setScalar(3 + 14 * Math.pow(f, 10));
    this.r.render(this.scene, this.cam);
  }
}
