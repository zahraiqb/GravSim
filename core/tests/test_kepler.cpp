#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include "gravsim/orbit.hpp"
#include "gravsim/units.hpp"

using namespace gravsim;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("Kepler equation solver round-trips", "[kepler]") {
    for (double e : {0.0, 0.1, 0.5, 0.9, 0.967, 0.999}) {
        for (double M = -7.0; M <= 7.0; M += 0.173) {
            const double E = solve_kepler(M, e);
            double Mr = std::fmod(M, 2 * units::PI);
            if (Mr > units::PI) Mr -= 2 * units::PI;
            if (Mr < -units::PI) Mr += 2 * units::PI;
            REQUIRE_THAT(E - e * std::sin(E), WithinAbs(Mr, 1e-13));
        }
    }
}

TEST_CASE("Elements -> state conserves the Keplerian invariants", "[kepler]") {
    const double mu = 2.5;
    Elements el{1.7, 0.6, 0.4, 1.1, 2.2, 0.9};
    Vec3 r, v;
    state_from_elements(mu, el, r, v);
    // vis-viva
    REQUIRE_THAT(semi_major_axis(mu, r, v), WithinRel(el.a, 1e-13));
    // specific angular momentum magnitude sqrt(mu a (1-e^2))
    REQUIRE_THAT(norm(cross(r, v)), WithinRel(std::sqrt(mu * el.a * (1 - el.e * el.e)), 1e-13));
    // eccentricity vector magnitude
    REQUIRE_THAT(norm(eccentricity_vector(mu, r, v)), WithinRel(el.e, 1e-12));
    // inclination from h_z / |h|
    const Vec3 h = cross(r, v);
    REQUIRE_THAT(std::acos(h.z / norm(h)), WithinAbs(el.inc, 1e-13));
}

TEST_CASE("Periapsis and apoapsis distances", "[kepler]") {
    Elements el{3.0, 0.8, 0.0, 0.0, 0.0, 0.0};  // M=0 -> periapsis
    Vec3 r, v;
    state_from_elements(1.0, el, r, v);
    REQUIRE_THAT(norm(r), WithinRel(3.0 * 0.2, 1e-14));
    el.M = units::PI;
    state_from_elements(1.0, el, r, v);
    REQUIRE_THAT(norm(r), WithinRel(3.0 * 1.8, 1e-14));
}
