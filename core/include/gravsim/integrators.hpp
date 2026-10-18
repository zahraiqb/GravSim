#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "gravsim/system.hpp"

namespace gravsim {

enum class Method {
    Euler = 0,     // explicit Euler, 1st order (pedagogical: shows secular energy drift)
    RK4 = 1,       // classic Runge-Kutta, 4th order, not symplectic
    Leapfrog = 2,  // kick-drift-kick velocity Verlet, 2nd order, symplectic, time-reversible
    Yoshida4 = 3,  // Yoshida (1990) triple-jump composition of leapfrog, 4th order, symplectic
    DOPRI5 = 4     // Dormand-Prince 5(4) with adaptive step size control
};

struct RKTableau;  // defined in integrators.cpp

const char* method_name(Method m);
bool method_from_string(const std::string& s, Method& out);

/// Owns a System and advances it in time.
///
/// Fixed-step methods split a requested interval T into n = ceil(|T|/dt) equal steps.
/// DOPRI5 treats dt as the initial step guess and adapts it to meet (rtol, atol).
/// Note: with post-Newtonian forces (velocity dependent) Leapfrog/Yoshida4 are no longer
/// exactly symplectic or time-reversible; DOPRI5 is recommended for relativistic runs.
class Simulation {
public:
    explicit Simulation(System sys, Method m = Method::Yoshida4, double dt = 1.0);

    System& system() { return sys_; }
    const System& system() const { return sys_; }

    Method method() const { return method_; }
    void set_method(Method m);
    double dt() const { return dt_; }
    void set_dt(double dt);
    void set_tolerances(double rtol, double atol);
    /// Call after modifying masses, positions or parameters of system() directly.
    void invalidate_cache() { cache_ok_ = false; }

    void advance(double T);                    // integrate by T (may be negative)
    std::uint64_t force_evaluations() const { return nevals_; }

private:
    using Vecs = std::vector<Vec3>;

    bool cache_valid() const;
    void fixed_step(double h);
    void euler(double h);
    void kdk(double h);
    void yoshida4(double h);
    void rk_stages(const RKTableau& T, double h);
    void rk_step(const RKTableau& T, double h);
    bool dopri_attempt(double h, double& err);
    void advance_dopri(double T);

    System sys_;
    Method method_;
    double dt_;
    double rtol_ = 1e-12, atol_ = 1e-14;
    double h_adapt_ = 0.0;
    std::uint64_t nevals_ = 0;

    Vecs acc_, cache_x_;
    bool cache_ok_ = false;
    Vecs xs_, vs_, xn_, vn_;
    std::array<Vecs, 7> kx_, kv_;
};

}  // namespace gravsim
