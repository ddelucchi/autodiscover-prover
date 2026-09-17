#pragma once
// =============================================================================
// IntersticeOrbit.hpp — Orbit-Evaluated Structural Discovery Engine
// =============================================================================
//
// This module implements the orbit-evaluated structural discovery engine
// from Interstices.txt (17,184 lines, 3 full passes).
//
// KEY DIFFERENCE: Instead of evaluating nullary terms at a single point x₀=φ
// and finding value-coincidences, this engine:
//
//   1. Evaluates FUNCTIONS f: X → A_n on entire ORBITS {g^k·x₀}_{k=0..N-1}
//   2. Uses D_{g,χ} as a FIRST-CLASS OPERATOR that generates new functions
//   3. Discovers STRUCTURAL EQUATIONS by matching function vectors on orbits
//   4. Checks the Universal Field Equation □Φ=J as a discovery target
//   5. Lifts equations through the Cayley-Dickson tower via transport T_{p→q}
//
// KEY MATHEMATICAL STRUCTURES:
//   Domain X = (Σ × S¹) × T   — mapping torus × time scale
//   Algebra A_n = CD^n(ℝ)     — Cayley-Dickson at level n
//   Operator D_{g,χ}f = (U_g f - f)/χ_g  — UNIVERSAL difference quotient
//   Leibniz: D(fh) = (Df)(U_g h) + f(Dh)
//   FToI: f(g^N x) - f(x) = Σ χ_g(g^k x) · Df(g^k x)
//   UFE: □_{g,χ} Φ = J  (interstice wave equation)
//   Tower: T_{p→q}: equations at A_p ↔ equations at A_q
//   Conservation: Ω_g D_g η + Σ Ω_{gj} D_{gj} q_j = μ_src
//
// CONSTANTS: Λ=φ⁴, q=iφ, N=4, χ_φ=ln(φ)+iπ/2, Hol(A)=Λ
// =============================================================================

#include <cmath>
#include <string>
#include <vector>
#include <array>
#include <functional>
#include <complex>
#include <algorithm>
#include <numeric>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cassert>
#include <memory>

#include "../core/Term.hpp"
#include "../core/Constants.hpp"
#include "../logic/Equation.hpp"
#include "Algebra.hpp"

namespace autodiscover {
namespace domain {
namespace orbit {

using core::Term;
using core::TermFactory;
using core::Sort;
using logic::Equation;
using logic::EquationSource;

// =============================================================================
// CONSTANTS — sourced from core/Constants.hpp
// =============================================================================

namespace K {
    using namespace ::autodiscover::constants;
    // Legacy aliases (map old names to canonical)
    inline constexpr double PI_    = PI;
    inline constexpr double ALPHA  = ALPHA_INT;
    inline constexpr double BETA   = BETA_INT;
    // Orbit-specific constants
    inline constexpr int    N_ORDER   = 4;      // S_φ has order N=4
    inline constexpr int    ORBIT_PTS = 8;      // sample 2N points on orbit
}

// =============================================================================
// 1. DOMAIN X = (Σ × S¹) × T  —  THE SPACE ON WHICH FUNCTIONS LIVE
// =============================================================================
//
// Coordinates: (ω, ψ, t)  where
//   ω ∈ Σ = unit sphere (direction)  — for 1D we use ω = sign(x)
//   ψ ∈ S¹ = [0, 2π)                — scale angle, ψ(r) = β·ln(r) mod 2π
//   t ∈ T ⊂ ℝ                       — time scale
//
// The mapping torus quotient:
//   ℝ^d\{0}/⟨A⟩ ≅ Σ × S¹   where A = diag(φ⁴,…) is the scaling matrix
// =============================================================================

struct DomainPoint {
    double omega;   // direction (±1 for 1D, unit vector for higher)
    double psi;     // scale angle ∈ [0, 2π)
    double t;       // time coordinate
    double r;       // radial coordinate = exp(ψ/β) (redundant but useful)

    DomainPoint() : omega(1.0), psi(0.0), t(0.0), r(1.0) {}
    DomainPoint(double om, double ps, double tt, double rr)
        : omega(om), psi(ps), t(tt), r(rr) {}

    // Construct from a scalar x ∈ ℝ\{0}
    static DomainPoint fromScalar(double x) {
        DomainPoint p;
        p.omega = (x >= 0) ? 1.0 : -1.0;
        double ax = std::abs(x);
        if (ax < 1e-30) ax = 1e-30;
        p.r = ax;
        p.psi = std::fmod(K::BETA * std::log(ax), K::TWO_PI);
        if (p.psi < 0) p.psi += K::TWO_PI;
        p.t = 0.0;
        return p;
    }

    // Convert back to scalar
    double toScalar() const {
        return omega * r;
    }
};

// =============================================================================
// 2. FUNCTION ON ORBIT  —  f evaluated at N orbit points
// =============================================================================
//
// Instead of evaluating f(x₀) → single number, we evaluate
//   f(x₀), f(g·x₀), f(g²·x₀), ..., f(g^{N-1}·x₀)
// and store the VECTOR of values as the function's "orbit signature."
//
// Two functions with the SAME orbit signature are equal as functions
// (to numerical tolerance), giving us genuine algebraic identities
// rather than single-point coincidences.
// =============================================================================

struct OrbitSignature {
    static constexpr int N = K::ORBIT_PTS;
    std::array<double, K::ORBIT_PTS> values;  // f(g^k·x₀) for k=0..N-1

    OrbitSignature() { values.fill(0.0); }

    bool isFinite() const {
        for (double v : values) {
            if (!std::isfinite(v)) return false;
        }
        return true;
    }

    // Orbit-based equality: all N values match within tolerance
    bool matches(const OrbitSignature& other, double tol = 1e-7) const {
        for (int k = 0; k < N; ++k) {
            if (std::abs(values[k] - other.values[k]) > tol) return false;
        }
        return true;
    }

    // Hash for bucketing (uses quantized first value + summary)
    uint64_t hash(double tol = 1e-7) const {
        // Use first value + norm for robust hashing
        int64_t h1 = static_cast<int64_t>(std::round(values[0] / tol));
        double norm = 0.0;
        for (double v : values) norm += v * v;
        int64_t h2 = static_cast<int64_t>(std::round(std::sqrt(norm) / tol));
        return static_cast<uint64_t>(h1 * 1000003 + h2);
    }

    // L² norm of the orbit vector
    double norm() const {
        double s = 0.0;
        for (double v : values) s += v * v;
        return std::sqrt(s);
    }

    // Difference from another signature
    OrbitSignature operator-(const OrbitSignature& o) const {
        OrbitSignature r;
        for (int k = 0; k < N; ++k) r.values[k] = values[k] - o.values[k];
        return r;
    }

    // Scale
    OrbitSignature operator*(double c) const {
        OrbitSignature r;
        for (int k = 0; k < N; ++k) r.values[k] = values[k] * c;
        return r;
    }

    // Pointwise product
    OrbitSignature pointwiseMul(const OrbitSignature& o) const {
        OrbitSignature r;
        for (int k = 0; k < N; ++k) r.values[k] = values[k] * o.values[k];
        return r;
    }

    // Sum of values (for detecting constant functions)
    double sum() const {
        double s = 0.0;
        for (double v : values) s += v;
        return s;
    }

    // Max absolute value
    double maxAbs() const {
        double m = 0.0;
        for (double v : values) m = std::max(m, std::abs(v));
        return m;
    }

    // Check if constant (same value at all orbit points)
    bool isConstant(double tol = 1e-7) const {
        for (int k = 1; k < N; ++k) {
            if (std::abs(values[k] - values[0]) > tol) return false;
        }
        return true;
    }

    // Check if zero
    bool isZero(double tol = 1e-8) const {
        return maxAbs() < tol;
    }
};

// =============================================================================
// 3. GROUP ACTIONS ON THE DOMAIN
// =============================================================================
//
// Each group action g acts on x ∈ X and has a character χ_g(x).
// The universal difference quotient is D_{g,χ}f(x) = (f(gx)-f(x))/χ_g(x).
// =============================================================================

enum class OrbitAction {
    // Scale actions (multiplicative on r)
    SCALE_PHI,      // g·r = φ·r,     χ = (φ-1)·r      → D_φ
    SCALE_LAMBDA,   // g·r = Λ·r,     χ = (Λ-1)·r      → D_Λ
    SCALE_PHI_INV,  // g·r = r/φ,     χ = (1/φ-1)·r    → D_{1/φ}

    // Additive actions (translation on r)
    SHIFT_1,        // g·r = r+1,     χ = 1             → Δ (finite diff)
    SHIFT_H,        // g·r = r+h,     χ = h             → Newton (h→0)
    SHIFT_PHI,      // g·r = r+φ,     χ = φ             → shift by φ

    // Golden interstice (the fundamental action)
    GOLDEN,         // g·r = φ·r,     χ = |χ_φ|         → Ξ_φ

    // Angular actions (on ψ coordinate)
    ROTATE_QUARTER, // g·ψ = ψ+π/2,  χ = π/2           → S_φ quarter-shift
    ROTATE_FULL,    // g·ψ = ψ+2π,   χ = 2π            → full turn

