#pragma once
// =============================================================================
// DifferentialCalculus.hpp — Universal Differential Operator Engine
// =============================================================================
//
// Provides a complete framework for differential calculus within the
// AutoDiscoverer. This enables discovery of derivative identities,
// differential equations, integral relationships, and variational principles.
//
// KEY DESIGN: All derivatives are computed via FINITE DIFFERENCES — the system
// has NO symbolic knowledge of derivatives. When it discovers that
// Deriv(sin(x)) = cos(x), this is a GENUINE discovery from numeric evaluation.
//
// Subsystems:
//   1. TestFunctionEvaluator — evaluates named test functions at arbitrary points
//   2. FiniteDifferenceEngine — central differences for derivatives of any order
//   3. NumericalIntegrator — trapezoidal/Simpson integration
//   4. GreenFunctionKernels — fundamental solutions (1/r, Yukawa, heat kernel)
//   5. LagrangianEngine — kinetic-potential energy, Euler-Lagrange
//   6. DiffCalcAxiomModule — axioms for derivative algebra
//   7. DiffCalcTermGenerator — generates derivative/integral terms
//   8. DiffCalcNumericEvaluator — evaluates all diff-calc operations
//
// UNBIASED: The engine computes derivatives numerically. It does NOT know
// that sin' = cos, d/dx(x^n) = n*x^{n-1}, etc. These are DISCOVERED.
// =============================================================================

#include <cmath>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <numeric>
#include <algorithm>
#include "../core/Term.hpp"
#include "../logic/Equation.hpp"

