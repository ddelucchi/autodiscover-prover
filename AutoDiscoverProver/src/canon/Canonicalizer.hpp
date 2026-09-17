/**
 * @file Canonicalizer.hpp
 * @brief INTERNAL canonicalization primitives (AlphaContext, ACRegistry)
 * 
 * 
 *   DEPRECATED AS PUBLIC API  use CanonScalarEngine instead.      
 *                                                                   
 *   CanonScalarEngine (CanonScalar.hpp) is the single public       
 *   entry point for all canonicalization.  It internally delegates  
 *   to NFEngine (which uses EquivalenceProfile passes) and the     
 *   structural encoder.  The primitives below (AlphaContext,       
 *   ACRegistry) are still used by NFEngine passes but should NOT   
 *   be called directly from application code.                      
 * 
 * 
 * MATHEMATICAL FOUNDATION:
 * ========================
 * 
 * This module provides low-level canonicalization components:
 * 
 *   Raw Term  Alpha-Rename  AC-Normalize  E-Graph  Canon Term  Bytecode  Fingerprint
 * 
 * CANONICALIZATION INVARIANTS:
 * ============================
 * 
 * 1. IDEMPOTENCE: canon(canon(t)) = canon(t)
 * 2. COMPLETENESS: t  u  canon(t) = canon(u)
 * 3. SOUNDNESS: canon(t) = canon(u)  t  u
 * 
 * The canonicalizer integrates:
 * - GOD (Graded Orbit Descent) for syntactic normalization
 * - E-Graph for semantic equivalence
 * - Zeckendorf/Carry rewriting for -expressions
 * - Prefix-free encoding for fingerprinting
 * 
 * @author AutoDiscoverProver Team
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <functional>
#include <optional>

#include "../core/Term.hpp"
#include "../core/Context.hpp"
#include "../encoding/Encoding.hpp"
#include "../fingerprint/Fingerprinter.hpp"
#include "../egraph/EGraph.hpp"
#include "../ring/ZPhi.hpp"

namespace autodiscover {
namespace canon {

// ===========================================================================
// ALPHA RENAMING
// ===========================================================================

/**
 * @brief Alpha-renaming context for canonical variable naming
 * 
 * Variables are renamed in depth-first, left-to-right order of first occurrence.
 * The canonical names are x, x, x, ... (or "x0", "x1", "x2", ...)
 */
class AlphaContext {
private:
    std::unordered_map<std::string, std::string> renaming_;
    size_t nextIndex_ = 0;
    std::string prefix_;
    
public:
    explicit AlphaContext(const std::string& prefix = "x") : prefix_(prefix) {}
    
    /**
     * @brief Get or create canonical name for a variable
     */
    std::string canonicalize(const std::string& varName) {
        auto it = renaming_.find(varName);
        if (it != renaming_.end()) {
            return it->second;
        }
        
        std::string canonName = prefix_ + std::to_string(nextIndex_++);
        renaming_[varName] = canonName;
        return canonName;
    }
    
    /**
     * @brief Check if variable has been seen
     */
    bool hasSeen(const std::string& varName) const {
        return renaming_.find(varName) != renaming_.end();
    }
    
    /**
     * @brief Get the renaming map
     */
    const std::unordered_map<std::string, std::string>& renaming() const {
        return renaming_;
    }
    
    /**
     * @brief Reset for reuse
     */
    void reset() {
        renaming_.clear();
        nextIndex_ = 0;
    }
};

// ===========================================================================
// AC NORMALIZATION
// ===========================================================================

/**
 * @brief Associative-Commutative operator registry
 */
class ACRegistry {
private:
    std::unordered_set<std::string> acOps_;      // Both A and C
    std::unordered_set<std::string> assocOps_;   // Associative only
    std::unordered_set<std::string> commOps_;    // Commutative only
    
public:
    ACRegistry() {
        // Default AC operators  ONLY those that are always both
        // associative AND commutative regardless of algebraic sort.
        // CRITICAL: "mul"/"*" are NOT universally commutative 
        // Quaternion/Octonion multiplication is non-commutative.
        acOps_.insert("+");
        acOps_.insert("add");
        acOps_.insert("union");
        acOps_.insert("intersect");
        acOps_.insert("join");
        acOps_.insert("meet");
        
        // Associative-only operators (NOT commutative in general).
        // mul/"*" is associative for , ,  but not for .
        assocOps_.insert("*");
        assocOps_.insert("mul");
        
        // Commutative only (not associative in general)
        commOps_.insert("=");
        commOps_.insert("==");
        commOps_.insert("!=");
    }
    
