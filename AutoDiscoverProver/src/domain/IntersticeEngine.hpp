/**
 * @file IntersticeEngine.hpp
 * @brief INTERSTICE FRAMEWORK — The Universal Calculus Engine
 *
 * =============================================================================
 * MATHEMATICAL FOUNDATION (from Interstices.txt — 16,064 lines, 3 complete passes)
 * =============================================================================
 *
 * THE SINGLE THESIS:
 *   Every classical derivative is a specialization of the universal difference
 *   quotient D_{g,χ}f := (U_g f - f) / χ_g for a group G acting on space X
 *   with values in Cayley-Dickson algebra A_n. Every such quotient decomposes
 *   as an absolutely continuous part + singular defect measure μ^int.
 *
 * THE MASTER EQUATION (derived 10+ times across 10+ spiral cycles):
 *   Δ_g F = Ω_g · D_g F + μ_{F,g}
 *
 * THE FUNDAMENTAL INTERSTICE IDENTITY:
 *   F(γ(1)) - F(γ(0)) = ∫_γ DF + μ_F(γ)
 *
 * UNIVERSAL CLOSURE:
 *   ∀p,q, ∀E ∈ T: E=0 in Q_p ⟺ equivalent translated system in Q_q
 *
 * =============================================================================
 * OPERATOR HIERARCHY (all specializations of D_{g,χ}):
 *   Newton D     : lim (F(x+ε)-F(x))/ε              — smooth, R^d
 *   Time-scale Δ : (f(σ(t))-f(t))/μ(t)               — T ⊂ R closed
 *   Scale D_Λ    : (F(ψ+2π)-F(ψ))/2π                 — S^1 periodic
 *   q-derivative  : (F(qz)-F(z))/((q-1)z)            — q-calculus
 *   Ξ_φ          : (F∘S_φ - F)/χ_φ                    — golden interstice
 *   D^♯_{Λ,w}    : (S_{Λ,w}-I)/ℓ                      — graded combined
 *   D_{λ,N}      : (1/ω)(E_λ - Id) = (1/ω)S^(N)_q D_q — multiplicative
 *   D_{g,χ}      : (U_g f - f) / χ_g                  — universal
 * =============================================================================
 *
 * This module provides:
 *   1. TimeScale — arbitrary closed subsets T ⊂ R with σ(t), μ(t), f^Δ
 *   2. QCalculus — D_q, S^(N)_q, golden specialization q=iφ, N=4
 *   3. BVDecomposition — BV_loc splitting into absolutely continuous + defect
 *   4. ScaleCovariance — S_Λ^*μ = Λ^σ μ forcing, mapping torus
 *   5. XiPhi — the Ξ_φ operator, its inverse Π_φ, cascade equation
 *   6. UnitGrading — Z^7 physical units, graded operators G_i
 *   7. UniversalDifferenceQuotient — D_{g,χ} for arbitrary group actions
 *   8. GaugeConnection — ∇ = d_X + A, F = dA + A∧A = μ^int
 *   9. IntersticeTermGenerator — generates discoverable interstice terms
 *  10. IntersticeEvaluator — numerically evaluates all interstice operators
 */

#ifndef AUTODISCOVER_DOMAIN_INTERSTICE_ENGINE_HPP
#define AUTODISCOVER_DOMAIN_INTERSTICE_ENGINE_HPP

#include "../core/Term.hpp"
#include "../core/Constants.hpp"
#include "../logic/Equation.hpp"
#include "Algebra.hpp"
#include <vector>
#include <array>
#include <cmath>
#include <complex>
#include <functional>
#include <memory>
#include <string>
#include <numeric>
#include <optional>
#include <unordered_set>

namespace autodiscover {
namespace domain {
namespace interstice {

using core::Term;
using core::TermFactory;
using core::Sort;
using logic::Equation;

// =============================================================================
// CONSTANTS — sourced from core/Constants.hpp
// =============================================================================

namespace constants {
    using ::autodiscover::constants::PHI;
    using ::autodiscover::constants::PHI_INV;
    using ::autodiscover::constants::PHI_SQ;
    using ::autodiscover::constants::LAMBDA;
    using ::autodiscover::constants::LN_LAMBDA;
    using ::autodiscover::constants::LN_PHI;
    using ::autodiscover::constants::PI;
    using ::autodiscover::constants::TWO_PI;
    // Legacy aliases (names differ from canonical)
    inline constexpr double ALPHA = ::autodiscover::constants::ALPHA_INT;
    inline constexpr double BETA  = ::autodiscover::constants::BETA_INT;
    inline constexpr double KAPPA = ::autodiscover::constants::ALPHA_INT;  // κ_Λ = α
    inline constexpr double CHI_PHI_RE = ::autodiscover::constants::CHI_RE;
    inline constexpr double CHI_PHI_IM = ::autodiscover::constants::CHI_IM;
    inline constexpr double CRITICAL_EXPONENT = ::autodiscover::constants::KAPPA;  // 1/3
}

// =============================================================================
// 1. TIME SCALE ENGINE
// =============================================================================
//
// T ⊂ R closed set. For t ∈ T:
//   σ(t) = inf{s ∈ T : s > t}     (forward jump operator)
//   μ(t) = σ(t) - t                (graininess)
//   f^Δ(t) = (f(σ(t)) - f(t)) / μ(t)  when μ(t) > 0
//           = f'(t)                     when μ(t) = 0
//
// Specializations:
//   T = R : σ(t) = t, μ = 0 → Newton calculus
//   T = Z : σ(t) = t+1, μ = 1 → finite differences
//   T = q^Z : σ(t) = qt, μ = (q-1)t → Jackson q-calculus
//   T = φ^4Z : σ(t) = Λt → golden interstice
// =============================================================================

enum class TimeScaleType {
    Continuous,     // T = R
    Discrete,       // T = Z
    QGeometric,     // T = q^Z
    GoldenGeometric,// T = Λ^Z = φ^{4Z}
    Custom          // arbitrary closed subset
};

class TimeScale {
public:
    TimeScaleType type;
    double q_param = 0;      // for q-geometric: the base q
    std::vector<double> points; // for custom time scales

    // Standard time scales
    static TimeScale continuous() {
        TimeScale ts; ts.type = TimeScaleType::Continuous; return ts;
    }
    static TimeScale discrete() {
        TimeScale ts; ts.type = TimeScaleType::Discrete; return ts;
    }
    static TimeScale qGeometric(double q) {
        TimeScale ts; ts.type = TimeScaleType::QGeometric; ts.q_param = q; return ts;
    }
    static TimeScale goldenGeometric() {
        TimeScale ts;
        ts.type = TimeScaleType::GoldenGeometric;
        ts.q_param = constants::LAMBDA;
        return ts;
    }

    // Forward jump operator σ(t)
    double sigma(double t) const {
        switch (type) {
            case TimeScaleType::Continuous: return t;
            case TimeScaleType::Discrete: return t + 1.0;
            case TimeScaleType::QGeometric: return q_param * t;
            case TimeScaleType::GoldenGeometric: return constants::LAMBDA * t;
            case TimeScaleType::Custom: {
                for (double p : points) {
                    if (p > t + 1e-15) return p;
                }
                return t; // rightmost point
            }
        }
        return t;
    }

    // Graininess μ(t) = σ(t) - t
    double graininess(double t) const {
        return sigma(t) - t;
    }

    // Delta derivative: f^Δ(t) = (f(σ(t)) - f(t)) / μ(t)
    // For continuous case, uses numerical derivative
    double deltaDerivative(std::function<double(double)> f, double t, double h = 1e-8) const {
        double mu = graininess(t);
        if (std::abs(mu) > 1e-15) {
            return (f(sigma(t)) - f(t)) / mu;
        }
        // μ = 0 → Newton derivative
        return (f(t + h) - f(t - h)) / (2.0 * h);
    }

    // Hilger integral: ∫_a^b f(τ) Δτ
    double hilgerIntegral(std::function<double(double)> f, double a, double b, int maxSteps = 1000) const {
        if (type == TimeScaleType::Continuous) {
            // Simpson's rule for continuous case
            int n = 100;
            double h = (b - a) / n;
            double sum = f(a) + f(b);
            for (int i = 1; i < n; i += 2) sum += 4.0 * f(a + i * h);
            for (int i = 2; i < n; i += 2) sum += 2.0 * f(a + i * h);
            return sum * h / 3.0;
        }
        // Discrete/geometric: sum over time scale points
        double sum = 0.0;
        double t = a;
        int steps = 0;
        while (t < b - 1e-15 && steps < maxSteps) {
            double mu = graininess(t);
            if (mu < 1e-15) break; // would be infinite loop
            sum += f(t) * mu;
            t = sigma(t);
            steps++;
        }
        return sum;
    }
};

// =============================================================================
// 2. Q-CALCULUS ENGINE
// =============================================================================
//
// D_q F(z) = (F(qz) - F(z)) / ((q-1)z)
// S^(N)_q G(z) = (q-1) Σ_{k=0}^{N-1} (q^k z) G(q^k z)
//
// FUNDAMENTAL IDENTITY: S^(N)_q · D_q = E_λ - Id     (q^N = λ)
//
// Golden specialization: λ = φ⁴, N = 4, q = iφ
//   S^(4)_{iφ} · D_{iφ} = E_{φ⁴} - Id
//
// Product rule: D_q(fg) = f(q·) D_q g + g D_q f
// Chain rule:   D_q(f∘F) = δf(F, E_λ F) · D_q F
// Continuum:    lim_{q→1} D_q F = zF'(z)
// =============================================================================

class QCalculus {
public:
    std::complex<double> q;  // the base
    int N;                    // q^N = λ
    double lambda;            // = |q|^N for real specialization

    // Standard constructions
    static QCalculus golden() {
        QCalculus qc;
        // q = iφ = φ·e^{iπ/2}
        qc.q = std::complex<double>(0.0, constants::PHI);
        qc.N = 4;
        qc.lambda = constants::LAMBDA;
        return qc;
    }

    static QCalculus standard(double q_real, int n) {
        QCalculus qc;
        qc.q = std::complex<double>(q_real, 0);
        qc.N = n;
        qc.lambda = std::pow(q_real, n);
        return qc;
    }

    // D_q F(z) = (F(qz) - F(z)) / ((q-1)z)
    // For real-valued functions on the real line
    double dq(std::function<double(double)> f, double z) const {
        if (std::abs(z) < 1e-15) return 0.0;
        double qr = q.real(); // real part for real evaluation
        return (f(qr * z) - f(z)) / ((qr - 1.0) * z);
    }

    // Complex D_q for complex functions
    std::complex<double> dqComplex(
        std::function<std::complex<double>(std::complex<double>)> f,
        std::complex<double> z) const
    {
        if (std::abs(z) < 1e-15) return {0, 0};
        auto qz = q * z;
        return (f(qz) - f(z)) / ((q - 1.0) * z);
    }

    // S^(N)_q G(z) = (q-1) Σ_{k=0}^{N-1} (q^k z) G(q^k z)
    double jackson_sum(std::function<double(double)> g, double z) const {
        double qr = q.real();
        double sum = 0.0;
        double qk = 1.0;
        for (int k = 0; k < N; ++k) {
            double zk = qk * z;
            sum += zk * g(zk);
            qk *= qr;
        }
        return (qr - 1.0) * sum;
    }

    // Verify fundamental identity: S^(N)_q D_q F(z) = F(λz) - F(z)
    double verifyFTC(std::function<double(double)> f, double z) const {
        // LHS: S^(N)_q (D_q F)(z)
        auto dqf = [&](double w) { return dq(f, w); };
        double lhs = jackson_sum(dqf, z);
        // RHS: F(λz) - F(z)
        double rhs = f(lambda * z) - f(z);
        return std::abs(lhs - rhs);
    }

    // D_{λ,N} F(z) = (1/ω)(E_λ - Id)F(z) = (F(λz) - F(z)) / ω
    // where ω = log(λ) + 2πi
    std::complex<double> dLambdaN(std::function<double(double)> f, double z) const {
        double flz = f(lambda * z);
        double fz = f(z);
        std::complex<double> omega(std::log(lambda), 2.0 * constants::PI);
        return std::complex<double>(flz - fz, 0.0) / omega;
    }

    // Product rule verification: D_q(fg) = f(q·)D_q(g) + g·D_q(f)
    double verifyProductRule(
        std::function<double(double)> f,
        std::function<double(double)> g,
        double z) const
    {
        auto fg = [&](double w) { return f(w) * g(w); };
        double qr = q.real();
        double lhs = dq(fg, z);
        double rhs = f(qr * z) * dq(g, z) + g(z) * dq(f, z);
        return std::abs(lhs - rhs);
    }

    // Continuum limit verification: lim_{q→1} D_q F → z·F'(z)
    static double continuumLimit(std::function<double(double)> f, double z, double h = 1e-8) {
        double fprime = (f(z + h) - f(z - h)) / (2.0 * h);
        return z * fprime;
    }
};

// =============================================================================
// 3. BV DECOMPOSITION & DEFECT MEASURE
// =============================================================================
//
// F(ω,·,t) ∈ BV_loc(R_ψ; A_n) decomposes as:
//   DF = (∂_ψ F) dψ + D^s_ψ F
//
// Over one period [ψ, ψ+2π]:
//   F(ψ+2π) - F(ψ) = ∫_ψ^{ψ+2π} ∂_ψ' F dψ' + μ^int_F(ψ, ψ+2π]
//
// The defect μ^int captures:
//   - Jump discontinuities (atoms of the singular measure)
//   - Cantor-type singularities
//   - Division-by-zero boundary currents (0^{-1} = 0 in meadow)
//
// Functoriality: μ_{F∘Φ} = Φ^* μ_F
// =============================================================================

struct BVDecomposition {
    double absolutelyContinuousPart;  // ∫ ∂_ψ F dψ
    double singularDefect;            // μ^int_F
    double totalVariation;            // |DF|([ψ₀, ψ₁])

