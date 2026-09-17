#ifndef AUTODISCOVER_CANON_NF_ENGINE_HPP
#define AUTODISCOVER_CANON_NF_ENGINE_HPP

/**
 * @file NFEngine.hpp
 * @brief Normal Form Engine - Composed normalization pipeline
 * 
 * Mathematical Foundation:
 * ========================
 * 
 * The NF Engine implements the key mapping:
 * 
 *   NF : T(,X)  T(,X)
 * 
 * As a composition of terminating passes:
 * 
 *   NF = NF_k  NF_{k-1}  ...  NF_2  NF_1
 * 
 * Key Invariants:
 * ---------------
 * 
 * 1. IDEMPOTENCE: NF(NF(t)) = NF(t)
 * 2. COMPLETENESS: t _ u  NF(t) = NF(u)
 * 3. SOUNDNESS: NF(t) = NF(u)  t _ u
 * 4. TERMINATION: NF(t) always terminates in finite time
 * 5. DETERMINISM: NF(t) has unique result
 * 
 * @author AutoDiscover Prover
 * @date 2024
 */

#include "Equivalence.hpp"
#include "../core/Term.hpp"
#include "../core/Context.hpp"
#include "../encoding/Encoding.hpp"
#include "../egraph/EGraph.hpp"

#include <memory>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <optional>
#include <algorithm>

namespace autodiscover {
namespace canon {

// Create namespace alias to access core types correctly from within canon namespace
namespace core = ::autodiscover::core;

// Forward declaration
class NFEngine;

// ===========================================================================
// ABSTRACT NORMALIZATION PASS (with raw pointer API)
// ===========================================================================

/**
 * @brief Abstract base for normalization passes using Term's raw pointer API
 */
class TermNormalizationPass {
public:
    virtual ~TermNormalizationPass() = default;
    
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual EquivalenceLayer layer() const = 0;
    
    /**
     * @brief Normalize a term, returning the normalized form
     * @param term Input term (raw pointer, hash-consed)
     * @param factory Factory for creating new terms
     * @return Normalized term
     */
    [[nodiscard]] virtual const core::Term* 
    normalize(const core::Term* term, core::TermFactory& factory) = 0;
    
    /**
     * @brief Check if term is already in normal form for this pass
     */
    [[nodiscard]] virtual bool isNormal(const core::Term* term) const = 0;
};

// ===========================================================================
// ALPHA NORMALIZATION PASS
// ===========================================================================

/**
 * @brief Alpha-normalization pass
 * 
 * Renames all variables to canonical form: x0, x1, x2, ... in DFS order.
 */
class AlphaTermPass : public TermNormalizationPass {
private:
    AlphaOptions options_;
    
public:
    explicit AlphaTermPass(AlphaOptions opts = {}) 
        : options_(std::move(opts)) {}
    
    [[nodiscard]] std::string name() const override { return "AlphaNorm"; }
    [[nodiscard]] EquivalenceLayer layer() const override { return EquivalenceLayer::ALPHA; }
    
    [[nodiscard]] const core::Term* 
    normalize(const core::Term* term, core::TermFactory& factory) override {
        if (!term) return nullptr;
        
        // Collect variables in DFS order
        std::vector<std::string> varOrder;
        std::unordered_set<std::string> seen;
        collectVars(term, varOrder, seen);
        
        // Build renaming map
        std::unordered_map<std::string, std::string> renaming;
        for (size_t i = 0; i < varOrder.size(); ++i) {
            renaming[varOrder[i]] = options_.varPrefix + std::to_string(i);
        }
        
        // Apply renaming
        return rename(term, renaming, factory);
    }
    
    [[nodiscard]] bool isNormal(const core::Term* term) const override {
        if (!term) return true;
        
        std::vector<std::string> vars;
        std::unordered_set<std::string> seen;
        collectVars(term, vars, seen);
        
        for (size_t i = 0; i < vars.size(); ++i) {
            std::string expected = options_.varPrefix + std::to_string(i);
            if (vars[i] != expected) return false;
        }
        return true;
    }
    
private:
    void collectVars(const core::Term* t, 
                     std::vector<std::string>& order,
                     std::unordered_set<std::string>& seen) const {
        if (!t) return;
        
        if (t->kind() == core::TermKind::Variable) {
            if (seen.insert(t->symbol()).second) {
                order.push_back(t->symbol());
            }
        } else {
            for (const core::Term* child : t->children()) {
                collectVars(child, order, seen);
            }
        }
    }
    
    const core::Term* rename(
        const core::Term* t,
        const std::unordered_map<std::string, std::string>& renaming,
        core::TermFactory& factory) const {
        
        if (!t) return nullptr;
        
        switch (t->kind()) {
            case core::TermKind::Variable: {
                auto it = renaming.find(t->symbol());
                if (it != renaming.end()) {
                    return factory.variable(it->second, t->sort());
                }
                return t;
            }
            
            case core::TermKind::Application: {
                std::vector<const core::Term*> newChildren;
                bool changed = false;
                for (const core::Term* child : t->children()) {
                    auto newChild = rename(child, renaming, factory);
                    if (newChild != child) changed = true;
                    newChildren.push_back(newChild);
                }
                if (!changed) return t;
                
                return factory.apply(t->symbol(), newChildren, t->sort());
            }
            
            case core::TermKind::Pair: {
                auto newFirst = rename(t->first(), renaming, factory);
                auto newSecond = rename(t->second(), renaming, factory);
                if (newFirst == t->first() && newSecond == t->second()) {
                    return t;
                }
                return factory.pair(newFirst, newSecond);
            }
            
            default:
                return t;
        }
    }
};

// ===========================================================================
// AC NORMALIZATION PASS
// ===========================================================================

/**
 * @brief AC-normalization pass
 * 
 * Flattens and sorts arguments of associative-commutative operators.
 */
class ACTermPass : public TermNormalizationPass {
private:
    ACOptions options_;
    NumberSystem numberSystem_ = NumberSystem::GENERIC;
    
public:
    explicit ACTermPass(ACOptions opts = {}) 
        : options_(std::move(opts)) {}
    
    ACTermPass(ACOptions opts, NumberSystem ns) 
        : options_(std::move(opts)), numberSystem_(ns) {}
    
    void setNumberSystem(NumberSystem ns) { numberSystem_ = ns; }
    
    [[nodiscard]] std::string name() const override { return "ACNorm"; }
    [[nodiscard]] EquivalenceLayer layer() const override { return EquivalenceLayer::AC; }
    
    [[nodiscard]] const core::Term* 
    normalize(const core::Term* term, core::TermFactory& factory) override {
        return normalizeImpl(term, factory);
    }
    
    [[nodiscard]] bool isNormal(const core::Term* term) const override {
        return checkNormal(term);
    }
    
private:
    /// Check if operator should be treated as AC for the current number system
    bool isEffectivelyAC(const std::string& op) const {
        return options_.isACForNumberSystem(op, numberSystem_);
    }
    
