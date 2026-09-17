#pragma once
// =============================================================================
// TensorSpinor.hpp — Tensor Algebra, Spinor Calculus & Lie Theory Engine
// =============================================================================
//
// Provides matrices, spinors, Clifford algebra, Lie algebra structure constants,
// and representation theory for the AutoDiscoverer. Enables discovery of:
//   - Pauli algebra: σᵢσⱼ = δᵢⱼI + iεᵢⱼₖσₖ
//   - Clifford algebra: {γ^μ, γ^ν} = 2η^{μν}
//   - Trace identities: Tr(σᵢσⱼ) = 2δᵢⱼ, Tr(γ^μγ^ν) = 4η^{μν}
//   - Levi-Civita / Kronecker identities
//   - su(2) & su(3) structure constants
//   - Casimir operator eigenvalues
//   - Representation dimensions
//
// All evaluations produce SCALAR values (Trace, Det, specific components)
// which participate in the standard bucket-and-discover pipeline.
// =============================================================================

#include <cmath>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <array>
#include "../core/Term.hpp"
#include "../logic/Equation.hpp"

namespace autodiscover {
namespace domain {
namespace tensor {

using core::Term;
using core::TermFactory;
using core::Sort;
using logic::Equation;
using logic::EquationSource;

// =============================================================================
// CONSTANTS
// =============================================================================
namespace constants {
    constexpr double PHI = 1.6180339887498949;
    constexpr double PI  = 3.14159265358979323846;
} // namespace constants

// =============================================================================
// 1. 2×2 REAL MATRIX ALGEBRA
// =============================================================================

struct Mat2 {
    double a, b, c, d; // [ a b ; c d ]

    static Mat2 identity() { return {1,0,0,1}; }
    static Mat2 zero() { return {0,0,0,0}; }

    // Pauli matrices (real representation where possible)
    // σ₁ = [0 1; 1 0], σ₂ = [0 -1; 1 0] (real part of [0 -i; i 0]), σ₃ = [1 0; 0 -1]
    static Mat2 pauli1() { return {0,1,1,0}; }
    static Mat2 pauli2R() { return {0,-1,1,0}; } // Real proxy for σ₂
    static Mat2 pauli3() { return {1,0,0,-1}; }

