/**
 * @file InferenceEngine.hpp
 * @brief Superposition/Paramodulation inference engine with SCOUT integration
 * 
 * =============================================================================
 * SUPERPOSITION CALCULUS
 * =============================================================================
 * 
 * Implements the superposition calculus for equational reasoning:
 * - Superposition Left/Right (paramodulation with ordering)
 * - Equality Resolution
 * - Demodulation (simplification)
 * - Subsumption
 * 
 * =============================================================================
 * SCOUT SIGNATURE EXTRACTION
 * =============================================================================
 * 
 * For each term t, extract the SCOUT signature:
 *   (t) = (K, z_Fib, _arrow)
 * 
 * where:
 *   - K = winding number (integer)
 *   - z_Fib = Fibonacci weight (in Z[])
 *   - _arrow = phase arrow (in J-plane)
 * 
 * Signature Invariant:
 *   (t) = (s)  GOD(t) = GOD(s)   (converse may not hold)
 * 
 * =============================================================================
 * PHASE-AWARE INFERENCE
 * =============================================================================
 * 
 * Phase Superposition Rule:
 *   l = r   s[l']_p = t
 *    where l' = lC_J(),   (-1,1)
 *   s[rC_J()]_p = t
 * 
 * This allows inference even when terms differ by a phase factor.
 * 
 * The engine uses a saturation loop with given clause selection.
 */

#ifndef AUTODISCOVER_LOGIC_INFERENCEENGINE_HPP
#define AUTODISCOVER_LOGIC_INFERENCEENGINE_HPP

#include "Equation.hpp"
#include "KnowledgeBase.hpp"
#include "MatcherUnifier.hpp"
#include "Normalizer.hpp"
#include "../core/Term.hpp"
#include "../core/TermUtils.hpp"
#include "../core/Constants.hpp"
#include "../fingerprint/Semantic.hpp"
#include "../ring/ZPhi.hpp"
#include <memory>
#include <vector>
#include <optional>
#include <functional>
#include <chrono>
#include <cmath>
#include <iostream>
#include <unordered_set>
#include <unordered_map>

namespace autodiscover {
namespace logic {

using core::Term;
using core::TermFactory;
using core::TermKind;

// =============================================================================
// SCOUT SIGNATURE
// =============================================================================

/**
 * @brief SCOUT signature for a term
 * 
 * The signature captures phase and Fibonacci-weight information
 * that is invariant under GOD canonicalization.
 */
struct ScoutSignature {
    int windingNumber = 0;           // K: integer winding number
    ring::ZPhi fibWeight{0, 0};      // z_Fib in Z[]  exact, no overflow
    uint8_t phase_mod4 = 0;          // /(/2) mod 4  exact element of /4
    
    /**
     * @brief Check if two signatures are equivalent (exact)
     */
    [[nodiscard]] bool equivalent(const ScoutSignature& other) const {
        return windingNumber == other.windingNumber &&
               fibWeight == other.fibWeight &&
               phase_mod4 == other.phase_mod4;
    }
    
    /**
     * @brief Compute hash for signature (exact, no float discretization)
     */
    [[nodiscard]] size_t hash() const {
        size_t h = std::hash<int>()(windingNumber);
        h ^= std::hash<std::string>{}(fibWeight.toString()) << 1;
        h ^= std::hash<uint8_t>()(phase_mod4) << 3;
        return h;
    }
    
    bool operator==(const ScoutSignature& other) const {
        return equivalent(other);
    }
};

/**
 * @brief Extract SCOUT signature from a term
 */
class SignatureExtractor {
public:
    explicit SignatureExtractor(TermFactory& factory) : factory_(factory) {}
    
    /**
     * @brief Extract signature from term
     */
    [[nodiscard]] ScoutSignature extract(const Term* term) {
        ScoutSignature sig;
        extractRecursive(term, sig);
        return sig;
    }
    
    /**
     * @brief Check if terms have matching signatures
     */
    [[nodiscard]] bool signaturesMatch(const Term* t1, const Term* t2) {
        return extract(t1).equivalent(extract(t2));
    }
    
private:
    TermFactory& factory_;
    
