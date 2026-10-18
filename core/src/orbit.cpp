#include "gravsim/orbit.hpp"
#include <cmath>
#include "gravsim/units.hpp"

namespace gravsim {

double solve_kepler(double M, double e) {
    using units::PI;
    // Reduce M to [-pi, pi].
    M = std::fmod(M, 2.0 * PI);
    if (M > PI) M -= 2.0 * PI;
    if (M < -PI) M += 2.0 * PI;
    // For M >= 0 and E0 = pi, f(E) = E - e sin E - M is convex and positive on [root, pi],
    // so Newton converges monotonically for every 0 <= e < 1 (symmetric for M < 0).
    double E = (e < 0.8) ? M + e * std::sin(M) : std::copysign(PI, M);
    for (int i = 0; i < 100; ++i) {
        double f = E - e * std::sin(E) - M;
        double fp = 1.0 - e * std::cos(E);
        double dE = f / fp;
        E -= dE;
        if (std::fabs(dE) < 1e-15) break;
    }
    return E;
}

void state_from_elements(double mu, const Elements& el, Vec3& r, Vec3& v) {
    const double E = solve_kepler(el.M, el.e);
    const double cE = std::cos(E), sE = std::sin(E);
    const double b = std::sqrt(1.0 - el.e * el.e);
    // Perifocal frame (x toward periapsis, y 90 deg ahead in the orbital plane).
    const double xp = el.a * (cE - el.e), yp = el.a * b * sE;
    const double rr = el.a * (1.0 - el.e * cE);
    const double fac = std::sqrt(mu * el.a) / rr;
    const double vxp = -fac * sE, vyp = fac * b * cE;
    // Rotation R = Rz(Omega) Rx(inc) Rz(omega).
    const double cO = std::cos(el.Omega), sO = std::sin(el.Omega);
    const double ci = std::cos(el.inc), si = std::sin(el.inc);
    const double cw = std::cos(el.omega), sw = std::sin(el.omega);
    const double Px = cO * cw - sO * sw * ci, Py = sO * cw + cO * sw * ci, Pz = sw * si;
    const double Qx = -cO * sw - sO * cw * ci, Qy = -sO * sw + cO * cw * ci, Qz = cw * si;
    r = {Px * xp + Qx * yp, Py * xp + Qy * yp, Pz * xp + Qz * yp};
    v = {Px * vxp + Qx * vyp, Py * vxp + Qy * vyp, Pz * vxp + Qz * vyp};
}

Vec3 eccentricity_vector(double mu, const Vec3& r, const Vec3& v) {
    const Vec3 h = cross(r, v);
    return cross(v, h) / mu - r / norm(r);
}

double semi_major_axis(double mu, const Vec3& r, const Vec3& v) {
    return 1.0 / (2.0 / norm(r) - norm2(v) / mu);
}

}  // namespace gravsim