    double trace() const { return a + d; }
    double det() const { return a*d - b*c; }
    double frobeniusNorm() const { return std::sqrt(a*a + b*b + c*c + d*d); }
};

inline Mat2 matMul(const Mat2& A, const Mat2& B) {
    return {
        A.a*B.a + A.b*B.c, A.a*B.b + A.b*B.d,
        A.c*B.a + A.d*B.c, A.c*B.b + A.d*B.d
    };
}

inline Mat2 matAdd(const Mat2& A, const Mat2& B) {
    return {A.a+B.a, A.b+B.b, A.c+B.c, A.d+B.d};
}

inline Mat2 matScale(double s, const Mat2& A) {
    return {s*A.a, s*A.b, s*A.c, s*A.d};
}

inline Mat2 commutator(const Mat2& A, const Mat2& B) {
    auto AB = matMul(A, B);
    auto BA = matMul(B, A);
    return {AB.a-BA.a, AB.b-BA.b, AB.c-BA.c, AB.d-BA.d};
}

inline Mat2 anticommutator(const Mat2& A, const Mat2& B) {
    auto AB = matMul(A, B);
    auto BA = matMul(B, A);
    return {AB.a+BA.a, AB.b+BA.b, AB.c+BA.c, AB.d+BA.d};
}

// =============================================================================
// 2. LEVI-CIVITA & KRONECKER
// =============================================================================

/// Levi-Civita symbol ε_{ijk} for 3D
inline int leviCivita3(int i, int j, int k) {
    // ε_{123} = +1, even permutations = +1, odd = -1, repeated = 0
    if (i == j || j == k || i == k) return 0;
    // Sort and count swaps
    int arr[3] = {i, j, k};
    int swaps = 0;
    for (int a = 0; a < 2; a++) {
        for (int b = a+1; b < 3; b++) {
            if (arr[a] > arr[b]) { std::swap(arr[a], arr[b]); swaps++; }
        }
    }
    return (swaps % 2 == 0) ? 1 : -1;
}

/// 4D Levi-Civita ε_{μνρσ}
inline int leviCivita4(int a, int b, int c, int d) {
    int idx[4] = {a, b, c, d};
    for (int i = 0; i < 4; i++)
        for (int j = i+1; j < 4; j++)
            if (idx[i] == idx[j]) return 0;
    int swaps = 0;
    for (int i = 0; i < 3; i++) {
        for (int j = i+1; j < 4; j++) {
            if (idx[i] > idx[j]) { std::swap(idx[i], idx[j]); swaps++; }
        }
    }
    return (swaps % 2 == 0) ? 1 : -1;
}

/// Kronecker delta
inline int kronecker(int i, int j) { return (i == j) ? 1 : 0; }

// =============================================================================
// 3. METRIC TENSORS
// =============================================================================

/// Minkowski metric η_{μν} = diag(-1, +1, +1, +1) (mostly plus)
inline double minkowski(int mu, int nu) {
    if (mu != nu) return 0.0;
    return (mu == 0) ? -1.0 : 1.0;
}

/// Euclidean metric δ_{ij}
inline double euclidean(int i, int j) {
    return (i == j) ? 1.0 : 0.0;
}

// =============================================================================
// 4. DIRAC GAMMA MATRIX TRACES (without full 4×4 representation)
// =============================================================================
// Key trace identities evaluated directly:
//   Tr(I₄) = 4
//   Tr(γ^μ) = 0
//   Tr(γ^μ γ^ν) = 4η^{μν}
//   Tr(γ^μ γ^ν γ^ρ γ^σ) = 4(η^{μν}η^{ρσ} - η^{μρ}η^{νσ} + η^{μσ}η^{νρ})

inline double gammaTrace0() { return 4.0; } // Tr(I₄)
inline double gammaTrace1(int /*mu*/) { return 0.0; } // Tr(γ^μ) = 0
inline double gammaTrace2(int mu, int nu) { return 4.0 * minkowski(mu, nu); }
inline double gammaTrace4(int mu, int nu, int rho, int sig) {
    return 4.0 * (minkowski(mu,nu)*minkowski(rho,sig)
                - minkowski(mu,rho)*minkowski(nu,sig)
                + minkowski(mu,sig)*minkowski(nu,rho));
}

/// Clifford algebra: {γ^μ, γ^ν} = 2η^{μν} → component (a,b)
inline double cliffordAnticomm(int mu, int nu) {
    return 2.0 * minkowski(mu, nu);
}

/// γ⁵ trace: Tr(γ⁵) = 0, Tr(γ⁵ γ^μ γ^ν) = 0
/// Tr(γ⁵ γ^μ γ^ν γ^ρ γ^σ) = -4i ε^{μνρσ} (imaginary, we return the coefficient)
inline double gamma5Trace4Coeff(int mu, int nu, int rho, int sig) {
    return -4.0 * leviCivita4(mu, nu, rho, sig);
}

// =============================================================================
// 5. LIE ALGEBRA: su(2) STRUCTURE CONSTANTS
// =============================================================================
// [Tₐ, Tᵦ] = i fₐᵦ꜀ T꜀
// For su(2) with generators Jᵢ = σᵢ/2: fᵢⱼₖ = εᵢⱼₖ

inline double su2StructureConst(int i, int j, int k) {
    return static_cast<double>(leviCivita3(i, j, k));
}

/// Casimir operator eigenvalue: C₂(j) = j(j+1)
inline double su2Casimir(double j) { return j * (j + 1.0); }

/// Dimension of spin-j representation: d(j) = 2j + 1
inline double su2Dim(double j) { return 2.0 * j + 1.0; }

/// Index of representation: T(j) = j(j+1)(2j+1)/3
inline double su2Index(double j) { return j * (j + 1.0) * (2.0 * j + 1.0) / 3.0; }

// =============================================================================
// 6. LIE ALGEBRA: su(3) STRUCTURE CONSTANTS (Gell-Mann)
// =============================================================================
// f_{abc} for su(3), a,b,c ∈ {1,...,8}
// Non-zero: f_{123}=1, f_{147}=f_{246}=f_{257}=f_{345}=½,
//           f_{156}=f_{367}=-½, f_{458}=f_{678}=√3/2

inline double su3StructureConst(int a, int b, int c) {
    // Normalize to 1-indexed
    if (a < 1 || a > 8 || b < 1 || b > 8 || c < 1 || c > 8) return 0.0;

    // Sorted table of |f_{abc}| with signs
    // Using totally antisymmetric property
    int sorted[3] = {a, b, c};
    int swaps = 0;
    for (int i = 0; i < 2; i++) {
        for (int j = i+1; j < 3; j++) {
            if (sorted[i] > sorted[j]) { std::swap(sorted[i], sorted[j]); swaps++; }
        }
    }
    if (sorted[0] == sorted[1] || sorted[1] == sorted[2]) return 0.0;

    int sign = (swaps % 2 == 0) ? 1 : -1;
    double val = 0.0;

    int s1 = sorted[0], s2 = sorted[1], s3 = sorted[2];
    if (s1==1 && s2==2 && s3==3) val = 1.0;
    else if (s1==1 && s2==4 && s3==7) val = 0.5;
    else if (s1==1 && s2==5 && s3==6) val = -0.5;
    else if (s1==2 && s2==4 && s3==6) val = 0.5;
    else if (s1==2 && s2==5 && s3==7) val = 0.5;
    else if (s1==3 && s2==4 && s3==5) val = 0.5;
    else if (s1==3 && s2==6 && s3==7) val = -0.5;
    else if (s1==4 && s2==5 && s3==8) val = std::sqrt(3.0) / 2.0;
    else if (s1==6 && s2==7 && s3==8) val = std::sqrt(3.0) / 2.0;

    return sign * val;
}

/// su(3) Casimir for fundamental representation: C₂ = 4/3
inline double su3CasimirFundamental() { return 4.0 / 3.0; }

/// su(3) Casimir for adjoint: C₂ = 3
inline double su3CasimirAdjoint() { return 3.0; }

/// Dimension of su(3) irrep (p,q): d = (p+1)(q+1)(p+q+2)/2
inline double su3Dim(int p, int q) {
    return static_cast<double>((p+1) * (q+1) * (p+q+2)) / 2.0;
}

// =============================================================================
// 7. REPRESENTATION THEORY DIMENSIONS
// =============================================================================

/// Dimension of SO(n) fundamental representation
inline double soNFundDim(int n) { return static_cast<double>(n); }

/// Dimension of SO(n) adjoint representation: n(n-1)/2
inline double soNAdjDim(int n) { return static_cast<double>(n * (n-1)) / 2.0; }

/// Dimension of SU(n) adjoint representation: n²-1
inline double suNAdjDim(int n) { return static_cast<double>(n * n - 1); }

/// Dimension of SU(n) fundamental: n
inline double suNFundDim(int n) { return static_cast<double>(n); }

// =============================================================================
// 8. TENSOR AXIOM MODULE
// =============================================================================

class TensorAxiomModule {
public:
    explicit TensorAxiomModule(TermFactory& factory)
        : factory_(factory) {}

