/**
 * @file Term.hpp
 * @brief DAG-based term representation with structural interning/hash-consing
 * 
 * Implements the term structure for the Autodiscovery Equational Prover.
 * Terms are represented as a maximally-shared DAG with O(1) equality via pointer comparison.
 * 
 * Mathematical Foundation:
 * - Terms in the Cayley-Dickson algebra tower: A = , A_{n+1} = A  A
 * - The encoding _Enc is the pullback of shortlex order for canonicalization
 */

#ifndef AUTODISCOVER_CORE_TERM_HPP
#define AUTODISCOVER_CORE_TERM_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <cassert>

namespace autodiscover {
namespace core {

// Forward declarations
class Term;
class TermFactory;

/**
 * @brief Unique identifier for terms (used for hash-consing)
 */
using TermId = uint64_t;

/**
 * @brief Type tag for sort discrimination
 */
enum class Sort : uint8_t {
    Real = 0,       // A = 
    Complex = 1,    // A =  (Cayley-Dickson level 1)
    Quaternion = 2, // A =  (Cayley-Dickson level 2)
    Octonion = 3,   // A =  (Cayley-Dickson level 3)
    Sedenion = 4,   // A (Cayley-Dickson level 4)
    Generic = 255   // Untyped/polymorphic
};

/**
 * @brief Kind of term node
 */
enum class TermKind : uint8_t {
    Variable,       // Free variable (pattern matching target)
    Constant,       // Named constant (e.g., , J, 0, 1)
    Application,    // Function application f(t, ..., t)
    Pair,           // Cayley-Dickson pair (a, b)
    Scalar,         // Real scalar value
    Phi,            // Golden ratio  = (1+5)/2
    J,              // Canonical imaginary unit J = (0,1)
    PhiBar          // Galois conjugate  = 1 = 1/  0.618
};

/**
 * @brief Hash for term structure (used in hash-consing)
 */
struct TermHash {
    size_t operator()(const Term* t) const noexcept;
};

/**
 * @brief Equality for term structure (structural equality before interning)
 */
struct TermEqual {
    bool operator()(const Term* a, const Term* b) const noexcept;
};

/**
 * @brief Core term representation
 * 
 * Terms are immutable after construction. The factory ensures structural
 * sharing so that equal terms share the same memory location.
 */
class Term {
public:
    friend class TermFactory;
    friend struct TermHash;
    friend struct TermEqual;

    // Accessors
    [[nodiscard]] TermId id() const noexcept { return id_; }
    [[nodiscard]] TermKind kind() const noexcept { return kind_; }
    [[nodiscard]] Sort sort() const noexcept { return sort_; }
    [[nodiscard]] const std::string& symbol() const noexcept { return symbol_; }
    [[nodiscard]] const std::vector<const Term*>& children() const noexcept { return children_; }
    
    // Query operations
    [[nodiscard]] bool isVariable() const noexcept { return kind_ == TermKind::Variable; }
    [[nodiscard]] bool isConstant() const noexcept { return kind_ == TermKind::Constant; }
    [[nodiscard]] bool isApplication() const noexcept { return kind_ == TermKind::Application; }
    [[nodiscard]] bool isPair() const noexcept { return kind_ == TermKind::Pair; }
    [[nodiscard]] bool isPhi() const noexcept { return kind_ == TermKind::Phi; }
    [[nodiscard]] bool isJ() const noexcept { return kind_ == TermKind::J; }
    [[nodiscard]] bool isScalar() const noexcept { return kind_ == TermKind::Scalar; }
    [[nodiscard]] bool isPhiBar() const noexcept { return kind_ == TermKind::PhiBar; }
    
    // For scalar terms
    [[nodiscard]] double scalarValue() const noexcept { return scalarValue_; }
    
    // For pair terms: (first, second)
    [[nodiscard]] const Term* first() const noexcept;
    [[nodiscard]] const Term* second() const noexcept;
    
    // Size metrics
    [[nodiscard]] size_t depth() const noexcept { return depth_; }
    [[nodiscard]] size_t size() const noexcept { return size_; }
    
    // String representation
    [[nodiscard]] std::string toString() const;
    
    // Encoding for shortlex comparison (GOD operator)
    [[nodiscard]] std::string encode() const;
    
    // O(1) equality via pointer comparison (due to hash-consing)
    [[nodiscard]] bool operator==(const Term& other) const noexcept { return this == &other; }
    [[nodiscard]] bool operator!=(const Term& other) const noexcept { return this != &other; }

private:
    Term(TermId id, TermKind kind, Sort sort, 
         const std::string& symbol, 
         std::vector<const Term*> children,
         double scalarValue = 0.0);
    
    TermId id_;
    TermKind kind_;
    Sort sort_;
    std::string symbol_;
    std::vector<const Term*> children_;
    double scalarValue_;
    size_t depth_;
    size_t size_;
    mutable size_t hash_; // Cached hash
    mutable bool hashComputed_;
    