    void registerAC(const std::string& op) { acOps_.insert(op); }
    void registerAssoc(const std::string& op) { assocOps_.insert(op); }
    void registerComm(const std::string& op) { commOps_.insert(op); }
    
    bool isAssociative(const std::string& op) const {
        return acOps_.count(op) || assocOps_.count(op);
    }
    
    bool isCommutative(const std::string& op) const {
        return acOps_.count(op) || commOps_.count(op);
    }
    
    bool isAC(const std::string& op) const {
        return acOps_.count(op);
    }
};

// ===========================================================================
// TERM CANONICALIZER
// ===========================================================================

/**
 * @brief Complete term canonicalizer
 * 
 * Applies the following normalization steps:
 * 1. Alpha-renaming (canonical variable names)
 * 2. AC-flattening (flatten nested AC operators)
 * 3. AC-sorting (sort arguments of commutative operators)
 * 4. Constant folding (evaluate constant expressions)
 * 5. Zeckendorf normalization (for -expressions)
 */
class TermCanonicalizer {
private:
    ACRegistry acRegistry_;
    core::TermFactory& factory_;       ///< Shared factory reference
    fingerprint::Fingerprinter fingerprinter_;
    
public:
    explicit TermCanonicalizer(core::TermFactory& factory) : factory_(factory) {}
    
    /**
     * @brief Get the AC registry for customization
     */
    ACRegistry& acRegistry() { return acRegistry_; }
    const ACRegistry& acRegistry() const { return acRegistry_; }
    
    /**
     * @brief Get the term factory
     */
    core::TermFactory& factory() { return factory_; }
    const core::TermFactory& factory() const { return factory_; }
    
    /**
     * @brief Get the fingerprinter
     */
    const fingerprint::Fingerprinter& fingerprinter() const { return fingerprinter_; }
    
    // -----------------------------------------------------------------------
    // FULL CANONICALIZATION
    // -----------------------------------------------------------------------
    
    /**
     * @brief Fully canonicalize a term
     * 
     * @param term Input term (raw pointer, owned by TermFactory)
     * @return Canonical term (also owned by TermFactory)
     */
    const core::Term* canonicalize(const core::Term* term) {
        AlphaContext alphaCtx;
        return canonicalizeImpl(term, alphaCtx);
    }
    
    /**
     * @brief Canonicalize with provided alpha context
     */
    const core::Term* canonicalize(
        const core::Term* term,
        AlphaContext& alphaCtx
    ) {
        return canonicalizeImpl(term, alphaCtx);
    }
    
private:
    const core::Term* canonicalizeImpl(
        const core::Term* term,
        AlphaContext& alphaCtx
    ) {
        if (!term) return nullptr;
        
        switch (term->kind()) {
            case core::TermKind::Variable: {
                std::string canonName = alphaCtx.canonicalize(term->symbol());
                return factory_.variable(canonName, term->sort());
            }
            
            case core::TermKind::Constant:
                return term;  // Constants are already canonical
            
            case core::TermKind::Scalar:
                return term;  // Scalars are already canonical
            
            case core::TermKind::Phi:
                return term;  //  is canonical
            
            case core::TermKind::PhiBar:
                return term;  //  is canonical
            
            case core::TermKind::J:
                return term;  // J values are canonical by index
            
            case core::TermKind::Application: {
                // Recursively canonicalize children
                std::vector<const core::Term*> canonKids;
                for (const core::Term* kid : term->children()) {
                    canonKids.push_back(canonicalizeImpl(kid, alphaCtx));
                }
                
                std::string funcSym = term->symbol();
                
                // AC-flattening and sorting
                if (acRegistry_.isAC(funcSym) && canonKids.size() >= 2) {
                    canonKids = flattenAC(funcSym, canonKids);
                    sortByCanonicalOrder(canonKids);
                } else if (acRegistry_.isCommutative(funcSym) && canonKids.size() == 2) {
                    sortByCanonicalOrder(canonKids);
                }
                
                return factory_.apply(funcSym, canonKids, term->sort());
            }
            
            case core::TermKind::Pair: {
                auto first = canonicalizeImpl(term->children()[0], alphaCtx);
                auto second = canonicalizeImpl(term->children()[1], alphaCtx);
                return factory_.pair(first, second);
            }
            
            default:
                return term;
        }
    }
    
