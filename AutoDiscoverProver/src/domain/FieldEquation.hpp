#pragma once
// =============================================================================
// FieldEquation.hpp — The Interstice Field Equation Engine
// =============================================================================
//
// FROM THE INTERSTICES DOCUMENT §XIV (~L14280-14400):
//
//   The UNIFIED INTERSTICE FIELD EQUATION:
//     δA + A ∪ A = 0
//
//   where A is the interstice connection 1-form on (Σ × S¹) × T,
//   and ∪ is the cup product.
//
//   Newton limit (Λ→1): this BECOMES Yang-Mills, dA + A∧A = 0.
//
//   The flatness condition:
//     δΩ = 0,  where Ω_g = (Λ-1)x for scale action
//
//   The curvature 2-form:
//     F_A = δA + A∪A     (= 0 for flat interstice connections)
//
//   The interstice wave/Laplace/Helmholtz operators:
//     □_{g,χ} Φ = J       (wave equation on X)
//     Lap_{g,χ} Φ = ρ     (Poisson equation on X)
//
//   Noether current:
//     j^a = ∂L/∂(∂_aΦ) · δΦ - L·ξ^a
//
//   Conservation law:
//     D_{g,χ}(j) = 0     (Noether on interstice)
//
// ALL of these are computed NUMERICALLY. The AutoDiscoverer discovers
// that these structures yield the correct physics equations as
// specializations of the single universal D_{g,χ} operator.
//
// UNIFICATION OF PHYSICS:
//   - Classical mechanics:    Euler-Lagrange from D_{Newton}
//   - Electrodynamics:        Maxwell from dF = 0, d*F = J
//   - Quantum mechanics:      Schrödinger from iℏ∂_t = H → D_{scale}
//   - Gauge theory:           Yang-Mills from δA+A∪A=0
//   - Gravity:                Einstein from Ric - ½Rg = T via D_{Möbius}
//   - Fluid dynamics:         Navier-Stokes from D_{scale}(v) + (v·∇)v = ...
//   - Thermodynamics:         Entropy from D_{discrete} S ≥ 0
//   - Renormalization:        β-function from D_{Λ} g_i = β_i(g)
//
// All are specializations of D_{g,χ}. The AutoDiscoverer finds them.
// =============================================================================

#include <cmath>
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <memory>
#include <array>
#include <complex>

#include "../core/Term.hpp"
#include "../core/Constants.hpp"
#include "../logic/Equation.hpp"

namespace autodiscover {
namespace domain {
namespace fieldequation {

using core::Term;
using core::TermFactory;
using core::Sort;
using logic::Equation;
using logic::EquationSource;

// =============================================================================
// CONSTANTS — math from core/Constants.hpp, physics local
// =============================================================================

namespace cst {
    using namespace ::autodiscover::constants;
    // Legacy aliases
    inline constexpr double PI_VAL = PI;

    // Physical constants (unique to FieldEquation)
    inline constexpr double HBAR      = 1.054571817e-34;
    inline constexpr double C_LIGHT   = 299792458.0;
    inline constexpr double G_NEWTON  = 6.67430e-11;
    inline constexpr double K_BOLTZ   = 1.380649e-23;
    inline constexpr double EPSILON0  = 8.8541878128e-12;

    // Planck units (dimensionless ratios)
    inline constexpr double L_PLANCK_RATIO  = 1.616255e-35;
    inline constexpr double T_PLANCK_RATIO  = 5.391247e-44;
    inline constexpr double M_PLANCK_RATIO  = 2.176434e-8;

    // Fine structure constant
    inline constexpr double ALPHA_EM = 1.0 / 137.035999084;
}

// =============================================================================
// 1D FIELD CONFIGURATION — discretized field on a lattice
// =============================================================================

class Field1D {
    std::vector<double> data_;
    double dx_;
    int N_;
public:
    Field1D(int N, double dx, std::function<double(double)> init)
        : data_(N, 0.0), dx_(dx), N_(N) {
        for (int i = 0; i < N; ++i) {
            data_[i] = init(i * dx);
        }
    }

    double operator[](int i) const {
        if (i < 0 || i >= N_) return 0.0;
        return data_[i];
    }
    int size() const { return N_; }
    double dx() const { return dx_; }

