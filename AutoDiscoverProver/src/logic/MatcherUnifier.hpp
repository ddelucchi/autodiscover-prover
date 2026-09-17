/**
 * @file MatcherUnifier.hpp
 * @brief Pattern matching and unification for equational reasoning
 * 
 * Implements:
 * - Syntactic unification (Robinson's algorithm)
 * - Pattern matching (one-way unification)
 * - Substitution composition
 * - Occurs check
 */

#ifndef AUTODISCOVER_LOGIC_MATCHERUNIFIER_HPP
#define AUTODISCOVER_LOGIC_MATCHERUNIFIER_HPP

#include "../core/Term.hpp"
#include <unordered_map>
#include <optional>
#include <vector>
#include <string>

namespace autodiscover {
namespace logic {

using core::Term;
using core::TermId;
using core::TermFactory;

/**
 * @brief Substitution: variable  term mapping
 */
class Substitution {
public:
    using Map = std::unordered_map<TermId, const Term*>;
    
    Substitution() = default;
    explicit Substitution(Map bindings) : bindings_(std::move(bindings)) {}
    
    /**
     * @brief Bind a variable to a term
     */
    bool bind(TermId varId, const Term* term) {
        if (bindings_.count(varId)) {
            return bindings_[varId]->id() == term->id();
        }
        bindings_[varId] = term;
        return true;
    }
    
    /**
     * @brief Look up a variable binding
     */
    [[nodiscard]] const Term* lookup(TermId varId) const {
        auto it = bindings_.find(varId);
        return it != bindings_.end() ? it->second : nullptr;
    }
    
    /**
     * @brief Apply substitution to a term
     */
    [[nodiscard]] const Term* apply(const Term* term, TermFactory& factory) const;
    
    /**
     * @brief Compose with another substitution: this  other
     */
    [[nodiscard]] Substitution compose(const Substitution& other, TermFactory& factory) const;
    
    /**
     * @brief Check if empty
     */
    [[nodiscard]] bool isEmpty() const { return bindings_.empty(); }
    
    /**
     * @brief Get all bindings
     */
    [[nodiscard]] const Map& bindings() const { return bindings_; }
    
    /**
     * @brief Get domain (bound variables)
     */
    [[nodiscard]] std::vector<TermId> domain() const {
        std::vector<TermId> result;
        result.reserve(bindings_.size());
        for (const auto& [var, _] : bindings_) {
            result.push_back(var);
        }
        return result;
    }
    
    [[nodiscard]] std::string toString() const {
        std::string result = "{";
        bool first = true;
        for (const auto& [var, term] : bindings_) {
            if (!first) result += ", ";
            first = false;
            result += "v" + std::to_string(var) + "  " + term->toString();
        }
        result += "}";
        return result;
    }

private:
    Map bindings_;
};

/**
 * @brief Unification result
 */
struct UnificationResult {
    bool success;
    Substitution substitution;
    std::string failureReason;
    
    static UnificationResult Success(Substitution sub) {
        return {true, std::move(sub), ""};
    }
    
    static UnificationResult Failure(std::string reason) {
        return {false, {}, std::move(reason)};
    }
    
    explicit operator bool() const { return success; }
};

/**
 * @brief Unifier implementing Robinson's algorithm
 */
class Unifier {
public:
    explicit Unifier(TermFactory& factory) : factory_(factory) {}
    
    /**
     * @brief Unify two terms
     */
    [[nodiscard]] UnificationResult unify(const Term* t1, const Term* t2) {
        Substitution sub;
        if (unifyTerms(t1, t2, sub)) {
            return UnificationResult::Success(std::move(sub));
        }
        return UnificationResult::Failure("Terms not unifiable");
    }
    
    /**
     * @brief Check if t1 matches t2 (one-way unification)
     * Variables in t1 can be bound, but t2 is treated as ground
     */
    [[nodiscard]] UnificationResult match(const Term* pattern, const Term* target) {
        Substitution sub;
        if (matchTerms(pattern, target, sub)) {
            return UnificationResult::Success(std::move(sub));
        }
        return UnificationResult::Failure("Pattern does not match");
    }
    