    void extractRecursive(const Term* t, ScoutSignature& sig) {
        switch (t->kind()) {
            case TermKind::Phi:
                //  contributes (0, 1) to Fibonacci weight in Z[]
                sig.fibWeight = sig.fibWeight + ring::ZPhi(0, 1);
                break;
                
            case TermKind::PhiBar:
                //  = 1 contributes (1, -1) to Fibonacci weight in Z[]
                sig.fibWeight = sig.fibWeight + ring::ZPhi(1, -1);
                break;
                
            case TermKind::Scalar: {
                // Incorporate scalar value into signature
                // Map to nearest integer for Z[] contribution
                double sv = t->scalarValue();
                int intPart = static_cast<int>(std::round(sv));
                sig.fibWeight = sig.fibWeight + ring::ZPhi(intPart, 0);
                break;
            }
                
            case TermKind::Application:
                if (t->symbol() == "cayley_map") {
                    // Cayley map contributes to phase
                    sig.windingNumber += 1;
                }
                else if (t->symbol() == "J" || t->symbol() == "j_plane") {
                    // J-plane imaginary unit: +1 quarter-turn in /4
                    sig.phase_mod4 = (sig.phase_mod4 + 1) & 3;
                }
                else if (t->symbol() == "conj") {
                    // Conjugation negates phase:   -, i.e. p  (4-p) mod 4
                    sig.phase_mod4 = (4 - sig.phase_mod4) & 3;
                }
                else if (t->symbol() == "neg") {
                    // Negation adds  (2 quarter-turns) in /4
                    sig.phase_mod4 = (sig.phase_mod4 + 2) & 3;
                }
                
                // Recurse into children
                for (const Term* child : t->children()) {
                    extractRecursive(child, sig);
                }
                break;
                
            case TermKind::Pair:
                // Cayley-Dickson pair: combine children signatures
                for (const Term* child : t->children()) {
                    extractRecursive(child, sig);
                }
                break;
                
            default:
                // Variables and others: no contribution
                break;
        }
    }
};

// =============================================================================
// INFERENCE RESULT AND CONFIG
// =============================================================================

/**
 * @brief Result of inference engine execution
 */
enum class InferenceResult {
    Saturated,        // No more inferences possible (equational theory complete)
    ProofFound,       // Empty clause derived (theorem proven)
    ResourceLimit,    // Time or memory limit reached
    StepLimit,        // Maximum step count reached
    Unknown           // Inconclusive
};

/**
 * @brief Configuration for inference engine
 */
struct InferenceConfig {
    size_t maxSteps = 100000;
    size_t maxClauses = 1000000;
    size_t maxInferencesPerGiven = 5000;  // Cap inferences from one given clause
    size_t heartbeatInterval = 100;       // Print progress every N steps
    std::chrono::seconds timeout{300};  // 5 minutes default
    std::chrono::milliseconds innerDeadline{500}; // Per-given-clause time limit
    bool useDemodulation = true;
    bool useSubsumption = true;
    bool useGODNormalization = true;
    bool usePhaseInference = true;      // Enable phase-aware inference
    bool useSignatureFiltering = true;  // Use SCOUT signatures for filtering
    bool verbose = false;
    // NOTE: Phase matching uses exact uint8_t phase_mod4 arithmetic,
    // not floating-point tolerance. No phaseTolerance needed.
};

/**
 * @brief Statistics from inference run
 */
struct InferenceStats {
    size_t steps = 0;
    size_t generatedClauses = 0;
    size_t keptClauses = 0;
    size_t deletedBySubsumption = 0;
    size_t deletedByDemodulation = 0;
    size_t superpositions = 0;
    size_t phaseSuperpositions = 0;     // Phase-aware superpositions
    size_t equalityResolutions = 0;
    size_t signatureMatches = 0;        // Terms matched by signature
    size_t signatureMismatches = 0;     // Terms filtered by signature
    size_t selfSkips = 0;               // Self-superposition skips
    size_t deadlineCuts = 0;            // Given clauses cut short by inner deadline
    std::chrono::milliseconds runtime{0};
};

/**
 * @brief Comparison result for term ordering (KBO, LPO, etc.)
 *
 * Critically distinguishes Equal from Incomparable  conflating these
 * breaks soundness of superposition (incomparable terms are NOT  each other).
 */
enum class CompareResult : int {
    Less         = -1,
    Equal        =  0,
    Greater      =  1,
    Incomparable =  2
};

/**
 * @brief Term ordering for superposition
 */
class TermOrdering {
public:
    virtual ~TermOrdering() = default;
    
    /**
     * @brief Compare two terms under the ordering
     */
    [[nodiscard]] virtual CompareResult compare(const Term* t1, const Term* t2) const = 0;
    
    [[nodiscard]] bool greater(const Term* t1, const Term* t2) const {
        return compare(t1, t2) == CompareResult::Greater;
    }
    
    /**
     * @brief t1  t2 means Greater OR Equal  NOT Incomparable.
     */
    [[nodiscard]] bool greaterOrEqual(const Term* t1, const Term* t2) const {
        auto r = compare(t1, t2);
        return r == CompareResult::Greater || r == CompareResult::Equal;
    }
};

/**
 * @brief Knuth-Bendix Ordering (KBO)
 */
class KnuthBendixOrdering : public TermOrdering {
public:
    KnuthBendixOrdering() {
        // Default weights  use canonical ASCII internal names
        symbolWeights_["phi"] = 3;
        symbolWeights_["J"] = 2;
        symbolWeights_["*"] = 2;     // TermFactory::mul() creates "*"
        symbolWeights_["mul"] = 2;   // Some code also creates "mul"
        symbolWeights_["+"] = 1;     // TermFactory::add() creates "+"
        symbolWeights_["add"] = 1;   // Some code also creates "add"
    }
    
    void setSymbolWeight(const std::string& sym, uint32_t weight) {
        symbolWeights_[sym] = weight;
    }
    
    [[nodiscard]] CompareResult compare(const Term* t1, const Term* t2) const override {
        if (t1->id() == t2->id()) return CompareResult::Equal;
        
        // Variable condition: t1 > t2 requires vars(t2)  vars(t1) (multiset)
        auto vars1 = collectVarCounts(t1);
        auto vars2 = collectVarCounts(t2);
        
        bool vars2SubsetOf1 = true;  // every var in t2 occurs  as often in t1
        bool vars1SubsetOf2 = true;  // every var in t1 occurs  as often in t2
        
        for (auto& [v, cnt] : vars2) {
            auto it = vars1.find(v);
            if (it == vars1.end() || it->second < cnt) {
                vars2SubsetOf1 = false;
                break;
            }
        }
        for (auto& [v, cnt] : vars1) {
            auto it = vars2.find(v);
            if (it == vars2.end() || it->second < cnt) {
                vars1SubsetOf2 = false;
                break;
            }
        }
        
        uint32_t w1 = weight(t1);
        uint32_t w2 = weight(t2);
        
        if (w1 > w2 && vars2SubsetOf1) return CompareResult::Greater;
        if (w2 > w1 && vars1SubsetOf2) return CompareResult::Less;
        if (w1 != w2) return CompareResult::Incomparable;  // different weight but var condition fails
        
        // Same weight: compare by precedence (lexicographic path)
        CompareResult precResult = comparePrecedence(t1, t2);
        
        // Precedence result may indicate greater/less, but we still need
        // the variable condition to hold for soundness
        if (precResult == CompareResult::Greater && vars2SubsetOf1) return CompareResult::Greater;
        if (precResult == CompareResult::Less && vars1SubsetOf2) return CompareResult::Less;
        if (precResult == CompareResult::Equal) return CompareResult::Equal;
        
        return CompareResult::Incomparable;
    }

private:
    std::unordered_map<std::string, uint32_t> symbolWeights_;
    
