#include "gravsim/system.hpp"
#include <cmath>

namespace gravsim {

void System::add_body(const Body& b) {
    info.push_back({b.name, b.kind, b.radius_km});
    mass.push_back(b.mass);
    pos.push_back(b.pos);
    vel.push_back(b.vel);
}

Body System::body(std::size_t i) const {
    return {info[i].name, info[i].kind, mass[i], info[i].radius_km, pos[i], vel[i]};
}

void System::accelerations(const std::vector<Vec3>& x, const std::vector<Vec3>& v,
                           std::vector<Vec3>& a) const {
    const std::size_t n = mass.size();
    a.assign(n, Vec3{});
    const double G = params.G;
    const double eps2 = params.softening * params.softening;

    // Newtonian part, each pair visited once (Newton's third law).
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            const Vec3 d = x[j] - x[i];
            const double r2 = norm2(d) + eps2;
            const double inv_r3 = 1.0 / (r2 * std::sqrt(r2));
            a[i] += (G * mass[j] * inv_r3) * d;
            a[j] -= (G * mass[i] * inv_r3) * d;
        }
    }
    if (!params.post_newtonian) return;

    // 1PN Einstein-Infeld-Hoffmann equations of motion (beta = gamma = 1), harmonic coordinates.
    // Formulation of Moyer / Standish (JPL). mu_j = G m_j; uses the Newtonian a_j.
    //   a_i = sum_j mu_j (r_j - r_i)/r_ij^3 * { 1 - 4/c^2 sum_{l!=i} mu_l/r_il - 1/c^2 sum_{k!=j} mu_k/r_jk
    //          + v_i^2/c^2 + 2 v_j^2/c^2 - 4 v_i.v_j/c^2 - 3/(2c^2) [(r_i-r_j).v_j/r_ij]^2
    //          + 1/(2c^2) (r_j - r_i).a_j }
    //       + 1/c^2 sum_j mu_j/r_ij^3 { (r_i - r_j).(4 v_i - 3 v_j) } (v_i - v_j)
    //       + 7/(2c^2) sum_j mu_j a_j / r_ij
    const double ic2 = 1.0 / (params.c * params.c);
    const std::vector<Vec3> aN = a;  // Newtonian accelerations
    std::vector<double> phi(n, 0.0); // phi_i = sum_{l != i} mu_l / r_il
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 1; j < n; ++j) {
            const double r = norm(x[j] - x[i]);
            phi[i] += G * mass[j] / r;
            phi[j] += G * mass[i] / r;
        }
    for (std::size_t i = 0; i < n; ++i) {
        const double vi2 = norm2(v[i]);
        Vec3 corr{};
        for (std::size_t j = 0; j < n; ++j) {
            if (j == i) continue;
            const double mu_j = G * mass[j];
            const Vec3 rij = x[j] - x[i];
            const double r = norm(rij);
            const double r3 = r * r * r;
            const double nv = dot(x[i] - x[j], v[j]) / r;
            const double brace =
                -4.0 * phi[i] * ic2 - phi[j] * ic2 + vi2 * ic2 + 2.0 * norm2(v[j]) * ic2 -
                4.0 * dot(v[i], v[j]) * ic2 - 1.5 * nv * nv * ic2 + 0.5 * dot(rij, aN[j]) * ic2;
            corr += (mu_j / r3 * brace) * rij;
            const double s = dot(x[i] - x[j], 4.0 * v[i] - 3.0 * v[j]);
            corr += (ic2 * mu_j / r3 * s) * (v[i] - v[j]);
            corr += (3.5 * ic2 * mu_j / r) * aN[j];
        }
        a[i] += corr;
    }
}

double System::total_mass() const {
    double m = 0.0;
    for (double mi : mass) m += mi;
    return m;
}

double System::kinetic_energy() const {
    double k = 0.0;
    for (std::size_t i = 0; i < size(); ++i) k += 0.5 * mass[i] * norm2(vel[i]);
    return k;
}

double System::potential_energy() const {
    double u = 0.0;
    const double eps2 = params.softening * params.softening;
    for (std::size_t i = 0; i < size(); ++i)
        for (std::size_t j = i + 1; j < size(); ++j)
            u -= params.G * mass[i] * mass[j] / std::sqrt(norm2(pos[j] - pos[i]) + eps2);
    return u;
}

Vec3 System::angular_momentum() const {
    Vec3 L{};
    for (std::size_t i = 0; i < size(); ++i) L += mass[i] * cross(pos[i], vel[i]);
    return L;
}

Vec3 System::linear_momentum() const {
    Vec3 p{};
    for (std::size_t i = 0; i < size(); ++i) p += mass[i] * vel[i];
    return p;
}

Vec3 System::center_of_mass() const {
    Vec3 c{};
    for (std::size_t i = 0; i < size(); ++i) c += mass[i] * pos[i];
    return c / total_mass();
}

Vec3 System::center_of_mass_velocity() const { return linear_momentum() / total_mass(); }

void System::move_to_barycenter() {
    const Vec3 c = center_of_mass(), w = center_of_mass_velocity();
    for (std::size_t i = 0; i < size(); ++i) {
        pos[i] -= c;
        vel[i] -= w;
    }
}

}  // namespace gravsim
