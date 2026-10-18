#pragma once
#include <string>
#include "gravsim/vec3.hpp"

namespace gravsim {

enum class BodyKind { Star = 0, Planet = 1, DwarfPlanet = 2, Comet = 3, BlackHole = 4, Other = 5 };

/// Descriptive data used for construction and rendering. Dynamical state lives in System (SoA).
struct Body {
    std::string name;
    BodyKind kind = BodyKind::Other;
    double mass = 0.0;       // in the system's mass unit (solar masses by default)
    double radius_km = 0.0;  // physical radius (Schwarzschild radius for black holes); display only
    Vec3 pos;                // length unit of the system (AU by default)
    Vec3 vel;                // length unit per time unit (AU/day by default)
};

}  // namespace gravsim