    /// Collect variable occurrence counts (multiset)
    [[nodiscard]] std::unordered_map<TermId, size_t> collectVarCounts(const Term* t) const {
        std::unordered_map<TermId, size_t> counts;
        collectVarCountsImpl(t, counts);
        return counts;
    }
    
    void collectVarCountsImpl(const Term* t, std::unordered_map<TermId, size_t>& counts) const {
        if (t->kind() == TermKind::Variable) {
            counts[t->id()]++;
        }
        for (const Term* child : t->children()) {
            collectVarCountsImpl(child, counts);
        }
    }
    
    [[nodiscard]] uint32_t weight(const Term* t) const {
        uint32_t w = 1; // Base weight
        
        auto it = symbolWeights_.find(t->symbol());
        if (it != symbolWeights_.end()) {
            w = it->second;
        }
        
        for (const Term* child : t->children()) {
            w += weight(child);
        }
        
        return w;
    }
    
    [[nodiscard]] CompareResult comparePrecedence(const Term* t1, const Term* t2) const {
        // Variables are smallest
        if (t1->kind() == TermKind::Variable && t2->kind() != TermKind::Variable) {
            return CompareResult::Less;
        }
        if (t1->kind() != TermKind::Variable && t2->kind() == TermKind::Variable) {
            return CompareResult::Greater;
        }
        
        // Two distinct variables: incomparable unless identical
        if (t1->kind() == TermKind::Variable && t2->kind() == TermKind::Variable) {
            if (t1->id() == t2->id()) return CompareResult::Equal;
            return CompareResult::Incomparable;
        }
        
        // Compare symbols lexicographically
        if (t1->symbol() < t2->symbol()) return CompareResult::Less;
        if (t1->symbol() > t2->symbol()) return CompareResult::Greater;
        
        // Same symbol: compare children lexicographically
        const auto& c1 = t1->children();
        const auto& c2 = t2->children();
        
        for (size_t i = 0; i < std::min(c1.size(), c2.size()); ++i) {
            CompareResult cmp = comparePrecedence(c1[i], c2[i]);
            if (cmp != CompareResult::Equal) return cmp;
        }
        
        if (c1.size() < c2.size()) return CompareResult::Less;
        if (c1.size() > c2.size()) return CompareResult::Greater;
        
        return CompareResult::Equal;
    }
};

/**
 * @brief Main inference engine implementing superposition calculus
 * 
 * Enhanced with SCOUT signature extraction and phase-aware inference.
 */
class InferenceEngine {
public:
    InferenceEngine(TermFactory& factory, InferenceConfig config = {})
        : factory_(factory)
        , config_(std::move(config))
        , normalizer_(factory)
        , unifier_(factory)
        , sigExtractor_(factory) {
        if (config_.useGODNormalization) {
            normalizer_.initStandardRules();
        }
    }
    
    /**
     * @brief Run saturation loop
     */
    [[nodiscard]] InferenceResult saturate(KnowledgeBase& kb) {
        auto startTime = std::chrono::steady_clock::now();
        stats_ = {};
        contradictionFound_ = false;
        contradictionEqId_ = 0;
        
        // Process clauses in given clause loop
        while (true) {
            // Check limits
            if (stats_.steps >= config_.maxSteps) {
                stats_.runtime = elapsed(startTime);
                return InferenceResult::StepLimit;
            }
            
            if (elapsed(startTime) > config_.timeout) {
                stats_.runtime = elapsed(startTime);
                return InferenceResult::ResourceLimit;
            }
            
            if (kb.hasContradiction()) {
                stats_.runtime = elapsed(startTime);
                return InferenceResult::ProofFound;
            }
            
            // Select next clause
            auto selected = kb.selectNext();
            if (!selected) {
                stats_.runtime = elapsed(startTime);
                return InferenceResult::Saturated;
            }
            
            Equation::Id givenId = *selected;
            const Equation* given = kb.get(givenId);
            if (!given) continue;
            
            ++stats_.steps;
            
            // Heartbeat: print progress every N steps
            if (config_.heartbeatInterval > 0 && 
                (stats_.steps % config_.heartbeatInterval) == 0) {
                auto now = std::chrono::steady_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - startTime).count();
                std::cerr << "[InferenceEngine] step=" << stats_.steps
                          << "  active=" << kb.activeCount()
                          << "  generated=" << stats_.generatedClauses
                          << "  kept=" << stats_.keptClauses
                          << "  time=" << ms << "ms\n";
            }
            
            // Generate inferences with given clause
            auto inferences = generateInferences(kb, *given);
            
            // Check if equality resolution found a contradiction
            if (contradictionFound_) {
                kb.recordContradiction(contradictionEqId_, givenId);
                stats_.runtime = elapsed(startTime);
                return InferenceResult::ProofFound;
            }
            
            // Backward simplify: use given to simplify existing active equations
            if (config_.useDemodulation) {
                kb.backwardSimplify(givenId, factory_, unifier_);
            }
            
            // Process inferences
            for (auto& inf : inferences) {
                processInference(kb, std::move(inf));
            }
        }
    }
    
    /**
     * @brief Extract SCOUT signature from a term
     */
    [[nodiscard]] ScoutSignature extractSignature(const Term* term) {
        return sigExtractor_.extract(term);
    }
    
    /**
     * @brief Check if two terms have compatible signatures for unification
     */
    [[nodiscard]] bool signaturesCompatible(const Term* t1, const Term* t2) {
        if (!config_.useSignatureFiltering) {
            return true; // Skip filtering if disabled
        }
        
        ScoutSignature sig1 = extractSignature(t1);
        ScoutSignature sig2 = extractSignature(t2);
        
        if (sig1.equivalent(sig2)) {
            ++stats_.signatureMatches;
            return true;
        } else {
            ++stats_.signatureMismatches;
            return false;
        }
    }
    
    /**
     * @brief Get statistics from last run
     */
    [[nodiscard]] const InferenceStats& stats() const { return stats_; }
    
    /**
     * @brief Set term ordering
     */
    void setOrdering(std::unique_ptr<TermOrdering> ordering) {
        ordering_ = std::move(ordering);
    }
    
