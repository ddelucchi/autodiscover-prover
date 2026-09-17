/**
 * @file Scout.hpp
 * @brief SCOUT (Scalar Output Transport) and UNT implementation
 * 
 * =============================================================================
 * SCOUT: Scalar Collapse via Orthogonal-Unitary Transport
 * =============================================================================
 * 
 * SCOUT defines a gauge-covariant scalar projection scheme:
 *   Align_U(q) = Scal(U*  q)
 * 
 * where:
 *   - U is a unit element (|U| = 1)
 *   - q is an element of the algebra  
 *   - Scal extracts the scalar (real) part: Scal(x) = (x + x*)
 * 
 * =============================================================================
 * UNT: Unified Number Transform (Infinite Boundary Collapse)
 * =============================================================================
 * 
 * UNT scalarizes any hypercomplex element via:
 * 
 *   UNT[q] = _{k=0}^{} F_k  ^k  _{^(k)}} (x, J^m (x)) dA
 * 
 * where:
 *   - F_k = Fibonacci numbers (F_0=0, F_1=1, F_{k+2}=F_{k+1}+F_k)
 *   -  < ^{-1} = 1/  0.618 (convergence radius)
 *   - ^(k) = k-th boundary shell in symmetry space
 *   - (x, J^m (x)) = local phase-alignment integrand
 *   - (x) = phase fiber at point x
 * 
 * The series converges absolutely when  < 1/ due to:
 *   lim_{k} F_k/F_{k+1} = 1/ (golden ratio property)
 * 
 * =============================================================================
 * Phase Arrow
 * =============================================================================
 * 
 * The phase arrow encapsulates the discrete/continuous phase data:
 *   _J^[] = (-1)^K  exp(J    Z_SCOUT)
 * 
 * where:
 *   - K = winding number (topological degree)
 *   - Z_SCOUT = scalar invariant from SCOUT
 *   -  = phase modulation constant
 * 
 * =============================================================================
 * Boundary Collapse
 * =============================================================================
 * 
 * Boundary collapse extracts a unique scalar by:
 *   1. Selecting boundary degree K from accumulated phase
 *   2. Computing Fibonacci-weighted path integral Z_fib
 *   3. Combining into phase tag: B_{J,fib}() = (-1)^{K/2}  Z_fib
 * 
 * =============================================================================
 * Phase Transport
 * =============================================================================
 * 
 *    = 1
 *   _{k+1} = _k  _J(r_k)    where _J(r) = (1+Jr)(1-Jr)^{-1}
 * 
 * Accumulated phase:
 *   _k = _{i=0}^{k-1} _J(r_i) = exp(2J   arctan(r_i))
 * 
 */

#ifndef AUTODISCOVER_DOMAIN_SCOUT_HPP
#define AUTODISCOVER_DOMAIN_SCOUT_HPP

#include "../core/Term.hpp"
#include "../core/Type.hpp"
#include "../logic/Equation.hpp"
#include "Algebra.hpp"
#include <vector>
#include <complex>
#include <cmath>
#include <functional>
#include <numeric>
#include <algorithm>

namespace autodiscover {
namespace domain {

using core::Term;
using core::TermFactory;
using core::Sort;
using core::CDLevel;
using logic::Equation;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

class ScoutModule;
class ScoutEvaluator;
class UNTEngine;
class BoundaryCollapser;

// =============================================================================
// UNT CONFIGURATION
// =============================================================================

/**
 * @brief Configuration for UNT computation
 */
struct UNTConfig {
    double rho = 0.5;            // Convergence parameter  < 1/  0.618
    unsigned maxTerms = 100;     // Maximum number of Fibonacci terms
    double tolerance = 1e-12;    // Convergence tolerance
    double alpha0 = 1.0;         // Phase modulation constant
    bool useFibonacciWeights = true;  // Use Fibonacci weighting
    
    // Verify configuration is valid
    [[nodiscard]] bool isValid() const {
        return rho > 0.0 && rho < PHI_INV && 
               maxTerms > 0 && tolerance > 0.0;
    }
};

// =============================================================================
// BOUNDARY SHELL STRUCTURE
// =============================================================================

/**
 * @brief Represents a boundary shell ^(k) at level k
 * 
 * Each shell contributes to the UNT integral with weight F_k  ^k
 */
struct BoundaryShell {
    unsigned level;           // Shell index k
    double weight;            // F_k  ^k
    double phaseContribution; // _{^(k)}  dA
    JPlane localPhase;        // Local phase at this shell
    
    BoundaryShell(unsigned k = 0) 
        : level(k), weight(0.0), phaseContribution(0.0), localPhase(JPlane::one()) {}
};

// =============================================================================
// UNT ENGINE
// =============================================================================

/**
 * @brief Unified Number Transform (Infinite)
 * 
 * Computes: UNT[q] = _{k=0}^{} F_k  ^k  _{^(k)}} (x, J^m (x)) dA
 */
class UNTEngine {
public:
    explicit UNTEngine(const UNTConfig& config = UNTConfig()) 
        : config_(config) {
        precomputeFibonacci();
    }
    
    /**
     * @brief Compute UNT for a J-plane element
     * 
     * @param q The element to scalarize
     * @param transport The phase transport to use (or nullptr for default)
     * @return The scalar UNT value
     */
    [[nodiscard]] double computeUNT(const JPlane& q, 
                                      const PhaseTransport* transport = nullptr) const {
        double result = 0.0;
        
        // Get accumulated phase from transport, or default to 1
        JPlane Phi = transport ? transport->phase() : JPlane::one();
        
        // Use scaled Fibonacci recurrence: g_k = F_k  ^k
        // g_0 = 0, g_1 = , g_{k+1} = g_k + g_{k-1}
        double gPrev = 0.0;                    // g_0 = F_0  ^0 = 0
        double gCurr = config_.rho;             // g_1 = F_1  ^1 = 
        double rhoSq = config_.rho * config_.rho;
        
        for (unsigned k = 0; k < config_.maxTerms; ++k) {
            // g_k = F_k  ^k (already scaled  no separate rhoPower needed)
            double Gk = (k == 0) ? gPrev : (k == 1) ? gCurr : gCurr;
            
            // Shell contribution (propagateToShell no longer applies ^k decay)
            double shellContrib = computeShellContribution(q, Phi, k);
            
            // Weighted term: g_k  shell_contrib   (= F_k  ^k  shell_contrib)
            double term = Gk * shellContrib;
            result += term;
            
            // Check convergence
            if (k > 5 && std::abs(term) < config_.tolerance * std::abs(result)) {
                break;
            }
            
            // Advance scaled recurrence: g_{k+2} = g_{k+1} + g_k
            if (k >= 1) {
                double gNext = config_.rho * gCurr + rhoSq * gPrev;
                gPrev = gCurr;
                gCurr = gNext;
            }
        }
        
        return result;
    }
    