    /**
     * @brief Flatten nested AC applications
     * 
     * +(+(a, b), c)  +(a, b, c)
     */
    std::vector<const core::Term*> flattenAC(
        const std::string& op,
        const std::vector<const core::Term*>& terms
    ) {
        std::vector<const core::Term*> result;
        
        for (const core::Term* term : terms) {
            if (term->kind() == core::TermKind::Application &&
                term->symbol() == op) {
                // Flatten recursively
                auto flattened = flattenAC(op, term->children());
                result.insert(result.end(), flattened.begin(), flattened.end());
            } else {
                result.push_back(term);
            }
        }
        
        return result;
    }
    
    /**
     * @brief Sort terms by canonical (shortlex) order
     */
    void sortByCanonicalOrder(std::vector<const core::Term*>& terms) {
        core::ShortlexComparator cmp;
        std::sort(terms.begin(), terms.end(),
            [&cmp](const core::Term* a, const core::Term* b) {
                return cmp(a, b);
            });
    }
    
public:
    // -----------------------------------------------------------------------
    // BYTECODE GENERATION
    // -----------------------------------------------------------------------
    
    /**
     * @brief Generate canonical bytecode for a term
     */
    std::vector<uint8_t> toCanonicalBytes(const core::Term* term) {
        auto canonTerm = canonicalize(term);
        return encoding::TermEncoder::encode(canonTerm);
    }
    
    /**
     * @brief Compute fingerprint of a term
     */
    fingerprint::Fingerprint fingerprint(const core::Term* term) {
        auto bytes = toCanonicalBytes(term);
        return fingerprinter_.fp_bytes(bytes);
    }
};

// ===========================================================================
// EQUATION CANONICALIZER
// ===========================================================================

/**
 * @brief Canonicalizer for equations (l = r)
 * 
 * Equations are canonicalized by:
 * 1. Canonicalizing both sides
 * 2. Ordering so that lhs  rhs (lexicographically on bytecode)
 */
class EquationCanonicalizer {
private:
    TermCanonicalizer termCanon_;
    
public:
    explicit EquationCanonicalizer(core::TermFactory& factory) : termCanon_(factory) {}
    
    /**
     * @brief Get the term canonicalizer
     */
    TermCanonicalizer& termCanonicalizer() { return termCanon_; }
    const TermCanonicalizer& termCanonicalizer() const { return termCanon_; }
    
    /**
     * @brief Canonicalize an equation
     * 
     * @param lhs Left-hand side
     * @param rhs Right-hand side
     * @return Pair of (canon_lhs, canon_rhs) with lhs  rhs
     */
    std::pair<const core::Term*, const core::Term*>
    canonicalize(const core::Term* lhs, const core::Term* rhs) {
        // Use single alpha context for both sides (shared variable namespace)
        AlphaContext ctx;
        
        auto canonLhs = termCanon_.canonicalize(lhs, ctx);
        auto canonRhs = termCanon_.canonicalize(rhs, ctx);
        
        // Order by bytecode
        auto lhsBytes = encoding::TermEncoder::encode(canonLhs);
        auto rhsBytes = encoding::TermEncoder::encode(canonRhs);
        
        if (lhsBytes > rhsBytes) {
            std::swap(canonLhs, canonRhs);
        }
        
        return {canonLhs, canonRhs};
    }
    
    /**
     * @brief Generate canonical bytecode for an equation
     */
    std::vector<uint8_t> toCanonicalBytes(
        const core::Term* lhs,
        const core::Term* rhs
    ) {
        auto [canonLhs, canonRhs] = canonicalize(lhs, rhs);
        auto lhsBytes = encoding::TermEncoder::encode(canonLhs);
        auto rhsBytes = encoding::TermEncoder::encode(canonRhs);
        return encoding::EquationEncoder::encode(lhsBytes, rhsBytes);
    }
    
    /**
     * @brief Compute fingerprint of an equation
     */
    fingerprint::Fingerprint fingerprint(
        const core::Term* lhs,
        const core::Term* rhs
    ) {
        auto bytes = toCanonicalBytes(lhs, rhs);
        return termCanon_.fingerprinter().fp_bytes(bytes);
    }
};