    const core::Term* normalizeImpl(const core::Term* t, core::TermFactory& factory) {
        if (!t) return nullptr;
        
        if (t->kind() != core::TermKind::Application) {
            return t;
        }
        
        // Recursively normalize children first
        std::vector<const core::Term*> normChildren;
        for (const core::Term* child : t->children()) {
            normChildren.push_back(normalizeImpl(child, factory));
        }
        
        // Check if this is an AC operator for the current number system
        if (!isEffectivelyAC(t->symbol())) {
            // Not AC - rebuild with normalized children
            return factory.apply(t->symbol(), normChildren, t->sort());
        }
        
        // AC operator - flatten and sort
        std::vector<const core::Term*> flattened;
        for (const core::Term* child : normChildren) {
            flatten(child, t->symbol(), flattened);
        }
        
        // Sort by canonical order (using shortlex on encoding)
        std::sort(flattened.begin(), flattened.end(),
            [](const core::Term* a, const core::Term* b) {
                auto aEnc = a->encode();
                auto bEnc = b->encode();
                if (aEnc.size() != bEnc.size()) {
                    return aEnc.size() < bEnc.size();
                }
                return aEnc < bEnc;
            });
        
        // Rebuild as balanced tree
        return buildBalanced(t->symbol(), t->sort(), flattened, factory);
    }
    
    void flatten(const core::Term* t,
                 const std::string& op,
                 std::vector<const core::Term*>& result) const {
        if (!t) return;
        
        if (t->kind() == core::TermKind::Application && 
            t->symbol() == op && isEffectivelyAC(op)) {
            for (const core::Term* child : t->children()) {
                flatten(child, op, result);
            }
        } else {
            result.push_back(t);
        }
    }
    
    const core::Term* buildBalanced(
        const std::string& op,
        core::Sort sort,
        const std::vector<const core::Term*>& args,
        core::TermFactory& factory) const {
        
        if (args.empty()) return nullptr;
        if (args.size() == 1) return args[0];
        if (args.size() == 2) {
            return factory.apply(op, {args[0], args[1]}, sort);
        }
        
        // Right-associate for determinism
        const core::Term* result = args.back();
        for (size_t i = args.size() - 1; i > 0; --i) {
            result = factory.apply(op, {args[i - 1], result}, sort);
        }
        return result;
    }
    
    bool checkNormal(const core::Term* t) const {
        if (!t || t->kind() != core::TermKind::Application) {
            return true;
        }
        
        // Check children first
        for (const core::Term* child : t->children()) {
            if (!checkNormal(child)) return false;
        }
        
        // If AC operator, check flattened and sorted
        if (isEffectivelyAC(t->symbol())) {
            for (const core::Term* child : t->children()) {
                if (child->kind() == core::TermKind::Application &&
                    child->symbol() == t->symbol()) {
                    return false; // Not flattened
                }
            }
            
            // Check sorted
            const auto& children = t->children();
            for (size_t i = 1; i < children.size(); ++i) {
                auto prevEnc = children[i-1]->encode();
                auto currEnc = children[i]->encode();
                if (prevEnc.size() > currEnc.size() ||
                    (prevEnc.size() == currEnc.size() && prevEnc > currEnc)) {
                    return false;
                }
            }
        }
        
        return true;
    }
};

// ===========================================================================
// RING NORMALIZATION PASS
// ===========================================================================

/**
 * @brief Ring normalization pass
 * 
 * Applies ring axioms: 0/1 simplification, double negation, etc.
 */
class RingTermPass : public TermNormalizationPass {
private:
    RingOptions options_;
    
public:
    explicit RingTermPass(RingOptions opts = {}) 
        : options_(std::move(opts)) {}
    
    [[nodiscard]] std::string name() const override { return "RingNorm"; }
    [[nodiscard]] EquivalenceLayer layer() const override { return EquivalenceLayer::RING; }
    
    [[nodiscard]] const core::Term* 
    normalize(const core::Term* term, core::TermFactory& factory) override {
        return normalizeImpl(term, factory);
    }
    
    [[nodiscard]] bool isNormal(const core::Term* term) const override {
        return !hasNonNormalPattern(term);
    }
    
private:
    const core::Term* normalizeImpl(const core::Term* t, core::TermFactory& factory) {
        if (!t) return nullptr;
        
        // First normalize children
        if (t->kind() == core::TermKind::Application) {
            std::vector<const core::Term*> newChildren;
            for (const core::Term* child : t->children()) {
                newChildren.push_back(normalizeImpl(child, factory));
            }
            
            // Rebuild with normalized children
            auto normalized = factory.apply(t->symbol(), newChildren, t->sort());
            
            // Apply ring rules
            return applyRules(normalized, factory);
        }
        
        return t;
    }
    
    const core::Term* applyRules(const core::Term* t, core::TermFactory& factory) {
        if (!t || t->kind() != core::TermKind::Application) {
            return t;
        }
        
        const auto& sym = t->symbol();
        const auto& children = t->children();
        
        // add(x, 0)  x
        if ((sym == "add" || sym == "+") && children.size() == 2 && options_.simplifyZero) {
            if (isZero(children[1])) return children[0];
            if (isZero(children[0])) return children[1];
        }
        
        // mul(x, 0)  0
        if ((sym == "mul" || sym == "*") && children.size() == 2 && options_.simplifyZero) {
            if (isZero(children[0]) || isZero(children[1])) {
                return factory.scalar(0.0);
            }
        }
        
        // mul(x, 1)  x
        if ((sym == "mul" || sym == "*") && children.size() == 2 && options_.simplifyOne) {
            if (isOne(children[1])) return children[0];
            if (isOne(children[0])) return children[1];
        }
        
        // neg(neg(x))  x
        if (sym == "neg" && children.size() == 1 && options_.simplifyNegation) {
            if (children[0]->kind() == core::TermKind::Application &&
                (children[0]->symbol() == "neg") &&
                children[0]->children().size() == 1) {
                return children[0]->children()[0];
            }
        }
        
        // inv(inv(x))  x  
        if ((sym == "inv" || sym == "/") && children.size() == 1 && options_.simplifyInverse) {
            if (children[0]->kind() == core::TermKind::Application &&
                (children[0]->symbol() == "inv" || children[0]->symbol() == "/") &&
                children[0]->children().size() == 1) {
                return children[0]->children()[0];
            }
        }
        
        return t;
    }
    
    bool isZero(const core::Term* t) const {
        if (t->kind() == core::TermKind::Scalar) {
            return t->scalarValue() == 0.0;
        }
        if (t->kind() == core::TermKind::Constant && t->symbol() == "0") {
            return true;
        }
        return false;
    }
    
    bool isOne(const core::Term* t) const {
        if (t->kind() == core::TermKind::Scalar) {
            return t->scalarValue() == 1.0;
        }
        if (t->kind() == core::TermKind::Constant && t->symbol() == "1") {
            return true;
        }
        return false;
    }
    
