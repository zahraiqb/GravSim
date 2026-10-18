// Flat C API exported to JavaScript via Emscripten. State lives in a single global simulation.
// All pointers returned stay valid until the next call that returns the same kind of buffer.
#include <emscripten/emscripten.h>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include "gravsim/integrators.hpp"
#include "gravsim/scenarios.hpp"

using namespace gravsim;

namespace {
std::unique_ptr<Simulation> g_sim;
Scenario g_scenario;
std::vector<double> g_pos, g_vel, g_diag(8);
std::string g_str;
double g_E0 = 1.0, g_L0 = 1.0;

void rebaseline() {
    const System& s = g_sim->system();
    g_E0 = s.total_energy();
    g_L0 = norm(s.angular_momentum());
    if (g_L0 == 0.0) g_L0 = 1.0;
}
const char* keep(std::string s) {
    g_str = std::move(s);
    return g_str.c_str();
}
bool ok(int i) { return g_sim && i >= 0 && static_cast<std::size_t>(i) < g_sim->system().size(); }
}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE int gs_scenario_count() { return static_cast<int>(scenario_names().size()); }

EMSCRIPTEN_KEEPALIVE const char* gs_scenario_name(int i) {
    auto n = scenario_names();
    return keep(i >= 0 && i < static_cast<int>(n.size()) ? n[i] : "");
}
EMSCRIPTEN_KEEPALIVE const char* gs_scenario_title(int i) {
    Scenario sc;
    auto n = scenario_names();
    return keep(i >= 0 && i < static_cast<int>(n.size()) && make_scenario(n[i], sc) ? sc.title : "");
}
EMSCRIPTEN_KEEPALIVE const char* gs_scenario_description(int i) {
    Scenario sc;
    auto n = scenario_names();
    return keep(i >= 0 && i < static_cast<int>(n.size()) && make_scenario(n[i], sc) ? sc.description : "");
}

/// Load (or reload) scenario i. Returns the number of bodies, or -1.
EMSCRIPTEN_KEEPALIVE int gs_load(int i) {
    auto n = scenario_names();
    if (i < 0 || i >= static_cast<int>(n.size()) || !make_scenario(n[i], g_scenario)) return -1;
    g_sim = std::make_unique<Simulation>(g_scenario.system, g_scenario.method, g_scenario.default_dt);
    rebaseline();
    return static_cast<int>(g_sim->system().size());
}

/// Restore the loaded scenario's initial conditions, keeping current method/dt/PN settings.
EMSCRIPTEN_KEEPALIVE void gs_reset() {
    if (!g_sim) return;
    const Method m = g_sim->method();
    const double dt = g_sim->dt();
    const bool pn = g_sim->system().params.post_newtonian;
    g_sim = std::make_unique<Simulation>(g_scenario.system, m, dt);
    g_sim->system().params.post_newtonian = pn;
    rebaseline();
}

EMSCRIPTEN_KEEPALIVE const char* gs_time_unit() { return keep(g_scenario.time_unit); }
EMSCRIPTEN_KEEPALIVE double gs_default_dt() { return g_scenario.default_dt; }
EMSCRIPTEN_KEEPALIVE double gs_default_warp() { return g_scenario.default_warp; }
EMSCRIPTEN_KEEPALIVE double gs_view_scale() { return g_scenario.view_scale; }
EMSCRIPTEN_KEEPALIVE int gs_default_method() { return static_cast<int>(g_scenario.method); }
EMSCRIPTEN_KEEPALIVE int gs_default_pn() { return g_scenario.post_newtonian ? 1 : 0; }

EMSCRIPTEN_KEEPALIVE int gs_body_count() { return g_sim ? static_cast<int>(g_sim->system().size()) : 0; }
EMSCRIPTEN_KEEPALIVE const char* gs_body_name(int i) { return keep(ok(i) ? g_sim->system().info[i].name : ""); }
EMSCRIPTEN_KEEPALIVE int gs_body_kind(int i) { return ok(i) ? static_cast<int>(g_sim->system().info[i].kind) : 5; }
EMSCRIPTEN_KEEPALIVE double gs_body_mass(int i) { return ok(i) ? g_sim->system().mass[i] : 0.0; }
EMSCRIPTEN_KEEPALIVE double gs_body_radius_km(int i) { return ok(i) ? g_sim->system().info[i].radius_km : 0.0; }

EMSCRIPTEN_KEEPALIVE void gs_set_method(int m) {
    if (g_sim && m >= 0 && m <= 4) g_sim->set_method(static_cast<Method>(m));
}
EMSCRIPTEN_KEEPALIVE void gs_set_dt(double dt) {
    if (g_sim && dt > 0.0) g_sim->set_dt(dt);
}
EMSCRIPTEN_KEEPALIVE void gs_set_pn(int on) {
    if (!g_sim) return;
    g_sim->system().params.post_newtonian = on != 0;
    g_sim->invalidate_cache();
    rebaseline();  // the conserved 1PN energy differs from the Newtonian one
}
EMSCRIPTEN_KEEPALIVE void gs_set_tolerance(double rtol) {
    if (g_sim && rtol > 0.0) g_sim->set_tolerances(rtol, rtol * 1e-2);
}

/// Advance by T simulated time units (>= 0). Returns the new time.
EMSCRIPTEN_KEEPALIVE double gs_advance(double T) {
    if (!g_sim) return 0.0;
    g_sim->advance(T);
    return g_sim->system().time;
}

/// Flat [x0,y0,z0,x1,...] positions in the scenario's length unit.
EMSCRIPTEN_KEEPALIVE const double* gs_positions() {
    if (!g_sim) return nullptr;
    const auto& p = g_sim->system().pos;
    g_pos.resize(3 * p.size());
    for (std::size_t i = 0; i < p.size(); ++i) {
        g_pos[3 * i] = p[i].x;
        g_pos[3 * i + 1] = p[i].y;
        g_pos[3 * i + 2] = p[i].z;
    }
    return g_pos.data();
}
EMSCRIPTEN_KEEPALIVE const double* gs_velocities() {
    if (!g_sim) return nullptr;
    const auto& v = g_sim->system().vel;
    g_vel.resize(3 * v.size());
    for (std::size_t i = 0; i < v.size(); ++i) {
        g_vel[3 * i] = v[i].x;
        g_vel[3 * i + 1] = v[i].y;
        g_vel[3 * i + 2] = v[i].z;
    }
    return g_vel.data();
}

/// [time, kinetic, potential, total energy, dE/E0, |L|, dL/L0, force evaluations]
EMSCRIPTEN_KEEPALIVE const double* gs_diagnostics() {
    if (!g_sim) return nullptr;
    const System& s = g_sim->system();
    const double k = s.kinetic_energy(), u = s.potential_energy();
    g_diag[0] = s.time;
    g_diag[1] = k;
    g_diag[2] = u;
    g_diag[3] = k + u;
    g_diag[4] = (k + u - g_E0) / std::fabs(g_E0);
    g_diag[5] = norm(s.angular_momentum());
    g_diag[6] = (g_diag[5] - g_L0) / g_L0;
    g_diag[7] = static_cast<double>(g_sim->force_evaluations());
    return g_diag.data();
}

}  // extern "C"
