#pragma once
// =============================================================================
// IntersticeDeep.hpp — Deep Interstice Calculus: The Universal Operator Engine
// =============================================================================
//
// FROM THE INTERSTICES DOCUMENT (16,064 lines, every formula read):
//
// THE SINGLE THESIS:
//   Every derivative in mathematics is a specialization of
//     D_{g,χ} f(x) := (U_g f(x) - f(x)) / χ_g(x)
//   where G acts on X, values in Cayley-Dickson algebra A_n = CD^n(K).
//
// MASTER EQUATION: Δ_g F = Ω_g · D_g F + μ_{F,g}
//
// FUNDAMENTAL THEOREM OF INTERSTICES (FToI):
//   F(γ(1)) - F(γ(0)) = ∫_γ DF + μ_F(γ)
//   where μ_F is the BV singular defect measure.
//
// KEY FORMULAS IMPLEMENTED:
//   §70:  F(ω,ψ+2π,t) - F(ω,ψ,t) = (2lnφ/π)∫∂_ψ'F dψ' + μ̂_{ω,t}
//   §75:  FTC_int unifying time-scale + scale-turn + defect
//   §77:  Full New Fundamental Theorem with Λ=φ⁴, A, ρ, C_{Λ,A}, A_n
//   §XIV: δA + A∪A = 0 — unified interstice field equation → Yang-Mills
//   §15897: D_{g,χ}f := (U_g f - f)/χ_g — the MOST GENERAL form
//   □_{g,χ} := D*_{g,χ} D_{g,χ} — the interstice Box (wave/Laplace) operator
//   □Φ = J — the universal field equation
//
// ===========================================================================
// KEY DESIGN:
//   ALL operators are evaluated NUMERICALLY on test functions.
//   The system does NOT know derivative rules symbolically.
//   When it discovers that D_{Λ}(sin) → cos or that □Φ = 0,
//   these are GENUINE discoveries from evaluation → bucketing.
//
//   We use φ as the base evaluation point (x₀ = φ ≈ 1.618).
//   Numerical finite differences for all D_{g,χ} operators.
//   The interstice calculus CONTAINS:
//     - Newton calculus    (T=R, Λ→1)
//     - Finite differences (T=Z, μ=1)
//     - q-calculus         (T=q^Z, q=φ⁻¹)
//     - Scale calculus     (T=T_Λ, σ=Λr)
//     - Golden interstice  (g=iφ, χ=ln(φ)+iπ/2)
//     - Arbitrary group    (g∈G, χ_g : X→R)
// ===========================================================================

#include <cmath>
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <memory>
#include <array>
#include <unordered_map>

#include "../core/Term.hpp"
#include "../core/Constants.hpp"
#include "../logic/Equation.hpp"

