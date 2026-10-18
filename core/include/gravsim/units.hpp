#pragma once
// Unit system and physical constants.
//
// Default unit system ("solar-system units"):
//   length = astronomical unit (AU), time = mean solar day (86400 s), mass = solar mass.
// In these units the gravitational constant is the square of the Gaussian
// gravitational constant k: G = k^2 = 2.959122082855911e-4 AU^3 / (Msun day^2).
// Working in these units keeps all numbers O(1) and avoids the precision loss
// of mixing meters with astronomical distances.

namespace gravsim::units {

inline constexpr double PI = 3.14159265358979323846264338327950288;
inline constexpr double DEG = PI / 180.0;
inline constexpr double ARCSEC = PI / 648000.0;

inline constexpr double GAUSS_K = 0.01720209895;                 // rad/day
inline constexpr double G_AU_DAY_MSUN = GAUSS_K * GAUSS_K;        // AU^3 Msun^-1 day^-2
inline constexpr double AU_KM = 149597870.7;                      // IAU 2012 (exact)
inline constexpr double C_KM_S = 299792.458;                      // exact
inline constexpr double C_AU_DAY = C_KM_S * 86400.0 / AU_KM;      // ~173.1446 AU/day
inline constexpr double DAYS_PER_JULIAN_YEAR = 365.25;
inline constexpr double GM_SUN_KM3_S2 = 1.32712440018e11;         // IAU nominal-ish (TCB value)
inline constexpr double SCHWARZSCHILD_KM_PER_MSUN =
    2.0 * GM_SUN_KM3_S2 / (C_KM_S * C_KM_S);                      // ~2.9533 km

}  // namespace gravsim::units
