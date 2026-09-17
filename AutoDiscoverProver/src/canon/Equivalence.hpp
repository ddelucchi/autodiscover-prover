#ifndef AUTODISCOVER_CANON_EQUIVALENCE_HPP
#define AUTODISCOVER_CANON_EQUIVALENCE_HPP

/**
 * @file Equivalence.hpp
 * @brief Configurable equivalence stack for canonical normal forms
 * 
 * Mathematical Foundation:
 * ========================
 * 
 * The equivalence stack defines a chain of quotients:
 * 
 *   T(,X)  _  _{AC}  _{ring}  _{poly}  _{dim}  _{tensor}
 * 
 * Each equivalence relation induces a normal form:
 * 
 *   NF_k : T/_{k-1}  T/_k
 * 
 * The total canonicalizer is the composition:
 * 
 *   NF = NF_k  NF_{k-1}  ...  NF_1
 * 
 * Equivalence Layers:
 * -------------------
 * 
 * 1. -equivalence (_): De Bruijn indices, canonical variable naming
 *    - Variables renamed to x, x, x, ... in DFS order
 *    - x.y.x+y _ a.b.a+b
 * 
 * 2. AC-equivalence (_{AC}): Associative-Commutative operators
 *    - Flatten nested AC applications: (a+b)+c  a+b+c (flattened)
 *    - Sort arguments canonically: b+a+c  a+b+c
 *    - AC operators: +, *, , , , , gcd, lcm
 * 
 * 3. Ring equivalence (_{ring}): Ring axioms modulo
 *    - a + 0  a, a * 1  a, a * 0  0
 *    - -(-a)  a, a + (-a)  0
 *    - Distribution: a*(b+c)  a*b + a*c (optional)
 * 
 * 4. Polynomial equivalence (_{poly}): Polynomial normal form
 *    - Collect like terms: ax + bx  (a+b)x
 *    - Power reduction:   +1
 *    - Canonical monomial ordering (grlex, grevlex, etc.)
 * 
 * 5. Dimensional equivalence (_{dim}): Physical dimension constraints
 *    - [a+b] requires [a] = [b]
 *    - Dimensionally incompatible terms are 
 * 
 * 6. Tensor equivalence (_{tensor}): Tensor/index notation
 *    - Einstein summation convention
 *    - Index renaming, contraction rules
 * 
 * Profile Configuration:
 * ----------------------
 * 
 * An EquivalenceProfile  = (layers, options) specifies:
 *   - Which equivalence layers are active
 *   - Layer-specific options (e.g., which operators are AC)
 *   - Termination guarantees
 * 
 * Invariants:
 * -----------
 * 
 * For each layer NF_k:
 *   - NF_k(NF_k(t)) = NF_k(t)        [idempotence]
 *   - t _k u  NF_k(t) = NF_k(u)   [completeness]  
 *   - NF_k(t) = NF_k(u)  t _k u   [soundness]
 * 
 * @author AutoDiscover Prover
 * @date 2024
 */

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <memory>
#include <optional>
#include <bitset>
#include <variant>
#include <stdexcept>

#include "../core/Term.hpp"