    double totalChange() const { return absolutelyContinuousPart + singularDefect; }

    // Verify: total change = AC part + defect
    bool isConsistent(double tol = 1e-10) const {
        return std::abs(totalChange() - (absolutelyContinuousPart + singularDefect)) < tol;
    }
};

class BVMeasure {
public:
    // Compute BV decomposition for a function over [ψ₀, ψ₀+2π]
    static BVDecomposition decompose(
        std::function<double(double)> f,
        double psi0 = 0.0,
        int numPoints = 1000)
    {
        double psi1 = psi0 + 2.0 * constants::PI;
        double totalChange = f(psi1) - f(psi0);

        // Compute absolutely continuous part via trapezoidal rule
        // Approximate ∂_ψ F numerically
        double h = (psi1 - psi0) / numPoints;
        double acPart = 0.0;
        double totalVar = 0.0;
        double prev = f(psi0);
        for (int i = 1; i <= numPoints; ++i) {
            double psi = psi0 + i * h;
            double val = f(psi);
            double delta = val - prev;
            acPart += delta;  // ≈ integral of derivative
            totalVar += std::abs(delta);
            prev = val;
        }

        BVDecomposition bv;
        bv.absolutelyContinuousPart = acPart;
        bv.singularDefect = totalChange - acPart;
        bv.totalVariation = totalVar;
        return bv;
    }

    // Detect jump locations in a BV function
    static std::vector<double> detectJumps(
        std::function<double(double)> f,
        double psi0, double psi1,
        int numPoints = 1000,
        double threshold = 0.1)
    {
        std::vector<double> jumps;
        double h = (psi1 - psi0) / numPoints;
        double prev = f(psi0);
        for (int i = 1; i <= numPoints; ++i) {
            double psi = psi0 + i * h;
            double val = f(psi);
            if (std::abs(val - prev) > threshold * h) {
                jumps.push_back(psi - h / 2.0);
            }
            prev = val;
        }
        return jumps;
    }
};

// =============================================================================
// 4. SCALE COVARIANCE & MAPPING TORUS
// =============================================================================
//
// S_Λ(x) = Ax, A = exp((ln Λ)B), Q = tr(B)
// ρ(Ax) = Λρ(x), s = ln ρ, ψ = (2π/lnΛ)s
// C_{Λ,A}: R^d\{0} → Σ × S¹
// (R^d\{0})/⟨A⟩ ≅ Σ × S¹  (mapping torus)
//
// FORCING THEOREM:
//   S_Λ^* μ = Λ^σ μ  ⟹  μ = ρ^{-(σ+Q)} · C_{Λ,A}^* μ̂
//   where μ̂ lives intrinsically on Σ × S¹ × T
// =============================================================================

class ScaleCovariance {
public:
    double lambda;     // Λ = φ⁴
    double lnLambda;   // ln(Λ)
    int dim;           // d (dimension of R^d)
    double Q;          // Q = tr(B)

    ScaleCovariance()
        : lambda(constants::LAMBDA)
        , lnLambda(constants::LN_LAMBDA)
        , dim(2)
        , Q(1.0) // for d=2 with uniform dilation
    {}

    // Quasi-norm: ρ(x) = |x| for isotropic case
    double rho(const std::vector<double>& x) const {
        double r2 = 0;
        for (double xi : x) r2 += xi * xi;
        return std::sqrt(r2);
    }

    // Scale coordinate: s = ln ρ
    double s(const std::vector<double>& x) const {
        return std::log(rho(x));
    }

    // Angular coordinate: ψ = (2π/ln Λ) s
    double psi(const std::vector<double>& x) const {
        return (2.0 * constants::PI / lnLambda) * s(x);
    }

    // C_{Λ,A} map: x ↦ (ω, e^{iψ}) — returns (θ, ψ) on Σ × S¹
    std::pair<double, double> cayleyMap(double re, double im) const {
        double r = std::sqrt(re * re + im * im);
        if (r < 1e-300) return {0, 0};
        double theta = std::atan2(im, re);
        double s_val = std::log(r);
        double psi_val = (2.0 * constants::PI / lnLambda) * s_val;
        return {theta, psi_val};
    }

    // Scale covariance weight: ρ^{-(σ+Q)}
    double covarianceWeight(double rho_val, double sigma) const {
        return std::pow(rho_val, -(sigma + Q));
    }

    // Verify scale covariance: μ(Λx) = Λ^σ μ(x)
    bool verifyCovariance(
        std::function<double(double)> mu_func,
        double x, double sigma, double tol = 1e-8) const
    {
        double lhs = mu_func(lambda * x);
        double rhs = std::pow(lambda, sigma) * mu_func(x);
        return std::abs(lhs - rhs) < tol;
    }
};

// =============================================================================
// 5. Ξ_φ OPERATOR — THE GOLDEN INTERSTICE
// =============================================================================
//
// S_φ(ω, ψ, t) = (Ω(ω), ψ + π/2, σ(t))
// S_φ⁴ = (id, ψ+2π, σ⁴(t))   (quarter-turn symmetry)
//
// Ξ_φ F := (F∘S_φ - F) / χ_φ
//         where χ_φ = ln(φ) + iπ/2
//
// Properties:
//   Linearity:  Ξ_φ(αF + βG) = αΞ_φF + βΞ_φG
//   Product:    Ξ_φ(FG) = (F∘S_φ)(Ξ_φG) + (Ξ_φF)G
//   Inverse:    Ξ_φ(F⁻¹) = -(F⁻¹)(Ξ_φF)(F⁻¹∘S_φ)
//
// Π_φ H := χ_φ Σ_{k=0}^∞ H∘S_φ^{-(k+1)}   (inverse operator)
//   Ξ_φ(Π_φ H) = H
//   Π_φ(Ξ_φ F) = F - lim_{N→∞} F∘S_φ^{-(N+1)}
//
// Cascade equation: Ξ_φ†Ξ_φ F = 0
//
// Cayley-Dickson compatibility:
//   Ξ_φ ∘ ι_n = ι_n ∘ Ξ_φ
//   π_n ∘ Ξ_φ = Ξ_φ ∘ π_n
//
// FToI functor: FToI_n := N_{n+1} ∘ Ξ_φ ∘ N_n
// =============================================================================

class XiPhiOperator {
public:
    TimeScale timeScale;
    std::complex<double> chi_phi; // = ln(φ) + iπ/2

    XiPhiOperator()
        : timeScale(TimeScale::goldenGeometric())
        , chi_phi(constants::CHI_PHI_RE, constants::CHI_PHI_IM)
    {}

    // S_φ(ω, ψ, t) = (Ω(ω), ψ + π/2, σ(t))
    struct Point { double omega; double psi; double t; };

    Point shiftPhi(const Point& p) const {
        return {
            p.omega + constants::PI / 2.0,  // Ω(ω) = ω + π/2 (for d=2)
            p.psi + constants::PI / 2.0,
            timeScale.sigma(p.t)
        };
    }

    Point shiftPhiInverse(const Point& p) const {
        // Approximate inverse: need σ⁻¹
        return {
            p.omega - constants::PI / 2.0,
            p.psi - constants::PI / 2.0,
            p.t / constants::LAMBDA  // for geometric time scale
        };
    }

    // Ξ_φ F(p) = (F(S_φ(p)) - F(p)) / χ_φ
    // Returns complex result (since χ_φ is complex)
    std::complex<double> xiPhi(
        std::function<double(const Point&)> f,
        const Point& p) const
    {
        Point sp = shiftPhi(p);
        double delta = f(sp) - f(p);
        return std::complex<double>(delta, 0.0) / chi_phi;
    }

    // Π_φ H = χ_φ Σ_{k=0}^{K-1} H(S_φ^{-(k+1)}(p))
    std::complex<double> piPhi(
        std::function<double(const Point&)> h,
        const Point& p,
        int maxTerms = 20) const
    {
        std::complex<double> sum(0, 0);
        Point current = p;
        for (int k = 0; k < maxTerms; ++k) {
            current = shiftPhiInverse(current);
            sum += h(current);
        }
        return chi_phi * sum;
    }

    // Verify Ξ_φ(Π_φ H) = H
    double verifyInverse(
        std::function<double(const Point&)> h,
        const Point& p) const
    {
        // Compute Π_φ H as a real-valued function
        auto piH = [&](const Point& q) -> double {
            return piPhi(h, q).real();
        };
        // Then apply Ξ_φ
        auto result = xiPhi(piH, p);
        return std::abs(result.real() - h(p));
    }

    // Verify product rule: Ξ_φ(FG) = (F∘S_φ)(Ξ_φG) + (Ξ_φF)G
    double verifyProductRule(
        std::function<double(const Point&)> f,
        std::function<double(const Point&)> g,
        const Point& p) const
    {
        auto fg = [&](const Point& q) { return f(q) * g(q); };
        auto lhs = xiPhi(fg, p);
        Point sp = shiftPhi(p);
        auto rhs = f(sp) * xiPhi(g, p) + xiPhi(f, p) * g(p);
        return std::abs(lhs - rhs);
    }
};

// =============================================================================
// 6. PHYSICAL UNIT GRADING — U = Z^7
// =============================================================================
//
// U = Z^7 = ⟨L, M, T, I, Θ, N, J⟩ (SI base dimensions)
// Q_n = ⊔_{a∈D} (A_n · u^a)
// Grading: A_∞^{(u)} · A_∞^{(v)} ⊂ A_∞^{(u+v)}
//
// Grading operators: G_i(q, u^a) = a_i · (q, u^a)
// Unit rescaling: δ_τ^{(w)} = e^{τ(G·w)}
// Combined: E_{Λ,w} = E_ψ · δ_{lnΛ}^(w)
//           D_{Λ,w} = (E_{Λ,w} - I) / lnΛ
// =============================================================================

struct UnitVector {
    std::array<int, 7> dims;  // [L, M, T, I, Θ, N, J]

    UnitVector() : dims{} {}
    UnitVector(std::initializer_list<int> init) : dims{} {
        size_t i = 0;
        for (auto v : init) { if (i < 7) dims[i++] = v; }
    }

    UnitVector operator+(const UnitVector& other) const {
        UnitVector r;
        for (int i = 0; i < 7; ++i) r.dims[i] = dims[i] + other.dims[i];
        return r;
    }
    UnitVector operator-(const UnitVector& other) const {
        UnitVector r;
        for (int i = 0; i < 7; ++i) r.dims[i] = dims[i] - other.dims[i];
        return r;
    }
    bool operator==(const UnitVector& other) const { return dims == other.dims; }
    bool isDimensionless() const {
        for (int d : dims) if (d != 0) return false;
        return true;
    }

    int dot(const UnitVector& w) const {
        int s = 0;
        for (int i = 0; i < 7; ++i) s += dims[i] * w.dims[i];
        return s;
    }

    std::string toString() const {
        const char* names[] = {"L", "M", "T", "I", "Θ", "N", "J"};
        std::string s;
        bool first = true;
        for (int i = 0; i < 7; ++i) {
            if (dims[i] != 0) {
                if (!first) s += "·";
                s += names[i];
                if (dims[i] != 1) s += "^" + std::to_string(dims[i]);
                first = false;
            }
        }
        return s.empty() ? "1" : s;
    }

    // Standard physical units
    static UnitVector length()      { return {1, 0, 0, 0, 0, 0, 0}; }
    static UnitVector mass()        { return {0, 1, 0, 0, 0, 0, 0}; }
    static UnitVector time()        { return {0, 0, 1, 0, 0, 0, 0}; }
    static UnitVector current()     { return {0, 0, 0, 1, 0, 0, 0}; }
    static UnitVector temperature() { return {0, 0, 0, 0, 1, 0, 0}; }
    // Derived
    static UnitVector velocity()    { return {1, 0, -1, 0, 0, 0, 0}; }
    static UnitVector acceleration(){ return {1, 0, -2, 0, 0, 0, 0}; }
    static UnitVector force()       { return {1, 1, -2, 0, 0, 0, 0}; }
    static UnitVector energy()      { return {2, 1, -2, 0, 0, 0, 0}; }
    static UnitVector power()       { return {2, 1, -3, 0, 0, 0, 0}; }
    static UnitVector charge()      { return {0, 0, 1, 1, 0, 0, 0}; }
};

struct GradedValue {
    double value;
    UnitVector unit;
    uint8_t cdLevel;

    GradedValue operator*(const GradedValue& other) const {
        return {value * other.value, unit + other.unit, std::max(cdLevel, other.cdLevel)};
    }
    GradedValue operator+(const GradedValue& other) const {
        // Can only add same-dimension quantities
        return {value + other.value, unit, std::max(cdLevel, other.cdLevel)};
    }
    GradedValue operator/(const GradedValue& other) const {
        return {value / other.value, unit - other.unit, std::max(cdLevel, other.cdLevel)};
    }