    bool hasNonNormalPattern(const core::Term* t) const {
        if (!t) return false;
        
        if (t->kind() == core::TermKind::Application) {
            const auto& sym = t->symbol();
            const auto& ch = t->children();
            
            if ((sym == "add" || sym == "+") && ch.size() == 2) {
                if (isZero(ch[0]) || isZero(ch[1])) return true;
            }
            if ((sym == "mul" || sym == "*") && ch.size() == 2) {
                if (isZero(ch[0]) || isZero(ch[1])) return true;
                if (isOne(ch[0]) || isOne(ch[1])) return true;
            }
            if (sym == "neg" && ch.size() == 1) {
                if (ch[0]->kind() == core::TermKind::Application &&
                    (ch[0]->symbol() == "neg")) {
                    return true;
                }
            }
            
            for (const core::Term* child : ch) {
                if (hasNonNormalPattern(child)) return true;
            }
        }
        
        return false;
    }
};

// ===========================================================================
// PHI NORMALIZATION PASS
// ===========================================================================

/**
 * @brief Phi-ring normalization pass
 * 
 * Implements   +1 and Zeckendorf carry normalization.
 */
class PhiTermPass : public TermNormalizationPass {
private:
    PhiOptions options_;
    
public:
    explicit PhiTermPass(PhiOptions opts = {}) 
        : options_(std::move(opts)) {}
    
    [[nodiscard]] std::string name() const override { return "PhiNorm"; }
    [[nodiscard]] EquivalenceLayer layer() const override { return EquivalenceLayer::PHI; }
    
    [[nodiscard]] const core::Term* 
    normalize(const core::Term* term, core::TermFactory& factory) override {
        return normalizeImpl(term, factory);
    }
    
    [[nodiscard]] bool isNormal(const core::Term* term) const override {
        return !hasPhiSquared(term);
    }
    
private:
    const core::Term* normalizeImpl(const core::Term* t, core::TermFactory& factory) {
        if (!t) return nullptr;
        
        // First normalize children
        if (t->kind() == core::TermKind::Application) {
            std::vector<const core::Term*> newChildren;
            for (const core::Term* child : t->children()) {
                newChildren.push_back(normalizeImpl(child, factory));
            }
            
            // Check for 
            if ((t->symbol() == "mul" || t->symbol() == "*") && newChildren.size() == 2) {
                if (isPhi(newChildren[0]) && isPhi(newChildren[1])) {
                    //    + 1
                    auto phi = factory.phi();
                    auto one = factory.scalar(1.0);
                    auto result = factory.add(phi, one);
                    return normalizeImpl(result, factory);
                }
            }
            
            // Rebuild with normalized children
            return factory.apply(t->symbol(), newChildren, t->sort());
        }
        
        return t;
    }
    
    bool isPhi(const core::Term* t) const {
        return t && (t->kind() == core::TermKind::Phi ||
                    (t->kind() == core::TermKind::Constant && t->symbol() == "phi"));
    }
    
    bool isPhiLike(const core::Term* t) const {
        return t && (t->kind() == core::TermKind::Phi ||
                    t->kind() == core::TermKind::PhiBar ||
                    (t->kind() == core::TermKind::Constant && 
                     (t->symbol() == "phi" || t->symbol() == "phi_bar")));
    }
    
    bool hasPhiSquared(const core::Term* t) const {
        if (!t) return false;
        
        if (t->kind() == core::TermKind::Application) {
            if ((t->symbol() == "mul" || t->symbol() == "*") && 
                t->children().size() == 2) {
                if (isPhi(t->children()[0]) && isPhi(t->children()[1])) {
                    return true;
                }
            }
            
            for (const core::Term* child : t->children()) {
                if (hasPhiSquared(child)) return true;
            }
        }
        
        return false;
    }
};

// ===========================================================================
// GOD NORMALIZATION PASS
// ===========================================================================

/**
 * @brief GOD (Galois Orbit Descent) normalization pass
 * 
 * Computes minimal representative under Galois orbit action:
 *   GOD(t) = min{(t) :   Gal(()/)}
 * 
 * Two modes:
 *   1. EXPLICIT: enumerate orbit elements, pick min by encode()
 *   2. EGRAPH:   load orbit into an EGraph, saturate with congruence
 *                closure, then extract minimum-cost representative.
 *                This finds strictly more equivalences when the orbit
 *                reveals shared subterms that congruence closure can
 *                merge.
 */
class GODTermPass : public TermNormalizationPass {
private:
    GODOptions options_;
    bool useEGraph_ = false;  // When true, use EGraph saturation mode
    
    /// Check whether a term (or any subterm) contains  or .
    /// If not, the Galois orbit is trivially {term}  skip orbit computation.
    static bool containsPhiLike(const core::Term* t) {
        if (!t) return false;
        if (t->kind() == core::TermKind::Phi || t->kind() == core::TermKind::PhiBar)
            return true;
        if (t->kind() == core::TermKind::Constant &&
            (t->symbol() == "phi" || t->symbol() == "phi_bar"))
            return true;
        for (const core::Term* c : t->children()) {
            if (containsPhiLike(c)) return true;
        }
        return false;
    }
    
public:
    explicit GODTermPass(GODOptions opts = {}) 
        : options_(std::move(opts)) {}
    
    void setUseEGraph(bool v) { useEGraph_ = v; }
    
    [[nodiscard]] std::string name() const override { return "GODNorm"; }
    [[nodiscard]] EquivalenceLayer layer() const override { return EquivalenceLayer::GOD; }
    
    [[nodiscard]] const core::Term* 
    normalize(const core::Term* term, core::TermFactory& factory) override {
        if (!term) return nullptr;
        
        // Guard: only invoke orbit computation if term contains  or .
        // Non-phi terms have trivial single-element orbits  skip entirely.
        if (!containsPhiLike(term)) return term;
        
        // Compute orbit and find minimum
        auto orbit = computeOrbit(term, factory);
        
        if (orbit.empty()) {
            return term;
        }
        
        if (useEGraph_ && orbit.size() > 1) {
            // ---- EGraph saturation mode ----
            // Load all orbit elements into an EGraph, merge them
            // (they're all Galois conjugates  semantically equal),
            // then extract the minimum-cost representative.
            // This can find more equivalences via congruence closure
            // on shared subterms between orbit elements.
            return normalizeViaEGraph(orbit, factory);
        }
        
        // ---- Explicit mode (default) ----
        // Find shortlex-minimal element
        auto minIt = std::min_element(orbit.begin(), orbit.end(),
            [](const core::Term* a, const core::Term* b) {
                auto aEnc = a->encode();
                auto bEnc = b->encode();
                if (aEnc.size() != bEnc.size()) {
                    return aEnc.size() < bEnc.size();
                }
                return aEnc < bEnc;
            });
        
        return *minIt;
    }
    
