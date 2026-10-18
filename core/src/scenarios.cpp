#include "gravsim/scenarios.hpp"
#include <cmath>
#include <fstream>
#include <sstream>
#include "gravsim/units.hpp"

namespace gravsim {

using units::DEG;
using units::G_AU_DAY_MSUN;
using units::SCHWARZSCHILD_KM_PER_MSUN;

System make_binary(double G, double m1, double m2, const Elements& rel) {
    System s;
    s.params.G = G;
    Body a;
    a.name = "Primary";
    a.mass = m1;
    Body b;
    b.name = "Secondary";
    b.mass = m2;
    Vec3 r, v;
    state_from_elements(G * (m1 + m2), rel, r, v);
    b.pos = r;
    b.vel = v;
    s.add_body(a);
    s.add_body(b);
    s.move_to_barycenter();
    return s;
}

namespace {

// Approximate J2000 mean elements: E.M. Standish, "Keplerian Elements for Approximate
// Positions of the Major Planets" (JPL), Table 1 (1800-2050 AD). Angles in degrees.
// Earth row is the Earth-Moon barycenter. inv_mass = Msun / (planet system mass).
struct PlanetRow {
    const char* name;
    BodyKind kind;
    double inv_mass, radius_km;
    double a, e, I, L, varpi, Omega;
};

const PlanetRow kPlanets[] = {
    {"Mercury", BodyKind::Planet, 6023600.0, 2439.7, 0.38709927, 0.20563593, 7.00497902, 252.25032350, 77.45779628, 48.33076593},
    {"Venus", BodyKind::Planet, 408523.71, 6051.8, 0.72333566, 0.00677672, 3.39467605, 181.97909950, 131.60246718, 76.67984255},
    {"Earth", BodyKind::Planet, 328900.56, 6371.0, 1.00000261, 0.01671123, -0.00001531, 100.46457166, 102.93768193, 0.0},
    {"Mars", BodyKind::Planet, 3098708.0, 3389.5, 1.52371034, 0.09339410, 1.84969142, -4.55343205, -23.94362959, 49.55953891},
    {"Jupiter", BodyKind::Planet, 1047.3486, 69911.0, 5.20288700, 0.04838624, 1.30439695, 34.39644051, 14.72847983, 100.47390909},
    {"Saturn", BodyKind::Planet, 3497.898, 58232.0, 9.53667594, 0.05386179, 2.48599187, 49.95424423, 92.59887831, 113.66242448},
    {"Uranus", BodyKind::Planet, 22902.98, 25362.0, 19.18916464, 0.04725744, 0.77263783, 313.23810451, 170.95427630, 74.01692503},
    {"Neptune", BodyKind::Planet, 19412.24, 24622.0, 30.06992276, 0.00859048, 1.77004347, -55.12002969, 44.96476227, 131.78422574},
};
const PlanetRow kPluto = {"Pluto", BodyKind::DwarfPlanet, 1.36e8, 1188.3, 39.48211675, 0.24882730, 17.14001206, 238.92903833, 224.06891629, 110.30393684};

// Orbit of `i`-th body added relative to a primary at index `p`, from elements in degrees.
void add_orbiting(System& s, std::size_t p, Body b, double a, double e, double inc_deg,
                  double Omega_deg, double omega_deg, double M_deg) {
    Elements el{a, e, inc_deg * DEG, Omega_deg * DEG, omega_deg * DEG, M_deg * DEG};
    Vec3 r, v;
    state_from_elements(s.params.G * (s.mass[p] + b.mass), el, r, v);
    b.pos = s.pos[p] + r;
    b.vel = s.vel[p] + v;
    s.add_body(b);
}

double wrap_deg(double d) {
    d = std::fmod(d, 360.0);
    if (d > 180.0) d -= 360.0;
    if (d < -180.0) d += 360.0;
    return d;
}

void add_planet(System& s, const PlanetRow& p) {
    Body b;
    b.name = p.name;
    b.kind = p.kind;
    b.mass = 1.0 / p.inv_mass;
    b.radius_km = p.radius_km;
    const double M = wrap_deg(p.L - p.varpi);
    const double omega = p.varpi - p.Omega;
    add_orbiting(s, 0, b, p.a, p.e, p.I, p.Omega, omega, M);
}

System solar_system(bool extended) {
    System s;
    Body sun;
    sun.name = "Sun";
    sun.kind = BodyKind::Star;
    sun.mass = 1.0;
    sun.radius_km = 695700.0;
    s.add_body(sun);
    for (const auto& p : kPlanets) add_planet(s, p);
    if (extended) {
        add_planet(s, kPluto);
        // Halley's comet (1P): approximate osculating elements (JPL SBDB), propagated from
        // perihelion 1986-02-09 (JD 2446470.5) to J2000 (JD 2451545.0): M ~ 66.4 deg.
        Body h;
        h.name = "Halley";
        h.kind = BodyKind::Comet;
        h.mass = 1.1e-16;
        h.radius_km = 5.5;
        add_orbiting(s, 0, h, 17.834, 0.96714, 162.26, 58.42, 111.33, 66.4);
    }
    s.move_to_barycenter();
    return s;
}

Scenario base(const char* name, const char* title, const char* desc) {
    Scenario sc;
    sc.name = name;
    sc.title = title;
    sc.description = desc;
    sc.time_unit = "day";
    return sc;
}

}  // namespace

std::vector<std::string> scenario_names() {
    return {"solar_system", "solar_system_extended", "alpha_centauri", "sgr_a_s2",
            "binary_black_hole", "kepler", "figure_eight", "pythagorean"};
}

bool make_scenario(const std::string& name, Scenario& out) {
    if (name == "solar_system") {
        out = base("solar_system", "Solar System",
                   "Sun and eight planets from J2000 mean elements (Standish/JPL), integrated in the "
                   "barycentric frame. Earth is the Earth-Moon barycenter.");
        out.system = solar_system(false);
        out.method = Method::Yoshida4;
        out.default_dt = 1.0;
        out.default_warp = 120.0;
        out.view_scale = 35.0;
        return true;
    }
    if (name == "solar_system_extended") {
        out = base("solar_system_extended", "Solar System + Pluto + Halley",
                   "Adds Pluto and Halley's comet (retrograde, e = 0.967). Uses the adaptive DOPRI5 "
                   "integrator to resolve the comet's close perihelion pass.");
        out.system = solar_system(true);
        out.method = Method::DOPRI5;
        out.default_dt = 1.0;
        out.default_warp = 365.25;
        out.view_scale = 40.0;
        return true;
    }
    if (name == "alpha_centauri") {
        out = base("alpha_centauri", "Alpha Centauri A+B",
                   "Nearest Sun-like binary: 1.1 + 0.907 Msun, a = 23.4 AU, e = 0.518, P ~ 80 yr.");
        Elements el{23.4, 0.5179, 79.2 * DEG, 204.85 * DEG, 231.65 * DEG, 0.0};
        out.system = make_binary(G_AU_DAY_MSUN, 1.1, 0.907, el);
        out.system.info[0] = {"Alpha Cen A", BodyKind::Star, 848000.0};
        out.system.info[1] = {"Alpha Cen B", BodyKind::Star, 598000.0};
        out.method = Method::Yoshida4;
        out.default_dt = 5.0;
        out.default_warp = 730.0;
        out.view_scale = 30.0;
        return true;
    }
    if (name == "sgr_a_s2") {
        out = base("sgr_a_s2", "S2 around Sagittarius A*",
                   "The star S2 orbits the 4.3 million solar-mass black hole at the Galactic Centre "
                   "(a ~ 1031 AU, e = 0.884, P ~ 16 yr). 1PN corrections on by default: pericenter "
                   "precesses ~12 arcmin per orbit.");
        Elements el{1031.0, 0.8844, 134.7 * DEG, 228.2 * DEG, 66.3 * DEG, 0.0};
        out.system = make_binary(G_AU_DAY_MSUN, 4.3e6, 13.0, el);
        out.system.info[0] = {"Sgr A*", BodyKind::BlackHole, SCHWARZSCHILD_KM_PER_MSUN * 4.3e6};
        out.system.info[1] = {"S2", BodyKind::Star, 4.5e6};
        out.system.params.post_newtonian = true;
        out.method = Method::DOPRI5;
        out.default_dt = 0.5;
        out.default_warp = 300.0;
        out.view_scale = 1200.0;
        out.post_newtonian = true;
        return true;
    }
    if (name == "binary_black_hole") {
        out = base("binary_black_hole", "Binary black hole (wide, 1PN)",
                   "36 + 29 Msun black holes (GW150914 masses) on a wide 1 AU orbit with 1PN dynamics. "
                   "No gravitational-wave emission is modelled, so this does not inspiral.");
        Elements el{1.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        out.system = make_binary(G_AU_DAY_MSUN, 36.0, 29.0, el);
        out.system.info[0] = {"BH 1", BodyKind::BlackHole, SCHWARZSCHILD_KM_PER_MSUN * 36.0};
        out.system.info[1] = {"BH 2", BodyKind::BlackHole, SCHWARZSCHILD_KM_PER_MSUN * 29.0};
        out.system.params.post_newtonian = true;
        out.method = Method::DOPRI5;
        out.default_dt = 0.1;
        out.default_warp = 6.0;
        out.view_scale = 1.4;
        out.post_newtonian = true;
        return true;
    }
    if (name == "kepler") {
        out = base("kepler", "Kepler two-body orbit (G = 1)",
                   "Mass ratio 10:1, a = 1, e = 0.5 in G = 1 units. Compare integrators against the "
                   "exact analytic solution.");
        Elements el{1.0, 0.5, 0.0, 0.0, 0.0, 0.0};
        out.system = make_binary(1.0, 1.0, 0.1, el);
        out.system.info[0] = {"Primary", BodyKind::Star, 0.0};
        out.system.info[1] = {"Secondary", BodyKind::Planet, 0.0};
        out.time_unit = "G=1";
        out.method = Method::Leapfrog;
        out.default_dt = 0.01;
        out.default_warp = 1.0;
        out.view_scale = 1.8;
        return true;
    }
    if (name == "figure_eight") {
        out = base("figure_eight", "Figure-eight three-body choreography",
                   "Chenciner-Montgomery (2000) periodic solution: three equal masses chase each other "
                   "along one figure-eight curve (G = m = 1, T = 6.3259).");
        System s;
        s.params.G = 1.0;
        const Vec3 p1(0.97000436, -0.24308753, 0.0), v3(-0.93240737, -0.86473146, 0.0);
        const Vec3 v12 = v3 * -0.5;
        Body b;
        b.mass = 1.0;
        b.kind = BodyKind::Star;
        b.name = "A"; b.pos = p1; b.vel = v12; s.add_body(b);
        b.name = "B"; b.pos = -p1; b.vel = v12; s.add_body(b);
        b.name = "C"; b.pos = Vec3(); b.vel = v3; s.add_body(b);
        out.system = s;
        out.time_unit = "G=1";
        out.method = Method::DOPRI5;
        out.default_dt = 0.001;
        out.default_warp = 1.0;
        out.view_scale = 1.6;
        return true;
    }
    if (name == "pythagorean") {
        out = base("pythagorean", "Pythagorean three-body problem",
                   "Burrau's problem: masses 3, 4, 5 released from rest at the vertices of a 3-4-5 "
                   "right triangle (G = 1). Chaotic, with close encounters; adaptive stepping essential.");
        System s;
        s.params.G = 1.0;
        Body b;
        b.kind = BodyKind::Star;
        b.name = "m=3"; b.mass = 3.0; b.pos = Vec3(1.0, 3.0, 0.0); s.add_body(b);
        b.name = "m=4"; b.mass = 4.0; b.pos = Vec3(-2.0, -1.0, 0.0); s.add_body(b);
        b.name = "m=5"; b.mass = 5.0; b.pos = Vec3(1.0, -1.0, 0.0); s.add_body(b);
        s.move_to_barycenter();
        out.system = s;
        out.time_unit = "G=1";
        out.method = Method::DOPRI5;
        out.default_dt = 0.001;
        out.default_warp = 0.5;
        out.view_scale = 4.0;
        return true;
    }
    return false;
}

bool load_csv(const std::string& path, System& out, std::string& error) {
    std::ifstream f(path);
    if (!f) {
        error = "cannot open " + path;
        return false;
    }
    System s;
    std::string line;
    int lineno = 0;
    while (std::getline(f, line)) {
        ++lineno;
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> t;
        std::stringstream ss(line);
        std::string tok;
        while (std::getline(ss, tok, ',')) t.push_back(tok);
        if (t.size() != 9) {
            error = "line " + std::to_string(lineno) + ": expected 9 columns";
            return false;
        }
        Body b;
        b.name = t[0];
        const std::string& k = t[1];
        b.kind = k == "star" ? BodyKind::Star : k == "planet" ? BodyKind::Planet
               : k == "dwarf" ? BodyKind::DwarfPlanet : k == "comet" ? BodyKind::Comet
               : k == "blackhole" ? BodyKind::BlackHole : BodyKind::Other;
        try {
            b.mass = std::stod(t[2]);
            b.pos = {std::stod(t[3]), std::stod(t[4]), std::stod(t[5])};
            b.vel = {std::stod(t[6]), std::stod(t[7]), std::stod(t[8])};
        } catch (...) {
            error = "line " + std::to_string(lineno) + ": bad number";
            return false;
        }
        s.add_body(b);
    }
    if (s.size() < 2) {
        error = "need at least two bodies";
        return false;
    }
    out = s;
    return true;
}

}  // namespace gravsim