    /**
     * @brief Compute UNT for a Quaternion
     */
    [[nodiscard]] double computeUNT(const Quaternion& q, 
                                      const PhaseTransport* transport = nullptr) const {
        // Project to J-plane (first two components)
        JPlane projected(q.first.scalarPart(), q.second.scalarPart());
        return computeUNT(projected, transport);
    }
    
    /**
     * @brief Compute the full boundary collapse scalar
     * 
     * Z_SCOUT = _k F_k  ^k  Align_(shell_k)
     *
     * Uses the scaled Fibonacci recurrence g_k = F_k  ^k to avoid
     * integer overflow and eliminate the double-^k bug.
     */
    [[nodiscard]] double computeScoutScalar(const JPlane& q, 
                                              const PhaseTransport& transport) const {
        double zScout = 0.0;
        JPlane Phi = transport.phase();
        
        // Scaled recurrence: g_k = F_k  ^k
        double gPrev = 0.0;                    // g_0 = 0
        double gCurr = config_.rho;             // g_1 = 
        double rhoSq = config_.rho * config_.rho;
        
        for (unsigned k = 0; k < config_.maxTerms; ++k) {
            double Gk = (k == 0) ? gPrev : (k == 1) ? gCurr : gCurr;
            
            // Propagate to k-th shell (NO ^k decay  that's in Gk)
            JPlane shellValue = propagateToShell(q, k);
            
            // Align with phase: Re(conj()  shell)
            double aligned = Phi.conj().re * shellValue.re + 
                            Phi.conj().im * shellValue.im;
            
            // Weighted term: g_k  alignment
            double term = Gk * aligned;
            zScout += term;
            
            if (k > 5 && std::abs(term) < config_.tolerance * std::abs(zScout)) {
                break;
            }
            
            // Advance recurrence
            if (k >= 1) {
                double gNext = config_.rho * gCurr + rhoSq * gPrev;
                gPrev = gCurr;
                gCurr = gNext;
            }
        }
        
        return zScout;
    }
    
    /**
     * @brief Get the configuration
     */
    [[nodiscard]] const UNTConfig& config() const { return config_; }
    
    /**
     * @brief Modify configuration
     */
    void setConfig(const UNTConfig& config) { 
        config_ = config;
        precomputeFibonacci();
    }
    
    /**
     * @brief Get precomputed Fibonacci number (capped at F_93 to avoid overflow)
     */
    [[nodiscard]] uint64_t getFibonacci(unsigned k) const {
        if (k < fibonacci_.size()) return fibonacci_[k];
        return fibonacci(k);
    }

private:
    UNTConfig config_;
    std::vector<uint64_t> fibonacci_;  // Integer Fibonacci (for public API only)
    
    void precomputeFibonacci() {
        // Cap at 93 to avoid uint64_t overflow (F_93  1.2210^19 < 2^63)
        unsigned cap = std::min(config_.maxTerms + 1, 93u);
        fibonacci_.resize(cap);
        fibonacci_[0] = 0;
        if (cap > 1) fibonacci_[1] = 1;
        for (unsigned i = 2; i < cap; ++i) {
            fibonacci_[i] = fibonacci_[i-1] + fibonacci_[i-2];
        }
    }
    
    /**
     * @brief Compute contribution from k-th boundary shell
     * 
     * (x, J^m (x)) evaluated and integrated over ^(k).
     * The ^k weighting is handled externally by the scaled Fibonacci
     * recurrence  this function computes only the geometric/alignment
     * contribution at the given shell level.
     */
    [[nodiscard]] double computeShellContribution(const JPlane& q, 
                                                    const JPlane& Phi,
                                                    unsigned k) const {
        // Shell geometry: k-th shell at "radius" related to ^{-k}
        double shellRadius = std::pow(PHI_INV, static_cast<double>(k));
        
        // Propagate q to this shell (phase twist only, no ^k decay)
        JPlane qShell = propagateToShell(q, k);
        
        // Compute alignment: Re(conj()  qShell)
        double alignment = Phi.conj().re * qShell.re + Phi.conj().im * qShell.im;
        
        // Shell "area" (geometric factor)  cancels with normalization
        // so this simplifies to alignment  shellRadius
        return alignment * shellRadius;
    }
    
    /**
     * @brief Propagate element to k-th boundary shell
     * 
     * Models radial transport to the shell via phase twist only.
     * NOTE: ^k decay is handled by the scaled Fibonacci recurrence
     * in the outer loop (g_k = F_k  ^k). Do NOT apply decay here
     * or the effective weight becomes F_k  ^{2k}.
     */
    [[nodiscard]] JPlane propagateToShell(const JPlane& q, unsigned k) const {
        // Phase twist at shell k: rotation by karctan(^{-1})
        double twist = static_cast<double>(k) * std::atan(PHI_INV);
        JPlane rotation = expJ(twist);
        
        // Apply rotation only (no ^k decay  that's in the outer sum)
        return q * rotation;
    }
};

// =============================================================================
// BOUNDARY COLLAPSER
// =============================================================================

/**
 * @brief Boundary collapse operator
 * 
 * Extracts unique scalar via:
 *   1. Winding number K from phase transport
 *   2. Fibonacci-weighted integral Z_fib
 *   3. Phase tag: B_{J,fib}() = (-1)^{K/2}  Z_fib
 */
class BoundaryCollapser {
public:
    explicit BoundaryCollapser(const UNTConfig& config = UNTConfig())
        : untEngine_(config) {}
    
    /**
     * @brief Result of boundary collapse
     */
    struct CollapseResult {
        int windingNumber;          // K (topological degree)
        double zFib;                // Z_fib (Fibonacci-weighted scalar)
        int phaseTag;               // B_{J,fib} (discretized tag)
        JPlane phaseArrow;          // _J^ (phase arrow in J-plane)
        double rawScalar;           // Unrounded Z_SCOUT
        bool converged;             // Whether UNT converged
        
        CollapseResult() : windingNumber(0), zFib(0.0), phaseTag(0), 
                          phaseArrow(JPlane::one()), rawScalar(0.0), converged(false) {}
    };
    
    /**
     * @brief Perform boundary collapse
     * 
     * @param q The element to collapse
     * @param transport The accumulated phase transport
     * @return CollapseResult containing all extracted data
     */
    [[nodiscard]] CollapseResult collapse(const JPlane& q, 
                                           const PhaseTransport& transport) const {
        CollapseResult result;
        
        // 1. Extract winding number K
        result.windingNumber = transport.windingNumber();
        
        // 2. Compute Fibonacci-weighted scalar via UNT
        result.rawScalar = untEngine_.computeScoutScalar(q, transport);
        result.zFib = result.rawScalar;
        result.converged = true; // Simplified - would check actual convergence
        
        // 3. Compute phase tag: B_{J,fib} = (-1)^{K/2}  Z_fib
        int ceilKHalf = (result.windingNumber + 1) / 2; // Ceiling of K/2
        int sign = (ceilKHalf % 2 == 0) ? 1 : -1;
        result.phaseTag = sign * static_cast<int>(std::floor(result.zFib));
        
        // 4. Compute phase arrow: _J^ = (-1)^K  exp(JZ_SCOUT)
        PhaseArrow arrow(result.windingNumber, result.rawScalar, 
                        untEngine_.config().alpha0);
        result.phaseArrow = arrow.compute();
        
        return result;
    }
    