namespace autodiscover {
namespace canon {

// Create namespace alias to access core types correctly from within canon namespace
namespace core = ::autodiscover::core;

// ===========================================================================
// NUMBER SYSTEM ENUM
// ===========================================================================

/**
 * @brief Number system / algebraic structure
 * 
 * Gates certain normalization passes:
 * - PHI pass is only sound when the coefficient ring is [] or 
 * - AC-commutativity of mul is only valid for REAL/COMPLEX (not QUATERNION/OCTONION)
 */
enum class NumberSystem : uint8_t {
    REAL       = 0,   //   commutative field
    RATIONAL   = 1,   //   commutative field
    ZPHI       = 2,   // [] = [(1+5)/2]  commutative ring
    COMPLEX    = 3,   //   commutative field
    QUATERNION = 4,   //   non-commutative division algebra
    OCTONION   = 5,   //   non-commutative, non-associative
    SEDENION   = 6,   //   non-commutative, non-associative, zero divisors
    GENERIC    = 255  // Unknown / polymorphic
};

inline const char* numberSystemName(NumberSystem ns) {
    switch (ns) {
        case NumberSystem::REAL:       return "Real";
        case NumberSystem::RATIONAL:   return "Rational";
        case NumberSystem::ZPHI:       return "ZPhi";
        case NumberSystem::COMPLEX:    return "Complex";
        case NumberSystem::QUATERNION: return "Quaternion";
        case NumberSystem::OCTONION:   return "Octonion";
        case NumberSystem::SEDENION:   return "Sedenion";
        case NumberSystem::GENERIC:    return "Generic";
        default: return "Unknown";
    }
}

/// Is multiplication commutative in this number system?
inline bool isMulCommutative(NumberSystem ns) {
    switch (ns) {
        case NumberSystem::REAL:
        case NumberSystem::RATIONAL:
        case NumberSystem::ZPHI:
        case NumberSystem::COMPLEX:
            return true;
        default:
            return false;
    }
}

/// Is multiplication associative in this number system?
inline bool isMulAssociative(NumberSystem ns) {
    switch (ns) {
        case NumberSystem::REAL:
        case NumberSystem::RATIONAL:
        case NumberSystem::ZPHI:
        case NumberSystem::COMPLEX:
        case NumberSystem::QUATERNION:
            return true;
        default:
            return false; // Octonion, Sedenion are non-associative
    }
}

/// Does this number system support +1 rewriting?
inline bool supportsPhiReduction(NumberSystem ns) {
    switch (ns) {
        case NumberSystem::REAL:
        case NumberSystem::ZPHI:
        case NumberSystem::COMPLEX:
        case NumberSystem::GENERIC:
            return true;
        default:
            return false;
    }
}

// ===========================================================================
// EQUIVALENCE LAYER ENUM
// ===========================================================================

/**
 * @brief Enumeration of equivalence layers
 */
enum class EquivalenceLayer : uint8_t {
    ALPHA    = 0,  // -equivalence (de Bruijn renaming)
    AC       = 1,  // Associative-commutative
    RING     = 2,  // Ring axioms
    POLY     = 3,  // Polynomial normal form
    DIM      = 4,  // Dimensional analysis
    TENSOR   = 5,  // Tensor/index notation
    PHI      = 6,  // -ring:   +1
    GOD      = 7,  // GOD orbit minimization
    
    NUM_LAYERS = 8
};

inline const char* layerName(EquivalenceLayer layer) {
    switch (layer) {
        case EquivalenceLayer::ALPHA:  return "Alpha";
        case EquivalenceLayer::AC:     return "AC";
        case EquivalenceLayer::RING:   return "Ring";
        case EquivalenceLayer::POLY:   return "Poly";
        case EquivalenceLayer::DIM:    return "Dim";
        case EquivalenceLayer::TENSOR: return "Tensor";
        case EquivalenceLayer::PHI:    return "Phi";
        case EquivalenceLayer::GOD:    return "GOD";
        default: return "Unknown";
    }
}

// ===========================================================================
// LAYER OPTIONS
// ===========================================================================

/**
 * @brief Options for -equivalence layer
 */
struct AlphaOptions {
    std::string varPrefix = "x";   // Prefix for canonical variables
    bool preserveTypes = true;     // Keep type annotations during renaming
};

/**
 * @brief Options for AC-equivalence layer
 */
struct ACOptions {
    /// Operators that are ALWAYS associative-commutative regardless of sort.
    /// CRITICAL: "mul"/"*" are NOT listed here because they are only
    /// commutative for Real/Complex  for Quaternion/Octonion they are
    /// associative but NOT commutative.  Commutativity of mul is gated
    /// by NumberSystem via EquivalenceProfile::isMulCommutative().
    std::unordered_set<std::string> acOperators = {
        "+", "add", "union", "intersect", 
        "join", "meet", "gcd", "lcm", "max", "min"
    };
    
    /// Operators that are associative only (not commutative).
    /// mul/"*" are associative for , ,  (but NOT for ).
    /// Associativity of mul is gated by NumberSystem.
    std::unordered_set<std::string> assocOnlyOperators = {
        "*", "mul"
    };
    
    // Comparison function for sorting (default: lexicographic on canonical bytes)
    enum class SortOrder {
        LEXICOGRAPHIC,    // Bytecode lexicographic
        SHORTLEX,         // Length first, then lexicographic
        WEIGHT,           // By term weight/complexity
        CUSTOM            // User-provided comparator
    };
    SortOrder sortOrder = SortOrder::SHORTLEX;
    
    bool isAC(const std::string& op) const {
        return acOperators.count(op) > 0;
    }
    
    /// Check if op is associative-only (flatten args but DON'T sort)
    bool isAssocOnly(const std::string& op) const {
        return assocOnlyOperators.count(op) > 0;
    }
    