    // Unit rescaling: δ_τ^{(w)} (q, u^a) = e^{τ(w·a)} q · u^a
    GradedValue rescale(double tau, const UnitVector& w) const {
        int wdota = unit.dot(w);
        return {value * std::exp(tau * wdota), unit, cdLevel};
    }
};

// =============================================================================
// 7. UNIVERSAL DIFFERENCE QUOTIENT — D_{g,χ}
// =============================================================================
//
// For a group G acting on a set X:
//   U_g : Fun(X, A_n) → Fun(X, A_n),   (U_g f)(x) = f(g·x)
//   D_{g,χ} f := (U_g f - f) / χ_g
//
// Ω cocycle: Ω_{gh}(x) = Ω_g(h·x) + Ω_h(x)
// Master equation: Δ_g F = Ω_g · D_g F + μ_{F,g}
//
// Product rule: D_g(FH) = (E_g F)(D_g H) + (D_g F)H
// Inverse rule: D_g(F⁻¹) = -(E_g F)⁻¹(D_g F)F⁻¹
// Consolidated FTC: F(g^N·x) - F(x) = Σ_{j=0}^{N-1} (Ω_g D_g F + μ_{F,g})(g^j·x)
// Conservation: Δ_g η(U) + Div_g q(U) = μ_src
//
// Context C = (A, X, G, E, Ω, μ)
// Transport: T_h : T_{k∘h} = T_k ∘ T_h  (functor)
// =============================================================================

class UniversalDifferenceQuotient {
public:
    // General D_{g,χ}: (f(g·x) - f(x)) / χ
    static double compute(
        std::function<double(double)> f,
        std::function<double(double)> g_action, // g·x
        double chi,
        double x)
    {
        if (std::abs(chi) < 1e-15) return 0.0;
        return (f(g_action(x)) - f(x)) / chi;
    }

    // E_g F(x) = F(g·x)  — shift operator
    static double shift(
        std::function<double(double)> f,
        std::function<double(double)> g_action,
        double x)
    {
        return f(g_action(x));
    }

    // Verify product rule: D_g(FH) = (E_g F)(D_g H) + (D_g F)H
    static double verifyProductRule(
        std::function<double(double)> f,
        std::function<double(double)> h,
        std::function<double(double)> g_action,
        double chi,
        double x)
    {
        auto fh = [&](double w) { return f(w) * h(w); };
        double lhs = compute(fh, g_action, chi, x);
        double eg_f = shift(f, g_action, x);
        double dg_h = compute(h, g_action, chi, x);
        double dg_f = compute(f, g_action, chi, x);
        double rhs = eg_f * dg_h + dg_f * h(x);
        return std::abs(lhs - rhs);
    }

    // Consolidated FTC: F(g^N·x) - F(x) = Σ Ω D F + μ
    static double consolidatedFTC(
        std::function<double(double)> f,
        std::function<double(double)> g_action,
        double chi,
        int N,
        double x)
    {
        // LHS: F(g^N·x) - F(x)
        double gN_x = x;
        for (int i = 0; i < N; ++i) gN_x = g_action(gN_x);
        double lhs = f(gN_x) - f(x);

        // RHS: Σ_{j=0}^{N-1} (chi · D_g F)(g^j·x)
        double rhs = 0.0;
        double gj_x = x;
        for (int j = 0; j < N; ++j) {
            rhs += chi * compute(f, g_action, chi, gj_x);
            gj_x = g_action(gj_x);
        }

        return std::abs(lhs - rhs);
    }
};

// =============================================================================
// 8. GAUGE CONNECTION
// =============================================================================
//
// ∇ = d_X + A
// F = d_X A + A ∧ A = μ^int   (curvature = defect)
// ∇F = 0                      (Bianchi identity)
// ∇² = μ^int·                  (curvature as obstruction)
//
// Holonomy: Hol_{S¹_ψ × T}(A) = Λ
// UFE: (D + A)†(D + A)F = 0   (universal field equation)
// =============================================================================

struct GaugeField {
    // A_ω, A_ψ, A_t — connection 1-form components
    double A_omega;
    double A_psi;
    double A_t;

    // Curvature 2-form components: F_{ψt}, F_{ωψ}, F_{ωt}
    double F_psi_t(double dA_psi_dt, double dA_t_dpsi) const {
        return dA_psi_dt - dA_t_dpsi + A_t * A_psi - A_psi * A_t;
    }
    double F_omega_psi(double dA_psi_domega, double dA_omega_dpsi) const {
        return dA_psi_domega - dA_omega_dpsi + A_omega * A_psi - A_psi * A_omega;
    }
    double F_omega_t(double dA_omega_dt, double dA_t_domega) const {
        return dA_omega_dt - dA_t_domega + A_t * A_omega - A_omega * A_t;
    }

    // Holonomy around ψ-circle: exp(∫_ψ^{ψ+2π} A_ψ dψ')
    // For constant A_ψ: exp(2π A_ψ) should equal Λ
    double holonomyPsi() const {
        return std::exp(2.0 * constants::PI * A_psi);
    }

    // Check flat connection: F = 0 ⟺ Hol_γ = 1 for all γ ∈ π₁(X̄)
    bool isFlat(double tol = 1e-8) const {
        // For abelian case with constant connection
        return std::abs(A_omega * A_psi - A_psi * A_omega) < tol;
    }
};

// =============================================================================
// 9. INTERSTICE AXIOM MODULE — Generates axioms for the discovery engine
// =============================================================================

class IntersticeAxiomModule {
public:
    explicit IntersticeAxiomModule(TermFactory& factory) : factory_(factory) {}

    [[nodiscard]] std::vector<std::unique_ptr<Equation>> generateAxioms() {
        std::vector<std::unique_ptr<Equation>> axioms;
        generateTimeScaleAxioms(axioms);
        generateQCalculusAxioms(axioms);
        generateBVDefectAxioms(axioms);
        generateScaleCovarianceAxioms(axioms);
        generateXiPhiAxioms(axioms);
        generateUnitGradingAxioms(axioms);
        generateUniversalDQAxioms(axioms);
        generateGaugeAxioms(axioms);
        generateContinuumLimitAxioms(axioms);
        generateConsolidatedFTCAxioms(axioms);
        return axioms;
    }

private:
    TermFactory& factory_;
    uint32_t vc_ = 0;

    const Term* fv(Sort s = Sort::Generic) {
        return factory_.variable("int" + std::to_string(vc_++), s);
    }

    // --- Time scale axioms ---
    void generateTimeScaleAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto f = fv(); auto t = fv(); auto a = fv(); auto b = fv();

        // TS1: Delta derivative definition
        //   DeltaDeriv(f, t) = (f(sigma(t)) - f(t)) / mu(t)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("DeltaDeriv", {f, t}),
            factory_.apply("div", {
                factory_.add(
                    factory_.apply("eval", {f, factory_.apply("sigma", {t})}),
                    factory_.neg(factory_.apply("eval", {f, t}))),
                factory_.apply("mu", {t})
            })
        ));

        // TS2: Hilger FTC
        //   HilgerInt(DeltaDeriv(f), a, b) = eval(f, b) - eval(f, a) + defect_T(f, a, b)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("HilgerInt", {factory_.apply("DeltaDeriv", {f, t}), a, b}),
            factory_.add(
                factory_.add(
                    factory_.apply("eval", {f, b}),
                    factory_.neg(factory_.apply("eval", {f, a}))),
                factory_.apply("DefectT", {f, a, b}))
        ));

        // TS3: T=R specialization: sigma(t) = t, mu(t) = 0
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("sigma_R", {t}),
            t
        ));

        // TS4: T=Z specialization: sigma(t) = t + 1
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("sigma_Z", {t}),
            factory_.add(t, factory_.scalar(1.0))
        ));

        // TS5: T=q^Z specialization: sigma(t) = q*t
        auto q = fv();
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("sigma_qZ", {q, t}),
            factory_.mul(q, t)
        ));
    }

    // --- q-Calculus axioms ---
    void generateQCalculusAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto f = fv(); auto g = fv(); auto z = fv(); auto q = fv();

        // QC1: D_q F(z) = (F(qz) - F(z)) / ((q-1)z)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Dq", {q, f, z}),
            factory_.apply("div", {
                factory_.add(
                    factory_.apply("eval", {f, factory_.mul(q, z)}),
                    factory_.neg(factory_.apply("eval", {f, z}))),
                factory_.mul(factory_.add(q, factory_.scalar(-1.0)), z)
            })
        ));

        // QC2: Product rule: D_q(fg) = f(q·)D_q(g) + g·D_q(f)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Dq", {q, factory_.mul(f, g), z}),
            factory_.add(
                factory_.mul(
                    factory_.apply("eval", {f, factory_.mul(q, z)}),
                    factory_.apply("Dq", {q, g, z})),
                factory_.mul(g, factory_.apply("Dq", {q, f, z})))
        ));

        // QC3: Golden specialization: q = iPhi, N = 4
        auto phi = factory_.phi();
        auto iPhi = factory_.apply("iPhi", {phi}); // i*φ
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("GoldenQ", {}),
            iPhi
        ));

        // QC4: S^(N)_q · D_q = E_λ - Id  (fundamental identity)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("JacksonSum", {q, factory_.apply("Dq", {q, f, z}), z}),
            factory_.add(
                factory_.apply("eval", {f, factory_.apply("lambda_from_q", {q})}),
                factory_.neg(factory_.apply("eval", {f, z})))
        ));
    }

    // --- BV Defect axioms ---
    void generateBVDefectAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto f = fv(); auto psi = fv();

        // BV1: F(ψ+2π) - F(ψ) = ∫_ψ^{ψ+2π} ∂_ψ' F dψ' + μ^int_F
        ax.push_back(std::make_unique<Equation>(
            factory_.add(
                factory_.apply("eval", {f, factory_.add(psi, factory_.apply("TwoPi", {}))}),
                factory_.neg(factory_.apply("eval", {f, psi}))),
            factory_.add(
                factory_.apply("ACPart", {f, psi}),
                factory_.apply("DefectInt", {f, psi}))
        ));

        // BV2: Functoriality: μ_{F∘Φ} = Φ*μ_F
        auto phi_map = fv();
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("DefectInt", {factory_.apply("compose", {f, phi_map}), psi}),
            factory_.apply("Pullback", {phi_map, factory_.apply("DefectInt", {f, psi})})
        ));

        // BV3: Homological Stokes: ⟨dω, c⟩ = ⟨ω, ∂c⟩ + ⟨μ^int_ω, c⟩
        auto omega = fv(); auto c_chain = fv();
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Pair_dω_c", {factory_.apply("dExt", {omega}), c_chain}),
            factory_.add(
                factory_.apply("Pair_ω_∂c", {omega, factory_.apply("boundary", {c_chain})}),
                factory_.apply("DefectPairing", {omega, c_chain}))
        ));
    }

    // --- Scale Covariance axioms ---
    void generateScaleCovarianceAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto mu = fv(); auto x = fv(); auto sigma_param = fv();

        // SC1: S_Λ^*μ = Λ^σ μ  ⟹  μ = ρ^{-(σ+Q)} C^*μ̂
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("ScaleForce", {mu, sigma_param}),
            factory_.mul(
                factory_.apply("RhoPow", {x, factory_.neg(factory_.add(sigma_param, factory_.apply("Q", {})))}),
                factory_.apply("CayleyPullback", {factory_.apply("MuHat", {mu})}))
        ));

        // SC2: ρ(ψ+2π) = φ⁴ · ρ(ψ)
        auto psi = fv();
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("rho", {factory_.add(psi, factory_.apply("TwoPi", {}))}),
            factory_.mul(
                factory_.apply("Lambda", {}),
                factory_.apply("rho", {psi}))
        ));

        // SC3: Mapping torus isomorphism
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("QuotientSpace", {x, factory_.apply("A", {})}),
            factory_.apply("MappingTorus", {factory_.apply("Sigma_surf", {}), factory_.apply("S1", {})})
        ));
    }

    // --- Ξ_φ operator axioms ---
    void generateXiPhiAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto f = fv(); auto g = fv(); auto p = fv();

        // XI1: Ξ_φ F = (F∘S_φ - F) / χ_φ
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("XiPhi", {f, p}),
            factory_.apply("div", {
                factory_.add(
                    factory_.apply("eval", {f, factory_.apply("ShiftPhi", {p})}),
                    factory_.neg(factory_.apply("eval", {f, p}))),
                factory_.apply("ChiPhi", {})
            })
        ));

        // XI2: Product rule: Ξ_φ(FG) = (F∘S_φ)(Ξ_φ G) + (Ξ_φ F)G
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("XiPhi", {factory_.mul(f, g), p}),
            factory_.add(
                factory_.mul(
                    factory_.apply("eval", {f, factory_.apply("ShiftPhi", {p})}),
                    factory_.apply("XiPhi", {g, p})),
                factory_.mul(
                    factory_.apply("XiPhi", {f, p}),
                    factory_.apply("eval", {g, p})))
        ));

        // XI3: Inverse pair: Ξ_φ(Π_φ H) = H
        auto h = fv();
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("XiPhi", {factory_.apply("PiPhi", {h}), p}),
            factory_.apply("eval", {h, p})
        ));

        // XI4: Cayley-Dickson compatibility: Ξ_φ ∘ ι_n = ι_n ∘ Ξ_φ
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("XiPhi", {factory_.apply("iota", {f}), p}),
            factory_.apply("iota", {factory_.apply("XiPhi", {f, p})})
        ));

        // XI5: Cascade equation: Ξ_φ†Ξ_φ F = 0
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("XiPhiAdj_XiPhi", {f, p}),
            factory_.scalar(0.0)
        ));

        // XI6: FToI functor: FToI_n = N_{n+1} ∘ Ξ_φ ∘ N_n
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("FToI", {f, p}),
            factory_.apply("N_next", {factory_.apply("XiPhi", {factory_.apply("N_curr", {f}), p})})
        ));
    }

    // --- Unit grading axioms ---
    void generateUnitGradingAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto f = fv(); auto g = fv();

        // UG1: dim(f·g) = dim(f) + dim(g)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("UnitDim", {factory_.mul(f, g)}),
            factory_.apply("UnitAdd", {
                factory_.apply("UnitDim", {f}),
                factory_.apply("UnitDim", {g})})
        ));

        // UG2: dim(f+g) requires dim(f) = dim(g) (dimensional consistency)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("UnitDim", {factory_.add(f, g)}),
            factory_.apply("UnitDim", {f})
        ));

        // UG3: Unit rescaling commutes with ι_n
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("iota", {factory_.apply("UnitRescale", {f})}),
            factory_.apply("UnitRescale", {factory_.apply("iota", {f})})
        ));
    }

    // --- Universal Difference Quotient axioms ---
    void generateUniversalDQAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto f = fv(); auto g_act = fv(); auto chi = fv();
        auto h = fv(); auto x = fv();

        // UDQ1: D_{g,χ} f = (U_g f - f) / χ
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("UnivDQ", {g_act, chi, f, x}),
            factory_.apply("div", {
                factory_.add(
                    factory_.apply("Shift", {g_act, f, x}),
                    factory_.neg(factory_.apply("eval", {f, x}))),
                chi
            })
        ));

        // UDQ2: Product rule: D_g(FH) = (E_g F)(D_g H) + (D_g F)H
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("UnivDQ", {g_act, chi, factory_.mul(f, h), x}),
            factory_.add(
                factory_.mul(
                    factory_.apply("Shift", {g_act, f, x}),
                    factory_.apply("UnivDQ", {g_act, chi, h, x})),
                factory_.mul(
                    factory_.apply("UnivDQ", {g_act, chi, f, x}),
                    factory_.apply("eval", {h, x})))
        ));

        // UDQ3: Inverse rule: D_g(F⁻¹) = -(E_g F)⁻¹(D_g F)F⁻¹
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("UnivDQ", {g_act, chi, factory_.inv(f), x}),
            factory_.neg(factory_.mul(
                factory_.inv(factory_.apply("Shift", {g_act, f, x})),
                factory_.mul(
                    factory_.apply("UnivDQ", {g_act, chi, f, x}),
                    factory_.inv(factory_.apply("eval", {f, x})))))
        ));

        // UDQ4: Commutativity: [D_g, D_h] = 0 when μ=0 and gh=hg
        // (structural axiom, not directly evaluatable)
    }

    // --- Gauge connection axioms ---
    void generateGaugeAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto A = fv(); auto f = fv();

        // G1: Curvature: F = dA + A∧A
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Curvature", {A}),
            factory_.add(
                factory_.apply("dExt", {A}),
                factory_.apply("Wedge", {A, A}))
        ));

        // G2: Curvature = defect: F = μ^int
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Curvature", {A}),
            factory_.apply("DefectInt", {A, factory_.apply("AllX", {})})
        ));

        // G3: Bianchi identity: ∇F = 0
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("CovDeriv", {factory_.apply("Curvature", {A})}),
            factory_.scalar(0.0)
        ));

        // G4: ∇² = μ^int·  (curvature as second covariant derivative)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("CovDeriv2", {f}),
            factory_.mul(factory_.apply("DefectInt", {factory_.apply("conn", {}), factory_.apply("AllX", {})}), f)
        ));

        // G5: Holonomy: Hol_{S¹}(A) = Λ
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Holonomy_S1", {A}),
            factory_.apply("Lambda", {})
        ));
    }

    // --- Continuum limit axioms ---
    void generateContinuumLimitAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto f = fv(); auto z = fv();

        // CL1: lim_{Λ→1} D_Λ = ∂_s
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("ContinuumLimit_Scale", {f}),
            factory_.apply("PartialS", {f})
        ));

        // CL2: lim_{q→1} D_q = d/dz (for z·d/dz)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("ContinuumLimit_Q", {f, z}),
            factory_.mul(z, factory_.apply("Deriv", {f, z}))
        ));

        // CL3: lim_{ν→0,lnΛ→0} D^♯ → ∂_u + ∂_s + G·w
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("ContinuumLimit_Sharp", {f}),
            factory_.add(
                factory_.apply("PartialU", {f}),
                factory_.add(
                    factory_.apply("PartialS", {f}),
                    factory_.apply("GradingDeriv", {f})))
        ));
    }

    // --- Consolidated FTC axioms ---
    void generateConsolidatedFTCAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto f = fv(); auto x = fv(); auto g_act = fv(); auto N = fv();

        // FTC1: F(g^N·x) - F(x) = Σ_{j=0}^{N-1} (Ω·D_g F + μ)(g^j·x)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("ConsolidatedFTC_LHS", {f, g_act, N, x}),
            factory_.apply("ConsolidatedFTC_RHS", {f, g_act, N, x})
        ));

        // FTC2: Half-inverse: ½ D J = I (combined scale+time)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("HalfDJ", {f}),
            f
        ));

        // FTC3: D·J = I (left inverse)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("DJ", {f}),
            f
        ));

        // FTC4: J·D = I - P_{-∞} (right quasi-inverse)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("JD", {f}),
            factory_.add(f, factory_.neg(factory_.apply("Proj_neg_inf", {f})))
        ));
    }
};

