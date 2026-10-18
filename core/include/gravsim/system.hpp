#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "gravsim/body.hpp"
#include "gravsim/units.hpp"
#include "gravsim/vec3.hpp"

namespace gravsim {

struct Params {
    double G = units::G_AU_DAY_MSUN;  // gravitational constant in the system's units
    double c = units::C_AU_DAY;       // speed of light in the system's units
    bool post_newtonian = false;      // enable 1PN (Einstein-Infeld-Hoffmann) corrections
    double softening = 0.0;           // Plummer softening length (Newtonian term only)
};

struct BodyInfo {
    std::string name;
    BodyKind kind = BodyKind::Other;
    double radius_km = 0.0;
};

/// N-body system in structure-of-arrays layout.
class System {
public:
    Params params;
    double time = 0.0;
    std::vector<BodyInfo> info;
    std::vector<double> mass;
    std::vector<Vec3> pos, vel;

    std::size_t size() const { return mass.size(); }
    void add_body(const Body& b);
    Body body(std::size_t i) const;

    /// Acceleration of every body for hypothetical positions x and velocities v.
    /// Newtonian: a_i = sum_j G m_j (x_j - x_i)/|x_j - x_i|^3. With params.post_newtonian the
    /// 1PN EIH corrections (which depend on v) are added. Cost O(N^2).
    void accelerations(const std::vector<Vec3>& x, const std::vector<Vec3>& v,
                       std::vector<Vec3>& a) const;

    // --- diagnostics (Newtonian definitions) ---
    double total_mass() const;
    double kinetic_energy() const;
    double potential_energy() const;
    double total_energy() const { return kinetic_energy() + potential_energy(); }
    Vec3 angular_momentum() const;   // about the origin
    Vec3 linear_momentum() const;
    Vec3 center_of_mass() const;
    Vec3 center_of_mass_velocity() const;

    /// Shift to the barycentric frame (COM at origin, zero total momentum).
    void move_to_barycenter();
};

}  // namespace gravsim
