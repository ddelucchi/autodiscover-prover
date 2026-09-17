/**
 * @file CertificateKernel.hpp
 * @brief Trusted proof kernel  the ONLY module that decides truth
 * 
 * ARCHITECTURE (LCF / De Bruijn Principle):
 * ==========================================
 * 
 * This kernel is the tiny trusted core. Everything else is untrusted
 * automation (search, heuristics, SCOUT, etc.) that produces certificates
 * the kernel replays.
 * 
 * KERNEL RULES (exhaustive  no other rule may produce a Theorem):
 * 
 *   1. Reflexivity:    t = t
 *   2. Symmetry:      from  t = u, derive  u = t
 *   3. Transitivity:  from  t = u and  u = v, derive  t = v
 *   4. Congruence:    from  t_i = u_i, derive  f(..t_i..) = f(..u_i..)
 *   5. Rewrite:       from axiom l = r and substitution , derive  l = r
 *   6. Axiom:         introduce a declared axiom as a theorem
 * 
 * A Theorem object can ONLY be constructed by these kernel functions.
 * There is no public constructor. This is the entire trust boundary.
 * 
 * @author AutoDiscoverProver Team
 */

#pragma once

#include "../core/Term.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <cassert>
#include <sstream>

namespace autodiscover {
namespace proof {

// =========================================================================
// KERNEL JUDGMENT: Theorem
// =========================================================================

/**
 * @brief A certified equation t = u that has been verified by the kernel
 * 
 * INVARIANT: A Theorem can ONLY be created through kernel rules.
 * There is no public constructor  this is what makes the kernel trusted.
 */
class Theorem {
    friend class Kernel;  // Only the Kernel can construct Theorems
    
public:
    using TermPtr = const core::Term*;
    
    [[nodiscard]] TermPtr lhs() const { return lhs_; }
    [[nodiscard]] TermPtr rhs() const { return rhs_; }
    [[nodiscard]] uint64_t id() const { return id_; }
    [[nodiscard]] const std::string& justification() const { return justification_; }
    [[nodiscard]] const std::vector<uint64_t>& premises() const { return premises_; }
    
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << "[T" << id_ << "] " << lhs_->toString() << " = " << rhs_->toString();
        oss << "  (by " << justification_ << ")";
        return oss.str();
    }
    
private:
    Theorem(TermPtr lhs, TermPtr rhs, uint64_t id, 
            const std::string& justification, std::vector<uint64_t> premises)
        : lhs_(lhs), rhs_(rhs), id_(id), 
          justification_(justification), premises_(std::move(premises)) {}
    
    TermPtr lhs_;
    TermPtr rhs_;
    uint64_t id_;
    std::string justification_;
    std::vector<uint64_t> premises_;  // IDs of premise Theorems
};

// =========================================================================
// CERTIFICATE STEP (input from untrusted automation)
// =========================================================================

/**
 * @brief A single step in a proof certificate
 * 
 * This is the untrusted input format. The kernel replays each step
 * and either produces a Theorem or rejects it.
 */
struct CertificateStep {
    enum class Rule {
        Reflexivity,
        Symmetry,
        Transitivity,
        Congruence,
        Rewrite,
        Axiom
    };
    
    Rule rule;
    
    // For Reflexivity: the term
    const core::Term* term = nullptr;
    
    // For Symmetry: premise theorem ID
    uint64_t symmetryPremise = 0;
    
    // For Transitivity: two premise theorem IDs
    uint64_t transPremise1 = 0;
    uint64_t transPremise2 = 0;
    
    // For Congruence: function symbol + child theorem IDs
    std::string congruenceFunc;
    std::vector<uint64_t> congruencePremises;
    
    // For Rewrite: axiom ID + substitution mapping
    size_t axiomId = 0;
    std::unordered_map<std::string, const core::Term*> substitution;
    
    // For Axiom: axiom index
    size_t axiomIndex = 0;
    
    // Helper constructors
    static CertificateStep reflexivity(const core::Term* t) {
        CertificateStep s;
        s.rule = Rule::Reflexivity;
        s.term = t;
        return s;
    }
    