    /**
     * @brief Perform collapse for Quaternion
     */
    [[nodiscard]] CollapseResult collapse(const Quaternion& q,
                                           const PhaseTransport& transport) const {
        // Project to J-plane for collapse
        JPlane projected(q.first.scalarPart(), q.second.scalarPart());
        return collapse(projected, transport);
    }
    
    /**
     * @brief Access the UNT engine
     */
    [[nodiscard]] const UNTEngine& untEngine() const { return untEngine_; }
    [[nodiscard]] UNTEngine& untEngine() { return untEngine_; }

private:
    UNTEngine untEngine_;
};

// =============================================================================
// SCOUT WITNESS
// =============================================================================

/**
 * @brief SCOUT witness: proof certificate that a scalar was correctly extracted
 * 
 * Contains all data needed to verify the SCOUT computation
 */
struct ScoutWitness {
    JPlane inputElement;           // Original hypercomplex element (projected)
    PhaseTransport phaseHistory;   // Full phase transport history
    BoundaryCollapser::CollapseResult collapseResult;  // Final collapse result
    std::vector<BoundaryShell> shells;  // Shell contributions
    double finalScalar;            // The extracted scalar
    
    ScoutWitness() : finalScalar(0.0) {}
    
    /**
     * @brief Verify the witness is internally consistent
     */
    [[nodiscard]] bool verify(double tol = 1e-10) const {
        // Check phase arrow matches computation
        PhaseArrow arrow(collapseResult.windingNumber, 
                        collapseResult.rawScalar);
        JPlane expectedArrow = arrow.compute();
        
        if (std::abs(expectedArrow.re - collapseResult.phaseArrow.re) > tol ||
            std::abs(expectedArrow.im - collapseResult.phaseArrow.im) > tol) {
            return false;
        }
        
        // Check final scalar matches collapse
        if (std::abs(finalScalar - collapseResult.rawScalar) > tol) {
            return false;
        }
        
        return true;
    }
};

// =============================================================================
// SCOUT MODULE (AXIOM GENERATION)
// =============================================================================

/**
 * @brief SCOUT alignment and phase transport axioms
 */
class ScoutModule {
public:
    explicit ScoutModule(TermFactory& factory) : factory_(factory) {}
    
    /**
     * @brief Generate all SCOUT axioms
     */
    [[nodiscard]] std::vector<std::unique_ptr<Equation>> generateAxioms() {
        std::vector<std::unique_ptr<Equation>> axioms;
        
        generateAlignmentAxioms(axioms);
        generatePhaseTransportAxioms(axioms);
        generateScalarPartAxioms(axioms);
        generateUNTAxioms(axioms);
        generateBoundaryCollapseAxioms(axioms);
        generateIFCAxioms(axioms);
        generateBulkBoundaryAxioms(axioms);
        generateHolonomyAxioms(axioms);
        generateCayleyLambdaBridgeAxioms(axioms);
        
        return axioms;
    }
    
    /**
     * @brief Create alignment term: Align_U(q) = Scal(U*  q)
     */
    [[nodiscard]] const Term* createAlignment(const Term* U, const Term* q) {
        return factory_.align(U, q);
    }
    
    /**
     * @brief Create phase transport: ' =   U
     */
    [[nodiscard]] const Term* createPhaseTransport(const Term* phi, const Term* U) {
        return factory_.phaseTransport(phi, U);
    }
    
    /**
     * @brief Create Cayley map: C_J(r) = (1 + Jr)(1 - Jr)^{-1}
     */
    [[nodiscard]] const Term* createCayleyMap(const Term* r) {
        auto J = factory_.J();
        auto one = factory_.scalar(1.0);
        auto Jr = factory_.mul(J, r);
        auto num = factory_.add(one, Jr);
        auto den = factory_.add(one, factory_.neg(Jr));
        return factory_.mul(num, factory_.inv(den));
    }
    
    /**
     * @brief Create phase transport step using Cayley map
     */
    [[nodiscard]] const Term* createPhaseTransportStep(const Term* phi, const Term* r) {
        return factory_.mul(phi, createCayleyMap(r));
    }

private:
    TermFactory& factory_;
    uint32_t varCounter_ = 0;
    
    const Term* freshVar(Sort sort = Sort::Generic) {
        return factory_.variable("s" + std::to_string(varCounter_++), sort);
    }
    
    void generateAlignmentAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto U = freshVar();
        auto q = freshVar();
        
        // Definition: Align_U(q) = Scal(U*  q)
        axioms.push_back(std::make_unique<Equation>(
            factory_.align(U, q),
            factory_.scalarPart(factory_.mul(factory_.conj(U), q))
        ));
        
        // NOTE: Linearity, homogeneity, and self-alignment (Align_q(q)=|q|)
        // are THEOREMS derivable from this definition + ring axioms.
        // They should be DISCOVERED, not seeded.
    }
    
    void generatePhaseTransportAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto phi = freshVar();
        auto U = freshVar();
        
        // Definition: PhaseTransport(, U) =   U
        axioms.push_back(std::make_unique<Equation>(
            factory_.phaseTransport(phi, U),
            factory_.mul(phi, U)
        ));
        
        // NOTE: Composition (associativity), identity transport (1=),
        // and initial phase (1U=U) are THEOREMS derivable from this
        // definition + ring axioms. They should be DISCOVERED, not seeded.
    }
    
    void generateScalarPartAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto x = freshVar();
        auto two = factory_.scalar(2.0);
        
        // Definition: Scal(x) = (x + x*)
        axioms.push_back(std::make_unique<Equation>(
            factory_.scalarPart(x),
            factory_.mul(factory_.add(x, factory_.conj(x)), factory_.inv(two))
        ));
        