    /// Check if op should be treated as AC for a given number system.
    /// mul/"*" are upgraded to AC when the number system is commutative.
    bool isACForNumberSystem(const std::string& op, NumberSystem ns) const {
        if (acOperators.count(op) > 0) return true;
        if (assocOnlyOperators.count(op) > 0 && canon::isMulCommutative(ns)) return true;
        return false;
    }
};

/**
 * @brief Options for ring equivalence layer
 */
struct RingOptions {
    bool simplifyZero = true;       // a + 0  a, a * 0  0
    bool simplifyOne = true;        // a * 1  a
    bool simplifyNegation = true;   // -(-a)  a
    bool simplifyInverse = true;    // 1/(1/a)  a
    bool expandDistribution = false;// a*(b+c)  a*b + a*c (may increase size)
    bool collectTerms = true;       // a*x + b*x  (a+b)*x
};

/**
 * @brief Options for polynomial equivalence layer
 */
struct PolyOptions {
    enum class MonomialOrder {
        LEX,      // Lexicographic
        GRLEX,    // Graded lexicographic
        GREVLEX,  // Graded reverse lexicographic
        CUSTOM
    };
    MonomialOrder order = MonomialOrder::GRLEX;
    
    bool collectLikeTerms = true;
    bool sortMonomials = true;
    bool reducePhiSquared = true;  //   +1
};

/**
 * @brief Options for dimensional equivalence layer
 */
struct DimOptions {
    bool enforceAddition = true;    // [a+b] requires [a]=[b]
    bool propagateDimensions = true;// Infer dimensions through expressions
    bool rejectIncompatible = true; // Error on dimension mismatch (vs. mark as )
};

/**
 * @brief Options for tensor/index equivalence layer
 */
struct TensorOptions {
    bool canonicalizeIndices = true;   // Rename indices canonically
    bool contractDummy = true;         // Perform Einstein summation
    bool sortIndices = true;           // Sort symmetric indices
};

/**
 * @brief Options for -ring layer
 */
struct PhiOptions {
    bool reducePhiSquared = true;   //   +1
    bool normalizeZeckendorf = true;// 011  100 in Zeckendorf representation
    int maxPhiPower = 100;          // Maximum power to reduce
};

/**
 * @brief Options for GOD (Galois Orbit Descent) layer
 */
struct GODOptions {
    size_t maxOrbitSize = 16;       // Maximum orbit to enumerate (tiny:  is order-2)
    bool useHashing = true;         // Use hash-based orbit detection
    bool cacheOrbits = true;        // Cache computed orbits
};

// ===========================================================================
// LAYER OPTIONS VARIANT
// ===========================================================================

using LayerOptions = std::variant<
    AlphaOptions,
    ACOptions,
    RingOptions,
    PolyOptions,
    DimOptions,
    TensorOptions,
    PhiOptions,
    GODOptions
>;

inline LayerOptions defaultOptions(EquivalenceLayer layer) {
    switch (layer) {
        case EquivalenceLayer::ALPHA:  return AlphaOptions{};
        case EquivalenceLayer::AC:     return ACOptions{};
        case EquivalenceLayer::RING:   return RingOptions{};
        case EquivalenceLayer::POLY:   return PolyOptions{};
        case EquivalenceLayer::DIM:    return DimOptions{};
        case EquivalenceLayer::TENSOR: return TensorOptions{};
        case EquivalenceLayer::PHI:    return PhiOptions{};
        case EquivalenceLayer::GOD:    return GODOptions{};
        default: throw std::runtime_error("Unknown layer");
    }
}

// ===========================================================================
// EQUIVALENCE PROFILE
// ===========================================================================

/**
 * @brief A complete configuration of the equivalence stack
 * 
 * Specifies which layers are active and their options.
 */
class EquivalenceProfile {
public:
    using LayerSet = std::bitset<static_cast<size_t>(EquivalenceLayer::NUM_LAYERS)>;
    
private:
    std::string name_;
    LayerSet activeLayers_;
    std::unordered_map<EquivalenceLayer, LayerOptions> options_;
    std::vector<EquivalenceLayer> order_;  // Processing order
    NumberSystem numberSystem_ = NumberSystem::GENERIC;  // Algebraic structure
    
public:
    // -----------------------------------------------------------------------
    // CONSTRUCTORS
    // -----------------------------------------------------------------------
    
