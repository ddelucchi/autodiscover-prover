/**
 * @file HybridCalculus.hpp
 * @brief Hybrid PDE+Discrete Operator, Onsager-Floquet Analysis,
 *        Renormalization Group Flow, and Critical Phenomena
 * See calculusnumberunification.txt for full mathematical details.
 */

#ifndef AUTODISCOVER_DOMAIN_HYBRID_CALCULUS_HPP
#define AUTODISCOVER_DOMAIN_HYBRID_CALCULUS_HPP

#include "../core/Term.hpp"
#include "../core/Constants.hpp"
#include "../logic/Equation.hpp"
#include <vector>
#include <memory>
#include <string>

namespace autodiscover {
namespace domain {

using core::Term;
using core::TermFactory;
using core::Sort;
using logic::Equation;

// =============================================================================
// HYBRID CALCULUS AXIOM MODULE
// =============================================================================

/**
 * @brief Generates axioms for the hybrid PDE+discrete operator,
 *        Floquet analysis, RG flow, and critical phenomena
 */
class HybridCalculusModule {
public:
    explicit HybridCalculusModule(TermFactory& factory) : factory_(factory) {}

    /**
     * @brief Generate ALL hybrid calculus axioms
     */
    [[nodiscard]] std::vector<std::unique_ptr<Equation>> generateAxioms() {
        std::vector<std::unique_ptr<Equation>> axioms;

        generateHybridOperatorAxioms(axioms);
        generateFloquetAxioms(axioms);
        generateRGFlowAxioms(axioms);
        generateCriticalPhenomenaAxioms(axioms);
        generateLogPeriodicAxioms(axioms);
        generateFourFifthsLawAxioms(axioms);
        generateIntermittencyAxioms(axioms);

        return axioms;
    }

private:
    TermFactory& factory_;
    uint32_t varCounter_ = 0;

    const Term* freshVar(Sort sort = Sort::Generic) {
        return factory_.variable("hc" + std::to_string(varCounter_++), sort);
    }

    // =========================================================================
    // HYBRID OPERATOR AXIOMS  H = _' + (T* - id)
    // =========================================================================
    void generateHybridOperatorAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto V     = freshVar(); // torus function
        auto W     = freshVar();
        auto gamma = freshVar(); // coupling

        // HO1: Hybrid operator definition
        // H(V) = _'(V) + (T*(V) - V)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("HybridOp", {gamma, V}),
            factory_.add(
                factory_.apply("ScaledTimeDeriv", {V}),
                factory_.mul(gamma,
                    factory_.add(
                        factory_.apply("TStarPullback", {V}),
                        factory_.neg(V))))
        ));

