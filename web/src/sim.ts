// Thin typed wrapper around the Emscripten module exported by wasm/bindings.cpp.

export interface GravsimModule {
  HEAPF64: Float64Array;
  UTF8ToString(ptr: number): string;
  _gs_scenario_count(): number;
  _gs_scenario_name(i: number): number;
  _gs_scenario_title(i: number): number;
  _gs_scenario_description(i: number): number;
  _gs_load(i: number): number;
  _gs_reset(): void;
  _gs_time_unit(): number;
  _gs_default_dt(): number;
  _gs_default_warp(): number;
  _gs_view_scale(): number;
  _gs_default_method(): number;
  _gs_default_pn(): number;
  _gs_body_count(): number;
  _gs_body_name(i: number): number;
  _gs_body_kind(i: number): number;
  _gs_body_mass(i: number): number;
  _gs_body_radius_km(i: number): number;
  _gs_set_method(m: number): void;
  _gs_set_dt(dt: number): void;
  _gs_set_pn(on: number): void;
  _gs_set_tolerance(rtol: number): void;
  _gs_advance(T: number): number;
  _gs_positions(): number;
  _gs_velocities(): number;
  _gs_diagnostics(): number;
}

declare global {
  interface Window {
    createGravsim?: () => Promise<GravsimModule>;
  }
}

export enum Kind { Star, Planet, DwarfPlanet, Comet, BlackHole, Other }

export interface ScenarioInfo { index: number; name: string; title: string; description: string }
export interface BodyMeta { name: string; kind: Kind; mass: number; radiusKm: number }
export interface Diagnostics { time: number; kinetic: number; potential: number; energy: number; dE: number; L: number; dL: number; evals: number }
export interface ScenarioDefaults { timeUnit: string; dt: number; warp: number; viewScale: number; method: number; pn: boolean }

function loadScript(src: string): Promise<void> {
  return new Promise((resolve, reject) => {
    const s = document.createElement("script");
    s.src = src;
    s.onload = () => resolve();
    s.onerror = () => reject(new Error(`Could not load ${src}`));
    document.head.appendChild(s);
  });
}

export class Gravsim {
  readonly scenarios: ScenarioInfo[] = [];
  constructor(private m: GravsimModule) {
    const n = m._gs_scenario_count();
    for (let i = 0; i < n; i++) {
      this.scenarios.push({
        index: i,
        name: this.str(m._gs_scenario_name(i)),
        title: this.str(m._gs_scenario_title(i)),
        description: this.str(m._gs_scenario_description(i)),
      });
    }
  }

  static async create(): Promise<Gravsim> {
    await loadScript(`${import.meta.env.BASE_URL}wasm/gravsim.js`);
    if (!window.createGravsim) throw new Error("gravsim.js loaded but createGravsim is missing");
    return new Gravsim(await window.createGravsim());
  }

  private str(ptr: number): string { return this.m.UTF8ToString(ptr); }

  load(index: number): { bodies: BodyMeta[]; defaults: ScenarioDefaults } {
    const n = this.m._gs_load(index);
    if (n < 0) throw new Error("unknown scenario");
    const bodies: BodyMeta[] = [];
    for (let i = 0; i < n; i++) {
      bodies.push({
        name: this.str(this.m._gs_body_name(i)),
        kind: this.m._gs_body_kind(i) as Kind,
        mass: this.m._gs_body_mass(i),
        radiusKm: this.m._gs_body_radius_km(i),
      });
    }
    return {
      bodies,
      defaults: {
        timeUnit: this.str(this.m._gs_time_unit()),
        dt: this.m._gs_default_dt(),
        warp: this.m._gs_default_warp(),
        viewScale: this.m._gs_view_scale(),
        method: this.m._gs_default_method(),
        pn: this.m._gs_default_pn() === 1,
      },
    };
  }

  reset() { this.m._gs_reset(); }
  setMethod(m: number) { this.m._gs_set_method(m); }
  setDt(dt: number) { this.m._gs_set_dt(dt); }
  setPN(on: boolean) { this.m._gs_set_pn(on ? 1 : 0); }
  advance(T: number): number { return this.m._gs_advance(T); }

  /** Copy of current positions as [x0,y0,z0,x1,...] (length unit of the scenario). */
  positions(): Float64Array {
    const ptr = this.m._gs_positions();
    const n = this.m._gs_body_count() * 3;
    return this.m.HEAPF64.slice(ptr / 8, ptr / 8 + n);
  }

  diagnostics(): Diagnostics {
    const p = this.m._gs_diagnostics() / 8;
    const h = this.m.HEAPF64;
    return { time: h[p], kinetic: h[p + 1], potential: h[p + 2], energy: h[p + 3], dE: h[p + 4], L: h[p + 5], dL: h[p + 6], evals: h[p + 7] };
  }
}
