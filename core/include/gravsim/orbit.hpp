#pragma once
#include "gravsim/vec3.hpp"

namespace gravsim {

/// Classical Keplerian elements (elliptic orbits only, 0 <= e < 1). Angles in radians.
struct Elements {
    double a = 1.0;       // semi-major axis
    double e = 0.0;       // eccentricity
    double inc = 0.0;     // inclination
    double Omega = 0.0;   // longitude of ascending node
    double omega = 0.0;   // argument of periapsis
    double M = 0.0;       // mean anomaly at the reference epoch
};

/// Solve Kepler's equation  M = E - e sin E  for the eccentric anomaly E (Newton iteration).
double solve_kepler(double M, double e);

/// Relative state (r, v) of the secondary w.r.t. the primary for gravitational parameter mu = G(m1+m2).
void state_from_elements(double mu, const Elements& el, Vec3& r, Vec3& v);

/// Laplace-Runge-Lenz / eccentricity vector  e = (v x h)/mu - r/|r|  (points to periapsis).
Vec3 eccentricity_vector(double mu, const Vec3& r, const Vec3& v);

/// Semi-major axis from vis-viva:  1/a = 2/r - v^2/mu.
double semi_major_axis(double mu, const Vec3& r, const Vec3& v);

}  // namespace gravsim