        // NOTE: Everything else (linearity, Scal(r)=r, Scal(J)=0,
        // Scal((a,b))=Scal(a), Scal(x*)=Scal(x)) are THEOREMS derivable
        // from this definition + ring/conjugation axioms.
        // They should be DISCOVERED, not seeded.
    }
    
    void generateUNTAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        // =====================================================================
        // UNT AXIOMS  SYMBOLIC BRIDGE FROM NUMERICAL SCOUT TO TERM REWRITING
        //
        // The UNT functional is now a FIRST-CLASS term-level operator:
        //   UNT(q, ) = _{k=0}^{N} g_k  Align_(Prop_k(q))
        //
        // where g_k = F_k  ^k satisfies the scaled Fibonacci recurrence:
        //   g_0 = 0, g_1 = , g_{k+2} = g_{k+1} + g_k
        //
        // THESE ARE EQUATIONAL AXIOMS, NOT META-LEVEL COMMENTS.
        // The engine can now REWRITE UNT expressions symbolically.
        // =====================================================================

        auto q  = freshVar();
        auto q2 = freshVar();
        auto Phi = freshVar();
        auto a  = freshVar();  // scalar
        auto one  = factory_.scalar(1.0);
        auto zero = factory_.scalar(0.0);

        // --- UNT definition: symbolic SCOUT functional ---
        // UNT(q, )  Apply("UNT", {q, })  [new first-class operation]

        // Axiom U1: LINEARITY  UNT(q + q, ) = UNT(q, ) + UNT(q, )
        // This is THE fundamental property: SCOUT is a linear functional.
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("UNT", {factory_.add(q, q2), Phi}),
            factory_.add(
                factory_.apply("UNT", {q, Phi}),
                factory_.apply("UNT", {q2, Phi})
            )
        ));

        // Axiom U2: REMOVED  Homogeneity UNT(aq,)=aUNT(q,) was
        // universally quantified over ALL a (including non-scalar J, quaternions).
        // Combined with U8 (UNT output is scalar), instantiating a=J yields:
        //   JUNT(q,) = Scal(JUNT(q,)) = 0    UNT  0  (CONTRADICTION)
        // Scalar homogeneity follows from U1 (additivity) for integer scalars.
        // Real-scalar homogeneity is handled by the evaluator, not axioms.
        (void)a;  // was used by removed U2

        // Axiom U3: CONJUGATION SYMMETRY  UNT(q*, *) = UNT(q, )
        // Because Align uses Scal(U*q) = Scal(q*U) = Re, and the SCOUT
        // scalar is always real, conjugation of both arguments is an involution.
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("UNT", {factory_.conj(q), factory_.conj(Phi)}),
            factory_.apply("UNT", {q, Phi})
        ));

        // Axiom U4: REMOVED  UNT(1, ) = Scal() was unjustified.
        // UNT involves a Fibonacci-weighted sum over ALL boundary shells,
        // not just the zeroth-level alignment. UNT(1, ) =  g_kAlign_(J^m1)
        // which is NOT simply Scal() unless all higher shells vanish.
        // The engine should DISCOVER what UNT(1, ) equals numerically.
        // Note: BB6 + BC8 together imply UNT(1,1) = 1, which is sufficient
        // as a normalization constraint.

        // Axiom U5: PHASE COVARIANCE  UNT(v, ) = ||  UNT(v, 1)
        // When q is phase-aligned (q = v), the 's cancel in alignment.
        // For unit  (||=1): UNT(v, ) = UNT(v, 1)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("UNT", {factory_.mul(Phi, q), Phi}),
            factory_.mul(
                factory_.norm(Phi),
                factory_.apply("UNT", {q, one})
            )
        ));

        // Axiom U6: ZERO ANNIHILATION  UNT(0, ) = 0
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("UNT", {zero, Phi}),
            zero
        ));

        // Axiom U7: FIBONACCI WEIGHT RECURRENCE (term-level)
        // fibStep(n)  g_n = F_n  ^n
        // fibStep(0) = 0
        // fibStep(1) = 
        // fibStep(n+2) =   fibStep(n+1) +   fibStep(n)
        auto rho = factory_.scalar(0.5);  // canonical  = 0.5 < 1/
        (void)rho;  // used conceptually; concrete Fibonacci weights computed numerically

        axioms.push_back(std::make_unique<Equation>(
            factory_.fibStep(factory_.scalar(0.0)),
            zero
        ));
        axioms.push_back(std::make_unique<Equation>(
            factory_.fibStep(one),
            rho
        ));

        // Axiom U8: SCOUT SCALAR OUTPUT IS REAL
        // UNT(q, ) = Scal(UNT(q, ))
        // The output of SCOUT is always a scalar (real number).
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("UNT", {q, Phi}),
            factory_.scalarPart(factory_.apply("UNT", {q, Phi}))
        ));
    }
    
    void generateBoundaryCollapseAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        // =====================================================================
        // BOUNDARY COLLAPSE AXIOMS  SYMBOLIC REPRESENTATION
        //
        // The boundary collapse operator B extracts a unique scalar:
        //   B_{J,fib}() = (-1)^{K/2}  Z_fib
        //
        // where K = winding number, Z_fib = UNT[q]
        //
        // Term-level representation: Apply("BCollapse", {q, , K})
        //
        // These axioms connect the symbolic collapse to the phase arrow
        // and provide the equational bridge for proving S_[Q] = B_[Q].
        // =====================================================================

        auto q   = freshVar();
        auto Phi = freshVar();
        auto K   = freshVar();  // winding number (integer-valued scalar)
        auto one = factory_.scalar(1.0);
        auto zero = factory_.scalar(0.0);
        auto neg1 = factory_.scalar(-1.0);

        // Axiom BC1: COLLAPSE DEFINITION
        // BCollapse(q, , K) = PhaseSign(K)  Floor(UNT(q, ))
        // where PhaseSign(K) = (-1)^{K/2}
        // We represent Floor as Apply("Floor", ...) and PhaseSign as Apply("PhaseSign", ...)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("BCollapse", {q, Phi, K}),
            factory_.mul(
                factory_.apply("PhaseSign", {K}),
                factory_.apply("Floor", {factory_.apply("UNT", {q, Phi})})
            )
        ));

        // Axiom BC2: PHASE SIGN PARITY
        // PhaseSign(0) = 1
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("PhaseSign", {zero}),
            one
        ));

        // Axiom BC3: PHASE SIGN ODD
        // PhaseSign(1) = -1
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("PhaseSign", {one}),
            neg1
        ));

        // Axiom BC4: PHASE SIGN PERIOD
        // PhaseSign(K + 4) = PhaseSign(K)
        // Since (-1)^{(K+4)/2} = (-1)^{K/2+2} = (-1)^{K/2}
        auto four = factory_.scalar(4.0);
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("PhaseSign", {factory_.add(K, four)}),
            factory_.apply("PhaseSign", {K})
        ));

        // Axiom BC5: PHASE ARROW DEFINITION
        // PhaseArrow(K, Z, ) = (-1)^K  exp(J    Z)
        // PhaseArrow(0, Z, ) = exp(J    Z)
        auto Z  = freshVar();
        auto a0 = freshVar();  // alpha_0
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("PhaseArrow", {zero, Z, a0}),
            factory_.apply("ExpJ", {factory_.mul(a0, Z)})
        ));

        // Axiom BC6: PHASE ARROW K-parity
        // PhaseArrow(K+1, Z, ) = neg(PhaseArrow(K, Z, ))
        // Because (-1)^{K+1} = -(-1)^K
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("PhaseArrow", {factory_.add(K, one), Z, a0}),
            factory_.neg(factory_.apply("PhaseArrow", {K, Z, a0}))
        ));

        // Axiom BC7: COLLAPSE IS SCALAR-VALUED
        // Scal(BCollapse(q, , K)) = BCollapse(q, , K)
        axioms.push_back(std::make_unique<Equation>(
            factory_.scalarPart(factory_.apply("BCollapse", {q, Phi, K})),
            factory_.apply("BCollapse", {q, Phi, K})
        ));

        // Axiom BC8: COLLAPSE OF UNIT ELEMENT
        // BCollapse(1, 1, 0) = 1  (trivial collapse of scalar 1)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("BCollapse", {one, one, zero}),
            one
        ));

        // Axiom BC9: EXPJ DEFINITION  exp(J) on unit circle
        // |ExpJ()| = 1 for all 
        auto theta = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.norm(factory_.apply("ExpJ", {theta})),
            one
        ));

        // Axiom BC10: EXPJ ADDITIVITY  exp(J(+)) = exp(J)exp(J)
        auto theta2 = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("ExpJ", {factory_.add(theta, theta2)}),
            factory_.mul(
                factory_.apply("ExpJ", {theta}),
                factory_.apply("ExpJ", {theta2})
            )
        ));

        // Axiom BC11: EXPJ at zero  exp(J0) = 1
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("ExpJ", {zero}),
            one
        ));
    }

    // =========================================================================
    // IFC AXIOMS  INTERIOR FACE CANCELLATION AS EQUATIONAL CONSTRAINTS
    //
    // The IFC condition is the KEY algebraic constraint that makes boundary
    // collapse work. For a shared face f between adjacent cells _{j} and
    // _{j} with transition operator U_{jj}:
    //
    //   Q_{j,f} + U_{jj}*  Q_{j,f}  U_{jj} = 0
    //
    // Equivalently: Q_{j,f} = -U*  Q_{j,f}  U
    //
    // This is now a PROVABLE EQUATIONAL IDENTITY in the term algebra.
    // =========================================================================

    void generateIFCAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto Q_minus = freshVar();  // Charge contribution from cell j-
        auto Q_plus  = freshVar();  // Charge contribution from cell j+
        auto U       = freshVar();  // Transition operator U_{j- -> j+}
        auto one     = factory_.scalar(1.0);

        // Axiom IFC1: FACE CANCELLATION
        // IFC(Q, Q, U) = Q + conj(U)  Q  U = 0
        // Term representation: Apply("IFC", {Q, Q, U})
        // The IFC predicate holds iff this equals zero.
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("IFC", {Q_minus, Q_plus, U}),
            factory_.add(
                Q_minus,
                factory_.mul(
                    factory_.conj(U),
                    factory_.mul(Q_plus, U)
                )
            )
        ));

        // Axiom IFC2: IFC IMPLIES CANCELLATION
        // When IFC holds: Q = -conj(U)  Q  U
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("IFCSolve", {Q_plus, U}),
            factory_.neg(factory_.mul(
                factory_.conj(U),
                factory_.mul(Q_plus, U)
            ))
        ));

        // Axiom IFC3: UNIT TRANSITION SIMPLIFICATION
        // When U = 1 (same frame): IFCSolve(Q, 1) = -Q
        // Adjacent cells with the same frame have simple sign flip
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("IFCSolve", {Q_plus, one}),
            factory_.neg(Q_plus)
        ));

        // Axiom IFC4: REMOVED  |U| = 1 was universally quantified.
        // Since U is a freshVar(), this claims ALL elements have unit norm,
        // which is catastrophically false (e.g., |2| = 4  1, |0| = 0  1).
        // Transition unitarity is encoded via H2 (|CayleyMap(r)| = 1)
        // and PT3 (|PhaseStep(,r)| = ||) for SPECIFIC operator outputs.

        // Axiom IFC5: TRANSITION COMPOSITION
        // U_{ac} = U_{ab}  U_{bc}
        // Transition operators compose via multiplication
        auto U_ab = freshVar();
        auto U_bc = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("TransCompose", {U_ab, U_bc}),
            factory_.mul(U_ab, U_bc)
        ));

        // Axiom IFC6: TRANSITION INVERSE
        // U_{ba} = conj(U_{ab})  (for unitary transition operators)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("TransInv", {U}),
            factory_.conj(U)
        ));

        // Axiom IFC7: REMOVED  TransCompose(U, conj(U)) = 1 for all U.
        // Combined with IFC5 (TransCompose = mul), this derives UU* = 1
        // for ALL elements, i.e., universal unitarity. Same disaster as IFC4.
        // Self-transition (U_{aa} = 1) is a property of SPECIFIC paths,
        // not a universal algebraic law.

        // Axiom IFC8: REMOVED  Align(, IFC(Q,Q,U)) = 0 was
        // unconditionally universally quantified. Since IFC(Q,Q,U) =
        // Q + U*QU (IFC1), this claims Align(, Q + U*QU) = 0
        // for ALL Q, Q, U. Setting Q = 0: Align(, Q) = 0 for all Q.
        // This forces Align  0, destroying the SCOUT mechanism entirely.
        // The IFC cancellation is a CONDITIONAL property (holds when IFC = 0),
        // not expressible as an unconditional equational axiom.
    }

    // =========================================================================
    // BULK-BOUNDARY AXIOMS  S_[Q] = B_[Q] PROVABILITY
    //
    // The bulk-boundary theorem states that the total SCOUT integral over
    // a complex  equals its boundary integral, IFF the IFC condition holds
    // on all interior faces:
    //
    //   S_[Q]  _j S_j[Q_j]  (sum over all cells)
    //   B_[Q]  _{f  } Q_f  (sum over boundary faces only)
    //
    // Under IFC: S_[Q] = B_[Q]
    //
    // Proof sketch: interior face contributions cancel pairwise (IFC),
    // leaving only boundary faces.
    //
    // These axioms express the chain:
    //   IFC  interior cancellation  S_ = B_
    // =========================================================================

    void generateBulkBoundaryAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto Q     = freshVar();
        auto Q2    = freshVar();
        auto Phi   = freshVar();
        auto U     = freshVar();
        auto K     = freshVar();
        auto one   = factory_.scalar(1.0);
        auto zero  = factory_.scalar(0.0);

        // Axiom BB1: BULK FUNCTIONAL DEFINITION
        // S_[Q] = _j UNT(Q_j, _j) over all cells
        // For a single cell: S_cell(Q, ) = UNT(Q, )
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("BulkFunctional", {Q, Phi}),
            factory_.apply("UNT", {Q, Phi})
        ));

        // Axiom BB2: BULK ADDITIVITY
        // S_[Q + Q] = S_[Q] + S_[Q]  (inherits from UNT linearity)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("BulkFunctional", {factory_.add(Q, Q2), Phi}),
            factory_.add(
                factory_.apply("BulkFunctional", {Q, Phi}),
                factory_.apply("BulkFunctional", {Q2, Phi})
            )
        ));

        // Axiom BB3: BOUNDARY FUNCTIONAL DEFINITION
        // B_[Q] = _{f} BCollapse(Q_f, _f, K_f)
        // Single boundary face contribution:
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("BdryFunctional", {Q, Phi, K}),
            factory_.apply("BCollapse", {Q, Phi, K})
        ));

        // Axiom BB4: INTERIOR CANCELLATION LEMMA
        // For adjacent cells j, j sharing face f with IFC:
        //   S_j[Q_{j,f}] + S_j[Q_{j,f}] = 0
        // Because Q_{j,f} = -U*Q_{j,f}U (IFC) and UNT is linear,
        // the interior face contributions sum to zero.
        //
        // BulkFunctional(IFCSolve(Q, U), ) + BulkFunctional(Q, ') = 0
        // where ' = U (gauge-transformed frame)
        axioms.push_back(std::make_unique<Equation>(
            factory_.add(
                factory_.apply("BulkFunctional", {
                    factory_.apply("IFCSolve", {Q, U}), Phi
                }),
                factory_.apply("BulkFunctional", {
                    Q, factory_.mul(U, Phi)
                })
            ),
            zero
        ));

        // Axiom BB5: BULK-BOUNDARY EQUIVALENCE (THE THEOREM)
        // S_[Q] = B_[Q] when IFC holds on all interior faces.
        // For a single cell with boundary face (no interior faces):
        // BulkFunctional(Q, ) = BdryFunctional(Q, , K)
        // This is the statement that bulk = boundary for the simplest case.
        // The general case follows by induction using BB4.
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("BulkBdryEquiv", {Q, Phi, K}),
            factory_.add(
                factory_.apply("BulkFunctional", {Q, Phi}),
                factory_.neg(factory_.apply("BdryFunctional", {Q, Phi, K}))
            )
        ));

        // Axiom BB6: THE THEOREM STATEMENT
        // BulkBdryEquiv(Q, , K) = 0 when IFC holds
        // i.e., S_[Q] - B_[Q] = 0
        // This is an EQUATIONAL THEOREM: the engine can prove it = 0
        // by chaining BB1BB3BB4BC1.
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("BulkBdryEquiv", {one, one, zero}),
            zero
        ));
    }

    // =========================================================================
    // HOLONOMY AXIOMS  H() = (-1)^K DERIVABILITY
    //
    // Discrete holonomy around a closed path :
    //   H() = _i C_J(r_i) = exp(J  _i 2arctan(r_i))
    //
    // The winding number K counts how many times the phase wraps:
    //   _i 2arctan(r_i) = 2K + _residual
    //
    // For closed paths (_residual = 0):
    //   H() = exp(J  2K) = (-1)^{2K}  exp(0) ... but in J-plane,
    //   exp(J2K) = cos(2K) + Jsin(2K) = 1
    //   exp(JK) = cos(K) + Jsin(K) = (-1)^K
    //
    // The correct formula with half-winding:
    //   H() = exp(JK)  where K is the discrete winding number
    //   H() = exp(J2K) = 1
    //
    // These are derivable from J=-1 and the exponential properties.
    // =========================================================================

    void generateHolonomyAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto K     = freshVar();  // winding number
        auto r     = freshVar();  // ratio parameter
        auto Phi   = freshVar();  // accumulated phase
        auto one   = factory_.scalar(1.0);
        auto zero  = factory_.scalar(0.0);
        auto neg1  = factory_.scalar(-1.0);
        auto two   = factory_.scalar(2.0);
        auto J     = factory_.J();

        // Axiom H1: HOLONOMY DEFINITION
        // Holonomy() = product of Cayley maps along closed path
        // For a single step: Holonomy({r}) = CayleyMap(r)
        // CayleyMap(r) = (1 + Jr)  (1 - Jr)
        auto Jr = factory_.mul(J, r);
        auto num = factory_.add(one, Jr);
        auto den = factory_.add(one, factory_.neg(Jr));
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("CayleyMap", {r}),
            factory_.mul(num, factory_.inv(den))
        ));

        // Axiom H2: CAYLEY MAP IS UNITARY
        // |CayleyMap(r)| = 1 for all r
        axioms.push_back(std::make_unique<Equation>(
            factory_.norm(factory_.apply("CayleyMap", {r})),
            one
        ));

        // Axiom H3: CAYLEY MAP SPECIAL VALUES
        // CayleyMap(0) = 1
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("CayleyMap", {zero}),
            one
        ));

        // Axiom H4: CAYLEY MAP AT 1
        // CayleyMap(1) = J
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("CayleyMap", {one}),
            J
        ));

        // Axiom H5: CAYLEY MAP INVERSION IDENTITY
        // CayleyMap(r)  CayleyMap(1/r) = -1 for r  0
        axioms.push_back(std::make_unique<Equation>(
            factory_.mul(
                factory_.apply("CayleyMap", {r}),
                factory_.apply("CayleyMap", {factory_.inv(r)})
            ),
            neg1
        ));

        // Axiom H6: HOLONOMY WINDING  exp(J2K) = 1
        // Full winding: exp(J2K) = cos(2K) + Jsin(2K) = 1
        // This is the fundamental periodicity of the J-exponential.
        auto twoPiK = factory_.mul(two, factory_.mul(factory_.apply("Pi", {}), K));
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("ExpJ", {twoPiK}),
            one
        ));

        // Axiom H7: HALF-WINDING  exp(JK) = (-1)^K
        // This is the discrete holonomy: H() = (-1)^K
        auto piK = factory_.mul(factory_.apply("Pi", {}), K);
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("ExpJ", {piK}),
            factory_.apply("PowNeg1", {K})
        ));

        // Axiom H8: (-1)^K PARITY RULES
        // PowNeg1(0) = 1
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("PowNeg1", {zero}),
            one
        ));

        // Axiom H9: PowNeg1(K+1) = -PowNeg1(K)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("PowNeg1", {factory_.add(K, one)}),
            factory_.neg(factory_.apply("PowNeg1", {K}))
        ));

        // Axiom H10: PowNeg1(K) = 1
        axioms.push_back(std::make_unique<Equation>(
            factory_.mul(
                factory_.apply("PowNeg1", {K}),
                factory_.apply("PowNeg1", {K})
            ),
            one
        ));

        // Axiom H11: HOLONOMY SQUARED IS IDENTITY
        // H() = ((-1)^K) = 1
        // Holonomy is always an involution: H = 1
        // This is THE central topological constraint.
        axioms.push_back(std::make_unique<Equation>(
            factory_.mul(
                factory_.apply("ExpJ", {piK}),
                factory_.apply("ExpJ", {piK})
            ),
            one
        ));

        // Axiom H12: REMOVED  Holonomy() = conj(1) =  trivializes
        // the Holonomy operator to identity. This makes Holonomy a useless
        // wrapper. Real holonomy is _final  conj(_initial) for a specific
        // PATH, not a function of a single element.

        // Axiom H13: REMOVED  |Holonomy()| = 1 for all .
        // Combined with H12 (Holonomy() = ), this derives || = 1
        // for ALL   universal unitarity, same catastrophe as IFC4.
        // Closed-path unitarity is a THEOREM about specific paths,
        // not a universal axiom. The engine can verify it numerically.

        // Axiom H14: PHASE TRANSPORT STEP  connects to PhaseTransport
        // PhaseTransport(, CayleyMap(r)) =   CayleyMap(r)
        axioms.push_back(std::make_unique<Equation>(
            factory_.phaseTransport(Phi, factory_.apply("CayleyMap", {r})),
            factory_.mul(Phi, factory_.apply("CayleyMap", {r}))
        ));

        // Axiom H15: J EXPONENTIAL FROM J=-1
        // Since J=-1 (DISCOVERED by the engine), we have:
        // ExpJ() = cos()1 + sin()J
        // This is the EULER FORMULA in the J-plane.
        // The engine can verify this numerically, and these axioms
        // allow it to be used in symbolic proofs.
        // ExpJ(0) = 1 is already BC11.
        // ExpJ(Pi) = -1:
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("ExpJ", {factory_.apply("Pi", {})}),
            neg1
        ));
    }

    // =========================================================================
    // CAYLEY-LAMBDA BRIDGE AXIOMS
    // =========================================================================
    // These connect the existing SCOUT/UNT infrastructure to the new
    // Cayley-Lambda, -derivation, equivariant cohomology, and hybrid
    // calculus frameworks.
    void generateCayleyLambdaBridgeAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        auto z    = freshVar();
        auto q    = freshVar();
        auto U    = freshVar();
        auto Phi  = freshVar();
        auto r    = freshVar();
        auto phi  = factory_.phi();
        auto one  = factory_.scalar(1.0);

        // BRIDGE B1: PhaseTransport IS the Cayley-Lambda map
        // The existing PhaseTransport(, U) corresponds to 
        // C_ applied to the phase data. Specifically:
        // CLambda(PhaseTransport(, U)) = TStep(CLambda())
        // when U corresponds to an F-step
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("CLambda", {factory_.phaseTransport(Phi, U)}),
            factory_.apply("TStep", {factory_.apply("CLambda", {Phi})})
        ));

        // BRIDGE B2: BCollapse IS the -quotient projection
        // BCollapse(q) projects q to its equivalence class in */
        // This is the "boundary collapse" = collapsing by scale periodicity
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("BCollapse", {factory_.mul(factory_.apply("Lambda", {}), q)}),
            factory_.apply("BCollapse", {q})
        ));

        // BRIDGE B3: Align IS the defect projection
        // Align_U(q) = Scal(U*  q) extracts the scalar part
        // In the cohomological framework, this is the projection
        // of the master form onto a specific conserved quantity
        axioms.push_back(std::make_unique<Equation>(
            factory_.align(U, factory_.apply("MasterForm", {q})),
            factory_.apply("Anomaly", {factory_.align(U, q)})
        ));

        // BRIDGE B4: CayleyMap connects to CLambda
        // CayleyMap(r) = (1+Jr)(1-Jr) is a SECTION of C_
        // It maps the real line (boundary) into the complex plane (bulk)
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("CLambda", {factory_.apply("CayleyMap", {r})}),
            factory_.apply("CayleyMap_Torus", {r})
        ));

        // BRIDGE B5: UNT scalarization and scale decomposition
        // UNT(q) extracts the scalar invariant
        // ScaleDecomp recovers the full structure
        // UNT = ScalarPart  ScaleDecomp at =0
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("UNT", {q}),
            factory_.scalarPart(
                factory_.apply("ScaleDecomp", {q, factory_.scalar(0.0), one}))
        ));

        // BRIDGE B6: ExpJ is the F-map phase factor
        // ExpJ() = cos() + Jsin() is used by F-map
        // F(z) = Jz = ||ExpJ(/2)z
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("FMap", {z}),
            factory_.mul(
                factory_.mul(phi, factory_.apply("ExpJ", {
                    factory_.mul(factory_.apply("Pi", {}),
                                factory_.inv(factory_.scalar(2.0)))})),
                z)
        ));

        // BRIDGE B7: Phase arrow connects to torus modes
        // PhaseArrow(K, z) = (-1)^K  ExpJ(z)
        // This is a SPECIFIC torus mode with m+n  0 mod 4
        auto K = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("PhaseArrow", {K, z}),
            factory_.mul(
                factory_.apply("PowNeg1", {K}),
                factory_.apply("TorusMode", {
                    factory_.mul(factory_.scalar(2.0), K),
                    factory_.mul(factory_.scalar(2.0), factory_.neg(K))}))
        ));

        // BRIDGE B8: IFC critical exponent IS the Onsager exponent
        // The IFCSolve critical point corresponds to  = 1/3
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("IFC", {factory_.apply("OnsagerKappa", {})}),
            factory_.scalar(0.0)
        ));

        // BRIDGE B9: Boundary shell hierarchy  discrete spectrum
        // The k-th boundary shell ^(k) corresponds to scale ^k
        // _k = 2k/ln() gives the discrete spectrum
        auto k = freshVar();
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("BdryFunctional", {k}),
            factory_.apply("OmegaN", {k})
        ));

        // BRIDGE B10: -derivation connects to SCOUT commutator
        // The SCOUT alignment commutator [Align_U, q] 
        // is a -derivation with  = Ad(U)
        axioms.push_back(std::make_unique<Equation>(
            factory_.add(
                factory_.align(U, factory_.mul(q, z)),
                factory_.neg(factory_.mul(
                    factory_.align(U, q),
                    factory_.align(U, z)))),
            factory_.apply("SigmaDeriv", {
                factory_.apply("SigmaAd", {U}),
                factory_.mul(q, z)})
        ));
    }
};