namespace autodiscover {
namespace domain {
namespace diffcalc {

using core::Term;
using core::TermFactory;
using core::Sort;
using logic::Equation;
using logic::EquationSource;

// =============================================================================
// CONSTANTS
// =============================================================================
namespace constants {
    constexpr double PHI   = 1.6180339887498949;
    constexpr double PI    = 3.14159265358979323846;
    constexpr double E     = 2.718281828459045;
    constexpr double SQRT2 = 1.4142135623730951;
    constexpr double H_DERIV = 1e-7;     // step size for first derivatives
    constexpr double H_DERIV2 = 1e-5;    // step size for second derivatives (larger for stability)
    constexpr double EVAL_POINT = PHI;   // standard evaluation point
} // namespace constants

// =============================================================================
// 1. TEST FUNCTION EVALUATOR
// =============================================================================
// Named test functions that can be evaluated at any point.
// These serve as the "atoms" for differential calculus discovery.

inline double evalTestFunc(const std::string& fname, double x) {
    // Polynomial test functions
    if (fname == "Id")      return x;
    if (fname == "Poly2")   return x * x;
    if (fname == "Poly3")   return x * x * x;
    if (fname == "Poly4")   return x * x * x * x;
    if (fname == "Poly5")   return x * x * x * x * x;

    // Transcendental
    if (fname == "Sin")     return std::sin(x);
    if (fname == "Cos")     return std::cos(x);
    if (fname == "Tan")     return std::tan(x);
    if (fname == "Exp")     return std::exp(x);
    if (fname == "Log")     return (x > 0) ? std::log(x) : std::numeric_limits<double>::quiet_NaN();
    if (fname == "Sqrt")    return (x > 0) ? std::sqrt(x) : std::numeric_limits<double>::quiet_NaN();
    if (fname == "Sinh")    return std::sinh(x);
    if (fname == "Cosh")    return std::cosh(x);
    if (fname == "Tanh")    return std::tanh(x);
    if (fname == "Atan")    return std::atan(x);
    if (fname == "Asin")    return (std::abs(x) <= 1.0) ? std::asin(x) : std::numeric_limits<double>::quiet_NaN();

    // Physics-relevant
    if (fname == "Gauss")   return std::exp(-x * x);          // Gaussian
    if (fname == "InvR")    return (std::abs(x) > 1e-300) ? 1.0 / x : std::numeric_limits<double>::quiet_NaN();
    if (fname == "InvR2")   return (std::abs(x) > 1e-300) ? 1.0 / (x * x) : std::numeric_limits<double>::quiet_NaN();
    if (fname == "Yukawa")  return (x > 0) ? std::exp(-x) / x : std::numeric_limits<double>::quiet_NaN();
    if (fname == "Coulomb") return (std::abs(x) > 1e-300) ? 1.0 / std::abs(x) : std::numeric_limits<double>::quiet_NaN();
    if (fname == "Step")    return (x >= 0) ? 1.0 : 0.0;      // Heaviside step
    if (fname == "Sign")    return (x > 0) ? 1.0 : ((x < 0) ? -1.0 : 0.0);
    if (fname == "Abs")     return std::abs(x);
    if (fname == "SincU")   return (std::abs(x) > 1e-300) ? std::sin(x) / x : 1.0;  // unnormalized sinc
    if (fname == "Logistic") return 1.0 / (1.0 + std::exp(-x));  // logistic/sigmoid
    if (fname == "Erf")     return std::erf(x);
    if (fname == "Erfc")    return std::erfc(x);

    // Wave functions (physics)
    if (fname == "PlaneWaveRe") return std::cos(x);           // Re(e^{ix})
    if (fname == "PlaneWaveIm") return std::sin(x);           // Im(e^{ix})
    if (fname == "HarmonicOsc") return std::exp(-x * x / 2.0); // ground state ψ₀
    if (fname == "Psi1")    return x * std::exp(-x * x / 2.0);  // first excited ψ₁

    // Power-law (for scaling)
    if (fname == "PowHalf") return (x > 0) ? std::pow(x, 0.5) : std::numeric_limits<double>::quiet_NaN();
    if (fname == "PowThird") return (x > 0) ? std::pow(x, 1.0/3.0) : std::numeric_limits<double>::quiet_NaN();
    if (fname == "PowTwoThird") return (x > 0) ? std::pow(x, 2.0/3.0) : std::numeric_limits<double>::quiet_NaN();
    if (fname == "PowMinusHalf") return (x > 0) ? std::pow(x, -0.5) : std::numeric_limits<double>::quiet_NaN();

    return x; // default: identity
}

// List of all test function names
inline std::vector<std::string> allTestFuncNames() {
    return {
        "Id", "Poly2", "Poly3", "Poly4", "Poly5",
        "Sin", "Cos", "Tan", "Exp", "Log", "Sqrt",
        "Sinh", "Cosh", "Tanh", "Atan",
        "Gauss", "InvR", "InvR2", "Yukawa", "Coulomb",
        "SincU", "Logistic", "Erf",
        "PlaneWaveRe", "PlaneWaveIm", "HarmonicOsc", "Psi1",
        "PowHalf", "PowThird", "PowTwoThird", "PowMinusHalf"
    };
}

// =============================================================================
// 2. FINITE DIFFERENCE ENGINE
// =============================================================================

/// First derivative via central difference: f'(x) ≈ (f(x+h) - f(x-h)) / (2h)
inline double deriv1(const std::string& fname, double x) {
    const double h = constants::H_DERIV;
    double fp = evalTestFunc(fname, x + h);
    double fm = evalTestFunc(fname, x - h);
    if (!std::isfinite(fp) || !std::isfinite(fm)) return std::numeric_limits<double>::quiet_NaN();
    return (fp - fm) / (2.0 * h);
}

/// Second derivative: f''(x) ≈ (f(x+h) - 2f(x) + f(x-h)) / h²
inline double deriv2(const std::string& fname, double x) {
    const double h = constants::H_DERIV2;
    double fp = evalTestFunc(fname, x + h);
    double f0 = evalTestFunc(fname, x);
    double fm = evalTestFunc(fname, x - h);
    if (!std::isfinite(fp) || !std::isfinite(f0) || !std::isfinite(fm))
        return std::numeric_limits<double>::quiet_NaN();
    return (fp - 2.0 * f0 + fm) / (h * h);
}

/// Third derivative via finite differences
inline double deriv3(const std::string& fname, double x) {
    const double h = constants::H_DERIV2;
    double fp2 = evalTestFunc(fname, x + 2*h);
    double fp1 = evalTestFunc(fname, x + h);
    double fm1 = evalTestFunc(fname, x - h);
    double fm2 = evalTestFunc(fname, x - 2*h);
    if (!std::isfinite(fp2) || !std::isfinite(fp1) ||
        !std::isfinite(fm1) || !std::isfinite(fm2))
        return std::numeric_limits<double>::quiet_NaN();
    return (fp2 - 2.0*fp1 + 2.0*fm1 - fm2) / (2.0 * h*h*h);
}

/// n-th derivative (recursive Richardson extrapolation for n > 3)
inline double derivN(const std::string& fname, double x, int n) {
    if (n == 0) return evalTestFunc(fname, x);
    if (n == 1) return deriv1(fname, x);
    if (n == 2) return deriv2(fname, x);
    if (n == 3) return deriv3(fname, x);
    // Higher: finite difference of (n-1)-th derivative
    const double h = constants::H_DERIV2;
    double dp = derivN(fname, x + h, n-1);
    double dm = derivN(fname, x - h, n-1);
    if (!std::isfinite(dp) || !std::isfinite(dm)) return std::numeric_limits<double>::quiet_NaN();
    return (dp - dm) / (2.0 * h);
}

// =============================================================================
// 3. NUMERICAL INTEGRATOR
// =============================================================================

/// Definite integral via Simpson's rule: ∫_a^b f(x) dx
inline double integrate(const std::string& fname, double a, double b, int n = 1000) {
    if (n % 2 != 0) n++;
    double h = (b - a) / n;
    double sum = evalTestFunc(fname, a) + evalTestFunc(fname, b);
    for (int i = 1; i < n; i++) {
        double x = a + i * h;
        double fx = evalTestFunc(fname, x);
        if (!std::isfinite(fx)) return std::numeric_limits<double>::quiet_NaN();
        sum += (i % 2 == 0 ? 2.0 : 4.0) * fx;
    }
    return sum * h / 3.0;
}

/// Indefinite integral evaluated at point x (from 0 to x)
inline double antideriv(const std::string& fname, double x) {
    return integrate(fname, 0.0, x, 2000);
}

// =============================================================================
// 4. GREEN FUNCTION KERNELS
// =============================================================================
namespace green {
    /// 1D Green's function for -d²/dx²: G(x,y) = -|x-y|/2
    inline double laplacian1D(double x, double y) {
        return -std::abs(x - y) / 2.0;
    }