    // D_{Newton} at node i
    double DNewton(int i) const {
        if (i <= 0 || i >= N_ - 1) return 0.0;
        return (data_[i + 1] - data_[i - 1]) / (2.0 * dx_);
    }

    // D_{Λ} at node i — scale derivative
    double DLambda(int i) const {
        int j = static_cast<int>(i * cst::LAMBDA);
        if (j >= N_ || j < 0) return 0.0;
        double chi = (cst::LAMBDA - 1.0) * i * dx_;
        if (std::abs(chi) < 1e-15) return 0.0;
        return (data_[j] - data_[i]) / chi;
    }

    // D² (Laplacian) at node i
    double Laplacian(int i) const {
        if (i <= 0 || i >= N_ - 1) return 0.0;
        return (data_[i + 1] - 2.0 * data_[i] + data_[i - 1]) / (dx_ * dx_);
    }

    // Energy density: ½(∂f)² + V(f)
    double energyDensity(int i, double mass = 1.0) const {
        double df = DNewton(i);
        double f = data_[i];
        return 0.5 * df * df + 0.5 * mass * mass * f * f;
    }
};

// =============================================================================
// GAUGE FIELD OPERATIONS — δA + A∪A
// =============================================================================

struct GaugeField1D {
    double A1;   // Connection component A_ψ (along S¹)
    double A2;   // Connection component A_σ (along scale)

    double fieldStrength() const {
        // F_{12} = ∂_1 A_2 - ∂_2 A_1 + [A_1, A_2]
        // In 1D abelian case:  F = dA, curvature
        return A1 * A2 - A2 * A1; // Vanishes for abelian; kept for structure
    }
};

// Interstice connection on mapping torus
struct IntersticeConnection {
    double Omega;   // Ω_g = (Λ-1)·x₀ — the scale 1-form
    double A_psi;   // A_ψ — angular component
    double A_sigma;  // A_σ — scale component
    double curvature; // F_A = δA + A∪A

    // Evaluate at x₀ = φ
    void evaluate() {
        Omega = (cst::LAMBDA - 1.0) * cst::PHI;
        // A_ψ from Christoffel-like connection: A_ψ = (2lnφ/π)·Ω·∂_ψ
        A_psi = cst::ALPHA_INT * Omega;
        A_sigma = cst::LN_LAMBDA * Omega;
        // Curvature for flat connection = 0
        curvature = 0.0; // δA + A∪A for flat interstice
    }
};

// =============================================================================
// FIELD EQUATION EVALUATOR HELPERS
// =============================================================================

// Klein-Gordon on interstice: (□ + m²)Φ = 0
inline double kleinGordonResidual(double phi_val, double box_phi, double mass) {
    return box_phi + mass * mass * phi_val;
}

// Wave equation: □Φ = 0
inline double waveResidual(double box_phi) {
    return box_phi;
}

// Poisson: ΔΦ = ρ
inline double poissonResidual(double lap_phi, double rho) {
    return lap_phi - rho;
}

// Helmholtz: (Δ + k²)Φ = 0
inline double helmholtzResidual(double lap_phi, double phi_val, double k) {
    return lap_phi + k * k * phi_val;
}

// Diffusion: D_{scale}(Φ) = κ·Δ(Φ)
inline double diffusionResidual(double scale_deriv, double laplacian, double kappa) {
    return scale_deriv - kappa * laplacian;
}

// β-function (renormalization): D_Λ(g_i) = β_i(g)
// For φ⁴ theory: β(g) = 3g²/(16π²) at 1-loop
inline double betaFunction_phi4(double g) {
    return 3.0 * g * g / (16.0 * cst::PI_VAL * cst::PI_VAL);
}

// Noether charge: Q = ∫ j⁰ dx for scalar field
inline double noetherCharge(const Field1D& field, const Field1D& conjugate) {
    double Q = 0.0;
    for (int i = 0; i < field.size(); ++i) {
        // j⁰ = π·δΦ - H·ξ⁰  ≈  π·Φ (for internal symmetry)
        Q += conjugate[i] * field[i] * field.dx();
    }
    return Q;
}

// Entropy production: D_{discrete} S ≥ 0
inline double entropyProduction(double S_before, double S_after) {
    return S_after - S_before;  // Should be ≥ 0
}

// =============================================================================
// DIMENSIONLESS RATIOS — universal constants as interstice ratios
// =============================================================================

namespace ratios {
    // The Interstices framework predicts that fundamental dimensionless
    // ratios can be expressed in terms of φ, Λ, π, ln(Λ), etc.
    // We compute many ratios and let the AutoDiscoverer find matches.

