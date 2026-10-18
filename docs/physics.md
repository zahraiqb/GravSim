# Physics and numerics

All formulas below are implemented in `core/` and checked in `core/tests/`.

## 1. Units
Length = AU, time = day, mass = solar mass. Then G = k², k = 0.01720209895 (Gaussian constant),
G = 2.959122082855911×10⁻⁴ AU³ M☉⁻¹ day⁻². Speed of light c = 299792.458 km/s · 86400 s / 149597870.7 km
= 173.1446 AU/day. Everything is `double`. Some scenarios use G = 1 (dimensionless).

## 2. Newtonian N-body
For bodies with masses mᵢ at positions **r**ᵢ:

  **a**ᵢ = Σⱼ≠ᵢ G mⱼ (**r**ⱼ − **r**ᵢ) / (|**r**ⱼ − **r**ᵢ|² + ε²)^{3/2}

Each pair is visited once (Newton's 3rd law), cost O(N²). Conserved quantities, monitored in
the UI and CLI: E = Σ ½mᵢvᵢ² − Σᵢ<ⱼ G mᵢmⱼ/rᵢⱼ, **L** = Σ mᵢ **r**ᵢ×**v**ᵢ, **P**, centre of mass.
Scenarios are placed in the barycentric frame.

## 3. Integrators
| Method | Order | Symplectic | Force evals/step | Notes |
|---|---|---|---|---|
| Euler | 1 | no | 1 | Energy grows secularly; pedagogical |
| RK4 | 4 | no | 4 | Energy drifts (slowly) |
| Leapfrog KDK | 2 | yes | 1 | Time-reversible; bounded energy error |
| Yoshida-4 | 4 | yes | 3 | S₄ = S₂(w₁h) S₂(w₀h) S₂(w₁h), w₁ = 1/(2−2^{1/3}), w₀ = −2^{1/3}/(2−2^{1/3}) |
| Dormand–Prince 5(4) | 5 | no | 6 | Adaptive; error norm √(mean((δᵢ/(atol+rtol·max|yᵢ|))²)); step ×0.9·err^{−1/5}, clamped [0.2, 5] |

Symplectic methods exactly conserve a nearby "shadow Hamiltonian", so their energy error
oscillates instead of drifting. This is why they are preferred for long Solar System runs.
Chaotic systems (the Pythagorean problem, planetary motion over Gyr) are sensitive to
initial conditions: no integrator can give a long-term *trajectory*, only statistically valid ones.

## 4. Orbital elements → state vectors
Kepler's equation M = E − e sin E is solved by Newton's method (monotone convergence from
E�� = π·sgn M for e ≥ 0.8). Perifocal position/velocity:

  x′ = a(cos E − e), y′ = a√(1−e²) sin E,  ẋ′ = −√(μa) sin E / r, ẏ′ = √(μa)√(1−e²) cos E / r

rotated by R = R_z(Ω) R_x(i) R_z(ω). μ = G(m₁+m₂).

## 5. Relativistic corrections (1PN)
With `post_newtonian` enabled, the Einstein–Infeld–Hoffmann equations (β = γ = 1, harmonic
coordinates; Moyer 2003 / Standish & Williams) are added, μⱼ = G mⱼ, rᵢⱼ = |**r**ⱼ − **r**ᵢ|:

  **a**ᵢ = Σⱼ μⱼ(**r**ⱼ−**r**ᵢ)/rᵢⱼ³ · { 1 − (4/c²)Σ_{l≠i} μₗ/rᵢₗ − (1/c²)Σ_{k≠j} μₖ/rⱼₖ + vᵢ²/c² + 2vⱼ²/c² − 4 **v**ᵢ·**v**ⱼ/c² − (3/2c²)[(**r**ᵢ−**r**ⱼ)·**v**ⱼ/rᵢⱼ]² + (1/2c²)(**r**ⱼ−**r**ᵢ)·**a**ⱼ }
   + (1/c²) Σⱼ μⱼ/rᵢⱼ³ { (**r**ᵢ−**r**ⱼ)·(4**v**ᵢ − 3**v**ⱼ) } (**v**ᵢ − **v**ⱼ) + (7/2c²) Σⱼ μⱼ **a**ⱼ/rᵢⱼ

where **a**ⱼ are the Newtonian accelerations. Cost is still O(N²) (the potential sums are precomputed).
For a test mass around a fixed mass this reduces to the standard 1PN Schwarzschild result and
yields the periapsis advance per orbit

  Δϖ = 6π G M / (c² a (1 − e²)).

**Validation:** Mercury → 42.98″/century; S2 around Sgr A* → ≈ 12′ per orbit (both tested).

**Limits (be honest in any write-up):** 1PN is valid for v/c ≪ 1 and r ≫ GM/c². There is no
gravitational-wave back-reaction (2.5PN), no spin, no frame dragging, no tidal effects, no
strong-field dynamics. Black holes are point masses; the "binary black hole" scenario cannot
inspiral. Leapfrog/Yoshida with velocity-dependent forces are no longer exactly symplectic;
use DOPRI5 for relativistic runs.

## 6. Validation matrix
| Test | What it proves |
|---|---|
| Kepler solver round-trip, invariants | Element conversion is exact (a, e, i, h) |
| Two-body vs analytic solution | Integrators reproduce the exact Kepler orbit |
| Convergence order | Euler≈1, Leapfrog≈2, RK4≈4, Yoshida≈4 (halve dt, check error ratio) |
| Energy over 300 orbits | Symplectic: bounded, no secular growth; Euler: not |
| Time reversal | Leapfrog returns to its start to ~1e-9 |
| Solar System 1000 yr | ΔE/E < 1e-7, ΔL/L ~ roundoff, orbit ranges, Jupiter period |
| Figure-eight, Pythagorean | Periodic choreography; adaptive close-encounter handling |
| Mercury / S2 precession | 1PN force matches analytic GR |
| c → ∞ limit | 1PN force reduces to Newtonian |

## 7. Data accuracy
Planet initial conditions are mean J2000 Keplerian elements converted to heliocentric state
vectors and shifted to the barycentre. Real osculating barycentric vectors from JPL Horizons
are more accurate; see `data/README.md` and `--csv`. Earth is the Earth–Moon barycentre; the Moon
is not modelled.

## References
- Standish, E. M., *Keplerian Elements for Approximate Positions of the Major Planets*, JPL.
- Yoshida, H., *Construction of higher order symplectic integrators*, Phys. Lett. A 150 (1990).
- Dormand, J. R. & Prince, P. J., *A family of embedded Runge–Kutta formulae*, J. Comp. Appl. Math. 6 (1980).
- Einstein, Infeld & Hoffmann (1938); Moyer, *Formulation for Observed and Computed Values of Deep Space Network Data Types* (2003).
- Chenciner & Montgomery, *A remarkable periodic solution of the three-body problem*, Ann. Math. 152 (2000).
- Hairer, Lubich & Wanner, *Geometric Numerical Integration* (2006).
