#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <cmath>
#include "gravsim/integrators.hpp"
#include "gravsim/scenarios.hpp"
#include "gravsim/units.hpp"

using namespace gravsim;
using Catch::Matchers::WithinRel;
using namespace units;

namespace {
// Eccentricity vector (points to periapsis) of body 1 relative to body 0.
Vec3 periapsis_vector(const System& s, double mu) {
    return eccentricity_vector(mu, s.pos[1] - s.pos[0], s.vel[1] - s.vel[0]);
}

// Precession of the longitude of periapsis accumulated over `T` (rad), PN on/off.
double precession(const Elements& el, double m1, double m2, bool pn, double T) {
    System sys = make_binary(G_AU_DAY_MSUN, m1, m2, el);
    sys.params.post_newtonian = pn;
    const double mu = G_AU_DAY_MSUN * (m1 + m2);
    Simulation sim(sys, Method::DOPRI5, 0.05);
    sim.set_tolerances(1e-13, 1e-16);
    const Vec3 e0 = periapsis_vector(sim.system(), mu);
    sim.advance(T);
    const Vec3 e1 = periapsis_vector(sim.system(), mu);
    // Angle between periapsis directions; both lie in (nearly) the same orbital plane, so this
    // is the in-plane precession regardless of inclination or sense of rotation.
    return std::acos(std::clamp(dot(e0, e1) / (norm(e0) * norm(e1)), -1.0, 1.0));
}


// Advance to the next periapsis passage (radial velocity crosses zero from - to +), refined by
// Newton iteration on r.v. Measuring at the same orbital phase cancels the short-period
// oscillation that the osculating eccentricity vector shows in a relativistic orbit.
void advance_to_periapsis(Simulation& sim, double period) {
    auto rv = [&] {
        const System& s = sim.system();
        return dot(s.pos[1] - s.pos[0], s.vel[1] - s.vel[0]);
    };
    sim.advance(0.75 * period);  // inbound leg (r.v < 0), safely past apoapsis
    while (rv() < 0.0) sim.advance(period / 4000.0);
    for (int it = 0; it < 8; ++it) {
        const System& s = sim.system();
        std::vector<Vec3> a;
        s.accelerations(s.pos, s.vel, a);
        const Vec3 r = s.pos[1] - s.pos[0], v = s.vel[1] - s.vel[0], ar = a[1] - a[0];
        sim.advance(-dot(r, v) / (dot(v, v) + dot(r, ar)));
    }
}

// Periapsis advance accumulated over `orbits` radial periods, measured periapsis-to-periapsis.
double precession_at_periapsis(const Elements& el, double m1, double m2, int orbits) {
    System sys = make_binary(G_AU_DAY_MSUN, m1, m2, el);  // M = 0 -> starts at periapsis
    sys.params.post_newtonian = true;
    const double mu = G_AU_DAY_MSUN * (m1 + m2);
    const double period = 2 * PI * std::sqrt(el.a * el.a * el.a / mu);
    Simulation sim(sys, Method::DOPRI5, 0.05);
    sim.set_tolerances(1e-13, 1e-16);
    const Vec3 e0 = periapsis_vector(sim.system(), mu);
    for (int k = 0; k < orbits; ++k) advance_to_periapsis(sim, period);
    const Vec3 e1 = periapsis_vector(sim.system(), mu);
    return std::acos(std::clamp(dot(e0, e1) / (norm(e0) * norm(e1)), -1.0, 1.0));
}

// First-order GR periapsis advance per orbit: 6 pi G M / (c^2 a (1 - e^2)).
double gr_advance_per_orbit(double mu, double a, double e) {
    return 6.0 * PI * mu / (C_AU_DAY * C_AU_DAY * a * (1.0 - e * e));
}
}  // namespace

TEST_CASE("Mercury: GR perihelion precession is ~43 arcsec/century", "[relativity][mercury]") {
    const Elements el{0.38709927, 0.20563593, 7.0 * DEG, 48.3 * DEG, 29.1 * DEG, 0.0};
    const double m_sun = 1.0, m_merc = 1.0 / 6023600.0;
    const double mu = G_AU_DAY_MSUN * (m_sun + m_merc);
    const double century = 100.0 * DAYS_PER_JULIAN_YEAR;
    const double period = 2 * PI * std::sqrt(el.a * el.a * el.a / mu);
    const double expected = gr_advance_per_orbit(mu, el.a, el.e) * (century / period) / ARCSEC;

    REQUIRE_THAT(expected, WithinRel(42.98, 0.01));  // sanity-check the analytic formula itself

    const double newton = precession(el, m_sun, m_merc, false, century) / ARCSEC;
    const double with_pn = precession(el, m_sun, m_merc, true, century) / ARCSEC;
    INFO("analytic " << expected << "  simulated " << with_pn << " arcsec/century");
    REQUIRE(std::fabs(newton) < 1e-3);                       // pure Newtonian 2-body: no precession
    REQUIRE_THAT(with_pn, WithinRel(expected, 0.02));        // 1PN EIH reproduces the GR value
}

TEST_CASE("S2 around Sgr A*: ~12 arcmin periapsis advance per orbit", "[relativity][s2]") {
    Scenario sc;
    REQUIRE(make_scenario("sgr_a_s2", sc));
    const double M = sc.system.mass[0], m = sc.system.mass[1];
    const Elements el{1031.0, 0.8844, 134.7 * DEG, 228.2 * DEG, 66.3 * DEG, 0.0};
    const double mu = G_AU_DAY_MSUN * (M + m);
    const double period = 2 * PI * std::sqrt(el.a * el.a * el.a / mu);
    const double per_orbit = gr_advance_per_orbit(mu, el.a, el.e);
    REQUIRE_THAT(per_orbit / ARCSEC / 60.0, WithinRel(12.0, 0.05));
    const int orbits = 3;
    const double sim = precession_at_periapsis(el, M, m, orbits);
    INFO("analytic " << orbits * per_orbit / ARCSEC << "  simulated " << sim / ARCSEC << " arcsec");
    REQUIRE_THAT(sim, WithinRel(orbits * per_orbit, 0.02));
}

TEST_CASE("1PN forces reduce to Newtonian as c -> infinity", "[relativity]") {
    Scenario sc;
    REQUIRE(make_scenario("solar_system", sc));
    std::vector<Vec3> a_newton, a_pn;
    System s = sc.system;
    s.params.post_newtonian = false;
    s.accelerations(s.pos, s.vel, a_newton);
    s.params.post_newtonian = true;
    s.params.c = 1e12;
    s.accelerations(s.pos, s.vel, a_pn);
    for (std::size_t i = 0; i < s.size(); ++i)
        REQUIRE(norm(a_pn[i] - a_newton[i]) < 1e-18 * norm(a_newton[i]) + 1e-30);
}

TEST_CASE("1PN correction magnitude is ~ (v/c)^2 for Mercury", "[relativity]") {
    Scenario sc;
    REQUIRE(make_scenario("solar_system", sc));
    std::vector<Vec3> an, ap;
    System s = sc.system;
    s.accelerations(s.pos, s.vel, an);
    s.params.post_newtonian = true;
    s.accelerations(s.pos, s.vel, ap);
    const double rel = norm(ap[1] - an[1]) / norm(an[1]);  // Mercury is index 1
    REQUIRE(rel > 1e-9);
    REQUIRE(rel < 1e-6);
}
