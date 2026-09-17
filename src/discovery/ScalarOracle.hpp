#ifndef AUTODISCOVER_DISCOVERY_SCALAR_ORACLE_HPP
#define AUTODISCOVER_DISCOVERY_SCALAR_ORACLE_HPP

/**
 * @file ScalarOracle.hpp
 * @brief Exact Z[] oracle + SCOUT validator + scalar-upward proof lifter
 *
 * =============================================================================
 * ARCHITECTURE: SCALAR-UPWARD DISCOVERY
 * =============================================================================
 *
 * Traditional provers work TOP-DOWN: symbolic manipulation  hope to find results.
 * This oracle works BOTTOM-UP: compute exact scalar  match  generate proof.
 *
 * Key insight: Z[] = {a + b : a,b  Z} is an integral domain with EXACT
 * equality. If two ground terms evaluate to the same (a,b) pair, they are
 * provably equal  no tolerance, no epsilon, no false positives, no search.
 *
 * Pipeline:
 *   1. ZPhiEvaluator:  Term*  optional<ZPhi>  (exact ground-term evaluation)
 *   2. ScoutValidator:  validates equality via UNT/BoundaryCollapse/phase-tags
 *   3. ProofLifter:     generates kernel-verifiable CertificateStep sequences
 *                       from ZPhi equalities, working UPWARD from the scalar value
 *
 * The proof strategy for "scalar-upward":
 *   Given termA and termB with ZPhi(termA) == ZPhi(termB):
 *   1. Reduce termA to canonical form (a + b) using ring axioms
 *   2. Reduce termB to canonical form (a + b) using ring axioms
 *   3. By transitivity: termA = (a+b) = termB  
 *
 * =============================================================================
 */

#include "../core/Term.hpp"
#include "../core/Context.hpp"
#include "../ring/ZPhi.hpp"
#include "../domain/Scout.hpp"
#include "../proof/CertificateKernel.hpp"
#include "../canon/NFEngine.hpp"

#include <optional>
#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <cmath>

namespace autodiscover::discovery {

using namespace autodiscover::core;
using namespace autodiscover::ring;

// =============================================================================
// Z[] EVALUATOR  exact ground-term evaluation
// =============================================================================

/**
 * @brief Evaluates ground terms (no free variables) to exact Z[] values.
 *
 * Handles the full ring: +, *, neg over {, , integer scalars}.
 * Uses the identities:
 *     = ZPhi(0, 1)        i.e.  0 + 1
 *     = ZPhi(1, -1)       i.e.  1 -   (since  +  = 1)
 *   n  = ZPhi(n, 0)        i.e.  n + 0
 *    =  + 1             (built into ZPhi multiplication)
 *
 * Returns nullopt for:
 *   - Terms containing free variables
 *   - Division/inversion (inv) that would leave Z[]
 *   - The J unit (quaternionic, not in Z[])
 *   - Unknown operators
 *
 * For inv(x) where x has norm 1 in Z[], we CAN compute the inverse
 * exactly: if N(x) = xx = 1, then x = x. This covers
 * inv() = /(-1) = - = -1, which is correct.
 */
class ZPhiEvaluator {
public:
    struct EvalResult {
        ZPhi value;
        bool exact = true;  ///< true if the result is exact in Z[]
    };

    /**
     * @brief Evaluate a term to its exact Z[] value.
     *
     * @param t  The term to evaluate (must be ground  no free variables).
     * @return The exact Z[] value, or nullopt if the term can't be evaluated.
     */
    [[nodiscard]] std::optional<ZPhi> evaluate(const Term* t) {
        if (!t) return std::nullopt;

        // Check cache
        auto it = cache_.find(t);
        if (it != cache_.end()) return it->second;

        auto result = evaluateImpl(t);
        if (result) {
            cache_[t] = *result;
        }
        return result;
    }

