#pragma once
// =============================================================================
// PhysicsUniverse.hpp — Fundamental Constants, Information Theory & Topology
// =============================================================================
//
// Provides:
//   1. ALL fundamental physical constants (SI + natural units)
//   2. Dimensionless ratios (fine structure α, proton/electron mass ratio, etc.)
//   3. Planck units
//   4. Information theory (Shannon entropy, partition functions, Boltzmann)
//   5. Topological invariants (Euler characteristic, Gauss-Bonnet, Betti numbers)
//   6. Symmetry dimensions and group-theoretic numbers
//   7. Cosmological parameters
//
// These constants become ATOMS in the term grammar, allowing the discovery
// engine to find relationships between fundamental constants, information-
// theoretic quantities, and topological invariants.
// =============================================================================

#include <cmath>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <numeric>
#include <algorithm>
#include "../core/Term.hpp"
#include "../logic/Equation.hpp"

namespace autodiscover {
namespace domain {
namespace physics {

using core::Term;
using core::TermFactory;
using core::Sort;
using logic::Equation;
using logic::EquationSource;

// =============================================================================
// 1. FUNDAMENTAL CONSTANTS (SI units)
// =============================================================================
namespace si {
    constexpr double C          = 299792458.0;             // speed of light (m/s)
    constexpr double HBAR       = 1.054571817e-34;         // reduced Planck (J·s)
    constexpr double H_PLANCK   = 6.62607015e-34;          // Planck constant (J·s)
    constexpr double G_NEWTON   = 6.67430e-11;             // gravitational (m³/kg/s²)
    constexpr double KB         = 1.380649e-23;            // Boltzmann (J/K)
    constexpr double E_CHARGE   = 1.602176634e-19;         // elementary charge (C)
    constexpr double EPSILON_0  = 8.8541878128e-12;        // vacuum permittivity (F/m)
    constexpr double MU_0       = 1.25663706212e-6;        // vacuum permeability (H/m)
    constexpr double M_ELECTRON = 9.1093837015e-31;        // electron mass (kg)
    constexpr double M_PROTON   = 1.67262192369e-27;       // proton mass (kg)
    constexpr double AVOGADRO   = 6.02214076e23;           // Avogadro's number
    constexpr double R_GAS      = 8.314462618;             // ideal gas constant (J/mol/K)
    constexpr double SIGMA_SB   = 5.670374419e-8;          // Stefan-Boltzmann (W/m²/K⁴)
    constexpr double BOHR_RADIUS = 5.29177210903e-11;      // Bohr radius (m)
    constexpr double RYDBERG_EV = 13.605693122994;         // Rydberg energy (eV)
} // namespace si

// =============================================================================
// 2. DIMENSIONLESS CONSTANTS (the ones the discoverer can actually use)
// =============================================================================
namespace dimensionless {
    constexpr double ALPHA_EM        = 7.2973525693e-3;    // fine structure ≈ 1/137.036
    constexpr double ALPHA_EM_INV    = 137.035999084;      // 1/α
    constexpr double ALPHA_S_MZ     = 0.1179;             // strong coupling at M_Z
    constexpr double SIN2_WEINBERG  = 0.23122;            // sin²(θ_W) Weinberg angle
    constexpr double COS2_WEINBERG  = 1.0 - 0.23122;     // cos²(θ_W)
    constexpr double PROTON_ELECTRON = 1836.15267343;     // m_p / m_e
    constexpr double ELECTRON_G_MINUS_2 = 0.00115965218128; // (g-2)/2 of electron
    constexpr double MUON_G_MINUS_2 = 0.00116592061;      // (g-2)/2 of muon
    constexpr double PI             = 3.14159265358979323846;
    constexpr double E_EULER        = 2.718281828459045;
    constexpr double PHI            = 1.6180339887498949;   // golden ratio
    constexpr double EULER_GAMMA    = 0.5772156649015329;  // Euler-Mascheroni
    constexpr double ZETA_3         = 1.2020569031595943;  // Apéry's constant ζ(3)
    constexpr double ZETA_5         = 1.0369277551433699;  // ζ(5)
    constexpr double CATALAN        = 0.9159655941772190;  // Catalan's constant
    constexpr double FEIGENBAUM_D   = 4.6692016091029907;  // Feigenbaum δ
    constexpr double FEIGENBAUM_A   = 2.5029078750958929;  // Feigenbaum α
    constexpr double KHINCHIN       = 2.6854520010653064;  // Khinchin's constant
    constexpr double TWIN_PRIME     = 0.6601618158468696;  // twin prime constant
    constexpr double OMEGA          = 0.5671432904097838;  // Ω: W(1) Lambert W
    constexpr double PLASTIC        = 1.3247179572447460;  // plastic ratio
    constexpr double SUPERGOLDEN    = 1.4655712318767680;  // supergolden ratio