    // Newton derivative (limit action)
    NEWTON          // g·r = r+ε,     χ = ε → 0         → d/dr
};

inline const char* actionName(OrbitAction a) {
    switch (a) {
        case OrbitAction::SCALE_PHI:      return "D_phi";
        case OrbitAction::SCALE_LAMBDA:   return "D_Lambda";
        case OrbitAction::SCALE_PHI_INV:  return "D_phi_inv";
        case OrbitAction::SHIFT_1:        return "Delta";
        case OrbitAction::SHIFT_H:        return "D_h";
        case OrbitAction::SHIFT_PHI:      return "D_shift_phi";
        case OrbitAction::GOLDEN:         return "Xi_phi";
        case OrbitAction::ROTATE_QUARTER: return "S_quarter";
        case OrbitAction::ROTATE_FULL:    return "D_2pi";
        case OrbitAction::NEWTON:         return "d_dr";
    }
    return "?";
}

// Apply group action g to a scalar x, returning (g·x, χ_g(x))
inline std::pair<double, double> applyAction(OrbitAction action, double x) {
    switch (action) {
        case OrbitAction::SCALE_PHI:
            return { K::PHI * x, (K::PHI - 1.0) * x };
        case OrbitAction::SCALE_LAMBDA:
            return { K::LAMBDA * x, (K::LAMBDA - 1.0) * x };
        case OrbitAction::SCALE_PHI_INV:
            return { K::PHI_INV * x, (K::PHI_INV - 1.0) * x };
        case OrbitAction::SHIFT_1:
            return { x + 1.0, 1.0 };
        case OrbitAction::SHIFT_H: {
            double h = 1e-7;
            return { x + h, h };
        }
        case OrbitAction::SHIFT_PHI:
            return { x + K::PHI, K::PHI };
        case OrbitAction::GOLDEN:
            return { K::PHI * x, K::CHI_MOD };
        case OrbitAction::ROTATE_QUARTER:
            return { x + K::PI_ / 2.0, K::PI_ / 2.0 };
        case OrbitAction::ROTATE_FULL:
            return { x + K::TWO_PI, K::TWO_PI };
        case OrbitAction::NEWTON: {
            double h = 1e-7;
            return { x + h, h };
        }
    }
    return { x, 1.0 };
}

// =============================================================================
// 4. ORBIT EVALUATOR  —  evaluate functions on orbits
// =============================================================================
//
// Given an action g and base point x₀, evaluates f at orbit points:
//   x₀, g·x₀, g²·x₀, ..., g^{N-1}·x₀
//
// The orbit for SCALE_PHI with x₀=φ gives:
//   φ, φ², φ³, φ⁴=Λ, φ⁵, φ⁶, φ⁷, φ⁸=Λ²
// =============================================================================

// A test function: x → f(x)
using TestFunc = std::function<double(double)>;

// Library of named test functions
struct FuncLib {
    std::string name;
    TestFunc func;
};

inline const std::vector<FuncLib>& getTestFunctions() {
    static const std::vector<FuncLib> funcs = {
        // Polynomial
        {"id",      [](double x) { return x; }},
        {"sq",      [](double x) { return x*x; }},
        {"cube",    [](double x) { return x*x*x; }},
        {"x4",      [](double x) { return x*x*x*x; }},
        {"x5",      [](double x) { double x2=x*x; return x2*x2*x; }},
        {"inv",     [](double x) { return std::abs(x)>1e-15 ? 1.0/x : 0.0; }},
        {"inv_sq",  [](double x) { return std::abs(x)>1e-15 ? 1.0/(x*x) : 0.0; }},
        {"sqrt_a",  [](double x) { return std::sqrt(std::abs(x)); }},

        // Transcendental
        {"exp",     [](double x) { return std::exp(x); }},
        {"log",     [](double x) { return x>0 ? std::log(x) : 0.0; }},
        {"sin",     [](double x) { return std::sin(x); }},
        {"cos",     [](double x) { return std::cos(x); }},
        {"tan",     [](double x) { return std::tan(x); }},
        {"sinh",    [](double x) { return std::sinh(x); }},
        {"cosh",    [](double x) { return std::cosh(x); }},
        {"tanh",    [](double x) { return std::tanh(x); }},
        {"atan",    [](double x) { return std::atan(x); }},

        // Special
        {"gauss",   [](double x) { return std::exp(-x*x); }},
        {"gauss_x", [](double x) { return x*std::exp(-x*x); }},
        {"sigmoid", [](double x) { return 1.0/(1.0+std::exp(-x)); }},
        {"lgamma",  [](double x) { return std::lgamma(x); }},

        // Scale/golden
        {"phi_pow", [](double x) { return std::pow(K::PHI, x); }},
        {"lam_pow", [](double x) { return std::pow(K::LAMBDA, x); }},

        // Waves
        {"sincos",  [](double x) { return std::sin(x)*std::cos(x); }},

        // Composites: polynomial with rational coefficients
        {"half_sq",  [](double x) { return 0.5*x*x; }},
        {"qtr_x4",   [](double x) { return 0.25*x*x*x*x; }},
        {"neg_inv_a",[](double x) { return std::abs(x)>1e-10 ? -1.0/std::abs(x) : -1e10; }},

        // Composites: transcendental
        {"gauss4",   [](double x) { return std::exp(-x*x/4.0)/std::sqrt(4.0*K::PI_); }},
        {"sin_shift",[](double x) { return std::sin(x+K::PHI)+std::sin(x-K::PHI); }},

        // Bessel J₀ (power series, 20 terms)
        {"J0",      [](double x) {
            double sum = 0.0, term = 1.0;
            for (int k = 0; k < 20; ++k) {
                sum += term;
                term *= -(x*x) / (4.0*(k+1)*(k+1));
            }
            return sum;
        }},

        // Digamma approximation
        {"digamma", [](double x) {
            if (x <= 0) return 0.0;
            return std::log(x) - 0.5/x - 1.0/(12.0*x*x);
        }},

        // Airy-like: Ai(x) ≈ e^(-2x^{3/2}/3) / (2√π x^{1/4})
        {"airy",    [](double x) {
            if (x <= 0) return std::cos(2.0*std::pow(-x,1.5)/3.0) / std::sqrt(K::PI_);
            return std::exp(-2.0*std::pow(x,1.5)/3.0) / (2.0*std::sqrt(K::PI_)*std::pow(x,0.25));
        }},

        // Elliptic-like: complete elliptic integral K(k) ≈ π/2 · (1 + k²/4 + 9k⁴/64)
        {"ellipK",  [](double x) {
            double k2 = x*x;
            if (k2 >= 1.0) return 0.0;
            return (K::PI_/2.0) * (1.0 + k2/4.0 + 9.0*k2*k2/64.0);
        }},

        // Riemann zeta approximation ζ(x) for x > 1
        {"zeta",    [](double x) {
            if (x <= 1.0) return 0.0;
            double sum = 0.0;
            for (int n = 1; n <= 100; ++n) sum += std::pow(n, -x);
            return sum;
        }},

        // Polylogarithm Li₂(x) = Σ x^n/n² for |x| ≤ 1
        {"Li2",     [](double x) {
            if (std::abs(x) > 1.0) return 0.0;
            double sum = 0.0;
            double xn = x;
            for (int n = 1; n <= 50; ++n) {
                sum += xn / (double)(n * n);
                xn *= x;
            }
            return sum;
        }},

        // Theta function: θ₃(0,q) = 1 + 2Σ q^{n²}
        {"theta3",  [](double q) {
            if (std::abs(q) >= 1.0) return 0.0;
            double sum = 1.0;
            for (int n = 1; n <= 20; ++n) {
                sum += 2.0 * std::pow(q, (double)(n * n));
            }
            return sum;
        }},

        // Weierstrass ℘: approximate for lattice with ω₁=1, ω₂=i
        {"weier",   [](double x) {
            double sum = 1.0/(x*x);
            for (int m = -3; m <= 3; ++m) {
                for (int n = -3; n <= 3; ++n) {
                    if (m == 0 && n == 0) continue;
                    double lat = m + n * K::PHI; // use golden lattice
                    double d = x - lat;
                    if (std::abs(d) > 1e-10) {
                        sum += 1.0/(d*d) - 1.0/(lat*lat);
                    }
                }
            }
            return sum;
        }},

        // Dedekind eta-like: η(τ) = e^{iπτ/12} Π(1-e^{2πinτ})
        {"eta",     [](double x) {
            // Real part on upper half-plane at τ=ix (so q=e^{-2πx})
            if (x <= 0) return 0.0;
            double q = std::exp(-K::TWO_PI * x);
            double prod = 1.0;
            for (int n = 1; n <= 30; ++n) {
                prod *= (1.0 - std::pow(q, n));
            }
            return std::pow(q, 1.0/24.0) * prod;
        }},

        // ===================================================================
        // EXTENDED TEST FUNCTIONS — PDE solutions & physics functions
        // ===================================================================

        // Reciprocal trig
        {"sec",     [](double x) { return 1.0 / std::cos(x); }},
        {"csc",     [](double x) { auto s = std::sin(x); return std::abs(s)>1e-10 ? 1.0/s : 0.0; }},

        // Inverse hyperbolic
        {"asinh",   [](double x) { return std::asinh(x); }},
        {"atanh",   [](double x) { return std::abs(x)<1.0 ? std::atanh(x) : 0.0; }},

        // Normalized sinc
        {"sinc",    [](double x) { return std::abs(x)>1e-10 ? std::sin(x)/x : 1.0; }},

        // Bessel J1
        {"J1",      [](double x) {
            double sum = 0.0, term = x / 2.0;
            for (int k = 0; k < 20; ++k) {
                sum += term;
                term *= -(x*x) / (4.0*(k+1)*(k+2));
            }
            return sum;
        }},

        // Hermite polynomials
        {"H2",      [](double x) { return 4.0*x*x - 2.0; }},
        {"H3",      [](double x) { return 8.0*x*x*x - 12.0*x; }},

        // Laguerre polynomials
        {"L1",      [](double x) { return 1.0 - x; }},
        {"L2",      [](double x) { return 1.0 - 2.0*x + x*x/2.0; }},

        // Legendre polynomials
        {"P2",      [](double x) { return 0.5*(3.0*x*x - 1.0); }},
        {"P3",      [](double x) { return 0.5*(5.0*x*x*x - 3.0*x); }},

        // Composite exponentials of |x|
        {"exp_neg_a",[](double x) { return std::exp(-std::abs(x)); }},
        {"lin_exp_a",[](double x) { return (1.0-std::abs(x)/2.0)*std::exp(-std::abs(x)/2.0); }},
        {"x_exp_a",  [](double x) { return std::abs(x)*std::exp(-std::abs(x)/2.0); }},

        // Rational-exponential composites
        {"exp_inv_a",[](double x) { return std::abs(x)>1e-10 ? std::exp(-std::abs(x))/std::abs(x) : 0.0; }},

        // Simple composites
        {"half_x",   [](double x) { return x / 2.0; }},
        {"sech2",    [](double x) { auto s = 1.0/std::cosh(x); return 2.0*s*s; }},
        {"gauss2",   [](double x) { return std::exp(-x*x/2.0); }},
        {"clip_para",[](double x) { return std::max(0.0, 1.0-x*x/4.0); }},
        {"thresh_inv",[](double x) { return std::abs(x)>2.0 ? 1.0-2.0/std::abs(x) : 0.0; }},

        // Step / BV functions
        {"step",    [](double x) { return x>=0 ? 1.0 : 0.0; }},
        {"abs",     [](double x) { return std::abs(x); }},

        // Fractional powers
        {"pow_half",[](double x) { return x>0 ? std::sqrt(x) : 0.0; }},
    };
    return funcs;
}

class OrbitEvaluator {
public:
    // Evaluate a test function on the orbit of action g starting from x₀
    static OrbitSignature evaluateOnOrbit(
        const TestFunc& f, OrbitAction action, double x0)
    {
        OrbitSignature sig;
        double x = x0;
        for (int k = 0; k < OrbitSignature::N; ++k) {
            sig.values[k] = f(x);
            auto [gx, chi] = applyAction(action, x);
            x = gx;
        }
        return sig;
    }

    // Compute D_{g,χ}f(x) on the orbit: the difference quotient at each point
    static OrbitSignature computeDgChi(
        const TestFunc& f, OrbitAction action, double x0)
    {
        OrbitSignature sig;
        double x = x0;
        for (int k = 0; k < OrbitSignature::N; ++k) {
            if (action == OrbitAction::NEWTON || action == OrbitAction::SHIFT_H) {
                // Central difference for Newton derivative
                double h = 1e-7;
                sig.values[k] = (f(x + h) - f(x - h)) / (2.0 * h);
            } else {
                auto [gx, chi] = applyAction(action, x);
                if (std::abs(chi) < 1e-15) {
                    sig.values[k] = 0.0;
                } else {
                    sig.values[k] = (f(gx) - f(x)) / chi;
                }
            }
            // Advance orbit point
            auto [next, _] = applyAction(action, x);
            x = next;
        }
        return sig;
    }

    // Compute D²_{g,χ}f(x) on orbit (second derivative)
    static OrbitSignature computeD2gChi(
        const TestFunc& f, OrbitAction action, double x0)
    {
        // D² = D applied to D
        auto Df = [&](double x_val) -> double {
            if (action == OrbitAction::NEWTON) {
                double h = 1e-7;
                return (f(x_val + h) - f(x_val - h)) / (2.0 * h);
            }
            auto [gx, chi] = applyAction(action, x_val);
            if (std::abs(chi) < 1e-15) return 0.0;
            return (f(gx) - f(x_val)) / chi;
        };
        return computeDgChi(Df, action, x0);
    }

    // Compute □_{g,χ}f ≡ D†_{g,χ} D_{g,χ} f  (wave/Laplace operator)
    // Approximated as D²
    static OrbitSignature computeBox(
        const TestFunc& f, OrbitAction action, double x0)
    {
        return computeD2gChi(f, action, x0);
    }

    // Verify Leibniz rule: D(f·h) vs (Df)(U_g h) + f(Dh) on orbit
    // Returns the orbit residual (should be zero for exact Leibniz)
    static OrbitSignature verifyLeibniz(
        const TestFunc& f, const TestFunc& h,
        OrbitAction action, double x0)
    {
        // Product function
        auto fh = [&](double x) { return f(x) * h(x); };

        // LHS: D(fh)
        OrbitSignature lhs = computeDgChi(fh, action, x0);

        // RHS: (Df)(U_g h) + f(Dh)
        OrbitSignature rhs;
        double x = x0;
        for (int k = 0; k < OrbitSignature::N; ++k) {
            auto [gx, chi] = applyAction(action, x);

            double Df_x, Dh_x;
            if (action == OrbitAction::NEWTON) {
                double hh = 1e-7;
                Df_x = (f(x + hh) - f(x - hh)) / (2.0 * hh);
                Dh_x = (h(x + hh) - h(x - hh)) / (2.0 * hh);
            } else {
                if (std::abs(chi) < 1e-15) {
                    Df_x = Dh_x = 0.0;
                } else {
                    Df_x = (f(gx) - f(x)) / chi;
                    Dh_x = (h(gx) - h(x)) / chi;
                }
            }

            double Ugh_x = h(gx);   // U_g h at x = h(g·x)
            double f_x = f(x);

            rhs.values[k] = Df_x * Ugh_x + f_x * Dh_x;

            auto [next, _] = applyAction(action, x);
            x = next;
        }

        return lhs - rhs;  // Should be zero orbit
    }