    /**
     * @brief Get the normalizer for external access
     */
    [[nodiscard]] Normalizer& normalizer() { return normalizer_; }

private:
    TermFactory& factory_;
    InferenceConfig config_;
    Normalizer normalizer_;
    Unifier unifier_;
    SignatureExtractor sigExtractor_;
    std::unique_ptr<TermOrdering> ordering_ = std::make_unique<KnuthBendixOrdering>();
    InferenceStats stats_;
    size_t varCounter_ = 0;   ///< Counter for generating fresh variable names (standardize-apart)
    
    // Contradiction tracking
    bool contradictionFound_ = false;
    Equation::Id contradictionEqId_ = 0;
    
    // =========================================================================
    // Variable standardize-apart
    // =========================================================================
    
    /**
     * @brief Collect all variable names in a term
     */
    void collectVarNames(const Term* t, std::unordered_set<std::string>& names) const {
        if (t->kind() == TermKind::Variable) {
            names.insert(t->symbol());
        }
        for (const Term* child : t->children()) {
            collectVarNames(child, names);
        }
    }
    
    /**
     * @brief Rename all variables in a term using a mapping, creating fresh names
     *        for any variable not yet in the map.
     */
    const Term* renameVars(const Term* t,
                           std::unordered_map<std::string, std::string>& renaming) {
        if (t->kind() == TermKind::Variable) {
            auto it = renaming.find(t->symbol());
            if (it == renaming.end()) {
                std::string fresh = "_V" + std::to_string(varCounter_++);
                renaming[t->symbol()] = fresh;
                return factory_.variable(fresh, t->sort());
            }
            return factory_.variable(it->second, t->sort());
        }
        if (t->children().empty()) return t;  // constant or scalar
        
        std::vector<const Term*> newChildren;
        newChildren.reserve(t->children().size());
        bool changed = false;
        for (const Term* child : t->children()) {
            const Term* nc = renameVars(child, renaming);
            newChildren.push_back(nc);
            if (nc != child) changed = true;
        }
        if (!changed) return t;
        
        if (t->isPair() && newChildren.size() == 2) {
            return factory_.pair(newChildren[0], newChildren[1]);
        }
        return factory_.apply(t->symbol(), std::move(newChildren), t->sort());
    }
    
    /**
     * @brief Standardize apart: rename variables in 'from' so they are disjoint
     *        from variables in 'into'.
     *
     * This is REQUIRED before unification in superposition to prevent unsound
     * variable capture. Without it, x in equation 1 and x in equation 2 are
     * treated as the same variable, which is wrong.
     *
     * @param fromL  LHS of 'from' equation (modified in place via return)
     * @param fromR  RHS of 'from' equation (modified in place via return)
     * @param into   The 'into' equation (not modified, used to detect conflicts)
     */
    std::pair<const Term*, const Term*>
    standardizeApart(const Term* fromL, const Term* fromR, const Equation& into) {
        // Collect variable names from 'into'
        std::unordered_set<std::string> intoVars;
        collectVarNames(into.lhs(), intoVars);
        collectVarNames(into.rhs(), intoVars);
        
        // Collect variable names from 'from'
        std::unordered_set<std::string> fromVars;
        collectVarNames(fromL, fromVars);
        collectVarNames(fromR, fromVars);
        
        // Check if there's any overlap
        bool hasOverlap = false;
        for (const auto& v : fromVars) {
            if (intoVars.count(v)) { hasOverlap = true; break; }
        }
        if (!hasOverlap) {
            return {fromL, fromR};  // No renaming needed
        }
        
        // Build renaming for all variables in 'from' (to guarantee disjointness)
        std::unordered_map<std::string, std::string> renaming;
        const Term* newL = renameVars(fromL, renaming);
        const Term* newR = renameVars(fromR, renaming);
        return {newL, newR};
    }
    
    /**
     * @brief Generate all inferences with given clause
     * 
     * Guardrails:
     *   - Skips self-superposition (activeId == givenId)
     *   - SCOUT signature pre-filter on standard superposition
     *   - Inner deadline per given clause
     *   - Cap on total inferences per given clause
     */
    [[nodiscard]] std::vector<std::unique_ptr<Equation>> 
    generateInferences(const KnowledgeBase& kb, const Equation& given) {
        std::vector<std::unique_ptr<Equation>> result;
        
        auto deadline = std::chrono::steady_clock::now() + config_.innerDeadline;
        bool deadlineCut = false;
        
        // Superposition with all active clauses
        // Use forEachActive to avoid copying the entire active vector
        kb.forEachActive([&](Equation::Id activeId, const Equation& active) {
            // Inner deadline check
            if (deadlineCut) return;
            if (std::chrono::steady_clock::now() > deadline) {
                ++stats_.deadlineCuts;
                deadlineCut = true;
                return;
            }
            
            // Cap on inferences from this given clause
            if (result.size() >= config_.maxInferencesPerGiven) {
                deadlineCut = true;
                return;
            }
            
            // Skip self-superposition: produces only trivial results
            if (activeId == given.id()) {
                ++stats_.selfSkips;
                return;
            }
            
            // Standard superposition (with signature pre-filter)
            if (config_.useSignatureFiltering) {
                // Only attempt superposition if top-level signatures are compatible
                bool lhsCompat = signaturesCompatible(active.lhs(), given.lhs()) ||
                                 signaturesCompatible(active.lhs(), given.rhs());
                bool rhsCompat = signaturesCompatible(active.rhs(), given.lhs()) ||
                                 signaturesCompatible(active.rhs(), given.rhs());
                if (lhsCompat || rhsCompat) {
                    superpose(active, given, result);
                    superpose(given, active, result);
                }
            } else {
                superpose(active, given, result);
                superpose(given, active, result);
            }
            
            // Phase-aware superposition if enabled
            if (config_.usePhaseInference && !deadlineCut) {
                phaseSuperpose(active, given, result);
                phaseSuperpose(given, active, result);
            }
        });
        
        // Equality resolution on given
        equalityResolution(given, result);
        
        return result;
    }
    
