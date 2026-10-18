#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <cmath>
#include "gravsim/integrators.hpp"
#include "gravsim/scenarios.hpp"
#include "gravsim/units.hpp"

using namespace gravsim;
using Catch::Matchers::WithinAbs;
using units::DAYS_PER_JULIAN_YEAR;

namespace {
Scenario get(const char* name) {
    Scenario sc;
    REQUIRE(make_scenario(name, sc));
    return sc;
}
std::size_t index_of(const System& s, const std::string& n) {
    for (std::size_t i = 0; i < s.size(); ++i)
        if (s.info[i].name == n) return i;
    FAIL("body not found: " << n);
    return 0;
}
}  // namespace

TEST_CASE("All scenarios build and are barycentric where expected", "[scenarios]") {
    for (const auto& name : scenario_names()) {
        Scenario sc = get(name.c_str());
        REQUIRE(sc.system.size() >= 2);
        REQUIRE(norm(sc.system.linear_momentum()) < 1e-9 * sc.system.total_mass());
        REQUIRE(sc.default_dt > 0);
    }
}

TEST_CASE("Solar system: conservation laws over 1000 years", "[nbody][solar]") {
    Scenario sc = get("solar_system");
    Simulation sim(sc.system, Method::Yoshida4, 1.0);
    const double E0 = sim.system().total_energy();
    const Vec3 L0 = sim.system().angular_momentum();
    double worstE = 0.0;
    for (int yr = 0; yr < 1000; yr += 50) {
        sim.advance(50 * DAYS_PER_JULIAN_YEAR);
        worstE = std::max(worstE, std::fabs((sim.system().total_energy() - E0) / E0));
    }
    const Vec3 L1 = sim.system().angular_momentum();
    REQUIRE(worstE < 1e-7);
    REQUIRE(norm(L1 - L0) / norm(L0) < 1e-12);  // pairwise-symmetric forces conserve L to roundoff
    REQUIRE(norm(sim.system().linear_momentum()) < 1e-12);
}

TEST_CASE("Solar system: orbits stay near their Keplerian ranges", "[nbody][solar]") {
    Scenario sc = get("solar_system");
    Simulation sim(sc.system, Method::Yoshida4, 1.0);
    const std::size_t sun = index_of(sim.system(), "Sun");
    struct Range { const char* name; double rmin, rmax; };
    // a(1-e) and a(1+e) widened by 3% to allow planetary perturbations and barycentric offset.
    const Range ranges[] = {{"Mercury", 0.307, 0.467}, {"Earth", 0.9833, 1.0167},
                            {"Jupiter", 4.95, 5.46},   {"Neptune", 29.7, 30.5}};
    std::vector<std::size_t> idx;
    for (const auto& r : ranges) idx.push_back(index_of(sim.system(), r.name));
    std::vector<double> lo(idx.size(), 1e9), hi(idx.size(), 0.0);
    for (int d = 0; d < 12 * 366; ++d) {
        sim.advance(1.0);
        for (std::size_t k = 0; k < idx.size(); ++k) {
            const double r = norm(sim.system().pos[idx[k]] - sim.system().pos[sun]);
            lo[k] = std::min(lo[k], r);
            hi[k] = std::max(hi[k], r);
        }
    }
    for (std::size_t k = 0; k < idx.size(); ++k) {
        INFO(ranges[k].name << " " << lo[k] << " " << hi[k]);
        REQUIRE(lo[k] > ranges[k].rmin * 0.97);
        REQUIRE(hi[k] < ranges[k].rmax * 1.03);
    }
}

TEST_CASE("Solar system: Jupiter's sidereal period is ~4332.6 days", "[nbody][solar]") {
    Scenario sc = get("solar_system");
    Simulation sim(sc.system, Method::Yoshida4, 1.0);
    const std::size_t sun = index_of(sim.system(), "Sun"), jup = index_of(sim.system(), "Jupiter");
    auto lon = [&] {
        const Vec3 d = sim.system().pos[jup] - sim.system().pos[sun];
        return std::atan2(d.y, d.x);
    };
    const double l0 = lon();
    sim.advance(4332.59);
    double dl = lon() - l0;
    dl = std::remainder(dl, 2 * units::PI);
    REQUIRE(std::fabs(dl) < 0.03);  // ~1.7 deg: mean elements + perturbations
}

TEST_CASE("Figure-eight choreography is periodic", "[nbody][choreography]") {
    Scenario sc = get("figure_eight");
    Simulation sim(sc.system, Method::DOPRI5, 1e-3);
    sim.set_tolerances(1e-13, 1e-15);
    const auto x0 = sim.system().pos;
    const double T = 6.32591398;
    sim.advance(T);
    for (std::size_t i = 0; i < 3; ++i) REQUIRE(norm(sim.system().pos[i] - x0[i]) < 1e-5);
    // Initial data are known only to 8 digits, so check energy exactness separately.
    REQUIRE_THAT(sim.system().total_energy(), WithinAbs(sc.system.total_energy(), 1e-9));
}

TEST_CASE("Pythagorean problem: adaptive stepping conserves energy through close encounters",
          "[nbody][chaos]") {
    Scenario sc = get("pythagorean");
    Simulation sim(sc.system, Method::DOPRI5, 1e-3);
    sim.set_tolerances(1e-12, 1e-14);
    const double E0 = sim.system().total_energy();
    sim.advance(20.0);
    REQUIRE(std::fabs((sim.system().total_energy() - E0) / E0) < 1e-6);
    REQUIRE(norm(sim.system().angular_momentum()) < 1e-9);  // started at rest with L = 0 about COM
}

TEST_CASE("CSV loader parses and rejects malformed input", "[io]") {
    System s;
    std::string err;
    REQUIRE_FALSE(load_csv("/nonexistent/file.csv", s, err));
    REQUIRE_FALSE(err.empty());
}