    // Cabibbo-related
    constexpr double SIN_CABIBBO    = 0.22500;             // sin(θ_C) Cabibbo angle
    constexpr double COS_CABIBBO   = 0.97437;             // cos(θ_C)
} // namespace dimensionless

// =============================================================================
// 3. PLANCK UNITS (derived)
// =============================================================================
namespace planck {
    constexpr double LENGTH = 1.616255e-35;       // l_P (m)
    constexpr double MASS   = 2.176434e-8;        // m_P (kg)
    constexpr double TIME   = 5.391247e-44;       // t_P (s)
    constexpr double TEMP   = 1.416784e32;        // T_P (K)
    constexpr double CHARGE = 1.8755459e-18;      // q_P (C)
    constexpr double ENERGY_GEV = 1.22089e19;     // E_P (GeV)

    // In natural units (c = ℏ = 1), only G remains:
    // G = l_P² in Planck units
} // namespace planck

// =============================================================================
// 4. INFORMATION THEORY
// =============================================================================
namespace infotheo {
    /// Shannon entropy: H = -Σᵢ pᵢ log₂(pᵢ)
    inline double shannonEntropy(const std::vector<double>& probs) {
        double H = 0.0;
        for (double p : probs) {
            if (p > 0.0) H -= p * std::log2(p);
        }
        return H;
    }

    /// Natural (Gibbs) entropy: S = -Σᵢ pᵢ ln(pᵢ)
    inline double gibbsEntropy(const std::vector<double>& probs) {
        double S = 0.0;
        for (double p : probs) {
            if (p > 0.0) S -= p * std::log(p);
        }
        return S;
    }

    /// Boltzmann entropy: S = k_B ln(W)
    inline double boltzmannEntropy(int W) {
        return std::log(static_cast<double>(W));
    }

    /// Partition function: Z = Σᵢ e^{-βEᵢ}
    inline double partitionFunction(const std::vector<double>& energies, double beta) {
        double Z = 0.0;
        for (double E : energies) Z += std::exp(-beta * E);
        return Z;
    }

    /// Free energy: F = -kT ln(Z) = -(1/β) ln(Z)
    inline double freeEnergy(double Z, double beta) {
        return -(1.0 / beta) * std::log(Z);
    }

    /// KL divergence: D_KL(P||Q) = Σᵢ pᵢ ln(pᵢ/qᵢ)
    inline double klDivergence(const std::vector<double>& P, const std::vector<double>& Q) {
        if (P.size() != Q.size()) return std::numeric_limits<double>::quiet_NaN();
        double D = 0.0;
        for (size_t i = 0; i < P.size(); i++) {
            if (P[i] > 0 && Q[i] > 0) D += P[i] * std::log(P[i] / Q[i]);
        }
        return D;
    }

    /// Fisher information: I(θ) = Σᵢ (1/p(xᵢ;θ)) (dp/dθ)²
    /// For Gaussian with known variance σ²: I = 1/σ²
    inline double fisherInfoGaussian(double sigma) {
        return 1.0 / (sigma * sigma);
    }

    /// Maximum entropy for n-state system: H_max = ln(n) (natural) or log₂(n) (bits)
    inline double maxEntropy(int n) {
        return std::log(static_cast<double>(n));
    }