    static CertificateStep symmetry(uint64_t premise) {
        CertificateStep s;
        s.rule = Rule::Symmetry;
        s.symmetryPremise = premise;
        return s;
    }
    
    static CertificateStep transitivity(uint64_t p1, uint64_t p2) {
        CertificateStep s;
        s.rule = Rule::Transitivity;
        s.transPremise1 = p1;
        s.transPremise2 = p2;
        return s;
    }
    
    static CertificateStep congruence(
        const std::string& func, 
        std::vector<uint64_t> childProofs
    ) {
        CertificateStep s;
        s.rule = Rule::Congruence;
        s.congruenceFunc = func;
        s.congruencePremises = std::move(childProofs);
        return s;
    }
    
    static CertificateStep rewrite(
        size_t axId,
        std::unordered_map<std::string, const core::Term*> subst
    ) {
        CertificateStep s;
        s.rule = Rule::Rewrite;
        s.axiomId = axId;
        s.substitution = std::move(subst);
        return s;
    }
    
    static CertificateStep axiom(size_t idx) {
        CertificateStep s;
        s.rule = Rule::Axiom;
        s.axiomIndex = idx;
        return s;
    }
};

// =========================================================================
// AXIOM BASE (the only place axioms live)
// =========================================================================

/**
 * @brief A declared axiom: lhs = rhs under a theory tag
 */
struct Axiom {
    const core::Term* lhs;
    const core::Term* rhs;
    std::string name;
    std::string theory;  // e.g., "ring", "group", "field"
    
    Axiom(const core::Term* l, const core::Term* r, 
          const std::string& n, const std::string& th = "")
        : lhs(l), rhs(r), name(n), theory(th) {}
};

// =========================================================================
// THE KERNEL
// =========================================================================

/**
 * @brief The trusted proof kernel
 * 
 * This is the ONLY module that produces Theorem objects.
 * It is deliberately minimal  no heuristics, no search, no canonicalization.
 * 
 * The kernel:
 * - Replays certificate steps
 * - Applies substitutions
 * - Checks structural properties
 * - Produces Theorems or errors
 * 
 * Everything else (e-graph, saturation, SCOUT, normalization) is untrusted
 * and must produce certificates that this kernel checks.
 */
class Kernel {
public:
    /**
     * @brief Replay result: either a theorem or an error
     */
    struct Result {
        bool success = false;
        std::string error;
        std::unique_ptr<Theorem> theorem;
        
        static Result ok(std::unique_ptr<Theorem> thm) {
            return {true, "", std::move(thm)};
        }
        static Result fail(const std::string& err) {
            return {false, err, nullptr};
        }
    };
    
    Kernel() = default;
    
    /**
     * @brief Register an axiom in the axiom base
     * @return The axiom index
     */
    size_t addAxiom(const core::Term* lhs, const core::Term* rhs,
                    const std::string& name, const std::string& theory = "") {
        size_t idx = axioms_.size();
        axioms_.emplace_back(lhs, rhs, name, theory);
        return idx;
    }
    
    /**
     * @brief Get an axiom by index
     */
    const Axiom& getAxiom(size_t idx) const {
        return axioms_.at(idx);
    }
    
    /**
     * @brief Number of registered axioms
     */
    size_t numAxioms() const { return axioms_.size(); }
    
    // -------------------------------------------------------------------
    // KERNEL RULES (the ONLY way to produce Theorems)
    // -------------------------------------------------------------------
    
    /**
     * @brief Rule 1: Reflexivity   t = t
     */
    Result reflexivity(const core::Term* t) {
        if (!t) return Result::fail("reflexivity: null term");
        return Result::ok(makeTheorem(t, t, "refl", {}));
    }
    
    /**
     * @brief Rule 2: Symmetry  from  t = u, derive  u = t
     */
    Result symmetry(const Theorem& premise) {
        return Result::ok(makeTheorem(
            premise.rhs(), premise.lhs(), "sym", {premise.id()}));
    }
    