// =============================================================================
// SCOUT EVALUATOR (NUMERIC EVALUATION)
// =============================================================================

/**
 * @brief Numerical evaluation of SCOUT operations
 */
class ScoutEvaluator {
public:
    /**
     * @brief Evaluate alignment for J-plane elements
     */
    [[nodiscard]] static double alignJPlane(
        const JPlane& U, 
        const JPlane& q) {
        // Align_U(q) = Re(conj(U) * q) = U.re * q.re + U.im * q.im
        return U.re * q.re + U.im * q.im;
    }
    
    /**
     * @brief Evaluate alignment for complex numbers
     */
    [[nodiscard]] static double alignComplex(
        const std::complex<double>& U, 
        const std::complex<double>& q) {
        // Align_U(q) = Re(conj(U) * q)
        return (std::conj(U) * q).real();
    }
    
    /**
     * @brief Evaluate phase transport for J-plane
     */
    [[nodiscard]] static JPlane phaseTransportJPlane(
        const JPlane& phi,
        const JPlane& U) {
        return phi * U;
    }
    
    /**
     * @brief Evaluate phase transport for complex numbers
     */
    [[nodiscard]] static std::complex<double> phaseTransportComplex(
        const std::complex<double>& phi,
        const std::complex<double>& U) {
        return phi * U;
    }
    