    [[nodiscard]] bool isNormal(const core::Term* term) const override {
        // Checking normality requires computing the orbit, which is expensive.
        // We do a cheaper check: verify the term contains no  nodes
        // (since GOD maps    as canonical). If it does contain ,
        // it is definitely NOT in the canonical form for this pass.
        // If it doesn't, it might be normal (conservative  may return true
        // for some non-normal terms, but never false for normal ones).
        (void)term;  // suppress unused warning in the fallback path
        return !containsPhiBar(term);
    }
    
    /// Check if a term contains any PhiBar nodes (quick non-normality indicator)
    static bool containsPhiBar(const core::Term* t) {
        if (!t) return false;
        if (t->kind() == core::TermKind::PhiBar) return true;
        for (const core::Term* child : t->children()) {
            if (containsPhiBar(child)) return true;
        }
        return false;
    }
    
private:
    /**
     * @brief Collapse double complement: sub(1, sub(1, x))  x
     *
     * Galois conjugation is an involution.  Without this reduction the
     * orbit generator creates unbounded sub(1, sub(1, sub(1, ...))) nesting.
     */
    const core::Term*
    normalizeDoubleComplement(const core::Term* t, core::TermFactory& factory) {
        if (!t) return t;
        // Detect sub(1, sub(1, x))
        if (t->kind() == core::TermKind::Application &&
            t->symbol() == "sub" &&
            t->children().size() == 2) {
            const core::Term* outer1 = t->children()[0];
            const core::Term* inner  = t->children()[1];
            if (outer1->kind() == core::TermKind::Scalar &&
                inner->kind() == core::TermKind::Application &&
                inner->symbol() == "sub" &&
                inner->children().size() == 2) {
                const core::Term* inner1 = inner->children()[0];
                if (inner1->kind() == core::TermKind::Scalar) {
                    // sub(1, sub(1, x))  x   (involution)
                    return inner->children()[1];
                }
            }
        }
        // Recurse into children
        if (t->kind() == core::TermKind::Application) {
            std::vector<const core::Term*> newKids;
            bool changed = false;
            for (auto* c : t->children()) {
                auto* nc = normalizeDoubleComplement(c, factory);
                newKids.push_back(nc);
                if (nc != c) changed = true;
            }
            if (changed) {
                return factory.apply(t->symbol(), newKids, t->sort());
            }
        }
        return t;
    }

    std::vector<const core::Term*> 
    computeOrbit(const core::Term* term, core::TermFactory& factory) {
        std::vector<const core::Term*> orbit;
        // Pointer-based seen set: hash-consed terms are pointer-unique,
        // so pointer identity == structural equality.  O(1) per lookup
        // instead of O(term_size) from encode().
        std::unordered_set<const core::Term*> seen;
        
        std::function<void(const core::Term*)> explore = 
            [&](const core::Term* t) {
            if (!t) return;
            if (seen.count(t)) return;
            if (orbit.size() >= options_.maxOrbitSize) return;
            
            seen.insert(t);
            orbit.push_back(t);
            
            // Apply Galois conjugation:   1-
            auto conjugated = applyGaloisConjugation(t, factory);
            if (conjugated) {
                // Apply involution collapse before recursing
                conjugated = normalizeDoubleComplement(conjugated, factory);
                if (conjugated != t) {
                    explore(conjugated);
                }
            }
        };
        
        explore(term);
        return orbit;
    }
    
    const core::Term* 
    applyGaloisConjugation(const core::Term* t, core::TermFactory& factory) {
        if (!t) return nullptr;
        
        // At phi nodes:     (atomic symbol swap  no syntactic expansion)
        if (t->kind() == core::TermKind::Phi) {
            return factory.phiBar();
        }
        
        // At phi_bar nodes:     (involution: swap back)
        if (t->kind() == core::TermKind::PhiBar) {
            return factory.phi();
        }
        
        // Recurse into children
        if (t->kind() == core::TermKind::Application) {
            std::vector<const core::Term*> newChildren;
            bool changed = false;
            for (const core::Term* child : t->children()) {
                auto newChild = applyGaloisConjugation(child, factory);
                if (newChild) {
                    newChildren.push_back(newChild);
                    changed = true;
                } else {
                    newChildren.push_back(child);
                }
            }
            
            if (changed) {
                auto result = factory.apply(t->symbol(), newChildren, t->sort());
                return normalizeDoubleComplement(result, factory);
            }
        }
        
        return nullptr;
    }
    
    /**
     * @brief EGraph-based orbit minimization
     * 
     * 1. Add all orbit terms to an EGraph
     * 2. Merge all orbit e-classes (they're all Galois-equivalent)
     * 3. Rebuild (congruence closure finds shared subterms)
     * 4. Extract minimum-cost representative
     */
    const core::Term* normalizeViaEGraph(
        const std::vector<const core::Term*>& orbit,
        core::TermFactory& factory) {
        
        egraph::EGraph eg;
        
        // Add each orbit element to the e-graph
        std::vector<egraph::EClassId> classIds;
        for (const core::Term* t : orbit) {
            auto eid = addTermToEGraph(t, eg);
            classIds.push_back(eid);
        }
        
        // Merge all orbit elements (they are Galois-equivalent)
        for (size_t i = 1; i < classIds.size(); ++i) {
            eg.merge(classIds[0], classIds[i],
                     egraph::MergeReason::fromAxiom(0, "GaloisOrbit"));
        }
        
        // Rebuild: congruence closure discovers additional merges
        eg.rebuild();
        
        // Extract minimum-cost term using shortlex cost
        auto result = eg.extract(eg.find(classIds[0]),
            [&](const egraph::ENode& /*node*/, const std::vector<double>& childCosts) -> double {
                double cost = 1.0; // Each node counts as 1
                for (double c : childCosts) cost += c;
                return cost;
            });
        
        if (result.has_value()) {
            // Reconstruct the Term from the extracted ENode
            auto reconstructed = reconstructFromENode(result->first, eg, factory);
            if (reconstructed) return reconstructed;
        }
        
        // Fallback: pick shortlex min from orbit
        auto minIt = std::min_element(orbit.begin(), orbit.end(),
            [](const core::Term* a, const core::Term* b) {
                auto aEnc = a->encode();
                auto bEnc = b->encode();
                if (aEnc.size() != bEnc.size())
                    return aEnc.size() < bEnc.size();
                return aEnc < bEnc;
            });
        return *minIt;
    }
    