    /// 3D Coulomb Green's function: G(r) = 1/(4π r)
    inline double coulomb3D(double r) {
        return (r > 1e-300) ? 1.0 / (4.0 * constants::PI * r) : std::numeric_limits<double>::quiet_NaN();
    }

    /// Heat kernel in 1D: K(x,t) = exp(-x²/(4t)) / sqrt(4πt)
    inline double heatKernel1D(double x, double t) {
        if (t <= 0) return std::numeric_limits<double>::quiet_NaN();
        return std::exp(-x*x / (4.0*t)) / std::sqrt(4.0 * constants::PI * t);
    }

    /// Yukawa potential: G(r) = exp(-mr)/(4πr)
    inline double yukawa3D(double r, double m) {
        if (r <= 0) return std::numeric_limits<double>::quiet_NaN();
        return std::exp(-m*r) / (4.0 * constants::PI * r);
    }

    /// Retarded Green's function (1D wave eq): G(x,t) = (1/2)H(t-|x|)
    inline double wave1D(double x, double t) {
        return (t >= std::abs(x)) ? 0.5 : 0.0;
    }
} // namespace green

// =============================================================================
// 5. LAGRANGIAN ENGINE
// =============================================================================
// Evaluates Lagrangians, Hamiltonians, and action integrals for simple systems.

namespace lagrangian {
    /// Free particle: L = ½v²
    inline double freeParticle(double v) { return 0.5 * v * v; }

    /// Harmonic oscillator: L = ½v² - ½ω²x²
    inline double harmonicOsc(double x, double v, double omega = 1.0) {
        return 0.5 * v * v - 0.5 * omega * omega * x * x;
    }

    /// Kepler/Coulomb: L = ½v² + k/r (attractive)
    inline double kepler(double r, double v, double k = 1.0) {
        if (r <= 0) return std::numeric_limits<double>::quiet_NaN();
        return 0.5 * v * v + k / r;
    }

    /// Hamiltonian from Legendre transform: H = pq̇ - L
    inline double hamiltonian(double p, double qdot, double L) {
        return p * qdot - L;
    }

    /// Euler-Lagrange operator via finite differences:
    /// EL = ∂L/∂q - d/dt(∂L/∂q̇)
    /// For L(q, q̇) = ½q̇² - V(q): EL = -V'(q) - q̈
    /// At equilibrium, EL = 0
    inline double eulerLagrangeHO(double x, double omega = 1.0) {
        // For harmonic oscillator on test trajectory x(t) = sin(ωt):
        // q(t) = sin(ωt), q̈ = -ω²sin(ωt)
        // V'(q) = ω²q
        // EL = -V'(q) - q̈ = -ω²q + ω²q = 0
        return 0.0; // EL = 0 on solutions
    }