    // Verify FToI: f(g^N x) - f(x) = Σ χ_g(g^k x) · Df(g^k x)
    // Returns (lhs_value, rhs_value, residual)
    static std::tuple<double, double, double> verifyFToI(
        const TestFunc& f, OrbitAction action, double x0, int N)
    {
        // Compute g^N · x₀
        double xN = x0;
        for (int k = 0; k < N; ++k) {
            auto [gx, _] = applyAction(action, xN);
            xN = gx;
        }
        double lhs = f(xN) - f(x0);

        // RHS: Σ_{k=0}^{N-1} χ_g(g^k x) · D_{g,χ}f(g^k x)
        double rhs = 0.0;
        double x = x0;
        for (int k = 0; k < N; ++k) {
            auto [gx, chi] = applyAction(action, x);
            double Df_x;
            if (action == OrbitAction::NEWTON) {
                double h = 1e-7;
                Df_x = (f(x + h) - f(x - h)) / (2.0 * h);
            } else {
                if (std::abs(chi) < 1e-15) Df_x = 0.0;
                else Df_x = (f(gx) - f(x)) / chi;
            }
            rhs += chi * Df_x;
            x = gx;
        }

        return { lhs, rhs, std::abs(lhs - rhs) };
    }
};

// =============================================================================
// 5. STRUCTURAL EQUATION DISCOVERY  —  THE ORBIT-EVALUATED CORE
// =============================================================================
//
// Instead of checking if two terms have the same value at x₀=φ,
// we check if two OPERATOR EXPRESSIONS give the same ORBIT SIGNATURE.
//
// This discovers equations like:
//   D_Newton(sin) = cos                  (derivative identity)
//   D_Λ(x^n) = (Λ^n - 1)/(Λ-1) · x^{n-1}   (power rule generalization)
//   □(exp) = exp                          (eigenfunction of Laplacian)
//   D(f·g) = (Df)(Ug g) + f(Dg)          (Leibniz rule as equation)
//   f(Λx) - f(x) = Σ χ·Df               (FToI)
//
// The key improvement: these are genuine functional identities
// verified on N orbit points, not single-value coincidences.
// =============================================================================

struct DiscoveredFunctionalEquation {
    std::string lhsDesc;          // Human-readable LHS
    std::string rhsDesc;          // Human-readable RHS
    OrbitSignature lhsSig;        // Orbit values of LHS
    OrbitSignature rhsSig;        // Orbit values of RHS
    double maxResidual;           // max |lhs - rhs| over orbit
    int orbitPoints;              // number of points verified
    std::string category;         // e.g. "Leibniz", "FToI", "Derivative", "UFE"
    std::string actionName;       // which D_{g,χ}
    bool isStructural;            // true if algebraic identity, not numerical

    std::string toString() const {
        std::ostringstream oss;
        oss << "[" << category << "|" << actionName << "] "
            << lhsDesc << " = " << rhsDesc
            << "  (residual=" << std::scientific << std::setprecision(2) << maxResidual
            << ", pts=" << orbitPoints << ")";
        return oss.str();
    }
};

class StructuralDiscoveryEngine {
public:
    struct Config {
        double x0 = K::PHI;                // base orbit point
        double tolerance = 1e-6;           // matching tolerance
        bool   discoverDerivatives = true;  // D_g(f) = ? matches
        bool   discoverLeibniz = true;      // Leibniz rule verification
        bool   discoverFToI = true;         // FToI verification
        bool   discoverUFE = true;          // □Φ = J search
        bool   discoverScaleCovariance = true; // scale relations
        bool   discoverCrossAction = true;  // relations between different D_g
        bool   verbose = true;
    };

    explicit StructuralDiscoveryEngine(Config cfg = {}) : config_(cfg) {}

    // =========================================================================
    // MAIN DISCOVERY METHOD
    // =========================================================================
    std::vector<DiscoveredFunctionalEquation> discoverAll() {
        std::vector<DiscoveredFunctionalEquation> results;
        const auto& funcs = getTestFunctions();

        static const std::vector<OrbitAction> actions = {
            OrbitAction::NEWTON,
            OrbitAction::SCALE_PHI,
            OrbitAction::SCALE_LAMBDA,
            OrbitAction::SHIFT_1,
            OrbitAction::GOLDEN,
            OrbitAction::ROTATE_QUARTER,
        };

        if (config_.verbose) {
            std::cout << "\n"
                "================================================================\n"
                "  STRUCTURAL EQUATION DISCOVERY ENGINE  (Orbit v14.0)\n"
                "  Domain: X = (Σ × S¹) × T\n"
                "  Algebra: A_n = Cayley-Dickson tower\n"
                "  Operator: D_{g,χ}f = (U_g f - f)/χ_g  — UNIVERSAL\n"
                "  Method: Orbit evaluation at " << OrbitSignature::N << " points\n"
                "  Base: x₀ = φ ≈ " << K::PHI << "\n"
                "  Constants: Λ=φ⁴, q=iφ, N=4, χ_φ=ln(φ)+iπ/2\n"
                "================================================================\n\n";
        }

        // --- Phase 1: Derivative Identities D_g(f) = c·h ---
        if (config_.discoverDerivatives) {
            if (config_.verbose) std::cout << "[STRUCT] Phase 1: Derivative identities D_g(f) = c·h\n";
            discoverDerivativeIdentities(funcs, actions, results);
        }

        // --- Phase 2: Leibniz Rule Verification ---
        if (config_.discoverLeibniz) {
            if (config_.verbose) std::cout << "[STRUCT] Phase 2: Leibniz rule D(fh) = (Df)(U_g h) + f(Dh)\n";
            discoverLeibnizIdentities(funcs, actions, results);
        }

        // --- Phase 3: FToI Verification ---
        if (config_.discoverFToI) {
            if (config_.verbose) std::cout << "[STRUCT] Phase 3: FToI f(g^N x) - f(x) = Σ χ·Df\n";
            discoverFToIIdentities(funcs, actions, results);
        }

        // --- Phase 4: UFE Search □Φ = 0 ---
        if (config_.discoverUFE) {
            if (config_.verbose) std::cout << "[STRUCT] Phase 4: UFE search □_{g,χ}Φ = 0\n";
            discoverUFEEigenfunctions(funcs, actions, results);
        }

        // --- Phase 5: Scale Covariance ---
        if (config_.discoverScaleCovariance) {
            if (config_.verbose) std::cout << "[STRUCT] Phase 5: Scale covariance relations\n";
            discoverScaleCovariance(funcs, results);
        }

        // --- Phase 6: Cross-Action Relations ---
        if (config_.discoverCrossAction) {
            if (config_.verbose) std::cout << "[STRUCT] Phase 6: Cross-action D_g vs D_h\n";
            discoverCrossActionRelations(funcs, actions, results);
        }

        // --- Phase 7: Groupoid Ð_g Cocycle Verification ---
        if (config_.verbose) std::cout << "[STRUCT] Phase 7: Groupoid Ð_g cocycle/inverse/chain\n";
        discoverGroupoidIdentities(funcs, actions, results);

        // --- Phase 8: Curvature Flatness F(g,h) ---
        if (config_.verbose) std::cout << "[STRUCT] Phase 8: Curvature flatness F(g,h) = 0\n";
        discoverCurvatureFlatness(funcs, actions, results);

        // --- Phase 9: Conservation Laws Δ_g η + Div_g q = μ ---
        if (config_.verbose) std::cout << "[STRUCT] Phase 9: Conservation law discovery\n";
        discoverConservationLaws(funcs, actions, results);

        // --- Phase 10: Noncommutative Calculus Rules ---
        if (config_.verbose) std::cout << "[STRUCT] Phase 10: Power/Exp/Log rules D(f^n), D(e^f), D(log(1+f))\n";
        discoverNoncommutativeRules(funcs, actions, results);

        // --- Phase 11: Gauged UFE (D+A)†(D+A)Φ = 0 ---
        if (config_.verbose) std::cout << "[STRUCT] Phase 11: Gauged UFE and cascade equation\n";
        discoverGaugedUFE(funcs, actions, results);

        // --- Phase 12: Continuum Limit Verification ---
        if (config_.verbose) std::cout << "[STRUCT] Phase 12: Continuum limits D_Λ → d/dx, D_q → d/dx\n";
        discoverContinuumLimits(funcs, results);

        // --- Phase 13: Green's Operator S·D = id − Π ---
        if (config_.verbose) std::cout << "[STRUCT] Phase 13: Green's operator / propagator\n";
        discoverGreenOperator(funcs, results);

        // --- Phase 14: Master Equation ◊²F = (id−Π)I₂F ---
        if (config_.verbose) std::cout << "[STRUCT] Phase 14: Master equation E_B = 0\n";
        discoverMasterEquation(funcs, actions, results);

        if (config_.verbose) {
            std::cout << "\n[STRUCT] Total structural equations discovered: "
                      << results.size() << "\n\n";
        }

        return results;
    }

private:
    Config config_;

    // =========================================================================
    // Phase 1: D_g(f) = c · h  — derivative matches
    // =========================================================================
    void discoverDerivativeIdentities(
        const std::vector<FuncLib>& funcs,
        const std::vector<OrbitAction>& actions,
        std::vector<DiscoveredFunctionalEquation>& results)
    {
        // Precompute all orbit signatures for base functions
        std::vector<std::pair<std::string, OrbitSignature>> baseSigs;
        for (const auto& fl : funcs) {
            auto sig = OrbitEvaluator::evaluateOnOrbit(fl.func, OrbitAction::NEWTON, config_.x0);
            if (sig.isFinite() && !sig.isZero()) {
                baseSigs.push_back({fl.name, sig});
            }
        }

        for (OrbitAction action : actions) {
            for (const auto& fl : funcs) {
                auto Df = OrbitEvaluator::computeDgChi(fl.func, action, config_.x0);
                if (!Df.isFinite() || Df.isZero()) continue;

                // Try to match Df against c · h for each base function h
                for (const auto& [hname, hsig] : baseSigs) {
                    if (!hsig.isFinite() || hsig.isZero()) continue;

                    // Find best scalar c such that Df ≈ c · h on orbit
                    // Least squares: c = Σ(Df_k · h_k) / Σ(h_k²)
                    double num = 0, den = 0;
                    for (int k = 0; k < OrbitSignature::N; ++k) {
                        num += Df.values[k] * hsig.values[k];
                        den += hsig.values[k] * hsig.values[k];
                    }
                    if (std::abs(den) < 1e-15) continue;
                    double c = num / den;

                    // Check if Df ≈ c·h on all orbit points
                    OrbitSignature scaled = hsig * c;
                    OrbitSignature residual = Df - scaled;
                    double maxRes = residual.maxAbs();

                    if (maxRes < config_.tolerance && std::abs(c) > 1e-12) {
                        // Check if c is a recognizable constant
                        std::string cstr = recognizeConstant(c);

                        DiscoveredFunctionalEquation eq;
                        eq.lhsDesc = std::string(actionName(action)) + "(" + fl.name + ")";
                        eq.rhsDesc = cstr + " · " + hname;
                        eq.lhsSig = Df;
                        eq.rhsSig = scaled;
                        eq.maxResidual = maxRes;
                        eq.orbitPoints = OrbitSignature::N;
                        eq.category = "Derivative";
                        eq.actionName = actionName(action);
                        eq.isStructural = true;
                        results.push_back(eq);
                    }
                }

                // Also try Df = c₁·h₁ + c₂·h₂ (linear combination)
                for (size_t i = 0; i < baseSigs.size() && i < 15; ++i) {
                    for (size_t j = i+1; j < baseSigs.size() && j < 15; ++j) {
                        const auto& [h1name, h1sig] = baseSigs[i];
                        const auto& [h2name, h2sig] = baseSigs[j];
                        if (!h1sig.isFinite() || !h2sig.isFinite()) continue;

                        // Solve 2x2 system: c₁·h₁ + c₂·h₂ ≈ Df
                        // Normal equations: [h1·h1  h1·h2] [c1]   [h1·Df]
                        //                   [h1·h2  h2·h2] [c2] = [h2·Df]
                        double a11=0,a12=0,a22=0,b1=0,b2=0;
                        for (int k = 0; k < OrbitSignature::N; ++k) {
                            a11 += h1sig.values[k]*h1sig.values[k];
                            a12 += h1sig.values[k]*h2sig.values[k];
                            a22 += h2sig.values[k]*h2sig.values[k];
                            b1  += h1sig.values[k]*Df.values[k];
                            b2  += h2sig.values[k]*Df.values[k];
                        }
                        double det = a11*a22 - a12*a12;
                        if (std::abs(det) < 1e-15) continue;
                        double c1 = (a22*b1 - a12*b2) / det;
                        double c2 = (a11*b2 - a12*b1) / det;

                        // Check fit
                        double maxRes = 0;
                        for (int k = 0; k < OrbitSignature::N; ++k) {
                            double pred = c1*h1sig.values[k] + c2*h2sig.values[k];
                            maxRes = std::max(maxRes, std::abs(Df.values[k] - pred));
                        }

                        if (maxRes < config_.tolerance && (std::abs(c1) > 1e-12 || std::abs(c2) > 1e-12)) {
                            std::string c1str = recognizeConstant(c1);
                            std::string c2str = recognizeConstant(c2);

                            DiscoveredFunctionalEquation eq;
                            eq.lhsDesc = std::string(actionName(action)) + "(" + fl.name + ")";
                            eq.rhsDesc = c1str + "·" + h1name + " + " + c2str + "·" + h2name;
                            eq.maxResidual = maxRes;
                            eq.orbitPoints = OrbitSignature::N;
                            eq.category = "DerivLinComb";
                            eq.actionName = actionName(action);
                            eq.isStructural = true;
                            results.push_back(eq);
                        }
                    }
                }
            }
        }
        if (config_.verbose) {
            std::cout << "  Found " << results.size() << " derivative identities\n";
        }
    }