    EquivalenceProfile() : name_("empty") {}
    
    explicit EquivalenceProfile(std::string name) : name_(std::move(name)) {}
    
    // -----------------------------------------------------------------------
    // BUILDER PATTERN
    // -----------------------------------------------------------------------
    
    EquivalenceProfile& named(std::string name) {
        name_ = std::move(name);
        return *this;
    }
    
    EquivalenceProfile& enable(EquivalenceLayer layer) {
        activeLayers_.set(static_cast<size_t>(layer));
        if (options_.find(layer) == options_.end()) {
            options_[layer] = defaultOptions(layer);
        }
        order_.push_back(layer);
        return *this;
    }
    
    EquivalenceProfile& disable(EquivalenceLayer layer) {
        activeLayers_.reset(static_cast<size_t>(layer));
        order_.erase(
            std::remove(order_.begin(), order_.end(), layer),
            order_.end()
        );
        return *this;
    }
    
    template<typename OptT>
    EquivalenceProfile& withOptions(EquivalenceLayer layer, OptT opts) {
        options_[layer] = std::move(opts);
        return *this;
    }
    
    EquivalenceProfile& setOrder(std::vector<EquivalenceLayer> ord) {
        order_ = std::move(ord);
        // Enable all layers in order
        for (auto layer : order_) {
            activeLayers_.set(static_cast<size_t>(layer));
            if (options_.find(layer) == options_.end()) {
                options_[layer] = defaultOptions(layer);
            }
        }
        return *this;
    }
    
    EquivalenceProfile& withNumberSystem(NumberSystem ns) {
        numberSystem_ = ns;
        return *this;
    }
    
    // -----------------------------------------------------------------------
    // ACCESSORS
    // -----------------------------------------------------------------------
    
    [[nodiscard]] const std::string& name() const { return name_; }
    
    [[nodiscard]] bool isEnabled(EquivalenceLayer layer) const {
        return activeLayers_.test(static_cast<size_t>(layer));
    }
    
    [[nodiscard]] const std::vector<EquivalenceLayer>& order() const { 
        return order_; 
    }
    
    [[nodiscard]] size_t numActiveLayers() const {
        return activeLayers_.count();
    }
    
    [[nodiscard]] NumberSystem numberSystem() const {
        return numberSystem_;
    }
    
    /// Is mul commutative in this profile's number system?
    [[nodiscard]] bool isMulCommutative() const {
        return canon::isMulCommutative(numberSystem_);
    }
    
    template<typename OptT>
    [[nodiscard]] const OptT& getOptions(EquivalenceLayer layer) const {
        auto it = options_.find(layer);
        if (it == options_.end()) {
            throw std::runtime_error("No options for layer: " + std::string(layerName(layer)));
        }
        return std::get<OptT>(it->second);
    }
    
    template<typename OptT>
    [[nodiscard]] OptT& getOptions(EquivalenceLayer layer) {
        auto it = options_.find(layer);
        if (it == options_.end()) {
            options_[layer] = OptT{};
            it = options_.find(layer);
        }
        return std::get<OptT>(it->second);
    }
    
    // -----------------------------------------------------------------------
    // PREDEFINED PROFILES
    // -----------------------------------------------------------------------
    
    /**
     * @brief Minimal profile: only -renaming
     */
    static EquivalenceProfile minimal() {
        return EquivalenceProfile("minimal")
            .enable(EquivalenceLayer::ALPHA);
    }
    
    /**
     * @brief Standard profile:  + AC + Ring
     */
    static EquivalenceProfile standard() {
        return EquivalenceProfile("standard")
            .setOrder({
                EquivalenceLayer::ALPHA,
                EquivalenceLayer::AC,
                EquivalenceLayer::RING
            });
    }
    
    /**
     * @brief Polynomial profile: standard + polynomial normalization
     */
    static EquivalenceProfile polynomial() {
        return EquivalenceProfile("polynomial")
            .setOrder({
                EquivalenceLayer::ALPHA,
                EquivalenceLayer::AC,
                EquivalenceLayer::RING,
                EquivalenceLayer::POLY
            });
    }
    
    /**
     * @brief Golden profile: standard + -ring
     */
    static EquivalenceProfile golden() {
        return EquivalenceProfile("golden")
            .setOrder({
                EquivalenceLayer::ALPHA,
                EquivalenceLayer::AC,
                EquivalenceLayer::RING,
                EquivalenceLayer::PHI
            });
    }
    
