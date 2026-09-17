/**
 * @file CayleyLambda.hpp
 * @brief Cayley-Lambda Map, Hopf Manifold, Torus Geometry, and Scale-Covariant Spaces
 * See calculusnumberunification.txt for full mathematical details.
 */

#ifndef AUTODISCOVER_DOMAIN_CAYLEY_LAMBDA_HPP
#define AUTODISCOVER_DOMAIN_CAYLEY_LAMBDA_HPP

#include "../core/Term.hpp"
#include "../core/Constants.hpp"
#include "../logic/Equation.hpp"
#include <vector>
#include <memory>
#include <cmath>
#include <string>

namespace autodiscover {
namespace domain {

using core::Term;
using core::TermFactory;
using core::Sort;
using logic::Equation;
using constants::PI;
using constants::PHI;

// =============================================================================
// CONSTANTS — sourced from core/Constants.hpp
// =============================================================================

namespace cayley_lambda_constants {
    using ::autodiscover::constants::LAMBDA;
    using ::autodiscover::constants::LN_LAMBDA;
    using ::autodiscover::constants::LN_PHI;
    // Legacy aliases
    inline constexpr double TWO_PI_OVER_LN_LAMBDA = ::autodiscover::constants::BETA_INT;
    inline constexpr double OMEGA_1 = ::autodiscover::constants::BETA_INT;
}

// =============================================================================
// CAYLEY-LAMBDA NUMERIC EVALUATOR
// =============================================================================

/**
 * @brief Numeric evaluation of Cayley-Lambda map and related operations
 */
class CayleyLambdaEvaluator {
public:
    // Constants from cayley_lambda_constants namespace
    static constexpr double LAMBDA = cayley_lambda_constants::LAMBDA;
    static constexpr double LN_LAMBDA = cayley_lambda_constants::LN_LAMBDA;
    static constexpr double TWO_PI_OVER_LN_LAMBDA = cayley_lambda_constants::TWO_PI_OVER_LN_LAMBDA;
    static constexpr double OMEGA_1 = cayley_lambda_constants::OMEGA_1;

    /**
     * @brief Compute C_Lambda(z) = (theta, psi) on the torus
     */
    static std::pair<double, double> cayleyLambda(double re, double im) {
        double r = std::sqrt(re*re + im*im);
        if (r < 1e-300) return {0.0, 0.0};
        double theta = std::atan2(im, re);
        double s = std::log(r);
        double psi = TWO_PI_OVER_LN_LAMBDA * s;
        // Reduce to [0, 2)
        theta = std::fmod(theta, 2.0 * constants::PI);
        psi = std::fmod(psi, 2.0 * constants::PI);
        return {theta, psi};
    }

    /**
     * @brief Compute F(z) = i*phi*z (rotation-dilation)
     */
    static std::pair<double, double> fMap(double re, double im) {
        // i*phi*(re + i*im) = phi*(i*re - im) = (-phi*im + i*phi*re)
        return {-constants::PHI * im, constants::PHI * re};
    }

    /**
     * @brief Compute T(theta,psi) = (theta + pi/2, psi + pi/2) on torus
     */
    static std::pair<double, double> tStep(double theta, double psi) {
        return {theta + constants::PI / 2.0, psi + constants::PI / 2.0};
    }

    /**
     * @brief Compute torus Fourier mode f_{m,n}(theta,psi) = e^{i(m*theta+n*psi)}
     * Returns (Re, Im) pair
     */
    static std::pair<double, double> torusMode(int m, int n, double theta, double psi) {
        double phase = m * theta + n * psi;
        return {std::cos(phase), std::sin(phase)};
    }

    /**
     * @brief Check T-equivariance selection rule: m + n == 0 (mod 4)
     */
    static bool satisfiesSelectionRule(int m, int n) {
        return ((m + n) % 4 + 4) % 4 == 0;
    }