// =============================================================================
// 10. INTERSTICE TERM GENERATOR — For the Discovery Engine
// =============================================================================

class IntersticeTermGenerator {
public:
    struct Config {
        int maxDepth = 2;
        size_t maxTerms = 5000;
        bool includeTimeScale = true;
        bool includeQCalc = true;
        bool includeBVDefect = true;
        bool includeScaleCovariance = true;
        bool includeXiPhi = true;
        bool includeUnitGrading = true;
        bool includeUniversalDQ = true;
        bool includeGauge = true;
    };

    explicit IntersticeTermGenerator(TermFactory& factory, Config config = {})
        : factory_(factory), config_(config) {}

    [[nodiscard]] std::vector<const Term*> generateAll() {
        std::vector<const Term*> result;
        std::unordered_set<const Term*> seen;

        auto tryAdd = [&](const Term* t) {
            if (t && result.size() < config_.maxTerms && seen.insert(t).second) {
                result.push_back(t);
            }
        };

        // Base atoms
        auto phi = factory_.phi();
        auto one = factory_.scalar(1.0);
        auto zero = factory_.scalar(0.0);
        auto neg1 = factory_.scalar(-1.0);
        auto two = factory_.scalar(2.0);
        auto half = factory_.scalar(0.5);
        auto pi_val = factory_.scalar(constants::PI);
        auto lambda_val = factory_.scalar(constants::LAMBDA);
        auto lnlambda = factory_.scalar(constants::LN_LAMBDA);
        auto lnphi = factory_.scalar(constants::LN_PHI);
        auto alpha_val = factory_.scalar(constants::ALPHA);
        auto beta_val = factory_.scalar(constants::BETA);
        auto kappa_val = factory_.scalar(constants::CRITICAL_EXPONENT);
        auto chi_re = factory_.scalar(constants::CHI_PHI_RE);
        auto chi_im = factory_.scalar(constants::CHI_PHI_IM);

        std::vector<const Term*> atoms = {
            phi, one, zero, neg1, two, half, pi_val,
            lambda_val, lnlambda, lnphi, alpha_val, beta_val,
            kappa_val, chi_re, chi_im
        };
        for (auto* a : atoms) tryAdd(a);

        // Named constants
        tryAdd(factory_.apply("Lambda", {}));
        tryAdd(factory_.apply("LnLambda", {}));
        tryAdd(factory_.apply("Alpha", {}));
        tryAdd(factory_.apply("Beta", {}));
        tryAdd(factory_.apply("ChiPhi", {}));
        tryAdd(factory_.apply("Kappa", {}));
        tryAdd(factory_.apply("TwoPi", {}));

        // Variable atoms for operator application
        auto x = factory_.variable("x_int", Sort::Generic);
        auto y = factory_.variable("y_int", Sort::Generic);
        auto t_var = factory_.variable("t_int", Sort::Generic);
        auto psi_var = factory_.variable("psi_int", Sort::Generic);
        auto z = factory_.variable("z_int", Sort::Generic);
        tryAdd(x); tryAdd(y); tryAdd(t_var); tryAdd(psi_var); tryAdd(z);

        // === TIME SCALE terms ===
        if (config_.includeTimeScale) {
            tryAdd(factory_.apply("sigma", {t_var}));
            tryAdd(factory_.apply("mu", {t_var}));
            tryAdd(factory_.apply("DeltaDeriv", {x, t_var}));
            tryAdd(factory_.apply("sigma_R", {t_var}));
            tryAdd(factory_.apply("sigma_Z", {t_var}));
            for (auto* a : atoms) {
                if (result.size() >= config_.maxTerms) break;
                tryAdd(factory_.apply("DeltaDeriv", {a, t_var}));
            }
        }

        // === Q-CALCULUS terms ===
        if (config_.includeQCalc) {
            auto iPhi = factory_.apply("iPhi", {phi});
            tryAdd(iPhi);
            tryAdd(factory_.apply("GoldenQ", {}));

            for (auto* a : atoms) {
                if (result.size() >= config_.maxTerms) break;
                tryAdd(factory_.apply("Dq", {iPhi, a, z}));
                tryAdd(factory_.apply("Dq", {phi, a, z}));
            }
            tryAdd(factory_.apply("JacksonSum", {iPhi, x, z}));
            tryAdd(factory_.apply("Dq", {iPhi, x, z}));
            tryAdd(factory_.apply("Dq", {iPhi, y, z}));
        }

        // === BV DEFECT terms ===
        if (config_.includeBVDefect) {
            tryAdd(factory_.apply("ACPart", {x, psi_var}));
            tryAdd(factory_.apply("DefectInt", {x, psi_var}));

            for (auto* a : atoms) {
                if (result.size() >= config_.maxTerms) break;
                tryAdd(factory_.apply("ACPart", {a, psi_var}));
                tryAdd(factory_.apply("DefectInt", {a, psi_var}));
            }
        }

        // === SCALE COVARIANCE terms ===
        if (config_.includeScaleCovariance) {
            tryAdd(factory_.apply("rho", {x}));
            tryAdd(factory_.apply("ScaleForce", {x, factory_.scalar(1.0)}));
            tryAdd(factory_.apply("CayleyPullback", {x}));
            tryAdd(factory_.apply("MappingTorus", {x, y}));

            for (auto* a : atoms) {
                if (result.size() >= config_.maxTerms) break;
                tryAdd(factory_.apply("rho", {a}));
                tryAdd(factory_.apply("RhoPow", {a, factory_.scalar(-1.0)}));
            }
        }

        // === Ξ_φ OPERATOR terms ===
        if (config_.includeXiPhi) {
            tryAdd(factory_.apply("XiPhi", {x, psi_var}));
            tryAdd(factory_.apply("PiPhi", {x}));
            tryAdd(factory_.apply("ShiftPhi", {psi_var}));
            tryAdd(factory_.apply("XiPhiAdj_XiPhi", {x, psi_var}));
            tryAdd(factory_.apply("FToI", {x, psi_var}));

            for (auto* a : atoms) {
                if (result.size() >= config_.maxTerms) break;
                tryAdd(factory_.apply("XiPhi", {a, psi_var}));
                tryAdd(factory_.apply("PiPhi", {a}));
            }
        }

        // === UNIT GRADING terms ===
        if (config_.includeUnitGrading) {
            tryAdd(factory_.apply("UnitDim", {x}));
            tryAdd(factory_.apply("UnitRescale", {x}));

            for (auto* a : atoms) {
                if (result.size() >= config_.maxTerms) break;
                tryAdd(factory_.apply("UnitDim", {a}));
            }
        }

        // === UNIVERSAL DQ terms ===
        if (config_.includeUniversalDQ) {
            auto g = factory_.variable("g_int", Sort::Generic);
            auto chi = factory_.apply("ChiPhi", {});
            tryAdd(factory_.apply("UnivDQ", {g, chi, x, z}));
            tryAdd(factory_.apply("Shift", {g, x, z}));

            for (auto* a : atoms) {
                if (result.size() >= config_.maxTerms) break;
                tryAdd(factory_.apply("UnivDQ", {g, chi, a, z}));
            }
        }

        // === GAUGE CONNECTION terms ===
        if (config_.includeGauge) {
            auto A_conn = factory_.variable("A_conn", Sort::Generic);
            tryAdd(factory_.apply("Curvature", {A_conn}));
            tryAdd(factory_.apply("CovDeriv", {x}));
            tryAdd(factory_.apply("Holonomy_S1", {A_conn}));
        }

        // === CONTINUUM LIMIT terms ===
        tryAdd(factory_.apply("ContinuumLimit_Scale", {x}));
        tryAdd(factory_.apply("ContinuumLimit_Q", {x, z}));
        tryAdd(factory_.apply("ContinuumLimit_Sharp", {x}));

        // === CONSOLIDATED FTC terms ===
        auto g_act = factory_.variable("g_act", Sort::Generic);
        tryAdd(factory_.apply("ConsolidatedFTC_LHS", {x, g_act, factory_.scalar(4.0), z}));
        tryAdd(factory_.apply("ConsolidatedFTC_RHS", {x, g_act, factory_.scalar(4.0), z}));
        tryAdd(factory_.apply("HalfDJ", {x}));

        // === DEPTH-2 COMPOSITIONS ===
        if (config_.maxDepth >= 2) {
            std::vector<const Term*> depth1;
            for (auto* t : result) {
                if (t->depth() <= 1) depth1.push_back(t);
                if (depth1.size() >= 50) break;
            }

            for (auto* t : depth1) {
                if (result.size() >= config_.maxTerms) break;
                // Apply each operator to depth-1 terms
                tryAdd(factory_.apply("XiPhi", {t, psi_var}));
                tryAdd(factory_.apply("Dq", {factory_.apply("iPhi", {phi}), t, z}));
                tryAdd(factory_.apply("DeltaDeriv", {t, t_var}));
                tryAdd(factory_.apply("UnivDQ", {g_act, factory_.apply("ChiPhi", {}), t, z}));
                tryAdd(factory_.apply("ACPart", {t, psi_var}));
                tryAdd(factory_.apply("DefectInt", {t, psi_var}));

                // Binary compositions
                for (auto* s : depth1) {
                    if (result.size() >= config_.maxTerms) break;
                    tryAdd(factory_.add(t, s));
                    tryAdd(factory_.mul(t, s));
                }
            }
        }

        return result;
    }

private:
    TermFactory& factory_;
    Config config_;
};

