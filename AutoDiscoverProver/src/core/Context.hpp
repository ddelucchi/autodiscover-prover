#ifndef AUTODISCOVER_CORE_CONTEXT_HPP
#define AUTODISCOVER_CORE_CONTEXT_HPP

/**
 * @file Context.hpp
 * @brief Shared canonical context  single source of truth for term interning
 * 
 * DESIGN RATIONALE
 * =================
 * 
 * Before this header, every module (NFEngine, Canonicalizer, DiscoveryEngine,
 * CanonScalarEngine) owned a SEPARATE TermFactory.  That broke hash-consing's
 * central guarantee:
 * 
 *      terms a,b : structurally_equal(a,b)  (&a == &b)
 * 
 * because a term interned in one factory could have a numerically different
 * pointer than the same term interned in another.
 * 
 * Solution: every module that creates terms must obtain its factory from a
 * shared Context.  The Context owns exactly one TermFactory and one (future)
 * SymbolTable.  Multiple Contexts are allowed (e.g. for unit-testing) but
 * within a single pipeline run there is exactly one.
 * 
 * INVARIANTS
 * ===========
 * 
 * 1. Context owns the sole TermFactory for its scope.
 * 2. All passes, engines, and encoders that create/intern terms within that
 *    scope MUST use ctx.factory()  never a locally declared TermFactory.
 * 3. Context is non-copyable, non-movable (stable pointer identity).
 * 
 * @author AutoDiscoverProver Team
 * @date 2024
 */

#include "Term.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace autodiscover {
namespace core {

// ===========================================================================
// OPERATOR REGISTRY
// ===========================================================================

/**
 * @brief Operator property registry  maps operator names to algebraic metadata
 * 
 * Tracks associativity, commutativity, idempotency, and result sorts.
 * Complementary to SymbolTable.hpp which handles symbol declarations.
 */
class OperatorRegistry {
public:
    struct SymbolInfo {
        std::string name;
        int arity = -1;            // -1 = variadic
        bool isAssociative = false;
        bool isCommutative = false;
        bool isIdempotent  = false;
        Sort resultSort = Sort::Generic;
    };

    OperatorRegistry() { registerBuiltins(); }

    /// Register or overwrite a symbol definition
    void registerSymbol(SymbolInfo info) {
        symbols_[info.name] = std::move(info);
    }

    /// Query  returns nullptr if unknown
    [[nodiscard]] const SymbolInfo* lookup(const std::string& name) const {
        auto it = symbols_.find(name);
        return it == symbols_.end() ? nullptr : &it->second;
    }

    /// Convenience predicates
    [[nodiscard]] bool isAC(const std::string& op) const {
        auto* s = lookup(op);
        return s && s->isAssociative && s->isCommutative;
    }

    [[nodiscard]] bool isAssociativeOnly(const std::string& op) const {
        auto* s = lookup(op);
        return s && s->isAssociative && !s->isCommutative;
    }

private:
    std::unordered_map<std::string, SymbolInfo> symbols_;