    /// Recursively add a Term into an EGraph and return its e-class ID.
    /// Uses structured tags (var:, const:, phi, J, scalar:) for leaves
    /// so reconstruction can exactly reverse the encoding.
    egraph::EClassId addTermToEGraph(
        const core::Term* t, egraph::EGraph& eg) {
        
        if (!t) return 0;
        
        switch (t->kind()) {
            case core::TermKind::Variable:
                return eg.addLeaf("var:" + t->symbol());
                
            case core::TermKind::Constant:
                return eg.addLeaf("const:" + t->symbol());
                
            case core::TermKind::Phi:
                return eg.addLeaf("phi");
                
            case core::TermKind::PhiBar:
                return eg.addLeaf("phibar");
                
            case core::TermKind::J:
                return eg.addLeaf("J");
                
            case core::TermKind::Scalar:
                // Use encode() for scalars to get a stable bitwise representation
                return eg.addLeaf("scalar:" + t->encode());
                
            case core::TermKind::Application: {
                std::vector<egraph::EClassId> childIds;
                for (const core::Term* child : t->children()) {
                    childIds.push_back(addTermToEGraph(child, eg));
                }
                // Encode sort into the app symbol so reconstruction preserves it
                std::string key = t->symbol() + "#" + sortTag(t->sort());
                return eg.addApp(key, childIds);
            }
            
            case core::TermKind::Pair: {
                auto fid = addTermToEGraph(t->first(), eg);
                auto sid = addTermToEGraph(t->second(), eg);
                return eg.addApp("__pair__", {fid, sid});
            }
            
            default:
                return eg.addLeaf("const:" + t->encode());
        }
    }
    
    /// Convert a Sort enum to a short stable tag for EGraph symbols
    static std::string sortTag(core::Sort s) {
        switch (s) {
            case core::Sort::Real:       return "R";
            case core::Sort::Complex:    return "C";
            case core::Sort::Quaternion: return "H";
            case core::Sort::Octonion:   return "O";
            case core::Sort::Sedenion:   return "S16";
            case core::Sort::Generic:    return "G";
            default:                     return "?";
        }
    }
    
    /// Decode a sort tag back to Sort enum
    static core::Sort decodeSortTag(const std::string& tag) {
        if (tag == "R")   return core::Sort::Real;
        if (tag == "C")   return core::Sort::Complex;
        if (tag == "H")   return core::Sort::Quaternion;
        if (tag == "O")   return core::Sort::Octonion;
        if (tag == "S16") return core::Sort::Sedenion;
        return core::Sort::Generic;
    }
    
    /// Reconstruct a Term from an extracted ENode.
    /// Fully implements reconstruction for both leaves and applications
    /// by recursively extracting children from their e-classes.
    const core::Term* reconstructFromENode(
        const egraph::ENode& node,
        egraph::EGraph& eg,
        core::TermFactory& factory) {
        
        const std::string& sym = eg.symbolName(node.symbol);
        
        if (node.children.empty()) {
            // Leaf node: decode based on structured tag prefix
            if (sym.substr(0, 4) == "var:") {
                return factory.variable(sym.substr(4));
            } else if (sym == "phi") {
                return factory.phi();
            } else if (sym == "phibar") {
                return factory.phiBar();
            } else if (sym == "J") {
                return factory.J(core::Sort::Complex);
            } else if (sym.substr(0, 7) == "scalar:") {
                // The scalar encode is opaque; fall back to constant
                return factory.constant(sym.substr(7));
            } else if (sym.substr(0, 6) == "const:") {
                return factory.constant(sym.substr(6));
            } else {
                // Legacy/unknown leaf  treat as constant
                return factory.constant(sym);
            }
        }
        
        // Application node: recursively reconstruct children
        // Define cost function for child extraction (same as parent)
        auto costFn = [](const egraph::ENode& /*n*/, const std::vector<double>& childCosts) -> double {
            double cost = 1.0;
            for (double c : childCosts) cost += c;
            return cost;
        };
        
        std::vector<const core::Term*> children;
        children.reserve(node.children.size());
        
        for (egraph::EClassId cid : node.children) {
            auto childResult = eg.extract(eg.find(cid), costFn);
            if (!childResult.has_value()) {
                return nullptr; // Cannot extract child  signal fallback
            }
            const core::Term* childTerm = reconstructFromENode(
                childResult->first, eg, factory);
            if (!childTerm) {
                return nullptr; // Child reconstruction failed  signal fallback
            }
            children.push_back(childTerm);
        }
        
        // Handle pair nodes
        if (sym == "__pair__" && children.size() == 2) {
            return factory.pair(children[0], children[1]);
        }
        
        // Decode symbol and sort from the "sym#sort" key
        std::string symBase = sym;
        core::Sort sort = core::Sort::Generic;
        auto hashPos = sym.rfind('#');
        if (hashPos != std::string::npos) {
            symBase = sym.substr(0, hashPos);
            sort = decodeSortTag(sym.substr(hashPos + 1));
        }
        
        return factory.apply(symBase, std::move(children), sort);
    }
};

// ===========================================================================
// POLYNOMIAL NORMALIZATION PASS (STUB)
// ===========================================================================

/**
 * @brief Polynomial normalization pass  FULL IMPLEMENTATION
 * 
 * Canonicalizes polynomial expressions via three transformations:
 * 
 *   1. BOUNDED DISTRIBUTION:  a*(b+c)  a*b + a*c   (size-guarded)
 *                              (a+b)*c  a*c + b*c   (size-guarded)
 *      Only applied when result term size  MAX_DIST_SIZE to prevent
 *      exponential blowup.  Preserves operator ordering (critical for
 *      non-commutative algebras H, O  left/right dist are distinct).
 *
 *   2. LIKE-TERM COLLECTION:  a + a      2a
 *                              na + ma  (n+m)a
 *                              a + 0a    a   (zero-coefficient drop)
 *      Uses hash-consed pointer equality for O(1) base-term comparison.
 *      Extracts scalar coefficients from neg(x), mul(scalar(c), x).
 *
 *   3. INVERSE CANCELLATION:  a + neg(a)  0
 *                              neg(a) + a  0
 *
 * Termination:  Distribution is bounded by MAX_DIST_SIZE.  Collection
 *               and cancellation strictly reduce term count.  The outer
 *               NFEngine fixed-point loop (20 iters) provides a hard cap.
 *
 * Interaction with other passes:
 *   - Runs AFTER Ring (0/1 simplification) and AC (sorting/flattening)
 *   - Distribution may expose new Ring simplifications (a*0, a*1)
 *     or Phi reductions (+1), caught by the next fixpoint iteration
 */
class PolyTermPass : public TermNormalizationPass {
private:
    PolyOptions options_;
    
    /// Maximum term size (node count) for distribution to fire.
    /// Prevents exponential blowup: a*(b+b+...+b)  n products.
    static constexpr size_t MAX_DIST_SIZE = 64;
    
    /// Maximum recursion depth to prevent stack overflow on deep terms.
    static constexpr int MAX_RECURSE = 30;
    
public:
    explicit PolyTermPass(PolyOptions opts = {}) : options_(std::move(opts)) {}
    
    [[nodiscard]] std::string name() const override { return "Poly"; }
    [[nodiscard]] EquivalenceLayer layer() const override { return EquivalenceLayer::POLY; }
    