    /**
     * @brief Check if a term is a ground term (no free variables).
     */
    [[nodiscard]] static bool isGround(const Term* t) {
        if (!t) return true;
        switch (t->kind()) {
            case TermKind::Variable:
                return false;
            case TermKind::Scalar:
            case TermKind::Phi:
            case TermKind::PhiBar:
            case TermKind::J:
            case TermKind::Constant:
                return true;
            case TermKind::Application:
                for (const Term* child : t->children()) {
                    if (!isGround(child)) return false;
                }
                return true;
            case TermKind::Pair:
                return isGround(t->first()) && isGround(t->second());
        }
        return false;
    }

    /**
     * @brief Clear the evaluation cache (e.g., between discovery rounds).
     */
    void clearCache() { cache_.clear(); }

    /**
     * @brief Number of cached evaluations.
     */
    [[nodiscard]] size_t cacheSize() const { return cache_.size(); }

private:
    std::unordered_map<const Term*, ZPhi> cache_;

    [[nodiscard]] std::optional<ZPhi> evaluateImpl(const Term* t) {
        switch (t->kind()) {
            case TermKind::Phi:
                return ZPhi(0, 1);        //  = 0 + 1

            case TermKind::PhiBar:
                return ZPhi(1, -1);       //  = 1 - 

            case TermKind::Scalar: {
                double v = t->scalarValue();
                // Only handle exact integers
                double rounded = std::round(v);
                if (std::abs(v - rounded) < 1e-15 &&
                    rounded >= -1e15 && rounded <= 1e15) {
                    return ZPhi(static_cast<int64_t>(rounded), 0);
                }
                return std::nullopt;  // Non-integer scalar
            }

            case TermKind::Constant: {
                // Named constants: try to parse as integer
                try {
                    int64_t val = std::stoll(t->symbol());
                    return ZPhi(val, 0);
                } catch (...) {
                    return std::nullopt;  // Unknown constant
                }
            }

            case TermKind::J:
                return std::nullopt;  // J is quaternionic, not in Z[]

            case TermKind::Variable:
                return std::nullopt;  // Can't evaluate free variables

            case TermKind::Application:
                return evaluateApp(t);

            case TermKind::Pair:
                return std::nullopt;  // Pairs don't have scalar values
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<ZPhi> evaluateApp(const Term* t) {
        const auto& sym = t->symbol();
        const auto& ch = t->children();

        // -- Unary operators --
        if (ch.size() == 1) {
            auto inner = evaluate(ch[0]);
            if (!inner) return std::nullopt;

            if (sym == "neg") {
                return -(*inner);
            }
            if (sym == "inv") {
                return tryInvert(*inner);
            }
            if (sym == "conj") {
                // Galois conjugation: (a + b)  (a + b) - b
                return inner->conjugate();
            }
            return std::nullopt;
        }

        // -- Binary operators --
        if (ch.size() == 2) {
            auto lhs = evaluate(ch[0]);
            auto rhs = evaluate(ch[1]);
            if (!lhs || !rhs) return std::nullopt;

            if (sym == "+" || sym == "add") {
                return *lhs + *rhs;
            }
            if (sym == "*" || sym == "mul") {
                return *lhs * *rhs;
            }
            if (sym == "-" || sym == "sub") {
                return *lhs - *rhs;
            }
            return std::nullopt;
        }

        return std::nullopt;
    }

    /**
     * @brief Try to invert a Z[] element.
     *
     * An element x = a + b is invertible in Z[] iff its norm N(x) = a+ab-b = 1.
     * If N(x) = 1:  x = conjugate(x)
     * If N(x) = -1: x = -conjugate(x)
     *
     * This covers:  = -1,  = -1, 1 = 1, (-1) = -1,
     * and all units of Z[] (which are exactly  for n  Z).
     */
    [[nodiscard]] std::optional<ZPhi> tryInvert(const ZPhi& x) {
        if (x.isZero()) return std::nullopt;  // Division by zero

        BigInt n = x.norm();
        if (n == BigInt(1)) {
            return x.conjugate();
        }
        if (n == BigInt(-1)) {
            return -(x.conjugate());
        }
        // Norm is not 1  not invertible in Z[]
        return std::nullopt;
    }
};


// =============================================================================
// SCOUT VALIDATOR  independent SCOUT-level verification of equalities
// =============================================================================

/**
 * @brief Validates discovered equalities via SCOUT boundary collapse.
 *
 * After the Z[] oracle discovers that termA and termB have the same
 * exact scalar value, the ScoutValidator provides INDEPENDENT confirmation
 * by computing:
 *   1. UNT values for both terms (Fibonacci-weighted boundary sums)
 *   2. BoundaryCollapse phase tags
 *   3. Phase arrows
 *
 * If the phase tags match, this gives a second independent verification
 * that the equality holds at the SCOUT level (boundary-collapse level).
 */
class ScoutValidator {
public:
    struct ValidationResult {
        bool scalarMatch = false;    ///< Z[] exact equality
        bool scoutMatch  = false;    ///< UNT values match (within tolerance)
        bool phaseTagMatch = false;  ///< Phase tags identical
        double lhsUNT = 0.0;
        double rhsUNT = 0.0;
        int    lhsPhaseTag = 0;
        int    rhsPhaseTag = 0;
        std::string detail;

        [[nodiscard]] bool allPassed() const {
            return scalarMatch && scoutMatch && phaseTagMatch;
        }
    };

    explicit ScoutValidator(const domain::UNTConfig& config = {})
        : untEngine_(config)
        , collapser_(config)
    {}

    /**
     * @brief Validate that termA = termB via SCOUT boundary collapse.
     *
     * @param lhsVal  Exact Z[] value of termA
     * @param rhsVal  Exact Z[] value of termB
     * @return Detailed validation result
     */
    [[nodiscard]] ValidationResult validate(
        const ZPhi& lhsVal,
        const ZPhi& rhsVal) const
    {
        ValidationResult result;

        // Level 1: Exact Z[] equality
        result.scalarMatch = (lhsVal == rhsVal);

        // Level 2: Evaluate through SCOUT UNT
        // Map ZPhi  JPlane: a+b  (a + b_real, 0)  (real axis of J-plane)
        double lhsReal = lhsVal.toDoubleFast();
        double rhsReal = rhsVal.toDoubleFast();

        domain::JPlane lhsJ{lhsReal, 0.0};
        domain::JPlane rhsJ{rhsReal, 0.0};

        // Default transport (identity)
        domain::PhaseTransport transport;

        result.lhsUNT = untEngine_.computeUNT(lhsJ, &transport);
        result.rhsUNT = untEngine_.computeUNT(rhsJ, &transport);
        result.scoutMatch = std::abs(result.lhsUNT - result.rhsUNT) < 1e-10;

        // Level 3: Phase tag comparison
        auto lhsCollapse = collapser_.collapse(lhsJ, transport);
        auto rhsCollapse = collapser_.collapse(rhsJ, transport);
        result.lhsPhaseTag = lhsCollapse.phaseTag;
        result.rhsPhaseTag = rhsCollapse.phaseTag;
        result.phaseTagMatch = (lhsCollapse.phaseTag == rhsCollapse.phaseTag);

        // Detail string
        std::ostringstream oss;
        oss << "Z[]=" << (result.scalarMatch ? "MATCH" : "MISMATCH")
            << " UNT=" << (result.scoutMatch ? "MATCH" : "MISMATCH")
            << " PhaseTag=" << (result.phaseTagMatch ? "MATCH" : "MISMATCH")
            << " [" << result.lhsPhaseTag << " vs " << result.rhsPhaseTag << "]";
        result.detail = oss.str();

        return result;
    }

private:
    domain::UNTEngine untEngine_;
    domain::BoundaryCollapser collapser_;
};


// =============================================================================
// PROOF LIFTER  scalar-upward proof generation
// =============================================================================

/**
 * @brief Generates machine-checkable proof certificates from scalar equalities.
 *
 * This is the "scalar-upward" proof strategy:
 *   Given: ZPhi(termA) == ZPhi(termB) == (a, b)  (exact Z[] equality)
 *   Prove: termA = termB via the trusted Kernel
 *
 * Strategy:
 *   1. Register ring axioms in the Kernel:
 *      -  =  + 1                    (defining relation)
 *      -  = 1 -                      (Galois conjugate)
 *      - neg(neg(x)) = x               (double negation)
 *      - x + 0 = x, x * 1 = x, etc.   (ring axioms)
 *
 *   2. For each term, produce a chain of rewrites reducing it to
 *      its canonical Z[] form: scalar(a) + scalar(b) * 
 *
 *   3. Since both terms reduce to the same canonical form,
 *      transitivity gives: termA = canonical = termB
 *
 * The Kernel verifies every step  the ProofLifter is untrusted automation.
 */
class ProofLifter {
public:
    /**
     * @brief A proven equation with its Kernel certificate.
     */
    struct LiftedProof {
        const Term* lhs = nullptr;
        const Term* rhs = nullptr;
        ZPhi scalarValue;
        bool proven = false;
        std::string kernelJudgment;   ///< The Kernel's Theorem::toString()
        std::string proofTrace;       ///< Human-readable proof trace
        size_t proofSteps = 0;
    };

    explicit ProofLifter(TermFactory& factory)
        : factory_(factory) {}

    /**
     * @brief Lift a scalar equality to a kernel-verified proof.
     *
     * Given that evaluate(termA) == evaluate(termB) == val,
     * produce a Kernel-verified Theorem: termA = termB.
     *
     * @param termA  First term
     * @param termB  Second term
     * @param val    Their shared Z[] value
     * @return LiftedProof with kernel verification status
     */
    [[nodiscard]] LiftedProof lift(
        const Term* termA,
        const Term* termB,
        const ZPhi& val)
    {
        LiftedProof result;
        result.lhs = termA;
        result.rhs = termB;
        result.scalarValue = val;

        proof::Kernel kernel;
        std::ostringstream trace;

        // Step 1: Register the ring axioms
        trace << "=== Proof: " << termA->toString()
              << " = " << termB->toString() << " ===\n";
        trace << "Shared Z[] value: " << val.toString() << "\n\n";

        registerRingAxioms(kernel, trace);

        // Step 2: Build the canonical form as a Term
        const Term* canonical = buildCanonicalTerm(val);
        trace << "Canonical form: " << canonical->toString() << "\n\n";

        // Step 3: Prove termA = canonical
        trace << "--- Reducing LHS ---\n";
        auto r1 = proveReduction(termA, canonical, val, kernel, trace);

        // Step 4: Prove termB = canonical
        trace << "--- Reducing RHS ---\n";
        auto r2 = proveReduction(termB, canonical, val, kernel, trace);

        // Step 5: Transitivity: termA = canonical = termB
        if (r1.success && r2.success) {
            // termA = canonical  (r1)
            // termB = canonical  (r2)
            // By symmetry on r2: canonical = termB
            auto r2sym = kernel.symmetry(*r2.theorem);
            if (r2sym.success) {
                // By transitivity: termA = canonical, canonical = termB  termA = termB
                auto final_ = kernel.transitivity(*r1.theorem, *r2sym.theorem);
                if (final_.success) {
                    result.proven = true;
                    result.kernelJudgment = final_.theorem->toString();
                    trace << "\n=== PROVEN by transitivity ===\n"
                          << final_.theorem->toString() << "\n";
                }
            }
        }

        // If the full chain didn't work, try direct e-graph approach as fallback
        if (!result.proven) {
            trace << "\n--- Attempting direct axiom chain ---\n";
            auto direct = proveDirectAxiomChain(termA, termB, kernel, trace);
            if (direct.success) {
                result.proven = true;
                result.kernelJudgment = direct.theorem->toString();
                trace << "=== PROVEN by direct chain ===\n"
                      << direct.theorem->toString() << "\n";
            }
        }

        result.proofSteps = kernel.totalTheorems();
        result.proofTrace = trace.str();
        return result;
    }

private:
    TermFactory& factory_;

    /**
     * @brief Register the Z[] ring axioms in the Kernel.
     */
    void registerRingAxioms(proof::Kernel& kernel, std::ostringstream& trace) {
        const Term* phi = factory_.phi();
        const Term* phiBar = factory_.phiBar();
        const Term* one = factory_.scalar(1.0);
        const Term* zero = factory_.scalar(0.0);
        const Term* negOne = factory_.scalar(-1.0);

        // Axiom 0:  =  + 1
        kernel.addAxiom(
            factory_.apply("*", {phi, phi}),
            factory_.apply("+", {phi, one}),
            "phi_squared", "ring_zphi");

        // Axiom 1:  = 1 -  (DEFINITION of phi-bar)
        kernel.addAxiom(
            phiBar,
            factory_.apply("+", {one, factory_.apply("neg", {phi})}),
            "phibar_def", "ring_zphi");

        // NOTE: +=1 and =-1 are DERIVABLE from axiom 0 + axiom 1.
        // They are NOT registered here  they should be DISCOVERED.
        // (ScalarOracle is not used in the --discover-all pipeline anyway.)

        // Axiom 4: neg(x) + x = 0
        // (parametric  will be instantiated via Rewrite)
        const Term* x = factory_.variable("x");
        kernel.addAxiom(
            factory_.apply("+", {factory_.apply("neg", {x}), x}),
            zero,
            "add_inverse", "ring");

        // Axiom 5: x + 0 = x
        kernel.addAxiom(
            factory_.apply("+", {x, zero}),
            x,
            "add_identity", "ring");

        // Axiom 6: x * 1 = x
        kernel.addAxiom(
            factory_.apply("*", {x, one}),
            x,
            "mul_identity", "ring");

        // Axiom 7: x * 0 = 0
        kernel.addAxiom(
            factory_.apply("*", {x, zero}),
            zero,
            "mul_zero", "ring");

        // Axiom 8: neg(neg(x)) = x
        kernel.addAxiom(
            factory_.apply("neg", {factory_.apply("neg", {x})}),
            x,
            "double_neg", "ring");

        // Axiom 9: neg(0) = 0
        kernel.addAxiom(
            factory_.apply("neg", {zero}),
            zero,
            "neg_zero", "ring");

        // Axiom 10: neg(1) = -1 (ground fact)
        kernel.addAxiom(
            factory_.apply("neg", {one}),
            negOne,
            "neg_one", "ground");

        trace << "Registered " << kernel.numAxioms() << " ring axioms\n";
    }

    /**
     * @brief Build the canonical Z[] term for a value a + b.
     *
     * Canonical form:
     *   - (0,0)  scalar(0)
     *   - (a,0)  scalar(a)
     *   - (0,b)  scalar(b) *    (or neg(scalar(|b|) * ) if b < 0)
     *   - (a,b)  scalar(a) + scalar(b) *    (with neg wrapping if needed)
     */
    [[nodiscard]] const Term* buildCanonicalTerm(const ZPhi& val) {
        int64_t a = 0, b = 0;

        // Extract int64 values (safe for our candidate terms which are small)
        if (val.a().isZero()) a = 0;
        else a = val.a().toInt64();

        if (val.b().isZero()) b = 0;
        else b = val.b().toInt64();

        const Term* phi = factory_.phi();

        if (a == 0 && b == 0) {
            return factory_.scalar(0.0);
        }
        if (b == 0) {
            return factory_.scalar(static_cast<double>(a));
        }
        if (a == 0) {
            if (b == 1) return phi;
            if (b == -1) return factory_.apply("neg", {phi});
            const Term* bTerm = factory_.scalar(static_cast<double>(b));
            return factory_.apply("*", {bTerm, phi});
        }

        // General case: a + b
        const Term* aTerm = factory_.scalar(static_cast<double>(a));
        const Term* bPhi = (b == 1) ? phi :
                           (b == -1) ? factory_.apply("neg", {phi}) :
                           factory_.apply("*", {factory_.scalar(static_cast<double>(b)), phi});

        return factory_.apply("+", {aTerm, bPhi});
    }

    /**
     * @brief Prove term = canonical via ring axiom rewriting.
     *
     * This is the scalar-upward reduction: apply ring axioms to
     * simplify the term until it matches the canonical form.
     */
    [[nodiscard]] proof::Kernel::Result proveReduction(
        const Term* term,
        const Term* canonical,
        const ZPhi& /*val*/,
        proof::Kernel& kernel,
        std::ostringstream& trace)
    {
        // If they're already structurally identical, use reflexivity
        if (term == canonical || term->encode() == canonical->encode()) {
            trace << "  " << term->toString() << " = "
                  << canonical->toString() << " [refl]\n";
            return kernel.reflexivity(term);
        }

        // Strategy: try direct axiom instantiation
        // For : apply axiom 0 ( = +1)
        // For : apply axiom 1 ( = 1-)
        // Then use congruence to propagate through operators

        // Try: is the term directly an axiom LHS?
        for (size_t i = 0; i < kernel.numAxioms(); ++i) {
            const auto& ax = kernel.getAxiom(i);
            if (term->encode() == ax.lhs->encode()) {
                // Check if ax.rhs matches canonical (possibly after further reduction)
                auto axResult = kernel.introduceAxiom(i);
                if (axResult.success) {
                    trace << "  " << term->toString() << " = "
                          << ax.rhs->toString() << " [axiom: " << ax.name << "]\n";

                    // If ax.rhs == canonical, we're done
                    if (ax.rhs->encode() == canonical->encode()) {
                        return std::move(axResult);
                    }
                    // Otherwise, chain: term = ax.rhs, then ax.rhs = canonical
                    auto rest = proveReduction(ax.rhs, canonical, ZPhi(), kernel, trace);
                    if (rest.success) {
                        return kernel.transitivity(*axResult.theorem, *rest.theorem);
                    }
                }
            }
        }

        // Fallback: reflexivity (this means we couldn't build a full chain,
        // but the equality is known to hold from the Z[] oracle)
        trace << "  " << term->toString() << "  [oracle-certified, chain incomplete]\n";
        return proof::Kernel::Result::fail("reduction chain incomplete");
    }

    /**
     * @brief Try to prove termA = termB via a direct axiom application chain.
     * Handles simple cases like: + = 1, * = +1, etc.
     */
    [[nodiscard]] proof::Kernel::Result proveDirectAxiomChain(
        const Term* termA,
        const Term* termB,
        proof::Kernel& kernel,
        std::ostringstream& trace)
    {
        // Check if termA is directly an axiom LHS with RHS == termB
        for (size_t i = 0; i < kernel.numAxioms(); ++i) {
            const auto& ax = kernel.getAxiom(i);
            // Forward: ax.lhs == termA, ax.rhs == termB
            if (ax.lhs->encode() == termA->encode() &&
                ax.rhs->encode() == termB->encode()) {
                trace << "  Direct axiom: " << ax.name << "\n";
                return kernel.introduceAxiom(i);
            }
            // Reverse: ax.lhs == termB, ax.rhs == termA
            if (ax.lhs->encode() == termB->encode() &&
                ax.rhs->encode() == termA->encode()) {
                auto fwd = kernel.introduceAxiom(i);
                if (fwd.success) {
                    trace << "  Direct axiom (sym): " << ax.name << "\n";
                    return kernel.symmetry(*fwd.theorem);
                }
            }
        }

        return proof::Kernel::Result::fail("no direct axiom chain found");
    }
};


// =============================================================================
// DISCOVERED EQUATION  a proven scalar identity
// =============================================================================

/**
 * @brief A discovered and (optionally) proven equation.
 */
struct DiscoveredEquation {
    const Term* lhs = nullptr;
    const Term* rhs = nullptr;
    ZPhi scalarValue;                    ///< shared exact value
    bool kernelProven = false;           ///< true if Kernel verified
    bool scoutValidated = false;         ///< true if SCOUT confirmed
    int  phaseTag = 0;                   ///< SCOUT phase tag
    std::string proofTrace;              ///< human-readable proof trace

    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << lhs->toString() << " = " << rhs->toString()
            << "  [val=" << scalarValue.toString()
            << ", kernel=" << (kernelProven ? "YES" : "NO")
            << ", scout=" << (scoutValidated ? "YES" : "NO")
            << ", tag=" << phaseTag << "]";
        return oss.str();
    }
};

} // namespace autodiscover::discovery

#endif // AUTODISCOVER_DISCOVERY_SCALAR_ORACLE_HPP
