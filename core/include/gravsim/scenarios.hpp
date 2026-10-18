#pragma once
#include <string>
#include <vector>
#include "gravsim/integrators.hpp"
#include "gravsim/orbit.hpp"
#include "gravsim/system.hpp"

namespace gravsim {

struct Scenario {
    std::string name;         // machine id, e.g. "solar_system"
    std::string title;
    std::string description;
    std::string time_unit;    // "day" or "G=1"
    System system;
    Method method = Method::Yoshida4;
    double default_dt = 1.0;
    double default_warp = 100.0;   // simulated time units per real second in the viewer
    double view_scale = 10.0;      // length that should fit in the viewer
    bool post_newtonian = false;
};

std::vector<std::string> scenario_names();
/// Returns false if the name is unknown.
bool make_scenario(const std::string& name, Scenario& out);

/// Two-body system built from relative elements; result is in the barycentric frame.
System make_binary(double G, double m1, double m2, const Elements& rel);

/// Load a CSV: name,kind,mass,x,y,z,vx,vy,vz (kind: star|planet|dwarf|comet|blackhole|other).
/// Lines starting with '#' are ignored. Units: Msun, AU, AU/day (e.g. JPL Horizons vectors).
bool load_csv(const std::string& path, System& out, std::string& error);

}  // namespace gravsim