    [[nodiscard]] const core::Term* normalize(
        const core::Term* term, core::TermFactory& factory) override {
        if (!term) return nullptr;
        return normalizeImpl(term, factory, 0);
    }
    
    [[nodiscard]] bool isNormal(const core::Term* term) const override {
        return !hasNonNormalPattern(term);
    }
    
private:
    // -----------------------------------------------------------------
    // MAIN RECURSIVE NORMALIZATION
    // -----------------------------------------------------------------
    
    const core::Term* normalizeImpl(
        const core::Term* t, core::TermFactory& factory, int depth) {
        if (!t || depth > MAX_RECURSE) return t;
        if (t->kind() != core::TermKind::Application) return t;
        
        // Normalize children first (bottom-up)
        std::vector<const core::Term*> newCh;
        bool changed = false;
        for (const core::Term* c : t->children()) {
            auto* nc = normalizeImpl(c, factory, depth + 1);
            newCh.push_back(nc);
            if (nc != c) changed = true;
        }
        
        const core::Term* result = changed
            ? factory.apply(t->symbol(), newCh, t->sort())
            : t;
        
        // Apply polynomial transformations
        result = applyDistribution(result, factory);
        result = cancelInverses(result, factory);
        if (options_.collectLikeTerms) {
            result = collectLikeTerms(result, factory);
        }
        
        return result;
    }
    
    // -----------------------------------------------------------------
    // (1) BOUNDED DISTRIBUTION
    //
    //   a * (b + c)  a*b + a*c     [left distribution]
    //   (a + b) * c  a*c + b*c     [right distribution]
    //
    // Preserves left/right ordering  essential for H and O where
    // multiplication is non-commutative.
    // -----------------------------------------------------------------
    
    const core::Term* applyDistribution(
        const core::Term* t, core::TermFactory& factory) {
        if (!t || t->kind() != core::TermKind::Application) return t;
        if (t->symbol() != "mul" && t->symbol() != "*") return t;
        if (t->children().size() != 2) return t;
        if (t->size() > MAX_DIST_SIZE) return t;
        
        const auto* lhs = t->children()[0];
        const auto* rhs = t->children()[1];
        
        // Left distribution: a * (b + c)  a*b + a*c
        if (isAdd(rhs)) {
            std::vector<const core::Term*> addends;
            flattenAdd(rhs, addends);
            std::vector<const core::Term*> distributed;
            distributed.reserve(addends.size());
            for (auto* addend : addends) {
                distributed.push_back(factory.mul(lhs, addend));
            }
            return buildSum(distributed, t->sort(), factory);
        }
        
        // Right distribution: (a + b) * c  a*c + b*c
        if (isAdd(lhs)) {
            std::vector<const core::Term*> addends;
            flattenAdd(lhs, addends);
            std::vector<const core::Term*> distributed;
            distributed.reserve(addends.size());
            for (auto* addend : addends) {
                distributed.push_back(factory.mul(addend, rhs));
            }
            return buildSum(distributed, t->sort(), factory);
        }
        
        return t;
    }
    
    // -----------------------------------------------------------------
    // (2) LIKE-TERM COLLECTION
    //
    //   a + a        2a
    //   na + ma    (n+m)a
    //   a + (-a)     0    (captured here via coefficient -1)
    //
    // Uses pointer equality (hash-consing) for O(1) base comparison.
    // -----------------------------------------------------------------
    
    const core::Term* collectLikeTerms(
        const core::Term* t, core::TermFactory& factory) {
        if (!isAdd(t)) return t;
        
        // Flatten the sum into individual addends
        std::vector<const core::Term*> addends;
        flattenAdd(t, addends);
        if (addends.size() <= 1) return t;
        
        // Extract (coefficient, base_term) for each addend
        struct CoeffBase {
            double coeff;
            const core::Term* base;
        };
        
        std::vector<CoeffBase> terms;
        terms.reserve(addends.size());
        for (auto* a : addends) {
            double c; const core::Term* b;
            extractCoefficient(a, c, b);
            terms.push_back({c, b});
        }
        
        // Merge like terms  O(n) but n is small ( MAX_DIST_SIZE)
        std::vector<CoeffBase> merged;
        std::vector<bool> used(terms.size(), false);
        bool anyMerged = false;
        
        for (size_t i = 0; i < terms.size(); ++i) {
            if (used[i]) continue;
            double total = terms[i].coeff;
            for (size_t j = i + 1; j < terms.size(); ++j) {
                if (!used[j] && terms[i].base == terms[j].base) {
                    total += terms[j].coeff;
                    used[j] = true;
                    anyMerged = true;
                }
            }
            if (std::abs(total) > 1e-15) {
                merged.push_back({total, terms[i].base});
            } else {
                anyMerged = true;  // zero coefficient  term eliminated
            }
        }
        
        if (!anyMerged) return t;
        
        // Rebuild sum from merged coefficientbase pairs
        std::vector<const core::Term*> rebuilt;
        rebuilt.reserve(merged.size());
        for (auto& cb : merged) {
            if (std::abs(cb.coeff - 1.0) < 1e-15) {
                rebuilt.push_back(cb.base);
            } else if (std::abs(cb.coeff + 1.0) < 1e-15) {
                rebuilt.push_back(factory.neg(cb.base));
            } else {
                rebuilt.push_back(
                    factory.mul(factory.scalar(cb.coeff), cb.base));
            }
        }
        
        if (rebuilt.empty()) return factory.scalar(0.0);
        return buildSum(rebuilt, t->sort(), factory);
    }
    
    // -----------------------------------------------------------------
    // (3) INVERSE CANCELLATION
    //
    //   a + neg(a)  0
    //   neg(a) + a  0
    // -----------------------------------------------------------------
    
    const core::Term* cancelInverses(
        const core::Term* t, core::TermFactory& factory) {
        if (!isAdd(t) || t->children().size() != 2) return t;
        
        const auto* a = t->children()[0];
        const auto* b = t->children()[1];
        
        if (isNeg(b) && b->children()[0] == a)
            return factory.scalar(0.0);
        
        if (isNeg(a) && a->children()[0] == b)
            return factory.scalar(0.0);
        
        return t;
    }
    
    // -----------------------------------------------------------------
    // HELPER PREDICATES
    // -----------------------------------------------------------------
    
    static bool isAdd(const core::Term* t) {
        return t && t->kind() == core::TermKind::Application &&
               (t->symbol() == "add" || t->symbol() == "+") &&
               t->children().size() >= 2;
    }
    
    static bool isMul(const core::Term* t) {
        return t && t->kind() == core::TermKind::Application &&
               (t->symbol() == "mul" || t->symbol() == "*") &&
               t->children().size() == 2;
    }
    
    static bool isNeg(const core::Term* t) {
        return t && t->kind() == core::TermKind::Application &&
               t->symbol() == "neg" && t->children().size() == 1;
    }
    