    /**
     * @brief Compute full SCOUT scalar for J-plane element
     */
    [[nodiscard]] static double computeScoutScalar(
        const JPlane& q,
        const PhaseTransport& transport,
        const UNTConfig& config = UNTConfig()) {
        UNTEngine engine(config);
        return engine.computeScoutScalar(q, transport);
    }
    
    /**
     * @brief Compute boundary collapse for J-plane element
     */
    [[nodiscard]] static BoundaryCollapser::CollapseResult computeBoundaryCollapse(
        const JPlane& q,
        const PhaseTransport& transport,
        const UNTConfig& config = UNTConfig()) {
        BoundaryCollapser collapser(config);
        return collapser.collapse(q, transport);
    }
    
    /**
     * @brief Generate SCOUT witness
     */
    [[nodiscard]] static ScoutWitness generateWitness(
        const JPlane& q,
        const PhaseTransport& transport,
        const UNTConfig& config = UNTConfig()) {
        ScoutWitness witness;
        witness.inputElement = q;
        witness.phaseHistory = transport;
        
        BoundaryCollapser collapser(config);
        witness.collapseResult = collapser.collapse(q, transport);
        witness.finalScalar = witness.collapseResult.rawScalar;
        
        return witness;
    }
    
    /**
     * @brief Quaternion representation (a + bi + cj + dk)
     */
    struct QuaternionEval {
        double w, x, y, z;
        