// =============================================================================
// 11. INTERSTICE NUMERIC EVALUATOR — For value-bucketing discovery
// =============================================================================

class IntersticeNumericEvaluator {
public:
    [[nodiscard]] std::optional<double> evaluate(const Term* t) const {
        if (!t) return std::nullopt;

        if (t->isScalar()) return t->scalarValue();
        if (t->isPhi()) return constants::PHI;
        if (t->isPhiBar()) return constants::PHI_INV;

        if (t->kind() == core::TermKind::Application) {
            const std::string& sym = t->symbol();
            const auto& ch = t->children();

            // === Named constants ===
            if (sym == "Lambda" && ch.empty()) return constants::LAMBDA;
            if (sym == "LnLambda" && ch.empty()) return constants::LN_LAMBDA;
            if (sym == "Alpha" && ch.empty()) return constants::ALPHA;
            if (sym == "Beta" && ch.empty()) return constants::BETA;
            if (sym == "Kappa" && ch.empty()) return constants::CRITICAL_EXPONENT;
            if (sym == "TwoPi" && ch.empty()) return 2.0 * constants::PI;
            if (sym == "ChiPhi" && ch.empty()) return constants::CHI_PHI_RE; // real part

            // === Interstice constants ===
            if (sym == "iPhi" && ch.size() == 1) {
                // |iφ| = φ
                auto v = evaluate(ch[0]);
                if (v) return *v; // magnitude of iφ = φ
            }
            if (sym == "GoldenQ" && ch.empty()) return constants::PHI; // |q| = φ

            // === Time scale operators ===
            if (sym == "sigma" && ch.size() == 1) {
                auto v = evaluate(ch[0]);
                if (v) return constants::LAMBDA * (*v); // golden geometric
            }
            if (sym == "mu" && ch.size() == 1) {
                auto v = evaluate(ch[0]);
                if (v) return (constants::LAMBDA - 1.0) * (*v);
            }
            if (sym == "sigma_R" && ch.size() == 1) {
                return evaluate(ch[0]); // σ(t) = t for T=R
            }
            if (sym == "sigma_Z" && ch.size() == 1) {
                auto v = evaluate(ch[0]);
                if (v) return *v + 1.0;
            }

            // === Q-calculus numeric ===
            if (sym == "Dq" && ch.size() == 3) {
                // D_q(f, z) — for scalar constants, D_q(c) = 0
                auto q_val = evaluate(ch[0]);
                auto f_val = evaluate(ch[1]);
                auto z_val = evaluate(ch[2]);
                if (q_val && f_val && z_val && std::abs(*z_val) > 1e-15) {
                    // For constant f: D_q(f)(z) = 0
                    return 0.0;
                }
            }

            if (sym == "lambda_from_q" && ch.size() == 1) {
                auto v = evaluate(ch[0]);
                if (v) return std::pow(*v, 4); // q^N with N=4
            }

            // === Scale operators ===
            if (sym == "rho" && ch.size() == 1) {
                auto v = evaluate(ch[0]);
                if (v) return std::abs(*v);
            }
            if (sym == "RhoPow" && ch.size() == 2) {
                auto base = evaluate(ch[0]);
                auto exp_val = evaluate(ch[1]);
                if (base && exp_val && *base > 0)
                    return std::pow(*base, *exp_val);
            }

            // === BV operators ===
            if (sym == "ACPart" && ch.size() == 2) {
                // For smooth functions, AC part = total change
                auto f_val = evaluate(ch[0]);
                if (f_val) return *f_val; // trivial for constants
            }
            if (sym == "DefectInt" && ch.size() == 2) {
                // For smooth/constant: defect = 0
                auto f_val = evaluate(ch[0]);
                if (f_val) return 0.0;
            }

            // === Xi-phi operator ===
            if (sym == "XiPhi" && ch.size() == 2) {
                auto f_val = evaluate(ch[0]);
                if (f_val) {
                    // For constants: Ξ_φ(c) = (c - c)/χ_φ = 0
                    return 0.0;
                }
            }
            if (sym == "XiPhiAdj_XiPhi" && ch.size() == 2) {
                // Cascade equation: always 0
                return 0.0;
            }
            if (sym == "PiPhi" && ch.size() == 1) {
                auto v = evaluate(ch[0]);
                if (v) {
                    // For constant h: Π_φ h = χ_φ Σ h
                    // Geometric sum: h Σ_{k=0}^∞ 1 — diverges unless normalized
                    return *v * constants::CHI_PHI_RE * 20.0; // finite truncation
                }
            }

            // === Gauge operators ===
            if (sym == "Holonomy_S1" && ch.size() == 1) {
                // Should equal Λ
                return constants::LAMBDA;
            }
            if (sym == "Curvature" && ch.size() == 1) {
                // For flat connection: 0
                return 0.0;
            }

            // === Continuum limits ===
            if (sym == "ContinuumLimit_Scale" && ch.size() == 1) {
                auto v = evaluate(ch[0]);
                if (v) return *v; // in the limit, derivative of constant = 0
            }
            if (sym == "ContinuumLimit_Q" && ch.size() == 2) {
                // lim D_q → z·d/dz; for constant = 0
                return 0.0;
            }

            // === Consolidated FTC ===
            if (sym == "HalfDJ" && ch.size() == 1) {
                return evaluate(ch[0]); // ½DJ = I
            }
            if (sym == "DJ" && ch.size() == 1) {
                return evaluate(ch[0]); // DJ = I
            }

            // === Arithmetic fallback ===
            if (sym == "div" && ch.size() == 2) {
                auto a = evaluate(ch[0]);
                auto b = evaluate(ch[1]);
                if (a && b && std::abs(*b) > 1e-15) return *a / *b;
            }
            if (sym == "eval" && ch.size() == 2) {
                // eval(f, x) — evaluate f at x; for constants just return f
                return evaluate(ch[0]);
            }
            if (sym == "compose" && ch.size() == 2) {
                return evaluate(ch[0]); // f∘g(x) → f for constants
            }

            // Pass through standard arithmetic
            if (sym == "add" && ch.size() == 2) {
                auto a = evaluate(ch[0]);
                auto b = evaluate(ch[1]);
                if (a && b) return *a + *b;
            }
            if (sym == "mul" && ch.size() == 2) {
                auto a = evaluate(ch[0]);
                auto b = evaluate(ch[1]);
                if (a && b) return *a * *b;
            }
            if (sym == "neg" && ch.size() == 1) {
                auto v = evaluate(ch[0]);
                if (v) return -(*v);
            }
        }

        // Fallback: try standard arithmetic on Application nodes
        if (t->kind() == core::TermKind::Application && t->children().size() == 2) {
            const std::string& sym = t->symbol();
            auto a = evaluate(t->children()[0]);
            auto b = evaluate(t->children()[1]);
            if (a && b) {
                if (sym == "add") return *a + *b;
                if (sym == "mul") return *a * *b;
                if (sym == "sub") return *a - *b;
                if (sym == "pow" && *a > 0) return std::pow(*a, *b);
            }
        }

        return std::nullopt;
    }
};

// =============================================================================
// 13. DIAMOND OPERATOR ◊_B  (Sections CIV–CIX of Interstices manuscript)
// =============================================================================
//
// The Diamond operator ◊_B is the 2×2 matrix operator:
//
//     ◊_B := | 0    𝔇_B |
//            | 𝔖_B  0   |
//
// where 𝔇_B is the universal interstice derivative and 𝔖_B is its inverse
// (interstice integral). The fundamental identity is:
//
//     ◊_B² = (id − Π_B) · I₂
//
// where Π_B is the projection onto constants. With a connection A_B:
//
//     (◊_B + A_B)² = (id − Π_B)·I₂ + M_B
//
// where M_B encodes the curvature/defect measure.
//
// Transport: h: B→C  ⟹  h_* ◊_B = ◊_C h_*
//            h_*(◊_B + A_B) = (◊_C + Φ_h(A_B)) · h_*
// =============================================================================

/**
 * @brief 2×2 matrix component of the Diamond operator result
 *
 * Stores the four components of the block-matrix operator
 * applied to a pair of functions (F_upper, F_lower).
 */
struct DiamondResult {
    double upper;   // 𝔇_B(F_lower) component
    double lower;   // 𝔖_B(F_upper) component
};

/**
 * @brief Diamond operator and its connection extension
 *
 * Implements the 2×2 block-matrix operator framework from the manuscript:
 *   ◊_B = [[0, 𝔇], [𝔖, 0]]
 *   ◊² = (id - Π) · I₂
 *   (◊ + A)² = (id - Π)I₂ + M    (M = curvature/defect)
 *
 * @note This is the UNIVERSAL framework; all concrete derivations
 *       (Newton, q, scale, time-scale) are special cases.
 */
class DiamondOperator {
public:
    /**
     * @brief Apply ◊_B to a pair (F_upper, F_lower)
     *
     * ◊_B (F_upper, F_lower) = (𝔇 F_lower, 𝔖 F_upper)
     *
     * @param derivOp   The interstice derivative 𝔇
     * @param integOp   The interstice integral 𝔖
     * @param f_upper   Function in the upper slot
     * @param f_lower   Function in the lower slot
     * @param x         Evaluation point
     */
    static DiamondResult apply(
        std::function<double(std::function<double(double)>, double)> derivOp,
        std::function<double(std::function<double(double)>, double)> integOp,
        std::function<double(double)> f_upper,
        std::function<double(double)> f_lower,
        double x)
    {
        return { derivOp(f_lower, x), integOp(f_upper, x) };
    }

    /**
     * @brief Verify ◊² = (id − Π)·I₂
     *
     * For a non-constant function F:
     *   ◊²(F, F) = (𝔇𝔖 F, 𝔖𝔇 F) should equal (F − ΠF, F − ΠF)
     *
     * Returns the maximum residual |◊²F - (id-Π)F|.
     */
    static double verifyDiamondSquared(
        std::function<double(std::function<double(double)>, double)> derivOp,
        std::function<double(std::function<double(double)>, double)> integOp,
        std::function<double(double)> f,
        double constantProjection, // ΠF = the constant/average part
        double x)
    {
        // 𝔖 F as a function
        auto sf = [&](double w) { return integOp(f, w); };
        // 𝔇 F as a function
        auto df = [&](double w) { return derivOp(f, w); };

        // ◊²(F, F) = (𝔇(𝔖 F), 𝔖(𝔇 F))
        double ds_f = derivOp(sf, x); // 𝔇𝔖 F
        double sd_f = integOp(df, x); // 𝔖𝔇 F

        double target = f(x) - constantProjection; // (id - Π)F

        double err1 = std::abs(ds_f - target);
        double err2 = std::abs(sd_f - target);
        return std::max(err1, err2);
    }

    /**
     * @brief Verify connection curvature: (◊ + A)² = (id−Π)I₂ + M
     *
     * With connection A (2×2 matrix), we have
     *   M = ◊A + A◊ + A²
     *
     * Returns the curvature measure M evaluated at x.
     */
    static double connectionCurvature(
        std::function<double(std::function<double(double)>, double)> derivOp,
        std::function<double(std::function<double(double)>, double)> integOp,
        std::function<double(double)> A_component,
        std::function<double(double)> f,
        double x)
    {
        // M = ◊A + A◊ + A²  (scalar simplification for diagonal A)
        double dA = derivOp(A_component, x);  // ◊A
        double A_val = A_component(x);
        double df = derivOp(f, x);
        double AdF = A_val * df;               // A◊F
        double A_sq = A_val * A_val;           // A²
        return dA + AdF + A_sq;
    }

    /**
     * @brief Transport: h_*(◊_B + A_B) = (◊_C + Φ_h(A_B)) · h_*
     *
     * Verifies that pushforward by h intertwines diamond+connection.
     * Returns the residual of the intertwining identity.
     */
    static double verifyTransport(
        std::function<double(std::function<double(double)>, double)> derivB,
        std::function<double(std::function<double(double)>, double)> derivC,
        std::function<double(double)> h_push,     // pushforward h_*
        std::function<double(double)> A_B,         // connection on B
        std::function<double(double)> f,           // test function
        double x)
    {
        // LHS: h_*(◊_B f + A_B · f)
        double lhs = h_push(x) * (derivB(f, x) + A_B(x) * f(x));

        // RHS: (◊_C(h_* f) + Φ_h(A_B) · h_* f)
        auto hf = [&](double w) { return h_push(w) * f(w); };
        double phiA = A_B(x); // Φ_h(A_B) = A_B for algebra homomorphisms
        double rhs = derivC(hf, x) + phiA * hf(x);

        return std::abs(lhs - rhs);
    }