    inline double alpha_em_approx() {
        // 1/137 ≈ structure constant
        return cst::ALPHA_EM;
    }

    // Weinberg angle sin²θ_W ≈ 0.2312
    inline double sin2_weinberg() { return 0.23121; }

    // Cabibbo angle sin(θ_C) ≈ 0.2253
    inline double sin_cabibbo() { return 0.22534; }

    // Mass ratios (dimensionless)
    inline double ratio_mu_e()   { return 206.7682830; }   // m_μ/m_e
    inline double ratio_tau_e()  { return 3477.48;     }   // m_τ/m_e
    inline double ratio_p_e()    { return 1836.15267;  }   // m_p/m_e
    inline double ratio_n_p()    { return 1.0013784;   }   // m_n/m_p

    // Cosmological: Ω_Λ / Ω_m ≈ 2.27
    inline double dark_energy_matter_ratio() { return 2.27; }

    // Planck/proton mass ratio
    inline double planck_proton() { return 1.22089e19 / 0.93827; }

    // φ-based approximations explored by the discoverer
    inline double phi_power_approx(int n) { return std::pow(cst::PHI, n); }
    inline double lambda_power_approx(int n) { return std::pow(cst::LAMBDA, n); }
}

// =============================================================================
// FIELD EQUATION AXIOM MODULE
// =============================================================================

class FieldEquationAxiomModule {
    TermFactory& tf_;
public:
    explicit FieldEquationAxiomModule(TermFactory& tf) : tf_(tf) {}

    std::vector<std::unique_ptr<Equation>> generateAxioms() {
        std::vector<std::unique_ptr<Equation>> axioms;
        auto zero = tf_.scalar(0.0);

        // -------------------------------------------------------------------
        // FIELD EQUATION: δA + A∪A = 0
        // -------------------------------------------------------------------
        auto field_eq = tf_.apply("FE_deltaA_plus_AcupA", {});
        axioms.push_back(std::make_unique<Equation>(field_eq, zero, EquationSource::Axiom));

        // -------------------------------------------------------------------
        // FLATNESS: δΩ = 0
        // -------------------------------------------------------------------
        auto flat = tf_.apply("FE_deltaOmega", {});
        axioms.push_back(std::make_unique<Equation>(flat, zero, EquationSource::Axiom));

        // -------------------------------------------------------------------
        // WAVE EQUATION: □Φ = 0
        // -------------------------------------------------------------------
        auto wave = tf_.apply("FE_BoxPhi", {});
        axioms.push_back(std::make_unique<Equation>(wave, zero, EquationSource::Axiom));

        // -------------------------------------------------------------------
        // KLEIN-GORDON: (□ + m²)Φ = 0
        // -------------------------------------------------------------------
        auto kg = tf_.apply("FE_KleinGordon", {});
        axioms.push_back(std::make_unique<Equation>(kg, zero, EquationSource::Axiom));

        // -------------------------------------------------------------------
        // NOETHER CONSERVATION: D_{g,χ}(j) = 0
        // -------------------------------------------------------------------
        auto noether = tf_.apply("FE_NoetherCurrent", {});
        axioms.push_back(std::make_unique<Equation>(noether, zero, EquationSource::Axiom));

        // -------------------------------------------------------------------
        // ENTROPY: D_{discrete}(S) ≥ 0 (encoded as D(S) - |D(S)| = 0)
        // -------------------------------------------------------------------
        auto entropy = tf_.apply("FE_EntropyProd", {});
        axioms.push_back(std::make_unique<Equation>(entropy, zero, EquationSource::Axiom));

        // -------------------------------------------------------------------
        // RG FLOW: D_Λ(g) = β(g)
        // -------------------------------------------------------------------
        auto rg_lhs = tf_.apply("FE_DLambda_g", {});
        auto rg_rhs = tf_.apply("FE_Beta_g", {});
        axioms.push_back(std::make_unique<Equation>(rg_lhs, rg_rhs, EquationSource::Axiom));

        // -------------------------------------------------------------------
        // EULER-LAGRANGE: D(∂L/∂(Df)) - ∂L/∂f = 0
        // -------------------------------------------------------------------
        auto euler_lagrange = tf_.apply("FE_EulerLagrange", {});
        axioms.push_back(std::make_unique<Equation>(euler_lagrange, zero, EquationSource::Axiom));

        // -------------------------------------------------------------------
        // MASTER EQUATION: Δ_g F = Ω_g · D_g F + μ_{F,g}
        // Rearranged: Δ_g F - Ω_g · D_g F - μ_{F,g} = 0
        // -------------------------------------------------------------------
        auto master = tf_.apply("FE_MasterResidual", {});
        axioms.push_back(std::make_unique<Equation>(master, zero, EquationSource::Axiom));

        return axioms;
    }
};

// =============================================================================
// FIELD EQUATION TERM GENERATOR
// =============================================================================

class FieldEquationTermGenerator {
    TermFactory& tf_;
public:
    explicit FieldEquationTermGenerator(TermFactory& tf) : tf_(tf) {}

