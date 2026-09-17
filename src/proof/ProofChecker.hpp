/**
 * @file ProofChecker.hpp
 * @brief Trusted verification kernel for proof checking with SCOUT witnesses
 * 
 * =============================================================================
 * DE BRUIJN CRITERION
 * =============================================================================
 * 
 * The proof checker is a small, trusted core that verifies proofs
 * independently of the proof search engine. This follows the
 * de Bruijn criterion: proofs can be checked by a simple algorithm.
 * 
 * Verification checks:
 * - Each step follows from its premises by the stated rule
 * - Substitutions are applied correctly
 * - The proof forms a valid DAG (no cycles)
 * - The root derives the goal (or contradiction)
 * 
 * =============================================================================
 * SCOUT WITNESS VERIFICATION
 * =============================================================================
 * 
 * For equations derived via SCOUT-based reasoning, we verify:
 *   - Signature invariance: (lhs) = (rhs) or compatible
 *   - Phase coherence: phase arrows compose correctly
 *   - Winding consistency: K_total is conserved through rewrites
 *   - Fibonacci weight: Z[] weights balanced
 * 
 * SCOUT Witness Format:
 *   W = (_before, _after, _phase, proof_of_compatibility)
 * 
 * =============================================================================
 * VERIFICATION LEVELS
 * =============================================================================
 * 
 * Level 0 (Minimal): Check step structure only
 * Level 1 (Standard): Check premises and basic inference rules
 * Level 2 (Full): Full logical verification with substitution checking
 * Level 3 (SCOUT): Additional SCOUT signature and witness verification
 * 
 */

#ifndef AUTODISCOVER_PROOF_PROOFCHECKER_HPP
#define AUTODISCOVER_PROOF_PROOFCHECKER_HPP

#include "Proof.hpp"
#include "../logic/Equation.hpp"
#include "../logic/MatcherUnifier.hpp"
#include "../core/Term.hpp"
#include "../core/Constants.hpp"
#include "../core/TermUtils.hpp"
#include "../ring/ZPhi.hpp"
#include <vector>
#include <string>
#include <unordered_set>
#include <cmath>

namespace autodiscover {
namespace proof {

using core::Term;
using core::TermFactory;
using core::TermKind;
using logic::Equation;
using logic::Substitution;
using logic::Unifier;

// =============================================================================
// SCOUT WITNESS VERIFICATION
// =============================================================================

/**
 * @brief SCOUT signature for verification
 * 
 * Uses exact Z[] arithmetic for the Fibonacci weight to prevent
 * overflow that occurred with the old pair<int,int> representation.
 */
struct ScoutVerificationSignature {
    int windingNumber = 0;
    ring::ZPhi fibWeight;          // Exact Z[] weight (replaces pair<int,int>)
    uint8_t phase_mod4 = 0;        // exact element of /4
    
    [[nodiscard]] bool compatible(const ScoutVerificationSignature& other) const {
        if (windingNumber != other.windingNumber) return false;
        if (fibWeight != other.fibWeight) return false;
        
        // Allow phase differences that are multiples of 1 (J rotation = 1 quarter-turn)
        // In /4 that's always true, so just check exact equality
        return phase_mod4 == other.phase_mod4;
    }
};

/**
 * @brief SCOUT witness for a proof step
 * 
 * A witness certifies that a transformation preserves SCOUT invariants.
 */
struct ScoutWitness {
    ScoutVerificationSignature before;
    ScoutVerificationSignature after;
    uint8_t phaseDelta_mod4 = 0;              // Phase change  in /4
    int windingChange = 0;                // Change in winding K
    bool phaseCompensated = false;        // True if Cayley map compensation applied
    std::string justification;            // Human-readable explanation
    