    std::vector<std::unique_ptr<Equation>> generateAxioms() {
        std::vector<std::unique_ptr<Equation>> axioms;

        int vc = 9500;
        auto freshVar = [&]() { return factory_.variable("ts" + std::to_string(vc++)); };
        (void)freshVar;

        auto s0 = factory_.scalar(0.0);
        auto s2 = factory_.scalar(2.0);
        auto s4 = factory_.scalar(4.0);

        // Tr(I₂) = 2
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("Tr2_I", {}),
            s2, EquationSource::Axiom));

        // Det(I₂) = 1
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("Det2_I", {}),
            factory_.scalar(1.0), EquationSource::Axiom));

        // Tr(σᵢ) = 0 for i = 1,2,3
        for (int i = 1; i <= 3; i++) {
            axioms.push_back(std::make_unique<Equation>(
                factory_.apply("Tr2_Pauli", {factory_.scalar(static_cast<double>(i))}),
                s0, EquationSource::Axiom));
        }

        // {γ^μ, γ^ν} = 2η^{μν}
        for (int mu = 0; mu < 4; mu++) {
            for (int nu = mu; nu < 4; nu++) {
                axioms.push_back(std::make_unique<Equation>(
                    factory_.apply("CliffordAC", {
                        factory_.scalar(static_cast<double>(mu)),
                        factory_.scalar(static_cast<double>(nu))}),
                    factory_.scalar(2.0 * minkowski(mu, nu)),
                    EquationSource::Axiom));
            }
        }