    /**
     * @brief Replace the subterm at position given by path in the term tree.
     *
     * Given a root term and a path (sequence of child indices), returns
     * a new term identical to the root except at the specified position,
     * where `replacement` appears instead.
     *
     * @param term The root term to reconstruct
     * @param path Sequence of child indices from root to target position
     * @param depth Current depth in the path (start at 0)
     * @param replacement The term to place at the target position
     * @return New interned term with the replacement at the given position
     */
    const Term* replaceAtPath(const Term* term, const std::vector<size_t>& path,
                               size_t depth, const Term* replacement) {
        return core::replaceAtPath(term, path, depth, replacement, factory_);
    }
    
    /**
     * @brief Superposition inference (corrected subterm replacement)
     *
     * Given: l = r (from equation)
     * Into:  s = t (into equation)
     * At subterm s|_p where  = mgu(l, s|_p)
     * Result: s[r]_p = t   (with all side-conditions checked)
     *
     * Previous implementation incorrectly set newLhs = r for non-root 
     * positions, losing the surrounding context s. The fix rebuilds the
     * full top-level term with the replacement at the matched position.
     */
    void superpose(const Equation& from, const Equation& into,
                   std::vector<std::unique_ptr<Equation>>& result) {
        // Standardize apart: rename variables in 'from' to avoid capture
        auto [saL, saR] = standardizeApart(from.lhs(), from.rhs(), into);
        
        std::vector<size_t> path;
        // Try superposing into LHS of 'into'
        superpositionPositions(saL, saR, into.lhs(), into.rhs(),
                               true, result, into.lhs(), path);
        
        // Try superposing into RHS of 'into'
        superpositionPositions(saL, saR, into.rhs(), into.lhs(),
                               false, result, into.rhs(), path);
    }
    
    /**
     * @brief Attempt superposition at all positions in term s
     *
     * Walks the term s top-down, attempting to unify l with each non-variable
     * subterm. When unification succeeds at position path within topS, the
     * result equation is:   topS[r]_path = t
     *
     * @param l     LHS of the 'from' equation (the rewrite rule's LHS)
     * @param r     RHS of the 'from' equation (the rewrite rule's RHS)
     * @param s     Current subterm being examined (subterm of topS at path)
     * @param t     The other side of the 'into' equation
     * @param intoLhs  Whether we're superposing into the LHS of 'into'
     * @param result   Output vector for new equations
     * @param topS     The original top-level term (full LHS/RHS of 'into')
     * @param path     Current position path from topS root to s
     */
    void superpositionPositions(const Term* l, const Term* r,
                                const Term* s, const Term* t,
                                bool intoLhs,
                                std::vector<std::unique_ptr<Equation>>& result,
                                const Term* topS,
                                std::vector<size_t>& path) {
        // Don't superpose into variables (standard side-condition)
        if (s->kind() != TermKind::Variable) {
            auto unifyResult = unifier_.unify(l, s);
            if (unifyResult.success) {
                // Side-condition: l  r in the term ordering
                const Term* lSigma = unifyResult.substitution.apply(l, factory_);
                const Term* rSigma = unifyResult.substitution.apply(r, factory_);
                
                // Check ordering: l must not be smaller than r
                if (ordering_ && ordering_->compare(lSigma, rSigma) == CompareResult::Less) {
                    // Ordering violation: l < r, skip this inference
                } else {
                    // Build the result LHS: topS[r]_path
                    // At root (empty path): the whole term is replaced  r
                    // At non-root: rebuild topS with r at position path, then apply 
                    const Term* newLhs;
                    if (path.empty()) {
                        newLhs = rSigma;
                    } else {
                        // Replace subterm at path with r (uninstantiated),
                        // then apply  to the entire rebuilt term.
                        // This correctly gives: (topS[r]_path) = topS[r]_path  (rest)
                        const Term* replaced = replaceAtPath(topS, path, 0, r);
                        newLhs = unifyResult.substitution.apply(replaced, factory_);
                    }
                    const Term* newRhs = unifyResult.substitution.apply(t, factory_);
                    
                    // Note: normalization is deferred to processInference()
                    // to avoid redundant work on inferences that get discarded
                    // by subsumption or forward simplification.
                    
                    // Don't add trivial equations
                    if (newLhs->id() != newRhs->id()) {
                        auto newEq = std::make_unique<Equation>(
                            newLhs, newRhs, EquationSource::Inference);
                        
                        // Orient the new equation if possible
                        if (ordering_) {
                            auto cmp = ordering_->compare(newLhs, newRhs);
                            if (cmp == CompareResult::Greater) newEq->setOrientation(Orientation::LeftToRight);
                            else if (cmp == CompareResult::Less) newEq->setOrientation(Orientation::RightToLeft);
                        }
                        
                        result.push_back(std::move(newEq));
                        ++stats_.superpositions;
                    }
                }
            }
        }
        
        // Recurse into children of s, extending the position path
        for (size_t i = 0; i < s->children().size(); ++i) {
            path.push_back(i);
            superpositionPositions(l, r, s->children()[i], t, intoLhs, result, topS, path);
            path.pop_back();
        }
    }
    
    /**
     * @brief Equality resolution: s = t with s, t unifiable   (contradiction)
     * 
     * If the equation comes from a negated goal and lhs/rhs unify,
     * we have derived the empty clause (). This is the refutational
     * proof-finding step.
     */
    void equalityResolution(const Equation& eq,
                            std::vector<std::unique_ptr<Equation>>& /*result*/) {
        auto unifyResult = unifier_.unify(eq.lhs(), eq.rhs());
        if (unifyResult.success) {
            ++stats_.equalityResolutions;
            // Record the equation ID that led to contradiction
            // The caller's KB will be notified via contradictionEqId_
            contradictionEqId_ = eq.id();
            contradictionFound_ = true;
        }
    }
    
