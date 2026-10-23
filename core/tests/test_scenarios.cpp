#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include "gravsim/integrators.hpp"
#include "gravsim/scenarios.hpp"

using namespace gravsim;

static double orbital_period(double mu, double a) { return 2.0 * 3.14159265358979323846 * std::sqrt(a * a * a / mu); }
using Catch::Matchers::WithinRel;

TEST_CASE("Every scenario builds and stays finite", "[scenarios]") {
    for (const auto& name : scenario_names()) {
        Scenario sc;
        REQUIRE(make_scenario(name, sc));
        REQUIRE(sc.system.size() >= 2);
        Simulation sim(sc.system, sc.method, sc.default_dt);
        sim.advance(sc.default_dt * 200.0);
        for (const Vec3& p : sim.system().pos) {
            INFO(name);
            REQUIRE(std::isfinite(p.x + p.y + p.z));
        }
    }
}

TEST_CASE("Galilean moons have their real orbital periods", "[scenarios]") {
    Scenario sc;
    REQUIRE(make_scenario("galilean_moons", sc));
    const auto& s = sc.system;
    for (auto [idx, days] : {std::pair{1, 1.769}, {2, 3.551}, {3, 7.155}, {4, 16.69}}) {
        const double a = norm(s.pos[idx] - s.pos[0]);
        const double mu = s.params.G * (s.mass[0] + s.mass[idx]);
        CHECK_THAT(orbital_period(mu, a), WithinRel(days, 0.02));
    }
}

TEST_CASE("Kepler-16b period is about 229 days", "[scenarios]") {
    Scenario sc;
    REQUIRE(make_scenario("kepler16_tatooine", sc));
    const auto& s = sc.system;
    const double a = norm(s.pos[2]);  // barycentric: companion mass inside is ~M_total
    const double mu = s.params.G * (s.mass[0] + s.mass[1] + s.mass[2]);
    CHECK_THAT(orbital_period(mu, a), WithinRel(228.8, 0.03));
}

TEST_CASE("Tidal forces stretch the falling astronaut", "[scenarios][tides]") {
    Scenario sc;
    REQUIRE(make_scenario("astronaut_bh", sc));
    Simulation sim(sc.system, sc.method, sc.default_dt);
    const auto len = [&] { return norm(sim.system().pos[7] - sim.system().pos[1]); };  // head - feet
    const double before = len();
    double maxlen = before;
    for (int i = 0; i < 400; ++i) {
        sim.advance(0.25);
        if (norm(sim.system().pos[1]) < 3.0) break;  // stop before the singular region
        maxlen = std::max(maxlen, len());
    }
    CHECK(maxlen > 1.8 * before);  // measured ~2.1x by r = 3 for a plunge from r = 24
}

TEST_CASE("Big Bang toy model expands and conserves momentum", "[scenarios]") {
    Scenario sc;
    REQUIRE(make_scenario("big_bang", sc));
    auto rms = [](const System& s) {
        double q = 0; for (const Vec3& p : s.pos) q += norm2(p);
        return std::sqrt(q / static_cast<double>(s.size()));
    };
    const double r0 = rms(sc.system);
    Simulation sim(sc.system, sc.method, sc.default_dt);
    sim.advance(0.5);
    CHECK(rms(sim.system()) > 3.0 * r0);
    CHECK(norm(sim.system().linear_momentum()) < 1e-10);
}

TEST_CASE("Changing a mass changes the dynamics", "[scenarios]") {
    Scenario sc;
    REQUIRE(make_scenario("solar_system", sc));
    Simulation a(sc.system, sc.method, 1.0), b(sc.system, sc.method, 1.0);
    b.system().mass[5] *= 100.0;  // heavier Jupiter
    b.invalidate_cache();
    a.advance(365.0); b.advance(365.0);
    CHECK(norm(a.system().pos[3] - b.system().pos[3]) > 1e-8);  // Earth feels it
}

TEST_CASE("Scenario speed (informational)", "[.perf]") {
    for (const char* n : {"big_bang", "astronaut_bh"}) {
        Scenario sc; make_scenario(n, sc);
        Simulation sim(sc.system, sc.method, sc.default_dt);
        auto t0 = std::chrono::steady_clock::now();
        sim.advance(n[0] == 'b' ? 1.0 : 60.0);
        auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        WARN(n << ": " << ms << " ms");
    }
}
