#include "gravsim/integrators.hpp"
#include <algorithm>
#include <cmath>

namespace gravsim {

const char* method_name(Method m) {
    switch (m) {
        case Method::Euler: return "euler";
        case Method::RK4: return "rk4";
        case Method::Leapfrog: return "leapfrog";
        case Method::Yoshida4: return "yoshida4";
        case Method::DOPRI5: return "dopri5";
    }
    return "?";
}

bool method_from_string(const std::string& s, Method& out) {
    for (int i = 0; i <= 4; ++i) {
        if (s == method_name(static_cast<Method>(i))) {
            out = static_cast<Method>(i);
            return true;
        }
    }
    return false;
}

// Explicit Runge-Kutta tableau applied to the first-order system x' = v, v' = a(x, v).
struct RKTableau {
    int s;
    double a[7][7];
    double b[7];
    double e[7];  // b - b_embedded; used for error estimation
};

namespace {

const RKTableau kRK4 = {
    4,
    {{0, 0, 0, 0, 0, 0, 0},
     {0.5, 0, 0, 0, 0, 0, 0},
     {0, 0.5, 0, 0, 0, 0, 0},
     {0, 0, 1.0, 0, 0, 0, 0}},
    {1.0 / 6, 1.0 / 3, 1.0 / 3, 1.0 / 6, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0}};

// Dormand & Prince (1980), 5(4) pair. The 5th-order solution is propagated.
const RKTableau kDOPRI5 = {
    7,
    {{0, 0, 0, 0, 0, 0, 0},
     {1.0 / 5, 0, 0, 0, 0, 0, 0},
     {3.0 / 40, 9.0 / 40, 0, 0, 0, 0, 0},
     {44.0 / 45, -56.0 / 15, 32.0 / 9, 0, 0, 0, 0},
     {19372.0 / 6561, -25360.0 / 2187, 64448.0 / 6561, -212.0 / 729, 0, 0, 0},
     {9017.0 / 3168, -355.0 / 33, 46732.0 / 5247, 49.0 / 176, -5103.0 / 18656, 0, 0},
     {35.0 / 384, 0, 500.0 / 1113, 125.0 / 192, -2187.0 / 6784, 11.0 / 84, 0}},
    {35.0 / 384, 0, 500.0 / 1113, 125.0 / 192, -2187.0 / 6784, 11.0 / 84, 0},
    {71.0 / 57600, 0, -71.0 / 16695, 71.0 / 1920, -17253.0 / 339200, 22.0 / 525, -1.0 / 40}};

}  // namespace

Simulation::Simulation(System sys, Method m, double dt)
    : sys_(std::move(sys)), method_(m), dt_(dt) {}

void Simulation::set_method(Method m) {
    method_ = m;
    cache_ok_ = false;
}

void Simulation::set_dt(double dt) {
    dt_ = dt;
    h_adapt_ = 0.0;
}

void Simulation::set_tolerances(double rtol, double atol) {
    rtol_ = rtol;
    atol_ = atol;
}

bool Simulation::cache_valid() const {
    return cache_ok_ && cache_x_.size() == sys_.pos.size() && cache_x_ == sys_.pos;
}

// ---------------------------------------------------------------- fixed-step methods

void Simulation::euler(double h) {
    sys_.accelerations(sys_.pos, sys_.vel, acc_);
    ++nevals_;
    for (std::size_t i = 0; i < sys_.size(); ++i) {
        sys_.pos[i] += h * sys_.vel[i];  // uses the old velocity (explicit Euler)
        sys_.vel[i] += h * acc_[i];
    }
}

// Kick-drift-kick leapfrog (velocity Verlet). Newtonian acceleration at the end of a step is
// reused as the start of the next, so there is one force evaluation per step.
void Simulation::kdk(double h) {
    auto& x = sys_.pos;
    auto& v = sys_.vel;
    const std::size_t n = sys_.size();
    const bool pn = sys_.params.post_newtonian;  // velocity-dependent force: no caching
    if (pn || !cache_valid()) {
        sys_.accelerations(x, v, acc_);
        ++nevals_;
    }
    for (std::size_t i = 0; i < n; ++i) v[i] += (0.5 * h) * acc_[i];
    for (std::size_t i = 0; i < n; ++i) x[i] += h * v[i];
    sys_.accelerations(x, v, acc_);
    ++nevals_;
    for (std::size_t i = 0; i < n; ++i) v[i] += (0.5 * h) * acc_[i];
    if (!pn) {
        cache_x_ = x;
        cache_ok_ = true;
    } else {
        cache_ok_ = false;
    }
}

// Yoshida (1990): S4(h) = S2(w1 h) S2(w0 h) S2(w1 h) with
// w1 = 1/(2 - 2^(1/3)), w0 = -2^(1/3)/(2 - 2^(1/3)).
void Simulation::yoshida4(double h) {
    const double c = std::cbrt(2.0);
    const double w1 = 1.0 / (2.0 - c);
    const double w0 = -c / (2.0 - c);
    kdk(w1 * h);
    kdk(w0 * h);
    kdk(w1 * h);
}

void Simulation::rk_stages(const RKTableau& T, double h) {
    const std::size_t n = sys_.size();
    xs_.resize(n);
    vs_.resize(n);
    for (int s = 0; s < T.s; ++s) {
        for (std::size_t i = 0; i < n; ++i) {
            Vec3 dx{}, dv{};
            for (int j = 0; j < s; ++j) {
                if (T.a[s][j] != 0.0) {
                    dx += T.a[s][j] * kx_[j][i];
                    dv += T.a[s][j] * kv_[j][i];
                }
            }
            xs_[i] = sys_.pos[i] + h * dx;
            vs_[i] = sys_.vel[i] + h * dv;
        }
        kx_[s] = vs_;
        sys_.accelerations(xs_, vs_, kv_[s]);
        ++nevals_;
    }
}

void Simulation::rk_step(const RKTableau& T, double h) {
    rk_stages(T, h);
    const std::size_t n = sys_.size();
    for (std::size_t i = 0; i < n; ++i) {
        Vec3 dx{}, dv{};
        for (int s = 0; s < T.s; ++s) {
            dx += T.b[s] * kx_[s][i];
            dv += T.b[s] * kv_[s][i];
        }
        sys_.pos[i] += h * dx;
        sys_.vel[i] += h * dv;
    }
}

void Simulation::fixed_step(double h) {
    switch (method_) {
        case Method::Euler: euler(h); break;
        case Method::RK4: rk_step(kRK4, h); break;
        case Method::Leapfrog: kdk(h); break;
        case Method::Yoshida4: yoshida4(h); break;
        case Method::DOPRI5: rk_step(kDOPRI5, h); break;
    }
    if (method_ == Method::Euler || method_ == Method::RK4 || method_ == Method::DOPRI5)
        cache_ok_ = false;
    sys_.time += h;
}

// ---------------------------------------------------------------- adaptive DOPRI5

bool Simulation::dopri_attempt(double h, double& err) {
    const RKTableau& T = kDOPRI5;
    rk_stages(T, h);
    const std::size_t n = sys_.size();
    xn_.resize(n);
    vn_.resize(n);
    double sum = 0.0;
    auto sc = [&](double y, double yn) { return atol_ + rtol_ * std::max(std::fabs(y), std::fabs(yn)); };
    for (std::size_t i = 0; i < n; ++i) {
        Vec3 dx{}, dv{}, ex{}, ev{};
        for (int s = 0; s < T.s; ++s) {
            dx += T.b[s] * kx_[s][i];
            dv += T.b[s] * kv_[s][i];
            ex += T.e[s] * kx_[s][i];
            ev += T.e[s] * kv_[s][i];
        }
        xn_[i] = sys_.pos[i] + h * dx;
        vn_[i] = sys_.vel[i] + h * dv;
        const double xo[3] = {sys_.pos[i].x, sys_.pos[i].y, sys_.pos[i].z};
        const double xnn[3] = {xn_[i].x, xn_[i].y, xn_[i].z};
        const double ee[3] = {ex.x, ex.y, ex.z};
        const double vo[3] = {sys_.vel[i].x, sys_.vel[i].y, sys_.vel[i].z};
        const double vnn[3] = {vn_[i].x, vn_[i].y, vn_[i].z};
        const double ev3[3] = {ev.x, ev.y, ev.z};
        for (int k = 0; k < 3; ++k) {
            const double qx = h * ee[k] / sc(xo[k], xnn[k]);
            const double qv = h * ev3[k] / sc(vo[k], vnn[k]);
            sum += qx * qx + qv * qv;
        }
    }
    err = std::sqrt(sum / (6.0 * static_cast<double>(n)));
    return err <= 1.0;
}

void Simulation::advance_dopri(double T) {
    const double dir = T >= 0.0 ? 1.0 : -1.0;
    const double total = std::fabs(T);
    if (h_adapt_ <= 0.0) h_adapt_ = std::fabs(dt_);
    double done = 0.0;
    int rejections = 0;
    while (done < total) {
        double h = std::min(h_adapt_, total - done);
        const bool clipped = h < h_adapt_;
        double err = 0.0;
        const bool ok = dopri_attempt(dir * h, err);
        double fac = (err == 0.0) ? 5.0 : 0.9 * std::pow(err, -0.2);
        fac = std::clamp(fac, 0.2, 5.0);
        if (ok || rejections > 50) {
            sys_.pos = xn_;
            sys_.vel = vn_;
            sys_.time += dir * h;
            done += h;
            rejections = 0;
            h_adapt_ = clipped ? std::min(h_adapt_, h * fac) : h * fac;
        } else {
            ++rejections;
            h_adapt_ = h * std::min(fac, 0.9);
        }
    }
    cache_ok_ = false;
}

void Simulation::advance(double T) {
    if (T == 0.0) return;
    if (method_ == Method::DOPRI5) {
        advance_dopri(T);
        return;
    }
    const double h0 = std::fabs(dt_);
    const double n = std::max(1.0, std::ceil(std::fabs(T) / h0 - 1e-9));
    const double h = T / n;
    const long steps = static_cast<long>(n);
    for (long i = 0; i < steps; ++i) fixed_step(h);
}

}  // namespace gravsim