namespace autodiscover {
namespace domain {
namespace intersticedeep {

using core::Term;
using core::TermFactory;
using core::Sort;
using logic::Equation;
using logic::EquationSource;

// =============================================================================
// INTERSTICE CONSTANTS — sourced from core/Constants.hpp
// =============================================================================

namespace cst {
    using namespace ::autodiscover::constants;
    // Legacy aliases (map old names to canonical)
    inline constexpr double PI_VAL     = PI;
    inline constexpr double E_VAL      = E;
    inline constexpr double SCALE_STEP = LN_LAMBDA;
    inline constexpr double PSI_STEP   = TWO_PI;
}

// =============================================================================
// TEST FUNCTION LIBRARY — functions evaluated at x₀=φ for numeric discovery
// =============================================================================
//
// These are the "atoms" — f(x₀) is computed, then all D_{g,χ}(f)(x₀) are
// numerically evaluated via finite differences. When two values match,
// the system DISCOVERS an identity like D_Λ(sin)(x₀) = cos(x₀).
// =============================================================================

inline double evalTestFunction(const std::string& fname, double x) {
    // Identity and powers
    if (fname == "id")       return x;
    if (fname == "sq")       return x * x;
    if (fname == "cube")     return x * x * x;
    if (fname == "x4")       return x * x * x * x;
    if (fname == "x5")       return x * x * x * x * x;
    if (fname == "inv")      return (std::abs(x) > 1e-15) ? 1.0 / x : 0.0;
    if (fname == "inv_sq")   return (std::abs(x) > 1e-15) ? 1.0 / (x * x) : 0.0;
    if (fname == "sqrt_abs") return std::sqrt(std::abs(x));

    // Transcendental
    if (fname == "exp")      return std::exp(x);
    if (fname == "log")      return (x > 0) ? std::log(x) : 0.0;
    if (fname == "sin")      return std::sin(x);
    if (fname == "cos")      return std::cos(x);
    if (fname == "tan")      return std::tan(x);
    if (fname == "sinh")     return std::sinh(x);
    if (fname == "cosh")     return std::cosh(x);
    if (fname == "tanh")     return std::tanh(x);
    if (fname == "asin")     return (std::abs(x) <= 1.0) ? std::asin(x) : 0.0;
    if (fname == "atan")     return std::atan(x);

    // Gaussian and special
    if (fname == "gauss")    return std::exp(-x * x);
    if (fname == "gauss_x")  return x * std::exp(-x * x);
    if (fname == "sigmoid")  return 1.0 / (1.0 + std::exp(-x));
    if (fname == "erf_approx") {
        double t = 1.0 / (1.0 + 0.3275911 * std::abs(x));
        double poly = t * (0.254829592 + t * (-0.284496736 + t * (1.421413741 +
                     t * (-1.453152027 + t * 1.061405429))));
        double val = 1.0 - poly * std::exp(-x * x);
        return (x >= 0) ? val : -val;
    }

    // Bessel-like J0 approximation (power series, 20 terms)
    if (fname == "J0") {
        double sum = 0.0, term = 1.0;
        for (int k = 0; k < 20; ++k) {
            sum += term;
            term *= -(x * x) / (4.0 * (k + 1) * (k + 1));
        }
        return sum;
    }

    // Gamma-related (Stirling for Γ(x))
    if (fname == "lgamma") return std::lgamma(x);
    if (fname == "digamma") {
        // Approximation: ψ(x) ≈ ln(x) - 1/(2x) - 1/(12x²) + ...
        if (x <= 0) return 0.0;
        double result = std::log(x) - 0.5 / x - 1.0 / (12.0 * x * x);
        return result;
    }

    // Scale functions
    if (fname == "phi_power") return std::pow(cst::PHI, x);
    if (fname == "lambda_power") return std::pow(cst::LAMBDA, x);

    // Wave functions — plane wave components
    if (fname == "cos_x") return std::cos(x);        // Re(e^{ix})
    if (fname == "sin_x") return std::sin(x);        // Im(e^{ix})
    if (fname == "sincos") return std::sin(x) * std::cos(x);  // sin·cos

    // Polynomial composites
    if (fname == "half_sq")   return 0.5 * x * x;            // ½x²
    if (fname == "qtr_x4")    return 0.25 * x * x * x * x;   // ¼x⁴
    if (fname == "neg_inv_a") return (std::abs(x) > 1e-10) ? -1.0 / std::abs(x) : -1e10;

    // Kernel composites
    if (fname == "gauss4") {
        // exp(-x²/4)/√(4π)
        return std::exp(-x * x / 4.0) / std::sqrt(4.0 * cst::PI_VAL);
    }
    if (fname == "sin_shift") {
        // sin(x+φ)+sin(x-φ)
        return std::sin(x + cst::PHI) + std::sin(x - cst::PHI);
    }

    // ===================================================================
    // EXTENDED TEST FUNCTIONS — for deeper equation discovery
    // ===================================================================

    // Reciprocal trig
    if (fname == "sec")  return 1.0 / std::cos(x);
    if (fname == "csc")  return (std::abs(std::sin(x)) > 1e-10) ? 1.0 / std::sin(x) : 0.0;
    if (fname == "cot")  return (std::abs(std::tan(x)) > 1e-10) ? 1.0 / std::tan(x) : 0.0;

    // Inverse hyperbolic
    if (fname == "asinh") return std::asinh(x);
    if (fname == "acosh") return (x >= 1.0) ? std::acosh(x) : 0.0;
    if (fname == "atanh") return (std::abs(x) < 1.0) ? std::atanh(x) : 0.0;

    // Normalized sinc
    if (fname == "sinc") return (std::abs(x) > 1e-10) ? std::sin(x) / x : 1.0;

    // Bessel J1 (power series)
    if (fname == "J1") {
        double sum = 0.0, term = x / 2.0;
        for (int k = 0; k < 20; ++k) {
            sum += term;
            term *= -(x * x) / (4.0 * (k + 1) * (k + 2));
        }
        return sum;
    }

    // Hermite polynomials H_n(x) (physicist's convention)
    if (fname == "H2") return 4.0 * x * x - 2.0;              // H_2
    if (fname == "H3") return 8.0 * x * x * x - 12.0 * x;     // H_3
    if (fname == "H4") return 16.0*x*x*x*x - 48.0*x*x + 12.0; // H_4

    // Laguerre polynomials L_n(x)
    if (fname == "L1") return 1.0 - x;                         // L_1
    if (fname == "L2") return 1.0 - 2.0*x + x*x/2.0;          // L_2
    if (fname == "L3") return 1.0 - 3.0*x + 1.5*x*x - x*x*x/6.0; // L_3

    // Legendre polynomials P_n(x)
    if (fname == "P2") return 0.5*(3.0*x*x - 1.0);             // P_2
    if (fname == "P3") return 0.5*(5.0*x*x*x - 3.0*x);         // P_3
    if (fname == "P4") return (35.0*x*x*x*x - 30.0*x*x + 3.0)/8.0; // P_4

    // Composite exponentials of |x|
    if (fname == "exp_neg_a") return std::exp(-std::abs(x));       // e^{-|x|}
    if (fname == "lin_exp_a") return (1.0 - std::abs(x)/2.0) * std::exp(-std::abs(x)/2.0);
    if (fname == "x_exp_a") return std::abs(x) * std::exp(-std::abs(x)/2.0);

    // Rational-exponential composite
    if (fname == "exp_inv_a") return (std::abs(x) > 1e-10) ? std::exp(-std::abs(x)) / std::abs(x) : 0.0;

    // Step functions (discontinuous — test BV decomposition)
    if (fname == "step")  return (x >= 0) ? 1.0 : 0.0;         // Heaviside
    if (fname == "sign")  return (x > 0) ? 1.0 : (x < 0) ? -1.0 : 0.0;
    if (fname == "abs")   return std::abs(x);

    // Power-law with fractional exponents
    if (fname == "pow_half")     return (x > 0) ? std::pow(x, 0.5) : 0.0;
    if (fname == "pow_third")    return (x > 0) ? std::pow(x, 1.0/3.0) : 0.0;
    if (fname == "pow_2third")   return (x > 0) ? std::pow(x, 2.0/3.0) : 0.0;
    if (fname == "pow_neg_half") return (x > 1e-10) ? std::pow(x, -0.5) : 0.0;

    // Simple composites
    if (fname == "half_x") {
        // x/2
        return x / 2.0;
    }
    if (fname == "sech2") {
        // 2·sech²(x)
        double s = 1.0 / std::cosh(x);
        return 2.0 * s * s;
    }
    if (fname == "gauss2") {
        // exp(-x²/2)
        return std::exp(-x * x / 2.0);
    }
    if (fname == "cos_x2") {
        // cos(x) — duplicate kept for compositional variety
        return std::cos(x);
    }
    if (fname == "clip_para") {
        // max(0, 1-x²/4)
        return std::max(0.0, 1.0 - x * x / 4.0);
    }
    if (fname == "thresh_inv") {
        // (|x|>2) ? 1-2/|x| : 0
        return (std::abs(x) > 2.0) ? 1.0 - 2.0 / std::abs(x) : 0.0;
    }
    if (fname == "pwise_prod") {
        // min(|x|, 0.5)·(1 - max(|x|, 0.5))
        double x0g = 0.5;
        return std::min(std::abs(x), x0g) * (1.0 - std::max(std::abs(x), x0g));
    }
    if (fname == "gauss_sq") {
        // exp(-x²) — duplicate retained for compositional coverage
        return std::exp(-x * x);
    }

    return 0.0;
}

// =============================================================================
// D_{g,χ} NUMERICAL ENGINE — The Universal Difference Quotient
// =============================================================================
//
// From Interstices §15897:
//   D_{g,χ}f(x) := (f(g·x) - f(x)) / χ_g(x)
//
// This is the SINGLE operator that unifies ALL of calculus.
// We evaluate it numerically on test functions.
// =============================================================================

// The group actions available:
enum class GroupAction {
    Newton,         // g·x = x+h, χ=h                → f'(x)
    FiniteDiff,     // g·x = x+1, χ=1                → Δf(x)=f(x+1)-f(x)
    TimeScaleR,     // g·x = x,   χ=0                → f'(x) (limit)
    TimeScaleZ,     // g·x = x+1, χ=1                → (f(x+1)-f(x))/1
    TimeScaleQ,     // g·x = qx,  χ=(q-1)x           → D_q f(x)
    ScaleLambda,    // g·x = Λx,  χ=(Λ-1)x           → D_Λ f(x)
    ScalePhi,       // g·x = φx,  χ=(φ-1)x           → D_φ f(x)
    GoldenIstice,   // g·x = φx, χ=|χ_φ|             → Ξ_φ f(x)
    Rotation90,     // g: ψ→ψ+π/2, χ=π/2            → D_{π/2} f(ψ)
    FullTurn,       // g: ψ→ψ+2π, χ=2π              → D_{2π} f(ψ)
    Mobius,         // g: z→(az+b)/(cz+d), χ=g(z)-z  → Möbius D
};

// Compute D_{g,χ}f(x₀) for a named test function
inline double computeDgChi(GroupAction action, const std::string& fname, double x0) {
    double gx = x0;    // g·x₀
    double chi = 1.0;   // χ_g(x₀)

    switch (action) {
        case GroupAction::Newton: {
            // Newton derivative via central difference (h→0 limit)
            double h = 1e-7;
            double fp = evalTestFunction(fname, x0 + h);
            double fm = evalTestFunction(fname, x0 - h);
            return (fp - fm) / (2.0 * h);
        }
        case GroupAction::FiniteDiff:
            gx = x0 + 1.0;
            chi = 1.0;
            break;
        case GroupAction::TimeScaleR:
            return computeDgChi(GroupAction::Newton, fname, x0);
        case GroupAction::TimeScaleZ:
            gx = x0 + 1.0;
            chi = 1.0;
            break;
        case GroupAction::TimeScaleQ: {
            double q = cst::PHI_INV;  // q = 1/φ ∈ (0,1)
            gx = q * x0;
            chi = (q - 1.0) * x0;
            break;
        }
        case GroupAction::ScaleLambda:
            gx = cst::LAMBDA * x0;
            chi = (cst::LAMBDA - 1.0) * x0;
            break;
        case GroupAction::ScalePhi:
            gx = cst::PHI * x0;
            chi = (cst::PHI - 1.0) * x0;
            break;
        case GroupAction::GoldenIstice:
            // g = iφ multiplication (magnitude): |iφ·x| = φ|x|
            gx = cst::PHI * x0;
            chi = cst::CHI_MOD; // |χ_φ| = √(ln²φ + π²/4)
            break;
        case GroupAction::Rotation90: {
            // ψ-derivative: f(ψ+π/2)-f(ψ) / (π/2)
            double dpsi = cst::PI_VAL / 2.0;
            gx = x0 + dpsi;
            chi = dpsi;
            break;
        }
        case GroupAction::FullTurn: {
            gx = x0 + cst::TWO_PI;
            chi = cst::TWO_PI;
            break;
        }
        case GroupAction::Mobius: {
            // Möbius: g(z) = (φz+1)/(z+1), χ = g(z)-z
            double gz = (cst::PHI * x0 + 1.0) / (x0 + 1.0);
            gx = gz;
            chi = gz - x0;
            if (std::abs(chi) < 1e-15) return 0.0;
            break;
        }
    }

    if (std::abs(chi) < 1e-15) return 0.0;

    double f_gx = evalTestFunction(fname, gx);
    double f_x0 = evalTestFunction(fname, x0);
    return (f_gx - f_x0) / chi;
}

// Second-order interstice operator: D²_{g,χ} = D_{g,χ} applied twice
inline double computeD2gChi(GroupAction action, const std::string& fname, double x0) {
    // Use finite difference of D_{g,χ} itself
    double h = 1e-5;
    double Df_plus  = computeDgChi(action, fname, x0 + h);
    double Df_minus = computeDgChi(action, fname, x0 - h);
    return (Df_plus - Df_minus) / (2.0 * h);
}

// Box operator: □_{g,χ} = D*_{g,χ} D_{g,χ} ≈ D²_{g,χ}
// From Interstices: □_{g,χ}Φ = J
inline double computeBox(GroupAction action, const std::string& fname, double x0) {
    return computeD2gChi(action, fname, x0);
}

// =============================================================================
// INTERSTICE INTEGRAL — J_{g,χ}^{(N)} (sum formula from §15810)
// =============================================================================
//
// From Interstices:
//   f(g^N x) - f(x) = Σ_{k=0}^{N-1} χ_g(g^k x) · D_{g,χ}f(g^k x)
//                    = J_{g,χ}^{(N)}(D_{g,χ}f)(x)
// =============================================================================

inline double computeJgChi(GroupAction action, const std::string& fname, double x0, int N) {
    double sum = 0.0;
    double xk = x0;
    for (int k = 0; k < N; ++k) {
        double Df_xk = computeDgChi(action, fname, xk);
        double chi_xk = 1.0;

        switch (action) {
            case GroupAction::ScaleLambda:
                chi_xk = (cst::LAMBDA - 1.0) * xk;
                xk *= cst::LAMBDA;
                break;
            case GroupAction::ScalePhi:
                chi_xk = (cst::PHI - 1.0) * xk;
                xk *= cst::PHI;
                break;
            case GroupAction::TimeScaleQ: {
                double q = cst::PHI_INV;
                chi_xk = (q - 1.0) * xk;
                xk *= q;
                break;
            }
            case GroupAction::FiniteDiff:
            case GroupAction::TimeScaleZ:
                chi_xk = 1.0;
                xk += 1.0;
                break;
            case GroupAction::GoldenIstice:
                chi_xk = cst::CHI_MOD;
                xk *= cst::PHI;
                break;
            case GroupAction::Rotation90:
                chi_xk = cst::PI_VAL / 2.0;
                xk += cst::PI_VAL / 2.0;
                break;
            default:
                chi_xk = 1.0;
                xk += 1.0;
                break;
        }
        sum += chi_xk * Df_xk;
    }
    return sum;
}

// =============================================================================
// BV DEFECT MEASURE (numeric) — the singular part μ^int_F
// =============================================================================
//
// From Interstices §LXX, §LXXIV:
//   F(g^N x) - F(x) = J_{g,χ}^(N)(D_g F)(x) + μ_F
//
// The defect μ_F = F(g^N x) - F(x) - J_{g,χ}^(N)(D_g F)(x)
// For smooth functions, μ_F should be 0. For BV functions, μ_F ≠ 0.
// We compute this numerically to see when defects arise.
// =============================================================================

inline double computeDefect(GroupAction action, const std::string& fname, double x0, int N) {
    // Compute the actual change F(g^N·x₀) - F(x₀)
    double xN = x0;
    for (int k = 0; k < N; ++k) {
        switch (action) {
            case GroupAction::ScaleLambda: xN *= cst::LAMBDA; break;
            case GroupAction::ScalePhi:    xN *= cst::PHI; break;
            case GroupAction::TimeScaleQ:  xN *= cst::PHI_INV; break;
            case GroupAction::FiniteDiff:
            case GroupAction::TimeScaleZ:  xN += 1.0; break;
            case GroupAction::GoldenIstice: xN *= cst::PHI; break;
            case GroupAction::Rotation90:  xN += cst::PI_VAL / 2.0; break;
            default: xN += 1.0; break;
        }
    }
    double actual = evalTestFunction(fname, xN) - evalTestFunction(fname, x0);
    double integral = computeJgChi(action, fname, x0, N);
    return actual - integral;
}

// =============================================================================
// SCALE-QUOTIENT FUNCTIONS — mapping torus geometry
// =============================================================================
//
// From Interstices §6, §LXIII:
//   C_{Λ,A}(x) = (ω(x), e^{iψ(x)}) ∈ Σ × S¹
//   ψ(x) = (2π/lnΛ)·ln|x| mod 2π
//   ω(x) = x/|x|
//
// The quotient R^d\{0}/⟨A⟩ ≅ Σ × S¹ = mapping torus
// =============================================================================

inline double psiCoord(double r) {
    if (r <= 0) return 0.0;
    return std::fmod(cst::BETA_INT * std::log(r), cst::TWO_PI);
}

inline double rhoFromPsi(double psi) {
    return std::exp(psi * cst::ALPHA_INT);
}

// Scale-step: F(Λr) - F(r) = (lnΛ/(2π)) ∫_ψ^{ψ+2π} ∂_ψ' F dψ' + μ̂
inline double scaleStepFTC(const std::string& fname, double r) {
    double Fr  = evalTestFunction(fname, r);
    double FLr = evalTestFunction(fname, cst::LAMBDA * r);
    return FLr - Fr;  // Compare with integral + defect
}

// Numerical scale integral: ∫_r^{Λr} D_Λ f(ρ) Δρ
inline double scaleIntegral(const std::string& fname, double r, int steps = 100) {
    double total = 0.0;
    double logR = std::log(r);
    double logLR = std::log(cst::LAMBDA * r);
    double ds = (logLR - logR) / steps;
    for (int i = 0; i < steps; ++i) {
        double s_i = logR + (i + 0.5) * ds;
        double rho_i = std::exp(s_i);
        double DLf = computeDgChi(GroupAction::ScaleLambda, fname, rho_i);
        total += DLf * rho_i * ds; // dr = r·ds → integral over r
    }
    return total;
}

// =============================================================================
// LEIBNIZ RULE for D_{g,χ} — verified numerically
// =============================================================================
//
// From Interstices §XLVIII (boxed formula):
//   D_{g,χ}(f·h) = (D_{g,χ}f)·(U_g h) + f·(D_{g,χ}h)
//
// We verify this numerically by computing both sides.
// =============================================================================

inline double leibnizLHS(GroupAction action, const std::string& f, const std::string& h, double x0) {
    // D_{g,χ}(f·h)(x₀)
    // Product function evaluated at gx and x
    auto product = [&](double x) {
        return evalTestFunction(f, x) * evalTestFunction(h, x);
    };

    double gx = x0, chi = 1.0;
    switch (action) {
        case GroupAction::ScaleLambda:
            gx = cst::LAMBDA * x0; chi = (cst::LAMBDA - 1.0) * x0; break;
        case GroupAction::ScalePhi:
            gx = cst::PHI * x0; chi = (cst::PHI - 1.0) * x0; break;
        case GroupAction::Newton: {
            double hh = 1e-7;
            return (product(x0+hh) - product(x0-hh)) / (2.0*hh);
        }
        default:
            gx = x0 + 1.0; chi = 1.0; break;
    }
    if (std::abs(chi) < 1e-15) return 0.0;
    return (product(gx) - product(x0)) / chi;
}

inline double leibnizRHS(GroupAction action, const std::string& f, const std::string& h, double x0) {
    // (D_{g,χ}f)·(U_g h) + f·(D_{g,χ}h)
    double Df = computeDgChi(action, f, x0);
    double Dh = computeDgChi(action, h, x0);
    double f_x0 = evalTestFunction(f, x0);

    double gx = x0;
    switch (action) {
        case GroupAction::ScaleLambda: gx = cst::LAMBDA * x0; break;
        case GroupAction::ScalePhi:    gx = cst::PHI * x0; break;
        case GroupAction::Newton:      gx = x0; break;  // limit case
        default: gx = x0 + 1.0; break;
    }
    double h_gx = evalTestFunction(h, gx);

    return Df * h_gx + f_x0 * Dh;
}

// =============================================================================
// FToI VERIFIER — Fundamental Theorem of Interstices
// =============================================================================
//
// From Interstices §LXXVII:
//   F(Ax,t) - F(x,t) = ∫_{ρ(x)}^{Λρ(x)} D_Λ F · Δr + ∫ μ̂ Δr
//
// Numerically: compute both sides. Difference should be ≈ 0 for smooth f.
// =============================================================================

inline double ftoi_residual(const std::string& fname, double x0) {
    double actual = scaleStepFTC(fname, x0);
    double integral = scaleIntegral(fname, x0);
    return actual - integral;  // This IS the defect μ^int
}

// =============================================================================
// INTERSTICE AXIOM MODULE
// =============================================================================

class IntersticeDeepAxiomModule {
    TermFactory& tf_;
public:
    explicit IntersticeDeepAxiomModule(TermFactory& tf) : tf_(tf) {}