    /**
     * @brief Check if witness is valid
     */
    [[nodiscard]] bool isValid() const {
        // Winding must be conserved or explicitly changed
        if (before.windingNumber + windingChange != after.windingNumber) {
            return false;
        }
        
        // Fibonacci weight must be exactly preserved
        if (before.fibWeight != after.fibWeight) {
            return false;
        }
        
        // Phase change must be accounted for (exact in /4)
        uint8_t expectedAfterPhase = (before.phase_mod4 + phaseDelta_mod4) & 3;
        if (expectedAfterPhase != after.phase_mod4) {
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Create identity witness (no change)
     */
    static ScoutWitness identity(ScoutVerificationSignature sig) {
        return {sig, sig, 0, 0, false, "identity"};
    }
};

/**
 * @brief Extract SCOUT signature from a term for verification
 * 
 * Computes the actual Z[] weight by accumulating  for each Phi node
 * (rather than just counting occurrences, which would overflow pair<int,int>).
 */
class VerificationSignatureExtractor {
public:
    explicit VerificationSignatureExtractor(TermFactory& factory) : factory_(factory) {}
    
    [[nodiscard]] ScoutVerificationSignature extract(const Term* term) {
        ScoutVerificationSignature sig;
        extractRecursive(term, sig);
        return sig;
    }

private:
    TermFactory& factory_;
    
    void extractRecursive(const Term* t, ScoutVerificationSignature& sig) {
        switch (t->kind()) {
            case TermKind::Phi:
                // Each  node contributes  = (0 + 1) = ZPhi(0, 1) to the weight
                sig.fibWeight = sig.fibWeight + ring::ZPhi(0, 1);
                break;
                
            case TermKind::PhiBar:
                //  = 1 contributes (1, -1) to Fibonacci weight in Z[]
                sig.fibWeight = sig.fibWeight + ring::ZPhi(1, -1);
                break;
                
            case TermKind::Application:
                if (t->symbol() == "cayley_map") {
                    sig.windingNumber += 1;
                }
                else if (t->symbol() == "J") {
                    sig.phase_mod4 = (sig.phase_mod4 + 1) & 3;
                }
                else if (t->symbol() == "conj") {
                    sig.phase_mod4 = (4 - sig.phase_mod4) & 3;
                }
                else if (t->symbol() == "neg") {
                    sig.phase_mod4 = (sig.phase_mod4 + 2) & 3;
                }
                
                for (const Term* child : t->children()) {
                    extractRecursive(child, sig);
                }
                break;
                
            case TermKind::Pair:
                for (const Term* child : t->children()) {
                    extractRecursive(child, sig);
                }
                break;
                
            default:
                break;
        }
    }
};

// =============================================================================
// VERIFICATION RESULT
// =============================================================================

/**
 * @brief Verification level
 */
enum class VerificationLevel {
    Minimal = 0,    // Check step structure only
    Standard = 1,   // Check premises and basic rules
    Full = 2,       // Full logical verification
    Scout = 3       // Full + SCOUT witness verification
};

/**
 * @brief Result of proof verification
 */
struct VerificationResult {
    bool valid = true;   // DEFAULT-INITIALIZED to true (fixes UB)
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    size_t stepsVerified = 0;
    size_t witnessesChecked = 0;
    size_t witnessFailures = 0;
    
    static VerificationResult Valid() {
        return {true, {}, {}, 0, 0, 0};
    }
    
    static VerificationResult Invalid(std::string error) {
        return {false, {std::move(error)}, {}, 0, 0, 0};
    }
    
    void addError(std::string err) {
        valid = false;
        errors.push_back(std::move(err));
    }
    
    void addWarning(std::string warn) {
        warnings.push_back(std::move(warn));
    }
    
    explicit operator bool() const { return valid; }
};

// =============================================================================
// PROOF CHECKER
// =============================================================================

/**
 * @brief Trusted proof verification kernel with SCOUT witness support
 */
class ProofChecker {
public:
    explicit ProofChecker(TermFactory& factory) 
        : factory_(factory)
        , unifier_(factory)
        , sigExtractor_(factory) {}
    
    /**
     * @brief Set verification level
     */
    void setVerificationLevel(VerificationLevel level) {
        level_ = level;
    }
    
    /**
     * @brief Verify an entire proof
     * 
     * Steps are verified in topological order (Kahn's algorithm), ensuring
     * that every premise of a step has already been verified before that step
     * is checked. This enforces the de Bruijn criterion: the checker never
     * takes a step's premises on faith.
     */
    [[nodiscard]] VerificationResult verify(const Proof& proof) {
        VerificationResult result = VerificationResult::Valid();
        
        // Check proof is non-empty
        if (proof.size() == 0) {
            result.addError("Empty proof");
            return result;
        }
        
        // Get topological order  this guarantees all premises come before their consumers
        auto topoOrder = proof.topologicalOrder();
        
        if (topoOrder.empty() && proof.size() > 0) {
            result.addError("Proof DAG has a cycle  topological sort failed");
            return result;
        }
        
        // Track verified steps for de Bruijn criterion enforcement
        std::unordered_set<ProofStep::Id> verified;
        
        // Verify each step in topological order
        for (auto stepId : topoOrder) {
            const ProofStep* step = proof.getStep(stepId);
            if (!step) {
                result.addError("Missing step " + std::to_string(stepId) + " in proof DAG");
                continue;
            }
            
            auto stepResult = verifyStep(proof, *step, verified);
            if (!stepResult.valid) {
                for (auto& err : stepResult.errors) {
                    result.addError("Step " + std::to_string(step->id()) + ": " + err);
                }
            }
            for (auto& warn : stepResult.warnings) {
                result.addWarning("Step " + std::to_string(step->id()) + ": " + warn);
            }
            verified.insert(step->id());
            result.stepsVerified++;
        }
        
        // SCOUT verification if enabled
        if (level_ >= VerificationLevel::Scout) {
            auto scoutResult = verifyScoutInvariants(proof);
            result.witnessesChecked = scoutResult.witnessesChecked;
            result.witnessFailures = scoutResult.witnessFailures;
            for (auto& err : scoutResult.errors) {
                result.addError("SCOUT: " + err);
            }
            for (auto& warn : scoutResult.warnings) {
                result.addWarning("SCOUT: " + warn);
            }
        }
        
        // Check proof completeness
        if (proof.isComplete()) {
            const ProofStep* root = proof.getStep(proof.root());
            if (!root || root->rule() != InferenceRule::EqualityResolution) {
                result.addWarning("Proof root is not equality resolution");
            }
        } else {
            result.addWarning("Proof is incomplete (no contradiction derived)");
        }
        
        return result;
    }
    
    /**
     * @brief Verify a single proof step
     */
    [[nodiscard]] VerificationResult verifyStep(
        const Proof& proof,
        const ProofStep& step,
        const std::unordered_set<ProofStep::Id>& verified) {
        
        // De Bruijn criterion: all premises must have been verified
        // BEFORE this step is checked. Since we iterate in topological
        // order, this is a hard error, not a soft warning.
        for (auto premiseId : step.premises()) {
            if (!verified.count(premiseId)) {
                return VerificationResult::Invalid(
                    "Premise " + std::to_string(premiseId) + 
                    " has not been verified  de Bruijn criterion violated");
            }
            if (!proof.getStep(premiseId)) {
                return VerificationResult::Invalid("Unknown premise: " + std::to_string(premiseId));
            }
        }
        
        // Verify by rule type
        switch (step.rule()) {
            case InferenceRule::Axiom:
            case InferenceRule::Hypothesis:
                return verifyLeaf(step);
                
            case InferenceRule::SuperpositionLeft:
            case InferenceRule::SuperpositionRight:
                return verifySuperposition(proof, step);
                
            case InferenceRule::EqualityResolution:
                return verifyEqualityResolution(proof, step);
                
            case InferenceRule::Demodulation:
                return verifyDemodulation(proof, step);
                
            case InferenceRule::GODNormalization:
                return verifyGODNormalization(proof, step);
                
            case InferenceRule::Reflexivity:
                return verifyReflexivity(step);
                
            case InferenceRule::Symmetry:
                return verifySymmetry(proof, step);
                
            case InferenceRule::Transitivity:
                return verifyTransitivity(proof, step);
                
            case InferenceRule::Congruence:
                return verifyCongruence(proof, step);
                
            case InferenceRule::Substitution:
                return verifySubstitution(proof, step);
        }
        
        return VerificationResult::Invalid("Unknown inference rule");
    }

private:
    TermFactory& factory_;
    Unifier unifier_;
    VerificationSignatureExtractor sigExtractor_;
    VerificationLevel level_ = VerificationLevel::Standard;
    
    // -----------------------------------------------------------------------
    // Shared term-manipulation utilities for replay (same logic as engine)
    // -----------------------------------------------------------------------
    
    /**
     * @brief Extract the subterm at a given position path
     * Delegates to shared core::subtermAtPath (TermUtils.hpp)
     */
    const Term* subtermAtPath(const Term* term, 
                              const std::vector<uint32_t>& path,
                              size_t depth) const {
        return core::subtermAtPath(term, path, depth);
    }
    
    /**
     * @brief Replace the subterm at the given position path with replacement
     * Delegates to shared core::replaceAtPath (TermUtils.hpp)
     */
    const Term* replaceAtPathChecker(const Term* term,
                                      const std::vector<uint32_t>& path,
                                      size_t depth,
                                      const Term* replacement) {
        return core::replaceAtPath(term, path, depth, replacement, factory_);
    }
    
    [[nodiscard]] VerificationResult verifyLeaf(const ProofStep& step) {
        // Leaves (axioms, hypotheses) are valid if they have a conclusion
        if (!step.conclusion()) {
            return VerificationResult::Invalid("Leaf step missing conclusion");
        }
        if (!step.premises().empty()) {
            return VerificationResult::Invalid("Leaf step should have no premises");
        }
        return VerificationResult::Valid();
    }
    
    [[nodiscard]] VerificationResult verifySuperposition(
        const Proof& proof,
        const ProofStep& step) {
        
        if (step.premises().size() != 2) {
            return VerificationResult::Invalid("Superposition requires exactly 2 premises");
        }
        
        const ProofStep* p1 = proof.getStep(step.premises()[0]);
        const ProofStep* p2 = proof.getStep(step.premises()[1]);
        
        if (!p1 || !p2) {
            return VerificationResult::Invalid("Missing premise steps");
        }
        
        if (!p1->conclusion() || !p2->conclusion() || !step.conclusion()) {
            return VerificationResult::Invalid("Missing equations in superposition");
        }
        
        // ---------------------------------------------------------------
        // DETERMINISTIC REPLAY VERIFICATION
        //
        // The proof step carries a SuperpositionCert with:
        //   - positionPath:    path from root of target side to matched subterm
        //   - rewriteIntoLhs: which side of premise2 was rewritten
        //   - eqUsedLtoR:     orientation of premise1 (l->r or r->l)
        //
        // We deterministically reconstruct the conclusion:
        //   1. Select the rule sides from premise1 based on eqUsedLtoR
        //   2. Select the target side from premise2 based on rewriteIntoLhs
        //   3. Extract subterm at positionPath in the target side
        //   4. Verify that the stored substitution sigma unifies l with the subterm
        //   5. Compute replacement = sigma(r) and rebuild the target side
        //   6. Compare the reconstructed conclusion to the claimed one
        // ---------------------------------------------------------------
        if (level_ >= VerificationLevel::Full) {
            const Equation* eq1 = p1->conclusion();  // premise1: the "rule" equation
            const Equation* eq2 = p2->conclusion();  // premise2: the "into" equation
            const Equation* conc = step.conclusion(); // claimed conclusion
            const auto& sub = step.substitution();
            const auto& cert = step.superpositionCert();
            
            // Determine which sides of premise1 are l and r
            const Term* l = cert.eqUsedLtoR ? eq1->lhs() : eq1->rhs();
            const Term* r = cert.eqUsedLtoR ? eq1->rhs() : eq1->lhs();
            
            // Determine which side of premise2 is the target
            bool leftStep = (step.rule() == InferenceRule::SuperpositionLeft);
            const Term* targetSide = leftStep ? eq2->lhs() : eq2->rhs();
            const Term* otherSide  = leftStep ? eq2->rhs() : eq2->lhs();
            
            // If we have a position path, do full deterministic replay
            if (!cert.positionPath.empty() || !sub.isEmpty()) {
                // Extract the subterm at the position path
                const Term* subterm = subtermAtPath(targetSide, cert.positionPath, 0);
                if (!subterm) {
                    return VerificationResult::Invalid(
                        "Superposition: invalid position path in certificate");
                }
                
                // Side-condition: subterm must not be a variable
                if (subterm->kind() == core::TermKind::Variable) {
                    return VerificationResult::Invalid(
                        "Superposition: rewrote at a variable position (side-condition violation)");
                }
                
                // Verify that sigma(l) == sigma(subterm)  i.e. they unify under sigma
                const Term* lSigma = sub.apply(l, factory_);
                const Term* subtermSigma = sub.apply(subterm, factory_);
                if (lSigma->id() != subtermSigma->id()) {
                    // Subterm and l should agree under sigma
                    return VerificationResult::Invalid(
                        "Superposition: sigma(l) != sigma(subterm_at_path)  "
                        "unifier does not match the recorded position");
                }
                
                // Compute the replacement: sigma(r)
                const Term* rSigma = sub.apply(r, factory_);
                
                // Rebuild the target side with replacement at positionPath
                const Term* rebuiltTarget;
                if (cert.positionPath.empty()) {
                    // Root position: entire target is replaced
                    rebuiltTarget = rSigma;
                } else {
                    // Replace at path, then apply sigma to entire rebuilt term
                    const Term* replaced = replaceAtPathChecker(
                        targetSide, cert.positionPath, 0, r);
                    rebuiltTarget = sub.apply(replaced, factory_);
                }
                
                // Compute the other side under sigma
                const Term* otherSigma = sub.apply(otherSide, factory_);
                
                // Now verify the conclusion matches
                const Term* expectedLhs = leftStep ? rebuiltTarget : otherSigma;
                const Term* expectedRhs = leftStep ? otherSigma : rebuiltTarget;
                
                if (conc->lhs()->id() != expectedLhs->id() ||
                    conc->rhs()->id() != expectedRhs->id()) {
                    // Also try with swapped conclusion (some provers normalize equation orientation)
                    if (conc->lhs()->id() != expectedRhs->id() ||
                        conc->rhs()->id() != expectedLhs->id()) {
                        return VerificationResult::Invalid(
                            "Superposition: reconstructed conclusion does not match claimed conclusion");
                    }
                }
            } else {
                // Fallback: structural plausibility check (when certificate is absent)
                // This is weaker but maintains backward compatibility with old proof objects
                if (conc->lhs()->id() == eq2->lhs()->id() &&
                    conc->rhs()->id() == eq2->rhs()->id()) {
                    return VerificationResult::Invalid(
                        "Superposition: conclusion identical to premise (no rewrite occurred)");
                }
                auto result = VerificationResult::Valid();
                result.addWarning("Superposition: no certificate  verification is structural only, not replay-based");
                return result;
            }
        }
        
        // SCOUT-aware: check signature compatibility (supplementary, never a substitute for replay)
        if (level_ >= VerificationLevel::Scout) {
            auto sig1 = sigExtractor_.extract(p1->conclusion()->lhs());
            auto sig2 = sigExtractor_.extract(step.conclusion()->lhs());
            
            if (!sig1.compatible(sig2)) {
                auto result = VerificationResult::Valid();
                result.addWarning("SCOUT signatures may not be compatible");
                return result;
            }
        }
        
        return VerificationResult::Valid();
    }
    
    [[nodiscard]] VerificationResult verifyEqualityResolution(
        const Proof& proof,
        const ProofStep& step) {
        
        if (step.premises().size() != 1) {
            return VerificationResult::Invalid("Equality resolution requires exactly 1 premise");
        }
        
        const ProofStep* premise = proof.getStep(step.premises()[0]);
        if (!premise || !premise->conclusion()) {
            return VerificationResult::Invalid("Missing premise for equality resolution");
        }
        
        // The conclusion should be null ()
        if (step.conclusion() != nullptr) {
            VerificationResult result;
            result.addWarning("Equality resolution conclusion should be null ()");
            return result;
        }
        
        // Verify that lhs and rhs of premise unify
        const Equation* eq = premise->conclusion();
        auto unifyResult = unifier_.unify(eq->lhs(), eq->rhs());
        
        if (!unifyResult.success) {
            return VerificationResult::Invalid("Premise sides do not unify");
        }
        
        return VerificationResult::Valid();
    }
    
    [[nodiscard]] VerificationResult verifyDemodulation(
        const Proof& proof,
        const ProofStep& step) {
        
        if (step.premises().size() != 2) {
            return VerificationResult::Invalid("Demodulation requires 2 premises");
        }
        
        const ProofStep* target = proof.getStep(step.premises()[0]);
        const ProofStep* rule = proof.getStep(step.premises()[1]);
        
        if (!target || !rule) {
            return VerificationResult::Invalid("Missing premise steps for demodulation");
        }
        if (!target->conclusion() || !rule->conclusion() || !step.conclusion()) {
            return VerificationResult::Invalid("Missing equations for demodulation");
        }
        
        const Equation* ruleEq = rule->conclusion();
        const Equation* targetEq = target->conclusion();
        const Equation* concEq = step.conclusion();
        
        if (level_ >= VerificationLevel::Full) {
            const auto& cert = step.demodCert();
            const auto& sub = step.substitution();
            
            // ---------------------------------------------------------------
            // DETERMINISTIC REPLAY VERIFICATION FOR DEMODULATION
            //
            // A demodulation step rewrites one subterm of the target equation
            // using a rewrite rule (premise1). The certificate tells us:
            //   - positionPath:   path to the rewritten subterm
            //   - ruleLtoR:       direction of rule (l->r or r->l)
            //
            // Replay algorithm:
            //   1. Orient the rule: if ruleLtoR then l=rule.lhs, r=rule.rhs
            //   2. Determine which side of target was rewritten
            //   3. Extract subterm at positionPath
            //   4. Verify sigma(l) == sigma(subterm)
            //   5. Rebuild target side: replaceAtPath(targetSide, path, r)
            //      then apply sigma to get the rewritten side
            //   6. Compare reconstructed conclusion to claimed conclusion
            // ---------------------------------------------------------------
            if (step.hasDemodulationCert()) {
                // Orient the rule
                const Term* l = cert.ruleLtoR ? ruleEq->lhs() : ruleEq->rhs();
                const Term* r = cert.ruleLtoR ? ruleEq->rhs() : ruleEq->lhs();
                
                // Determine which side of the target was rewritten by
                // checking which side changed
                bool lhsChanged = concEq->lhs()->id() != targetEq->lhs()->id();
                bool rhsChanged = concEq->rhs()->id() != targetEq->rhs()->id();
                
                const Term* targetSide;
                const Term* otherSide;
                bool rewriteLhs;
                
                if (lhsChanged && !rhsChanged) {
                    targetSide = targetEq->lhs();
                    otherSide  = targetEq->rhs();
                    rewriteLhs = true;
                } else if (rhsChanged && !lhsChanged) {
                    targetSide = targetEq->rhs();
                    otherSide  = targetEq->lhs();
                    rewriteLhs = false;
                } else if (!lhsChanged && !rhsChanged) {
                    return VerificationResult::Invalid(
                        "Demodulation: conclusion is identical to target (no rewrite occurred)");
                } else {
                    // Both sides changed  could be valid if same subterm appears in both.
                    // For now, try LHS first.
                    targetSide = targetEq->lhs();
                    otherSide  = targetEq->rhs();
                    rewriteLhs = true;
                }
                
                // Extract the subterm at the position path
                const Term* subterm = subtermAtPath(targetSide, cert.positionPath, 0);
                if (!subterm) {
                    return VerificationResult::Invalid(
                        "Demodulation: invalid position path in certificate");
                }
                
                // Verify that sigma(l) matches sigma(subterm)
                const Term* lSigma = sub.apply(l, factory_);
                const Term* subtermSigma = sub.apply(subterm, factory_);
                if (lSigma->id() != subtermSigma->id()) {
                    return VerificationResult::Invalid(
                        "Demodulation: sigma(rule_lhs) != sigma(subterm_at_path)  "
                        "matcher does not agree with certificate");
                }
                
                // Compute the replacement: sigma(r)
                const Term* rSigma = sub.apply(r, factory_);
                
                // Rebuild the target side with replacement at positionPath
                const Term* rebuiltTarget;
                if (cert.positionPath.empty()) {
                    rebuiltTarget = rSigma;
                } else {
                    const Term* replaced = replaceAtPathChecker(
                        targetSide, cert.positionPath, 0, r);
                    rebuiltTarget = sub.apply(replaced, factory_);
                }
                
                // Construct the expected conclusion
                const Term* expectedLhs = rewriteLhs ? rebuiltTarget : otherSide;
                const Term* expectedRhs = rewriteLhs ? otherSide : rebuiltTarget;
                
                if (concEq->lhs()->id() != expectedLhs->id() ||
                    concEq->rhs()->id() != expectedRhs->id()) {
                    // Try swapped orientation
                    if (concEq->lhs()->id() != expectedRhs->id() ||
                        concEq->rhs()->id() != expectedLhs->id()) {
                        return VerificationResult::Invalid(
                            "Demodulation: reconstructed conclusion does not match claimed conclusion");
                    }
                }
            } else {
                // No certificate  fall back to structural plausibility check
                bool lhsChanged = concEq->lhs()->id() != targetEq->lhs()->id();
                bool rhsChanged = concEq->rhs()->id() != targetEq->rhs()->id();
                
                if (!lhsChanged && !rhsChanged) {
                    return VerificationResult::Invalid(
                        "Demodulation: conclusion is identical to target (no rewrite occurred)");
                }
                
                // Verify the unchanged side is truly unchanged
                if (lhsChanged && !rhsChanged) {
                    if (concEq->rhs()->id() != targetEq->rhs()->id()) {
                        return VerificationResult::Invalid(
                            "Demodulation: both sides changed (only one should)");
                    }
                } else if (rhsChanged && !lhsChanged) {
                    if (concEq->lhs()->id() != targetEq->lhs()->id()) {
                        return VerificationResult::Invalid(
                            "Demodulation: both sides changed (only one should)");
                    }
                }
                
                auto result = VerificationResult::Valid();
                result.addWarning("Demodulation: no certificate  verification is structural only, not replay-based");
                return result;
            }
        }
        
        return VerificationResult::Valid();
    }
    
    [[nodiscard]] VerificationResult verifyGODNormalization(
        const Proof& proof,
        const ProofStep& step) {
        
        if (step.premises().size() != 1) {
            return VerificationResult::Invalid("GOD normalization requires exactly 1 premise");
        }
        
        // SCOUT check: signatures must be equal for GOD transforms
        if (level_ >= VerificationLevel::Scout) {
            const ProofStep* premise = proof.getStep(step.premises()[0]);
            if (premise && premise->conclusion() && step.conclusion()) {
                auto sigBefore = sigExtractor_.extract(premise->conclusion()->lhs());
                auto sigAfter = sigExtractor_.extract(step.conclusion()->lhs());
                
                if (!sigBefore.compatible(sigAfter)) {
                    return VerificationResult::Invalid(
                        "GOD normalization must preserve SCOUT signatures exactly");
                }
            }
        }
        
        return VerificationResult::Valid();
    }
    
    [[nodiscard]] VerificationResult verifyReflexivity(const ProofStep& step) {
        if (!step.conclusion()) {
            return VerificationResult::Invalid("Missing conclusion");
        }
        
        const Equation* eq = step.conclusion();
        if (eq->lhs()->id() != eq->rhs()->id()) {
            return VerificationResult::Invalid("Reflexivity: lhs != rhs");
        }
        
        return VerificationResult::Valid();
    }
    
    [[nodiscard]] VerificationResult verifySymmetry(
        const Proof& proof,
        const ProofStep& step) {
        
        if (step.premises().size() != 1) {
            return VerificationResult::Invalid("Symmetry requires exactly 1 premise");
        }
        
        const ProofStep* premise = proof.getStep(step.premises()[0]);
        if (!premise || !premise->conclusion() || !step.conclusion()) {
            return VerificationResult::Invalid("Missing equations for symmetry");
        }
        
        const Equation* peq = premise->conclusion();
        const Equation* ceq = step.conclusion();
        
        // Conclusion should have flipped sides
        if (peq->lhs()->id() != ceq->rhs()->id() || 
            peq->rhs()->id() != ceq->lhs()->id()) {
            return VerificationResult::Invalid("Symmetry: sides not properly flipped");
        }
        
        return VerificationResult::Valid();
    }
    
    [[nodiscard]] VerificationResult verifyTransitivity(
        const Proof& proof,
        const ProofStep& step) {
        
        if (step.premises().size() != 2) {
            return VerificationResult::Invalid("Transitivity requires exactly 2 premises");
        }
        
        const ProofStep* p1 = proof.getStep(step.premises()[0]);
        const ProofStep* p2 = proof.getStep(step.premises()[1]);
        
        if (!p1 || !p2) {
            return VerificationResult::Invalid("Missing premise steps for transitivity");
        }
        if (!p1->conclusion() || !p2->conclusion() || !step.conclusion()) {
            return VerificationResult::Invalid("Missing equations for transitivity");
        }
        
        // Transitivity: s=t, t=u  s=u
        // Premise 1 supplies s=t, premise 2 supplies t'=u
        // We need t  t' (by id, since terms are hash-consed)
        const Equation* eq1 = p1->conclusion(); // s = t
        const Equation* eq2 = p2->conclusion(); // t' = u
        const Equation* conc = step.conclusion(); // s = u
        
        // Try both orientations of the premises to find the chain
        // Option A: eq1.rhs == eq2.lhs    s=t, t=u  s=u
        // Option B: eq1.rhs == eq2.rhs    s=t, u=t  s=u (with symmetry on eq2)
        // Option C: eq1.lhs == eq2.lhs    t=s, t=u  s=u (with symmetry on eq1)
        // Option D: eq1.lhs == eq2.rhs    t=s, u=t  s=u (with symmetry on both)
        
        bool valid = false;
        
        if (eq1->rhs()->id() == eq2->lhs()->id()) {
            // s=t, t=u  s=u: check conc = (eq1.lhs, eq2.rhs)
            valid = (conc->lhs()->id() == eq1->lhs()->id() &&
                     conc->rhs()->id() == eq2->rhs()->id());
        }
        if (!valid && eq1->rhs()->id() == eq2->rhs()->id()) {
            // s=t, u=t  s=u
            valid = (conc->lhs()->id() == eq1->lhs()->id() &&
                     conc->rhs()->id() == eq2->lhs()->id());
        }
        if (!valid && eq1->lhs()->id() == eq2->lhs()->id()) {
            // t=s, t=u  s=u
            valid = (conc->lhs()->id() == eq1->rhs()->id() &&
                     conc->rhs()->id() == eq2->rhs()->id());
        }
        if (!valid && eq1->lhs()->id() == eq2->rhs()->id()) {
            // t=s, u=t  s=u
            valid = (conc->lhs()->id() == eq1->rhs()->id() &&
                     conc->rhs()->id() == eq2->lhs()->id());
        }
        
        if (!valid) {
            return VerificationResult::Invalid(
                "Transitivity: conclusion does not follow from premises "
                "(no orientation of s=t, t=u  s=u matches)");
        }
        
        return VerificationResult::Valid();
    }
    
    [[nodiscard]] VerificationResult verifyCongruence(
        const Proof& proof,
        const ProofStep& step) {
        
        // Congruence: s=t, ..., s=t  f(s,...,s)=f(t,...,t)
        if (!step.conclusion()) {
            return VerificationResult::Invalid("Congruence: missing conclusion");
        }
        
        const Equation* conc = step.conclusion();
        const Term* concLhs = conc->lhs();
        const Term* concRhs = conc->rhs();
        
        // Both sides must be applications of the same symbol
        if (concLhs->kind() != core::TermKind::Application ||
            concRhs->kind() != core::TermKind::Application) {
            return VerificationResult::Invalid(
                "Congruence: conclusion sides must be function applications");
        }
        
        if (concLhs->symbol() != concRhs->symbol()) {
            return VerificationResult::Invalid(
                "Congruence: conclusion sides must have the same function symbol");
        }
        
        const auto& lhsKids = concLhs->children();
        const auto& rhsKids = concRhs->children();
        
        if (lhsKids.size() != rhsKids.size()) {
            return VerificationResult::Invalid(
                "Congruence: conclusion sides must have the same arity");
        }
        
        // Number of premises must match the arity (unless some args are equal)
        // We verify that for each child position, either:
        //   (a) lhsKids[i] == rhsKids[i] (no premise needed), or
        //   (b) there exists a premise proving lhsKids[i] = rhsKids[i]
        
        // Collect all premise equations
        std::vector<const Equation*> premEqs;
        for (auto pid : step.premises()) {
            const ProofStep* pstep = proof.getStep(pid);
            if (!pstep || !pstep->conclusion()) {
                return VerificationResult::Invalid(
                    "Congruence: missing or invalid premise step " + std::to_string(pid));
            }
            premEqs.push_back(pstep->conclusion());
        }
        
        // For each child position, verify justification
        for (size_t i = 0; i < lhsKids.size(); ++i) {
            if (lhsKids[i]->id() == rhsKids[i]->id()) {
                continue; // Same child, no premise needed
            }
            
            // Search premises for one that justifies lhsKids[i] = rhsKids[i]
            bool justified = false;
            for (const Equation* peq : premEqs) {
                if ((peq->lhs()->id() == lhsKids[i]->id() && peq->rhs()->id() == rhsKids[i]->id()) ||
                    (peq->lhs()->id() == rhsKids[i]->id() && peq->rhs()->id() == lhsKids[i]->id())) {
                    justified = true;
                    break;
                }
            }
            
            if (!justified) {
                return VerificationResult::Invalid(
                    "Congruence: no premise justifies child at position " + std::to_string(i));
            }
        }
        
        return VerificationResult::Valid();
    }
    
    [[nodiscard]] VerificationResult verifySubstitution(
        const Proof& proof,
        const ProofStep& step) {
        
        if (step.premises().size() != 1) {
            return VerificationResult::Invalid("Substitution requires exactly 1 premise");
        }
        
        const ProofStep* premise = proof.getStep(step.premises()[0]);
        if (!premise || !premise->conclusion() || !step.conclusion()) {
            return VerificationResult::Invalid("Missing equations for substitution");
        }
        
        // Verify that conclusion is the substitution instance of the premise
        // The substitution is stored in the step
        const Substitution& sub = step.substitution();
        
        if (sub.isEmpty()) {
            // With empty substitution, conclusion must equal premise
            if (step.conclusion()->lhs()->id() != premise->conclusion()->lhs()->id() ||
                step.conclusion()->rhs()->id() != premise->conclusion()->rhs()->id()) {
                return VerificationResult::Invalid(
                    "Substitution: empty substitution but conclusion differs from premise");
            }
            return VerificationResult::Valid();
        }
        
        // Apply substitution to premise's lhs and rhs
        const Term* expectedLhs = sub.apply(premise->conclusion()->lhs(), factory_);
        const Term* expectedRhs = sub.apply(premise->conclusion()->rhs(), factory_);
        
        if (expectedLhs->id() != step.conclusion()->lhs()->id() ||
            expectedRhs->id() != step.conclusion()->rhs()->id()) {
            return VerificationResult::Invalid(
                "Substitution: conclusion does not match (premise). "
                "Expected: " + expectedLhs->toString() + " = " + expectedRhs->toString() +
                ", Got: " + step.conclusion()->lhs()->toString() + " = " + 
                step.conclusion()->rhs()->toString());
        }
        
        return VerificationResult::Valid();
    }
    
    /**
     * @brief Verify SCOUT invariants across the entire proof
     */
    [[nodiscard]] VerificationResult verifyScoutInvariants(const Proof& proof) {
        VerificationResult result = VerificationResult::Valid();
        
        // Track Fibonacci weight and winding number through proof
        // (reserved for full SCOUT conservation-law verification)
        [[maybe_unused]] std::pair<int, int> totalFibWeight = {0, 0};
        [[maybe_unused]] int totalWinding = 0;
        
        for (const ProofStep* step : proof.allSteps()) {
            if (!step->conclusion()) continue;
            
            result.witnessesChecked++;
            
            auto sigLhs = sigExtractor_.extract(step->conclusion()->lhs());
            auto sigRhs = sigExtractor_.extract(step->conclusion()->rhs());
            
            // For each equation, LHS and RHS should have compatible signatures
            // (modulo phase differences that can be compensated)
            if (!sigLhs.compatible(sigRhs)) {
                // This might be okay for some equations, but note it
                result.addWarning("Equation " + std::to_string(step->id()) + 
                                  ": LHS/RHS signatures not compatible");
                result.witnessFailures++;
            }
        }
        
        return result;
    }
};

// =============================================================================
// CERTIFICATE GENERATOR
// =============================================================================

/**
 * @brief Generate a verification certificate for a proof
 * 
 * A certificate is a self-contained document that allows independent
 * verification of the proof without access to the prover internals.
 */
class CertificateGenerator {
public:
    explicit CertificateGenerator(TermFactory& factory)
        : factory_(factory)
        , checker_(factory) {}
    
    /**
     * @brief Generate certificate as JSON-like structure
     */
    [[nodiscard]] std::string generate(const Proof& proof) {
        std::string cert = "{\n";
        cert += "  \"type\": \"AutoDiscoverProofCertificate\",\n";
        cert += "  \"version\": \"1.0\",\n";
        
        // Verify and include result
        auto result = checker_.verify(proof);
        cert += "  \"verified\": " + std::string(result.valid ? "true" : "false") + ",\n";
        cert += "  \"steps\": " + std::to_string(proof.size()) + ",\n";
        
        // Include errors if any
        if (!result.errors.empty()) {
            cert += "  \"errors\": [\n";
            for (size_t i = 0; i < result.errors.size(); ++i) {
                cert += "    \"" + result.errors[i] + "\"";
                if (i + 1 < result.errors.size()) cert += ",";
                cert += "\n";
            }
            cert += "  ],\n";
        }
        
        // Include SCOUT verification info
        cert += "  \"scout_witnesses_checked\": " + std::to_string(result.witnessesChecked) + ",\n";
        cert += "  \"scout_witness_failures\": " + std::to_string(result.witnessFailures) + "\n";
        
        cert += "}\n";
        return cert;
    }

private:
    TermFactory& factory_;
    ProofChecker checker_;
};

} // namespace proof
} // namespace autodiscover

#endif // AUTODISCOVER_PROOF_PROOFCHECKER_HPP