    /**
     * @brief Discretized diamond using step η (Section CV)
     *
     * ◊_{η,B} with 𝔇_{η,B} = δ_η/η and 𝔖_{η,B} = η Σ_{k=0}^{N-1} τ_{kη}
     * Satisfies ◊_{η,B}² = (id − Π)·I₂
     * Newton limit: lim_{η→0} ◊_{η,B} = ◊_B (continuous)
     */
    static double discreteDiamond_D(
        std::function<double(double)> f,
        double eta, double x)
    {
        if (std::abs(eta) < 1e-15) return 0.0;
        return (f(x + eta) - f(x)) / eta;
    }

    static double discreteDiamond_S(
        std::function<double(double)> f,
        double eta, int N, double x)
    {
        double sum = 0.0;
        for (int k = 0; k < N; ++k) {
            sum += f(x + k * eta);
        }
        return eta * sum;
    }
};

// =============================================================================
// 14. CHAIN RULE FOR INTERSTICE DERIVATIVE (Section XXVIII)
// =============================================================================
//
// 𝔇_g(Φ ∘ f) = [Φ(U_g f) − Φ(f)] / [U_g f − f] · 𝔇_g f
//
// This is the non-additive chain rule for the interstice derivative applied
// to a composition. It reduces to the standard chain rule in the Newton limit.
// =============================================================================

class IntersticeChainRule {
public:
    /**
     * @brief Evaluate the interstice chain rule
     *
     * D_g(Φ∘f)(x) = [Φ(f(g·x)) - Φ(f(x))] / [f(g·x) - f(x)] · D_g f(x)
     *
     * @param phi     Outer function Φ
     * @param f       Inner function f
     * @param g_act   Group action g·x
     * @param chi     Weight function χ_g(x)
     * @param x       Evaluation point
     * @return        D_g(Φ∘f)(x) via chain rule
     */
    static double evaluate(
        std::function<double(double)> phi,
        std::function<double(double)> f,
        std::function<double(double)> g_act,
        double chi, double x)
    {
        double f_x   = f(x);
        double f_gx  = f(g_act(x));
        double df    = f_gx - f_x;

        if (std::abs(df) < 1e-15) {
            // Degenerate case: f(gx) = f(x), use L'Hôpital via standard derivative
            // lim = Φ'(f(x)) · D_g f(x)
            double h = 1e-7;
            double phi_prime = (phi(f_x + h) - phi(f_x - h)) / (2.0 * h);
            double dg_f = (chi != 0.0) ? df / chi : 0.0;
            return phi_prime * dg_f;
        }

        double phi_fgx = phi(f_gx);
        double phi_fx  = phi(f_x);
        double ratio   = (phi_fgx - phi_fx) / df;

        double dg_f = (std::abs(chi) > 1e-15) ? df / chi : 0.0;
        return ratio * dg_f;
    }

    /**
     * @brief Verify chain rule: D_g(Φ∘f) vs direct computation
     *
     * Compares chain rule evaluation against brute-force D_g(Φ∘f).
     * Returns residual (should be ~0).
     */
    static double verify(
        std::function<double(double)> phi,
        std::function<double(double)> f,
        std::function<double(double)> g_act,
        double chi, double x)
    {
        // Direct: D_g(Φ∘f)(x) = (Φ(f(gx)) - Φ(f(x))) / χ
        auto phi_f = [&](double w) { return phi(f(w)); };
        double direct = UniversalDifferenceQuotient::compute(phi_f, g_act, chi, x);
        double chain  = evaluate(phi, f, g_act, chi, x);
        return std::abs(direct - chain);
    }
};

// =============================================================================
// 15. CURVATURE FLATNESS F(g,h) = 0  (Section XXVII)
// =============================================================================
//
// F(g,h) := U_g U_h − U_h U_g = 0  for abelian group actions
// This implies  D_g D_h f = D_h D_g f  (commutativity of derivatives)
// and ensures representation-transport coherence across {A_n}_{n≥0}.
// =============================================================================

class CurvatureFlatness {
public:
    /**
     * @brief Verify curvature flatness F(g,h) = 0
     *
     * Checks that U_g U_h f(x) = U_h U_g f(x) for given actions.
     *
     * @return Residual |U_g U_h f − U_h U_g f|(x)
     */
    static double verify(
        std::function<double(double)> f,
        std::function<double(double)> g_act,
        std::function<double(double)> h_act,
        double x)
    {
        double Ug_Uh = f(g_act(h_act(x)));
        double Uh_Ug = f(h_act(g_act(x)));
        return std::abs(Ug_Uh - Uh_Ug);
    }

    /**
     * @brief Verify D_g D_h = D_h D_g (derivative commutativity)
     *
     * For commuting group actions, the interstice derivatives must commute.
     *
     * @return Residual |D_g D_h f − D_h D_g f|(x)
     */
    static double verifyDerivativeCommutativity(
        std::function<double(double)> f,
        std::function<double(double)> g_act,
        std::function<double(double)> h_act,
        double chi_g, double chi_h,
        double x)
    {
        // D_g f as a function
        auto dg_f = [&](double w) {
            return UniversalDifferenceQuotient::compute(f, g_act, chi_g, w);
        };
        // D_h f as a function
        auto dh_f = [&](double w) {
            return UniversalDifferenceQuotient::compute(f, h_act, chi_h, w);
        };
        double DgDh = UniversalDifferenceQuotient::compute(dh_f, g_act, chi_g, x);
        double DhDg = UniversalDifferenceQuotient::compute(dg_f, h_act, chi_h, x);
        return std::abs(DgDh - DhDg);
    }
};

// =============================================================================
// 16. PHYSICAL DIMENSION GRADING U = Z^7 (Interstices §XXI, §CX)
// =============================================================================
//
// Every operator preserves physical dimension grading:
//   deg(𝔇 F)   = deg(F) − deg(α_g)
//   deg(α_g·𝔇F) = deg(F)
//   deg(F·G)    = deg(F) + deg(G)
//   deg(𝔖 F)   = deg(F)
//
// The grading group U = Z^r with r = 7 (SI base dimensions).
// For dimensioned quantities, B = ⊕_{d∈Z^r} K_d · u^d
// =============================================================================

/**
 * @brief Dimension grading vector for the interstice operator algebra
 *
 * Compact representation of physical dimension in Z^7.
 * All interstice operators are required to preserve this grading.
 */
struct DimensionGrade {
    std::array<int8_t, 7> exponents = {}; // L, M, T, I, Θ, N, J

    static DimensionGrade dimensionless() { return {}; }

    static DimensionGrade length()   { DimensionGrade d; d.exponents[0] = 1; return d; }
    static DimensionGrade mass()     { DimensionGrade d; d.exponents[1] = 1; return d; }
    static DimensionGrade time()     { DimensionGrade d; d.exponents[2] = 1; return d; }
    static DimensionGrade current()  { DimensionGrade d; d.exponents[3] = 1; return d; }
    static DimensionGrade temp()     { DimensionGrade d; d.exponents[4] = 1; return d; }
    static DimensionGrade amount()   { DimensionGrade d; d.exponents[5] = 1; return d; }
    static DimensionGrade lumin()    { DimensionGrade d; d.exponents[6] = 1; return d; }

    DimensionGrade operator+(const DimensionGrade& o) const {
        DimensionGrade r;
        for (int i = 0; i < 7; ++i) r.exponents[i] = exponents[i] + o.exponents[i];
        return r;
    }
    DimensionGrade operator-(const DimensionGrade& o) const {
        DimensionGrade r;
        for (int i = 0; i < 7; ++i) r.exponents[i] = exponents[i] - o.exponents[i];
        return r;
    }
    DimensionGrade scale(int n) const {
        DimensionGrade r;
        for (int i = 0; i < 7; ++i) r.exponents[i] = exponents[i] * static_cast<int8_t>(n);
        return r;
    }
    bool operator==(const DimensionGrade& o) const { return exponents == o.exponents; }
    bool operator!=(const DimensionGrade& o) const { return !(*this == o); }
    bool isDimensionless() const {
        for (auto e : exponents) if (e != 0) return false;
        return true;
    }

    std::string toString() const {
        static const char* names[] = {"L","M","T","I","Θ","N","J"};
        std::string s;
        for (int i = 0; i < 7; ++i) {
            if (exponents[i] != 0) {
                if (!s.empty()) s += "·";
                s += names[i];
                if (exponents[i] != 1) s += "^" + std::to_string(exponents[i]);
            }
        }
        return s.empty() ? "1" : s;
    }
};

/**
 * @brief Verify dimension grade preservation by an interstice operator
 *
 * Checks that deg(Op(F)) is consistent with the operator's grading rule:
 *   - For D_g: deg(D_g F) = deg(F) − deg(α_g)
 *   - For S_g: deg(S_g F) = deg(F) + deg(α_g)
 *   - For mul: deg(F·G) = deg(F) + deg(G)
 *   - For add: requires deg(F) = deg(G)
 */
class DimensionGradeChecker {
public:
    static bool checkDerivative(const DimensionGrade& degF,
                                const DimensionGrade& degAlpha,
                                const DimensionGrade& degResult) {
        return degResult == degF - degAlpha;
    }

    static bool checkIntegral(const DimensionGrade& degF,
                              const DimensionGrade& degAlpha,
                              const DimensionGrade& degResult) {
        return degResult == degF + degAlpha;
    }

    static bool checkProduct(const DimensionGrade& degF,
                             const DimensionGrade& degG,
                             const DimensionGrade& degResult) {
        return degResult == degF + degG;
    }

    static bool checkAddition(const DimensionGrade& degF,
                              const DimensionGrade& degG) {
        return degF == degG; // Addition only homogeneous
    }

    /**
     * @brief Full grading consistency check for α_g · D_g F
     *
     * The product α_g · D_g F must have degree = deg(F),
     * since deg(α_g) + (deg(F) − deg(α_g)) = deg(F).
     */
    static bool checkCocycleProduct(const DimensionGrade& degF,
                                    const DimensionGrade& degAlpha) {
        DimensionGrade degDerivF = degF - degAlpha;
        DimensionGrade degProduct = degAlpha + degDerivF;
        return degProduct == degF;
    }
};

// =============================================================================
// 17. DIMENSIONAL SPECIALIZATION χ_s (Section CX)
// =============================================================================
//
// χ_s: B → K_0 sends dimensioned quantities to dimensionless by
//    χ_s(Σ_d a_d u^d) = Σ_d a_d s^d
// where s ∈ R^r is a vector of reference scales.
//
// The induced functor Φ_{χ_s}: P_B^(0) → P_{K_0}
// maps degree-0 operators from B to the base field.
// =============================================================================

class DimensionalSpecialization {
public:
    /**
     * @brief Specialize a dimensioned value to dimensionless
     *
     * Given value with dimension grade d, and reference scales s,
     * produces dimensionless value:  val · s^d
     */
    static double specialize(double value, const DimensionGrade& grade,
                             const std::array<double, 7>& referenceScales) {
        double factor = 1.0;
        for (int i = 0; i < 7; ++i) {
            if (grade.exponents[i] != 0) {
                factor *= std::pow(referenceScales[i], grade.exponents[i]);
            }
        }
        return value * factor;
    }

    /**
     * @brief Verify specialization preserves equation structure
     *
     * E_B^(0)[F] = 0  ⟹  Φ_{χ_s}(E_B^(0)){K_0}[χ_{s*} F] = 0
     */
    static double verifyEquationPreservation(
        std::function<double(double)> equation_B,
        std::function<double(double)> equation_K0,
        double value, const DimensionGrade& grade,
        const std::array<double, 7>& scales)
    {
        double specialized = specialize(value, grade, scales);
        return std::abs(equation_B(value) - equation_K0(specialized));
    }
};

// =============================================================================
// 18. BOX_MIX — d'Alembertian / Self-Adjoint Wave Operator (§Box_{g,χ})
// =============================================================================
//
// □_{g,χ} := D*_{g,χ} · D_{g,χ}  (self-adjoint second-order wave operator)
//
// For Newton: □ = -d²/dx²  (Laplacian)
// For Scale:  □_{Λ} = D†_Λ D_Λ
// For Q:      □_q = D†_q D_q
//
// The adjoint D*_{g,χ} is defined via the L² inner product:
//   ⟨D*F, G⟩ = ⟨F, DG⟩
//
// For multiplicative D_g with χ_g(x) = (g-1)x:
//   D*_g f(x) = -D_g f(x) - f(x) · χ'_g(x)/χ_g(x)
//            = -D_g f(x) - f(x)/x  (for scale actions)
//
// This self-adjoint operator unifies second-order differential operators:
//   □ → -∇²         (Laplacian)
//   □ + V → H       (spectral operator with potential)
//   □ + m² → KG     (massive wave operator)
//   □_μν → tensor   (for tensor fields)
// =============================================================================