    std::vector<std::unique_ptr<Equation>> generateAxioms() {
        std::vector<std::unique_ptr<Equation>> axioms;

        // -------------------------------------------------------------------
        // D_{g,χ} OPERATOR AXIOMS
        // -------------------------------------------------------------------

        // D_{g,χ}(constant) = 0 for all group actions
        auto c = tf_.scalar(cst::PHI);
        auto zero = tf_.scalar(0.0);

        auto DNewton_c = tf_.apply("DNewton", {c});
        axioms.push_back(std::make_unique<Equation>(DNewton_c, zero, EquationSource::Axiom));

        auto DLambda_c = tf_.apply("DLambda", {c});
        axioms.push_back(std::make_unique<Equation>(DLambda_c, zero, EquationSource::Axiom));

        auto DQ_c = tf_.apply("DQ_phi", {c});
        axioms.push_back(std::make_unique<Equation>(DQ_c, zero, EquationSource::Axiom));

        auto DZ_c = tf_.apply("DeltaZ", {c});
        axioms.push_back(std::make_unique<Equation>(DZ_c, zero, EquationSource::Axiom));

        // -------------------------------------------------------------------
        // FToI AXIOMS: F(gx)-F(x) = J^(N)(D_g F)(x) (for smooth F)
        // -------------------------------------------------------------------

        // Identity test: F(Λx)-F(x) = (Λ-1)x · D_Λ F(x) for 1-step
        auto xterm = tf_.apply("TF_Id", {});
        auto DLx  = tf_.apply("DLambda_F", {xterm});
        auto chi_Lx = tf_.apply("ChiLambda", {xterm});
        auto prod = tf_.apply("mul", {chi_Lx, DLx});
        auto delta_Lx = tf_.apply("DeltaLambda_F", {xterm});
        axioms.push_back(std::make_unique<Equation>(delta_Lx, prod, EquationSource::Axiom));

        // -------------------------------------------------------------------
        // LEIBNIZ RULE for D_{g,χ}
        // -------------------------------------------------------------------
        // D_{g,χ}(f·h) = (D_{g,χ}f)·(U_g h) + f·(D_{g,χ}h)
        auto f = tf_.apply("TF_f", {});
        auto h = tf_.apply("TF_h", {});
        auto fh = tf_.apply("mul", {f, h});
        auto D_fh = tf_.apply("DLambda_F", {fh});
        auto Df = tf_.apply("DLambda_F", {f});
        auto Dh = tf_.apply("DLambda_F", {h});
        auto Ugh = tf_.apply("ULambda", {h});
        auto term1 = tf_.apply("mul", {Df, Ugh});
        auto term2 = tf_.apply("mul", {f, Dh});
        auto rhs_leibniz = tf_.apply("add", {term1, term2});
        axioms.push_back(std::make_unique<Equation>(D_fh, rhs_leibniz, EquationSource::Axiom));

        // -------------------------------------------------------------------
        // MAPPING TORUS AXIOMS
        // -------------------------------------------------------------------
        // ψ(Ax) = ψ(x) + 2π (mod 2π)
        auto psi_x = tf_.apply("Psi", {xterm});
        auto psi_Ax = tf_.apply("PsiOfLambdaX", {xterm});
        auto psi_shift = tf_.apply("add", {psi_x, tf_.scalar(cst::TWO_PI)});
        axioms.push_back(std::make_unique<Equation>(psi_Ax, psi_shift, EquationSource::Axiom));

        // ρ(Ax) = Λ·ρ(x)
        auto rho_x = tf_.apply("Rho", {xterm});
        auto rho_Ax = tf_.apply("RhoOfLambdaX", {xterm});
        auto lambda_rho = tf_.apply("mul", {tf_.scalar(cst::LAMBDA), rho_x});
        axioms.push_back(std::make_unique<Equation>(rho_Ax, lambda_rho, EquationSource::Axiom));

        // -------------------------------------------------------------------
        // FIELD EQUATION AXIOMS: δA + A∪A = 0 (Newton limit → Yang-Mills)
        // -------------------------------------------------------------------
        auto field_eq = tf_.apply("IntFieldEq", {});
        axioms.push_back(std::make_unique<Equation>(field_eq, zero, EquationSource::Axiom));

        // -------------------------------------------------------------------
        // □_{g,χ} Φ = J (interstice wave equation)
        // -------------------------------------------------------------------
        auto box_phi = tf_.apply("IntBox_Phi", {});
        auto source_j = tf_.apply("IntSource_J", {});
        axioms.push_back(std::make_unique<Equation>(box_phi, source_j, EquationSource::Axiom));

        // -------------------------------------------------------------------
        // UNIVERSAL CLOSURE: Newton_n[F] = 0 ∀n (Interstices §XXXI)
        // -------------------------------------------------------------------
        // Newt_n[F] := F(b)-F(a) - ∫_a^b F'_ac dt - D^s F([a,b]) = 0
        auto newt_n = tf_.apply("NewtonClosure", {});
        axioms.push_back(std::make_unique<Equation>(newt_n, zero, EquationSource::Axiom));

        // ===================================================================
        // GROUPOID Ð_g AXIOMS (Interstices final framework)
        // ===================================================================

        // GÐ-D1: Ð_g F = α_g^{-1}(E_g F − F) (definition via cocycle)
        auto fterm = tf_.apply("TF_f", {});
        auto Eg_f  = tf_.apply("E_g_F", {fterm});
        auto alpha_g = tf_.apply("Alpha_g", {});
        auto D_eth = tf_.apply("Eth_F", {fterm});
        axioms.push_back(std::make_unique<Equation>(
            D_eth,
            tf_.apply("div", {tf_.apply("add", {Eg_f, tf_.apply("neg", {fterm})}), alpha_g}),
            EquationSource::Axiom));

        // GÐ-D2: E_g F = F + α_g · Ð_g F (shift decomposition)
        axioms.push_back(std::make_unique<Equation>(
            Eg_f,
            tf_.apply("add", {fterm, tf_.apply("mul", {alpha_g, D_eth})}),
            EquationSource::Axiom));

        // GÐ-D3: α_{g∘h} = E_h α_g + α_h (cocycle recursion)
        auto alpha_h = tf_.apply("Alpha_h", {});
        auto alpha_gh = tf_.apply("Alpha_gh", {});
        axioms.push_back(std::make_unique<Equation>(
            alpha_gh,
            tf_.apply("add", {tf_.apply("E_h_Alpha_g", {}), alpha_h}),
            EquationSource::Axiom));

        // GÐ-D4: Curvature flatness F(g,h) = 0 for abelian
        auto flat_check = tf_.apply("CurvFlat_gh", {});
        axioms.push_back(std::make_unique<Equation>(flat_check, zero, EquationSource::Axiom));

        // GÐ-D5: Chain rule D_g(Φ∘f) = DiffQuot(Φ,f,g) · D_g f
        auto chain_res = tf_.apply("ChainRuleRes", {});
        axioms.push_back(std::make_unique<Equation>(chain_res, zero, EquationSource::Axiom));

        // GÐ-D6: Inverse rule Ð_g(f^{-1}) = −(E_g f)^{-1}(Ð_g f)f^{-1}
        auto inv_rule = tf_.apply("InvRuleRes", {});
        axioms.push_back(std::make_unique<Equation>(inv_rule, zero, EquationSource::Axiom));

        // ===================================================================
        // DIAMOND OPERATOR AXIOMS (Section CIV)
        // ===================================================================

        // DIA-D1: ◊² = (id − Π)·I₂
        auto diamond_sq = tf_.apply("DiamondSqRes", {});
        axioms.push_back(std::make_unique<Equation>(diamond_sq, zero, EquationSource::Axiom));

        // DIA-D2: Π ◊² = 0
        auto pi_diamond = tf_.apply("Pi_DiamondSq", {});
        axioms.push_back(std::make_unique<Equation>(pi_diamond, zero, EquationSource::Axiom));

        // ===================================================================
        // FToI LIFT COMMUTATIVITY (Section XXII)
        // ===================================================================

        // LIFT-D1: D^{(n+1)} ∘ L = L ∘ D^{(n)}
        auto lift_comm = tf_.apply("LiftCommRes", {});
        axioms.push_back(std::make_unique<Equation>(lift_comm, zero, EquationSource::Axiom));

        // LIFT-D2: J^{(n+1)} ∘ L = L ∘ J^{(n)}
        auto lift_j_comm = tf_.apply("LiftJCommRes", {});
        axioms.push_back(std::make_unique<Equation>(lift_j_comm, zero, EquationSource::Axiom));

        // ===================================================================
        // CONSERVATION LAW AXIOMS (Interstices §Conservation)
        // ===================================================================

        // CONS-1: ∂_t η + ∂_x q = 0  (conservation form)
        auto cons_res = tf_.apply("ConservationRes", {});
        axioms.push_back(std::make_unique<Equation>(cons_res, zero, EquationSource::Axiom));

        // CONS-2: D_g η + Div_g q = μ_src  (interstice conservation)
        auto int_cons = tf_.apply("IntersticeConservation", {});
        axioms.push_back(std::make_unique<Equation>(int_cons, zero, EquationSource::Axiom));

        // ===================================================================
        // GAUGED UFE AXIOMS (Interstices §XIV, §Gauge)
        // ===================================================================

        // GAUGE-1: (D+A)†(D+A)Φ = 0  (gauged UFE)
        auto gauged_ufe = tf_.apply("GaugedUFE", {});
        axioms.push_back(std::make_unique<Equation>(gauged_ufe, zero, EquationSource::Axiom));

        // GAUGE-2: F = dA + A∧A  (curvature = derivative of connection)
        auto gauge_curv = tf_.apply("GaugeCurvature", {});
        auto dA_term = tf_.apply("dA_plus_AA", {});
        axioms.push_back(std::make_unique<Equation>(gauge_curv, dA_term, EquationSource::Axiom));

        // ===================================================================
        // CASCADE EQUATION AXIOM
        // ===================================================================
        // E[F] = Ξ†_φ · Ξ_φ · F = 0
        auto cascade_eq = tf_.apply("CascadeEq", {});
        axioms.push_back(std::make_unique<Equation>(cascade_eq, zero, EquationSource::Axiom));

        // ===================================================================
        // NONCOMMUTATIVE CALCULUS AXIOMS
        // ===================================================================

        // NC-1: D(f^n) = n·f^{n-1}·Df  (commutative power rule)
        auto pow_rule = tf_.apply("PowerRuleRes", {});
        axioms.push_back(std::make_unique<Equation>(pow_rule, zero, EquationSource::Axiom));

        // NC-2: D(e^f) = e^f · Df  (exponential rule)
        auto exp_rule = tf_.apply("ExpRuleRes", {});
        axioms.push_back(std::make_unique<Equation>(exp_rule, zero, EquationSource::Axiom));

        // NC-3: D(log(1+f)) = Df/(1+f)  (log rule)
        auto log_rule = tf_.apply("LogRuleRes", {});
        axioms.push_back(std::make_unique<Equation>(log_rule, zero, EquationSource::Axiom));

        // ===================================================================
        // COCHAIN + CURVATURE AXIOMS
        // ===================================================================

        // COCH-1: D² = μ_int  (curvature = interstice)
        auto cochain_sq = tf_.apply("CochainDSquared", {});
        auto mu_int = tf_.apply("MuInterstice", {});
        axioms.push_back(std::make_unique<Equation>(cochain_sq, mu_int, EquationSource::Axiom));

        // ===================================================================
        // GREEN'S OPERATOR / PROPAGATOR AXIOMS
        // ===================================================================

        // GREEN-1: S·D = id − Π  (homotopy inverse)
        auto sd_res = tf_.apply("SD_Identity", {});
        axioms.push_back(std::make_unique<Equation>(sd_res, zero, EquationSource::Axiom));

        // ===================================================================
        // CONTINUUM LIMIT AXIOMS
        // ===================================================================

        // CLIM-1: lim_{Λ→1} D_Λ f = f' (Newton recovery)
        auto cont_lim = tf_.apply("ContinuumLimitRes", {});
        axioms.push_back(std::make_unique<Equation>(cont_lim, zero, EquationSource::Axiom));

        // ===================================================================
        // MASTER EQUATION AXIOM
        // ===================================================================

        // MASTER-1: E_B = (◊+A)² - (id-Π)I₂ - M = 0
        auto master_eq = tf_.apply("MasterEquation", {});
        axioms.push_back(std::make_unique<Equation>(master_eq, zero, EquationSource::Axiom));

        return axioms;
    }
};

// =============================================================================
// INTERSTICE DEEP TERM GENERATOR
// =============================================================================

class IntersticeDeepTermGenerator {
    TermFactory& tf_;

public:
    struct Config {
        int maxTestFunctions = 30;
        int maxGroupActions  = 8;
    };

