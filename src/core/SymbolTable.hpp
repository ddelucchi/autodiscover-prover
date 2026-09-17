/**
 * @file SymbolTable.hpp
 * @brief Symbol management for variables, constants, and operations
 * 
 * Manages the symbol namespace for the theorem prover:
 * - Variable symbols (universally quantified)
 * - Constant symbols (ground terms)
 * - Function symbols with arity and type
 * - Built-in Cayley-Dickson operations
 */

#ifndef AUTODISCOVER_CORE_SYMBOLTABLE_HPP
#define AUTODISCOVER_CORE_SYMBOLTABLE_HPP

#include "Type.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <memory>

namespace autodiscover {
namespace core {

/**
 * @brief Symbol classification
 */
enum class SymbolKind : uint8_t {
    Variable,       // Universally quantified variable
    Constant,       // Ground constant symbol (, e, i, j, k, etc.)
    Function,       // Function symbol with arity
    Builtin,        // Built-in operation (add, mul, conj, etc.)
};

/**
 * @brief Built-in operation identifiers
 */
enum class BuiltinOp : uint16_t {
    // Core algebra operations
    Add,            // x + y
    Sub,            // x - y
    Mul,            // x  y (Cayley-Dickson multiplication)
    Neg,            // -x
    Conj,           // x* (conjugation)
    Inv,            // x (multiplicative inverse for division algebras)
    
    // Scalar operations
    Norm,           // |x| = x  x*
    ScalarPart,     // Scal(x) = Re(x*) for octonion or real part
    VectorPart,     // Vec(x) = x - Scal(x)
    
    // Golden ratio operations
    Phi,            //  = (1 + 5)/2
    PhiBar,         //  = 1   (Galois conjugate)
    FibStep,        // R(z) = 1 + 1/z (Fibonacci recursion)
    
    // Cayley-Dickson construction
    Pair,           // (a, b) : A_n  A_n  A_{n+1}
    Fst,            // (a,b) = a
    Snd,            // (a,b) = b
    J,              // J = (0,1) imaginary unit at each level
    
    // SCOUT operations
    Align,          // Align_U(q) = Scal(U*  q)
    PhaseTransport, // _{j+1} = _j  U_j
    
    // GOD canonicalization
    GOD,            // GOD() = min_{_Enc}(Orbit())
    Encode,         // Enc(t) : shortlex encoding
    
    // Comparison
    Eq,             // x = y
    Lt,             // x < y (for scalar ordering)
};

/**
 * @brief Symbol metadata
 */
struct Symbol {
    std::string name;
    SymbolKind kind;
    TypePtr type;
    uint32_t arity;
    std::optional<BuiltinOp> builtin;
    
    Symbol(std::string n, SymbolKind k, TypePtr t, uint32_t a = 0,
           std::optional<BuiltinOp> b = std::nullopt)
        : name(std::move(n)), kind(k), type(std::move(t)), arity(a), builtin(b) {}
};

/**
 * @brief Symbol table for managing all symbols in the prover
 */
class SymbolTable {
public:
    SymbolTable() {
        initBuiltins();
    }
    
    /**
     * @brief Declare a new variable
     */
    Symbol& declareVariable(const std::string& name, TypePtr type) {
        return addSymbol(name, SymbolKind::Variable, std::move(type), 0);
    }
    
    /**
     * @brief Declare a new constant
     */
    Symbol& declareConstant(const std::string& name, TypePtr type) {
        return addSymbol(name, SymbolKind::Constant, std::move(type), 0);
    }
    
    /**
     * @brief Declare a new function symbol
     */
    Symbol& declareFunction(const std::string& name, TypePtr type, uint32_t arity) {
        return addSymbol(name, SymbolKind::Function, std::move(type), arity);
    }
    
    /**
     * @brief Look up a symbol by name
     */
    [[nodiscard]] std::optional<Symbol*> lookup(const std::string& name) {
        auto it = symbols_.find(name);
        if (it != symbols_.end()) {
            return &it->second;
        }
        return std::nullopt;
    }
    