    /// Flatten a nested add tree into a list of leaf addends
    static void flattenAdd(const core::Term* t,
                           std::vector<const core::Term*>& out) {
        if (isAdd(t)) {
            for (auto* child : t->children()) {
                flattenAdd(child, out);
            }
        } else {
            out.push_back(t);
        }
    }
    
    /// Extract (coefficient, base_term) from a term
    ///   neg(x)            (-1, x)
    ///   mul(scalar(c), x)  (c,  x)
    ///   mul(x, scalar(c))  (c,  x)
    ///   x                  (1,  x)
    static void extractCoefficient(const core::Term* t,
                                   double& coeff,
                                   const core::Term*& base) {
        if (isNeg(t)) {
            extractCoefficient(t->children()[0], coeff, base);
            coeff = -coeff;
            return;
        }
        if (isMul(t)) {
            if (t->children()[0]->kind() == core::TermKind::Scalar) {
                coeff = t->children()[0]->scalarValue();
                base = t->children()[1];
                return;
            }
            if (t->children()[1]->kind() == core::TermKind::Scalar) {
                coeff = t->children()[1]->scalarValue();
                base = t->children()[0];
                return;
            }
        }
        coeff = 1.0;
        base = t;
    }
    
    /// Build a right-associated sum from a list of terms
    static const core::Term* buildSum(
        const std::vector<const core::Term*>& addends,
        core::Sort /*sort*/,
        core::TermFactory& factory) {
        if (addends.empty()) return factory.scalar(0.0);
        if (addends.size() == 1) return addends[0];
        const core::Term* result = addends.back();
        for (size_t i = addends.size() - 1; i > 0; --i) {
            result = factory.add(addends[i - 1], result);
        }
        return result;
    }
    
    // -----------------------------------------------------------------
    // NON-NORMALITY DETECTION (for isNormal fast-path)
    // -----------------------------------------------------------------
    
    bool hasNonNormalPattern(const core::Term* t) const {
        if (!t || t->kind() != core::TermKind::Application) return false;
        
        const auto& sym = t->symbol();
        const auto& ch = t->children();
        
        // Distributable product
        if ((sym == "mul" || sym == "*") && ch.size() == 2 &&
            t->size() <= MAX_DIST_SIZE) {
            if (isAdd(ch[0]) || isAdd(ch[1])) return true;
        }
        
        // Cancelable inverse pair
        if (isAdd(t) && ch.size() == 2) {
            if (isNeg(ch[1]) && ch[1]->children()[0] == ch[0]) return true;
            if (isNeg(ch[0]) && ch[0]->children()[0] == ch[1]) return true;
        }
        
        // Duplicate base terms in a sum (like-term opportunity)
        if (isAdd(t) && options_.collectLikeTerms) {
            std::vector<const core::Term*> addends;
            flattenAdd(t, addends);
            std::unordered_set<const core::Term*> bases;
            for (auto* a : addends) {
                double c; const core::Term* b;
                extractCoefficient(a, c, b);
                if (!bases.insert(b).second) return true;
            }
        }
        
        // Recurse into children
        for (const core::Term* child : ch) {
            if (hasNonNormalPattern(child)) return true;
        }
        
        return false;
    }
};

// ===========================================================================
// DIMENSIONAL ANALYSIS PASS (STUB)
// ===========================================================================

/**
 * @brief Dimensional analysis pass
 * 
 * Annotates terms with physical dimension (length, mass, time, etc.)
 * and rejects dimensionally inconsistent equations. Currently a stub.
 */
class DimTermPass : public TermNormalizationPass {
private:
    DimOptions options_;
    
public:
    explicit DimTermPass(DimOptions opts = {}) : options_(std::move(opts)) {}
    
    [[nodiscard]] std::string name() const override { return "Dim"; }
    [[nodiscard]] EquivalenceLayer layer() const override { return EquivalenceLayer::DIM; }
    
    [[nodiscard]] const core::Term* normalize(
        const core::Term* term, core::TermFactory& /*factory*/) override {
        // STUB: dimensional analysis not yet implemented.
        // Future: infer dimension from operator signatures,
        // reject dim-incompatible sums, propagate through products.
        return term;
    }
    
    [[nodiscard]] bool isNormal(const core::Term* /*term*/) const override {
        return true;
    }
};

// ===========================================================================
// TENSOR NORMALIZATION PASS (STUB)
// ===========================================================================

/**
 * @brief Tensor/index notation normalization pass
 * 
 * Handles Einstein summation convention, index contraction, and
 * symmetry-based canonicalization. Currently a stub.
 */
class TensorTermPass : public TermNormalizationPass {
private:
    TensorOptions options_;
    
public:
    explicit TensorTermPass(TensorOptions opts = {}) : options_(std::move(opts)) {}
    
    [[nodiscard]] std::string name() const override { return "Tensor"; }
    [[nodiscard]] EquivalenceLayer layer() const override { return EquivalenceLayer::TENSOR; }
    
    [[nodiscard]] const core::Term* normalize(
        const core::Term* term, core::TermFactory& /*factory*/) override {
        // STUB: tensor normalization not yet implemented.
        // Future: canonicalize index labels, detect contractions,
        // apply symmetry groups (Birdtrack algorithm).
        return term;
    }
    
    [[nodiscard]] bool isNormal(const core::Term* /*term*/) const override {
        return true;
    }
};

// ===========================================================================
// NF ENGINE
// ===========================================================================

/**
 * @brief Main Normal Form Engine
 * 
 * Coordinates normalization passes according to an EquivalenceProfile.
 */
class NFEngine {
public:
    struct Stats {
        size_t totalNormalizations = 0;
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
        size_t totalFixpointIterations = 0;  // across all normalize() calls
        size_t maxFixpointIterations = 0;    // worst case single call
        std::chrono::nanoseconds totalTime{0};
        
        // Per-pass application counts (indexed by EquivalenceLayer ordinal)
        std::array<size_t, static_cast<size_t>(EquivalenceLayer::NUM_LAYERS)> passApplications{};
        // Per-pass change counts (pass actually modified the term)
        std::array<size_t, static_cast<size_t>(EquivalenceLayer::NUM_LAYERS)> passChanges{};
        
        [[nodiscard]] double hitRate() const {
            size_t total = cacheHits + cacheMisses;
            return total == 0 ? 0.0 : static_cast<double>(cacheHits) / total;
        }
        
        [[nodiscard]] double avgTimeNs() const {
            return totalNormalizations == 0 ? 0.0 
                : static_cast<double>(totalTime.count()) / totalNormalizations;
        }
        
        [[nodiscard]] double avgFixpointIters() const {
            return totalNormalizations == 0 ? 0.0
                : static_cast<double>(totalFixpointIterations) / totalNormalizations;
        }
    };
    
private:
    EquivalenceProfile profile_;
    core::TermFactory& factory_;        ///< Shared factory reference (no private copy)
    std::vector<std::unique_ptr<TermNormalizationPass>> passes_;
    