    /**
     * @brief Rule 3: Transitivity  from  t = u and  u = v, derive  t = v
     */
    Result transitivity(const Theorem& thm1, const Theorem& thm2) {
        // Check: thm1.rhs must be structurally identical to thm2.lhs
        // Use encode() for structural comparison instead of pointer equality.
        // Pointer equality (!=) only works if both terms are interned in the
        // same TermFactory, which is not guaranteed across module boundaries.
        if (thm1.rhs()->encode() != thm2.lhs()->encode()) {
            return Result::fail("transitivity: intermediate terms don't match: " 
                               + thm1.rhs()->toString() + " vs " + thm2.lhs()->toString());
        }
        return Result::ok(makeTheorem(
            thm1.lhs(), thm2.rhs(), "trans", {thm1.id(), thm2.id()}));
    }
    
    /**
     * @brief Rule 4: Congruence  from  t_i = u_i, derive  f(..t..) = f(..u..)
     */
    Result congruence(
        const std::string& funcName,
        const std::vector<const Theorem*>& childProofs,
        core::TermFactory& factory
    ) {
        // Build f(t_1, ..., t_n) and f(u_1, ..., u_n)
        std::vector<const core::Term*> lhsArgs, rhsArgs;
        std::vector<uint64_t> premiseIds;
        
        for (const Theorem* child : childProofs) {
            if (!child) return Result::fail("congruence: null child proof");
            lhsArgs.push_back(child->lhs());
            rhsArgs.push_back(child->rhs());
            premiseIds.push_back(child->id());
        }
        
        const core::Term* lhsTerm = factory.apply(funcName, lhsArgs);
        const core::Term* rhsTerm = factory.apply(funcName, rhsArgs);
        
        return Result::ok(makeTheorem(lhsTerm, rhsTerm, "cong(" + funcName + ")", premiseIds));
    }
    
    /**
     * @brief Rule 5: Rewrite  instantiate axiom l = r under substitution 
     */
    Result rewrite(
        size_t axiomIdx,
        const std::unordered_map<std::string, const core::Term*>& subst,
        core::TermFactory& factory
    ) {
        if (axiomIdx >= axioms_.size()) {
            return Result::fail("rewrite: invalid axiom index " + std::to_string(axiomIdx));
        }
        
        const Axiom& ax = axioms_[axiomIdx];
        
        // Apply substitution to LHS and RHS
        const core::Term* lhsInst = applySubstitution(ax.lhs, subst, factory);
        const core::Term* rhsInst = applySubstitution(ax.rhs, subst, factory);
        
        if (!lhsInst || !rhsInst) {
            return Result::fail("rewrite: substitution application failed");
        }
        
        return Result::ok(makeTheorem(
            lhsInst, rhsInst, 
            "rewrite(" + ax.name + ")", {}));
    }
    
    /**
     * @brief Rule 6: Axiom introduction  declare a known axiom as a theorem
     */
    Result introduceAxiom(size_t axiomIdx) {
        if (axiomIdx >= axioms_.size()) {
            return Result::fail("axiom: invalid index " + std::to_string(axiomIdx));
        }
        
        const Axiom& ax = axioms_[axiomIdx];
        return Result::ok(makeTheorem(
            ax.lhs, ax.rhs, "axiom(" + ax.name + ")", {}));
    }
    
    // -------------------------------------------------------------------
    // CERTIFICATE REPLAY
    // -------------------------------------------------------------------
    
    /**
     * @brief Replay a complete certificate (sequence of steps)
     * 
     * Each step produces a Theorem that subsequent steps can reference.
     * Returns the final Theorem, or an error if any step fails.
     */
    Result replayCertificate(
        const std::vector<CertificateStep>& steps,
        core::TermFactory& factory
    ) {
        std::unordered_map<uint64_t, const Theorem*> proved;
        std::vector<std::unique_ptr<Theorem>> storage;  // owns the Theorems
        
        for (size_t i = 0; i < steps.size(); ++i) {
            Result result = replayStep(steps[i], proved, factory);
            
            if (!result.success) {
                return Result::fail("Step " + std::to_string(i) + ": " + result.error);
            }
            
            uint64_t id = result.theorem->id();
            proved[id] = result.theorem.get();
            storage.push_back(std::move(result.theorem));
        }
        
        if (storage.empty()) {
            return Result::fail("Empty certificate");
        }
        
        // Return the last theorem (copy via makeTheorem since Theorem ctor is private)
        return Result::ok(makeTheorem(
            storage.back()->lhs(), storage.back()->rhs(),
            storage.back()->justification(),
            storage.back()->premises()));
    }
    