    [[nodiscard]] std::optional<const Symbol*> lookup(const std::string& name) const {
        auto it = symbols_.find(name);
        if (it != symbols_.end()) {
            return &it->second;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Get builtin operation symbol
     */
    [[nodiscard]] const Symbol& getBuiltin(BuiltinOp op) const {
        return builtins_.at(static_cast<size_t>(op));
    }
    
    /**
     * @brief Check if a symbol exists
     */
    [[nodiscard]] bool exists(const std::string& name) const {
        return symbols_.find(name) != symbols_.end();
    }
    
    /**
     * @brief Generate a fresh variable name
     */
    [[nodiscard]] std::string freshVar(const std::string& prefix = "x") {
        std::string name;
        do {
            name = prefix + std::to_string(varCounter_++);
        } while (exists(name));
        return name;
    }
    
    /**
     * @brief Get all symbols of a given kind
     */
    [[nodiscard]] std::vector<const Symbol*> symbolsOfKind(SymbolKind kind) const {
        std::vector<const Symbol*> result;
        for (const auto& [name, sym] : symbols_) {
            if (sym.kind == kind) {
                result.push_back(&sym);
            }
        }
        return result;
    }

private:
    std::unordered_map<std::string, Symbol> symbols_;
    std::vector<Symbol> builtins_;
    uint32_t varCounter_ = 0;
    
    Symbol& addSymbol(const std::string& name, SymbolKind kind, TypePtr type, uint32_t arity,
                      std::optional<BuiltinOp> builtin = std::nullopt) {
        auto [it, inserted] = symbols_.emplace(
            name, Symbol(name, kind, std::move(type), arity, builtin));
        return it->second;
    }
    
    void initBuiltins() {
        // Resize to hold all builtins
        builtins_.reserve(static_cast<size_t>(BuiltinOp::Lt) + 1);
        
        auto R = TypeFactory::Real();
        auto C = TypeFactory::Complex();
        auto Q = TypeFactory::Quaternion();
        auto O = TypeFactory::Octonion();
        auto tau = TypeFactory::Var(0);
        
        // Binary operations: tau -> tau -> tau
        auto binOp = TypeFactory::Function(tau, TypeFactory::Function(tau, tau));
        // Unary operations: tau -> tau
        auto unOp = TypeFactory::Function(tau, tau);
        // Scalar operations: tau -> Real
        auto scalarOp = TypeFactory::Function(tau, R);
        
        auto addBuiltin = [this](BuiltinOp op, const std::string& name, TypePtr type, uint32_t arity) {
            builtins_.emplace_back(name, SymbolKind::Builtin, std::move(type), arity, op);
            symbols_.emplace(name, builtins_.back());
        };
        
        // Core algebra operations  internal names are ASCII, matching TermFactory
        addBuiltin(BuiltinOp::Add, "+", binOp, 2);
        addBuiltin(BuiltinOp::Sub, "-", binOp, 2);
        addBuiltin(BuiltinOp::Mul, "*", binOp, 2);
        addBuiltin(BuiltinOp::Neg, "neg", unOp, 1);
        addBuiltin(BuiltinOp::Conj, "conj", unOp, 1);
        addBuiltin(BuiltinOp::Inv, "inv", unOp, 1);
        
        // Scalar operations
        addBuiltin(BuiltinOp::Norm, "norm", scalarOp, 1);
        addBuiltin(BuiltinOp::ScalarPart, "Scal", scalarOp, 1);
        addBuiltin(BuiltinOp::VectorPart, "Vec", unOp, 1);
        
        // Golden ratio operations  ASCII internal names
        addBuiltin(BuiltinOp::Phi, "phi", R, 0);
        addBuiltin(BuiltinOp::PhiBar, "phi_bar", R, 0);
        addBuiltin(BuiltinOp::FibStep, "FibStep", TypeFactory::Function(R, R), 1);
        
        // Cayley-Dickson construction
        auto pairType = TypeFactory::Function(tau, TypeFactory::Function(tau, tau));
        addBuiltin(BuiltinOp::Pair, "pair", pairType, 2);
        addBuiltin(BuiltinOp::Fst, "fst", unOp, 1);
        addBuiltin(BuiltinOp::Snd, "snd", unOp, 1);
        addBuiltin(BuiltinOp::J, "J", tau, 0);
        
        // SCOUT operations
        auto alignType = TypeFactory::Function(tau, TypeFactory::Function(tau, R));
        addBuiltin(BuiltinOp::Align, "Align", alignType, 2);
        addBuiltin(BuiltinOp::PhaseTransport, "PhaseTransport", binOp, 2);
        
        // GOD canonicalization
        addBuiltin(BuiltinOp::GOD, "GOD", unOp, 1);
        auto encodeType = TypeFactory::Function(tau, TypeFactory::AtLevel(CDLevel::REAL));
        addBuiltin(BuiltinOp::Encode, "Enc", encodeType, 1);
        
        // Comparison
        auto boolType = TypeFactory::AtLevel(CDLevel::REAL); // Using real as bool for now
        addBuiltin(BuiltinOp::Eq, "=", TypeFactory::Function(tau, TypeFactory::Function(tau, boolType)), 2);
        addBuiltin(BuiltinOp::Lt, "<", TypeFactory::Function(R, TypeFactory::Function(R, boolType)), 2);
    }
};

/**
 * @brief Display name for builtin operations (may use Unicode for readability)
 * 
 * NOTE: These are for human-readable output ONLY. Internal symbol matching
 * must use the ASCII names registered in initBuiltins().
 */
inline std::string builtinName(BuiltinOp op) {
    switch (op) {
        case BuiltinOp::Add: return "+";
        case BuiltinOp::Sub: return "-";
        case BuiltinOp::Mul: return "*";
        case BuiltinOp::Neg: return "neg";
        case BuiltinOp::Conj: return "conj";
        case BuiltinOp::Inv: return "inv";
        case BuiltinOp::Norm: return "norm";
        case BuiltinOp::ScalarPart: return "Scal";
        case BuiltinOp::VectorPart: return "Vec";
        case BuiltinOp::Phi: return "phi";
        case BuiltinOp::PhiBar: return "phi_bar";
        case BuiltinOp::FibStep: return "FibStep";
        case BuiltinOp::Pair: return "pair";
        case BuiltinOp::Fst: return "fst";
        case BuiltinOp::Snd: return "snd";
        case BuiltinOp::J: return "J";
        case BuiltinOp::Align: return "Align";
        case BuiltinOp::PhaseTransport: return "PhaseTransport";
        case BuiltinOp::GOD: return "GOD";
        case BuiltinOp::Encode: return "Enc";
        case BuiltinOp::Eq: return "=";
        case BuiltinOp::Lt: return "<";
    }
    return "?";
}

} // namespace core
} // namespace autodiscover

#endif // AUTODISCOVER_CORE_SYMBOLTABLE_HPP
