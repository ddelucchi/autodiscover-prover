#ifndef AUTODISCOVER_CORE_CONSTANTS_HPP
#define AUTODISCOVER_CORE_CONSTANTS_HPP

/**
 * @file Constants.hpp
 * @brief Canonical mathematical constants for the AutoDiscover framework
 * 
 * Single source of truth for PHI, PHI_INV, M_PI, and related constants.
 * All other files should reference these definitions rather than defining
 * their own copies.
 * 
 * @author AutoDiscover Prover
 */

// ===========================================================================
// M_PI GUARD  safe on MSVC where _USE_MATH_DEFINES may not be defined
// ===========================================================================
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace autodiscover {

/**
 * @brief Core mathematical constants used throughout the framework
 * 
 * All values are constexpr double with maximum representable precision.
 */
namespace constants {

// ---- Universal mathematical constants ----

/// Golden ratio φ = (1 + √5) / 2 ≈ 1.618...
constexpr double PHI = 1.6180339887498948482045868343656381177203091798057628621;

/// Golden ratio reciprocal 1/φ = φ − 1 ≈ 0.618...
constexpr double PHI_INV = 0.6180339887498948482045868343656381177203091798057628621;

/// φ² = φ + 1 ≈ 2.618...
constexpr double PHI_SQ = 2.6180339887498948482;

/// π ≈ 3.14159...
constexpr double PI = 3.14159265358979323846264338327950288419716939937510;

/// 2π ≈ 6.283...
constexpr double TWO_PI = 6.28318530717958647692;

/// Euler's number e ≈ 2.71828...
constexpr double E = 2.71828182845904523536028747135266249775724709369995;

/// √2 ≈ 1.41421...
constexpr double SQRT2 = 1.41421356237309504880168872420969807856967187537694;

/// √5 ≈ 2.23607...
constexpr double SQRT5 = 2.23606797749978969640917366873127623544061835961152;

// ---- Interstice framework constants (§5, §6, §LX–LXIII) ----

/// Λ = φ⁴ = 3φ + 2 ≈ 6.854...
constexpr double LAMBDA = 6.854101966249685;

/// ln(φ) ≈ 0.4812...
constexpr double LN_PHI = 0.48121182505960344;

/// ln(Λ) = 4·ln(φ) ≈ 1.9248...
constexpr double LN_LAMBDA = 1.9248473002384139;

/// α = ln(Λ)/(2π) = 2·ln(φ)/π ≈ 0.3063...
constexpr double ALPHA_INT = 0.30634896253602974;

/// β = 2π/ln(Λ) = π/(2·ln(φ)) ≈ 3.264...
constexpr double BETA_INT = 3.2641655952478734;

/// Critical exponent κ = 1/3
constexpr double KAPPA = 1.0 / 3.0;

/// Re(χ_φ) = ln(φ) ≈ 0.4812...
constexpr double CHI_RE = 0.48121182505960344;

/// Im(χ_φ) = π/2 ≈ 1.5708...
constexpr double CHI_IM = 1.5707963267948966;

/// |χ_φ|² = ln(φ)² + π²/4 ≈ 2.698...
constexpr double CHI_MOD2 = 2.6982447002052565;

/// |χ_φ| ≈ 1.643...
constexpr double CHI_MOD = 1.6427559301437252;

/// SCOUT maximum radius: ρ_max = 1/φ
constexpr double SCOUT_RHO_MAX = PHI_INV;

} // namespace constants
} // namespace autodiscover

#endif // AUTODISCOVER_CORE_CONSTANTS_HPP