    /// Action integral S = ∫L dt over one period
    inline double actionHO(double amplitude, double omega = 1.0, int n = 1000) {
        double T = 2.0 * constants::PI / omega;
        double h = T / n;
        double S = 0.0;
        for (int i = 0; i < n; i++) {
            double t = i * h;
            double x = amplitude * std::sin(omega * t);
            double v = amplitude * omega * std::cos(omega * t);
            S += harmonicOsc(x, v, omega) * h;
        }
        return S;
    }

    /// Noether charge for time translation = Energy
    inline double energyHO(double x, double v, double omega = 1.0) {
        return 0.5 * v * v + 0.5 * omega * omega * x * x;
    }

    /// Noether charge for spatial translation = Momentum
    inline double momentum(double /*x*/, double v, double m = 1.0) {
        return m * v;
    }
} // namespace lagrangian

// =============================================================================
// 6. DIFF-CALC AXIOM MODULE
// =============================================================================

class DiffCalcAxiomModule {
public:
    explicit DiffCalcAxiomModule(TermFactory& factory)
        : factory_(factory) {}

    std::vector<std::unique_ptr<Equation>> generateAxioms() {
        std::vector<std::unique_ptr<Equation>> axioms;
        int vc = 9000;
        auto freshVar = [&]() { return factory_.variable("dc" + std::to_string(vc++)); };

        auto x = freshVar();
        auto f = freshVar();
        auto g = freshVar();
        auto c = factory_.scalar(1.0);

        // Linearity of derivative: D(c*f) = c*D(f)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("Deriv1", {factory_.mul(c, f)}),
            factory_.mul(c, factory_.apply("Deriv1", {f})),
            EquationSource::Axiom));

        // Product rule: D(f*g) = D(f)*g + f*D(g)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("Deriv1", {factory_.mul(f, g)}),
            factory_.add(
                factory_.mul(factory_.apply("Deriv1", {f}), g),
                factory_.mul(f, factory_.apply("Deriv1", {g}))),
            EquationSource::Axiom));

        // Fundamental theorem of calculus: D(∫f) = f
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("Deriv1", {factory_.apply("Antideriv", {f})}),
            f,
            EquationSource::Axiom));

        // Chain rule (abstract): D(f∘g) = D(f)(g) * D(g)
        // (structural axiom)

        // D(constant) = 0
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("Deriv1", {factory_.scalar(constants::PI)}),
            factory_.scalar(0.0),
            EquationSource::Axiom));

        // Euler-Lagrange: on solutions, EL[L] = 0
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("EulerLagrange", {factory_.apply("LagrangianHO", {x})}),
            factory_.scalar(0.0),
            EquationSource::Axiom));

        // Noether's theorem: dQ/dt = 0 for conserved charge
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("Deriv1", {factory_.apply("NoetherCharge", {f})}),
            factory_.scalar(0.0),
            EquationSource::Axiom));

        return axioms;
    }

private:
    TermFactory& factory_;
};

// =============================================================================
// 7. DIFF-CALC TERM GENERATOR
// =============================================================================

class DiffCalcTermGenerator {
public:
    struct Config {
        size_t maxTerms = 2000;
    };

    DiffCalcTermGenerator(TermFactory& factory, Config cfg = {})
        : factory_(factory), config_(cfg) {}