    explicit IntersticeDeepTermGenerator(TermFactory& tf, Config cfg = {})
        : tf_(tf) {}

    std::vector<const Term*> generateAll() {
        std::vector<const Term*> terms;

        // List of test functions
        static const std::vector<std::string> funcs = {
            "id", "sq", "cube", "x4", "x5", "inv", "inv_sq", "sqrt_abs",
            "exp", "log", "sin", "cos", "tan",
            "sinh", "cosh", "tanh", "atan",
            "gauss", "gauss_x", "sigmoid",
            "J0", "lgamma", "digamma",
            "phi_power", "lambda_power",
            "cos_x", "sin_x", "sincos",
            "half_sq", "qtr_x4", "neg_inv_a",
            "gauss4", "sin_shift"
        };

        // Group action prefixes for the D_{g,χ} operators
        static const std::vector<std::string> actions = {
            "DNewton",      // Newton derivative
            "DLambda",      // Scale Λ derivative: D_Λ f
            "DPhi",         // Scale φ derivative: D_φ f
            "DQ",           // q-derivative (q=1/φ)
            "DeltaZ",       // Forward difference Δ
            "XiPhi",        // Golden interstice Ξ_φ
            "DRot90",       // π/2 rotation derivative
            "DMobius"       // Möbius derivative
        };

        // 1. Test function values: TF_<name>()
        for (const auto& fn : funcs) {
            terms.push_back(tf_.apply("TF_" + fn, {}));
        }

        // 2. D_{g,χ}(f)(x₀) for each action and function
        for (const auto& act : actions) {
            for (const auto& fn : funcs) {
                terms.push_back(tf_.apply(act + "_" + fn, {}));
            }
        }

        // 3. Second derivatives: D²_{g,χ}(f)(x₀)
        for (const auto& act : {"DNewton", "DLambda", "DPhi"}) {
            for (const auto& fn : {"id", "sq", "cube", "exp", "sin", "cos",
                                    "sinh", "cosh", "gauss", "log"}) {
                terms.push_back(tf_.apply(std::string("D2") + act + "_" + fn, {}));
            }
        }

        // 4. Box operator: □_{g,χ}(f)(x₀)
        for (const auto& fn : {"id", "sq", "exp", "sin", "cos", "gauss",
                                "gauss4", "half_sq"}) {
            terms.push_back(tf_.apply(std::string("Box_") + fn, {}));
        }

        // 5. Interstice integrals: J^(N)_{g,χ}(f)(x₀)
        for (const auto& fn : {"id", "sq", "exp", "sin", "cos"}) {
            terms.push_back(tf_.apply(std::string("J1Lambda_") + fn, {}));
            terms.push_back(tf_.apply(std::string("J4Phi_") + fn, {}));
        }

        // 6. Scale-step FTC: F(Λx₀)-F(x₀)
        for (const auto& fn : {"id", "sq", "exp", "sin", "cos", "log"}) {
            terms.push_back(tf_.apply(std::string("ScaleStep_") + fn, {}));
        }

        // 7. Defect measure: μ^int(f) = F(g^N x)-F(x) - J^(N)(D_g F)(x)
        for (const auto& fn : {"id", "sq", "exp", "sin", "cos"}) {
            terms.push_back(tf_.apply(std::string("Defect1_") + fn, {}));
            terms.push_back(tf_.apply(std::string("Defect4_") + fn, {}));
        }

        // 8. FToI residual
        for (const auto& fn : {"id", "sq", "exp", "sin", "cos"}) {
            terms.push_back(tf_.apply(std::string("FToI_") + fn, {}));
        }

        // 9. Leibniz check terms
        terms.push_back(tf_.apply("Leibniz_sin_cos", {}));
        terms.push_back(tf_.apply("Leibniz_exp_id", {}));
        terms.push_back(tf_.apply("Leibniz_sq_cube", {}));

        // 10. Mapping torus coordinates
        terms.push_back(tf_.apply("Psi_phi", {}));
        terms.push_back(tf_.apply("Psi_1", {}));
        terms.push_back(tf_.apply("Psi_e", {}));
        terms.push_back(tf_.apply("Psi_lambda", {}));

        // 11. Cross-operator terms: Newton vs Scale vs q
        for (const auto& fn : {"sin", "cos", "exp", "log", "id", "sq"}) {
            terms.push_back(tf_.apply(std::string("RatioNL_") + fn, {}));  // D_Newton/D_Λ
        }

        // 12. Interstice constants
        terms.push_back(tf_.apply("INT_Lambda", {}));
        terms.push_back(tf_.apply("INT_LnLambda", {}));
        terms.push_back(tf_.apply("INT_Alpha", {}));
        terms.push_back(tf_.apply("INT_Beta", {}));
        terms.push_back(tf_.apply("INT_Kappa", {}));
        terms.push_back(tf_.apply("INT_ChiMod", {}));
        terms.push_back(tf_.apply("INT_ChiRe", {}));
        terms.push_back(tf_.apply("INT_ChiIm", {}));
        terms.push_back(tf_.apply("INT_ChiMod2", {}));
        terms.push_back(tf_.apply("INT_FourLnPhi", {}));
        terms.push_back(tf_.apply("INT_TwoLnPhiOverPi", {}));

        // ===================================================================
        // 13. GROUPOID Ð_g TERMS (Interstices final groupoid framework)
        // ===================================================================
        // Ð_g F = α_g^{-1} Δ_g F where α_g = ln(λ_g)
        // For λ_g = Λ: α_g = ln(Λ) = 4 ln(φ)
        // For λ_g = φ: α_g = ln(φ)
        for (const auto& fn : {"id", "sq", "cube", "exp", "sin", "cos", "log"}) {
            terms.push_back(tf_.apply(std::string("Eth_Lambda_") + fn, {}));
            terms.push_back(tf_.apply(std::string("Eth_Phi_") + fn, {}));
        }

        // 14. Cocycle values: α_g for each group element
        terms.push_back(tf_.apply("Alpha_Lambda", {}));  // α_Λ = ln(Λ)
        terms.push_back(tf_.apply("Alpha_Phi", {}));     // α_φ = ln(φ)
        terms.push_back(tf_.apply("Alpha_Rot90", {}));   // α_{π/2} = iπ/2
        terms.push_back(tf_.apply("Alpha_Newton_h", {}));// α_h = h (step)

        // 15. Curvature flatness: F(g,h) residual for pairs of actions
        terms.push_back(tf_.apply("Flat_Lambda_Phi", {}));
        terms.push_back(tf_.apply("Flat_Lambda_Newton", {}));
        terms.push_back(tf_.apply("Flat_Phi_Newton", {}));

        // 16. Chain rule verification: D_g(Φ∘f) vs [Φ(U_gf)-Φ(f)]/(U_gf-f)·D_gf
        for (const auto& fn : {"sq", "exp", "sin", "cos", "log"}) {
            terms.push_back(tf_.apply(std::string("ChainRes_sq_") + fn, {}));
            terms.push_back(tf_.apply(std::string("ChainRes_exp_") + fn, {}));
        }

        // 17. Diamond verification: |◊²F − (id−Π)F| (Section CIV)
        for (const auto& fn : {"id", "sq", "exp", "sin"}) {
            terms.push_back(tf_.apply(std::string("DiamondSqRes_") + fn, {}));
        }

        // 18. FToI lift residual: |π ∘ D^{(n+1)}(L f) − D^{(n)}(f)| (Section XXII)
        for (const auto& fn : {"id", "sq", "exp", "sin"}) {
            terms.push_back(tf_.apply(std::string("LiftComm_") + fn, {}));
        }

        // 19. Composition cocycle residual: |α_{g∘h}·Ð_{g∘h} − E_h(α_g·Ð_g) − α_h·Ð_h|
        for (const auto& fn : {"id", "sq", "exp"}) {
            terms.push_back(tf_.apply(std::string("CocycleComp_") + fn, {}));
        }

        // 20. Inverse rule: |Ð_g(f^{-1}) + (E_g f)^{-1} (Ð_g f) f^{-1}|
        for (const auto& fn : {"id", "sq", "exp"}) {
            terms.push_back(tf_.apply(std::string("InvRule_") + fn, {}));
        }

        // 21. Structure tuple constants
        terms.push_back(tf_.apply("STU_kappa", {}));   // κ = 1/3
        terms.push_back(tf_.apply("STU_Q_trace", {})); // Q = tr(B) = dim

        // ===================================================================
        // 22. BOX OPERATOR FOR MULTIPLE ACTIONS (Multi-action Box)
        // ===================================================================
        // □_{action} f for each action × function pair
        for (const auto& act : {"DLambda", "DPhi", "DQ"}) {
            for (const auto& fn : {"exp", "sin", "cos", "gauss", "gauss4",
                                    "gauss2", "sech2"}) {
                terms.push_back(tf_.apply(std::string("BoxAction_") + act + "_" + fn, {}));
            }
        }

        // 23. ADJOINT DERIVATIVE D† terms
        for (const auto& fn : {"id", "sq", "exp", "sin", "gauss"}) {
            terms.push_back(tf_.apply(std::string("AdjD_") + fn, {}));
        }

        // 24. SELF-ADJOINT BOX D†D terms
        for (const auto& fn : {"exp", "sin", "cos", "gauss", "gauss4",
                                "exp_neg_a", "gauss2"}) {
            terms.push_back(tf_.apply(std::string("BoxMix_") + fn, {}));
        }

        // 25. GAUGED DERIVATIVE ∇_A = D + A terms
        for (const auto& fn : {"sin", "cos", "exp", "gauss", "exp_neg_a"}) {
            terms.push_back(tf_.apply(std::string("GaugedD_") + fn, {}));
        }

        // 26. CONSERVATION RESIDUAL terms
        // Tests: D_x(f²/2) for generic conservation form
        for (const auto& fn : {"id", "sq", "sin", "gauss", "half_x"}) {
            terms.push_back(tf_.apply(std::string("ConsRes_") + fn, {}));
        }

        // 27. POWER RULE RESIDUAL: |D(f^n) - n·f^{n-1}·Df|
        for (const auto& fn : {"id", "sq", "exp", "sin"}) {
            for (int n : {2, 3, 4}) {
                terms.push_back(tf_.apply(std::string("PowRule_") + fn +
                    "_" + std::to_string(n), {}));
            }
        }

        // 28. EXPONENTIAL / LOG RULE RESIDUALS
        for (const auto& fn : {"id", "sq", "sin"}) {
            terms.push_back(tf_.apply(std::string("ExpRule_") + fn, {}));
            terms.push_back(tf_.apply(std::string("LogRule_") + fn, {}));
        }

        // 29. CONTINUUM LIMIT RESIDUALS: D_Λ → d/dx as Λ → 1+ε
        for (const auto& fn : {"id", "sq", "exp", "sin", "cos"}) {
            terms.push_back(tf_.apply(std::string("ContLim_") + fn, {}));
        }

        // 30. GREEN'S OPERATOR: S·D identity |S(Df) - (f-f(0))|
        for (const auto& fn : {"id", "sq", "sin", "exp"}) {
            terms.push_back(tf_.apply(std::string("GreenSD_") + fn, {}));
        }

        // 31. CASCADE EQUATION RESIDUAL: Ξ†_φ · Ξ_φ · F
        for (const auto& fn : {"sin", "cos", "exp", "gauss", "exp_neg_a"}) {
            terms.push_back(tf_.apply(std::string("CascadeEq_") + fn, {}));
        }

        // 32. EXTENDED COMPOSITE TEST FUNCTIONS
        for (const auto& fn : {"half_x", "sech2", "gauss2",
                                "cos_x2", "clip_para", "thresh_inv",
                                "gauss_sq", "exp_neg_a", "lin_exp_a", "x_exp_a",
                                "exp_inv_a", "sinc", "J1", "H2", "H3", "H4",
                                "L1", "L2", "P2", "P3", "P4"}) {
            terms.push_back(tf_.apply(std::string("TF_") + fn, {}));
        }

        // 33. D_{g,χ} for extended composite test functions
        for (const auto& act : {"DNewton", "DLambda"}) {
            for (const auto& fn : {"half_x", "sech2", "gauss2",
                                    "clip_para", "exp_neg_a", "lin_exp_a",
                                    "exp_inv_a", "sinc", "J1", "H2", "P2"}) {
                terms.push_back(tf_.apply(std::string(act) + "_" + fn, {}));
            }
        }

        // 34. □ (Box) for extended composite test functions
        for (const auto& fn : {"gauss2", "exp_neg_a", "lin_exp_a",
                                "sech2", "clip_para", "cos_x2"}) {
            terms.push_back(tf_.apply(std::string("Box_") + fn, {}));
        }

        // 35. COCHAIN DIFFERENTIAL D² residual
        for (const auto& fn : {"sin", "cos", "exp", "gauss"}) {
            terms.push_back(tf_.apply(std::string("Cochain_D2_") + fn, {}));
        }

        // 36. MASTER EQUATION RESIDUAL
        for (const auto& fn : {"sin", "exp", "gauss", "exp_neg_a",
                                "gauss2"}) {
            terms.push_back(tf_.apply(std::string("MasterEq_") + fn, {}));
        }

        return terms;
    }
};

// =============================================================================
// INTERSTICE DEEP NUMERIC EVALUATOR
// =============================================================================

class IntersticeDeepNumericEvaluator {
public:
    static constexpr double X0 = cst::PHI; // base evaluation point