    /**
     * @brief Compute discrete-scale frequency omega_n = 2*pi*n/ln(Lambda)
     */
    static double omegaN(int n) {
        return TWO_PI_OVER_LN_LAMBDA * n;
    }

    /**
     * @brief Compute conformal weight h = 1/2*(kappa + i*omega_n + m)
     * Returns (Re(h), Im(h))
     */
    static std::pair<double, double> conformalWeight(double kappa, int n, int m) {
        double omega = omegaN(n);
        return {0.5 * (kappa + m), 0.5 * omega};
    }

    /**
     * @brief Compute U_{m,n,kappa}(z) = |z|^kappa * f_{m,n}(C_Lambda(z))
     * Returns (Re, Im) pair
     */
    static std::pair<double, double> scaleCovariantMode(
        double re, double im, int m, int n, double kappa) {
        double r = std::sqrt(re*re + im*im);
        if (r < 1e-300) return {0.0, 0.0};
        auto [theta, psi] = cayleyLambda(re, im);
        double rKappa = std::pow(r, kappa);
        auto [modeRe, modeIm] = torusMode(m, n, theta, psi);
        return {rKappa * modeRe, rKappa * modeIm};
    }

    /**
     * @brief Verify Lambda-covariance: U(Lambda*z) = Lambda^kappa * U(z)
     */
    static bool verifyScaleCovariance(double re, double im, int m, int n, 
                                       double kappa, double tol = 1e-10) {
        auto [u_re, u_im] = scaleCovariantMode(re, im, m, n, kappa);
        double lam_re = LAMBDA * re;
        double lam_im = LAMBDA * im;
        auto [ulam_re, ulam_im] = scaleCovariantMode(lam_re, lam_im, m, n, kappa);
        double scale = std::pow(LAMBDA, kappa);
        return std::abs(ulam_re - scale * u_re) < tol &&
               std::abs(ulam_im - scale * u_im) < tol;
    }

    /**
     * @brief Compute the Onsager critical exponent condition: 3*kappa - 1 = 0 => kappa = 1/3
     */
    static double onsagerExponent() {
        return 1.0 / 3.0;
    }

    /**
     * @brief Verify F-map equivariance: C_Lambda(F(z)) = T(C_Lambda(z))
     */
    static bool verifyEquivariance(double re, double im, double tol = 1e-10) {
        auto [f_re, f_im] = fMap(re, im);
        auto [fTheta, fPsi] = cayleyLambda(f_re, f_im);
        auto [theta, psi] = cayleyLambda(re, im);
        auto [tTheta, tPsi] = tStep(theta, psi);
        double dTheta = std::fmod(std::abs(fTheta - tTheta), 2.0 * constants::PI);
        double dPsi = std::fmod(std::abs(fPsi - tPsi), 2.0 * constants::PI);
        return (dTheta < tol || std::abs(dTheta - 2.0 * constants::PI) < tol) &&
               (dPsi < tol || std::abs(dPsi - 2.0 * constants::PI) < tol);
    }
};

// =============================================================================
// CAYLEY-LAMBDA AXIOM MODULE
// =============================================================================

/**
 * @brief Generates axioms for the Cayley-Lambda framework
 * 
 * These axioms encode the torus geometry, F-map dynamics, scale covariance,
 * Fourier modes, selection rules, conformal weights, and the Onsager exponent.
 */
class CayleyLambdaModule {
public:
    explicit CayleyLambdaModule(TermFactory& factory) : factory_(factory) {}

    /**
     * @brief Generate ALL Cayley-Lambda axioms
     */
    [[nodiscard]] std::vector<std::unique_ptr<Equation>> generateAxioms() {
        std::vector<std::unique_ptr<Equation>> axioms;

        generateCoreMapAxioms(axioms);
        generateFMapAxioms(axioms);
        generateScaleCovariantAxioms(axioms);
        generateTorusFourierAxioms(axioms);
        generateConformalWeightAxioms(axioms);
        generateOnsagerAxioms(axioms);
        generateSpectrumAxioms(axioms);

        return axioms;
    }

private:
    TermFactory& factory_;
    uint32_t varCounter_ = 0;