    /// Bekenstein-Hawking entropy: S = A/(4l_P²) = πr²/(l_P²)
    /// For Schwarzschild black hole of mass M: S = 4πGM²/(ℏc)
    /// In natural units: S = 4πM² (with G=1)
    inline double blackHoleEntropy(double mass) {
        return 4.0 * dimensionless::PI * mass * mass;
    }

    /// Binary entropy: H(p) = -p log₂(p) - (1-p) log₂(1-p)
    inline double binaryEntropy(double p) {
        if (p <= 0 || p >= 1) return 0.0;
        return -p * std::log2(p) - (1.0 - p) * std::log2(1.0 - p);
    }
} // namespace infotheo

// =============================================================================
// 5. TOPOLOGICAL INVARIANTS
// =============================================================================
namespace topology {
    /// Euler characteristic of standard surfaces
    /// S²: χ=2, T²: χ=0, RP²: χ=1, Klein: χ=0
    /// genus-g surface: χ = 2 - 2g
    inline double eulerCharSurface(int genus) {
        return 2.0 - 2.0 * genus;
    }

    /// Euler characteristic from Betti numbers: χ = Σ(-1)^k b_k
    inline double eulerCharFromBetti(const std::vector<int>& betti) {
        double chi = 0.0;
        for (size_t k = 0; k < betti.size(); k++) {
            chi += ((k % 2 == 0) ? 1 : -1) * betti[k];
        }
        return chi;
    }

    /// Gauss-Bonnet: ∫_M K dA = 2πχ(M)
    inline double gaussBonnetRHS(int genus) {
        return 2.0 * dimensionless::PI * eulerCharSurface(genus);
    }

    /// Euler characteristic for polyhedra: V - E + F = χ
    inline int eulerCharPolyhedra(int V, int E, int F) {
        return V - E + F;
    }

    /// Winding number (integer-valued)
    /// ω = (1/2π) ∮ dθ = n for n-fold winding
    inline double windingNumber(int n) { return static_cast<double>(n); }

    /// Chern number for U(1) bundle (integer)
    inline double chernNumber(int c1) { return static_cast<double>(c1); }

    /// Pontryagin number (integer)
    inline double pontryaginNumber(int p1) { return static_cast<double>(p1); }

    /// Volume of n-sphere S^n: V_n = 2π^{(n+1)/2} / Γ((n+1)/2)
    inline double sphereVolume(int n) {
        double nh = (n + 1.0) / 2.0;
        return 2.0 * std::pow(dimensionless::PI, nh) / std::tgamma(nh);
    }

    /// Volume of n-ball B^n (radius R):  V_n = π^{n/2} R^n / Γ(n/2 + 1)
    inline double ballVolume(int n, double R = 1.0) {
        double nh = n / 2.0;
        return std::pow(dimensionless::PI, nh) * std::pow(R, n) / std::tgamma(nh + 1.0);
    }
} // namespace topology

// =============================================================================
// 6. COSMOLOGICAL PARAMETERS
// =============================================================================
namespace cosmo {
    constexpr double H0_KM_S_MPC = 67.4;              // Hubble constant (km/s/Mpc)
    constexpr double OMEGA_MATTER = 0.315;             // matter density parameter
    constexpr double OMEGA_LAMBDA = 0.685;             // dark energy density
    constexpr double OMEGA_BARYON = 0.0493;            // baryon density
    constexpr double OMEGA_RADIATION = 9.1e-5;         // radiation density
    constexpr double CMB_TEMP = 2.7255;                // CMB temperature (K)
    constexpr double SIGMA_8 = 0.811;                  // matter fluctuation amplitude
    constexpr double N_S = 0.965;                      // scalar spectral index

    // Friedmann equation dimensionless: Ω_m + Ω_Λ + Ω_k = 1
    // For flat universe: Ω_k = 0
    inline double flatnessSum() {
        return OMEGA_MATTER + OMEGA_LAMBDA; // should ≈ 1.0
    }
} // namespace cosmo

// =============================================================================
// 7. PHYSICS AXIOM MODULE
// =============================================================================

class PhysicsAxiomModule {
public:
    explicit PhysicsAxiomModule(TermFactory& factory)
        : factory_(factory) {}