        // HO2: Hybrid operator is linear
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("HybridOp", {gamma, factory_.add(V, W)}),
            factory_.add(
                factory_.apply("HybridOp", {gamma, V}),
                factory_.apply("HybridOp", {gamma, W}))
        ));

        // HO3: T* on torus Fourier modes
        // T*(f_{m,n}) = f_{m,n}(T(,)) = e^{i(m+n)/2}  f_{m,n}
        auto m = freshVar();
        auto n = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("TStarPullback", {factory_.apply("TorusMode", {m, n})}),
            factory_.mul(
                factory_.apply("ExpJ", {
                    factory_.mul(factory_.add(m, n),
                                factory_.mul(factory_.apply("Pi", {}),
                                            factory_.inv(factory_.scalar(2.0))))}),
                factory_.apply("TorusMode", {m, n}))
        ));

        // HO4: Hybrid operator on modes
        // H[f_{m,n}] = (_' + (e^{i(m+n)/2} - 1)) f_{m,n}
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("HybridOp", {gamma, 
                factory_.apply("TorusMode", {m, n})}),
            factory_.add(
                factory_.apply("ScaledTimeDeriv", {
                    factory_.apply("TorusMode", {m, n})}),
                factory_.mul(gamma,
                    factory_.mul(
                        factory_.add(
                            factory_.apply("ExpJ", {
                                factory_.mul(factory_.add(m, n),
                                    factory_.mul(factory_.apply("Pi", {}),
                                                factory_.inv(factory_.scalar(2.0))))}),
                            factory_.neg(factory_.scalar(1.0))),
                        factory_.apply("TorusMode", {m, n}))))
        ));

        // HO5: T*-id kills modes with m+n  0 (mod 4)
        // For T-invariant modes: T*(f)-f = 0
        // These modes are in ker(T*-id)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("TStarMinusId", {
                factory_.apply("InvariantMode", {m, n})}),
            factory_.scalar(0.0)
        ));

        // HO6: Connection to -derivation:
        // (T* - id) IS a -derivation with  = T*
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("TStarMinusId", {V}),
            factory_.apply("SigmaDeriv", {factory_.apply("SigmaTStar", {}), V})
        ));
    }

    // =========================================================================
    // FLOQUET AXIOMS  period-4 T-step structure
    // =========================================================================
    void generateFloquetAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto V     = freshVar();
        auto kappa = freshVar();

        // FL1: T = ^  id (up to scaling)
        //  = 1- is the time-scaling exponent
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("TStarPullback", {
                factory_.apply("TStarPullback", {
                    factory_.apply("TStarPullback", {
                        factory_.apply("TStarPullback", {V})})})}),
            factory_.mul(
                factory_.apply("LambdaPow_kappa", {
                    factory_.apply("TimeScaleExp", {kappa})}),
                V)
        ));

        // FL2: Floquet multiplier for mode k
        // _k = ^{/4}  e^{2ik/4} = ^  J^k
        auto k = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("FloquetMult_k", {kappa, k}),
            factory_.mul(
                factory_.apply("PhiPow", {
                    factory_.apply("TimeScaleExp", {kappa})}),
                factory_.apply("ExpJ", {
                    factory_.mul(k,
                        factory_.mul(factory_.apply("Pi", {}),
                                    factory_.inv(factory_.scalar(2.0))))}))
        ));

        // FL3: Resonance condition
        // (_k - 1) = _n (PDE eigenvalue)
        auto gamma  = freshVar();
        auto lambda_n = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.mul(gamma,
                factory_.add(
                    factory_.apply("FloquetMult_k", {kappa, k}),
                    factory_.neg(factory_.scalar(1.0)))),
            lambda_n
        ));

        // FL4: Floquet exponents sum:  = ^
        axioms.push_back(std::make_unique<Equation>(
            factory_.mul(
                factory_.mul(
                    factory_.apply("FloquetMult_k", {kappa, factory_.scalar(0.0)}),
                    factory_.apply("FloquetMult_k", {kappa, factory_.scalar(1.0)})),
                factory_.mul(
                    factory_.apply("FloquetMult_k", {kappa, factory_.scalar(2.0)}),
                    factory_.apply("FloquetMult_k", {kappa, factory_.scalar(3.0)}))),
            factory_.apply("LambdaPow_kappa", {
                factory_.apply("TimeScaleExp", {kappa})})
        ));

        // FL5: At critical (=1/3, =2/3):
        // _k = ^{2/3}  i^k
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("FloquetMult_k", {
                factory_.apply("OnsagerKappa", {}), k}),
            factory_.mul(
                factory_.apply("PhiPow", {
                    factory_.mul(factory_.scalar(2.0),
                                factory_.inv(factory_.scalar(3.0)))}),
                factory_.apply("ExpJ", {
                    factory_.mul(k,
                        factory_.mul(factory_.apply("Pi", {}),
                                    factory_.inv(factory_.scalar(2.0))))}))
        ));
    }

    // =========================================================================
    // RENORMALIZATION GROUP FLOW AXIOMS
    // =========================================================================
    void generateRGFlowAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto V     = freshVar(); // torus function
        auto kappa = freshVar();
        auto one   = factory_.scalar(1.0);

        // RG1: RG equation on torus
        // dV/dl = (-1)V + N(V)
        // where l = -ln(r)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("RGFlow", {kappa, V}),
            factory_.add(
                factory_.mul(
                    factory_.add(kappa, factory_.neg(one)),
                    V),
                factory_.apply("NonlinearSelf", {V}))
        ));

        // RG2: Fixed point condition
        // V* satisfies: (-1)V* + N(V*) = 0
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("RGFlow", {kappa,
                factory_.apply("RGFixedPt", {kappa})}),
            factory_.scalar(0.0)
        ));

        // RG3: At critical =1/3:
        // dV/dl = -V + N(V)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("RGFlow", {factory_.apply("OnsagerKappa", {}), V}),
            factory_.add(
                factory_.mul(
                    factory_.neg(factory_.mul(
                        factory_.scalar(2.0),
                        factory_.inv(factory_.scalar(3.0)))),
                    V),
                factory_.apply("NonlinearSelf", {V}))
        ));

        // RG4: Linearized RG at fixed point
        // dV/dl = (-1+N'(V*))V
        // Eigenvalues of linearized RG = anomalous dimensions
        auto deltaV = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("RGLinearized", {kappa, deltaV}),
            factory_.mul(
                factory_.add(
                    factory_.add(kappa, factory_.neg(one)),
                    factory_.apply("NonlinearDeriv", {
                        factory_.apply("RGFixedPt", {kappa})})),
                deltaV)
        ));

        // RG5: RG eigenvalue = anomalous dimension
        // _n = eigenvalue of linearized RG at order n
        auto n_mode = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("AnomalousDim", {kappa, n_mode}),
            factory_.apply("RGEigenvalue", {kappa, n_mode})
        ));

        // RG6: RG and torus geometry compatibility
        // The RG flow preserves the T-equivariant structure
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("TStarPullback", {
                factory_.apply("RGFlow", {kappa, V})}),
            factory_.apply("RGFlow", {kappa,
                factory_.apply("TStarPullback", {V})})
        ));
    }

    // =========================================================================
    // CRITICAL PHENOMENA AXIOMS
    // =========================================================================
    void generateCriticalPhenomenaAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto r   = freshVar(); // separation distance

        // CP1: Structure function scaling
        // S_p(r) = |u(r)|^p ~ r^{_p}
        auto p = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("StructureFunc", {p, r}),
            factory_.apply("Pow", {r, factory_.apply("Zeta", {p})})
        ));

        // CP2: Kolmogorov scaling (mean field): _p = p/3
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("Zeta_MeanField", {p}),
            factory_.mul(p, factory_.inv(factory_.scalar(3.0)))
        ));

        // CP3: Anomalous scaling: _p = p/3 + _p
        // where _p are intermittency corrections
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("Zeta", {p}),
            factory_.add(
                factory_.mul(p, factory_.inv(factory_.scalar(3.0))),
                factory_.apply("Intermittency", {p}))
        ));

        // CP4: _3 = 1 exactly (the four-fifths law constraint)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("Zeta", {factory_.scalar(3.0)}),
            factory_.scalar(1.0)
        ));

        // CP5: Therefore _3 = 0 (no intermittency correction at p=3)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("Intermittency", {factory_.scalar(3.0)}),
            factory_.scalar(0.0)
        ));

        // CP6: Intermittency from torus geometry
        // _p arises from the DISCRETE spectrum of the torus
        // _p = _n c_n(p)  cos(_n  ln(r/r*))
        // where _n = 2n/ln() are the discrete-scale frequencies
        auto n = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("Intermittency_Torus", {p, n}),
            factory_.mul(
                factory_.apply("IntermittencyCoeff", {p, n}),
                factory_.apply("LogPeriodicCos", {n, r}))
        ));
    }

    // =========================================================================
    // LOG-PERIODIC OSCILLATION AXIOMS
    // =========================================================================
    void generateLogPeriodicAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto r   = freshVar();
        auto n   = freshVar();

        // LP1: Log-periodic oscillation definition
        // cos(_n  ln(r/r*)) with _n = 2n/ln()
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("LogPeriodicCos", {n, r}),
            factory_.apply("Cos", {
                factory_.mul(
                    factory_.apply("OmegaN", {n}),
                    factory_.apply("Ln", {r}))})
        ));

        // LP2: Period of log-periodic oscillation = ln() = 4ln()
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("LogPeriod", {}),
            factory_.apply("LnLambda", {})
        ));

        // LP3: Log-periodic -invariance
        // cos(_nln(r)) = cos(_nln(r) + _nln())
        //                   = cos(_nln(r) + 2n) = cos(_nln(r))
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("LogPeriodicCos", {n,
                factory_.mul(factory_.apply("Lambda", {}), r)}),
            factory_.apply("LogPeriodicCos", {n, r})
        ));

        // LP4: The n=0 mode is constant (no oscillation)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("LogPeriodicCos", {factory_.scalar(0.0), r}),
            factory_.scalar(1.0)
        ));

        // LP5: Observable consequence:
        // Structure functions have log-periodic corrections
        // S_p(r) = r^{_p}  (1 + _n A_n  cos(_n  ln(r)))
        auto p = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("StructureFunc_Full", {p, r}),
            factory_.mul(
                factory_.apply("Pow", {r, factory_.apply("Zeta", {p})}),
                factory_.add(factory_.scalar(1.0),
                    factory_.apply("LogPeriodicSum", {p, r})))
        ));
    }

    // =========================================================================
    // FOUR-FIFTHS LAW AXIOMS  exact relation from dissipation anomaly
    // =========================================================================
    void generateFourFifthsLawAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto r = freshVar();

        // FF1: The Kolmogorov 4/5 law
        // (u) = -(4/5)r
        // This is exact, derived from anomalous energy dissipation
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("ThirdMoment", {r}),
            factory_.neg(factory_.mul(
                factory_.mul(factory_.scalar(4.0),
                            factory_.inv(factory_.scalar(5.0))),
                factory_.mul(factory_.apply("Epsilon", {}), r)))
        ));

        // FF2: Energy dissipation rate  = lim_{0} |u|
        // At critical =1/3:  > 0 (anomalous dissipation)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("Epsilon_Anomalous", {}),
            factory_.apply("DefectIntegral", {
                factory_.apply("OnsagerKappa", {})})
        ));

        // FF3: 4/5 law is the p=3 case of _3 = 1
        // S_3(r) = |u| ~ r^{_3} = r^1
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("StructureFunc", {factory_.scalar(3.0), r}),
            factory_.mul(factory_.apply("Epsilon", {}), r)
        ));

        // FF4: The 4/5 law constant derives from the defect
        // -(4/5) comes from the 3D geometry of Euler equation
        // In dimension d: constant = -4/(d+1)  d/(d-1)
        // d=3: -4/4  3/2 = -3/2... actually -4/5 is the exact value
        // We encode: FOUR_FIFTHS = 4/5
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("FourFifthsConst", {}),
            factory_.mul(factory_.scalar(4.0), factory_.inv(factory_.scalar(5.0)))
        ));
    }

    // =========================================================================
    // INTERMITTENCY CORRECTION AXIOMS
    // =========================================================================
    void generateIntermittencyAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto p = freshVar();

        // IC1: She-Lvque model from torus geometry
        // _p = hierarchy of torus corrections
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("Intermittency_SL", {p}),
            factory_.mul(
                factory_.neg(factory_.scalar(2.0)),
                factory_.add(
                    factory_.mul(p, factory_.inv(factory_.scalar(9.0))),
                    factory_.add(
                        factory_.scalar(1.0),
                        factory_.neg(
                            factory_.apply("Pow", {
                                factory_.mul(factory_.scalar(2.0),
                                            factory_.inv(factory_.scalar(3.0))),
                                factory_.mul(p, factory_.inv(factory_.scalar(3.0)))})))))
        ));

        // IC2: _0 = 0 (trivially)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("Intermittency", {factory_.scalar(0.0)}),
            factory_.scalar(0.0)
        ));

        // IC3: Concavity: _p is concave as function of p
        // This is essential for multifractal formalism
        // d/dp  0
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("IntermittencyConcavity", {p}),
            factory_.apply("NonPositive", {
                factory_.apply("SecondDeriv_p", {
                    factory_.apply("Intermittency", {p})})})
        ));

        // IC4: Multifractal spectrum from Legendre transform
        // f() = min_p (p - _p + 1)
        // where  = d_p/dp
        auto alpha_mf = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("MultifractalSpectrum", {alpha_mf}),
            factory_.apply("LegendreTransform", {
                factory_.apply("Zeta", {}), alpha_mf})
        ));

        // IC5: The torus T provides the geometric origin:
        // Each (m,n) mode contributes to intermittency
        // through its Floquet multiplier
        auto m = freshVar();
        auto n = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("IntermittencyFromTorus", {p, m, n}),
            factory_.mul(
                factory_.apply("Pow", {
                    factory_.norm(factory_.apply("FloquetMult", {
                        factory_.apply("OnsagerKappa", {}), m, n})),
                    p}),
                factory_.apply("SR_Allow", {m, n}))
        ));
    }
};

} // namespace domain
} // namespace autodiscover

#endif // AUTODISCOVER_DOMAIN_HYBRID_CALCULUS_HPP