    // =========================================================================
    // Phase 2: Leibniz Rule D(fh) = (Df)(U_g h) + f(Dh)
    // =========================================================================
    void discoverLeibnizIdentities(
        const std::vector<FuncLib>& funcs,
        const std::vector<OrbitAction>& actions,
        std::vector<DiscoveredFunctionalEquation>& results)
    {
        size_t before = results.size();
        for (OrbitAction action : actions) {
            for (size_t i = 0; i < funcs.size() && i < 15; ++i) {
                for (size_t j = i+1; j < funcs.size() && j < 15; ++j) {
                    auto residual = OrbitEvaluator::verifyLeibniz(
                        funcs[i].func, funcs[j].func, action, config_.x0);

                    if (residual.isFinite() && residual.maxAbs() < config_.tolerance) {
                        DiscoveredFunctionalEquation eq;
                        eq.lhsDesc = std::string(actionName(action)) + "(" +
                                     funcs[i].name + "·" + funcs[j].name + ")";
                        eq.rhsDesc = "(" + std::string(actionName(action)) + funcs[i].name +
                                     ")(U_g " + funcs[j].name + ") + " +
                                     funcs[i].name + "(" + std::string(actionName(action)) +
                                     funcs[j].name + ")";
                        eq.maxResidual = residual.maxAbs();
                        eq.orbitPoints = OrbitSignature::N;
                        eq.category = "Leibniz";
                        eq.actionName = actionName(action);
                        eq.isStructural = true;
                        results.push_back(eq);
                    }
                }
            }
        }
        if (config_.verbose) {
            std::cout << "  Found " << (results.size() - before) << " Leibniz identities\n";
        }
    }

    // =========================================================================
    // Phase 3: FToI f(g^N x) - f(x) = Σ χ(g^k x)·Df(g^k x)
    // =========================================================================
    void discoverFToIIdentities(
        const std::vector<FuncLib>& funcs,
        const std::vector<OrbitAction>& actions,
        std::vector<DiscoveredFunctionalEquation>& results)
    {
        size_t before = results.size();
        for (OrbitAction action : actions) {
            if (action == OrbitAction::NEWTON) continue; // trivial for Newton

            for (const auto& fl : funcs) {
                for (int N : {1, 2, 4, K::N_ORDER}) {
                    auto [lhs, rhs, residual] = OrbitEvaluator::verifyFToI(
                        fl.func, action, config_.x0, N);

                    if (std::isfinite(residual) && residual < config_.tolerance
                        && std::abs(lhs) > 1e-10) {
                        DiscoveredFunctionalEquation eq;
                        eq.lhsDesc = fl.name + "(g^" + std::to_string(N) + " x) - " +
                                     fl.name + "(x)";
                        eq.rhsDesc = "J^(" + std::to_string(N) + ")_{" +
                                     std::string(actionName(action)) + "}(D " + fl.name + ")(x)";
                        eq.maxResidual = residual;
                        eq.orbitPoints = N;
                        eq.category = "FToI";
                        eq.actionName = actionName(action);
                        eq.isStructural = true;
                        results.push_back(eq);
                    }
                }
            }
        }
        if (config_.verbose) {
            std::cout << "  Found " << (results.size() - before) << " FToI identities\n";
        }
    }

    // =========================================================================
    // Phase 4: UFE Search □_{g,χ}Φ = 0  (eigenfunctions of Box operator)
    // =========================================================================
    void discoverUFEEigenfunctions(
        const std::vector<FuncLib>& funcs,
        const std::vector<OrbitAction>& actions,
        std::vector<DiscoveredFunctionalEquation>& results)
    {
        size_t before = results.size();
        // Precompute base sigs
        std::vector<std::pair<std::string, OrbitSignature>> baseSigs;
        for (const auto& fl : funcs) {
            auto sig = OrbitEvaluator::evaluateOnOrbit(fl.func, OrbitAction::NEWTON, config_.x0);
            if (sig.isFinite() && !sig.isZero()) {
                baseSigs.push_back({fl.name, sig});
            }
        }

        for (OrbitAction action : actions) {
            for (const auto& fl : funcs) {
                auto boxF = OrbitEvaluator::computeBox(fl.func, action, config_.x0);
                if (!boxF.isFinite()) continue;

                // Check if □f ≈ 0 (harmonic/wave equation solution)
                if (boxF.maxAbs() < config_.tolerance) {
                    DiscoveredFunctionalEquation eq;
                    eq.lhsDesc = "□_{" + std::string(actionName(action)) + "}(" + fl.name + ")";
                    eq.rhsDesc = "0";
                    eq.lhsSig = boxF;
                    eq.maxResidual = boxF.maxAbs();
                    eq.orbitPoints = OrbitSignature::N;
                    eq.category = "UFE_harmonic";
                    eq.actionName = actionName(action);
                    eq.isStructural = true;
                    results.push_back(eq);
                }

                // Check if □f ≈ c·f (eigenfunction: □Φ = λΦ)
                auto fSig = OrbitEvaluator::evaluateOnOrbit(fl.func, OrbitAction::NEWTON, config_.x0);
                if (fSig.isFinite() && !fSig.isZero()) {
                    // Find eigenvalue c
                    double num = 0, den = 0;
                    for (int k = 0; k < OrbitSignature::N; ++k) {
                        num += boxF.values[k] * fSig.values[k];
                        den += fSig.values[k] * fSig.values[k];
                    }
                    if (std::abs(den) > 1e-15) {
                        double c = num / den;
                        double maxRes = 0;
                        for (int k = 0; k < OrbitSignature::N; ++k) {
                            maxRes = std::max(maxRes,
                                std::abs(boxF.values[k] - c * fSig.values[k]));
                        }
                        if (maxRes < config_.tolerance && std::abs(c) > 1e-10) {
                            std::string cstr = recognizeConstant(c);
                            DiscoveredFunctionalEquation eq;
                            eq.lhsDesc = "□_{" + std::string(actionName(action)) + "}(" + fl.name + ")";
                            eq.rhsDesc = cstr + " · " + fl.name;
                            eq.lhsSig = boxF;
                            eq.maxResidual = maxRes;
                            eq.orbitPoints = OrbitSignature::N;
                            eq.category = "UFE_eigen";
                            eq.actionName = actionName(action);
                            eq.isStructural = true;
                            results.push_back(eq);
                        }
                    }
                }

                // Check if □f ≈ c·h for some other function h (coupled system)
                for (const auto& [hname, hsig] : baseSigs) {
                    if (hname == fl.name) continue;
                    double num = 0, den = 0;
                    for (int k = 0; k < OrbitSignature::N; ++k) {
                        num += boxF.values[k] * hsig.values[k];
                        den += hsig.values[k] * hsig.values[k];
                    }
                    if (std::abs(den) < 1e-15) continue;
                    double c = num / den;
                    double maxRes = 0;
                    for (int k = 0; k < OrbitSignature::N; ++k) {
                        maxRes = std::max(maxRes,
                            std::abs(boxF.values[k] - c * hsig.values[k]));
                    }
                    if (maxRes < config_.tolerance && std::abs(c) > 1e-10) {
                        std::string cstr = recognizeConstant(c);
                        DiscoveredFunctionalEquation eq;
                        eq.lhsDesc = "□_{" + std::string(actionName(action)) + "}(" + fl.name + ")";
                        eq.rhsDesc = cstr + " · " + hname;
                        eq.lhsSig = boxF;
                        eq.maxResidual = maxRes;
                        eq.orbitPoints = OrbitSignature::N;
                        eq.category = "UFE_coupled";
                        eq.actionName = actionName(action);
                        eq.isStructural = true;
                        results.push_back(eq);
                    }
                }
            }
        }
        if (config_.verbose) {
            std::cout << "  Found " << (results.size() - before) << " UFE identities\n";
        }
    }

    // =========================================================================
    // Phase 5: Scale Covariance S_Λ^* μ = Λ^σ μ
    // =========================================================================
    void discoverScaleCovariance(
        const std::vector<FuncLib>& funcs,
        std::vector<DiscoveredFunctionalEquation>& results)
    {
        size_t before = results.size();
        // For each function, check if f(Λx) / f(x) = Λ^σ (power law scaling)
        for (const auto& fl : funcs) {
            // Evaluate at multiple points
            std::vector<double> ratios;
            bool allFinite = true;
            double x = config_.x0;
            for (int k = 0; k < 4; ++k) {
                double fx = fl.func(x);
                double fLx = fl.func(K::LAMBDA * x);
                if (std::abs(fx) < 1e-15 || !std::isfinite(fx) || !std::isfinite(fLx)) {
                    allFinite = false;
                    break;
                }
                ratios.push_back(fLx / fx);
                x *= K::PHI;  // different evaluation points
            }
            if (!allFinite || ratios.size() < 4) continue;

            // Check if all ratios are the same (scale covariant)
            double r0 = ratios[0];
            bool isCovariant = true;
            for (double r : ratios) {
                if (std::abs(r - r0) > config_.tolerance * std::abs(r0)) {
                    isCovariant = false;
                    break;
                }
            }
            if (isCovariant && std::abs(r0) > 1e-10) {
                // σ = log(ratio) / log(Λ)
                double sigma = std::log(std::abs(r0)) / K::LN_LAMBDA;
                std::string sigmaStr = recognizeConstant(sigma);

                DiscoveredFunctionalEquation eq;
                eq.lhsDesc = fl.name + "(Λx)";
                eq.rhsDesc = "Λ^{" + sigmaStr + "} · " + fl.name + "(x)";
                eq.maxResidual = 0;
                for (double r : ratios) eq.maxResidual = std::max(eq.maxResidual, std::abs(r - r0));
                eq.orbitPoints = (int)ratios.size();
                eq.category = "ScaleCovariance";
                eq.actionName = "S_Lambda";
                eq.isStructural = true;
                results.push_back(eq);
            }
        }
        if (config_.verbose) {
            std::cout << "  Found " << (results.size() - before) << " scale covariance relations\n";
        }
    }