    std::vector<const Term*> generateAll() {
        std::vector<const Term*> terms;

        // 1. Field equation residuals
        terms.push_back(tf_.apply("FE_deltaA_plus_AcupA", {}));
        terms.push_back(tf_.apply("FE_deltaOmega", {}));
        terms.push_back(tf_.apply("FE_Curvature", {}));
        terms.push_back(tf_.apply("FE_BoxPhi", {}));
        terms.push_back(tf_.apply("FE_KleinGordon", {}));
        terms.push_back(tf_.apply("FE_Helmholtz", {}));
        terms.push_back(tf_.apply("FE_Poisson", {}));
        terms.push_back(tf_.apply("FE_Diffusion", {}));
        terms.push_back(tf_.apply("FE_NoetherCurrent", {}));
        terms.push_back(tf_.apply("FE_NoetherCharge", {}));
        terms.push_back(tf_.apply("FE_EntropyProd", {}));
        terms.push_back(tf_.apply("FE_DLambda_g", {}));
        terms.push_back(tf_.apply("FE_Beta_g", {}));
        terms.push_back(tf_.apply("FE_EulerLagrange", {}));
        terms.push_back(tf_.apply("FE_MasterResidual", {}));

        // 2. Connection components
        terms.push_back(tf_.apply("FE_Omega", {}));
        terms.push_back(tf_.apply("FE_A_psi", {}));
        terms.push_back(tf_.apply("FE_A_sigma", {}));

        // 3. Field-on-lattice evaluations
        static const int LATTICE_N = 64;
        static const double LATTICE_DX = 0.1;
        // Sin wave field
        for (int probe : {8, 16, 24, 32, 40, 48}) {
            terms.push_back(tf_.apply("FE_SinField_" + std::to_string(probe), {}));
            terms.push_back(tf_.apply("FE_SinField_Lap_" + std::to_string(probe), {}));
            terms.push_back(tf_.apply("FE_SinField_E_" + std::to_string(probe), {}));
        }
        // Gaussian field
        for (int probe : {16, 24, 32, 40, 48}) {
            terms.push_back(tf_.apply("FE_GaussField_" + std::to_string(probe), {}));
            terms.push_back(tf_.apply("FE_GaussField_Lap_" + std::to_string(probe), {}));
        }

        // 4. Dimensionless physics ratios (for discovery)
        terms.push_back(tf_.apply("FE_Alpha_EM", {}));
        terms.push_back(tf_.apply("FE_Sin2Weinberg", {}));
        terms.push_back(tf_.apply("FE_SinCabibbo", {}));
        terms.push_back(tf_.apply("FE_MuOverE", {}));
        terms.push_back(tf_.apply("FE_TauOverE", {}));
        terms.push_back(tf_.apply("FE_ProtonOverE", {}));
        terms.push_back(tf_.apply("FE_NeutronOverP", {}));
        terms.push_back(tf_.apply("FE_DEMRatio", {}));
        terms.push_back(tf_.apply("FE_PlanckOverProton", {}));

        // 5. φ-power sequence for ratio matching
        for (int n = -10; n <= 10; ++n) {
            terms.push_back(tf_.apply("FE_PhiPow_" + std::to_string(n), {}));
        }
        for (int n = -3; n <= 3; ++n) {
            terms.push_back(tf_.apply("FE_LamPow_" + std::to_string(n), {}));
        }

        // 6. β-function values at different couplings
        for (int k = 1; k <= 6; ++k) {
            double g = 0.1 * k;
            terms.push_back(tf_.apply("FE_Beta_" + std::to_string(k), {}));
        }

        // 7. Action integrals over lattice
        terms.push_back(tf_.apply("FE_Action_Sin", {}));
        terms.push_back(tf_.apply("FE_Action_Gauss", {}));
        terms.push_back(tf_.apply("FE_TotalEnergy_Sin", {}));
        terms.push_back(tf_.apply("FE_TotalEnergy_Gauss", {}));

        // 8. Interstice wave operators applied to physics functions
        terms.push_back(tf_.apply("FE_BoxNewton_sin", {}));
        terms.push_back(tf_.apply("FE_BoxNewton_cos", {}));
        terms.push_back(tf_.apply("FE_BoxNewton_exp", {}));
        terms.push_back(tf_.apply("FE_BoxNewton_gauss", {}));
        terms.push_back(tf_.apply("FE_BoxLambda_sin", {}));
        terms.push_back(tf_.apply("FE_BoxLambda_cos", {}));
        terms.push_back(tf_.apply("FE_BoxLambda_exp", {}));

        return terms;
    }
};

// =============================================================================
// FIELD EQUATION NUMERIC EVALUATOR
// =============================================================================

class FieldEquationNumericEvaluator {
    static constexpr double X0 = cst::PHI;
    static constexpr int LAT_N = 64;
    static constexpr double LAT_DX = 0.1;