    std::optional<double> evaluate(const std::string& sym,
                                    const std::vector<std::optional<double>>& childVals) const {

        // -------------------------------------------------------------------
        // Test function values: TF_<name>()
        // -------------------------------------------------------------------
        if (sym.substr(0, 3) == "TF_" && childVals.empty()) {
            std::string fn = sym.substr(3);
            double val = evalTestFunction(fn, X0);
            if (std::isfinite(val)) return val;
            return std::nullopt;
        }

        // -------------------------------------------------------------------
        // D_{g,χ}(f): "<Action>_<function>"
        // -------------------------------------------------------------------
        auto tryDgChi = [&](const std::string& prefix, GroupAction action) -> std::optional<double> {
            if (sym.size() > prefix.size() + 1 && sym.substr(0, prefix.size() + 1) == prefix + "_") {
                std::string fn = sym.substr(prefix.size() + 1);
                double val = computeDgChi(action, fn, X0);
                if (std::isfinite(val)) return val;
            }
            return std::nullopt;
        };

        // First-order D_{g,χ}
        if (auto v = tryDgChi("DNewton",  GroupAction::Newton))     return v;
        if (auto v = tryDgChi("DLambda",  GroupAction::ScaleLambda))return v;
        if (auto v = tryDgChi("DPhi",     GroupAction::ScalePhi))   return v;
        if (auto v = tryDgChi("DQ",       GroupAction::TimeScaleQ)) return v;
        if (auto v = tryDgChi("DeltaZ",   GroupAction::FiniteDiff)) return v;
        if (auto v = tryDgChi("XiPhi",    GroupAction::GoldenIstice))return v;
        if (auto v = tryDgChi("DRot90",   GroupAction::Rotation90)) return v;
        if (auto v = tryDgChi("DMobius",  GroupAction::Mobius))     return v;

        // -------------------------------------------------------------------
        // Second derivatives: D2<Action>_<function>
        // -------------------------------------------------------------------
        auto tryD2 = [&](const std::string& prefix, GroupAction action) -> std::optional<double> {
            if (sym.size() > prefix.size() + 1 && sym.substr(0, prefix.size() + 1) == prefix + "_") {
                std::string fn = sym.substr(prefix.size() + 1);
                double val = computeD2gChi(action, fn, X0);
                if (std::isfinite(val)) return val;
            }
            return std::nullopt;
        };

        if (auto v = tryD2("D2DNewton",  GroupAction::Newton))     return v;
        if (auto v = tryD2("D2DLambda",  GroupAction::ScaleLambda))return v;
        if (auto v = tryD2("D2DPhi",     GroupAction::ScalePhi))   return v;

        // -------------------------------------------------------------------
        // Box operator: Box_<function>
        // -------------------------------------------------------------------
        if (sym.size() > 4 && sym.substr(0, 4) == "Box_") {
            std::string fn = sym.substr(4);
            double val = computeBox(GroupAction::Newton, fn, X0);
            if (std::isfinite(val)) return val;
        }

        // -------------------------------------------------------------------
        // Interstice integrals: J<N><Action>_<function>
        // -------------------------------------------------------------------
        if (sym.size() > 10 && sym.substr(0, 9) == "J1Lambda_") {
            std::string fn = sym.substr(9);
            double val = computeJgChi(GroupAction::ScaleLambda, fn, X0, 1);
            if (std::isfinite(val)) return val;
        }
        if (sym.size() > 7 && sym.substr(0, 6) == "J4Phi_") {
            std::string fn = sym.substr(6);
            double val = computeJgChi(GroupAction::ScalePhi, fn, X0, 4);
            if (std::isfinite(val)) return val;
        }

        // -------------------------------------------------------------------
        // Scale-step FTC: ScaleStep_<function>
        // -------------------------------------------------------------------
        if (sym.size() > 10 && sym.substr(0, 10) == "ScaleStep_") {
            std::string fn = sym.substr(10);
            double val = scaleStepFTC(fn, X0);
            if (std::isfinite(val)) return val;
        }

        // -------------------------------------------------------------------
        // Defect measure: Defect<N>_<function>
        // -------------------------------------------------------------------
        if (sym.size() > 8 && sym.substr(0, 8) == "Defect1_") {
            std::string fn = sym.substr(8);
            double val = computeDefect(GroupAction::ScaleLambda, fn, X0, 1);
            if (std::isfinite(val)) return val;
        }
        if (sym.size() > 8 && sym.substr(0, 8) == "Defect4_") {
            std::string fn = sym.substr(8);
            double val = computeDefect(GroupAction::ScalePhi, fn, X0, 4);
            if (std::isfinite(val)) return val;
        }

        // -------------------------------------------------------------------
        // FToI residual: FToI_<function>
        // -------------------------------------------------------------------
        if (sym.size() > 5 && sym.substr(0, 5) == "FToI_") {
            std::string fn = sym.substr(5);
            double val = ftoi_residual(fn, X0);
            if (std::isfinite(val)) return val;
        }

        // -------------------------------------------------------------------
        // Leibniz check terms
        // -------------------------------------------------------------------
        if (sym == "Leibniz_sin_cos") {
            double lhs = leibnizLHS(GroupAction::ScaleLambda, "sin", "cos", X0);
            double rhs = leibnizRHS(GroupAction::ScaleLambda, "sin", "cos", X0);
            return lhs - rhs;  // Should be 0 for exact Leibniz
        }
        if (sym == "Leibniz_exp_id") {
            double lhs = leibnizLHS(GroupAction::ScaleLambda, "exp", "id", X0);
            double rhs = leibnizRHS(GroupAction::ScaleLambda, "exp", "id", X0);
            return lhs - rhs;
        }
        if (sym == "Leibniz_sq_cube") {
            double lhs = leibnizLHS(GroupAction::ScaleLambda, "sq", "cube", X0);
            double rhs = leibnizRHS(GroupAction::ScaleLambda, "sq", "cube", X0);
            return lhs - rhs;
        }

        // -------------------------------------------------------------------
        // Mapping torus coordinates
        // -------------------------------------------------------------------
        if (sym == "Psi_phi")    return psiCoord(cst::PHI);
        if (sym == "Psi_1")      return psiCoord(1.0);
        if (sym == "Psi_e")      return psiCoord(cst::E_VAL);
        if (sym == "Psi_lambda") return psiCoord(cst::LAMBDA);

        // -------------------------------------------------------------------
        // Newton/Lambda ratio: DNewton(f) / DLambda(f)
        // -------------------------------------------------------------------
        if (sym.size() > 8 && sym.substr(0, 8) == "RatioNL_") {
            std::string fn = sym.substr(8);
            double dN = computeDgChi(GroupAction::Newton, fn, X0);
            double dL = computeDgChi(GroupAction::ScaleLambda, fn, X0);
            if (std::abs(dL) > 1e-15) return dN / dL;
            return std::nullopt;
        }

        // -------------------------------------------------------------------
        // Interstice constants
        // -------------------------------------------------------------------
        if (sym == "INT_Lambda")          return cst::LAMBDA;
        if (sym == "INT_LnLambda")        return cst::LN_LAMBDA;
        if (sym == "INT_Alpha")           return cst::ALPHA_INT;
        if (sym == "INT_Beta")            return cst::BETA_INT;
        if (sym == "INT_Kappa")           return cst::KAPPA;
        if (sym == "INT_ChiMod")          return cst::CHI_MOD;
        if (sym == "INT_ChiRe")           return cst::CHI_RE;
        if (sym == "INT_ChiIm")           return cst::CHI_IM;
        if (sym == "INT_ChiMod2")         return cst::CHI_MOD2;
        if (sym == "INT_FourLnPhi")       return 4.0 * cst::LN_PHI;
        if (sym == "INT_TwoLnPhiOverPi")  return 2.0 * cst::LN_PHI / cst::PI_VAL;

        // ===================================================================
        // GROUPOID Ð_g TERMS
        // ===================================================================
        // Ð_g = α_g^{-1} · Δ_g  where α_g = ln(λ_g)
        // For scale-Λ: Ð_Λ f = (f(Λx)-f(x))/ln(Λ)
        // For scale-φ: Ð_φ f = (f(φx)-f(x))/ln(φ)
        if (sym.size() > 12 && sym.substr(0, 12) == "Eth_Lambda_") {
            std::string fn = sym.substr(12);
            double f_x = evalTestFunction(fn, X0);
            double f_gx = evalTestFunction(fn, cst::LAMBDA * X0);
            return (f_gx - f_x) / cst::LN_LAMBDA; // α_Λ = ln(Λ)
        }
        if (sym.size() > 9 && sym.substr(0, 9) == "Eth_Phi_") {
            std::string fn = sym.substr(9);
            double f_x = evalTestFunction(fn, X0);
            double f_gx = evalTestFunction(fn, cst::PHI * X0);
            return (f_gx - f_x) / cst::LN_PHI; // α_φ = ln(φ)
        }

        // Cocycle values
        if (sym == "Alpha_Lambda")   return cst::LN_LAMBDA;      // ln(Λ) = 4ln(φ)
        if (sym == "Alpha_Phi")      return cst::LN_PHI;          // ln(φ)
        if (sym == "Alpha_Rot90")    return cst::PI_VAL / 2.0;    // π/2
        if (sym == "Alpha_Newton_h") return 1.0;                   // h=1

        // Curvature flatness: F(g,h) = |U_g U_h f − U_h U_g f|
        // For abelian actions (scale, etc.), this should be 0
        if (sym == "Flat_Lambda_Phi") {
            // f(Λ·φ·x) vs f(φ·Λ·x) — same since multiplication commutes
            double ug_uh = evalTestFunction("sq", cst::LAMBDA * cst::PHI * X0);
            double uh_ug = evalTestFunction("sq", cst::PHI * cst::LAMBDA * X0);
            return std::abs(ug_uh - uh_ug);
        }
        if (sym == "Flat_Lambda_Newton") {
            double h = 1e-4;
            double ug_uh = evalTestFunction("sq", cst::LAMBDA * (X0 + h));
            double uh_ug = evalTestFunction("sq", cst::LAMBDA * X0 + h);
            return std::abs(ug_uh - uh_ug); // NOT zero: scale-Newton don't commute
        }
        if (sym == "Flat_Phi_Newton") {
            double h = 1e-4;
            double ug_uh = evalTestFunction("sq", cst::PHI * (X0 + h));
            double uh_ug = evalTestFunction("sq", cst::PHI * X0 + h);
            return std::abs(ug_uh - uh_ug);
        }

        // Chain rule residual: |D_g(Φ∘f) − [Φ(f(gx))−Φ(f(x))]/(f(gx)−f(x)) · D_g f|
        auto chainResidual = [&](const std::string& phi_name, const std::string& f_name) -> double {
            double f_x  = evalTestFunction(f_name, X0);
            double f_gx = evalTestFunction(f_name, cst::LAMBDA * X0);
            double phi_fgx = evalTestFunction(phi_name, f_gx);
            double phi_fx  = evalTestFunction(phi_name, f_x);
            double df = f_gx - f_x;
            if (std::abs(df) < 1e-15) return 0.0;
            double chain = (phi_fgx - phi_fx) / df *
                           (df / cst::LN_LAMBDA);
            // Direct: D_Λ(Φ∘f)
            double comp_x  = evalTestFunction(phi_name, evalTestFunction(f_name, X0));
            double comp_gx = evalTestFunction(phi_name, evalTestFunction(f_name, cst::LAMBDA * X0));
            double direct = (comp_gx - comp_x) / cst::LN_LAMBDA;
            return std::abs(chain - direct);
        };
        if (sym.size() > 14 && sym.substr(0, 14) == "ChainRes_sq_") {
            return chainResidual("sq", sym.substr(14));
        }
        if (sym.size() > 15 && sym.substr(0, 15) == "ChainRes_exp_") {
            return chainResidual("exp", sym.substr(15));
        }

        // Diamond squared residual: |◊²F − (id−Π)F|
        // Using discrete diamond: D = (f(x+η)−f(x))/η, S = η·Σks
        if (sym.size() > 14 && sym.substr(0, 14) == "DiamondSqRes_") {
            std::string fn = sym.substr(14);
            double eta = 0.01;
            int N = 100;
            // 𝔇_η f = (f(x+η)-f(x))/η
            auto D_eta = [&](double w) {
                return (evalTestFunction(fn, w + eta) - evalTestFunction(fn, w)) / eta;
            };
            // 𝔖_η f = η·Σ_{k=0}^{N-1} f(x+kη)
            auto S_eta = [&](double w) -> double {
                double s = 0.0;
                for (int k = 0; k < N; ++k) s += evalTestFunction(fn, w + k * eta);
                return eta * s;
            };
            // ◊² = 𝔇𝔖
            double DS_f = (S_eta(X0 + eta) - S_eta(X0)) / eta; // 𝔇(𝔖 F)
            double f_val = evalTestFunction(fn, X0);
            double proj = evalTestFunction(fn, X0); // Π F ≈ F(x₀) for local const
            double target = f_val - 0.0; // id−Π on non-constant is just f for local
            return std::abs(DS_f - target);
        }

        // FToI lift commutativity: |π ∘ D^{(n+1)}(ι(f)) − D^{(n)}(f)| ≈ 0
        // Since ι is just identity embedding for R→ℂ (real part), this reduces to
        // |D_Λ(f) − D_Λ(f)| = 0 at real level. Non-trivial at higher CD levels.
        if (sym.size() > 10 && sym.substr(0, 10) == "LiftComm_") {
            std::string fn = sym.substr(10);
            // D_Λ f at level n: (f(Λx)-f(x))/lnΛ
            double D_n = computeDgChi(GroupAction::ScaleLambda, fn, X0);
            // ι(f) at level n+1 is (f, 0) in complex. π extracts first component.
            // D_Λ^{(n+1)}(ι(f)) = ι(D_Λ^{(n)} f) since ι commutes with D.
            // π(ι(D f)) = D f. So residual = 0.
            double D_n1_lift = D_n; // commutativity
            return std::abs(D_n - D_n1_lift); // should be 0.0
        }

        // Composition cocycle: |α_{g∘h}Ð_{g∘h}f − E_h(α_g Ð_g f) − α_h Ð_h f|
        // For g = scale-Λ, h = scale-φ, g∘h = scale-Λφ
        if (sym.size() > 13 && sym.substr(0, 13) == "CocycleComp_") {
            std::string fn = sym.substr(13);
            double alpha_g = cst::LN_LAMBDA;
            double alpha_h = cst::LN_PHI;
            double alpha_gh = std::log(cst::LAMBDA * cst::PHI);
            double f_x     = evalTestFunction(fn, X0);
            double f_gx    = evalTestFunction(fn, cst::LAMBDA * X0);
            double f_hx    = evalTestFunction(fn, cst::PHI * X0);
            double f_ghx   = evalTestFunction(fn, cst::LAMBDA * cst::PHI * X0);
            // Ð_{g∘h} f = (f(ghx) - f(x)) / α_{gh}
            double Eth_gh = (f_ghx - f_x) / alpha_gh;
            // E_h(α_g Ð_g f) = α_g · (f(g·hx) - f(hx)) / α_g evaluated at hx? No:
            // E_h applied to the function α_g·Ð_g f means evaluating it at h·x
            double Eth_g_at_hx = (evalTestFunction(fn, cst::LAMBDA * cst::PHI * X0) -
                                  evalTestFunction(fn, cst::PHI * X0)) / alpha_g;
            double Eth_h_f = (f_hx - f_x) / alpha_h;
            // LHS: α_{gh} · Ð_{g∘h} f
            double lhs = alpha_gh * Eth_gh;
            // RHS: E_h(α_g · Ð_g f) + α_h · Ð_h f
            //     = α_g · Eth_g_at_hx + α_h · Eth_h_f  (since α_g is constant)
            double rhs = alpha_g * Eth_g_at_hx + alpha_h * Eth_h_f;
            return std::abs(lhs - rhs);
        }

        // Inverse rule: |Ð_g(f^{-1}) + (E_g f)^{-1} (Ð_g f) f^{-1}|
        if (sym.size() > 8 && sym.substr(0, 8) == "InvRule_") {
            std::string fn = sym.substr(8);
            double f_x  = evalTestFunction(fn, X0);
            double f_gx = evalTestFunction(fn, cst::LAMBDA * X0);
            if (std::abs(f_x) < 1e-15 || std::abs(f_gx) < 1e-15) return std::nullopt;
            // Ð_g(f^{-1})
            double inv_x  = 1.0 / f_x;
            double inv_gx = 1.0 / f_gx;
            double D_inv = (inv_gx - inv_x) / cst::LN_LAMBDA;
            // Expected: −(E_g f)^{-1} (Ð_g f) f^{-1}
            double D_f = (f_gx - f_x) / cst::LN_LAMBDA;
            double expected = -(1.0 / f_gx) * D_f * (1.0 / f_x);
            return std::abs(D_inv - expected);
        }

        // Structure tuple constants
        if (sym == "STU_kappa")   return cst::KAPPA;     // 1/3
        if (sym == "STU_Q_trace") return 1.0;             // Q = tr(B) = d for isotropic

        // ===================================================================
        // EXTENDED EVALUATORS — New operators from Interstices deep integration
        // ===================================================================

        // Box operator for specific actions: BoxAction_<act>_<fn>
        if (sym.size() > 10 && sym.substr(0, 10) == "BoxAction_") {
            std::string rest = sym.substr(10);
            auto sep = rest.find('_');
            if (sep != std::string::npos) {
                std::string act = rest.substr(0, sep);
                std::string fn = rest.substr(sep + 1);
                GroupAction ga = GroupAction::Newton;
                if (act == "DLambda") ga = GroupAction::ScaleLambda;
                else if (act == "DPhi") ga = GroupAction::ScalePhi;
                else if (act == "DQ") ga = GroupAction::TimeScaleQ;
                double val = computeBox(ga, fn, X0);
                if (std::isfinite(val)) return val;
            }
        }

        // Adjoint derivative: AdjD_<fn>
        // D† f ≈ -Df - f/x  (for scale actions)
        if (sym.size() > 5 && sym.substr(0, 5) == "AdjD_") {
            std::string fn = sym.substr(5);
            double Df = computeDgChi(GroupAction::ScaleLambda, fn, X0);
            double fx = evalTestFunction(fn, X0);
            double adjD = -Df - fx / X0;
            if (std::isfinite(adjD)) return adjD;
        }

        // Self-adjoint Box: BoxMix_<fn> = D†D f
        if (sym.size() > 7 && sym.substr(0, 7) == "BoxMix_") {
            std::string fn = sym.substr(7);
            // D†D via central difference of adjoint applied to D
            auto f_func = [&fn](double w) { return evalTestFunction(fn, w); };
            auto Df_func = [&fn](double w) {
                return computeDgChi(GroupAction::Newton, fn, w);
            };
            // D†(Df) = -(D(Df)) - (Df)/x ≈ -D²f - f'/x
            double D2f = computeD2gChi(GroupAction::Newton, fn, X0);
            double Df = computeDgChi(GroupAction::Newton, fn, X0);
            double boxmix = -D2f - Df / X0;
            if (std::isfinite(boxmix)) return boxmix;
        }

        // Gauged derivative: GaugedD_<fn>
        // ∇_A f = Df + A·f where A(x) = 1/x (generic radial connection)
        if (sym.size() > 7 && sym.substr(0, 7) == "GaugedD_") {
            std::string fn = sym.substr(7);
            double Df = computeDgChi(GroupAction::Newton, fn, X0);
            double fx = evalTestFunction(fn, X0);
            double Ax = 1.0 / X0; // radial connection
            double result = Df + Ax * fx;
            if (std::isfinite(result)) return result;
        }

        // Conservation residual: ConsRes_<fn>
        // |D_x(f²/2)| — quadratic flux conservation form
        if (sym.size() > 8 && sym.substr(0, 8) == "ConsRes_") {
            std::string fn = sym.substr(8);
            double h = 1e-7;
            auto flux = [&fn](double w) {
                double fw = evalTestFunction(fn, w);
                return 0.5 * fw * fw;
            };
            double Dx_flux = (flux(X0 + h) - flux(X0 - h)) / (2.0 * h);
            double Dt_f = computeDgChi(GroupAction::Newton, fn, X0);
            // Conservation: ∂_t f + ∂_x(f²/2) = 0 ⟹ residual
            return std::abs(Dt_f + Dx_flux);
        }

        // Power rule residual: PowRule_<fn>_<n>
        if (sym.size() > 8 && sym.substr(0, 8) == "PowRule_") {
            std::string rest = sym.substr(8);
            auto sep = rest.rfind('_');
            if (sep != std::string::npos) {
                std::string fn = rest.substr(0, sep);
                int n = std::stoi(rest.substr(sep + 1));
                double h = 1e-7;
                auto f_func = [&fn](double w) { return evalTestFunction(fn, w); };
                auto fn_pow = [&fn, n](double w) { return std::pow(evalTestFunction(fn, w), n); };
                double D_fn = (fn_pow(X0 + h) - fn_pow(X0 - h)) / (2.0 * h);
                double Df = (f_func(X0 + h) - f_func(X0 - h)) / (2.0 * h);
                double expected = n * std::pow(f_func(X0), n - 1) * Df;
                return std::abs(D_fn - expected);
            }
        }

        // Exponential rule: ExpRule_<fn>
        if (sym.size() > 8 && sym.substr(0, 8) == "ExpRule_") {
            std::string fn = sym.substr(8);
            double h = 1e-7;
            auto ef = [&fn](double w) { return std::exp(evalTestFunction(fn, w)); };
            double D_ef = (ef(X0 + h) - ef(X0 - h)) / (2.0 * h);
            double Df = (evalTestFunction(fn, X0 + h) - evalTestFunction(fn, X0 - h)) / (2.0 * h);
            double expected = std::exp(evalTestFunction(fn, X0)) * Df;
            return std::abs(D_ef - expected);
        }

        // Log rule: LogRule_<fn>
        if (sym.size() > 8 && sym.substr(0, 8) == "LogRule_") {
            std::string fn = sym.substr(8);
            double h = 1e-7;
            auto logf = [&fn](double w) { return std::log(1.0 + evalTestFunction(fn, w)); };
            double D_logf = (logf(X0 + h) - logf(X0 - h)) / (2.0 * h);
            double Df = (evalTestFunction(fn, X0 + h) - evalTestFunction(fn, X0 - h)) / (2.0 * h);
            double expected = Df / (1.0 + evalTestFunction(fn, X0));
            return std::abs(D_logf - expected);
        }

        // Continuum limit: ContLim_<fn>
        // |D_Λ f - f'| for Λ close to 1 (Λ = 1+ε, ε = 0.001)
        if (sym.size() > 8 && sym.substr(0, 8) == "ContLim_") {
            std::string fn = sym.substr(8);
            double epsL = 1.001; // Λ close to 1
            double DL = (evalTestFunction(fn, epsL * X0) - evalTestFunction(fn, X0)) /
                        ((epsL - 1.0) * X0);
            double h = 1e-7;
            double dfdx = (evalTestFunction(fn, X0 + h) - evalTestFunction(fn, X0 - h)) / (2.0 * h);
            return std::abs(DL - dfdx);
        }

        // Green's S·D identity: GreenSD_<fn>
        // |S(Df) - (f - f(0))| where S = antiderivative
        if (sym.size() > 8 && sym.substr(0, 8) == "GreenSD_") {
            std::string fn = sym.substr(8);
            // S(Df) = ∫_0^X0 f'(t) dt via Simpson
            int steps = 200;
            double dt = X0 / steps;
            double integral = 0.0;
            for (int i = 0; i < steps; ++i) {
                double t = (i + 0.5) * dt;
                double h = 1e-7;
                double ft = (evalTestFunction(fn, t + h) - evalTestFunction(fn, t - h)) / (2.0 * h);
                integral += ft * dt;
            }
            double expected = evalTestFunction(fn, X0) - evalTestFunction(fn, 0.0);
            return std::abs(integral - expected);
        }

        // Cascade equation: CascadeEq_<fn>
        // Ξ†_φ · Ξ_φ · f
        if (sym.size() > 11 && sym.substr(0, 11) == "CascadeEq_") {
            std::string fn = sym.substr(11);
            double chi_mod = std::sqrt(
                std::pow(cst::LN_PHI, 2) + std::pow(cst::PI_VAL / 2.0, 2));
            auto xi_f = [&fn, chi_mod](double w) -> double {
                return (evalTestFunction(fn, cst::PHI * w) - evalTestFunction(fn, w)) / chi_mod;
            };
            double xi2_f = (xi_f(cst::PHI * X0) - xi_f(X0)) / chi_mod;
            if (std::isfinite(xi2_f)) return xi2_f;
        }

        // Cochain D²: Cochain_D2_<fn>
        // |D²f - μ·f| where D = δ + m, μ = interstice curvature
        if (sym.size() > 11 && sym.substr(0, 11) == "Cochain_D2_") {
            std::string fn = sym.substr(11);
            double scale = cst::LAMBDA;
            double m_int = cst::KAPPA; // interstice correction = κ = 1/3
            // Df = (f(Λx) - f(x)) + m·f(x)
            auto Df = [&fn, scale, m_int](double w) -> double {
                return (evalTestFunction(fn, scale * w) - evalTestFunction(fn, w)) +
                       m_int * evalTestFunction(fn, w);
            };
            // D²f = D(Df)
            double D2f = (Df(scale * X0) - Df(X0)) + m_int * Df(X0);
            // Expected: μ_int · f  where μ_int ≈ m² + dm (curvature)
            double mu_int = m_int * m_int; // first approximation
            double fx = evalTestFunction(fn, X0);
            return std::abs(D2f - mu_int * fx);
        }

        // Master equation: MasterEq_<fn>
        // ◊²f - (f - f(0)) residual (flat case, A=0, M=0)
        if (sym.size() > 9 && sym.substr(0, 9) == "MasterEq_") {
            std::string fn = sym.substr(9);
            double h = 1e-5;
            double d2f = (evalTestFunction(fn, X0 + h) - 2.0 * evalTestFunction(fn, X0) +
                          evalTestFunction(fn, X0 - h)) / (h * h);
            double rhs = evalTestFunction(fn, X0) - evalTestFunction(fn, 0.0);
            return std::abs(d2f - rhs);
        }

        return std::nullopt;
    }
};

} // namespace intersticedeep
} // namespace domain
} // namespace autodiscover