    std::vector<std::unique_ptr<Equation>> generateAxioms() {
        std::vector<std::unique_ptr<Equation>> axioms;


        // α = e²/(4πε₀ℏc) ≈ 1/137.036
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("AlphaEM", {}),
            factory_.apply("inv", {factory_.scalar(dimensionless::ALPHA_EM_INV)}),
            EquationSource::Axiom));

        // Friedmann flatness: Ω_m + Ω_Λ = 1
        axioms.push_back(std::make_unique<Equation>(
            factory_.add(factory_.apply("OmegaMatter", {}),
                         factory_.apply("OmegaLambda", {})),
            factory_.scalar(1.0),
            EquationSource::Axiom));

        // Euler characteristic: S² has χ=2
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("EulerChar", {factory_.scalar(0.0)}), // genus 0
            factory_.scalar(2.0),
            EquationSource::Axiom));

        // Gauss-Bonnet: ∫K dA = 2πχ for S²: = 4π
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("GaussBonnet", {factory_.scalar(0.0)}),
            factory_.scalar(4.0 * dimensionless::PI),
            EquationSource::Axiom));

        // H_max(n=2) = ln(2) (maximum entropy for binary system)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("MaxEntropy", {factory_.scalar(2.0)}),
            factory_.apply("log", {factory_.scalar(2.0)}),
            EquationSource::Axiom));

        // Binary entropy maximum at p = ½: H(½) = 1 bit
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("BinaryEntropy", {factory_.scalar(0.5)}),
            factory_.scalar(1.0),
            EquationSource::Axiom));

        // Volume of S¹ = 2π
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("SphereVol", {factory_.scalar(1.0)}),
            factory_.scalar(2.0 * dimensionless::PI),
            EquationSource::Axiom));

        // Volume of S² = 4π
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("SphereVol", {factory_.scalar(2.0)}),
            factory_.scalar(4.0 * dimensionless::PI),
            EquationSource::Axiom));

        return axioms;
    }

private:
    TermFactory& factory_;
};

// =============================================================================
// 8. PHYSICS TERM GENERATOR
// =============================================================================

class PhysicsTermGenerator {
public:
    struct Config {
        size_t maxTerms = 2000;
    };

    PhysicsTermGenerator(TermFactory& factory, Config cfg = {})
        : factory_(factory), config_(cfg) {}