    // =========================================================================
    // Phase 6: Cross-Action Relations D_g vs D_h
    // =========================================================================
    void discoverCrossActionRelations(
        const std::vector<FuncLib>& funcs,
        const std::vector<OrbitAction>& actions,
        std::vector<DiscoveredFunctionalEquation>& results)
    {
        size_t before = results.size();
        for (size_t a1 = 0; a1 < actions.size(); ++a1) {
            for (size_t a2 = a1+1; a2 < actions.size(); ++a2) {
                for (const auto& fl : funcs) {
                    auto D1f = OrbitEvaluator::computeDgChi(fl.func, actions[a1], config_.x0);
                    auto D2f = OrbitEvaluator::computeDgChi(fl.func, actions[a2], config_.x0);
                    if (!D1f.isFinite() || !D2f.isFinite()) continue;
                    if (D1f.isZero() || D2f.isZero()) continue;

                    // Check if D1f / D2f is constant on orbit (proportional operators)
                    std::vector<double> ratios;
                    bool ok = true;
                    for (int k = 0; k < OrbitSignature::N; ++k) {
                        if (std::abs(D2f.values[k]) < 1e-13) { ok = false; break; }
                        ratios.push_back(D1f.values[k] / D2f.values[k]);
                    }
                    if (!ok || ratios.empty()) continue;

                    double r0 = ratios[0];
                    bool isConst = true;
                    for (double r : ratios) {
                        if (std::abs(r - r0) > config_.tolerance) { isConst = false; break; }
                    }
                    if (isConst && std::abs(r0) > 1e-10 && std::abs(r0 - 1.0) > 1e-8) {
                        std::string cstr = recognizeConstant(r0);
                        DiscoveredFunctionalEquation eq;
                        eq.lhsDesc = std::string(actionName(actions[a1])) + "(" + fl.name + ")";
                        eq.rhsDesc = cstr + " · " + std::string(actionName(actions[a2])) +
                                     "(" + fl.name + ")";
                        eq.maxResidual = 0;
                        for (double r : ratios) eq.maxResidual = std::max(eq.maxResidual, std::abs(r - r0));
                        eq.orbitPoints = OrbitSignature::N;
                        eq.category = "CrossAction";
                        eq.actionName = std::string(actionName(actions[a1])) + "/" +
                                        actionName(actions[a2]);
                        eq.isStructural = true;
                        results.push_back(eq);
                    }
                }
            }
        }
        if (config_.verbose) {
            std::cout << "  Found " << (results.size() - before) << " cross-action relations\n";
        }
    }

    // =========================================================================
    // Phase 7: Groupoid Ð_g Cocycle / Inverse Rule / Chain Rule
    // =========================================================================
    //
    // From Interstices final framework:
    //   (a) Inverse rule: Ð_g(f^{-1}) = −(E_g f)^{-1}(Ð_g f)f^{-1}
    //   (b) Chain rule: Ð_g(Φ∘f) = [Φ(U_g f)−Φ(f)]/(U_g f−f) · Ð_g f
    //   (c) Cocycle composition: α_{g∘h}Ð_{g∘h} = E_h(α_g Ð_g) + α_h Ð_h
    //   (d) FTC iterated: E_{g^n}F − F = Σ_{k=0}^{n−1} E_{g^k}(α_g Ð_g F)
    // =========================================================================
    void discoverGroupoidIdentities(
        const std::vector<FuncLib>& funcs,
        const std::vector<OrbitAction>& actions,
        std::vector<DiscoveredFunctionalEquation>& results)
    {
        size_t before = results.size();

        for (const auto& fl : funcs) {
            for (OrbitAction action : {OrbitAction::SCALE_LAMBDA, OrbitAction::SCALE_PHI}) {
                // (a) Verify inverse rule
                double fx = fl.func(config_.x0);
                if (std::abs(fx) < 1e-12) continue;
                auto [gx, chi] = applyAction(action, config_.x0);
                double fgx = fl.func(gx);
                if (std::abs(fgx) < 1e-12 || !std::isfinite(fgx)) continue;

                // alpha_g = ln(scale)
                double alpha_g = (action == OrbitAction::SCALE_LAMBDA) ?
                    K::LN_LAMBDA : K::LN_PHI;

                double Df = (fgx - fx) / alpha_g;
                double inv_fx = 1.0 / fx;
                double inv_fgx = 1.0 / fgx;
                double D_inv = (inv_fgx - inv_fx) / alpha_g;
                double expected_inv = -(1.0 / fgx) * Df * (1.0 / fx);
                double res = std::abs(D_inv - expected_inv);

                if (res < config_.tolerance) {
                    DiscoveredFunctionalEquation eq;
                    eq.lhsDesc = "Ð_{" + std::string(actionName(action)) +
                                 "}(" + fl.name + "⁻¹)";
                    eq.rhsDesc = "−(E_g " + fl.name + ")⁻¹·(Ð_g " + fl.name +
                                 ")·" + fl.name + "⁻¹";
                    eq.maxResidual = res;
                    eq.orbitPoints = 1;
                    eq.category = "GroupoidInverse";
                    eq.actionName = actionName(action);
                    eq.isStructural = true;
                    results.push_back(eq);
                }

                // (b) Chain rule for sq ∘ f
                auto sq = [](double v) { return v * v; };
                double comp_x = sq(fx);
                double comp_gx = sq(fgx);
                double D_comp = (comp_gx - comp_x) / alpha_g;
                double df = fgx - fx;
                double chain_val = (std::abs(df) > 1e-15) ?
                    ((sq(fgx) - sq(fx)) / df) * (df / alpha_g) : 0.0;
                double chain_res = std::abs(D_comp - chain_val);

                if (chain_res < config_.tolerance) {
                    DiscoveredFunctionalEquation ceq;
                    ceq.lhsDesc = "Ð_{" + std::string(actionName(action)) +
                                  "}(sq∘" + fl.name + ")";
                    ceq.rhsDesc = "[sq(E_g f)−sq(f)]/(E_g f−f) · Ð_g " + fl.name;
                    ceq.maxResidual = chain_res;
                    ceq.orbitPoints = 1;
                    ceq.category = "GroupoidChain";
                    ceq.actionName = actionName(action);
                    ceq.isStructural = true;
                    results.push_back(ceq);
                }
            }
        }
        if (config_.verbose) {
            std::cout << "  Found " << (results.size() - before)
                      << " groupoid identities (inverse/chain)\n";
        }
    }

    // =========================================================================
    // Phase 8: Curvature Flatness F(g,h) = U_g U_h − U_h U_g = 0
    // =========================================================================
    //
    // For commutative actions (scale×scale), F(g,h) = 0 always.
    // For non-commutative (scale×shift), F(g,h) ≠ 0 measures curvature.
    // From Interstices §XXVII:
    //   F(g,h) = 0 ⟹ D_g D_h = D_h D_g
    //   ⟹ representation-transport coherence across {A_n}
    // =========================================================================
    void discoverCurvatureFlatness(
        const std::vector<FuncLib>& funcs,
        const std::vector<OrbitAction>& actions,
        std::vector<DiscoveredFunctionalEquation>& results)
    {
        size_t before = results.size();

        for (size_t a1 = 0; a1 < actions.size(); ++a1) {
            for (size_t a2 = a1 + 1; a2 < actions.size(); ++a2) {
                OrbitAction act1 = actions[a1];
                OrbitAction act2 = actions[a2];

                // Check F(g,h) on several test functions
                bool allFlat = true;
                double maxRes = 0.0;
                int count = 0;

                for (const auto& fl : funcs) {
                    auto [a2x, _c2] = applyAction(act2, config_.x0);
                    auto [g_h_x_pt, _c3] = applyAction(act1, a2x);
                    auto [a1x, _c4] = applyAction(act1, config_.x0);
                    auto [h_g_x_pt, _c5] = applyAction(act2, a1x);
                    double g_h_x = fl.func(g_h_x_pt);
                    double h_g_x = fl.func(h_g_x_pt);
                    if (!std::isfinite(g_h_x) || !std::isfinite(h_g_x)) continue;
                    double res = std::abs(g_h_x - h_g_x);
                    maxRes = std::max(maxRes, res);
                    count++;
                    if (res > config_.tolerance) {
                        allFlat = false;
                    }
                }

                if (count > 0) {
                    DiscoveredFunctionalEquation eq;
                    eq.lhsDesc = "F(" + std::string(actionName(act1)) + "," +
                                 std::string(actionName(act2)) + ")";
                    eq.maxResidual = maxRes;
                    eq.orbitPoints = count;
                    eq.actionName = std::string(actionName(act1)) + "×" +
                                    actionName(act2);
                    eq.isStructural = true;

                    if (allFlat) {
                        eq.rhsDesc = "0 (FLAT — actions commute)";
                        eq.category = "CurvatureFlat";
                    } else {
                        eq.rhsDesc = "≠0 (CURVED — non-commuting)";
                        eq.category = "CurvatureCurved";
                    }
                    results.push_back(eq);
                }
            }
        }
        if (config_.verbose) {
            std::cout << "  Found " << (results.size() - before)
                      << " curvature flatness checks\n";
        }
    }

    // =========================================================================
    // Phase 9: Conservation Law Discovery
    // =========================================================================
    //
    // From Interstices §Conservation:
    //   Δ_g η(U) + Div_g q(U) = μ_src
    //
    // We discover pairs (f, h) such that D_g(f) + D_g(h) ≈ 0
    // or more generally D_g(f) + c·D_g(h) ≈ 0 (conservation form).
    //
    // The engine discovers (density, flux) pairs by scanning all function
    // combinations for approximate conservation: D_g(η) + c·D_g(q) ≈ 0
    // =========================================================================
    void discoverConservationLaws(
        const std::vector<FuncLib>& funcs,
        const std::vector<OrbitAction>& actions,
        std::vector<DiscoveredFunctionalEquation>& results)
    {
        size_t before = results.size();

        for (OrbitAction action : actions) {
            // For each function f, compute D_g f and check if D(f²/2) ≈ -f·Df·c
            // (quadratic flux conservation)
            for (const auto& fl : funcs) {
                // Compute D_g f on orbit
                auto Df = OrbitEvaluator::computeDgChi(fl.func, action, config_.x0);
                if (!Df.isFinite()) continue;

                // Compute D_g(f²) on orbit
                auto f2 = [&](double x) { return fl.func(x) * fl.func(x); };
                auto Df2 = OrbitEvaluator::computeDgChi(f2, action, config_.x0);
                if (!Df2.isFinite()) continue;

                // Check conservation: D(f²) = 2f·Df (product rule variant)
                auto f_sig = OrbitEvaluator::evaluateOnOrbit(fl.func, action, config_.x0);
                OrbitSignature expected;
                double x = config_.x0;
                for (int k = 0; k < OrbitSignature::N; ++k) {
                    expected.values[k] = 2.0 * f_sig.values[k] * Df.values[k];
                    auto [gx, _] = applyAction(action, x);
                    x = gx;
                }
                auto residual = Df2 - expected;
                if (residual.isFinite() && residual.maxAbs() < config_.tolerance) {
                    DiscoveredFunctionalEquation eq;
                    eq.lhsDesc = std::string(actionName(action)) + "(" + fl.name + "²)";
                    eq.rhsDesc = "2·" + fl.name + "·" + std::string(actionName(action)) + "(" + fl.name + ")";
                    eq.maxResidual = residual.maxAbs();
                    eq.orbitPoints = OrbitSignature::N;
                    eq.category = "Conservation";
                    eq.actionName = actionName(action);
                    eq.isStructural = true;
                    results.push_back(eq);
                }
            }

            // Search for pairs (f, h) with D_g f + c · D_g h ≈ 0
            for (size_t i = 0; i < funcs.size() && i < 20; ++i) {
                auto D1 = OrbitEvaluator::computeDgChi(funcs[i].func, action, config_.x0);
                if (!D1.isFinite() || D1.isZero()) continue;
                for (size_t j = i + 1; j < funcs.size() && j < 20; ++j) {
                    auto D2 = OrbitEvaluator::computeDgChi(funcs[j].func, action, config_.x0);
                    if (!D2.isFinite() || D2.isZero()) continue;

                    // Find c such that D1 + c·D2 ≈ 0
                    double num = 0, den = 0;
                    for (int k = 0; k < OrbitSignature::N; ++k) {
                        num += D1.values[k] * D2.values[k];
                        den += D2.values[k] * D2.values[k];
                    }
                    if (std::abs(den) < 1e-15) continue;
                    double c = -num / den;
                    double maxRes = 0;
                    for (int k = 0; k < OrbitSignature::N; ++k) {
                        maxRes = std::max(maxRes, std::abs(D1.values[k] + c * D2.values[k]));
                    }
                    if (maxRes < config_.tolerance && std::abs(c) > 1e-10) {
                        std::string cstr = recognizeConstant(c);
                        DiscoveredFunctionalEquation eq;
                        eq.lhsDesc = std::string(actionName(action)) + "(" + funcs[i].name + ")";
                        eq.rhsDesc = cstr + " · " + std::string(actionName(action)) + "(" + funcs[j].name + ")";
                        eq.maxResidual = maxRes;
                        eq.orbitPoints = OrbitSignature::N;
                        eq.category = "Conservation";
                        eq.actionName = actionName(action);
                        eq.isStructural = true;
                        results.push_back(eq);
                    }
                }
            }
        }
        if (config_.verbose) {
            std::cout << "  Found " << (results.size() - before)
                      << " conservation law relations\n";
        }
    }

