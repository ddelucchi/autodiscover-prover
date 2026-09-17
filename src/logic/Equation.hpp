/**
 * @file Equation.hpp
 * @brief Equation representation for equational reasoning
 * 
 * An equation l = r represents an oriented rewrite rule or 
 * an unoriented equality used in paramodulation.
 * 
 * Mathematical Foundation:
 * - Equations are pairs (l, r) with l, r  Term
 * - Orientation uses term ordering (KBO or LPO)
 * - GOD normalization applies to both sides
 */

#ifndef AUTODISCOVER_LOGIC_EQUATION_HPP
#define AUTODISCOVER_LOGIC_EQUATION_HPP

#include "../core/Term.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace autodiscover {
namespace logic {

using core::Term;
using core::TermId;

/**
 * @brief Equation orientation status
 */
enum class Orientation : uint8_t {
    Unoriented,   // l = r (symmetric)
    LeftToRight,  // l  r (l > r in term ordering)
    RightToLeft,  // l  r (r > l in term ordering)
};

/**
 * @brief Equation source for proof reconstruction
 */
enum class EquationSource : uint8_t {
    Axiom,          // User-provided axiom
    Goal,           // Negated goal (for refutation)
    Inference,      // Derived by inference rule
    Simplification, // Derived by simplification
    GODNormalized,  // Derived by GOD normalization
};

/**
 * @brief An equation l = r in the equational theory
 */
class Equation {
public:
    using Id = uint32_t;
    
    Equation(const Term* lhs, const Term* rhs, EquationSource source = EquationSource::Axiom)
        : lhs_(lhs), rhs_(rhs), source_(source), orientation_(Orientation::Unoriented) {}
    
    [[nodiscard]] const Term* lhs() const { return lhs_; }
    [[nodiscard]] const Term* rhs() const { return rhs_; }
    [[nodiscard]] EquationSource source() const { return source_; }
    [[nodiscard]] Orientation orientation() const { return orientation_; }
    [[nodiscard]] Id id() const { return id_; }
    
    void setId(Id id) { id_ = id; }
    void setOrientation(Orientation o) { orientation_ = o; }
    
    /**
     * @brief Check if equation is trivial (l = l)
     */
    [[nodiscard]] bool isTrivial() const {
        return lhs_->id() == rhs_->id();
    }
    
    /**
     * @brief Check if equation is ground (no variables)
     */
    [[nodiscard]] bool isGround() const {
        return isTermGround(lhs_) && isTermGround(rhs_);
    }
    
    /**
     * @brief Get the larger side according to orientation
     */
    [[nodiscard]] const Term* largerSide() const {
        switch (orientation_) {
            case Orientation::LeftToRight: return lhs_;
            case Orientation::RightToLeft: return rhs_;
            default: return nullptr;
        }
    }
    
    /**
     * @brief Get the smaller side according to orientation
     */
    [[nodiscard]] const Term* smallerSide() const {
        switch (orientation_) {
            case Orientation::LeftToRight: return rhs_;
            case Orientation::RightToLeft: return lhs_;
            default: return nullptr;
        }
    }
    
    /**
     * @brief Symmetric form: always returns (min, max) by STRUCTURAL encoding
     * 
     * Uses encode() for a deterministic, ID-independent ordering.
     * This ensures the same equation always gets the same canonical form
     * regardless of which TermFactory instance created the terms.
     */
    [[nodiscard]] std::pair<const Term*, const Term*> canonical() const {
        if (lhs_->encode() <= rhs_->encode()) {
            return {lhs_, rhs_};
        }
        return {rhs_, lhs_};
    }
    
    [[nodiscard]] std::string toString() const {
        std::string arrow;
        switch (orientation_) {
            case Orientation::Unoriented: arrow = " = "; break;
            case Orientation::LeftToRight: arrow = "  "; break;
            case Orientation::RightToLeft: arrow = "  "; break;
        }
        return lhs_->toString() + arrow + rhs_->toString();
    }
    
    // For hashing/equality in sets  uses structural encoding, not term IDs
    [[nodiscard]] size_t hash() const {
        auto [l, r] = canonical();
        std::string le = l->encode();
        std::string re = r->encode();
        size_t h = std::hash<std::string>{}(le);
        h ^= std::hash<std::string>{}(re) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
    
    [[nodiscard]] bool equals(const Equation& other) const {
        auto [l1, r1] = canonical();
        auto [l2, r2] = other.canonical();
        return l1->encode() == l2->encode() && r1->encode() == r2->encode();
    }

private:
    const Term* lhs_;
    const Term* rhs_;
    EquationSource source_;
    Orientation orientation_;
    Id id_ = 0;
    
    // Parent equations for proof reconstruction
    std::vector<Id> parents_;
    
    /**
     * @brief Recursively check if a term contains no variables
     */
    [[nodiscard]] static bool isTermGround(const Term* t) {
        if (!t) return true;
        if (t->isVariable()) return false;
        for (const Term* child : t->children()) {
            if (!isTermGround(child)) return false;
        }
        return true;
    }
};

/**
 * @brief Literal in a clause (equation with polarity)
 */
class Literal {
public:
    Literal(const Term* lhs, const Term* rhs, bool positive = true)
        : lhs_(lhs), rhs_(rhs), positive_(positive) {}
    
    [[nodiscard]] const Term* lhs() const { return lhs_; }
    [[nodiscard]] const Term* rhs() const { return rhs_; }
    [[nodiscard]] bool isPositive() const { return positive_; }
    [[nodiscard]] bool isNegative() const { return !positive_; }
    
    [[nodiscard]] Literal negated() const {
        return Literal(lhs_, rhs_, !positive_);
    }
    
    [[nodiscard]] std::string toString() const {
        if (positive_) {
            return lhs_->toString() + " = " + rhs_->toString();
        }
        return lhs_->toString() + "  " + rhs_->toString();
    }

private:
    const Term* lhs_;
    const Term* rhs_;
    bool positive_;
};

/**
 * @brief Clause (disjunction of literals)
 */
class Clause {
public:
    using Id = uint32_t;
    
    explicit Clause(std::vector<Literal> literals, EquationSource source = EquationSource::Inference)
        : literals_(std::move(literals)), source_(source) {}
    
    [[nodiscard]] const std::vector<Literal>& literals() const { return literals_; }
    [[nodiscard]] EquationSource source() const { return source_; }
    [[nodiscard]] Id id() const { return id_; }
    
    void setId(Id id) { id_ = id; }
    
    [[nodiscard]] bool isEmpty() const { return literals_.empty(); }
    [[nodiscard]] bool isUnit() const { return literals_.size() == 1; }
    
    /**
     * @brief Check if this is the empty clause (contradiction)
     */
    [[nodiscard]] bool isContradiction() const { return isEmpty(); }
    
    [[nodiscard]] std::string toString() const {
        if (isEmpty()) return ""; // Empty clause
        std::string result;
        for (size_t i = 0; i < literals_.size(); ++i) {
            if (i > 0) result += "  ";
            result += literals_[i].toString();
        }
        return result;
    }

private:
    std::vector<Literal> literals_;
    EquationSource source_;
    Id id_ = 0;
    std::vector<Id> parents_;
};

/**
 * @brief Hash functor for equations
 */
struct EquationHash {
    size_t operator()(const Equation& eq) const {
        return eq.hash();
    }
};

/**
 * @brief Equality functor for equations
 */
struct EquationEqual {
    bool operator()(const Equation& a, const Equation& b) const {
        return a.equals(b);
    }
};

} // namespace logic
} // namespace autodiscover

#endif // AUTODISCOVER_LOGIC_EQUATION_HPP