    void registerBuiltins() {
        // --- Truly AC operators (commutative AND associative) ---
        registerSymbol({"+",     2, true, true,  false, Sort::Generic});
        registerSymbol({"add",   2, true, true,  false, Sort::Generic});
        registerSymbol({"union",     2, true, true, true,  Sort::Generic});
        registerSymbol({"intersect", 2, true, true, true,  Sort::Generic});
        registerSymbol({"join",  2, true, true,  false, Sort::Generic});
        registerSymbol({"meet",  2, true, true,  false, Sort::Generic});
        registerSymbol({"gcd",   2, true, true,  false, Sort::Generic});
        registerSymbol({"lcm",   2, true, true,  false, Sort::Generic});
        registerSymbol({"max",   2, true, true,  true,  Sort::Generic});
        registerSymbol({"min",   2, true, true,  true,  Sort::Generic});

        // --- mul/"*"  associative but NOT universally commutative ---
        //     For Quaternion/Octonion sorts, mul is non-commutative.
        //     Commutativity is gated per-sort at the AC-pass level.
        registerSymbol({"*",     2, true, false, false, Sort::Generic});
        registerSymbol({"mul",   2, true, false, false, Sort::Generic});

        // --- Commutative-only (not associative in general) ---
        registerSymbol({"=",     2, false, true,  false, Sort::Generic});
        registerSymbol({"==",    2, false, true,  false, Sort::Generic});
        registerSymbol({"!=",    2, false, true,  false, Sort::Generic});

        // --- Unary operators ---
        registerSymbol({"neg",   1, false, false, false, Sort::Generic});
        registerSymbol({"conj",  1, false, false, false, Sort::Generic});
        registerSymbol({"inv",   1, false, false, false, Sort::Generic});
        registerSymbol({"norm",  1, false, false, false, Sort::Generic});
        registerSymbol({"N",     1, false, false, false, Sort::Generic});  // alias (legacy)
        registerSymbol({"scal",  1, false, false, false, Sort::Real});
        registerSymbol({"Scal",  1, false, false, false, Sort::Real});    // alias (TermFactory uses "Scal")

        // --- Binary non-AC ---
        registerSymbol({"sub",   2, false, false, false, Sort::Generic});
        registerSymbol({"div",   2, false, false, false, Sort::Generic});
        registerSymbol({"pow",   2, false, false, false, Sort::Generic});

        // --- Transcendental functions (unary) ---
        registerSymbol({"sin",       1, false, false, false, Sort::Real});
        registerSymbol({"cos",       1, false, false, false, Sort::Real});
        registerSymbol({"tan",       1, false, false, false, Sort::Real});
        registerSymbol({"asin",      1, false, false, false, Sort::Real});
        registerSymbol({"acos",      1, false, false, false, Sort::Real});
        registerSymbol({"atan",      1, false, false, false, Sort::Real});
        registerSymbol({"exp",       1, false, false, false, Sort::Real});
        registerSymbol({"log",       1, false, false, false, Sort::Real});
        registerSymbol({"sqrt",      1, false, false, false, Sort::Real});
        registerSymbol({"abs",       1, false, false, false, Sort::Real});
        registerSymbol({"floor",     1, false, false, false, Sort::Real});
        registerSymbol({"ceil",      1, false, false, false, Sort::Real});
        registerSymbol({"sign",      1, false, false, false, Sort::Real});
        registerSymbol({"sinh",      1, false, false, false, Sort::Real});
        registerSymbol({"cosh",      1, false, false, false, Sort::Real});
        registerSymbol({"tanh",      1, false, false, false, Sort::Real});

        // --- Number theory / combinatorial ---
        registerSymbol({"factorial", 1, false, false, false, Sort::Real});
        registerSymbol({"choose",    2, false, false, false, Sort::Real});
        registerSymbol({"mod",       2, false, false, false, Sort::Real});
        registerSymbol({"atan2",     2, false, false, false, Sort::Real});
    }
};

// ===========================================================================
// CONTEXT
// ===========================================================================

/**
 * @brief Shared canonical context
 * 
 * Single source of truth for term interning and operator metadata.
 */
class Context {
public:
    Context() = default;

    // Non-copyable, non-movable (stable pointer identity)
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

    /// The ONE TermFactory for this context
    [[nodiscard]] TermFactory& factory() noexcept { return factory_; }
    [[nodiscard]] const TermFactory& factory() const noexcept { return factory_; }

    /// Operator property registry
    [[nodiscard]] OperatorRegistry& operators() noexcept { return operators_; }
    [[nodiscard]] const OperatorRegistry& operators() const noexcept { return operators_; }

    /// Check if an operator is AC, consulting the operator registry
    [[nodiscard]] bool isAC(const std::string& op) const {
        return operators_.isAC(op);
    }

    /// Check if an operator is AC **for a given sort**.
    /// Multiplication is AC for Real/Complex but only associative for
    /// Quaternion/Octonion (not commutative).
    /// Generic sort does NOT get commutativity  it's the catch-all default
    /// and must be safe for non-commutative algebras.
    [[nodiscard]] bool isACForSort(const std::string& op, Sort sort) const {
        auto* info = operators_.lookup(op);
        if (!info) return false;
        if (!info->isAssociative) return false;

        // Gate commutativity on sort for mul/*
        if ((op == "*" || op == "mul") && !info->isCommutative) {
            // Explicitly commutative ONLY for Real and Complex
            // Generic is NOT included: it must be safe for non-commutative algebras
            return sort == Sort::Real || sort == Sort::Complex;
        }
        return info->isCommutative;
    }

private:
    TermFactory factory_;
    OperatorRegistry operators_;
};

// ===========================================================================
// GLOBAL CONTEXT
// ===========================================================================

/**
 * @brief The global context  use this unless you have a good reason not to
 * 
 * Thread-safe: TermFactory already uses stable pointers; the OperatorRegistry
 * is read-only after startup (registerBuiltins fills it, users may extend
 * during initialization only).
 */
inline Context& globalContext() {
    static Context ctx;
    return ctx;
}

} // namespace core
} // namespace autodiscover

#endif // AUTODISCOVER_CORE_CONTEXT_HPP