    /**
     * @brief Full profile: all layers enabled
     */
    static EquivalenceProfile full() {
        return EquivalenceProfile("full")
            .setOrder({
                EquivalenceLayer::ALPHA,
                EquivalenceLayer::AC,
                EquivalenceLayer::RING,
                EquivalenceLayer::PHI,
                EquivalenceLayer::POLY,
                EquivalenceLayer::DIM,
                EquivalenceLayer::GOD
            });
    }
    
    /**
     * @brief GOD-minimal: just  + GOD for orbit minimization
     */
    static EquivalenceProfile godMinimal() {
        return EquivalenceProfile("god-minimal")
            .setOrder({
                EquivalenceLayer::ALPHA,
                EquivalenceLayer::GOD
            });
    }
    
    /**
     * @brief Dimensional analysis profile
     */
    static EquivalenceProfile dimensional() {
        return EquivalenceProfile("dimensional")
            .setOrder({
                EquivalenceLayer::ALPHA,
                EquivalenceLayer::AC,
                EquivalenceLayer::DIM
            });
    }
    
    // -----------------------------------------------------------------------
    // SERIALIZATION
    // -----------------------------------------------------------------------
    
    [[nodiscard]] std::string toString() const {
        std::string result = "Profile[" + name_ + "]: ";
        bool first = true;
        for (auto layer : order_) {
            if (!first) result += "  ";
            result += layerName(layer);
            first = false;
        }
        return result;
    }
    
