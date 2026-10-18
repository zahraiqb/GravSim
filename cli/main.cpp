// gravsim command-line driver: run a scenario and write CSV (positions + conservation diagnostics).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include "gravsim/integrators.hpp"
#include "gravsim/scenarios.hpp"

using namespace gravsim;

static void usage() {
    std::puts(
        "gravsim - N-body gravity simulator\n\n"
        "Usage: gravsim [options]\n"
        "  --list                 list built-in scenarios\n"
        "  --scenario NAME        built-in scenario (default solar_system)\n"
        "  --csv FILE             initial conditions CSV (name,kind,mass,x,y,z,vx,vy,vz; Msun/AU/AU per day)\n"
        "  --method M             euler | rk4 | leapfrog | yoshida4 | dopri5 (default: scenario's)\n"
        "  --dt X                 step (initial step for dopri5), in scenario time units\n"
        "  --time T               simulated duration, scenario time units\n"
        "  --years Y              simulated duration in Julian years (day-based scenarios)\n"
        "  --sample S             output interval (default: duration/1000)\n"
        "  --pn / --no-pn         enable / disable 1PN relativistic corrections\n"
        "  --rtol R --atol A      DOPRI5 tolerances (default 1e-12 / 1e-14)\n"
        "  --out FILE             write CSV to FILE instead of stdout\n"
        "Output columns: t, dE_rel, dL_rel, then x,y,z for each body.");
}

int main(int argc, char** argv) {
    std::string scenario = "solar_system", csv, out_path, method_s;
    double dt = -1, T = -1, sample = -1, rtol = 1e-12, atol = 1e-14;
    int pn = -1;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) { std::fprintf(stderr, "missing value for %s\n", a.c_str()); std::exit(2); }
            return argv[++i];
        };
        if (a == "--help" || a == "-h") { usage(); return 0; }
        else if (a == "--list") {
            for (auto& n : scenario_names()) {
                Scenario sc; make_scenario(n, sc);
                std::printf("%-24s %s\n", n.c_str(), sc.title.c_str());
            }
            return 0;
        }
        else if (a == "--scenario") scenario = next();
        else if (a == "--csv") csv = next();
        else if (a == "--method") method_s = next();
        else if (a == "--dt") dt = std::atof(next());
        else if (a == "--time") T = std::atof(next());
        else if (a == "--years") T = std::atof(next()) * units::DAYS_PER_JULIAN_YEAR;
        else if (a == "--sample") sample = std::atof(next());
        else if (a == "--pn") pn = 1;
        else if (a == "--no-pn") pn = 0;
        else if (a == "--rtol") rtol = std::atof(next());
        else if (a == "--atol") atol = std::atof(next());
        else if (a == "--out") out_path = next();
        else { std::fprintf(stderr, "unknown option %s (try --help)\n", a.c_str()); return 2; }
    }

    Scenario sc;
    if (!csv.empty()) {
        std::string err;
        if (!load_csv(csv, sc.system, err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
        sc.system.move_to_barycenter();
        sc.method = Method::DOPRI5;
        sc.name = csv;
        sc.default_dt = 1.0;
    } else if (!make_scenario(scenario, sc)) {
        std::fprintf(stderr, "unknown scenario '%s' (try --list)\n", scenario.c_str());
        return 1;
    }
    Method m = sc.method;
    if (!method_s.empty() && !method_from_string(method_s, m)) {
        std::fprintf(stderr, "unknown method '%s'\n", method_s.c_str());
        return 2;
    }
    if (pn >= 0) sc.system.params.post_newtonian = pn == 1;
    if (dt <= 0) dt = sc.default_dt;
    if (T <= 0) T = 10.0 * units::DAYS_PER_JULIAN_YEAR;
    if (sample <= 0) sample = T / 1000.0;

    Simulation sim(sc.system, m, dt);
    sim.set_tolerances(rtol, atol);

    std::ofstream file;
    if (!out_path.empty()) file.open(out_path);
    std::ostream& os = out_path.empty() ? std::cout : file;
    os.precision(17);

    const double E0 = sim.system().total_energy();
    const double L0 = norm(sim.system().angular_momentum());
    os << "t,dE_rel,dL_rel";
    for (auto& bi : sim.system().info) os << ',' << bi.name << "_x," << bi.name << "_y," << bi.name << "_z";
    os << '\n';
    auto row = [&] {
        const System& s = sim.system();
        os << s.time << ',' << (s.total_energy() - E0) / std::fabs(E0) << ','
           << (norm(s.angular_momentum()) - L0) / L0;
        for (auto& p : s.pos) os << ',' << p.x << ',' << p.y << ',' << p.z;
        os << '\n';
    };
    row();
    double done = 0.0;
    while (done < T - 1e-12 * T) {
        const double h = std::min(sample, T - done);
        sim.advance(h);
        done += h;
        row();
    }
    std::fprintf(stderr, "done: %s, method=%s, t=%g, force evaluations=%llu\n", sc.name.c_str(),
                 method_name(m), sim.system().time,
                 static_cast<unsigned long long>(sim.force_evaluations()));
    return 0;
}