    /**
     * @brief Phase-aware superposition (corrected: path-based subterm replacement)
     * 
     * Allows superposition when terms differ by a phase factor:
     *   l = r,  s[lC_J()]_p = t
     *   
     *   s[rC_J()]_p = t
     *
     * Uses the same replaceAtPath mechanism as regular superposition to
     * correctly preserve surrounding context at non-root positions.
     */
    void phaseSuperpose(const Equation& from, const Equation& into,
                        std::vector<std::unique_ptr<Equation>>& result) {
        // Standardize apart: rename variables in 'from' to avoid capture
        auto [saL, saR] = standardizeApart(from.lhs(), from.rhs(), into);
        
        // Check if signatures are close enough to attempt phase matching
        ScoutSignature sigFrom = sigExtractor_.extract(saL);
        ScoutSignature sigInto = sigExtractor_.extract(into.lhs());
        
        // If winding numbers and Fibonacci weights match but phases differ,
        // we can create a phase-compensated superposition
        if (sigFrom.windingNumber == sigInto.windingNumber &&
            sigFrom.fibWeight == sigInto.fibWeight) {
            
            uint8_t phaseDiff = (sigInto.phase_mod4 - sigFrom.phase_mod4) & 3;
            
            // Only consider small phase differences (0 or 1 quarter-turn)
            if (phaseDiff <= 1) {
                std::vector<size_t> path;
                // Try direct unification with phase awareness into LHS
                phaseSuperpositionPositions(saL, saR, 
                                             into.lhs(), into.rhs(), 
                                             true, phaseDiff, result,
                                             into.lhs(), path);
                // Also try into RHS
                phaseSuperpositionPositions(saL, saR,
                                             into.rhs(), into.lhs(),
                                             false, phaseDiff, result,
                                             into.rhs(), path);
            }
        }
    }
    
    /**
     * @brief Phase-aware superposition at all positions (corrected)
     *
     * Mirrors the standard superpositionPositions logic: walks the term tree
     * tracking the position path, and uses replaceAtPath for non-root matches
     * so the surrounding context is preserved correctly.
     *
     * @param l     LHS of the 'from' equation
     * @param r     RHS of the 'from' equation
     * @param s     Current subterm being examined
     * @param t     The other side of the 'into' equation
     * @param intoLhs  Whether we're superposing into the LHS
     * @param phaseDiff Phase difference (mod 4)
     * @param result Output vector
     * @param topS   Original top-level target side
     * @param path   Current position path
     */
    void phaseSuperpositionPositions(const Term* l, const Term* r,
                                      const Term* s, const Term* t,
                                      bool intoLhs, uint8_t phaseDiff,
                                      std::vector<std::unique_ptr<Equation>>& result,
                                      const Term* topS,
                                      std::vector<size_t>& path) {
        // Phase-aware unification attempt (don't superpose into variables)
        if (s->kind() != TermKind::Variable && signaturesCompatible(l, s)) {
            auto unifyResult = unifier_.unify(l, s);
            if (unifyResult.success) {
                // Build the result using replaceAtPath (same as corrected superposition)
                const Term* rSigma = unifyResult.substitution.apply(r, factory_);
                
                const Term* newLhs;
                if (path.empty()) {
                    // Root position: entire target side is replaced
                    newLhs = rSigma;
                } else {
                    // Non-root: replace subterm at path with r, then apply sigma
                    const Term* replaced = replaceAtPath(topS, path, 0, r);
                    newLhs = unifyResult.substitution.apply(replaced, factory_);
                }
                const Term* newRhs = unifyResult.substitution.apply(t, factory_);
                
                // Note: normalization is deferred to processInference()
                // to avoid redundant work on inferences that get discarded.
                
                if (newLhs->id() != newRhs->id()) {
                    result.push_back(std::make_unique<Equation>(
                        newLhs, newRhs, EquationSource::Inference));
                    ++stats_.phaseSuperpositions;
                }
            }
        }
        
        // Recurse into children, maintaining position path
        for (size_t i = 0; i < s->children().size(); ++i) {
            path.push_back(i);
            phaseSuperpositionPositions(l, r, s->children()[i], t, intoLhs, 
                                        phaseDiff, result, topS, path);
            path.pop_back();
        }
    }
    
    /**
     * @brief Process a newly generated inference
     * 
     * Applies forward simplification and subsumption checks before
     * adding to the knowledge base.
     */
    void processInference(KnowledgeBase& kb, std::unique_ptr<Equation> eq) {
        ++stats_.generatedClauses;
        
        // Trivial check
        if (eq->isTrivial()) {
            return;
        }
        
        // GOD normalization: canonicalize LHS/RHS before simplification
        if (config_.useGODNormalization) {
            const Term* nL = normalizer_.normalize(eq->lhs());
            const Term* nR = normalizer_.normalize(eq->rhs());
            if (nL->id() == nR->id()) return; // normalized to trivial
            if (nL != eq->lhs() || nR != eq->rhs()) {
                eq = std::make_unique<Equation>(nL, nR, eq->source());
            }
        }
        
        // Forward simplification: rewrite eq using existing demodulators
        if (config_.useDemodulation) {
            if (kb.forwardSimplify(*eq, factory_, unifier_)) {
                const Term* simpL = kb.lastSimplifiedLhs();
                const Term* simpR = kb.lastSimplifiedRhs();
                if (simpL->id() == simpR->id()) {
                    // Simplified to trivial  discard
                    ++stats_.deletedByDemodulation;
                    return;
                }
                // Rebuild equation with simplified terms
                eq = std::make_unique<Equation>(simpL, simpR, eq->source());
                ++stats_.deletedByDemodulation; // counts simplification events
            }
        }
        
        // Subsumption check
        if (config_.useSubsumption && kb.isSubsumed(*eq)) {
            ++stats_.deletedBySubsumption;
            return;
        }
        
        // Orient the equation if possible
        if (ordering_) {
            auto cmp = ordering_->compare(eq->lhs(), eq->rhs());
            if (cmp == CompareResult::Greater) eq->setOrientation(Orientation::LeftToRight);
            else if (cmp == CompareResult::Less) eq->setOrientation(Orientation::RightToLeft);
        }
        
        // Add to knowledge base
        kb.addDerived(eq->lhs(), eq->rhs(), eq->source());
        ++stats_.keptClauses;
    }
    