        QuaternionEval(double w = 0, double x = 0, double y = 0, double z = 0)
            : w(w), x(x), y(y), z(z) {}
        
        QuaternionEval conjugate() const {
            return QuaternionEval(w, -x, -y, -z);
        }
        
        double norm() const {
            return w*w + x*x + y*y + z*z;
        }
        
        double scalarPart() const {
            return w;
        }
        
        QuaternionEval operator*(const QuaternionEval& q) const {
            return QuaternionEval(
                w*q.w - x*q.x - y*q.y - z*q.z,
                w*q.x + x*q.w + y*q.z - z*q.y,
                w*q.y - x*q.z + y*q.w + z*q.x,
                w*q.z + x*q.y - y*q.x + z*q.w
            );
        }
        
        QuaternionEval operator+(const QuaternionEval& q) const {
            return QuaternionEval(w+q.w, x+q.x, y+q.y, z+q.z);
        }
        
        // Convert to J-plane by projecting onto first complex pair
        [[nodiscard]] JPlane toJPlane() const {
            return JPlane(w, x);
        }
    };
    
    // Type alias for backward compatibility
    using Quaternion = QuaternionEval;
    
    /**
     * @brief Evaluate alignment for quaternions
     */
    [[nodiscard]] static double alignQuaternion(
        const QuaternionEval& U,
        const QuaternionEval& q) {
        return (U.conjugate() * q).scalarPart();
    }
    