    [[nodiscard]] size_t hash() const {
        size_t h = std::hash<std::string>{}(name_);
        h ^= activeLayers_.to_ulong() + 0x9e3779b9 + (h << 6) + (h >> 2);
        for (auto layer : order_) {
            h ^= static_cast<size_t>(layer) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};

// ===========================================================================
// NORMALIZATION PASS INTERFACE
// ===========================================================================

// Use core::Term from Term.hpp (included at top)

/**
 * @brief Abstract interface for a single normalization pass
 * 
 * Each pass implements one equivalence layer's normal form computation.
 */
class NormalizationPass {
public:
    virtual ~NormalizationPass() = default;
    
    /// Name of this pass
    [[nodiscard]] virtual std::string name() const = 0;
    
    /// Which equivalence layer this implements
    [[nodiscard]] virtual EquivalenceLayer layer() const = 0;
    
    /// Apply the normalization (returns normalized term, raw-pointer API)
    [[nodiscard]] virtual const core::Term* 
    normalize(const core::Term* term, core::TermFactory& factory) = 0;
    
    /// Check if term is already in normal form for this layer
    [[nodiscard]] virtual bool isNormal(const core::Term* term) const = 0;
    
    /// Estimated cost/complexity of this pass
    [[nodiscard]] virtual size_t estimatedCost() const { return 1; }
    
    /// Does this pass always terminate?
    [[nodiscard]] virtual bool isTerminating() const { return true; }
};

// ===========================================================================
// NORMALIZATION PIPELINE
// ===========================================================================

/**
 * @brief Composed normalization pipeline: NF = NF_k  ...  NF_1
 * 
 * Executes normalization passes in order according to an EquivalenceProfile.
 */
class NormalizationPipeline {
private:
    EquivalenceProfile profile_;
    std::vector<std::unique_ptr<NormalizationPass>> passes_;
    
    // Statistics
    mutable size_t totalCalls_ = 0;
    mutable size_t passInvocations_ = 0;
    mutable size_t cacheHits_ = 0;
    
public:
    explicit NormalizationPipeline(EquivalenceProfile profile)
        : profile_(std::move(profile)) {}
    
    /// Add a normalization pass
    void addPass(std::unique_ptr<NormalizationPass> pass) {
        passes_.push_back(std::move(pass));
    }
    
    /// Build standard passes for the profile
    void buildFromProfile();  // Implementation in NF engine
    
    /// Apply the full pipeline (raw-pointer API  uses hash-consed terms)
    [[nodiscard]] const core::Term* 
    normalize(const core::Term* term, core::TermFactory& factory) {
        ++totalCalls_;
        
        for (const auto& pass : passes_) {
            if (profile_.isEnabled(pass->layer())) {
                ++passInvocations_;
                term = pass->normalize(term, factory);
            }
        }
        return term;
    }
    
    /// Check if term is fully normalized
    [[nodiscard]] bool isFullyNormal(const core::Term* term) const {
        for (const auto& pass : passes_) {
            if (profile_.isEnabled(pass->layer()) && !pass->isNormal(term)) {
                return false;
            }
        }
        return true;
    }
    
    /// Get the profile
    [[nodiscard]] const EquivalenceProfile& profile() const { return profile_; }
    
    /// Get statistics
    [[nodiscard]] size_t totalCalls() const { return totalCalls_; }
    [[nodiscard]] size_t passInvocations() const { return passInvocations_; }
    [[nodiscard]] double avgPassesPerCall() const { 
        return totalCalls_ == 0 ? 0.0 : static_cast<double>(passInvocations_) / totalCalls_;
    }
    
    void resetStats() {
        totalCalls_ = 0;
        passInvocations_ = 0;
        cacheHits_ = 0;
    }
};

// ===========================================================================
// EQUIVALENCE RELATION INTERFACE  
// ===========================================================================

/**
 * @brief Abstract interface for an equivalence relation
 */
class EquivalenceRelation {
public:
    virtual ~EquivalenceRelation() = default;
    
    /// Check if two terms are equivalent under this relation
    [[nodiscard]] virtual bool areEquivalent(
        const core::Term* t1,
        const core::Term* t2
    ) const = 0;
    
    /// Get the canonical representative for an equivalence class
    [[nodiscard]] virtual const core::Term*
    representative(const core::Term* term, core::TermFactory& factory) const = 0;
    
    /// Name of this equivalence relation
    [[nodiscard]] virtual std::string name() const = 0;
};

// ===========================================================================
// EQUIVALENCE STACK
// ===========================================================================

/**
 * @brief Stack of equivalence relations with quotient chain
 * 
 * Implements: T(,X)//.../_k
 */
class EquivalenceStack {
private:
    std::vector<std::unique_ptr<EquivalenceRelation>> relations_;
    EquivalenceProfile profile_;
    
public:
    explicit EquivalenceStack(EquivalenceProfile profile) 
        : profile_(std::move(profile)) {}
    
    void pushRelation(std::unique_ptr<EquivalenceRelation> rel) {
        relations_.push_back(std::move(rel));
    }
    
    /// Check equivalence through full stack
    [[nodiscard]] bool areEquivalent(
        const core::Term* t1,
        const core::Term* t2,
        core::TermFactory& factory
    ) const {
        const core::Term* r1 = representative(t1, factory);
        const core::Term* r2 = representative(t2, factory);
        // After normalization, pointer equality suffices (hash-consed)
        return r1->id() == r2->id();
    }
    
    /// Get canonical representative through full stack
    [[nodiscard]] const core::Term*
    representative(const core::Term* term, core::TermFactory& factory) const {
        for (const auto& rel : relations_) {
            term = rel->representative(term, factory);
        }
        return term;
    }
    
    [[nodiscard]] const EquivalenceProfile& profile() const { return profile_; }
    [[nodiscard]] size_t depth() const { return relations_.size(); }
};

// ===========================================================================
// PROFILE REGISTRY  
// ===========================================================================

/**
 * @brief Registry of named equivalence profiles
 */
class ProfileRegistry {
private:
    std::unordered_map<std::string, EquivalenceProfile> profiles_;
    
public:
    ProfileRegistry() {
        // Register built-in profiles
        registerProfile(EquivalenceProfile::minimal());
        registerProfile(EquivalenceProfile::standard());
        registerProfile(EquivalenceProfile::polynomial());
        registerProfile(EquivalenceProfile::golden());
        registerProfile(EquivalenceProfile::full());
        registerProfile(EquivalenceProfile::godMinimal());
        registerProfile(EquivalenceProfile::dimensional());
    }
    
    void registerProfile(EquivalenceProfile profile) {
        profiles_[profile.name()] = std::move(profile);
    }
    
    [[nodiscard]] std::optional<EquivalenceProfile> get(const std::string& name) const {
        auto it = profiles_.find(name);
        if (it != profiles_.end()) return it->second;
        return std::nullopt;
    }
    
    [[nodiscard]] std::vector<std::string> listProfiles() const {
        std::vector<std::string> result;
        for (const auto& [name, _] : profiles_) {
            result.push_back(name);
        }
        return result;
    }
    
    static ProfileRegistry& global() {
        static ProfileRegistry instance;
        return instance;
    }
};

} // namespace canon
} // namespace autodiscover

#endif // AUTODISCOVER_CANON_EQUIVALENCE_HPP