    std::vector<const Term*> generateAll() {

        std::vector<const Term*> result;
        auto tryAdd = [&](const Term* t) {
            if (t && result.size() < config_.maxTerms) result.push_back(t);
        };

        auto phi = factory_.phi();

        // ===== DIMENSIONLESS FUNDAMENTAL CONSTANTS =====
        tryAdd(factory_.apply("AlphaEM", {}));                   // ≈ 1/137.036
        tryAdd(factory_.apply("AlphaEM_Inv", {}));               // ≈ 137.036
        tryAdd(factory_.apply("AlphaS", {}));                    // ≈ 0.1179
        tryAdd(factory_.apply("Sin2Weinberg", {}));              // ≈ 0.23122
        tryAdd(factory_.apply("Cos2Weinberg", {}));              // ≈ 0.76878
        tryAdd(factory_.apply("ProtonElectronRatio", {}));       // ≈ 1836.15
        tryAdd(factory_.apply("ElectronGMinus2", {}));           // ≈ 0.00116
        tryAdd(factory_.apply("MuonGMinus2", {}));               // ≈ 0.00117
        tryAdd(factory_.apply("SinCabibbo", {}));                // ≈ 0.225
        tryAdd(factory_.apply("CosCabibbo", {}));                // ≈ 0.974

        // ===== MATHEMATICAL CONSTANTS =====
        tryAdd(factory_.apply("EulerGamma_C", {}));              // ≈ 0.5772
        tryAdd(factory_.apply("Zeta3", {}));                     // ≈ 1.2021
        tryAdd(factory_.apply("Zeta5", {}));                     // ≈ 1.0369
        tryAdd(factory_.apply("Catalan_C", {}));                 // ≈ 0.9160
        tryAdd(factory_.apply("FeigenbaumDelta", {}));           // ≈ 4.6692
        tryAdd(factory_.apply("FeigenbaumAlpha", {}));           // ≈ 2.5029
        tryAdd(factory_.apply("Khinchin", {}));                  // ≈ 2.6855
        tryAdd(factory_.apply("TwinPrimeConst", {}));            // ≈ 0.6602
        tryAdd(factory_.apply("OmegaConst", {}));                // ≈ 0.5671
        tryAdd(factory_.apply("PlasticRatio", {}));              // ≈ 1.3247
        tryAdd(factory_.apply("SuperGoldenRatio", {}));          // ≈ 1.4656

        // ===== COMBINATIONS of fine structure constant =====
        // α², α³, 1/α², α/π, α/(2π), 2α/π
        tryAdd(factory_.apply("AlphaEM_Sq", {}));
        tryAdd(factory_.apply("AlphaEM_Cube", {}));
        tryAdd(factory_.apply("AlphaEM_Over_Pi", {}));
        tryAdd(factory_.apply("AlphaEM_Over_2Pi", {}));

        // ===== COSMOLOGICAL PARAMETERS =====
        tryAdd(factory_.apply("OmegaMatter", {}));               // 0.315
        tryAdd(factory_.apply("OmegaLambda", {}));               // 0.685
        tryAdd(factory_.apply("OmegaBaryon", {}));               // 0.0493
        tryAdd(factory_.apply("FlatnessSum", {}));               // should = 1.0
        tryAdd(factory_.apply("CMBTemp", {}));                   // 2.7255 K
        tryAdd(factory_.apply("SpectralIndex", {}));             // 0.965
        tryAdd(factory_.apply("Sigma8", {}));                    // 0.811

        // ===== INFORMATION THEORY =====
        // Shannon entropy of uniform distributions
        for (int n = 2; n <= 10; n++) {
            tryAdd(factory_.apply("MaxEntropy", {factory_.scalar(static_cast<double>(n))}));
        }
        // Binary entropy at various p
        for (double p : {0.1, 0.2, 0.25, 0.3, 0.4, 0.5}) {
            tryAdd(factory_.apply("BinaryEntropy", {factory_.scalar(p)}));
        }
        // Boltzmann entropy S = ln(W)
        for (int W = 1; W <= 20; W++) {
            tryAdd(factory_.apply("BoltzmannS", {factory_.scalar(static_cast<double>(W))}));
        }
        // Partition function: Z = Σ e^{-βE} for harmonic oscillator E_n = n + ½
        for (double beta : {0.5, 1.0, 2.0, 5.0}) {
            tryAdd(factory_.apply("PartitionHO", {factory_.scalar(beta)}));
        }
        // Free energy F = -(1/β) ln Z
        for (double beta : {0.5, 1.0, 2.0}) {
            tryAdd(factory_.apply("FreeEnergyHO", {factory_.scalar(beta)}));
        }
        // Black hole entropy S = 4πM²
        for (double M : {1.0, 2.0, 3.0}) {
            tryAdd(factory_.apply("BlackHoleEntropy", {factory_.scalar(M)}));
        }
        // Fisher information for Gaussian
        for (double sigma : {0.5, 1.0, 2.0}) {
            tryAdd(factory_.apply("FisherGaussian", {factory_.scalar(sigma)}));
        }

        // ===== TOPOLOGICAL INVARIANTS =====
        // Euler characteristic of genus-g surface
        for (int g = 0; g <= 5; g++) {
            tryAdd(factory_.apply("EulerChar", {factory_.scalar(static_cast<double>(g))}));
        }
        // Gauss-Bonnet: ∫K dA = 2πχ
        for (int g = 0; g <= 5; g++) {
            tryAdd(factory_.apply("GaussBonnet", {factory_.scalar(static_cast<double>(g))}));
        }
        // Euler characteristic for platonic solids: V - E + F = 2
        // Tetrahedron: 4-6+4=2, Cube: 8-12+6=2, Octahedron: 6-12+8=2
        // Dodecahedron: 20-30+12=2, Icosahedron: 12-30+20=2
        tryAdd(factory_.apply("EulerPoly", {factory_.scalar(4), factory_.scalar(6), factory_.scalar(4)}));
        tryAdd(factory_.apply("EulerPoly", {factory_.scalar(8), factory_.scalar(12), factory_.scalar(6)}));
        tryAdd(factory_.apply("EulerPoly", {factory_.scalar(6), factory_.scalar(12), factory_.scalar(8)}));
        tryAdd(factory_.apply("EulerPoly", {factory_.scalar(20), factory_.scalar(30), factory_.scalar(12)}));
        tryAdd(factory_.apply("EulerPoly", {factory_.scalar(12), factory_.scalar(30), factory_.scalar(20)}));

        // Volume of n-sphere
        for (int n = 0; n <= 8; n++) {
            tryAdd(factory_.apply("SphereVol", {factory_.scalar(static_cast<double>(n))}));
        }
        // Volume of n-ball
        for (int n = 1; n <= 8; n++) {
            tryAdd(factory_.apply("BallVol", {factory_.scalar(static_cast<double>(n))}));
        }

        // ===== CROSS-DOMAIN COMPOSITIONS =====
        // Products/ratios of dimensionless constants with phi, pi, e
        tryAdd(factory_.mul(factory_.apply("AlphaEM", {}), phi));
        tryAdd(factory_.mul(factory_.apply("AlphaEM_Inv", {}), phi));
        tryAdd(factory_.apply("div", {factory_.apply("AlphaEM_Inv", {}),
                                       factory_.scalar(dimensionless::PI)}));

        // Entropy × topology
        tryAdd(factory_.mul(factory_.apply("MaxEntropy", {factory_.scalar(2.0)}),
                            factory_.apply("EulerChar", {factory_.scalar(0.0)})));

        return result;
    }

private:
    TermFactory& factory_;
    Config config_;
};