    /**
     * @brief Occurs check: does variable occur in term?
     */
    [[nodiscard]] static bool occursIn(TermId varId, const Term* term) {
        if (term->kind() == core::TermKind::Variable) {
            return term->id() == varId;
        }
        for (const Term* child : term->children()) {
            if (occursIn(varId, child)) {
                return true;
            }
        }
        return false;
    }

private:
    TermFactory& factory_;
    
    bool unifyTerms(const Term* t1, const Term* t2, Substitution& sub) {
        // Apply current substitution
        t1 = deref(t1, sub);
        t2 = deref(t2, sub);
        
        // Same term?
        if (t1->id() == t2->id()) {
            return true;
        }
        
        // Variable cases
        if (t1->kind() == core::TermKind::Variable) {
            return unifyVar(t1, t2, sub);
        }
        if (t2->kind() == core::TermKind::Variable) {
            return unifyVar(t2, t1, sub);
        }
        
        // Application/compound terms
        if (t1->kind() != t2->kind()) {
            return false;
        }
        
        if (t1->symbol() != t2->symbol()) {
            return false;
        }
        
        const auto& c1 = t1->children();
        const auto& c2 = t2->children();
        
        if (c1.size() != c2.size()) {
            return false;
        }
        
        for (size_t i = 0; i < c1.size(); ++i) {
            if (!unifyTerms(c1[i], c2[i], sub)) {
                return false;
            }
        }
        
        return true;
    }
    
    bool unifyVar(const Term* var, const Term* term, Substitution& sub) {
        // Occurs check
        if (occursIn(var->id(), term)) {
            return false;
        }
        return sub.bind(var->id(), term);
    }
    
    const Term* deref(const Term* term, const Substitution& sub) {
        while (term->kind() == core::TermKind::Variable) {
            const Term* bound = sub.lookup(term->id());
            if (!bound) break;
            term = bound;
        }
        return term;
    }
    
    bool matchTerms(const Term* pattern, const Term* target, Substitution& sub) {
        // Dereference pattern
        pattern = deref(pattern, sub);
        
        // Variable in pattern matches anything
        if (pattern->kind() == core::TermKind::Variable) {
            const Term* bound = sub.lookup(pattern->id());
            if (bound) {
                return bound->id() == target->id();
            }
            return sub.bind(pattern->id(), target);
        }
        
        // Same kind and symbol required
        if (pattern->kind() != target->kind()) {
            return false;
        }
        
        if (pattern->symbol() != target->symbol()) {
            return false;
        }
        
        const auto& pc = pattern->children();
        const auto& tc = target->children();
        
        if (pc.size() != tc.size()) {
            return false;
        }
        
        for (size_t i = 0; i < pc.size(); ++i) {
            if (!matchTerms(pc[i], tc[i], sub)) {
                return false;
            }
        }
        
        return true;
    }
};

// Implementation of Substitution::apply
inline const Term* Substitution::apply(const Term* term, TermFactory& factory) const {
    if (term->kind() == core::TermKind::Variable) {
        const Term* bound = lookup(term->id());
        return bound ? apply(bound, factory) : term; // Recursive apply for chains
    }
    
    // Rebuild with substituted children
    std::vector<const Term*> newChildren;
    bool changed = false;
    
    for (const Term* child : term->children()) {
        const Term* newChild = apply(child, factory);
        newChildren.push_back(newChild);
        if (newChild != child) changed = true;
    }
    
    if (!changed) {
        return term;
    }
    
    // Rebuild term preserving the original kind (Pair, Application, etc.)
    if (term->kind() == core::TermKind::Pair && newChildren.size() == 2) {
        return factory.pair(newChildren[0], newChildren[1]);
    }
    return factory.apply(term->symbol(), newChildren, term->sort());
}

// Implementation of Substitution::compose
inline Substitution Substitution::compose(const Substitution& other, TermFactory& factory) const {
    Map result;
    
    // Apply this to bindings of other
    for (const auto& [var, term] : other.bindings_) {
        result[var] = this->apply(term, factory);
    }
    
    // Add bindings from this that are not in other's domain
    for (const auto& [var, term] : bindings_) {
        if (!result.count(var)) {
            result[var] = term;
        }
    }
    
    return Substitution(std::move(result));
}

} // namespace logic
} // namespace autodiscover

#endif // AUTODISCOVER_LOGIC_MATCHERUNIFIER_HPP