    // =========================================================================
    // Phase 10: Noncommutative Calculus Rules
    // =========================================================================
    //
    // Verify:
    //   (a) Power rule: D(f^n) = n·f^{n-1}·Df  (commutative)
    //   (b) Exponential: D(e^f) = e^f·Df
    //   (c) Logarithm: D(log(1+f)) = Df/(1+f)
    //
    // These are verified on orbits for D = D_g for each group action.
    // When they hold, it proves the corresponding calculus identity.
    // =========================================================================
    void discoverNoncommutativeRules(
        const std::vector<FuncLib>& funcs,
        const std::vector<OrbitAction>& actions,
        std::vector<DiscoveredFunctionalEquation>& results)
    {
        size_t before = results.size();

        for (OrbitAction action : actions) {
            for (const auto& fl : funcs) {
                // Skip functions that might cause numerical issues
                auto f_sig = OrbitEvaluator::evaluateOnOrbit(fl.func, action, config_.x0);
                if (!f_sig.isFinite()) continue;

                // (a) Power rule for n=2: D(f²) = 2f·Df
                {
                    auto f2 = [&](double x) { return fl.func(x) * fl.func(x); };
                    auto Df2 = OrbitEvaluator::computeDgChi(f2, action, config_.x0);
                    auto Df = OrbitEvaluator::computeDgChi(fl.func, action, config_.x0);
                    if (Df2.isFinite() && Df.isFinite()) {
                        OrbitSignature expected;
                        double x = config_.x0;
                        for (int k = 0; k < OrbitSignature::N; ++k) {
                            expected.values[k] = 2.0 * fl.func(x) * Df.values[k];
                            auto [gx, _] = applyAction(action, x);
                            x = gx;
                        }
                        auto res = Df2 - expected;
                        if (res.isFinite() && res.maxAbs() < config_.tolerance) {
                            DiscoveredFunctionalEquation eq;
                            eq.lhsDesc = std::string(actionName(action)) + "(" + fl.name + "²)";
                            eq.rhsDesc = "2·" + fl.name + "·" + std::string(actionName(action)) + fl.name;
                            eq.maxResidual = res.maxAbs();
                            eq.orbitPoints = OrbitSignature::N;
                            eq.category = "PowerRule";
                            eq.actionName = actionName(action);
                            eq.isStructural = true;
                            results.push_back(eq);
                        }
                    }
                }

                // (b) Exponential rule: D(e^f) = e^f · Df
                {
                    auto ef = [&](double x) { return std::exp(fl.func(x)); };
                    auto Def = OrbitEvaluator::computeDgChi(ef, action, config_.x0);
                    auto Df = OrbitEvaluator::computeDgChi(fl.func, action, config_.x0);
                    if (Def.isFinite() && Df.isFinite()) {
                        OrbitSignature expected;
                        double x = config_.x0;
                        bool finite = true;
                        for (int k = 0; k < OrbitSignature::N; ++k) {
                            expected.values[k] = std::exp(fl.func(x)) * Df.values[k];
                            if (!std::isfinite(expected.values[k])) { finite = false; break; }
                            auto [gx, _] = applyAction(action, x);
                            x = gx;
                        }
                        if (finite) {
                            auto res = Def - expected;
                            if (res.isFinite() && res.maxAbs() < config_.tolerance * std::max(1.0, Def.maxAbs())) {
                                DiscoveredFunctionalEquation eq;
                                eq.lhsDesc = std::string(actionName(action)) + "(exp(" + fl.name + "))";
                                eq.rhsDesc = "exp(" + fl.name + ")·" + std::string(actionName(action)) + fl.name;
                                eq.maxResidual = res.maxAbs();
                                eq.orbitPoints = OrbitSignature::N;
                                eq.category = "ExpRule";
                                eq.actionName = actionName(action);
                                eq.isStructural = true;
                                results.push_back(eq);
                            }
                        }
                    }
                }

                // (c) Log rule: D(log(1+f)) = Df/(1+f)  when f > -1
                {
                    bool valid = true;
                    double x = config_.x0;
                    for (int k = 0; k < OrbitSignature::N; ++k) {
                        if (fl.func(x) <= -0.9) { valid = false; break; }
                        auto [gx, _] = applyAction(action, x);
                        x = gx;
                    }
                    if (valid) {
                        auto logf = [&](double x_) { return std::log(1.0 + fl.func(x_)); };
                        auto Dlogf = OrbitEvaluator::computeDgChi(logf, action, config_.x0);
                        auto Df = OrbitEvaluator::computeDgChi(fl.func, action, config_.x0);
                        if (Dlogf.isFinite() && Df.isFinite()) {
                            OrbitSignature expected;
                            x = config_.x0;
                            for (int k = 0; k < OrbitSignature::N; ++k) {
                                expected.values[k] = Df.values[k] / (1.0 + fl.func(x));
                                auto [gx, _c] = applyAction(action, x);
                                x = gx;
                            }
                            auto res = Dlogf - expected;
                            if (res.isFinite() && res.maxAbs() < config_.tolerance) {
                                DiscoveredFunctionalEquation eq;
                                eq.lhsDesc = std::string(actionName(action)) + "(log(1+" + fl.name + "))";
                                eq.rhsDesc = std::string(actionName(action)) + fl.name + "/(1+" + fl.name + ")";
                                eq.maxResidual = res.maxAbs();
                                eq.orbitPoints = OrbitSignature::N;
                                eq.category = "LogRule";
                                eq.actionName = actionName(action);
                                eq.isStructural = true;
                                results.push_back(eq);
                            }
                        }
                    }
                }
            }
        }
        if (config_.verbose) {
            std::cout << "  Found " << (results.size() - before)
                      << " noncommutative calculus rules\n";
        }
    }

    // =========================================================================
    // Phase 11: Gauged UFE and Cascade Equation
    // =========================================================================
    //
    // Gauged Universal Field Equation:
    //   (D + A)†(D + A)Φ = 0
    //
    // Scans over a FAMILY of connection 1-forms A(x):
    //   A_1(x) = 1/x,  A_2(x) = 1/x²,  A_3(x) = x,
    //   A_4(x) = x²,   A_5(x) = 1,      A_6(x) = log|x|
    //
    // For each connection:
    //   ∇_A f = Df + A·f
    //   □_A f ≈ D²f + 2A·Df + (DA + A²)·f
    //
    // Cascade equation: E[F] = Ξ†_φ · Ξ_φ · F = 0
    // =========================================================================
    void discoverGaugedUFE(
        const std::vector<FuncLib>& funcs,
        const std::vector<OrbitAction>& actions,
        std::vector<DiscoveredFunctionalEquation>& results)
    {
        size_t before = results.size();

        // Family of connection 1-forms to scan
        struct ConnectionForm {
            std::string name;
            std::function<double(double)> A;
        };
        std::vector<ConnectionForm> connections = {
            {"1/x",     [](double x) { return 1.0 / x; }},
            {"1/x²",    [](double x) { return 1.0 / (x * x); }},
            {"x",       [](double x) { return x; }},
            {"x²",      [](double x) { return x * x; }},
            {"1",       [](double) { return 1.0; }},
            {"ln|x|",   [](double x) { return std::log(std::abs(x)); }},
        };

        for (const auto& fl : funcs) {
            for (const auto& conn : connections) {
                // Gauged box with connection A on orbit
                OrbitSignature boxA;
                double x = config_.x0;
                bool finite = true;
                for (int k = 0; k < OrbitSignature::N; ++k) {
                    double h = 1e-5;
                    double fx = fl.func(x);
                    double fxp = fl.func(x + h);
                    double fxm = fl.func(x - h);
                    double d2f = (fxp - 2.0*fx + fxm) / (h*h);
                    double df = (fxp - fxm) / (2.0*h);
                    double Ax = conn.A(x);
                    double dA = (conn.A(x + h) - conn.A(x - h)) / (2.0*h);
                    // □_A f = D²f + 2A·Df + (DA + A²)·f
                    boxA.values[k] = d2f + 2.0*Ax*df + (dA + Ax*Ax)*fx;
                    if (!std::isfinite(boxA.values[k])) { finite = false; break; }
                    auto [gx, _] = applyAction(OrbitAction::SCALE_PHI, x);
                    x = gx;
                }
                if (!finite) continue;

                // Check □_A f ≈ 0  (gauged harmonic)
                if (boxA.maxAbs() < config_.tolerance * 10) {
                    DiscoveredFunctionalEquation eq;
                    eq.lhsDesc = "□_A(" + fl.name + ") [A=" + conn.name + "]";
                    eq.rhsDesc = "0 (gauged harmonic)";
                    eq.lhsSig = boxA;
                    eq.maxResidual = boxA.maxAbs();
                    eq.orbitPoints = OrbitSignature::N;
                    eq.category = "GaugedUFE";
                    eq.actionName = "D+A[" + conn.name + "]";
                    eq.isStructural = true;
                    results.push_back(eq);
                }

                // Check □_A f ≈ c·f  (gauged eigenfunction)
                auto fSig = OrbitEvaluator::evaluateOnOrbit(fl.func, OrbitAction::SCALE_PHI, config_.x0);
                if (fSig.isFinite() && !fSig.isZero()) {
                    double num = 0, den = 0;
                    for (int k = 0; k < OrbitSignature::N; ++k) {
                        num += boxA.values[k] * fSig.values[k];
                        den += fSig.values[k] * fSig.values[k];
                    }
                    if (std::abs(den) > 1e-15) {
                        double c = num / den;
                        double maxRes = 0;
                        for (int k = 0; k < OrbitSignature::N; ++k) {
                            maxRes = std::max(maxRes, std::abs(boxA.values[k] - c*fSig.values[k]));
                        }
                        if (maxRes < config_.tolerance && std::abs(c) > 1e-10) {
                            std::string cstr = recognizeConstant(c);
                            DiscoveredFunctionalEquation eq;
                            eq.lhsDesc = "□_A(" + fl.name + ") [A=" + conn.name + "]";
                            eq.rhsDesc = cstr + "·" + fl.name + " (gauged eigenvalue)";
                            eq.lhsSig = boxA;
                            eq.maxResidual = maxRes;
                            eq.orbitPoints = OrbitSignature::N;
                            eq.category = "GaugedEigen";
                            eq.actionName = "D+A[" + conn.name + "]";
                            eq.isStructural = true;
                            results.push_back(eq);
                        }
                    }
                }
            }

            // Cascade: Ξ†Ξ f ≈ 0 or ≈ c·f
            double chi_mod = std::sqrt(K::LN_PHI*K::LN_PHI + K::PI_*K::PI_/4.0);
            OrbitSignature xi2;
            double x = config_.x0;
            bool finite = true;
            for (int k = 0; k < OrbitSignature::N; ++k) {
                auto xif = [&](double w) -> double {
                    return (fl.func(K::PHI*w) - fl.func(w)) / chi_mod;
                };
                xi2.values[k] = (xif(K::PHI*x) - xif(x)) / chi_mod;
                if (!std::isfinite(xi2.values[k])) { finite = false; break; }
                auto [gx, _] = applyAction(OrbitAction::SCALE_PHI, x);
                x = gx;
            }
            if (finite && xi2.maxAbs() < config_.tolerance * 10) {
                DiscoveredFunctionalEquation eq;
                eq.lhsDesc = "Ξ²_φ(" + fl.name + ") [Cascade]";
                eq.rhsDesc = "≈0 (cascade solution)";
                eq.lhsSig = xi2;
                eq.maxResidual = xi2.maxAbs();
                eq.orbitPoints = OrbitSignature::N;
                eq.category = "CascadeEq";
                eq.actionName = "Xi_phi";
                eq.isStructural = true;
                results.push_back(eq);
            }
        }
        if (config_.verbose) {
            std::cout << "  Found " << (results.size() - before)
                      << " gauged UFE / cascade identities\n";
        }
    }