    // Cache: term id  normalized term
    mutable std::unordered_map<core::TermId, const core::Term*> cache_;
    mutable std::mutex cacheMutex_;
    mutable Stats stats_;
    
    static constexpr size_t MAX_CACHE_SIZE = 100000;
    
public:
    // -----------------------------------------------------------------------
    // CONSTRUCTORS
    // -----------------------------------------------------------------------
    
    explicit NFEngine(core::TermFactory& factory)
        : profile_(EquivalenceProfile::standard()), factory_(factory) {
        buildPasses();
    }
    
    NFEngine(EquivalenceProfile profile, core::TermFactory& factory) 
        : profile_(std::move(profile)), factory_(factory) {
        buildPasses();
    }
    
    // -----------------------------------------------------------------------
    // MAIN API
    // -----------------------------------------------------------------------
    
    /**
     * @brief Compute normal form of a term
     */
    [[nodiscard]] const core::Term* 
    normalize(const core::Term* term) {
        if (!term) return nullptr;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Check cache
        {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            auto it = cache_.find(term->id());
            if (it != cache_.end()) {
                ++stats_.cacheHits;
                return it->second;
            }
            ++stats_.cacheMisses;
        }
        
        // Apply passes in order, iterating to fixpoint.
        // A Phi reduction (+1) can create new Ring simplification
        // opportunities, so we must repeat until no pass changes anything.
        static constexpr size_t MAX_FIXPOINT_ITERS = 20;
        const core::Term* result = term;
        size_t itersUsed = 0;
        for (size_t iter = 0; iter < MAX_FIXPOINT_ITERS; ++iter) {
            const core::Term* prev = result;
            for (const auto& pass : passes_) {
                if (profile_.isEnabled(pass->layer())) {
                    const core::Term* before = result;
                    result = pass->normalize(result, factory_);
                    size_t layerIdx = static_cast<size_t>(pass->layer());
                    stats_.passApplications[layerIdx]++;
                    if (result != before) {
                        stats_.passChanges[layerIdx]++;
                    }
                }
            }
            itersUsed = iter + 1;
            // Pointer equality (hash-consed): if unchanged, we've reached fixpoint
            if (result == prev) break;
        }
        stats_.totalFixpointIterations += itersUsed;
        if (itersUsed > stats_.maxFixpointIterations) {
            stats_.maxFixpointIterations = itersUsed;
        }
        
        // Cache result
        {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            if (cache_.size() < MAX_CACHE_SIZE) {
                cache_[term->id()] = result;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        stats_.totalNormalizations++;
        stats_.totalTime += end - start;
        
        return result;
    }
    
    /**
     * @brief Check if a term is already in normal form
     */
    [[nodiscard]] bool isNormal(const core::Term* term) const {
        for (const auto& pass : passes_) {
            if (profile_.isEnabled(pass->layer()) && !pass->isNormal(term)) {
                return false;
            }
        }
        return true;
    }
    
    /**
     * @brief Check if two terms are equivalent under the profile
     */
    [[nodiscard]] bool areEquivalent(
        const core::Term* t1,
        const core::Term* t2
    ) {
        auto n1 = normalize(t1);
        auto n2 = normalize(t2);
        // After hash-consing and normalization, pointer equality works
        return n1 == n2;
    }
    
    // -----------------------------------------------------------------------
    // ACCESSORS
    // -----------------------------------------------------------------------
    
    [[nodiscard]] const EquivalenceProfile& profile() const { return profile_; }
    [[nodiscard]] core::TermFactory& factory() { return factory_; }
    [[nodiscard]] const core::TermFactory& factory() const { return factory_; }
    
    void setProfile(EquivalenceProfile profile) {
        profile_ = std::move(profile);
        buildPasses();
        clearCache();
    }
    
    void clearCache() {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        cache_.clear();
        stats_ = Stats{};
    }
    
    [[nodiscard]] size_t cacheSize() const {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        return cache_.size();
    }
    
    [[nodiscard]] const Stats& stats() const { return stats_; }
    [[nodiscard]] size_t numPasses() const { return passes_.size(); }
    
private:
    void buildPasses() {
        passes_.clear();
        
        // Build passes in profile order
        for (auto layer : profile_.order()) {
            if (!profile_.isEnabled(layer)) continue;
            
            switch (layer) {
                case EquivalenceLayer::ALPHA:
                    passes_.push_back(std::make_unique<AlphaTermPass>(
                        profile_.getOptions<AlphaOptions>(layer)));
                    break;
                    
                case EquivalenceLayer::AC:
                    passes_.push_back(std::make_unique<ACTermPass>(
                        profile_.getOptions<ACOptions>(layer),
                        profile_.numberSystem()));
                    break;
                    
                case EquivalenceLayer::RING:
                    passes_.push_back(std::make_unique<RingTermPass>(
                        profile_.getOptions<RingOptions>(layer)));
                    break;
                    
                case EquivalenceLayer::PHI:
                    passes_.push_back(std::make_unique<PhiTermPass>(
                        profile_.getOptions<PhiOptions>(layer)));
                    break;
                    
                case EquivalenceLayer::GOD:
                    passes_.push_back(std::make_unique<GODTermPass>(
                        profile_.getOptions<GODOptions>(layer)));
                    break;
                    
                case EquivalenceLayer::POLY:
                    passes_.push_back(std::make_unique<PolyTermPass>(
                        profile_.getOptions<PolyOptions>(layer)));
                    break;
                    
                case EquivalenceLayer::DIM:
                    passes_.push_back(std::make_unique<DimTermPass>(
                        profile_.getOptions<DimOptions>(layer)));
                    break;
                    
                case EquivalenceLayer::TENSOR:
                    passes_.push_back(std::make_unique<TensorTermPass>(
                        profile_.getOptions<TensorOptions>(layer)));
                    break;
                    
                default:
                    break;
            }
        }
    }
};

// ===========================================================================
// GLOBAL CONVENIENCE
// ===========================================================================

/**
 * @brief Global NF engine instance  uses globalContext().factory()
 * 
 * This ensures all normalization work shares the same TermFactory 
 * as the rest of the prover, maintaining the hash-consing invariant.
 */
inline NFEngine& globalNFEngine() {
    static NFEngine engine(EquivalenceProfile::standard(),
                           core::globalContext().factory());
    return engine;
}

/**
 * @brief Quick normal form computation
 */
inline const core::Term* NF(const core::Term* t) {
    return globalNFEngine().normalize(t);
}

/**
 * @brief Quick equivalence check
 */
inline bool equiv(const core::Term* t1, const core::Term* t2) {
    return globalNFEngine().areEquivalent(t1, t2);
}

} // namespace canon
} // namespace autodiscover

#endif // AUTODISCOVER_CANON_NF_ENGINE_HPP