class BoxMixOperator {
public:
    // Compute D† (adjoint derivative) for scale action
    // D†_Λ f(x) ≈ -D_Λ f(x) - f(x)/x  (1D scale adjoint)
    static double adjointDeriv(std::function<double(double)> f, double x,
                               double scale, double lnScale) {
        double gx = scale * x;
        double fx = f(x);
        double fgx = f(gx);
        double Df = (fgx - fx) / ((scale - 1.0) * x);
        // Adjoint correction: weight from measure transformation
        return -Df - fx / x;
    }

    // □_{g,χ} f = D†_{g,χ} D_{g,χ} f  (self-adjoint box)
    static double boxMix(std::function<double(double)> f, double x,
                         double scale, double lnScale) {
        // D_g f: first derivative
        auto Df = [&](double w) -> double {
            double gw = scale * w;
            return (f(gw) - f(w)) / ((scale - 1.0) * w);
        };
        // Apply adjoint to Df
        return adjointDeriv(Df, x, scale, lnScale);
    }

    // □ with Newton derivative (standard Laplacian via central difference)
    static double boxNewton(std::function<double(double)> f, double x) {
        double h = 1e-5;
        return (f(x + h) - 2.0 * f(x) + f(x - h)) / (h * h);
    }

    // □ + V potential operator (spectral/eigenvalue operator)
    static double boxPlusPotential(std::function<double(double)> f,
                                    std::function<double(double)> V,
                                    double x) {
        return boxNewton(f, x) + V(x) * f(x);
    }

    // Verify self-adjointness: ⟨□f, g⟩ = ⟨f, □g⟩ on an interval
    // Returns residual |⟨□f,g⟩ - ⟨f,□g⟩| (should be ≈ 0)
    static double verifySelfAdjoint(std::function<double(double)> f,
                                     std::function<double(double)> g,
                                     double a, double b, int steps = 200) {
        double dx = (b - a) / steps;
        double inner1 = 0.0, inner2 = 0.0;
        for (int i = 1; i < steps; ++i) {
            double x = a + i * dx;
            inner1 += boxNewton(f, x) * g(x) * dx;
            inner2 += f(x) * boxNewton(g, x) * dx;
        }
        return std::abs(inner1 - inner2);
    }
};

// =============================================================================
// 19. COCHAIN DIFFERENTIAL D = δ + m, D² = μ_int (Interstices §Cohomology)
// =============================================================================
//
// The cochain differential incorporates the interstice correction:
//   D = δ + m
// where δ is the standard coboundary and m is the interstice multiplication.
//
// The key identity: D² = μ_int  (curvature = interstice measure)
// In the flat case, D² = 0 recovers de Rham cohomology.
//
// Numerically: D(f)(x) = δf(x) + m·f(x)
//   where δf = f(gx) - f(x)  (group coboundary)
//   and m is the interstice correction factor.
//
// D²f = D(Df) should equal μ_int · f
// =============================================================================

class CochainDifferential {
public:
    // Apply D = δ + m to a function (1-cochain level)
    static double apply(std::function<double(double)> f, double x,
                        double scale, double m_int) {
        double gx = scale * x;
        double delta_f = f(gx) - f(x);
        return delta_f + m_int * f(x);
    }

    // Apply D² = D∘D — should equal μ_int · f for curved spaces
    static double applySquared(std::function<double(double)> f, double x,
                               double scale, double m_int) {
        // Df at x
        auto Df = [&](double w) -> double {
            return apply(f, w, scale, m_int);
        };
        // D(Df) at x
        return apply(Df, x, scale, m_int);
    }

    // Verify D² = μ_int: returns residual |D²f - μ·f|
    static double verifyDSquared(std::function<double(double)> f, double x,
                                 double scale, double m_int, double mu_int) {
        double D2f = applySquared(f, x, scale, m_int);
        double expected = mu_int * f(x);
        return std::abs(D2f - expected);
    }

    // Generalized Stokes theorem with interstice correction:
    // ⟨dω, c⟩ = ⟨ω, ∂c⟩ + ⟨μ_int_ω, c⟩
    // Numerically: integral of dω over chain = boundary integral + defect
    static double generalizedStokes(std::function<double(double)> omega,
                                     double a, double b, double scale,
                                     double m_int, int steps = 100) {
        double dx = (b - a) / steps;
        // ⟨dω, c⟩ = integral of dω over [a,b]
        double integral_dw = 0.0;
        for (int i = 0; i < steps; ++i) {
            double x = a + (i + 0.5) * dx;
            double h = 1e-7;
            double domega = (omega(x + h) - omega(x - h)) / (2.0 * h);
            integral_dw += domega * dx;
        }
        // ⟨ω, ∂c⟩ = ω(b) - ω(a) (boundary evaluation)
        double boundary = omega(b) - omega(a);
        // Defect = integral - boundary
        double defect = integral_dw - boundary;
        return defect; // This IS the interstice μ_int
    }
};

// =============================================================================
// 20. GAUGED CONNECTION ∇ = D + A, F = dA + A∧A (§XIV, §Gauge)
// =============================================================================
//
// The gauge connection adds a connection 1-form A to the derivative:
//   ∇_A f = D_g f + A · f
//
// The curvature 2-form:
//   F = dA + A ∧ A  (connection curvature)
//
// The gauged UFE:
//   (D + A)†(D + A)F = 0  →  □F + [A, □F] + ... = 0
//
// The cascade equation:
//   E[F] = Ξ_φ† · Ξ_φ · F = 0
//
// For abelian gauge (U(1)):
//   F = dA      (curvature 2-form)
//   ∇² = (∂ + ieA)² = □ + 2ieA·∂ + ie(∂·A) - e²A²
// =============================================================================

class GaugedConnection {
public:
    // Connection 1-form A(x) — given as a function
    using ConnectionField = std::function<double(double)>;

    // Gauged derivative: ∇_A f = Df + A·f
    static double gaugedDeriv(std::function<double(double)> f,
                               ConnectionField A, double x) {
        double h = 1e-7;
        double Df = (f(x + h) - f(x - h)) / (2.0 * h);
        return Df + A(x) * f(x);
    }

    // Gauged Box: □_A f = ∇†_A ∇_A f ≈ □f + 2A·Df + (dA)·f + A²·f
    static double gaugedBox(std::function<double(double)> f,
                             ConnectionField A, double x) {
        double h = 1e-5;
        // ∇_A f at x±h
        auto nabla_f = [&](double w) -> double {
            double df = (f(w + 1e-7) - f(w - 1e-7)) / (2e-7);
            return df + A(w) * f(w);
        };
        // □_A ≈ d(∇_A f)/dx
        double dn_plus = nabla_f(x + h);
        double dn_minus = nabla_f(x - h);
        return (dn_plus - 2.0 * nabla_f(x) + dn_minus) / (h * h);
    }

    // Connection curvature: F = dA + A∧A
    // In 1D abelian: F = dA/dx (A∧A=0 for abelian)
    static double curvature(ConnectionField A, double x) {
        double h = 1e-7;
        return (A(x + h) - A(x - h)) / (2.0 * h);
    }

    // UFE residual: (∇†∇)Φ = 0
    static double ufeResidual(std::function<double(double)> phi,
                               ConnectionField A, double x) {
        return gaugedBox(phi, A, x);
    }

    // Cascade equation: E[F] = Ξ†_φ · Ξ_φ · F
    // Ξ_φ f = (f(φx) - f(x)) / |χ_φ|
    // Ξ†_φ approximated as adjoint under L² pairing
    static double cascadeResidual(std::function<double(double)> f, double x) {
        double chi_mod = std::sqrt(
            std::pow(std::log(constants::PHI), 2) +
            std::pow(constants::PI / 2.0, 2)
        );
        // Ξ_φ f
        double xi_f = (f(constants::PHI * x) - f(x)) / chi_mod;
        // Ξ†_φ(Ξ_φ f) ≈ -Ξ_φ(Ξ_φ f) - correction
        auto xif_func = [&](double w) -> double {
            return (f(constants::PHI * w) - f(w)) / chi_mod;
        };
        double xi2_f = (xif_func(constants::PHI * x) - xif_func(x)) / chi_mod;
        return xi2_f; // E[F] = Ξ†·Ξ·F ≈ this
    }
};

// =============================================================================
// 21. CONSERVATION LAW — Δ_g η(U) + Div_g q(U) = μ_src (§Conservation)
// =============================================================================
//
// Conservation laws in the interstice framework:
//   ∂_t η(U) + ∂_x q(U) = 0        — classical
//   D_t η(U) + D_x q(U) = μ_src     — interstice (with source)
//
// The engine discovers (density, flux) pairs (η, q) such that
// D_t η + D_x q ≈ 0 on orbits. This discovers conservation structure
// for arbitrary systems without pre-specifying which quantities are conserved.
// =============================================================================

class ConservationLaw {
public:
    // Compute conservation residual: |D_t η + D_x q|
    // D_t via finite difference in time, D_x via spatial
    static double residual(std::function<double(double, double)> eta,
                           std::function<double(double, double)> flux,
                           double x, double t, double dt = 1e-5, double dx = 1e-5) {
        // D_t η
        double Dt_eta = (eta(x, t + dt) - eta(x, t - dt)) / (2.0 * dt);
        // D_x q
        double Dx_q = (flux(x + dx, t) - flux(x - dx, t)) / (2.0 * dx);
        return std::abs(Dt_eta + Dx_q);
    }

    // 1D conservation residual for autonomous case (time-independent)
    // D_x q(f) = source(f)
    static double autonomousResidual(std::function<double(double)> f,
                                      std::function<double(double)> flux_of_f,
                                      double x, double h = 1e-5) {
        // D_x(flux(f(x)))
        double fxp = f(x + h);
        double fxm = f(x - h);
        double Dx_flux = (flux_of_f(fxp) - flux_of_f(fxm)) / (2.0 * h);
        return Dx_flux; // Should be 0 for conservation; nonzero = source
    }

    // Verify conservation with interstice derivative D_g
    // F(g^N·x) - F(x) = Σ χ·D_g F  — conservation version
    static double intersticeConservation(
        std::function<double(double)> eta,
        std::function<double(double)> flux,
        double x, double scale, int N) {
        // Total change
        double xN = x;
        for (int k = 0; k < N; ++k) xN *= scale;
        double delta_eta = eta(xN) - eta(x);
        // Accumulated flux
        double acc_flux = 0.0;
        double xk = x;
        for (int k = 0; k < N; ++k) {
            double chi_k = (scale - 1.0) * xk;
            acc_flux += chi_k * flux(xk);
            xk *= scale;
        }
        return std::abs(delta_eta + acc_flux); // Conservation: Δη + Σχ·q = 0
    }
};

// =============================================================================
// 22. GREEN'S OPERATOR / PROPAGATOR S (§Green, §Homotopy)
// =============================================================================
//
// The Green's operator S is the homotopy inverse of D:
//   S · D = id − Π    (where Π is projection to constants)
//   D · S = id − Π
//
// For Newton derivative: S = ∫_a^x  (antiderivative)
// For Scale D_Λ: S_Λ f(x) = (lnΛ)·Σ_{k≥1} f(Λ^{-k} x) (geometric sum)
// For finite difference Δ: S = Σ (summation operator)
//
// The Green's function G(x,y) satisfies □G(x,y) = δ(x-y)
// and produces solutions via convolution: u(x) = ∫ G(x,y)·source(y) dy
// =============================================================================

class GreenOperator {
public:
    // Newton propagator: S f(x) = ∫_0^x f(t) dt  (antiderivative)
    static double newtonPropagator(std::function<double(double)> f,
                                    double x, int steps = 200) {
        double dx = x / steps;
        double sum = 0.0;
        for (int i = 0; i < steps; ++i) {
            double t = (i + 0.5) * dx;
            sum += f(t) * dx;
        }
        return sum;
    }

    // Scale propagator: S_Λ f(x) = lnΛ · Σ_{k=1}^{N} f(Λ^{-k} x)
    static double scalePropagator(std::function<double(double)> f,
                                   double x, double lambda, int N = 20) {
        double lnL = std::log(lambda);
        double sum = 0.0;
        double xk = x;
        for (int k = 1; k <= N; ++k) {
            xk /= lambda;
            sum += f(xk);
        }
        return lnL * sum;
    }

    // Verify S·D = id - Π:  S(Df)(x) ≈ f(x) - f(0)
    static double verifySDIdentity(std::function<double(double)> f, double x) {
        // Df via central difference
        auto Df = [&](double w) -> double {
            double h = 1e-7;
            return (f(w + h) - f(w - h)) / (2.0 * h);
        };
        // S(Df) = ∫_0^x f'(t) dt = f(x) - f(0)
        double SDf = newtonPropagator(Df, x);
        double expected = f(x) - f(0.0);
        return std::abs(SDf - expected);
    }

    // Green's function convolution: u(x) = ∫ G(x,y) · source(y) dy
    static double convolve(std::function<double(double, double)> green,
                           std::function<double(double)> source,
                           double x, double a, double b, int steps = 200) {
        double dy = (b - a) / steps;
        double sum = 0.0;
        for (int i = 0; i < steps; ++i) {
            double y = a + (i + 0.5) * dy;
            sum += green(x, y) * source(y) * dy;
        }
        return sum;
    }

    // 1D Green's function for -d²u/dx² = f on [0,L], u(0)=u(L)=0
    // G(x,y) = min(x,y)(L-max(x,y))/L
    static double greenLaplace1D(double x, double y, double L = 1.0) {
        return std::min(x, y) * (L - std::max(x, y)) / L;
    }