        // Tr(γ^μ) = 0
        for (int mu = 0; mu < 4; mu++) {
            axioms.push_back(std::make_unique<Equation>(
                factory_.apply("GammaTr1", {factory_.scalar(static_cast<double>(mu))}),
                s0, EquationSource::Axiom));
        }

        // Tr(γ^μ γ^ν) = 4η^{μν}
        for (int mu = 0; mu < 4; mu++) {
            for (int nu = 0; nu < 4; nu++) {
                axioms.push_back(std::make_unique<Equation>(
                    factory_.apply("GammaTr2", {
                        factory_.scalar(static_cast<double>(mu)),
                        factory_.scalar(static_cast<double>(nu))}),
                    factory_.scalar(4.0 * minkowski(mu, nu)),
                    EquationSource::Axiom));
            }
        }

        // Cayley-Hamilton for 2×2: A² - Tr(A)A + Det(A)I = 0
        // (structural axiom)

        // Casimir: C₂(j=½) = 3/4
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("SU2Casimir", {factory_.scalar(0.5)}),
            factory_.scalar(0.75), EquationSource::Axiom));

        // Casimir: C₂(j=1) = 2
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("SU2Casimir", {factory_.scalar(1.0)}),
            s2, EquationSource::Axiom));

        return axioms;
    }

private:
    TermFactory& factory_;
};

// =============================================================================
// 9. TENSOR TERM GENERATOR
// =============================================================================

class TensorTermGenerator {
public:
    struct Config {
        size_t maxTerms = 2000;
    };

    TensorTermGenerator(TermFactory& factory, Config cfg = {})
        : factory_(factory), config_(cfg) {}