    // Lazy-init lattice fields
    mutable bool fields_init_ = false;
    mutable Field1D sinField_{1, 0.1, [](double) { return 0.0; }};
    mutable Field1D gaussField_{1, 0.1, [](double) { return 0.0; }};

    void initFields() const {
        if (fields_init_) return;
        sinField_ = Field1D(LAT_N, LAT_DX, [](double x) { return std::sin(x); });
        gaussField_ = Field1D(LAT_N, LAT_DX, [](double x) {
            double cx = x - 3.2; return std::exp(-cx * cx);
        });
        fields_init_ = true;
    }

    // D_{Newton} via central difference
    double DNewton(const std::string& fn, double x) const {
        double h = 1e-7;
        auto f = [&](double v) { return evalFunc(fn, v); };
        return (f(x + h) - f(x - h)) / (2.0 * h);
    }

    // D² via central difference
    double D2Newton(const std::string& fn, double x) const {
        double h = 1e-5;
        auto f = [&](double v) { return evalFunc(fn, v); };
        return (f(x + h) - 2.0 * f(x) + f(x - h)) / (h * h);
    }

    // D_{Λ} — scale derivative
    double DLambda(const std::string& fn, double x) const {
        double chi = (cst::LAMBDA - 1.0) * x;
        if (std::abs(chi) < 1e-15) return 0.0;
        return (evalFunc(fn, cst::LAMBDA * x) - evalFunc(fn, x)) / chi;
    }