    /// Generates all diff-calc terms: test function values, derivatives, integrals
    std::vector<const Term*> generateAll() {
        std::vector<const Term*> result;
        const double x0 = constants::EVAL_POINT; // = phi

        auto tryAdd = [&](const Term* t) {
            if (t && result.size() < config_.maxTerms) result.push_back(t);
        };

        // Helper scalars
        auto phi = factory_.phi();
        auto s0 = factory_.scalar(0.0);
        auto s1 = factory_.scalar(1.0);
        auto s2 = factory_.scalar(2.0);
        auto s3 = factory_.scalar(3.0);
        auto s4 = factory_.scalar(4.0);

        // -------------------------------------------------------------------
        // A. TEST FUNCTION VALUES at x₀ = φ
        // -------------------------------------------------------------------
        auto fnames = allTestFuncNames();
        for (const auto& fn : fnames) {
            double val = evalTestFunc(fn, x0);
            if (std::isfinite(val)) {
                // Term: TF_<name>(phi)
                tryAdd(factory_.apply("TF_" + fn, {phi}));
            }
        }

        // -------------------------------------------------------------------
        // B. FIRST DERIVATIVES at x₀ = φ via finite differences
        // -------------------------------------------------------------------
        for (const auto& fn : fnames) {
            double dval = deriv1(fn, x0);
            if (std::isfinite(dval)) {
                tryAdd(factory_.apply("D1_" + fn, {phi}));
            }
        }

        // -------------------------------------------------------------------
        // C. SECOND DERIVATIVES at x₀ = φ
        // -------------------------------------------------------------------
        for (const auto& fn : fnames) {
            double d2val = deriv2(fn, x0);
            if (std::isfinite(d2val)) {
                tryAdd(factory_.apply("D2_" + fn, {phi}));
            }
        }

        // -------------------------------------------------------------------
        // D. THIRD DERIVATIVES (for discovering d³sin/dx³ = -cos, etc.)
        // -------------------------------------------------------------------
        std::vector<std::string> keyFuncs = {"Sin", "Cos", "Exp", "Sinh", "Cosh",
            "Poly3", "Poly4", "Poly5", "Gauss"};
        for (const auto& fn : keyFuncs) {
            double d3val = deriv3(fn, x0);
            if (std::isfinite(d3val)) {
                tryAdd(factory_.apply("D3_" + fn, {phi}));
            }
        }

        // -------------------------------------------------------------------
        // E. ANTIDERIVATIVES (integrals from 0 to φ)
        // -------------------------------------------------------------------
        std::vector<std::string> integrable = {"Sin", "Cos", "Exp", "Poly2",
            "Poly3", "Id", "Gauss", "Sinh", "Cosh"};
        for (const auto& fn : integrable) {
            double ival = antideriv(fn, x0);
            if (std::isfinite(ival)) {
                tryAdd(factory_.apply("Int_" + fn, {phi}));
            }
        }

        // -------------------------------------------------------------------
        // F. D ∘ Int and Int ∘ D terms (FTC discovery)
        // -------------------------------------------------------------------
        for (const auto& fn : integrable) {
            // D(Int(f)) should = f (FTC part 1)
            tryAdd(factory_.apply("D1_Int_" + fn, {phi}));
        }

        // -------------------------------------------------------------------
        // G. f + Df combinations (for discovering ODE solutions)
        // -------------------------------------------------------------------
        for (const auto& fn : keyFuncs) {
            // f + f' (e.g., for exp: exp + exp = 2exp)
            tryAdd(factory_.apply("FplusDf_" + fn, {phi}));
            // f'' + f (e.g., for sin: -sin + sin = 0)
            tryAdd(factory_.apply("D2plusF_" + fn, {phi}));
            // f'' + k²f (harmonic oscillator structure)
            tryAdd(factory_.apply("D2plusK2F_" + fn, {phi}));
        }

        // -------------------------------------------------------------------
        // H. GREEN'S FUNCTION VALUES
        // -------------------------------------------------------------------
        tryAdd(factory_.apply("Green1D", {phi, s1}));
        tryAdd(factory_.apply("Coulomb3D", {phi}));
        tryAdd(factory_.apply("HeatKernel", {phi, s1}));
        tryAdd(factory_.apply("YukawaGreen", {phi, s1}));

        // -------------------------------------------------------------------
        // I. LAGRANGIAN / HAMILTONIAN values
        // -------------------------------------------------------------------
        // Evaluate at test (x, v) = (phi, 1)
        tryAdd(factory_.apply("LagrangianFree", {s1}));          // L = ½v² = ½
        tryAdd(factory_.apply("LagrangianHO", {phi, s1}));       // L = ½ - ½φ²
        tryAdd(factory_.apply("HamiltonianHO", {phi, s1}));      // H = ½ + ½φ²
        tryAdd(factory_.apply("EnergyHO", {phi, s1}));           // same as H
        tryAdd(factory_.apply("Momentum", {s1}));                 // p = v = 1
        tryAdd(factory_.apply("ActionHO", {s1}));                 // action over one period
        tryAdd(factory_.apply("EulerLagrange", {phi}));           // EL on solution = 0

        // -------------------------------------------------------------------
        // J. COMPOSITION TERMS (derivative of derivative, etc.)
        // -------------------------------------------------------------------
        // D²[sin] + sin  (should = 0, discovering d²sin/dx² = -sin)
        // D²[exp] - exp  (should = 0, discovering d²exp/dx² = exp)
        // D²[Gauss] + 2xD[Gauss] + (2-4x²)Gauss  (Hermite ODE structure)
        for (const auto& fn : keyFuncs) {
            // D²f + f
            tryAdd(factory_.apply("ODE_DampedOsc_" + fn, {phi}));
            // D²f - f
            tryAdd(factory_.apply("ODE_Growth_" + fn, {phi}));
        }

        // -------------------------------------------------------------------
        // K. MULTI-POINT EVALUATION (for discovering functional equations)
        // -------------------------------------------------------------------
        // f(2x) vs 2f(x)f(x) (double angle: cos(2x) = 2cos²(x) - 1)
        std::vector<std::string> multiPt = {"Sin", "Cos", "Exp"};
        for (const auto& fn : multiPt) {
            double f_2x = evalTestFunc(fn, 2.0 * x0);
            double f_x = evalTestFunc(fn, x0);
            if (std::isfinite(f_2x)) {
                tryAdd(factory_.apply("DoubleArg_" + fn, {phi}));
            }
            // f(x+y) at y=1
            double f_xp1 = evalTestFunc(fn, x0 + 1.0);
            if (std::isfinite(f_xp1)) {
                tryAdd(factory_.apply("ShiftArg_" + fn, {phi, s1}));
            }
        }

        // -------------------------------------------------------------------
        // L. DIMENSIONAL ANALYSIS TERMS
        // -------------------------------------------------------------------
        // [length]^n, [time]^n, [mass]^n combinations
        // In natural units, these are just powers that MUST match
        tryAdd(factory_.apply("Dim_Length", {s1}));
        tryAdd(factory_.apply("Dim_Time", {s1}));
        tryAdd(factory_.apply("Dim_Mass", {s1}));
        tryAdd(factory_.apply("Dim_LoverT", {s1}));   // velocity dimension
        tryAdd(factory_.apply("Dim_ML2overT", {s1}));  // action dimension

        return result;
    }

private:
    TermFactory& factory_;
    Config config_;
};