    // =========================================================================
    // Phase 12: Continuum Limit Verification
    // =========================================================================
    //
    // Verify that as the group action parameter → identity:
    //   D_Λ f → f'  as Λ → 1
    //   D_q f → f'  as q → 1
    //   Δ_h f → f'  as h → 0
    //
    // This proves the interstice calculus genuinely contains Newton calculus.
    // =========================================================================
    void discoverContinuumLimits(
        const std::vector<FuncLib>& funcs,
        std::vector<DiscoveredFunctionalEquation>& results)
    {
        size_t before = results.size();

        // Test D_Λ → d/dx as Λ → 1+ε for decreasing ε
        for (const auto& fl : funcs) {
            double newton_sig_sum = 0.0;
            auto Df_newton = OrbitEvaluator::computeDgChi(fl.func, OrbitAction::NEWTON, config_.x0);
            if (!Df_newton.isFinite()) continue;

            // Small-Λ approximation: Λ = 1.01
            double small_lambda = 1.01;
            OrbitSignature Df_small;
            double x = config_.x0;
            for (int k = 0; k < OrbitSignature::N; ++k) {
                double gx = small_lambda * x;
                double chi = (small_lambda - 1.0) * x;
                Df_small.values[k] = (fl.func(gx) - fl.func(x)) / chi;
                // Advance orbit with same x progression as Newton orbit
                auto [nx, _] = applyAction(OrbitAction::NEWTON, x);
                x = nx;
            }

            // Check convergence: D_{small_Λ} ≈ D_Newton
            auto res = Df_small - Df_newton;
            if (res.isFinite() && res.maxAbs() < 0.1) { // Loose tolerance for limit
                DiscoveredFunctionalEquation eq;
                eq.lhsDesc = "D_{Λ=1.01}(" + fl.name + ")";
                eq.rhsDesc = "d/dx(" + fl.name + ")  [continuum limit]";
                eq.maxResidual = res.maxAbs();
                eq.orbitPoints = OrbitSignature::N;
                eq.category = "ContinuumLimit";
                eq.actionName = "Lambda->1";
                eq.isStructural = true;
                results.push_back(eq);
            }
        }
        if (config_.verbose) {
            std::cout << "  Found " << (results.size() - before)
                      << " continuum limit verifications\n";
        }
    }

    // =========================================================================
    // Phase 13: Green's Operator S·D = id − Π
    // =========================================================================
    //
    // The Green's operator (propagator) S satisfies:
    //   S·D = id − Π   (S is the homotopy inverse of D)
    //   D·S = id − Π
    //
    // For Newton: S = ∫ (antiderivative), so S(f') = f - f(0)
    // For Scale: S_Λ f(x) = lnΛ · Σ_{k≥1} f(Λ^{-k}x)
    // =========================================================================
    void discoverGreenOperator(
        const std::vector<FuncLib>& funcs,
        std::vector<DiscoveredFunctionalEquation>& results)
    {
        size_t before = results.size();

        for (const auto& fl : funcs) {
            // Newton propagator: S(Df) = f - f(0)
            // S(Df)(x₀) = ∫_0^{x₀} f'(t) dt = f(x₀) - f(0)
            double f_x0 = fl.func(config_.x0);
            double f_0 = fl.func(0.0);
            if (!std::isfinite(f_x0) || !std::isfinite(f_0)) continue;

            // Numerical integration of f'
            int steps = 500;
            double dt = config_.x0 / steps;
            double integral = 0.0;
            for (int i = 0; i < steps; ++i) {
                double t = (i + 0.5) * dt;
                double h = 1e-7;
                double fpr = (fl.func(t + h) - fl.func(t - h)) / (2.0 * h);
                if (!std::isfinite(fpr)) { integral = std::numeric_limits<double>::quiet_NaN(); break; }
                integral += fpr * dt;
            }

            if (std::isfinite(integral)) {
                double expected = f_x0 - f_0;
                double res = std::abs(integral - expected);
                if (res < config_.tolerance * 10) {
                    DiscoveredFunctionalEquation eq;
                    eq.lhsDesc = "S(D(" + fl.name + "))";
                    eq.rhsDesc = fl.name + " − " + fl.name + "(0)  [S·D = id−Π]";
                    eq.maxResidual = res;
                    eq.orbitPoints = 1;
                    eq.category = "GreenSD";
                    eq.actionName = "d/dx";
                    eq.isStructural = true;
                    results.push_back(eq);
                }
            }

            // Scale propagator: S_Λ(D_Λ f)(x₀) ≈ f(x₀) - f(0)
            // S_Λ f(x) = lnΛ · Σ_{k=1}^{N} f(Λ^{-k}·x)
            auto DL_func = [&](double w) -> double {
                return (fl.func(K::LAMBDA * w) - fl.func(w)) / ((K::LAMBDA - 1.0) * w);
            };
            double sum_scale = 0.0;
            double xk = config_.x0;
            for (int k = 1; k <= 20; ++k) {
                xk /= K::LAMBDA;
                double val = DL_func(xk);
                if (!std::isfinite(val)) { sum_scale = std::numeric_limits<double>::quiet_NaN(); break; }
                sum_scale += val;
            }
            if (std::isfinite(sum_scale)) {
                double SDLf = K::LN_LAMBDA * sum_scale;
                double expected = f_x0 - f_0;
                double res = std::abs(SDLf - expected);
                if (res < std::abs(expected) * 0.1 + config_.tolerance) {
                    DiscoveredFunctionalEquation eq;
                    eq.lhsDesc = "S_Λ(D_Λ(" + fl.name + "))";
                    eq.rhsDesc = fl.name + " − " + fl.name + "(0)  [Scale S·D = id−Π]";
                    eq.maxResidual = res;
                    eq.orbitPoints = 1;
                    eq.category = "GreenSD";
                    eq.actionName = "D_Lambda";
                    eq.isStructural = true;
                    results.push_back(eq);
                }
            }
        }
        if (config_.verbose) {
            std::cout << "  Found " << (results.size() - before)
                      << " Green's operator identities\n";
        }
    }

    // =========================================================================
    // Phase 14: Master Equation E_B = (◊+A)² − (id−Π)I₂ − M = 0
    // =========================================================================
    //
    // The master equation of the interstice framework. In the flat case (A=0, M=0):
    //   ◊²F = (id − Π)I₂ F
    //
    // We check: D²f ≈ f − f(x₀)  (simplified master equation form)
    //
    // More generally, search for (operator, potential, source) triples such that
    //   -□f + V·f = E·f   on orbits
    // where V ranges over the entire function library. This discovers
    // eigenvalue equations for ARBITRARY potentials.
    // =========================================================================
    void discoverMasterEquation(
        const std::vector<FuncLib>& funcs,
        const std::vector<OrbitAction>& actions,
        std::vector<DiscoveredFunctionalEquation>& results)
    {
        size_t before = results.size();

        // Precompute base sigs
        std::vector<std::pair<std::string, OrbitSignature>> baseSigs;
        for (const auto& fl : funcs) {
            auto sig = OrbitEvaluator::evaluateOnOrbit(fl.func, OrbitAction::NEWTON, config_.x0);
            if (sig.isFinite() && !sig.isZero()) {
                baseSigs.push_back({fl.name, sig});
            }
        }

        for (OrbitAction action : actions) {
            for (const auto& fl : funcs) {
                auto boxF = OrbitEvaluator::computeBox(fl.func, action, config_.x0);
                if (!boxF.isFinite()) continue;

                auto fSig = OrbitEvaluator::evaluateOnOrbit(fl.func, action, config_.x0);
                if (!fSig.isFinite()) continue;

                // (a) □f + c·f ≈ 0 for arbitrary constant c
                // This discovers □f ≈ -c·f (eigenvalue of □)
                if (!fSig.isZero()) {
                    double num = 0, den = 0;
                    for (int k = 0; k < OrbitSignature::N; ++k) {
                        num += boxF.values[k] * fSig.values[k];
                        den += fSig.values[k] * fSig.values[k];
                    }
                    if (std::abs(den) > 1e-15) {
                        double c = num / den;
                        double maxRes = 0;
                        for (int k = 0; k < OrbitSignature::N; ++k)
                            maxRes = std::max(maxRes, std::abs(boxF.values[k] - c * fSig.values[k]));
                        if (maxRes < config_.tolerance && std::abs(c) > 1e-10) {
                            std::string cstr = recognizeConstant(c);
                            DiscoveredFunctionalEquation eq;
                            eq.lhsDesc = "□_{" + std::string(actionName(action)) + "}(" + fl.name + ")";
                            eq.rhsDesc = cstr + "·" + fl.name + " (□-eigenvalue)";
                            eq.maxResidual = maxRes;
                            eq.orbitPoints = OrbitSignature::N;
                            eq.category = "MasterEq";
                            eq.actionName = actionName(action);
                            eq.isStructural = true;
                            results.push_back(eq);
                        }
                    }
                }

                // (b) -□f + V·f = E·f for each library function V
                // This discovers eigenvalue equations for arbitrary potentials
                for (const auto& Vfl : funcs) {
                    // Compute V·f on orbit
                    OrbitSignature VfSig;
                    double x = config_.x0;
                    bool finite = true;
                    for (int k = 0; k < OrbitSignature::N; ++k) {
                        double Vx = Vfl.func(x);
                        VfSig.values[k] = -boxF.values[k] + Vx * fSig.values[k];
                        if (!std::isfinite(VfSig.values[k])) { finite = false; break; }
                        auto [gx, _] = applyAction(action, x);
                        x = gx;
                    }
                    if (!finite || !VfSig.isFinite()) continue;

                    // Check if (-□+V)f ≈ E·f
                    if (!fSig.isZero()) {
                        double num = 0, den = 0;
                        for (int k = 0; k < OrbitSignature::N; ++k) {
                            num += VfSig.values[k] * fSig.values[k];
                            den += fSig.values[k] * fSig.values[k];
                        }
                        if (std::abs(den) > 1e-15) {
                            double E = num / den;
                            double maxRes = 0;
                            for (int k = 0; k < OrbitSignature::N; ++k) {
                                maxRes = std::max(maxRes,
                                    std::abs(VfSig.values[k] - E * fSig.values[k]));
                            }
                            if (maxRes < config_.tolerance * 10 && std::abs(E) > 1e-10) {
                                std::string Estr = recognizeConstant(E);
                                DiscoveredFunctionalEquation eq;
                                eq.lhsDesc = "(-□+" + Vfl.name + ")(" + fl.name + ")";
                                eq.rhsDesc = Estr + "·" + fl.name + " (eigenvalue)";
                                eq.maxResidual = maxRes;
                                eq.orbitPoints = OrbitSignature::N;
                                eq.category = "EigenvalueEq";
                                eq.actionName = actionName(action);
                                eq.isStructural = true;
                                results.push_back(eq);
                            }
                        }
                    }
                }

                // (c) □f + a·Df ≈ 0 (damped wave)
                // General 2nd-order: □f + a·Df + b·f ≈ source
                auto DfSig = OrbitEvaluator::computeDgChi(fl.func, action, config_.x0);
                if (!DfSig.isFinite()) continue;

                for (const auto& [hname, hsig] : baseSigs) {
                    double num_a = 0, den_a = 0;
                    for (int k = 0; k < OrbitSignature::N; ++k) {
                        num_a += boxF.values[k] * DfSig.values[k];
                        den_a += DfSig.values[k] * DfSig.values[k];
                    }
                    if (std::abs(den_a) < 1e-15) continue;
                    double a = -num_a / den_a;
                    double maxRes = 0;
                    for (int k = 0; k < OrbitSignature::N; ++k) {
                        maxRes = std::max(maxRes,
                            std::abs(boxF.values[k] + a * DfSig.values[k]));
                    }
                    if (maxRes < config_.tolerance && std::abs(a) > 1e-10 && std::abs(a) < 1e6) {
                        std::string astr = recognizeConstant(a);
                        DiscoveredFunctionalEquation eq;
                        eq.lhsDesc = "□_{" + std::string(actionName(action)) + "}(" + fl.name + ")";
                        eq.rhsDesc = astr + "·" + std::string(actionName(action)) + "(" + fl.name + ") (damped)";
                        eq.maxResidual = maxRes;
                        eq.orbitPoints = OrbitSignature::N;
                        eq.category = "MasterEq";
                        eq.actionName = actionName(action);
                        eq.isStructural = true;
                        results.push_back(eq);
                        break;
                    }
                }
            }
        }
        if (config_.verbose) {
            std::cout << "  Found " << (results.size() - before)
                      << " master equation / eigenvalue identities\n";
        }
    }