// ===========================================================================
// PROOF CANONICALIZER
// ===========================================================================

/**
 * @brief Node in a proof DAG
 */
struct ProofNode {
    enum class Kind {
        Axiom,          // Leaf: axiom or hypothesis
        Reflexivity,    // t = t
        Symmetry,       // From a = b, derive b = a
        Transitivity,   // From a = b and b = c, derive a = c
        Congruence,     // From a = b, derive f(a) = f(b)
        Substitution    // Apply substitution  to equation
    };
    
    Kind kind;
    
    // For Axiom: axiom identifier
    std::string axiomId;
    
    // The conclusion of this proof step
    std::pair<const core::Term*, const core::Term*> conclusion;
    
    // Indices of premises in the proof DAG
    std::vector<size_t> premises;
    
    // Unique ID (for canonicalization)
    size_t id = 0;
};

/**
 * @brief Canonicalizer for proof DAGs
 * 
 * Proofs are canonicalized by:
 * 1. Topological sort (premises before conclusions)
 * 2. Canonical equation representation at each node
 * 3. De-duplication of identical subproofs
 */
class ProofCanonicalizer {
private:
    EquationCanonicalizer eqCanon_;
    
public:
    explicit ProofCanonicalizer(core::TermFactory& factory) : eqCanon_(factory) {}
    
    /**
     * @brief Get the equation canonicalizer
     */
    EquationCanonicalizer& equationCanonicalizer() { return eqCanon_; }
    
    /**
     * @brief Canonicalize a proof DAG
     * 
     * @param nodes The proof nodes (may be in any order)
     * @return Canonicalized proof nodes in topological order
     */
    std::vector<ProofNode> canonicalize(std::vector<ProofNode> nodes) {
        if (nodes.empty()) return {};
        
        // Assign IDs
        for (size_t i = 0; i < nodes.size(); ++i) {
            nodes[i].id = i;
        }
        
        // Canonicalize each node's conclusion
        for (auto& node : nodes) {
            auto [lhs, rhs] = eqCanon_.canonicalize(
                node.conclusion.first, node.conclusion.second);
            node.conclusion = {lhs, rhs};
        }
        
        // Topological sort
        std::vector<ProofNode> sorted;
        std::unordered_set<size_t> visited;
        std::unordered_set<size_t> inProgress;
        
        std::function<bool(size_t)> visit = [&](size_t idx) -> bool {
            if (visited.count(idx)) return true;
            if (inProgress.count(idx)) return false;  // Cycle detected
            
            inProgress.insert(idx);
            
            for (size_t premise : nodes[idx].premises) {
                if (!visit(premise)) return false;
            }
            
            inProgress.erase(idx);
            visited.insert(idx);
            sorted.push_back(nodes[idx]);
            return true;
        };
        
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (!visit(i)) {
                throw std::runtime_error("Cycle detected in proof DAG");
            }
        }
        
        // Reassign IDs based on topological order
        std::unordered_map<size_t, size_t> oldToNew;
        for (size_t i = 0; i < sorted.size(); ++i) {
            oldToNew[sorted[i].id] = i;
            sorted[i].id = i;
        }
        
        // Update premise references
        for (auto& node : sorted) {
            for (size_t& premise : node.premises) {
                premise = oldToNew[premise];
            }
            // Sort premises for canonical order
            std::sort(node.premises.begin(), node.premises.end());
        }
        
        return sorted;
    }
    
    /**
     * @brief Generate canonical bytecode for a proof
     */
    std::vector<uint8_t> toCanonicalBytes(const std::vector<ProofNode>& proof) {
        encoding::ByteWriter writer;
        
        // Header
        writer.writeByte(encoding::BytecodeTags::PROOF);
        writer.writeVarInt(proof.size());
        
        for (const auto& node : proof) {
            // Kind
            writer.writeByte(static_cast<uint8_t>(node.kind));
            
            // Axiom ID if applicable
            if (node.kind == ProofNode::Kind::Axiom) {
                writer.writeString(node.axiomId);
            }
            
            // Conclusion
            auto lhsBytes = encoding::TermEncoder::encode(node.conclusion.first);
            auto rhsBytes = encoding::TermEncoder::encode(node.conclusion.second);
            for (uint8_t b : lhsBytes) writer.writeByte(b);
            for (uint8_t b : rhsBytes) writer.writeByte(b);
            
            // Premises
            writer.writeVarInt(node.premises.size());
            for (size_t premise : node.premises) {
                writer.writeVarInt(premise);
            }
        }
        
        return writer.finish();
    }
    
    /**
     * @brief Compute fingerprint of a proof
     */
    fingerprint::Fingerprint fingerprint(const std::vector<ProofNode>& proof) {
        auto canonProof = canonicalize(proof);
        auto bytes = toCanonicalBytes(canonProof);
        return eqCanon_.termCanonicalizer().fingerprinter().fp_bytes(bytes);
    }
};

