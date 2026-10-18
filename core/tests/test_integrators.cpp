#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <cmath>
#include "gravsim/integrators.hpp"
#include "gravsim/scenarios.hpp"
#include "gravsim/units.hpp"

using namespace gravsim;
using Catch::Matchers::WithinAbs;
using units::PI;

namespace {
constexpr double kM1 = 1.0, kM2 = 0.1;

Elements test_orbit(double e) { return Elements{1.0, e, 0.0, 0.0, 0.0, 0.3}; }
double period() { return 2 * PI * std::sqrt(1.0 / (kM1 + kM2)); }  // a = 1, G = 1

// Relative separation vector (secondary - primary) at time t from the exact Kepler solution.
Vec3 exact_rel(double e, double t) {
    Elements el = test_orbit(e);
    const double n = std::sqrt((kM1 + kM2) / (el.a * el.a * el.a));
    el.M += n * t;
    Vec3 r, v;
    state_from_elements(kM1 + kM2, el, r, v);
    return r;
}

double rel_err_after(Method m, double e, double dt, double T) {
    Simulation sim(make_binary(1.0, kM1, kM2, test_orbit(e)), m, dt);
    sim.advance(T);
    const Vec3 rel = sim.system().pos[1] - sim.system().pos[0];
    return norm(rel - exact_rel(e, T));
}
}  // namespace

TEST_CASE("Two-body problem matches the analytic Kepler solution", "[integrators][kepler]") {
    const double T = 10 * period();
    SECTION("DOPRI5 adaptive") {
        Simulation sim(make_binary(1.0, kM1, kM2, test_orbit(0.5)), Method::DOPRI5, 0.01);
        sim.set_tolerances(1e-13, 1e-15);
        sim.advance(T);
        const Vec3 rel = sim.system().pos[1] - sim.system().pos[0];
        REQUIRE(norm(rel - exact_rel(0.5, T)) < 1e-8);
    }
    SECTION("Yoshida4 fixed step") {
        REQUIRE(rel_err_after(Method::Yoshida4, 0.5, period() / 400, T) < 1e-3);
    }
    SECTION("Leapfrog fixed step") {
        REQUIRE(rel_err_after(Method::Leapfrog, 0.5, period() / 2000, T) < 1e-2);
    }
}

TEST_CASE("Convergence order matches theory", "[integrators][convergence]") {
    // Halve dt: the global error should shrink by 2^p. Moderate eccentricity, a few orbits.
    const double e = 0.2, T = 2 * period();
    auto order = [&](Method m, double dt) {
        const double e1 = rel_err_after(m, e, dt, T);
        const double e2 = rel_err_after(m, e, dt / 2, T);
        return std::log2(e1 / e2);
    };
    REQUIRE_THAT(order(Method::Euler, period() / 20000), WithinAbs(1.0, 0.1));
    REQUIRE_THAT(order(Method::Leapfrog, period() / 200), WithinAbs(2.0, 0.15));
    REQUIRE_THAT(order(Method::RK4, period() / 100), WithinAbs(4.0, 0.6));
    REQUIRE_THAT(order(Method::Yoshida4, period() / 100), WithinAbs(4.0, 0.3));
}

TEST_CASE("Symplectic integrators keep energy bounded; Euler does not", "[integrators][energy]") {
    const double T = 300 * period();
    // Returns the worst |dE/E| over the whole run; optionally also over the first/last third.
    auto max_energy_err = [&](Method m, double dt, double* first = nullptr, double* last = nullptr) {
        Simulation sim(make_binary(1.0, kM1, kM2, test_orbit(0.3)), m, dt);
        const double E0 = sim.system().total_energy();
        double worst = 0.0, w1 = 0.0, w3 = 0.0;
        const int chunks = 600;
        for (int i = 0; i < chunks; ++i) {
            sim.advance(T / chunks);
            const double err = std::fabs((sim.system().total_energy() - E0) / E0);
            worst = std::max(worst, err);
            if (i < chunks / 3) w1 = std::max(w1, err);
            if (i >= 2 * chunks / 3) w3 = std::max(w3, err);
        }
        if (first) *first = w1;
        if (last) *last = w3;
        return worst;
    };
    double first = 0, last = 0;
    const double lf = max_energy_err(Method::Leapfrog, period() / 100, &first, &last);
    REQUIRE(last < 1.2 * first);  // bounded oscillation, no secular drift
    const double yo = max_energy_err(Method::Yoshida4, period() / 100);
    const double eu = max_energy_err(Method::Euler, period() / 100);
    REQUIRE(lf < 5e-3);
    REQUIRE(yo < 1e-4);
    REQUIRE(yo < lf / 20);
    REQUIRE(eu > 100 * lf);
}

TEST_CASE("Leapfrog is time-reversible", "[integrators][symmetry]") {
    Simulation sim(make_binary(1.0, kM1, kM2, test_orbit(0.4)), Method::Leapfrog, period() / 500);
    const auto x0 = sim.system().pos;
    const auto v0 = sim.system().vel;
    sim.advance(7 * period());
    for (auto& v : sim.system().vel) v = -1.0 * v;
    sim.invalidate_cache();
    sim.advance(7 * period());
    for (std::size_t i = 0; i < x0.size(); ++i) {
        REQUIRE(norm(sim.system().pos[i] - x0[i]) < 1e-9);
        REQUIRE(norm(sim.system().vel[i] + v0[i]) < 1e-9);
    }
}

TEST_CASE("Backward integration with DOPRI5 returns to the start", "[integrators]") {
    Simulation sim(make_binary(1.0, kM1, kM2, test_orbit(0.4)), Method::DOPRI5, 0.01);
    sim.set_tolerances(1e-13, 1e-15);
    const auto x0 = sim.system().pos;
    sim.advance(3 * period());
    sim.advance(-3 * period());
    for (std::size_t i = 0; i < x0.size(); ++i) REQUIRE(norm(sim.system().pos[i] - x0[i]) < 1e-9);
    REQUIRE_THAT(sim.system().time, WithinAbs(0.0, 1e-12));
}