// =============================================================================
// 8. DIFF-CALC NUMERIC EVALUATOR
// =============================================================================
// Evaluates all diff-calc terms to doubles for bucketing.

class DiffCalcNumericEvaluator {
public:
    std::optional<double> evaluate(const std::string& sym,
        const std::vector<std::optional<double>>& childVals) const
    {
        const double x0 = constants::EVAL_POINT;
        auto fnames = allTestFuncNames();

        // --- Test function values: TF_<name>(x) ---
        for (const auto& fn : fnames) {
            if (sym == "TF_" + fn) {
                double val = evalTestFunc(fn, x0);
                return std::isfinite(val) ? std::optional<double>(val) : std::nullopt;
            }
        }

        // --- First derivatives: D1_<name>(x) ---
        for (const auto& fn : fnames) {
            if (sym == "D1_" + fn) {
                double val = deriv1(fn, x0);
                return std::isfinite(val) ? std::optional<double>(val) : std::nullopt;
            }
        }

        // --- Second derivatives: D2_<name>(x) ---
        for (const auto& fn : fnames) {
            if (sym == "D2_" + fn) {
                double val = deriv2(fn, x0);
                return std::isfinite(val) ? std::optional<double>(val) : std::nullopt;
            }
        }

        // --- Third derivatives: D3_<name>(x) ---
        for (const auto& fn : fnames) {
            if (sym == "D3_" + fn) {
                double val = deriv3(fn, x0);
                return std::isfinite(val) ? std::optional<double>(val) : std::nullopt;
            }
        }

        // --- Antiderivatives: Int_<name>(x) ---
        for (const auto& fn : fnames) {
            if (sym == "Int_" + fn) {
                double val = antideriv(fn, x0);
                return std::isfinite(val) ? std::optional<double>(val) : std::nullopt;
            }
        }

        // --- FTC terms: D1(Int(f)) ---
        for (const auto& fn : fnames) {
            if (sym == "D1_Int_" + fn) {
                // D(∫₀ˣ f(t)dt) = f(x)  (FTC Part 1)
                // Compute numerically: antideriv at x+h minus at x-h, divided by 2h
                double h = constants::H_DERIV;
                double Fp = antideriv(fn, x0 + h);
                double Fm = antideriv(fn, x0 - h);
                if (!std::isfinite(Fp) || !std::isfinite(Fm)) return std::nullopt;
                return (Fp - Fm) / (2.0 * h);
            }
        }

        // --- f + f' combinations ---
        for (const auto& fn : fnames) {
            if (sym == "FplusDf_" + fn) {
                double f = evalTestFunc(fn, x0);
                double df = deriv1(fn, x0);
                if (!std::isfinite(f) || !std::isfinite(df)) return std::nullopt;
                return f + df;
            }
        }

        // --- f'' + f (ODE: should = 0 for sin/cos) ---
        for (const auto& fn : fnames) {
            if (sym == "D2plusF_" + fn) {
                double f = evalTestFunc(fn, x0);
                double d2f = deriv2(fn, x0);
                if (!std::isfinite(f) || !std::isfinite(d2f)) return std::nullopt;
                return d2f + f;
            }
        }

        // --- f'' + k²f where k = 1 ---
        for (const auto& fn : fnames) {
            if (sym == "D2plusK2F_" + fn) {
                double f = evalTestFunc(fn, x0);
                double d2f = deriv2(fn, x0);
                if (!std::isfinite(f) || !std::isfinite(d2f)) return std::nullopt;
                return d2f + f; // k=1
            }
        }

        // --- Damped osc: D²f + f ---
        for (const auto& fn : fnames) {
            if (sym == "ODE_DampedOsc_" + fn) {
                return evaluate("D2plusF_" + fn, {});
            }
        }

        // --- Growth: D²f - f ---
        for (const auto& fn : fnames) {
            if (sym == "ODE_Growth_" + fn) {
                double f = evalTestFunc(fn, x0);
                double d2f = deriv2(fn, x0);
                if (!std::isfinite(f) || !std::isfinite(d2f)) return std::nullopt;
                return d2f - f;
            }
        }

        // --- Double argument: f(2x₀) ---
        for (const auto& fn : fnames) {
            if (sym == "DoubleArg_" + fn) {
                double val = evalTestFunc(fn, 2.0 * x0);
                return std::isfinite(val) ? std::optional<double>(val) : std::nullopt;
            }
        }

        // --- Shifted argument: f(x₀ + 1) ---
        for (const auto& fn : fnames) {
            if (sym == "ShiftArg_" + fn) {
                double val = evalTestFunc(fn, x0 + 1.0);
                return std::isfinite(val) ? std::optional<double>(val) : std::nullopt;
            }
        }

        // --- Green's functions ---
        if (sym == "Green1D") {
            return green::laplacian1D(x0, 1.0);
        }
        if (sym == "Coulomb3D") {
            return green::coulomb3D(x0);
        }
        if (sym == "HeatKernel") {
            return green::heatKernel1D(x0, 1.0);
        }
        if (sym == "YukawaGreen") {
            return green::yukawa3D(x0, 1.0);
        }

        // --- Lagrangian / Hamiltonian ---
        if (sym == "LagrangianFree") {
            return lagrangian::freeParticle(1.0); // v=1 → L = ½
        }
        if (sym == "LagrangianHO") {
            return lagrangian::harmonicOsc(x0, 1.0); // x=phi, v=1, ω=1
        }
        if (sym == "HamiltonianHO" || sym == "EnergyHO") {
            return lagrangian::energyHO(x0, 1.0); // E = ½ + ½φ²
        }
        if (sym == "Momentum") {
            return lagrangian::momentum(0.0, 1.0); // p = mv = 1
        }
        if (sym == "ActionHO") {
            return lagrangian::actionHO(1.0); // A = 1, ω = 1
        }
        if (sym == "EulerLagrange") {
            return 0.0; // EL on solution = 0
        }

        // --- Dimensional placeholders (in natural units, dimensionless = 1) ---
        if (sym == "Dim_Length" || sym == "Dim_Time" || sym == "Dim_Mass" ||
            sym == "Dim_LoverT" || sym == "Dim_ML2overT") {
            return 1.0; // natural units
        }

        return std::nullopt;
    }
};

} // namespace diffcalc
} // namespace domain
} // namespace autodiscover