    void computeMetrics();
    size_t computeHash() const noexcept;
};

/**
 * @brief Factory for creating and interning terms
 * 
 * Implements hash-consing: structurally equal terms are represented
 * by the same object, enabling O(1) equality checks.
 * 
 * IMPORTANT: In a multi-module pipeline, all modules MUST share the
 * same TermFactory (via Context::factory()) to preserve the invariant
 *     structurally_equal(a,b)  (&a == &b)
 * Creating a separate TermFactory breaks pointer-identity comparisons
 * across module boundaries. See Context.hpp for the canonical pattern.
 * Per-test factories in unit tests are fine (isolated scope).
 */
class TermFactory {
public:
    TermFactory();
    ~TermFactory();
    
    // Disable copy
    TermFactory(const TermFactory&) = delete;
    TermFactory& operator=(const TermFactory&) = delete;
    
    // Allow move
    TermFactory(TermFactory&&) = default;
    TermFactory& operator=(TermFactory&&) = default;
    
    // Creation methods (all return interned terms)
    
    /**
     * @brief Create a variable term
     * @param name Variable name (e.g., "x", "y")
     * @param sort Type sort
     */
    const Term* variable(const std::string& name, Sort sort = Sort::Generic);
    
    /**
     * @brief Create a constant term
     * @param name Constant name (e.g., "zero", "one")
     * @param sort Type sort
     */
    const Term* constant(const std::string& name, Sort sort = Sort::Generic);
    
    /**
     * @brief Create a scalar term
     * @param value The real number value
     */
    const Term* scalar(double value);
    
    /**
     * @brief Create the golden ratio  = (1+5)/2
     * Fixed point of R(z) = 1 + 1/z, satisfies  =  + 1
     */
    const Term* phi();
    
    /**
     * @brief Create the Galois conjugate  = 1 = 1/  0.618
     * Atomic involution partner of . Conjugation swaps    
     * without syntactic expansion, preventing orbit blowup.
     */
    const Term* phiBar();
    
    /**
     * @brief Create the canonical imaginary unit J = (0,1)
     * Satisfies J = -1
     */
    const Term* J(Sort level = Sort::Complex);
    
    /**
     * @brief Create a Cayley-Dickson pair (a, b)  A_{n+1} = A  A
     */
    const Term* pair(const Term* first, const Term* second);
    
    /**
     * @brief Create a function application f(args...)
     */
    const Term* apply(const std::string& funcName, 
                      std::vector<const Term*> args,
                      Sort resultSort = Sort::Generic);
    
    // Common operations
    
    /**
     * @brief Addition: a + b
     */
    const Term* add(const Term* a, const Term* b);
    
    /**
     * @brief Multiplication: a  b
     * For Cayley-Dickson: (a,b)(c,d) = (ac - d*b, da + bc*)
     */
    const Term* mul(const Term* a, const Term* b);
    
    /**
     * @brief Conjugation: a*
     * For Cayley-Dickson: (a,b)* = (a*, -b)
     */
    const Term* conj(const Term* a);
    
    /**
     * @brief Negation: -a
     */
    const Term* neg(const Term* a);
    
    /**
     * @brief Inverse: a
     */
    const Term* inv(const Term* a);
    
    /**
     * @brief Norm: N(a) = a  a*
     */
    const Term* norm(const Term* a);
    
    /**
     * @brief Scalar part extraction: Scal(a)
     * For (a, b), returns Re(a)
     */
    const Term* scalarPart(const Term* a);
    
    // SCOUT operations
    
    /**
     * @brief Alignment: Align_U(q) = Scal(U*  q)
     */
    const Term* align(const Term* U, const Term* q);
    
    /**
     * @brief Phase transport: _{j+1} = _j  U_j
     */
    const Term* phaseTransport(const Term* phi, const Term* U);
    
    // Fibonacci iteration
    
    /**
     * @brief Fibonacci recursion step: R(z) = 1 + 1/z
     * Fixed point is 
     */
    const Term* fibStep(const Term* z);
    
    // Statistics
    [[nodiscard]] size_t termCount() const noexcept { return terms_.size(); }
    [[nodiscard]] size_t uniqueCount() const noexcept { return internMap_.size(); }

private:
    // Internal interning
    const Term* intern(std::unique_ptr<Term> term);
    TermId nextId();
    
    std::vector<std::unique_ptr<Term>> terms_;
    std::unordered_map<const Term*, const Term*, TermHash, TermEqual> internMap_;
    TermId nextId_ = 1;
    
    // Cached special terms
    const Term* phiTerm_ = nullptr;
    const Term* phiBarTerm_ = nullptr;
    const Term* zeroTerm_ = nullptr;
    const Term* oneTerm_ = nullptr;
};

/**
 * @brief Shortlex comparison for GOD canonicalization
 * 
 * _Enc ordering: (length, then lexicographic on encoding)
 * This is the well-order used by the GOD operator for orbit minimization.
 */
struct ShortlexComparator {
    bool operator()(const Term* a, const Term* b) const noexcept;
};

} // namespace core
} // namespace autodiscover

#endif // AUTODISCOVER_CORE_TERM_HPP