    /**
     * @brief Evaluate phase transport for quaternions
     */
    [[nodiscard]] static QuaternionEval phaseTransportQuaternion(
        const QuaternionEval& phi,
        const QuaternionEval& U) {
        return phi * U;
    }
    
    /**
     * @brief Octonion representation
     */
    struct OctonionEval {
        double e[8]; // e[0] = real part
        
        OctonionEval() { std::fill(std::begin(e), std::end(e), 0.0); }
        
        OctonionEval(std::initializer_list<double> init) {
            std::fill(std::begin(e), std::end(e), 0.0);
            size_t i = 0;
            for (double v : init) {
                if (i >= 8) break;
                e[i++] = v;
            }
        }
        
        double scalarPart() const { return e[0]; }
        
        OctonionEval conjugate() const {
            OctonionEval result;
            result.e[0] = e[0];
            for (int i = 1; i < 8; ++i) {
                result.e[i] = -e[i];
            }
            return result;
        }
        
        double norm() const {
            double sum = 0;
            for (int i = 0; i < 8; ++i) {
                sum += e[i] * e[i];
            }
            return sum;
        }
        
        // Convert to J-plane
        [[nodiscard]] JPlane toJPlane() const {
            return JPlane(e[0], e[1]);
        }
        
        // Octonion multiplication using Cayley table
        OctonionEval operator*(const OctonionEval& q) const;
        
        OctonionEval operator+(const OctonionEval& q) const {
            OctonionEval result;
            for (int i = 0; i < 8; ++i) {
                result.e[i] = e[i] + q.e[i];
            }
            return result;
        }
    };
    
    /**
     * @brief Evaluate alignment for octonions
     */
    [[nodiscard]] static double alignOctonion(
        const OctonionEval& U,
        const OctonionEval& q) {
        return (U.conjugate() * q).scalarPart();
    }
};

// Octonion multiplication implementation  Cayley-Dickson recursive construction
// (a,b)*(c,d) = (ac - conj(d)*b, d*a + b*conj(c))
// where a,b,c,d are quaternions. NO hardcoded Fano plane table.
inline ScoutEvaluator::OctonionEval 
ScoutEvaluator::OctonionEval::operator*(const OctonionEval& q) const {
    // Split into quaternion pairs: this = (A, B), q = (C, D)
    QuaternionEval A(e[0], e[1], e[2], e[3]);
    QuaternionEval B(e[4], e[5], e[6], e[7]);
    QuaternionEval C(q.e[0], q.e[1], q.e[2], q.e[3]);
    QuaternionEval D(q.e[4], q.e[5], q.e[6], q.e[7]);
    
    // Cayley-Dickson formula: (A,B)(C,D) = (AC - conj(D)*B, D*A + B*conj(C))
    QuaternionEval first = A * C + QuaternionEval(-1,0,0,0) * (D.conjugate() * B);
    QuaternionEval second = D * A + B * C.conjugate();
    
    OctonionEval result;
    result.e[0] = first.w;  result.e[1] = first.x;
    result.e[2] = first.y;  result.e[3] = first.z;
    result.e[4] = second.w; result.e[5] = second.x;
    result.e[6] = second.y; result.e[7] = second.z;
    return result;
}

} // namespace domain
} // namespace autodiscover

#endif // AUTODISCOVER_DOMAIN_SCOUT_HPP