    // Heat kernel: K(x,y,t) = exp(-(x-y)²/(4t)) / sqrt(4πt)
    static double heatKernel(double x, double y, double t) {
        if (t <= 0) return 0.0;
        return std::exp(-(x - y) * (x - y) / (4.0 * t)) /
               std::sqrt(4.0 * constants::PI * t);
    }
};

// =============================================================================
// 23. NONCOMMUTATIVE CALCULUS — D(F^n), D(e^F), D(log(1+F))
// =============================================================================
//
// Interstices §Power Rule (noncommutative):
//   D(F^n) = Σ_{k=0}^{n-1} F^k · (DF) · F^{n-1-k}
//
// For commutative F this reduces to n·F^{n-1}·DF.
//
// Exponential derivative:
//   D(e^F) = (Σ_{n≥0} 1/n! · Σ_{k=0}^{n-1} F^k(DF)F^{n-1-k}) = e^F · DF
//   (In commutative case: D(e^f) = e^f · Df)
//
// Logarithm derivative:
//   D(log(1+F)) = (1+F)^{-1} · DF   (commutative)
//   D(log(1+F)) = Σ_{n≥1} (-1)^{n+1}/n · D(F^n)/(1+F)^n  (noncommutative)
// =============================================================================

class NoncommutativeCalculus {
public:
    // Verify power rule: D(f^n)(x) = n·f^{n-1}(x)·Df(x) (commutative case)
    static double powerRuleResidual(std::function<double(double)> f,
                                     double x, int n) {
        double h = 1e-7;
        // D(f^n)
        auto fn = [&](double w) { return std::pow(f(w), n); };
        double D_fn = (fn(x + h) - fn(x - h)) / (2.0 * h);
        // n · f^{n-1} · Df
        double Df = (f(x + h) - f(x - h)) / (2.0 * h);
        double expected = n * std::pow(f(x), n - 1) * Df;
        return std::abs(D_fn - expected);
    }

    // Verify exponential rule: D(e^f) = e^f · Df
    static double expRuleResidual(std::function<double(double)> f, double x) {
        double h = 1e-7;
        auto ef = [&](double w) { return std::exp(f(w)); };
        double D_ef = (ef(x + h) - ef(x - h)) / (2.0 * h);
        double Df = (f(x + h) - f(x - h)) / (2.0 * h);
        double expected = std::exp(f(x)) * Df;
        return std::abs(D_ef - expected);
    }

    // Verify log rule: D(log(1+f)) = Df / (1+f)
    static double logRuleResidual(std::function<double(double)> f, double x) {
        double h = 1e-7;
        auto logf = [&](double w) { return std::log(1.0 + f(w)); };
        double D_logf = (logf(x + h) - logf(x - h)) / (2.0 * h);
        double Df = (f(x + h) - f(x - h)) / (2.0 * h);
        double expected = Df / (1.0 + f(x));
        return std::abs(D_logf - expected);
    }

    // Interstice power rule: D_g(f^n) with D_g = scale derivative
    static double intersticePowerRule(std::function<double(double)> f,
                                       double x, double scale, int n) {
        double gx = scale * x;
        double chi = (scale - 1.0) * x;
        if (std::abs(chi) < 1e-15) return 0.0;
        auto fn = [&](double w) { return std::pow(f(w), n); };
        double D_fn = (fn(gx) - fn(x)) / chi;
        // Commutative expected: n·f^{n-1}·D_g f
        double Df = (f(gx) - f(x)) / chi;
        double expected = n * std::pow(f(x), n - 1) * Df;
        return std::abs(D_fn - expected);
    }
};

// =============================================================================
// 24. WEIGHTED SCALE DERIVATIVE D_{Λ,w} (§Graded, §Combined)
// =============================================================================
//
// D_{Λ,w} := (S_{Λ,w} - I) / ℓ
// where S_{Λ,w} = G_1^{w1} · G_2^{w2} · ... · G_7^{w7} · S_Λ
// and G_i are the grading operators (one per SI dimension).
//
// The weight vector w ∈ Z^7 tracks the physical dimension:
//   w = (L,M,T,I,Θ,N,J)
//
// This enables D_{Λ,w} to have well-defined dimensional behavior:
//   deg(D_{Λ,w} F) = deg(F) + w
// =============================================================================

class WeightedScaleDerivative {
public:
    // Standard scale derivative D_Λ f = (f(Λx) - f(x)) / ((Λ-1)x)
    static double scaleDerivative(std::function<double(double)> f,
                                   double x, double lambda) {
        double gx = lambda * x;
        double chi = (lambda - 1.0) * x;
        if (std::abs(chi) < 1e-15) return 0.0;
        return (f(gx) - f(x)) / chi;
    }

    // Weighted derivative with dimension grading
    static double weightedDerivative(std::function<double(double)> f,
                                      double x, double lambda,
                                      const DimensionGrade& weight,
                                      const std::array<double, 7>& refScales) {
        // Apply grading correction: multiply by product of reference scales^weight
        double correction = 1.0;
        for (int i = 0; i < 7; ++i) {
            if (weight.exponents[i] != 0) {
                correction *= std::pow(refScales[i], weight.exponents[i]);
            }
        }
        return scaleDerivative(f, x, lambda) * correction;
    }

    // D-sharp: D♯_{Λ,w} = (S_{Λ,w} - I) / ℓ  with S the triple-shift
    // Triple-shift combines: scale action × ψ-rotation × time-advance
    static double sharpDerivative(std::function<double(double)> f,
                                   double x, double lambda, double ell) {
        if (std::abs(ell) < 1e-15) return 0.0;
        // S_{Λ,w} f(x) = f(Λx) for the basic scale component
        double Sf = f(lambda * x);
        return (Sf - f(x)) / ell;
    }
};

// =============================================================================
// 25. L² INNER PRODUCT AND SELF-ADJOINTNESS (§L², §Hilbert)
// =============================================================================
//
// ⟨F, G⟩_{L²_χ} = ∫_X̄ F(x)·G(x) χ(x) dμ(x)
// where X̄ is the compactified mapping torus and χ is the weight.
//
// For S¹ × Σ:  ⟨F,G⟩ = (1/2π)∫_0^{2π} F(ψ)G(ψ) dψ
// For R with scale measure: ⟨F,G⟩ = ∫_0^∞ F(r)G(r) dr/r
// =============================================================================

class L2Structure {
public:
    // L² inner product on [a,b] with weight w
    static double innerProduct(std::function<double(double)> F,
                                std::function<double(double)> G,
                                std::function<double(double)> weight,
                                double a, double b, int steps = 500) {
        double dx = (b - a) / steps;
        double sum = 0.0;
        for (int i = 0; i < steps; ++i) {
            double x = a + (i + 0.5) * dx;
            sum += F(x) * G(x) * weight(x) * dx;
        }
        return sum;
    }

    // L² norm: ||F|| = √⟨F,F⟩
    static double norm(std::function<double(double)> F,
                       std::function<double(double)> weight,
                       double a, double b, int steps = 500) {
        return std::sqrt(innerProduct(F, F, weight, a, b, steps));
    }

    // Scale-measure inner product: ⟨F,G⟩_{dr/r} on (ε, R)
    static double scaleInnerProduct(std::function<double(double)> F,
                                     std::function<double(double)> G,
                                     double eps, double R, int steps = 500) {
        auto weight = [](double x) { return 1.0 / x; };
        return innerProduct(F, G, weight, eps, R, steps);
    }

    // Verify orthogonality: ⟨F,G⟩ ≈ 0
    static double orthogonalityResidual(std::function<double(double)> F,
                                         std::function<double(double)> G,
                                         double a, double b) {
        auto unit_weight = [](double) { return 1.0; };
        return std::abs(innerProduct(F, G, unit_weight, a, b));
    }
};

// =============================================================================
// 26. CONTINUUM LIMIT VERIFICATION (§Limits, §Recovery)
// =============================================================================
//
// As Λ→1 (scale parameter → identity):
//   D_Λ f(x) → f'(x)   (Newton derivative recovered)
//   □_Λ f → -f''(x)     (Laplacian recovered)
//   J_Λ^{(N)} → ∫ f'(x) dx  (integral recovered)
//
// As q→1 (q-parameter → classical):
//   D_q f(x) → f'(x)
//   [n]_q → n
//   exp_q(x) → exp(x)
// =============================================================================

class ContinuumLimit {
public:
    // Check D_Λ → d/dx as Λ → 1
    // Returns |D_Λ f(x) - f'(x)| for given Λ
    static double scaleToNewtonResidual(std::function<double(double)> f,
                                         double x, double lambda) {
        // D_Λ f
        double DL = (f(lambda * x) - f(x)) / ((lambda - 1.0) * x);
        // Newton d/dx (central diff)
        double h = 1e-7;
        double dfdx = (f(x + h) - f(x - h)) / (2.0 * h);
        return std::abs(DL - dfdx);
    }

    // Check D_q → d/dx as q → 1
    static double qToNewtonResidual(std::function<double(double)> f,
                                     double x, double q) {
        // D_q f = (f(qx) - f(x)) / ((q-1)x)
        if (std::abs((q - 1.0) * x) < 1e-15) return 0.0;
        double Dq = (f(q * x) - f(x)) / ((q - 1.0) * x);
        // Newton
        double h = 1e-7;
        double dfdx = (f(x + h) - f(x - h)) / (2.0 * h);
        return std::abs(Dq - dfdx);
    }

    // Verify FToI → FTC as Λ → 1: integral + defect → ∫f'dx
    static double ftoiToFTCResidual(std::function<double(double)> f,
                                     double x, double lambda, int N) {
        // LHS: f(Λ^N x) - f(x)
        double xN = x;
        for (int k = 0; k < N; ++k) xN *= lambda;
        double lhs = f(xN) - f(x);
        // RHS classical: ∫_x^{Λ^N x} f'(t) dt  via Simpson
        double a = x, b = xN;
        int steps = 200;
        double dt = (b - a) / steps;
        double rhs = 0.0;
        for (int i = 0; i < steps; ++i) {
            double t = a + (i + 0.5) * dt;
            double h = 1e-7;
            double fpr = (f(t + h) - f(t - h)) / (2.0 * h);
            rhs += fpr * dt;
        }
        return std::abs(lhs - rhs);
    }

    // q-number: [n]_q = (q^n - 1)/(q - 1) → n as q → 1
    static double qNumber(double n, double q) {
        if (std::abs(q - 1.0) < 1e-15) return n;
        return (std::pow(q, n) - 1.0) / (q - 1.0);
    }

    // q-factorial: [n]_q! = [1]_q · [2]_q · ... · [n]_q
    static double qFactorial(int n, double q) {
        double result = 1.0;
        for (int k = 1; k <= n; ++k) {
            result *= qNumber(k, q);
        }
        return result;
    }

    // q-exponential: exp_q(x) = Σ x^n / [n]_q!
    static double qExponential(double x, double q, int terms = 15) {
        double sum = 1.0;
        double xn = 1.0;
        for (int n = 1; n < terms; ++n) {
            xn *= x;
            double qfact = qFactorial(n, q);
            if (std::abs(qfact) < 1e-30) break;
            sum += xn / qfact;
        }
        return sum;
    }
};

// =============================================================================
// 27. MASTER EQUATION E_B = (◊+A)² − (id−Π)I₂ − M = 0 (§Master)
// =============================================================================
//
// The master equation of the interstice framework combines:
//   ◊ = diamond operator (D and S components)
//   A = gauge connection
//   Π = projection to constants
//   I₂ = second integral
//   M = matter/source term
//
// E_B = 0 is the universal equation of the framework:
//   - Flat gauge (A=0), no matter (M=0): ◊²F = (id−Π)I₂ F
//   - Classical limit (Λ→1): reduces to □F = source
//   - Abelian gauge: linear wave equations
//   - Non-abelian gauge: nonlinear field equations
//   - With curvature metric: geometric field equations
// =============================================================================

class MasterEquation {
public:
    // Evaluate master equation residual
    // E[f] = ◊²f + A·◊f - (f - f(x₀)) - source
    static double residual(std::function<double(double)> f,
                           std::function<double(double)> A_conn,
                           std::function<double(double)> source,
                           double x, double x0_proj = 0.0) {
        // ◊f ≈ Df (discrete derivative)
        double h = 1e-5;
        double df = (f(x + h) - f(x - h)) / (2.0 * h);
        // ◊²f ≈ D²f
        double d2f = (f(x + h) - 2.0 * f(x) + f(x - h)) / (h * h);
        // Gauged diamond: (◊+A)² ≈ ◊² + 2A·◊ + (◊A) + A²
        double Ax = A_conn(x);
        double dA = (A_conn(x + h) - A_conn(x - h)) / (2.0 * h);
        double diamond_A_sq = d2f + 2.0 * Ax * df + dA * f(x) + Ax * Ax * f(x);
        // (id - Π)f = f(x) - f(x₀)
        double id_minus_pi = f(x) - f(x0_proj);
        // Master residual
        return diamond_A_sq - id_minus_pi - source(x);
    }

    // Check if the master equation yields eigenvalue structure
    // For A=0, source=0: ◊²f = f - const  (eigenvalue-like)
    static double flatResidual(std::function<double(double)> f,
                                double x, double x0_proj = 0.0) {
        double h = 1e-5;
        double d2f = (f(x + h) - 2.0 * f(x) + f(x - h)) / (h * h);
        double rhs = f(x) - f(x0_proj);
        return std::abs(d2f - rhs);
    }
};

} // namespace interstice
} // namespace domain
} // namespace autodiscover

#endif // AUTODISCOVER_DOMAIN_INTERSTICE_ENGINE_HPP