    [[nodiscard]] std::chrono::milliseconds 
    elapsed(std::chrono::steady_clock::time_point start) const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
    }
};

// =============================================================================
// DISCOVERY ENGINE
// =============================================================================

/**
 * @brief Discovery engine for finding new equations
 * 
 * Implements the full autodiscovery loop:
 *   1. Generate candidate equations (SCOUT signature clustering)
 *   2. Canonicalize via GOD normalization
 *   3. Deduplicate by canonical form
 *   4. Semantic falsification (evaluate at probe points)
 *   5. Prove surviving candidates via saturation
 *   6. Certify proven results (proof checker)
 */
class DiscoveryEngine {
public:
    /**
     * @brief Discovery result for a single equation
     */
    struct DiscoveryResult {
        const Term* lhs;
        const Term* rhs;
        bool proven;           // True if proven by saturation
        bool falsified;        // True if falsified by evaluation
        std::string proofInfo; // Proof summary string
    };
    
    explicit DiscoveryEngine(TermFactory& factory, InferenceConfig config = {})
        : factory_(factory)
        , inferenceEngine_(factory, std::move(config))
        , sigExtractor_(factory)
        , normalizer_(factory) {
        normalizer_.initStandardRules();
    }
    
    /**
     * @brief Full discovery loop: generate, filter, prove, certify
     * 
     * @param kb Knowledge base with axioms
     * @param seedTerms Terms to use for conjecture generation
     * @param maxDiscoveries Maximum new equations to find
     * @return Vector of discovery results
     */
    [[nodiscard]] std::vector<DiscoveryResult>
    discoverFull(KnowledgeBase& kb, const std::vector<const Term*>& seedTerms,
                 size_t maxDiscoveries = 100) {
        std::vector<DiscoveryResult> results;
        
        // Phase 1: Generate candidate conjectures via signature clustering
        auto candidates = generateConjectures(seedTerms);
        
        // Phase 2: Canonicalize both sides
        std::vector<std::pair<const Term*, const Term*>> canonical;
        for (auto& [lhs, rhs] : candidates) {
            const Term* normL = normalizer_.normalize(lhs);
            const Term* normR = normalizer_.normalize(rhs);
            if (normL->id() != normR->id()) {
                canonical.push_back({normL, normR});
            }
            // If normL == normR, the equation is trivially true  skip
        }
        
        // Phase 3: Deduplicate by canonical form
        // CRITICAL: Use encode() (canonical bytecode) for dedup keys, NOT
        // Term IDs.  Term IDs are assigned by the TermFactory and depend on
        // interning order, which is non-deterministic.  encode() produces a
        // canonical string that is the same regardless of which factory
        // interned the term.
        std::unordered_set<size_t> seen;
        std::vector<std::pair<const Term*, const Term*>> deduped;
        for (auto& [lhs, rhs] : canonical) {
            // Canonical bytecodes for collision-free dedup
            auto lhsEnc = lhs->encode();
            auto rhsEnc = rhs->encode();
            // Order the pair canonically (smaller encoding first)
            if (lhsEnc > rhsEnc) std::swap(lhsEnc, rhsEnc);
            // Hash the canonical pair
            size_t h1 = std::hash<std::string>{}(lhsEnc);
            size_t h2 = std::hash<std::string>{}(rhsEnc);
            size_t key = h1 ^ (h2 * 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
            if (seen.insert(key).second) {
                deduped.push_back({lhs, rhs});
            }
        }
        
        // Phase 4: Semantic falsification  evaluate at probe points
        // Use the constant  and a few concrete values to falsify candidates
        std::vector<std::pair<const Term*, const Term*>> surviving;
        for (auto& [lhs, rhs] : deduped) {
            if (!semanticFalsify(lhs, rhs)) {
                surviving.push_back({lhs, rhs}); // Not falsified
            }
        }
        
        // Phase 5: Attempt proof by saturation
        for (auto& [lhs, rhs] : surviving) {
            if (results.size() >= maxDiscoveries) break;
            
            // Create a fresh KB with the base axioms + the candidate as a goal
            KnowledgeBase proofKB(factory_);
            
            // Copy axioms from the original KB
            for (auto id : kb.activeEquations()) {
                const Equation* eq = kb.get(id);
                if (eq && eq->source() == EquationSource::Axiom) {
                    proofKB.addAxiom(eq->lhs(), eq->rhs());
                }
            }
            
            // Add the candidate as a goal (negated for refutation)
            proofKB.addGoal(lhs, rhs);
            
            // Run saturation with reduced limits for each candidate
            InferenceConfig proofConfig;
            proofConfig.maxSteps = 1000;
            proofConfig.timeout = std::chrono::seconds{5};
            proofConfig.useGODNormalization = true;
            proofConfig.useDemodulation = true;
            proofConfig.useSubsumption = true;
            
            InferenceEngine prover(factory_, proofConfig);
            InferenceResult proofResult = prover.saturate(proofKB);
            
            DiscoveryResult dr;
            dr.lhs = lhs;
            dr.rhs = rhs;
            dr.falsified = false;
            
            if (proofResult == InferenceResult::ProofFound) {
                dr.proven = true;
                dr.proofInfo = "Proven by refutational saturation (" + 
                    std::to_string(prover.stats().steps) + " steps)";
            } else if (proofResult == InferenceResult::Saturated) {
                dr.proven = false;
                dr.proofInfo = "Saturated (equational theory complete, conjecture independent)";
            } else {
                dr.proven = false;
                dr.proofInfo = "Inconclusive (resource limit)";
            }
            
            results.push_back(std::move(dr));
        }
        
        return results;
    }
    
    /**
     * @brief Simplified discovery: just run saturation and collect
     * 
     * @param kb Knowledge base with axioms
     * @param maxDiscoveries Maximum new equations to find
     * @return Vector of discovered equations
     */
    [[nodiscard]] std::vector<std::pair<const Term*, const Term*>>
    discover(KnowledgeBase& kb, size_t maxDiscoveries = 100) {
        std::vector<std::pair<const Term*, const Term*>> discoveries;
        
        // Run saturation
        (void)inferenceEngine_.saturate(kb);
        
        // Collect non-trivial derived equations
        for (auto id : kb.activeEquations()) {
            const Equation* eq = kb.get(id);
            if (eq && eq->source() == EquationSource::Inference) {
                discoveries.push_back({eq->lhs(), eq->rhs()});
                
                if (discoveries.size() >= maxDiscoveries) {
                    break;
                }
            }
        }
        
        return discoveries;
    }
    
    /**
     * @brief Generate conjectures based on signature patterns
     */
    [[nodiscard]] std::vector<std::pair<const Term*, const Term*>>
    generateConjectures(const std::vector<const Term*>& terms) {
        std::vector<std::pair<const Term*, const Term*>> conjectures;
        
        // Group terms by signature
        std::unordered_map<size_t, std::vector<const Term*>> bySignature;
        for (const Term* t : terms) {
            ScoutSignature sig = sigExtractor_.extract(t);
            bySignature[sig.hash()].push_back(t);
        }
        
        // Cap: maximum bucket size for pairwise comparison
        constexpr size_t MAX_BUCKET_SIZE = 64;
        // Cap: maximum total conjectures generated
        constexpr size_t MAX_CONJECTURES = 10000;
        
        // Precompute signatures once per term (avoid re-extraction in inner loop)
        std::unordered_map<TermId, ScoutSignature> sigCache;
        for (const auto& [hash, group] : bySignature) {
            for (const Term* t : group) {
                if (!sigCache.count(t->id())) {
                    sigCache[t->id()] = sigExtractor_.extract(t);
                }
            }
        }
        
        // Terms with same signature might be equal
        for (auto& [hash, group] : bySignature) {
            // If bucket is too large, truncate to avoid O(n^2) blowup
            size_t limit = std::min(group.size(), MAX_BUCKET_SIZE);
            
            for (size_t i = 0; i < limit; ++i) {
                for (size_t j = i + 1; j < limit; ++j) {
                    const auto& sig1 = sigCache[group[i]->id()];
                    const auto& sig2 = sigCache[group[j]->id()];
                    if (sig1.equivalent(sig2)) {
                        conjectures.push_back({group[i], group[j]});
                        if (conjectures.size() >= MAX_CONJECTURES) {
                            return conjectures;
                        }
                    }
                }
            }
        }
        
        return conjectures;
    }
    
    /**
     * @brief Get the underlying inference engine
     */
    [[nodiscard]] InferenceEngine& inferenceEngine() { return inferenceEngine_; }

private:
    TermFactory& factory_;
    InferenceEngine inferenceEngine_;
    SignatureExtractor sigExtractor_;
    Normalizer normalizer_;
    
    /**
     * @brief Attempt to semantically falsify an equation
     * 
     * Evaluates both sides at several probe families (golden, Fibonacci,
     * transcendental). If any probe point produces different values on
     * lhs vs rhs, the equation is falsified as a conjecture.
     * 
     * @return true if the equation is falsified (i.e., NOT a valid identity)
     */
    bool semanticFalsify(const Term* lhs, const Term* rhs) {
        // Identical terms (hash-consed pointer equality) are never falsified
        if (lhs->id() == rhs->id()) return false;
        
        // Collect all variable names in lhs and rhs
        std::vector<std::string> vars;
        collectVariables(lhs, vars);
        collectVariables(rhs, vars);
        // Remove duplicates
        std::sort(vars.begin(), vars.end());
        vars.erase(std::unique(vars.begin(), vars.end()), vars.end());
        
        // If there are no variables, both sides are ground;
        // use direct numeric evaluation at a single "empty" probe.
        // If there are variables, evaluate at multiple diverse probe points.
        
        fingerprint::TermEvaluator evaluator;
        
        // Build probe families
        std::vector<fingerprint::ProbeFamily> families;
        if (vars.empty()) {
            // Single empty probe for ground terms
            fingerprint::ProbeFamily fam("ground");
            fam.addProbe(fingerprint::ProbePoint("empty"));
            families.push_back(std::move(fam));
        } else {
            families.push_back(fingerprint::ProbeFamily::goldenFamily(vars));
            families.push_back(fingerprint::ProbeFamily::fibonacciFamily(vars));
            families.push_back(fingerprint::ProbeFamily::transcendentalFamily(vars));
        }
        
        constexpr double FALSIFICATION_TOLERANCE = 1e-8;
        
        for (const auto& family : families) {
            for (const auto& probe : family.probes()) {
                auto valLhs = evaluator.evaluate(*lhs, probe);
                auto valRhs = evaluator.evaluate(*rhs, probe);
                
                double distance = valLhs.distanceTo(valRhs);
                
                // If distance is large relative to the values, falsified
                if (distance > FALSIFICATION_TOLERANCE) {
                    // Double-check: large values might have large absolute
                    // differences that are relatively small. Use relative
                    // tolerance for large magnitudes.
                    double scale = std::max(valLhs.norm(), valRhs.norm());
                    if (scale > 1.0 && distance / scale < FALSIFICATION_TOLERANCE) {
                        continue;  // Relatively close  don't falsify
                    }
                    return true;  // Falsified!
                }
            }
        }
        
        return false;  // Survived all probes  not falsified
    }
    
    /**
     * @brief Collect variable names from a term
     */
    static void collectVariables(const Term* t, std::vector<std::string>& vars) {
        if (t->isVariable()) {
            vars.push_back(t->symbol());
            return;
        }
        for (const Term* c : t->children()) {
            collectVariables(c, vars);
        }
    }
    
    /**
     * @brief Check if a term is ground (no variables)
     */
    static bool isGround(const Term* t) {
        if (t->isVariable()) return false;
        for (const Term* c : t->children()) {
            if (!isGround(c)) return false;
        }
        return true;
    }
};

} // namespace logic
} // namespace autodiscover

#endif // AUTODISCOVER_LOGIC_INFERENCEENGINE_HPP