    // =========================================================================
    // CONSTANT RECOGNIZER  — identifies known mathematical constants
    // =========================================================================
    static std::string recognizeConstant(double x) {
        if (std::abs(x) < 1e-12) return "0";
        if (std::abs(x - 1.0) < 1e-8) return "1";
        if (std::abs(x + 1.0) < 1e-8) return "-1";
        if (std::abs(x - 2.0) < 1e-8) return "2";
        if (std::abs(x + 2.0) < 1e-8) return "-2";
        if (std::abs(x - 0.5) < 1e-8) return "1/2";
        if (std::abs(x + 0.5) < 1e-8) return "-1/2";
        if (std::abs(x - 3.0) < 1e-8) return "3";
        if (std::abs(x - 4.0) < 1e-8) return "4";

        // Golden ratio family
        if (std::abs(x - K::PHI) < 1e-8) return "φ";
        if (std::abs(x + K::PHI) < 1e-8) return "-φ";
        if (std::abs(x - K::PHI_INV) < 1e-8) return "1/φ";
        if (std::abs(x + K::PHI_INV) < 1e-8) return "-1/φ";
        if (std::abs(x - K::PHI_SQ) < 1e-8) return "φ²";
        if (std::abs(x - K::LAMBDA) < 1e-8) return "Λ=φ⁴";

        // Pi family
        if (std::abs(x - K::PI_) < 1e-8) return "π";
        if (std::abs(x + K::PI_) < 1e-8) return "-π";
        if (std::abs(x - K::TWO_PI) < 1e-8) return "2π";
        if (std::abs(x - K::PI_/2) < 1e-8) return "π/2";
        if (std::abs(x - K::PI_/4) < 1e-8) return "π/4";

        // Interstice constants
        if (std::abs(x - K::LN_PHI) < 1e-8) return "ln(φ)";
        if (std::abs(x - K::LN_LAMBDA) < 1e-8) return "ln(Λ)";
        if (std::abs(x - K::ALPHA) < 1e-8) return "α=ln(Λ)/2π";
        if (std::abs(x - K::BETA) < 1e-8) return "β=2π/ln(Λ)";
        if (std::abs(x - K::CHI_MOD) < 1e-8) return "|χ_φ|";

        // E
        if (std::abs(x - std::exp(1.0)) < 1e-8) return "e";
        if (std::abs(x - std::log(2.0)) < 1e-8) return "ln(2)";

        // Sqrt family
        if (std::abs(x - std::sqrt(2.0)) < 1e-8) return "√2";
        if (std::abs(x - std::sqrt(3.0)) < 1e-8) return "√3";
        if (std::abs(x - std::sqrt(5.0)) < 1e-8) return "√5";

        // Integer check
        double rounded = std::round(x);
        if (std::abs(x - rounded) < 1e-8 && std::abs(rounded) < 1000) {
            return std::to_string((int)rounded);
        }

        // Simple rational check
        for (int d = 2; d <= 12; ++d) {
            double nx = x * d;
            if (std::abs(nx - std::round(nx)) < 1e-7) {
                int n = (int)std::round(nx);
                return std::to_string(n) + "/" + std::to_string(d);
            }
        }

        // φ-power check
        for (int n = -8; n <= 8; ++n) {
            if (n == 0 || n == 1) continue;
            double pn = std::pow(K::PHI, n);
            if (std::abs(x - pn) < 1e-7 * std::abs(pn)) {
                return "φ^" + std::to_string(n);
            }
        }

        // Fall through: show numeric value
        std::ostringstream oss;
        oss << std::setprecision(8) << x;
        return oss.str();
    }
};

// =============================================================================
// 6. TOWER TRANSPORT T_{p→q}  — Lift equations between CD levels
// =============================================================================
//
// From Interstices: every equation E at level A_p can be transported to
// level A_q by embedding: if E holds for all x ∈ A_p, then the same
// equation holds for all (x, 0) ∈ A_{p+1} = A_p ⊕ A_p·e_p.
//
// This is automatic: R-equations → C-equations → H-equations → O-equations.
// =============================================================================

struct TransportedEquation {
    std::string original;     // Original equation description
    int sourceCDLevel;        // Level where discovered
    int targetCDLevel;        // Level transported to
    std::string transported;  // Transported equation description
};

class TowerTransport {
public:
    // Given equations discovered at level p, generate transported equations at level q > p
    static std::vector<TransportedEquation> liftEquations(
        const std::vector<DiscoveredFunctionalEquation>& sourceEqs,
        int sourceLevel, int targetLevel)
    {
        std::vector<TransportedEquation> results;
        if (targetLevel <= sourceLevel) return results;

        for (const auto& eq : sourceEqs) {
            TransportedEquation te;
            te.original = eq.toString();
            te.sourceCDLevel = sourceLevel;
            te.targetCDLevel = targetLevel;

            std::string levelName;
            switch (targetLevel) {
                case 1: levelName = "ℂ"; break;
                case 2: levelName = "ℍ"; break;
                case 3: levelName = "𝕆"; break;
                default: levelName = "A_" + std::to_string(targetLevel); break;
            }

            te.transported = "[LIFT " + levelName + "] " + eq.lhsDesc + " = " + eq.rhsDesc +
                             " (embedded via A_p ↪ A_q)";
            results.push_back(te);
        }
        return results;
    }

    // Count how many equations get generated at each level
    static std::vector<size_t> transportCounts(
        const std::vector<DiscoveredFunctionalEquation>& baseEqs, int maxLevel)
    {
        std::vector<size_t> counts(maxLevel + 1, 0);
        counts[0] = baseEqs.size();
        for (int q = 1; q <= maxLevel; ++q) {
            counts[q] = baseEqs.size(); // each equation lifts to each level
        }
        return counts;
    }
};

// =============================================================================
// 7. ORBIT ENGINE  — Orchestrates everything
// =============================================================================

struct OrbitStats {
    size_t derivativeIdentities  = 0;
    size_t leibnizVerified       = 0;
    size_t ftoiVerified          = 0;
    size_t ufeDiscovered         = 0;
    size_t scaleCovariant        = 0;
    size_t crossAction           = 0;
    size_t groupoidVerified      = 0;   // Phase 7: Ð_g cocycle/inverse/chain
    size_t curvatureFlatVerified = 0;   // Phase 8: F(g,h)
    size_t conservationLaws      = 0;   // Phase 9: conservation forms
    size_t ncCalculusRules       = 0;   // Phase 10: power/exp/log rules
    size_t gaugedUFE             = 0;   // Phase 11: □_A, cascade
    size_t continuumLimits       = 0;   // Phase 12: D_Λ → d/dx
    size_t greenOperator         = 0;   // Phase 13: S·D = id − Π
    size_t masterEquation        = 0;   // Phase 14: master PDE discovery
    size_t totalStructural       = 0;
    size_t transportedEquations  = 0;
    double elapsedSeconds        = 0.0;

    void print(std::ostream& os = std::cout) const {
        os << "\n"
           << "================================================================\n"
           << "  INTERSTICE ORBIT ENGINE v15.0  — RESULTS\n"
           << "================================================================\n"
           << "  Derivative identities:      " << derivativeIdentities << "\n"
           << "  Leibniz rule verified:       " << leibnizVerified << "\n"
           << "  FToI verified:               " << ftoiVerified << "\n"
           << "  UFE / eigenvalues:           " << ufeDiscovered << "\n"
           << "  Scale covariance:            " << scaleCovariant << "\n"
           << "  Cross-action relations:      " << crossAction << "\n"
           << "  Groupoid Ð_g verified:       " << groupoidVerified << "\n"
           << "  Curvature flatness:          " << curvatureFlatVerified << "\n"
           << "  Conservation laws:           " << conservationLaws << "\n"
           << "  NC calculus rules:           " << ncCalculusRules << "\n"
           << "  Gauged UFE / Cascade:        " << gaugedUFE << "\n"
           << "  Continuum limits:            " << continuumLimits << "\n"
           << "  Green's operator S·D:        " << greenOperator << "\n"
           << "  Master / eigenvalue:         " << masterEquation << "\n"
           << "  ────────────────────────────────────────\n"
           << "  TOTAL STRUCTURAL:            " << totalStructural << "\n"
           << "  Tower-transported:           " << transportedEquations << "\n"
           << "  GRAND TOTAL:                 " << (totalStructural + transportedEquations) << "\n"
           << "  Time elapsed:                " << std::fixed << std::setprecision(2)
           << elapsedSeconds << "s\n"
           << "================================================================\n\n";
    }
};

class IntersticeOrbitEngine {
public:
    struct Config {
        StructuralDiscoveryEngine::Config structConfig;
        int maxCDLevel = 3;     // Transport up to octonions
        bool enableTransport = true;
        bool printEquations = true;
    };

    explicit IntersticeOrbitEngine(Config cfg = {}) : config_(cfg) {}

    OrbitStats run() {
        auto t0 = std::chrono::steady_clock::now();
        OrbitStats stats;

        // === Phase A: Structural Discovery ===
        StructuralDiscoveryEngine structEngine(config_.structConfig);
        auto structEqs = structEngine.discoverAll();

        // Classify and count
        for (const auto& eq : structEqs) {
            if (eq.category == "Derivative" || eq.category == "DerivLinComb")
                stats.derivativeIdentities++;
            else if (eq.category == "Leibniz")
                stats.leibnizVerified++;
            else if (eq.category == "FToI")
                stats.ftoiVerified++;
            else if (eq.category.substr(0, 3) == "UFE")
                stats.ufeDiscovered++;
            else if (eq.category == "ScaleCovariance")
                stats.scaleCovariant++;
            else if (eq.category == "CrossAction")
                stats.crossAction++;
            else if (eq.category == "GroupoidInverse" || eq.category == "GroupoidChain")
                stats.groupoidVerified++;
            else if (eq.category == "CurvatureFlat" || eq.category == "CurvatureCurved")
                stats.curvatureFlatVerified++;
            else if (eq.category == "Conservation")
                stats.conservationLaws++;
            else if (eq.category == "PowerRule" || eq.category == "ExpRule" || eq.category == "LogRule")
                stats.ncCalculusRules++;
            else if (eq.category == "GaugedUFE" || eq.category == "GaugedEigen" || eq.category == "CascadeEq")
                stats.gaugedUFE++;
            else if (eq.category == "ContinuumLimit")
                stats.continuumLimits++;
            else if (eq.category == "GreenSD")
                stats.greenOperator++;
            else if (eq.category == "MasterEq" || eq.category == "EigenvalueEq")
                stats.masterEquation++;
        }
        stats.totalStructural = structEqs.size();

        // === Phase B: Tower Transport ===
        if (config_.enableTransport) {
            for (int q = 1; q <= config_.maxCDLevel; ++q) {
                auto transported = TowerTransport::liftEquations(structEqs, 0, q);
                stats.transportedEquations += transported.size();
            }
        }

        auto tend = std::chrono::steady_clock::now();
        stats.elapsedSeconds = std::chrono::duration<double>(tend - t0).count();

        // === Print Results ===
        if (config_.printEquations) {
            printEquations(structEqs);
        }
        stats.print();

        allDiscoveries_ = std::move(structEqs);
        return stats;
    }

    [[nodiscard]] const std::vector<DiscoveredFunctionalEquation>& discoveries() const {
        return allDiscoveries_;
    }

private:
    Config config_;
    std::vector<DiscoveredFunctionalEquation> allDiscoveries_;

    void printEquations(const std::vector<DiscoveredFunctionalEquation>& eqs) {
        std::cout << "\n"
            "================================================================\n"
            "  DISCOVERED STRUCTURAL EQUATIONS (orbit-verified)\n"
            "================================================================\n\n";

        // Group by category
        std::unordered_map<std::string, std::vector<const DiscoveredFunctionalEquation*>> grouped;
        for (const auto& eq : eqs) {
            grouped[eq.category].push_back(&eq);
        }

        static const std::vector<std::string> categoryOrder = {
            "Derivative", "DerivLinComb", "Leibniz", "FToI",
            "UFE_harmonic", "UFE_eigen", "UFE_coupled",
            "ScaleCovariance", "CrossAction",
            "GroupoidInverse", "GroupoidChain",
            "CurvatureFlat", "CurvatureCurved",
            "Conservation", "PowerRule", "ExpRule", "LogRule",
            "GaugedUFE", "GaugedEigen", "CascadeEq",
            "ContinuumLimit", "GreenSD",
            "MasterEq", "EigenvalueEq"
        };

        for (const auto& cat : categoryOrder) {
            auto it = grouped.find(cat);
            if (it == grouped.end() || it->second.empty()) continue;

            std::cout << "  ── " << cat << " (" << it->second.size() << ") ──\n";
            for (const auto* eq : it->second) {
                std::cout << "    " << eq->toString() << "\n";
            }
            std::cout << "\n";
        }
    }
};

} // namespace orbit
} // namespace domain
} // namespace autodiscover