    /**
     * @brief Get total number of theorems produced
     */
    uint64_t totalTheorems() const { return nextId_; }
    
private:
    std::vector<Axiom> axioms_;
    uint64_t nextId_ = 0;
    
    std::unique_ptr<Theorem> makeTheorem(
        const core::Term* lhs, const core::Term* rhs,
        const std::string& justification,
        std::vector<uint64_t> premises
    ) {
        uint64_t id = nextId_++;
        return std::unique_ptr<Theorem>(new Theorem(
            lhs, rhs, id, justification, std::move(premises)));
    }
    
    /**
     * @brief Replay a single certificate step
     */
    Result replayStep(
        const CertificateStep& step,
        const std::unordered_map<uint64_t, const Theorem*>& proved,
        core::TermFactory& factory
    ) {
        switch (step.rule) {
            case CertificateStep::Rule::Reflexivity:
                return reflexivity(step.term);
                
            case CertificateStep::Rule::Symmetry: {
                auto it = proved.find(step.symmetryPremise);
                if (it == proved.end()) {
                    return Result::fail("symmetry: premise " + 
                        std::to_string(step.symmetryPremise) + " not found");
                }
                return symmetry(*it->second);
            }
            
            case CertificateStep::Rule::Transitivity: {
                auto it1 = proved.find(step.transPremise1);
                auto it2 = proved.find(step.transPremise2);
                if (it1 == proved.end()) {
                    return Result::fail("transitivity: premise 1 not found");
                }
                if (it2 == proved.end()) {
                    return Result::fail("transitivity: premise 2 not found");
                }
                return transitivity(*it1->second, *it2->second);
            }
            
            case CertificateStep::Rule::Congruence: {
                std::vector<const Theorem*> childProofs;
                for (uint64_t pid : step.congruencePremises) {
                    auto it = proved.find(pid);
                    if (it == proved.end()) {
                        return Result::fail("congruence: premise " + 
                            std::to_string(pid) + " not found");
                    }
                    childProofs.push_back(it->second);
                }
                return congruence(step.congruenceFunc, childProofs, factory);
            }
            
            case CertificateStep::Rule::Rewrite:
                return rewrite(step.axiomId, step.substitution, factory);
                
            case CertificateStep::Rule::Axiom:
                return introduceAxiom(step.axiomIndex);
                
            default:
                return Result::fail("unknown certificate rule");
        }
    }
    
    /**
     * @brief Apply a substitution to a term (capture-avoiding)
     * 
     * Recursively replaces variables with their substitution values.
     */
    const core::Term* applySubstitution(
        const core::Term* term,
        const std::unordered_map<std::string, const core::Term*>& subst,
        core::TermFactory& factory
    ) const {
        if (!term) return nullptr;
        
        if (term->isVariable()) {
            auto it = subst.find(term->symbol());
            if (it != subst.end()) {
                return it->second;
            }
            return term;  // Unbound variable stays
        }
        
        if (term->isConstant() || term->isPhi() || term->isPhiBar() || term->isJ() || term->isScalar()) {
            return term;  // Constants are not substituted
        }
        
        if (term->isApplication()) {
            std::vector<const core::Term*> newArgs;
            bool changed = false;
            for (const core::Term* child : term->children()) {
                const core::Term* newChild = applySubstitution(child, subst, factory);
                newArgs.push_back(newChild);
                if (newChild != child) changed = true;
            }
            if (!changed) return term;
            return factory.apply(term->symbol(), std::move(newArgs), term->sort());
        }
        
        if (term->isPair()) {
            const core::Term* newFirst = applySubstitution(term->first(), subst, factory);
            const core::Term* newSecond = applySubstitution(term->second(), subst, factory);
            if (newFirst == term->first() && newSecond == term->second()) return term;
            return factory.pair(newFirst, newSecond);
        }
        
        return term;
    }
};

} // namespace proof
} // namespace autodiscover