// =============================================================================
// 9. PHYSICS NUMERIC EVALUATOR
// =============================================================================

class PhysicsNumericEvaluator {
public:
    std::optional<double> evaluate(const std::string& sym,
        const std::vector<std::optional<double>>& args) const
    {
        using namespace dimensionless;

        // ----- Fundamental dimensionless constants -----
        if (sym == "AlphaEM")           return ALPHA_EM;
        if (sym == "AlphaEM_Inv")       return ALPHA_EM_INV;
        if (sym == "AlphaS")            return ALPHA_S_MZ;
        if (sym == "Sin2Weinberg")      return SIN2_WEINBERG;
        if (sym == "Cos2Weinberg")      return COS2_WEINBERG;
        if (sym == "ProtonElectronRatio") return PROTON_ELECTRON;
        if (sym == "ElectronGMinus2")   return ELECTRON_G_MINUS_2;
        if (sym == "MuonGMinus2")       return MUON_G_MINUS_2;
        if (sym == "SinCabibbo")        return SIN_CABIBBO;
        if (sym == "CosCabibbo")        return COS_CABIBBO;

        // ----- Mathematical constants -----
        if (sym == "EulerGamma_C")      return EULER_GAMMA;
        if (sym == "Zeta3")             return ZETA_3;
        if (sym == "Zeta5")             return ZETA_5;
        if (sym == "Catalan_C")         return CATALAN;
        if (sym == "FeigenbaumDelta")   return FEIGENBAUM_D;
        if (sym == "FeigenbaumAlpha")   return FEIGENBAUM_A;
        if (sym == "Khinchin")          return KHINCHIN;
        if (sym == "TwinPrimeConst")    return TWIN_PRIME;
        if (sym == "OmegaConst")        return OMEGA;
        if (sym == "PlasticRatio")      return PLASTIC;
        if (sym == "SuperGoldenRatio")  return SUPERGOLDEN;

        // ----- Fine structure combinations -----
        if (sym == "AlphaEM_Sq")        return ALPHA_EM * ALPHA_EM;
        if (sym == "AlphaEM_Cube")      return ALPHA_EM * ALPHA_EM * ALPHA_EM;
        if (sym == "AlphaEM_Over_Pi")   return ALPHA_EM / PI;
        if (sym == "AlphaEM_Over_2Pi")  return ALPHA_EM / (2.0 * PI);

        // ----- Cosmological -----
        if (sym == "OmegaMatter")       return cosmo::OMEGA_MATTER;
        if (sym == "OmegaLambda")       return cosmo::OMEGA_LAMBDA;
        if (sym == "OmegaBaryon")       return cosmo::OMEGA_BARYON;
        if (sym == "FlatnessSum")       return cosmo::flatnessSum();
        if (sym == "CMBTemp")           return cosmo::CMB_TEMP;
        if (sym == "SpectralIndex")     return cosmo::N_S;
        if (sym == "Sigma8")            return cosmo::SIGMA_8;

        // ----- Information theory -----
        if (sym == "MaxEntropy" && args.size() >= 1 && args[0]) {
            int n = static_cast<int>(std::round(*args[0]));
            if (n < 1) return std::nullopt;
            return infotheo::maxEntropy(n);
        }
        if (sym == "BinaryEntropy" && args.size() >= 1 && args[0]) {
            return infotheo::binaryEntropy(*args[0]);
        }
        if (sym == "BoltzmannS" && args.size() >= 1 && args[0]) {
            int W = static_cast<int>(std::round(*args[0]));
            if (W < 1) return std::nullopt;
            return infotheo::boltzmannEntropy(W);
        }
        if (sym == "PartitionHO" && args.size() >= 1 && args[0]) {
            // Harmonic oscillator: E_n = n + ½, n = 0,1,...,N
            double beta = *args[0];
            int N = 50; // truncation
            std::vector<double> energies;
            for (int n = 0; n <= N; n++) energies.push_back(n + 0.5);
            return infotheo::partitionFunction(energies, beta);
        }
        if (sym == "FreeEnergyHO" && args.size() >= 1 && args[0]) {
            double beta = *args[0];
            int N = 50;
            std::vector<double> energies;
            for (int n = 0; n <= N; n++) energies.push_back(n + 0.5);
            double Z = infotheo::partitionFunction(energies, beta);
            return infotheo::freeEnergy(Z, beta);
        }
        if (sym == "BlackHoleEntropy" && args.size() >= 1 && args[0]) {
            return infotheo::blackHoleEntropy(*args[0]);
        }
        if (sym == "FisherGaussian" && args.size() >= 1 && args[0]) {
            return infotheo::fisherInfoGaussian(*args[0]);
        }

        // ----- Topology -----
        if (sym == "EulerChar" && args.size() >= 1 && args[0]) {
            int g = static_cast<int>(std::round(*args[0]));
            return topology::eulerCharSurface(g);
        }
        if (sym == "GaussBonnet" && args.size() >= 1 && args[0]) {
            int g = static_cast<int>(std::round(*args[0]));
            return topology::gaussBonnetRHS(g);
        }
        if (sym == "EulerPoly" && args.size() >= 3 && args[0] && args[1] && args[2]) {
            int V = static_cast<int>(std::round(*args[0]));
            int E = static_cast<int>(std::round(*args[1]));
            int F = static_cast<int>(std::round(*args[2]));
            return static_cast<double>(topology::eulerCharPolyhedra(V, E, F));
        }
        if (sym == "SphereVol" && args.size() >= 1 && args[0]) {
            int n = static_cast<int>(std::round(*args[0]));
            if (n < 0) return std::nullopt;
            return topology::sphereVolume(n);
        }
        if (sym == "BallVol" && args.size() >= 1 && args[0]) {
            int n = static_cast<int>(std::round(*args[0]));
            if (n < 1) return std::nullopt;
            return topology::ballVolume(n);
        }

        return std::nullopt;
    }
};

} // namespace physics
} // namespace domain
} // namespace autodiscover
