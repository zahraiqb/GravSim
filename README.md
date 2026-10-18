# gravsim

A physics-first N-body gravity simulator: a **C++20 core** (Newtonian + 1PN relativistic
corrections, symplectic and adaptive integrators), compiled to **WebAssembly** and rendered in
the browser with Three.js. Static site, so it deploys to GitHub Pages with zero servers.

Scenarios: Solar System (+ Pluto, Halley), Alpha Centauri A+B, S2 orbiting Sgr A\*, a wide
black-hole binary, a Kepler orbit, the figure-eight three-body choreography, and Burrau's
Pythagorean problem. Load your own bodies from CSV.

> The math, units, and limits are in [`docs/physics.md`](docs/physics.md). Correctness is
> enforced by a validation suite (analytic Kepler solution, convergence orders, conservation
> laws, Mercury's 42.98″/century and S2's ~12′ precession).

## Quick start (native)
```bash
cmake -S . -B build && cmake --build build -j
ctest --test-dir build --output-on-failure          # run the validation suite
./build/gravsim --list
./build/gravsim --scenario solar_system --years 100 --out solar.csv
./build/gravsim --scenario sgr_a_s2 --time 30000 --sample 10 --out s2.csv   # 1PN on by default
./build/gravsim --scenario kepler --method euler --dt 0.01 --time 200       # watch Euler's energy drift
```
Requires CMake ≥ 3.20 and a C++20 compiler (Catch2 is fetched automatically).

## Web app
```bash
# one-time: install Emscripten (https://emscripten.org) and activate it, then
./scripts/build-wasm.sh
cd web && npm install && npm run dev
```
Deploying: push to `main`; the *Deploy to GitHub Pages* workflow builds the WASM and the site.
In your repo, set **Settings → Pages → Source = GitHub Actions**.

## Layout
```
core/       pure C++20 physics (no web/graphics deps) + Catch2 tests
cli/        command-line driver (CSV output)
wasm/       flat C API for Emscripten
web/        TypeScript + Three.js + Vite viewer
docs/       derivations, validation matrix, references
data/       initial-condition notes and CSV format
```

## Scope and honesty
- Direct O(N²) summation: fine for dozens/hundreds of bodies, not for galaxies (Barnes–Hut is on the roadmap).
- 1PN only: no gravitational waves, spin, or strong-field dynamics. Black holes are point masses.
- Built-in planet data are J2000 mean elements, not a precision ephemeris; use `--csv` with JPL Horizons vectors for that.

## Roadmap
Barnes–Hut tree · IAS15 integrator · Schwarzschild geodesics (photon orbits, ISCO) · 2.5PN radiation reaction · moons · per-scenario energy-error plots in docs.

## License
MIT. See [LICENSE](LICENSE). Please cite via [CITATION.cff](CITATION.cff).
