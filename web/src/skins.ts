import * as THREE from "three";

// Procedural surfaces: no image files, so nothing to license or download. They are stylised
// likenesses (banded gas giants, blue-green Earth, rusty Mars...), not real photographs.
const cache = new Map<string, THREE.CanvasTexture>();
const hashStr = (s: string) => { let h = 2166136261; for (const c of s) h = Math.imul(h ^ c.charCodeAt(0), 16777619); return h >>> 0; };

export function skin(name: string): THREE.CanvasTexture {
  const hit = cache.get(name);
  if (hit) return hit;
  const W = 256, H = 128, c = document.createElement("canvas");
  c.width = W; c.height = H;
  const g = c.getContext("2d")!;
  let h = hashStr(name);
  const rnd = () => ((h = Math.imul(h ^ (h >>> 15), 2246822507) >>> 0) / 4294967296);
  const fill = (col: string) => { g.fillStyle = col; g.fillRect(0, 0, W, H); };
  const blobs = (n: number, cols: string[], r0: number, r1: number, alpha: number, stretch = 1) => {
    for (let i = 0; i < n; i++) {
      const r = r0 + rnd() * (r1 - r0);
      g.globalAlpha = alpha; g.fillStyle = cols[Math.floor(rnd() * cols.length)];
      g.beginPath(); g.ellipse(rnd() * W, rnd() * H, r * stretch, r * 0.6, 0, 0, 6.29); g.fill();
    }
    g.globalAlpha = 1;
  };
  const bands = (cols: string[], n: number) => {
    for (let i = 0; i < n; i++) { g.fillStyle = cols[Math.floor(rnd() * cols.length)]; g.fillRect(0, (i * H) / n, W, H / n + 1); }
    blobs(70, cols, 3, 9, 0.35, 6);
  };
  const caps = (col: string, hgt: number) => { g.fillStyle = col; g.fillRect(0, 0, W, hgt); g.fillRect(0, H - hgt, W, hgt); };
  switch (name) {
    case "Sun": fill("#ffb347"); blobs(500, ["#ffd36b", "#ff9a2e", "#ffe69a"], 2, 7, 0.5); break;
    case "Earth": fill("#1d4e9e"); blobs(26, ["#2f7d3a", "#7a6a3b", "#3e8a47"], 8, 22, 1); blobs(45, ["#ffffff"], 5, 14, 0.55, 2.5); caps("#f2f6ff", 7); break;
    case "Mars": fill("#b5532f"); blobs(40, ["#8a3a20", "#d17a4a"], 4, 14, 0.7); caps("#f3e9e4", 4); break;
    case "Jupiter": bands(["#d9b48a", "#b5835a", "#efe0c8", "#a86f4a", "#e6c9a0"], 15); g.fillStyle = "#b5472f"; g.beginPath(); g.ellipse(150, 82, 16, 9, 0, 0, 6.29); g.fill(); break;
    case "Saturn": bands(["#e6d3a0", "#d1b987", "#f1e4bd", "#c9ad7a"], 12); break;
    case "Uranus": fill("#9fe0e8"); bands(["#9fe0e8", "#8fd2dc", "#b2eef2"], 8); break;
    case "Neptune": fill("#3f64d8"); bands(["#3f64d8", "#2f4fb8", "#5b82ee"], 9); blobs(8, ["#ffffff"], 4, 9, 0.5, 5); break;
    case "Venus": fill("#e8d3a2"); blobs(60, ["#d0b06a", "#f3e6c0"], 6, 16, 0.5, 5); break;
    case "Pluto": fill("#c9b8a0"); blobs(30, ["#9a7f66", "#e9dfd0"], 4, 14, 0.7); g.fillStyle = "#f0e8dc"; g.beginPath(); g.ellipse(120, 70, 22, 18, 0, 0, 6.29); g.fill(); break;
    case "Io": fill("#e8d45a"); blobs(50, ["#c0602a", "#f6ecA0", "#3a3028"], 2, 8, 0.8); break;
    case "Europa": fill("#d9d2c0"); blobs(70, ["#8a5a3a", "#b8a58a"], 1, 6, 0.6, 8); break;
    case "Ganymede": fill("#8a8478"); blobs(60, ["#5a5448", "#b0a898"], 4, 14, 0.7); break;
    case "Callisto": fill("#5a5248"); blobs(150, ["#c8c0b0", "#3a342c"], 1, 4, 0.8); break;
    case "Mercury": case "Moon": case "Charon": fill("#8c8a86"); blobs(160, ["#5a5855", "#b4b2ac"], 1, 5, 0.7); break;
    default: {
      const hue = hashStr(name) % 360, gas = name.length % 2 === 0;
      if (gas) { fill(`hsl(${hue},40%,55%)`); bands([`hsl(${hue},45%,62%)`, `hsl(${hue},35%,45%)`, `hsl(${hue},30%,72%)`], 10); }
      else { fill(`hsl(${hue},30%,40%)`); blobs(60, [`hsl(${hue},35%,30%)`, `hsl(${hue},25%,55%)`], 3, 14, 0.7); }
    }
  }
  const tex = new THREE.CanvasTexture(c);
  tex.colorSpace = THREE.SRGBColorSpace;
  cache.set(name, tex);
  return tex;
}

const glowTex = new Map<number, THREE.CanvasTexture>();
/** Soft additive halo used for stars, black-hole glow and the pulsar. */
export function glow(color: number, size: number, opacity = 1): THREE.Sprite {
  let t = glowTex.get(color);
  if (!t) {
    const c = document.createElement("canvas"); c.width = c.height = 128;
    const g = c.getContext("2d")!, col = "#" + color.toString(16).padStart(6, "0");
    const grad = g.createRadialGradient(64, 64, 0, 64, 64, 64);
    grad.addColorStop(0, col); grad.addColorStop(0.25, col + "aa"); grad.addColorStop(1, col + "00");
    g.fillStyle = grad; g.fillRect(0, 0, 128, 128);
    t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; glowTex.set(color, t);
  }
  const s = new THREE.Sprite(new THREE.SpriteMaterial({ map: t, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true, opacity }));
  s.scale.setScalar(size);
  return s;
}