// ===========================================================================
// FULL CANONICALIZATION ENGINE
// ===========================================================================

/**
 * @brief Complete canonicalization engine integrating all components
 * 
 * This is the main entry point for canonicalization.
 */
class CanonicalizationEngine {
private:
    ProofCanonicalizer proofCanon_;
    
public:
    explicit CanonicalizationEngine(core::TermFactory& factory)
        : proofCanon_(factory) {}
    
    // Access to sub-canonicalizers
    TermCanonicalizer& termCanonicalizer() {
        return proofCanon_.equationCanonicalizer().termCanonicalizer();
    }
    
    EquationCanonicalizer& equationCanonicalizer() {
        return proofCanon_.equationCanonicalizer();
    }
    
    ProofCanonicalizer& proofCanonicalizer() {
        return proofCanon_;
    }
    
    // -----------------------------------------------------------------------
    // TERM OPERATIONS
    // -----------------------------------------------------------------------
    
    const core::Term* canonTerm(const core::Term* t) {
        return termCanonicalizer().canonicalize(t);
    }
    
    std::vector<uint8_t> canonTermBytes(const core::Term* t) {
        return termCanonicalizer().toCanonicalBytes(t);
    }
    
    fingerprint::Fingerprint termFingerprint(const core::Term* t) {
        return termCanonicalizer().fingerprint(t);
    }
    
    // -----------------------------------------------------------------------
    // EQUATION OPERATIONS
    // -----------------------------------------------------------------------
    
    std::pair<const core::Term*, const core::Term*>
    canonEq(const core::Term* lhs, const core::Term* rhs) {
        return equationCanonicalizer().canonicalize(lhs, rhs);
    }
    
    std::vector<uint8_t> canonEqBytes(
        const core::Term* lhs,
        const core::Term* rhs
    ) {
        return equationCanonicalizer().toCanonicalBytes(lhs, rhs);
    }
    
    fingerprint::Fingerprint eqFingerprint(
        const core::Term* lhs,
        const core::Term* rhs
    ) {
        return equationCanonicalizer().fingerprint(lhs, rhs);
    }
    
    // -----------------------------------------------------------------------
    // PROOF OPERATIONS
    // -----------------------------------------------------------------------
    
    std::vector<ProofNode> canonProof(std::vector<ProofNode> proof) {
        return proofCanonicalizer().canonicalize(std::move(proof));
    }
    
    std::vector<uint8_t> canonProofBytes(const std::vector<ProofNode>& proof) {
        return proofCanonicalizer().toCanonicalBytes(proof);
    }
    
    fingerprint::Fingerprint proofFingerprint(const std::vector<ProofNode>& proof) {
        return proofCanonicalizer().fingerprint(proof);
    }
};

// ===========================================================================
// CONVENIENCE FUNCTIONS
// ===========================================================================

/**
 * @brief Global canonicalization engine  uses globalContext().factory()
 */
inline CanonicalizationEngine& globalEngine() {
    static CanonicalizationEngine engine(core::globalContext().factory());
    return engine;
}

/**
 * @brief Quick term canonicalization
 */
inline const core::Term* canon(const core::Term* t) {
    return globalEngine().canonTerm(t);
}

/**
 * @brief Quick fingerprint computation
 */
inline fingerprint::Fingerprint fp(const core::Term* t) {
    return globalEngine().termFingerprint(t);
}

/**
 * @brief Quick equation fingerprint
 */
inline fingerprint::Fingerprint fp(
    const core::Term* lhs,
    const core::Term* rhs
) {
    return globalEngine().eqFingerprint(lhs, rhs);
}

} // namespace canon
} // namespace autodiscover