    std::vector<const Term*> generateAll() {

        std::vector<const Term*> result;
        auto tryAdd = [&](const Term* t) {
            if (t && result.size() < config_.maxTerms) result.push_back(t);
        };

        // ----- PAULI MATRIX INVARIANTS -----
        // Tr and Det of identity and Pauli matrices
        tryAdd(factory_.apply("Tr2_I", {}));          // 2
        tryAdd(factory_.apply("Det2_I", {}));          // 1
        for (int i = 1; i <= 3; i++) {
            auto si = factory_.scalar(static_cast<double>(i));
            tryAdd(factory_.apply("Tr2_Pauli", {si}));    // 0
            tryAdd(factory_.apply("Det2_Pauli", {si}));   // -1
            tryAdd(factory_.apply("Frob2_Pauli", {si}));  // sqrt(2)
        }

        // Tr of Pauli products: Tr(σᵢσⱼ) = 2δᵢⱼ
        for (int i = 1; i <= 3; i++) {
            for (int j = 1; j <= 3; j++) {
                auto si = factory_.scalar(static_cast<double>(i));
                auto sj = factory_.scalar(static_cast<double>(j));
                tryAdd(factory_.apply("Tr2_PauliProd", {si, sj}));    // 2δᵢⱼ
                tryAdd(factory_.apply("Det2_PauliProd", {si, sj}));
            }
        }

        // σᵢ² = I: Tr(σᵢ²) = Tr(I) = 2
        for (int i = 1; i <= 3; i++) {
            auto si = factory_.scalar(static_cast<double>(i));
            tryAdd(factory_.apply("Tr2_PauliSq", {si}));  // 2
            tryAdd(factory_.apply("Det2_PauliSq", {si})); // 1
        }

        // ----- KRONECKER DELTA -----
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                tryAdd(factory_.apply("Kronecker", {
                    factory_.scalar(static_cast<double>(i)),
                    factory_.scalar(static_cast<double>(j))}));
            }
        }

        // ----- LEVI-CIVITA 3D -----
        for (int i = 1; i <= 3; i++) {
            for (int j = 1; j <= 3; j++) {
                for (int k = 1; k <= 3; k++) {
                    tryAdd(factory_.apply("LeviCivita3", {
                        factory_.scalar(static_cast<double>(i)),
                        factory_.scalar(static_cast<double>(j)),
                        factory_.scalar(static_cast<double>(k))}));
                }
            }
        }

        // ----- LEVI-CIVITA 4D (selected) -----
        int vals4[] = {0, 1, 2, 3};
        for (int a : vals4) for (int b : vals4) for (int c : vals4) for (int d : vals4) {
            if (a != b && b != c && c != d && a != c && a != d && b != d) {
                tryAdd(factory_.apply("LeviCivita4", {
                    factory_.scalar(static_cast<double>(a)),
                    factory_.scalar(static_cast<double>(b)),
                    factory_.scalar(static_cast<double>(c)),
                    factory_.scalar(static_cast<double>(d))}));
            }
            if (result.size() >= config_.maxTerms) break;
        }

        // ----- MINKOWSKI METRIC -----
        for (int mu = 0; mu < 4; mu++) {
            for (int nu = 0; nu < 4; nu++) {
                tryAdd(factory_.apply("Minkowski", {
                    factory_.scalar(static_cast<double>(mu)),
                    factory_.scalar(static_cast<double>(nu))}));
            }
        }

        // ----- CLIFFORD ANTICOMMUTATOR -----
        for (int mu = 0; mu < 4; mu++) {
            for (int nu = 0; nu < 4; nu++) {
                tryAdd(factory_.apply("CliffordAC", {
                    factory_.scalar(static_cast<double>(mu)),
                    factory_.scalar(static_cast<double>(nu))}));
            }
        }

        // ----- GAMMA MATRIX TRACES -----
        tryAdd(factory_.apply("GammaTr0", {})); // Tr(I₄) = 4
        for (int mu = 0; mu < 4; mu++) {
            auto smu = factory_.scalar(static_cast<double>(mu));
            tryAdd(factory_.apply("GammaTr1", {smu})); // 0
        }
        for (int mu = 0; mu < 4; mu++) {
            for (int nu = 0; nu < 4; nu++) {
                tryAdd(factory_.apply("GammaTr2", {
                    factory_.scalar(static_cast<double>(mu)),
                    factory_.scalar(static_cast<double>(nu))}));
            }
        }
        // 4-gamma traces (selected)
        for (int mu = 0; mu < 4; mu++) {
            for (int nu = 0; nu < 4; nu++) {
                tryAdd(factory_.apply("GammaTr4", {
                    factory_.scalar(static_cast<double>(mu)),
                    factory_.scalar(static_cast<double>(nu)),
                    factory_.scalar(static_cast<double>(mu)),
                    factory_.scalar(static_cast<double>(nu))}));
            }
        }

        // ----- γ⁵ TRACES (coefficient of i) -----
        for (int mu = 0; mu < 4; mu++) {
            for (int nu = 0; nu < 4; nu++) {
                if (mu != nu) {
                    int rho = -1, sig = -1;
                    for (int r = 0; r < 4; r++) {
                        if (r != mu && r != nu) { if (rho < 0) rho = r; else sig = r; }
                    }
                    tryAdd(factory_.apply("Gamma5Tr4", {
                        factory_.scalar(static_cast<double>(mu)),
                        factory_.scalar(static_cast<double>(nu)),
                        factory_.scalar(static_cast<double>(rho)),
                        factory_.scalar(static_cast<double>(sig))}));
                }
            }
        }

        // ----- su(2) STRUCTURE CONSTANTS -----
        for (int i = 1; i <= 3; i++) {
            for (int j = 1; j <= 3; j++) {
                for (int k = 1; k <= 3; k++) {
                    tryAdd(factory_.apply("SU2f", {
                        factory_.scalar(static_cast<double>(i)),
                        factory_.scalar(static_cast<double>(j)),
                        factory_.scalar(static_cast<double>(k))}));
                }
            }
        }

        // ----- su(3) STRUCTURE CONSTANTS (selected) -----
        int su3idx[] = {1,2,3,4,5,6,7,8};
        for (int a : su3idx) for (int b : su3idx) for (int c : su3idx) {
            double f = su3StructureConst(a, b, c);
            if (std::abs(f) > 1e-15) {
                tryAdd(factory_.apply("SU3f", {
                    factory_.scalar(static_cast<double>(a)),
                    factory_.scalar(static_cast<double>(b)),
                    factory_.scalar(static_cast<double>(c))}));
            }
            if (result.size() >= config_.maxTerms) break;
        }

        // ----- CASIMIR EIGENVALUES -----
        std::vector<double> spins = {0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0};
        for (double j : spins) {
            tryAdd(factory_.apply("SU2Casimir", {factory_.scalar(j)}));
            tryAdd(factory_.apply("SU2Dim", {factory_.scalar(j)}));
            tryAdd(factory_.apply("SU2Index", {factory_.scalar(j)}));
        }

        // su(3) Casimirs
        tryAdd(factory_.apply("SU3Casimir_fund", {}));  // 4/3
        tryAdd(factory_.apply("SU3Casimir_adj", {}));   // 3

        // su(3) irrep dimensions (p,q)
        for (int p = 0; p <= 3; p++) {
            for (int q = 0; q <= 3; q++) {
                tryAdd(factory_.apply("SU3Dim", {
                    factory_.scalar(static_cast<double>(p)),
                    factory_.scalar(static_cast<double>(q))}));
            }
        }

        // ----- GROUP DIMENSIONS -----
        for (int n = 2; n <= 10; n++) {
            tryAdd(factory_.apply("SUNAdjDim", {factory_.scalar(static_cast<double>(n))}));
            tryAdd(factory_.apply("SONAdjDim", {factory_.scalar(static_cast<double>(n))}));
        }

        // ----- CONTRACTION IDENTITIES -----
        // δ_{ii} = n (trace of identity in n dimensions)
        for (int n = 2; n <= 4; n++) {
            tryAdd(factory_.apply("KroneckerTrace", {factory_.scalar(static_cast<double>(n))}));
        }

        // ε_{ijk}ε_{ilm} = δ_{jl}δ_{km} - δ_{jm}δ_{kl} (specific evaluations)
        for (int j = 1; j <= 3; j++) {
            for (int k = 1; k <= 3; k++) {
                for (int l = 1; l <= 3; l++) {
                    for (int m = 1; m <= 3; m++) {
                        tryAdd(factory_.apply("EpsEpsContract", {
                            factory_.scalar(static_cast<double>(j)),
                            factory_.scalar(static_cast<double>(k)),
                            factory_.scalar(static_cast<double>(l)),
                            factory_.scalar(static_cast<double>(m))}));
                        if (result.size() >= config_.maxTerms) goto done;
                    }
                }
            }
        }
        done:

        return result;
    }

