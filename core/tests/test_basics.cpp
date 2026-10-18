#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "gravsim/units.hpp"
#include "gravsim/vec3.hpp"

using namespace gravsim;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("Vec3 algebra", "[vec3]") {
    Vec3 a(1, 2, 3), b(-4, 5, 0.5);
    REQUIRE(dot(a, b) == 1 * -4 + 2 * 5 + 3 * 0.5);
    Vec3 c = cross(a, b);
    REQUIRE_THAT(dot(c, a), WithinAbs(0.0, 1e-14));
    REQUIRE_THAT(dot(c, b), WithinAbs(0.0, 1e-14));
    REQUIRE_THAT(norm(Vec3(3, 4, 0)), WithinAbs(5.0, 1e-15));
    REQUIRE((a + b - b) == a);
}

TEST_CASE("Unit constants are self-consistent", "[units]") {
    // c in AU/day from IAU AU and exact c
    REQUIRE_THAT(units::C_AU_DAY, WithinRel(173.1446327, 1e-9));
    // Gaussian G reproduces the IAU solar GM to ~1e-8 (k is a defining-era constant)
    const double gm_au3_d2 = units::G_AU_DAY_MSUN;
    const double gm_km3_s2 = gm_au3_d2 * std::pow(units::AU_KM, 3) / (86400.0 * 86400.0);
    REQUIRE_THAT(gm_km3_s2, WithinRel(units::GM_SUN_KM3_S2, 1e-6));
    REQUIRE_THAT(units::SCHWARZSCHILD_KM_PER_MSUN, WithinRel(2.9532, 1e-4));
}