    const Term* freshVar(Sort sort = Sort::Generic) {
        return factory_.variable("cl" + std::to_string(varCounter_++), sort);
    }

    // =========================================================================
    // CORE MAP AXIOMS -- C_Lambda, Lambda=phi^4, quotient structure
    // =========================================================================
    void generateCoreMapAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto z      = freshVar();
        auto phi    = factory_.phi();
        auto one    = factory_.scalar(1.0);

        // CL1: Lambda = phi^4
        // Lambda = mul(mul(phi, phi), mul(phi, phi))
        auto phi2 = factory_.mul(phi, phi);
        auto phi4 = factory_.mul(phi2, phi2);
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("Lambda", {}),
            phi4
        ));

        // CL2: C_Lambda(Lambda*z) = C_Lambda(z)  [scale periodicity]
        // The Cayley-Lambda map is periodic under Lambda-scaling
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("CLambda", {factory_.mul(factory_.apply("Lambda", {}), z)}),
            factory_.apply("CLambda", {z})
        ));

        // CL3: F^4(z) = Lambda*z  [F-map period-4 gives Lambda-scaling]
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("FMap", {factory_.apply("FMap", {
                factory_.apply("FMap", {factory_.apply("FMap", {z})})})}),
            factory_.mul(factory_.apply("Lambda", {}), z)
        ));

        // CL4: T^4 = id  [torus step has order 4]
        auto theta = freshVar();
        auto psi   = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("TStep", {factory_.apply("TStep", {
                factory_.apply("TStep", {factory_.apply("TStep", {theta, psi})})})}),
            factory_.apply("TorusPt", {theta, psi})
        ));

        // CL5: CLambda(F(z)) = TStep(CLambda(z))  [EQUIVARIANCE]
        // This is THE central structural equation:
        // bulk dynamics (F) <-> boundary dynamics (T)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("CLambda", {factory_.apply("FMap", {z})}),
            factory_.apply("TStep", {factory_.apply("CLambda", {z})})
        ));

        // CL6: Hopf manifold isomorphism
        // C*/z~Lambda*z -> T^2  encoded as:
        // CLambda(z) = CLambda(w) <=> exists k: w = Lambda^k * z
        // We encode the forward direction:
        auto k = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("CLambda", {
                factory_.mul(factory_.apply("LambdaPow", {k}), z)}),
            factory_.apply("CLambda", {z})
        ));

        // CL7: LambdaPow(0) = 1
        auto zero = factory_.scalar(0.0);
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("LambdaPow", {zero}),
            one
        ));

        // CL8: LambdaPow(1) = Lambda
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("LambdaPow", {one}),
            factory_.apply("Lambda", {})
        ));

        // CL9: LambdaPow(k+1) = Lambda * LambdaPow(k)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("LambdaPow", {factory_.add(k, one)}),
            factory_.mul(factory_.apply("Lambda", {}), factory_.apply("LambdaPow", {k}))
        ));
    }

    // =========================================================================
    // F-MAP AXIOMS -- F(z) = i*phi*z dynamics
    // =========================================================================
    void generateFMapAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto z    = freshVar();
        auto phi  = factory_.phi();
        auto J    = factory_.J();

        // FM1: F(z) = J*phi*z  [THE fundamental map]
        // This connects J (imaginary unit) and phi (golden ratio)
        // into a SINGLE dynamical operation
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("FMap", {z}),
            factory_.mul(J, factory_.mul(phi, z))
        ));

        // FM2: |F(z)|^2 = phi^2 * |z|^2  [F scales norm by phi]
        axioms.push_back(std::make_unique<Equation>(
            factory_.norm(factory_.apply("FMap", {z})),
            factory_.mul(factory_.mul(phi, phi), factory_.norm(z))
        ));

        // FM3: F(z)*conj(F(z)) = phi^2 * z*conj(z)
        // Norm compatibility through the CD conjugation
        axioms.push_back(std::make_unique<Equation>(
            factory_.mul(factory_.apply("FMap", {z}), 
                        factory_.conj(factory_.apply("FMap", {z}))),
            factory_.mul(factory_.mul(phi, phi),
                        factory_.mul(z, factory_.conj(z)))
        ));

        // FM4: F(F(z)) = J^2*phi^2*z = -phi^2*z  [since J^2=-1]
        // This is DERIVABLE from FM1 + J=-1, but explicitly stated
        // for the engine to discover the connection quickly
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("FMap", {factory_.apply("FMap", {z})}),
            factory_.neg(factory_.mul(factory_.mul(phi, phi), z))
        ));
    }

    // =========================================================================
    // SCALE-COVARIANT SPACE AXIOMS -- U_kappa decomposition
    // =========================================================================
    void generateScaleCovariantAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto z     = freshVar();
        auto kappa = freshVar();
        auto V     = freshVar();  // torus function V(theta,psi)

        // SC1: Scale-covariant decomposition
        // U(z) = |z|^kappa * V(C_Lambda(z))
        // ScaleDecomp(z, kappa, V) encodes U(z) = |z|^kappa * V(CLambda(z))
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("ScaleDecomp", {z, kappa, V}),
            factory_.mul(
                factory_.apply("RadialPow", {z, kappa}),
                factory_.apply("TorusEval", {V, factory_.apply("CLambda", {z})})
            )
        ));

        // SC2: RadialPow(z, 0) = 1
        auto zero = factory_.scalar(0.0);
        auto one  = factory_.scalar(1.0);
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("RadialPow", {z, zero}),
            one
        ));

        // SC3: RadialPow(Lambda*z, kappa) = Lambda^kappa * RadialPow(z, kappa)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("RadialPow", {
                factory_.mul(factory_.apply("Lambda", {}), z), kappa}),
            factory_.mul(
                factory_.apply("LambdaPow_kappa", {kappa}),
                factory_.apply("RadialPow", {z, kappa}))
        ));

        // SC4: Scale covariance identity
        // U(Lambda*z) = Lambda^kappa * U(z)  [THE defining property of U_kappa]
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("ScaleDecomp", {
                factory_.mul(factory_.apply("Lambda", {}), z), kappa, V}),
            factory_.mul(
                factory_.apply("LambdaPow_kappa", {kappa}),
                factory_.apply("ScaleDecomp", {z, kappa, V}))
        ));

        // SC5: PDE closed on torus
        // partial_t V = L[V] on (theta,psi) when L has similarity covariance
        // ScaleDecomp_PDE(V, L) means partial_t V = L[V] on T^2
        // This is the CLOSURE theorem: bulk PDE -> torus PDE
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("BulkPDE_Closure", {V}),
            factory_.apply("TorusPDE", {V})
        ));
    }

    // =========================================================================
    // TORUS FOURIER MODE AXIOMS -- f_{m,n} and selection rule
    // =========================================================================
    void generateTorusFourierAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto m     = freshVar();
        auto n     = freshVar();
        auto one   = factory_.scalar(1.0);
        auto zero  = factory_.scalar(0.0);

        // TF1: Torus mode product
        // f_{m,n} * f_{m',n'} = f_{m+m', n+n'}
        auto m2 = freshVar();
        auto n2 = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.mul(
                factory_.apply("TorusMode", {m, n}),
                factory_.apply("TorusMode", {m2, n2})),
            factory_.apply("TorusMode", {
                factory_.add(m, m2), factory_.add(n, n2)})
        ));

        // TF2: Torus mode conjugate
        // conj(f_{m,n}) = f_{-m,-n}
        axioms.push_back(std::make_unique<Equation>(
            factory_.conj(factory_.apply("TorusMode", {m, n})),
            factory_.apply("TorusMode", {factory_.neg(m), factory_.neg(n)})
        ));

        // TF3: Torus mode norm
        // |f_{m,n}| = 1 (unit norm on torus)
        axioms.push_back(std::make_unique<Equation>(
            factory_.norm(factory_.apply("TorusMode", {m, n})),
            one
        ));

        // TF4: Zero mode = 1
        // f_{0,0} = 1
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("TorusMode", {zero, zero}),
            one
        ));

        // TF5: T-EQUIVARIANCE SELECTION RULE
        // f_{m,n} o T = e^{i*(m+n)*pi/2} * f_{m,n}
        // f_{m,n} is T-invariant iff m + n == 0 (mod 4)
        // SelectionRule(m, n) = 1 when m+n == 0 mod 4, 0 otherwise
        // TStep transforms a mode by multiplying by phase factor:
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("ModeAfterT", {m, n}),
            factory_.mul(
                factory_.apply("ExpJ", {
                    factory_.mul(factory_.add(m, n),
                                factory_.mul(factory_.apply("Pi", {}),
                                            factory_.inv(factory_.scalar(2.0))))}),
                factory_.apply("TorusMode", {m, n}))
        ));

        // TF6: T-invariant modes have m+n == 0 (mod 4)
        // When ModeAfterT(m,n) = TorusMode(m,n), the phase factor must be 1
        // Phase factor e^{i*(m+n)*pi/2} = 1 iff (m+n) mod 4 = 0
        // Mod4(m+n) = 0 encodes this constraint
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("TInvariance", {m, n}),
            factory_.apply("Mod4Zero", {factory_.add(m, n)})
        ));

        // TF7: Y operator -- combined theta + s derivative
        // Y = X_theta + (ln(phi)/2)*X_s
        // Y*U_{m,n,kappa} = i*(m+n)*U_{m,n,kappa}
        auto kappa = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("YOperator", {m, n, kappa}),
            factory_.mul(
                factory_.mul(factory_.J(), factory_.add(m, n)),
                factory_.apply("ScaleMode", {m, n, kappa}))
        ));
    }

    // =========================================================================
    // CONFORMAL WEIGHT AXIOMS -- h, h_tilde from CFT
    // =========================================================================
    void generateConformalWeightAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto kappa  = freshVar();
        auto m      = freshVar();
        auto n      = freshVar();
        auto two    = factory_.scalar(2.0);
        auto J      = factory_.J();

        // CW1: Conformal weight h = 1/2*(kappa + i*omega_n + m)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("ConfWeight_h", {kappa, n, m}),
            factory_.mul(factory_.inv(two),
                factory_.add(factory_.add(kappa, m),
                    factory_.mul(J, factory_.apply("OmegaN", {n}))))
        ));

        // CW2: Conformal weight h_tilde = 1/2*(kappa + i*omega_n - m)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("ConfWeight_htilde", {kappa, n, m}),
            factory_.mul(factory_.inv(two),
                factory_.add(factory_.add(kappa, factory_.neg(m)),
                    factory_.mul(J, factory_.apply("OmegaN", {n}))))
        ));

        // CW3: h + h_tilde = kappa + i*omega_n  [total scaling dimension]
        axioms.push_back(std::make_unique<Equation>(
            factory_.add(
                factory_.apply("ConfWeight_h", {kappa, n, m}),
                factory_.apply("ConfWeight_htilde", {kappa, n, m})),
            factory_.add(kappa, factory_.mul(J, factory_.apply("OmegaN", {n})))
        ));

        // CW4: h - h_tilde = m  [spin]
        axioms.push_back(std::make_unique<Equation>(
            factory_.add(
                factory_.apply("ConfWeight_h", {kappa, n, m}),
                factory_.neg(factory_.apply("ConfWeight_htilde", {kappa, n, m}))),
            m
        ));

        // CW5: Holomorphic condition
        // (X_theta - i*X_s)U = 0 => partial_zbar U = 0
        // Encoded: Antiholomorphic(U) = 0 means U is holomorphic
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("Holomorphic", {
                factory_.apply("ScaleMode", {m, factory_.scalar(0.0), kappa})}),
            factory_.scalar(0.0)
        ));
    }

    // =========================================================================
    // ONSAGER CRITICAL EXPONENT AXIOMS
    // =========================================================================
    void generateOnsagerAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto kappa = freshVar();
        auto one   = factory_.scalar(1.0);
        auto third = factory_.mul(one, factory_.inv(factory_.scalar(3.0)));

        // ON1: Energy flux scale law: D(u)(Lambda*x) = Lambda^{3*kappa-1} * D(u)(x)
        // Defect(Lambda*x) = Lambda^{3*kappa-1} * Defect(x)
        auto x = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("DefectScaling", {kappa}),
            factory_.add(factory_.mul(factory_.scalar(3.0), kappa),
                        factory_.neg(one))
        ));

        // ON2: Critical exponent: 3*kappa - 1 = 0 => kappa = 1/3
        // When DefectScaling = 0, the defect is scale-invariant
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("OnsagerKappa", {}),
            third
        ));

        // ON3: At kappa = 1/3, D(u) lives on T^2
        // Defect(u) = CLambda*(D_hat(theta,psi))
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("DefectAtCritical", {x}),
            factory_.apply("TorusEval", {
                factory_.apply("DefectHat", {}),
                factory_.apply("CLambda", {x})})
        ));

        // ON4: beta = 1 - kappa (time scaling exponent from Euler invariance)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("TimeScaleExp", {kappa}),
            factory_.add(one, factory_.neg(kappa))
        ));

        // ON5: At critical: beta = 2/3
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("TimeScaleExp", {factory_.apply("OnsagerKappa", {})}),
            factory_.mul(factory_.scalar(2.0), factory_.inv(factory_.scalar(3.0)))
        ));
    }

    // =========================================================================
    // DISCRETE-SCALE SPECTRUM AXIOMS
    // =========================================================================
    void generateSpectrumAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto n    = freshVar();
        auto zero = factory_.scalar(0.0);
        auto two  = factory_.scalar(2.0);

        // SP1: OmegaN definition: omega_n = 2*pi*n/ln(Lambda)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("OmegaN", {n}),
            factory_.mul(n, factory_.apply("OmegaBase", {}))
        ));

        // SP2: OmegaBase = 2*pi/ln(Lambda) = 2*pi/(4*ln(phi)) = pi/(2*ln(phi))
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("OmegaBase", {}),
            factory_.mul(two,
                factory_.mul(factory_.apply("Pi", {}),
                    factory_.inv(factory_.apply("LnLambda", {}))))
        ));

        // SP3: ln(Lambda) = 4*ln(phi)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("LnLambda", {}),
            factory_.mul(factory_.scalar(4.0), factory_.apply("LnPhi", {}))
        ));

        // SP4: omega_0 = 0 (zero mode = pure scaling)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("OmegaN", {zero}),
            zero
        ));

        // SP5: Floquet multiplier for F-step:
        // U(F(z)) = phi^kappa * e^{i*(m+n)*pi/2} * U(z)
        auto kappa = freshVar();
        auto m     = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("FloquetMult", {kappa, m, n}),
            factory_.mul(
                factory_.apply("PhiPow", {kappa}),
                factory_.apply("ExpJ", {
                    factory_.mul(factory_.add(m, n),
                                factory_.mul(factory_.apply("Pi", {}),
                                            factory_.inv(two)))}))
        ));
    }
};

} // namespace domain
} // namespace autodiscover

#endif // AUTODISCOVER_DOMAIN_CAYLEY_LAMBDA_HPP