private:
    TermFactory& factory_;
    Config config_;
};

// =============================================================================
// 10. TENSOR NUMERIC EVALUATOR
// =============================================================================

class TensorNumericEvaluator {
public:
    std::optional<double> evaluate(const std::string& sym,
        const std::vector<std::optional<double>>& args) const
    {
        // ----- Pauli Traces -----
        if (sym == "Tr2_I") return 2.0;
        if (sym == "Det2_I") return 1.0;
        if (sym == "Tr2_Pauli" && args.size() >= 1) return 0.0; // Tr(σᵢ) = 0
        if (sym == "Det2_Pauli" && args.size() >= 1) return -1.0; // Det(σᵢ) = -1
        if (sym == "Frob2_Pauli" && args.size() >= 1) return std::sqrt(2.0); // ||σᵢ||_F

        if (sym == "Tr2_PauliProd" && args.size() >= 2 && args[0] && args[1]) {
            int i = static_cast<int>(std::round(*args[0]));
            int j = static_cast<int>(std::round(*args[1]));
            return 2.0 * kronecker(i, j); // Tr(σᵢσⱼ) = 2δᵢⱼ
        }
        if (sym == "Det2_PauliProd" && args.size() >= 2 && args[0] && args[1]) {
            int i = static_cast<int>(std::round(*args[0]));
            int j = static_cast<int>(std::round(*args[1]));
            if (i == j) return 1.0; // Det(σᵢ²) = Det(I) = 1
            // For i≠j: σᵢσⱼ = iεᵢⱼₖσₖ (has det = 1 for the real proxy)
            return 1.0;
        }
        if (sym == "Tr2_PauliSq" && args.size() >= 1) return 2.0; // σᵢ² = I₂
        if (sym == "Det2_PauliSq" && args.size() >= 1) return 1.0;

        // ----- Kronecker -----
        if (sym == "Kronecker" && args.size() >= 2 && args[0] && args[1]) {
            int i = static_cast<int>(std::round(*args[0]));
            int j = static_cast<int>(std::round(*args[1]));
            return static_cast<double>(kronecker(i, j));
        }
        if (sym == "KroneckerTrace" && args.size() >= 1 && args[0]) {
            return *args[0]; // δ_{ii} = n
        }

        // ----- Levi-Civita -----
        if (sym == "LeviCivita3" && args.size() >= 3 && args[0] && args[1] && args[2]) {
            int i = static_cast<int>(std::round(*args[0]));
            int j = static_cast<int>(std::round(*args[1]));
            int k = static_cast<int>(std::round(*args[2]));
            return static_cast<double>(leviCivita3(i, j, k));
        }
        if (sym == "LeviCivita4" && args.size() >= 4 && args[0] && args[1] && args[2] && args[3]) {
            return static_cast<double>(leviCivita4(
                static_cast<int>(std::round(*args[0])),
                static_cast<int>(std::round(*args[1])),
                static_cast<int>(std::round(*args[2])),
                static_cast<int>(std::round(*args[3]))));
        }

        // ε_{ijk}ε_{ilm} contraction
        if (sym == "EpsEpsContract" && args.size() >= 4 && args[0] && args[1] && args[2] && args[3]) {
            int j = static_cast<int>(std::round(*args[0]));
            int k = static_cast<int>(std::round(*args[1]));
            int l = static_cast<int>(std::round(*args[2]));
            int m = static_cast<int>(std::round(*args[3]));
            // Sum over i: Σᵢ ε_{ijk} ε_{ilm} = δ_{jl}δ_{km} - δ_{jm}δ_{kl}
            double sum = 0.0;
            for (int i = 1; i <= 3; i++) {
                sum += leviCivita3(i,j,k) * leviCivita3(i,l,m);
            }
            return sum;
        }

        // ----- Minkowski metric -----
        if (sym == "Minkowski" && args.size() >= 2 && args[0] && args[1]) {
            return minkowski(
                static_cast<int>(std::round(*args[0])),
                static_cast<int>(std::round(*args[1])));
        }

        // ----- Clifford anticommutator -----
        if (sym == "CliffordAC" && args.size() >= 2 && args[0] && args[1]) {
            return cliffordAnticomm(
                static_cast<int>(std::round(*args[0])),
                static_cast<int>(std::round(*args[1])));
        }

        // ----- Gamma traces -----
        if (sym == "GammaTr0") return 4.0;
        if (sym == "GammaTr1" && args.size() >= 1) return 0.0;
        if (sym == "GammaTr2" && args.size() >= 2 && args[0] && args[1]) {
            return gammaTrace2(
                static_cast<int>(std::round(*args[0])),
                static_cast<int>(std::round(*args[1])));
        }
        if (sym == "GammaTr4" && args.size() >= 4 && args[0] && args[1] && args[2] && args[3]) {
            return gammaTrace4(
                static_cast<int>(std::round(*args[0])),
                static_cast<int>(std::round(*args[1])),
                static_cast<int>(std::round(*args[2])),
                static_cast<int>(std::round(*args[3])));
        }
        if (sym == "Gamma5Tr4" && args.size() >= 4 && args[0] && args[1] && args[2] && args[3]) {
            return gamma5Trace4Coeff(
                static_cast<int>(std::round(*args[0])),
                static_cast<int>(std::round(*args[1])),
                static_cast<int>(std::round(*args[2])),
                static_cast<int>(std::round(*args[3])));
        }

        // ----- su(2) -----
        if (sym == "SU2f" && args.size() >= 3 && args[0] && args[1] && args[2]) {
            return su2StructureConst(
                static_cast<int>(std::round(*args[0])),
                static_cast<int>(std::round(*args[1])),
                static_cast<int>(std::round(*args[2])));
        }
        if (sym == "SU2Casimir" && args.size() >= 1 && args[0]) {
            return su2Casimir(*args[0]);
        }
        if (sym == "SU2Dim" && args.size() >= 1 && args[0]) {
            return su2Dim(*args[0]);
        }
        if (sym == "SU2Index" && args.size() >= 1 && args[0]) {
            return su2Index(*args[0]);
        }

        // ----- su(3) -----
        if (sym == "SU3f" && args.size() >= 3 && args[0] && args[1] && args[2]) {
            return su3StructureConst(
                static_cast<int>(std::round(*args[0])),
                static_cast<int>(std::round(*args[1])),
                static_cast<int>(std::round(*args[2])));
        }
        if (sym == "SU3Casimir_fund") return su3CasimirFundamental(); // 4/3
        if (sym == "SU3Casimir_adj") return su3CasimirAdjoint();      // 3
        if (sym == "SU3Dim" && args.size() >= 2 && args[0] && args[1]) {
            return su3Dim(
                static_cast<int>(std::round(*args[0])),
                static_cast<int>(std::round(*args[1])));
        }

        // ----- Group dimensions -----
        if (sym == "SUNAdjDim" && args.size() >= 1 && args[0]) {
            int n = static_cast<int>(std::round(*args[0]));
            return suNAdjDim(n);
        }
        if (sym == "SONAdjDim" && args.size() >= 1 && args[0]) {
            int n = static_cast<int>(std::round(*args[0]));
            return soNAdjDim(n);
        }

        return std::nullopt;
    }
};

} // namespace tensor
} // namespace domain
} // namespace autodiscover