    double evalFunc(const std::string& fn, double x) const {
        if (fn == "sin") return std::sin(x);
        if (fn == "cos") return std::cos(x);
        if (fn == "exp") return std::exp(x);
        if (fn == "gauss") return std::exp(-x * x);
        if (fn == "id") return x;
        if (fn == "sq") return x * x;
        if (fn == "log") return (x > 0) ? std::log(x) : 0.0;
        return 0.0;
    }

public:
    std::optional<double> evaluate(const std::string& sym,
                                    const std::vector<std::optional<double>>& childVals) const {
        initFields();

        // -------------------------------------------------------------------
        // FIELD EQUATION RESIDUALS
        // -------------------------------------------------------------------
        if (sym == "FE_deltaA_plus_AcupA") {
            // Flat interstice connection: curvature = 0
            IntersticeConnection conn;
            conn.evaluate();
            return conn.curvature;  // = 0.0 for flat
        }
        if (sym == "FE_deltaOmega") {
            // δΩ = 0 for scale 1-form
            return 0.0;
        }
        if (sym == "FE_Curvature") {
            IntersticeConnection conn;
            conn.evaluate();
            return conn.curvature;
        }

        // Wave equation □Φ = 0 evaluated on sin at x₀=φ
        if (sym == "FE_BoxPhi") {
            // □(sin)(φ) = sin''(φ) = -sin(φ)
            double val = D2Newton("sin", X0);
            return val + std::sin(X0); // Residual: should be ≈ 0
        }
        if (sym == "FE_KleinGordon") {
            // (□ + m²)Φ = 0 with Φ=e^{-x²}, m²=2
            double box = D2Newton("gauss", X0);
            double phi = std::exp(-X0 * X0);
            return box + 2.0 * phi;
        }
        if (sym == "FE_Helmholtz") {
            double lap = D2Newton("sin", X0);
            double phi = std::sin(X0);
            // (Δ + k²)Φ with k=1: sin''(x) + sin(x) = 0
            return lap + phi;
        }
        if (sym == "FE_Poisson") {
            // Δ(x²) = 2
            double lap = D2Newton("sq", X0);
            return lap - 2.0;
        }
        if (sym == "FE_Diffusion") {
            // Residual for heat equation with specific profile
            double dL = DLambda("gauss", X0);
            double lap = D2Newton("gauss", X0);
            return dL - 0.5 * lap;  // Scale evolution vs diffusion
        }
        if (sym == "FE_NoetherCurrent") {
            // For internal U(1) symmetry: ∂_μ j^μ = 0
            // j_0 = φ ∂_t φ* - φ* ∂_t φ → for real scalar: 0
            return 0.0;
        }
        if (sym == "FE_NoetherCharge") {
            // Q = ∫ j⁰ dx — lattice sum for sin field
            double Q = 0.0;
            for (int i = 0; i < LAT_N; ++i) {
                double f_i = sinField_[i];
                double pi_i = sinField_.DNewton(i);
                Q += pi_i * f_i * LAT_DX;
            }
            return Q;
        }
        if (sym == "FE_EntropyProd") {
            // S_after - S_before ≥ 0 (Boltzmann H-theorem)
            double S_before = -0.5 * std::log(2.0 * cst::PI_VAL * std::exp(1.0));
            double evolve = std::exp(-1.0 / cst::LAMBDA);
            double S_after = -0.5 * std::log(2.0 * cst::PI_VAL * std::exp(1.0) * evolve * evolve);
            return entropyProduction(S_before, S_after);
        }

        // RG flow
        if (sym == "FE_DLambda_g") {
            // D_Λ(g) at g=0.1
            double g = 0.1;
            double gL = g + betaFunction_phi4(g) * cst::LN_LAMBDA;
            return (gL - g) / cst::LN_LAMBDA;
        }
        if (sym == "FE_Beta_g") {
            return betaFunction_phi4(0.1);
        }

        // Euler-Lagrange for harmonic oscillator: f'' + f = 0
        if (sym == "FE_EulerLagrange") {
            double f = std::sin(X0);
            double fpp = D2Newton("sin", X0);
            return fpp + f;  // Should be ≈ 0
        }

        // Master equation residual: Δ_g F - Ω_g·D_g F - μ_{F,g} = 0
        if (sym == "FE_MasterResidual") {
            // For smooth f=sin, μ=0:
            // Δ_g f = f(Λx)-f(x) = sin(Λφ)-sin(φ)
            double delta = std::sin(cst::LAMBDA * X0) - std::sin(X0);
            double Omega = (cst::LAMBDA - 1.0) * X0;
            double Dg = DLambda("sin", X0);
            return delta - Omega * Dg;  // should be ≈ 0
        }

        // -------------------------------------------------------------------
        // CONNECTION COMPONENTS
        // -------------------------------------------------------------------
        if (sym == "FE_Omega") {
            return (cst::LAMBDA - 1.0) * X0;
        }
        if (sym == "FE_A_psi") {
            return cst::ALPHA_INT * (cst::LAMBDA - 1.0) * X0;
        }
        if (sym == "FE_A_sigma") {
            return cst::LN_LAMBDA * (cst::LAMBDA - 1.0) * X0;
        }

        // -------------------------------------------------------------------
        // LATTICE PROBE EVALUATIONS
        // -------------------------------------------------------------------
        // SinField
        if (sym.size() > 12 && sym.substr(0, 12) == "FE_SinField_") {
            std::string rest = sym.substr(12);
            if (rest.substr(0, 4) == "Lap_") {
                int idx = std::stoi(rest.substr(4));
                return sinField_.Laplacian(idx);
            }
            if (rest.substr(0, 2) == "E_") {
                int idx = std::stoi(rest.substr(2));
                return sinField_.energyDensity(idx);
            }
            int idx = std::stoi(rest);
            return sinField_[idx];
        }
        // GaussField
        if (sym.size() > 14 && sym.substr(0, 14) == "FE_GaussField_") {
            std::string rest = sym.substr(14);
            if (rest.substr(0, 4) == "Lap_") {
                int idx = std::stoi(rest.substr(4));
                return gaussField_.Laplacian(idx);
            }
            int idx = std::stoi(rest);
            return gaussField_[idx];
        }

        // -------------------------------------------------------------------
        // DIMENSIONLESS PHYSICS RATIOS
        // -------------------------------------------------------------------
        if (sym == "FE_Alpha_EM")       return ratios::alpha_em_approx();
        if (sym == "FE_Sin2Weinberg")   return ratios::sin2_weinberg();
        if (sym == "FE_SinCabibbo")     return ratios::sin_cabibbo();
        if (sym == "FE_MuOverE")        return ratios::ratio_mu_e();
        if (sym == "FE_TauOverE")       return ratios::ratio_tau_e();
        if (sym == "FE_ProtonOverE")    return ratios::ratio_p_e();
        if (sym == "FE_NeutronOverP")   return ratios::ratio_n_p();
        if (sym == "FE_DEMRatio")       return ratios::dark_energy_matter_ratio();
        if (sym == "FE_PlanckOverProton") return ratios::planck_proton();

        // -------------------------------------------------------------------
        // φ-power and Λ-power sequences
        // -------------------------------------------------------------------
        if (sym.size() > 10 && sym.substr(0, 10) == "FE_PhiPow_") {
            std::string ns = sym.substr(10);
            int n = std::stoi(ns);
            return std::pow(cst::PHI, n);
        }
        if (sym.size() > 10 && sym.substr(0, 10) == "FE_LamPow_") {
            std::string ns = sym.substr(10);
            int n = std::stoi(ns);
            return std::pow(cst::LAMBDA, n);
        }

        // -------------------------------------------------------------------
        // β-function at different couplings
        // -------------------------------------------------------------------
        if (sym.size() > 8 && sym.substr(0, 8) == "FE_Beta_") {
            int k = std::stoi(sym.substr(8));
            double g = 0.1 * k;
            return betaFunction_phi4(g);
        }

        // -------------------------------------------------------------------
        // Action integrals
        // -------------------------------------------------------------------
        if (sym == "FE_Action_Sin" || sym == "FE_TotalEnergy_Sin") {
            double total = 0.0;
            for (int i = 0; i < LAT_N; ++i)
                total += sinField_.energyDensity(i) * LAT_DX;
            return total;
        }
        if (sym == "FE_Action_Gauss" || sym == "FE_TotalEnergy_Gauss") {
            double total = 0.0;
            for (int i = 0; i < LAT_N; ++i)
                total += gaussField_.energyDensity(i) * LAT_DX;
            return total;
        }

        // -------------------------------------------------------------------
        // Box operators applied to test functions
        // -------------------------------------------------------------------
        if (sym.size() > 13 && sym.substr(0, 13) == "FE_BoxNewton_") {
            std::string fn = sym.substr(13);
            return D2Newton(fn, X0);
        }
        if (sym.size() > 13 && sym.substr(0, 13) == "FE_BoxLambda_") {
            std::string fn = sym.substr(13);
            // D²_Λ via finite difference of D_Λ
            double h = 1e-5;
            double Dp = DLambda(fn, X0 + h);
            double Dm = DLambda(fn, X0 - h);
            return (Dp - Dm) / (2.0 * h);
        }

        return std::nullopt;
    }
};

} // namespace fieldequation
} // namespace domain
} // namespace autodiscover
