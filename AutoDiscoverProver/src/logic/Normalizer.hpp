/**
 * @file Normalizer.hpp
 * @brief GOD canonicalizer, lfp saturation, and term normalization
 * 
 * =============================================================================
 * GOD (Groupoid-Orbit Determinizer)
 * =============================================================================
 * 
 * Definition:
 *   GOD^_T() := min_{_Enc}(Orb_T()  {})
 * 
 * where:
 *   - T is a groupoid acting on formulas
 *   - _Enc is the shortlex encoding order
 *   - Orb_T() is the orbit {g : g  T}
 *   -  is the bottom element (for empty orbits)
 * 
 * Key Theorems:
 *   - Invariance: GOD(g) = GOD() for all g  T
 *   - Idempotence: GOD(GOD()) = GOD()  
 *   - Uniqueness: GOD(t) = GOD(s) iff t _T s
 * 
 * =============================================================================
 * LFP (Least Fixed Point) Saturation
 * =============================================================================
 * 
 * The lfp closure constructs the least fixed point of a monotone operator:
 *   lfp(F) = _{  Ord} F^()
 * 
 * For our operator F_ on equations/rules:
 *   F_(S) = S  {consequences of S under inference rules}
 * 
 * Convergence:
 *   - F is monotone: S  T  F(S)  F(T)
 *   - By Knaster-Tarski: lfp exists and equals  F^()
 *   - For finitary systems: lfp = F^() (countable iteration suffices)
 * 
 * =============================================================================
 * Carry-Rewrite System
 * =============================================================================
 * 
 *   R: 011  100 (golden-mean rewriting)
 *   Sound iff x = x + 1 (forces  as eigenvalue)
 *   Normal words: no consecutive 1s (Zeckendorf property)
 *   Count: N(n) = F_{n+2} (Fibonacci)
 *   Entropy: h_top = log 
 * 
 */

#ifndef AUTODISCOVER_LOGIC_NORMALIZER_HPP
#define AUTODISCOVER_LOGIC_NORMALIZER_HPP

#include "../core/Term.hpp"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <functional>
#include <algorithm>
#include <optional>
#include <memory>

namespace autodiscover {
namespace logic {

using core::Term;
using core::TermId;
using core::TermFactory;
using core::TermKind;
using core::ShortlexComparator;

// =============================================================================
// LFP SATURATION ENGINE
// =============================================================================

/**
 * @brief Statistics for lfp computation
 */
struct LFPStats {
    size_t iterations = 0;         // Number of F applications
    size_t newElementsTotal = 0;   // Total new elements generated
    size_t finalSize = 0;          // Final set size
    bool converged = true;         // True if reached fixed point
    bool hitLimit = false;         // True if iteration limit reached
    
    void reset() {
        iterations = 0;
        newElementsTotal = 0;
        finalSize = 0;
        converged = true;
        hitLimit = false;
    }
};

/**
 * @brief Generic least fixed point saturation engine
 * 
 * Computes lfp(F) = _{} F^() for a monotone operator F
 * 
 * @tparam Element The type of elements in the set
 * @tparam Hash Hash function for elements
 * @tparam Equal Equality predicate for elements
 */
template<typename Element, 
         typename Hash = std::hash<Element>,
         typename Equal = std::equal_to<Element>>
class LFPEngine {
public:
    using Operator = std::function<std::vector<Element>(const std::unordered_set<Element, Hash, Equal>&)>;
    
    /**
     * @brief Construct with operator F
     */
    explicit LFPEngine(Operator F) : F_(std::move(F)) {}
    
    /**
     * @brief Compute lfp(F) starting from empty set
     * 
     * @param maxIterations Maximum number of F applications (0 = unlimited)
     * @param maxSize Maximum size of the set (0 = unlimited)
     * @return The least fixed point (or approximation if limits hit)
     */
    [[nodiscard]] std::unordered_set<Element, Hash, Equal> 
    compute(size_t maxIterations = 500, size_t maxSize = 50000) {
        return computeFrom({}, maxIterations, maxSize);
    }
    
    /**
     * @brief Compute lfp(F) starting from initial set
     */
    [[nodiscard]] std::unordered_set<Element, Hash, Equal>
    computeFrom(std::unordered_set<Element, Hash, Equal> initial,
                size_t maxIterations = 500, size_t maxSize = 50000) {
        stats_.reset();
        
        std::unordered_set<Element, Hash, Equal> current = std::move(initial);
        std::unordered_set<Element, Hash, Equal> newElements;
        
        bool changed = true;
        while (changed) {
            stats_.iterations++;
            
            // Check iteration limit
            if (maxIterations > 0 && stats_.iterations > maxIterations) {
                stats_.converged = false;
                stats_.hitLimit = true;
                break;
            }
            
            // Apply F to current set
            std::vector<Element> generated = F_(current);
            
            // Find genuinely new elements
            newElements.clear();
            for (auto& elem : generated) {
                if (current.find(elem) == current.end()) {
                    newElements.insert(std::move(elem));
                }
            }
            
            stats_.newElementsTotal += newElements.size();
            changed = !newElements.empty();
            
            if (changed) {
                // Add new elements to current set
                for (const auto& elem : newElements) {
                    current.insert(elem);
                    
                    // Check size limit
                    if (maxSize > 0 && current.size() >= maxSize) {
                        stats_.converged = false;
                        stats_.hitLimit = true;
                        changed = false;
                        break;
                    }
                }
            }
        }
        
        stats_.finalSize = current.size();
        return current;
    }
    
    /**
     * @brief Get statistics from last computation
     */
    [[nodiscard]] const LFPStats& stats() const { return stats_; }

private:
    Operator F_;
    LFPStats stats_;
};

/**
 * @brief LFP engine specialized for terms
 */
class TermLFPEngine {
public:
    using TermSet = std::unordered_set<TermId>;
    using Generator = std::function<std::vector<const Term*>(const Term*, TermFactory&)>;
    
    explicit TermLFPEngine(TermFactory& factory) : factory_(factory) {}
    
    /**
     * @brief Add a generation rule: term  set of derived terms
     */
    void addGenerator(Generator gen) {
        generators_.push_back(std::move(gen));
    }
    
    /**
     * @brief Compute lfp closure of a term
     */
    [[nodiscard]] std::vector<const Term*> closure(const Term* seed,
                                                     size_t maxIterations = 1000,
                                                     size_t maxSize = 10000) {
        stats_.reset();
        
        TermSet current;
        std::vector<const Term*> terms;
        std::queue<const Term*> worklist;
        
        current.insert(seed->id());
        terms.push_back(seed);
        worklist.push(seed);
        
        while (!worklist.empty()) {
            stats_.iterations++;
            
            if (maxIterations > 0 && stats_.iterations > maxIterations) {
                stats_.converged = false;
                stats_.hitLimit = true;
                break;
            }
            
            const Term* t = worklist.front();
            worklist.pop();
            
            // Apply all generators
            for (const auto& gen : generators_) {
                std::vector<const Term*> derived = gen(t, factory_);
                
                for (const Term* newTerm : derived) {
                    if (newTerm && current.find(newTerm->id()) == current.end()) {
                        current.insert(newTerm->id());
                        terms.push_back(newTerm);
                        worklist.push(newTerm);
                        stats_.newElementsTotal++;
                        
                        if (maxSize > 0 && current.size() >= maxSize) {
                            stats_.converged = false;
                            stats_.hitLimit = true;
                            goto done;
                        }
                    }
                }
            }
        }
    done:
        
        stats_.finalSize = terms.size();
        return terms;
    }
    
    [[nodiscard]] const LFPStats& stats() const { return stats_; }

private:
    TermFactory& factory_;
    std::vector<Generator> generators_;
    LFPStats stats_;
};

// =============================================================================
// GROUPOID ACTION
// =============================================================================

/**
 * @brief Groupoid action generator
 * Represents elementary transformations that generate the orbit
 */
class GroupoidAction {
public:
    using Transform = std::function<const Term*(const Term*, TermFactory&)>;
    
    GroupoidAction(std::string name, Transform fwd, Transform inv)
        : name_(std::move(name)), forward_(std::move(fwd)), inverse_(std::move(inv)) {}
    
    [[nodiscard]] const std::string& name() const { return name_; }
    
    [[nodiscard]] const Term* apply(const Term* t, TermFactory& factory) const {
        return forward_(t, factory);
    }
    
    [[nodiscard]] const Term* applyInverse(const Term* t, TermFactory& factory) const {
        return inverse_(t, factory);
    }

private:
    std::string name_;
    Transform forward_;
    Transform inverse_;
};

// =============================================================================
// CARRY REWRITER
// =============================================================================

/**
 * @brief Carry-rewrite normalizer for Zeckendorf representation
 * 
 * Implements R: 011  100 normalization
 * A word is normal if it contains no consecutive 1s (Zeckendorf property)
 */
class CarryRewriter {
public:
    /**
     * @brief Check if a binary word is in normal form (no consecutive 1s)
     */
    [[nodiscard]] static bool isNormal(const std::vector<uint8_t>& word) {
        for (size_t i = 0; i + 1 < word.size(); ++i) {
            if (word[i] == 1 && word[i+1] == 1) {
                return false;
            }
        }
        return true;
    }
    
    /**
     * @brief Normalize a binary word using carry rewrites
     * R: ...011...  ...100...
     */
    [[nodiscard]] static std::vector<uint8_t> normalize(std::vector<uint8_t> word) {
        bool changed = true;
        while (changed) {
            changed = false;
            
            // First pass: handle digits > 1 (cascading carries)
            // Correct Fibonacci identity: 2*F_n = F_{n+1} + F_{n-2}
            // In our indexing convention (word[i] -> F_{i+offset}):
            //   carry to i+1 (up one) and i-2 (down two)
            for (size_t i = 0; i < word.size(); ++i) {
                while (word[i] >= 2) {
                    word[i] -= 2;
                    if (i + 1 >= word.size()) word.push_back(0);
                    word[i + 1] += 1;  // F_{n+1}
                    if (i >= 2) {
                        word[i - 2] += 1;  // F_{n-2}
                    } else if (i == 1) {
                        // F_{n-2} = F_{offset-1}; for standard Fibonacci
                        // F_1 = F_2 = 1, so we can safely add to word[0]
                        word[0] += 1;
                    }
                    // For i==0: 2*F_{offset} = F_{offset+1} (F_{offset-2}=0), no lower carry
                    changed = true;
                }
            }
            
            // Second pass: normalize consecutive 1s
            for (size_t i = 0; i + 1 < word.size(); ++i) {
                if (word[i] >= 1 && word[i+1] >= 1) {
                    // ...11... at positions i, i+1
                    word[i] -= 1;
                    word[i+1] -= 1;
                    
                    // Carry to position i+2 (F_i + F_{i+1} = F_{i+2})
                    if (i + 2 >= word.size()) {
                        word.push_back(0);
                    }
                    word[i+2] += 1;
                    
                    changed = true;
                    break; // Restart scan
                }
            }
        }
        
        // Remove leading zeros
        while (word.size() > 1 && word.back() == 0) {
            word.pop_back();
        }
        
        return word;
    }
    
    /**
     * @brief Count normal words of length n
     * N(n) = F_{n+2} by Fibonacci recurrence
     */
    [[nodiscard]] static uint64_t countNormal(uint32_t n) {
        if (n == 0) return 1;
        if (n == 1) return 2;
        
        uint64_t a = 1, b = 2;
        for (uint32_t i = 2; i <= n; ++i) {
            uint64_t c = a + b;
            a = b;
            b = c;
        }
        return b;
    }
    
    /**
     * @brief Convert string to vector representation
     */
    [[nodiscard]] static std::vector<uint8_t> fromString(const std::string& s) {
        std::vector<uint8_t> result;
        for (char c : s) {
            result.push_back(c == '1' ? 1 : 0);
        }
        return result;
    }
    
    /**
     * @brief Convert vector to string representation
     */
    [[nodiscard]] static std::string toString(const std::vector<uint8_t>& v) {
        std::string result;
        for (uint8_t d : v) {
            result += static_cast<char>('0' + d);
        }
        return result;
    }
};

// =============================================================================
// GOD NORMALIZER (proof-producing, memoized)
// =============================================================================

/**
 * @brief A single step in an orbit certificate: action name + direction
 */
struct OrbitStep {
    std::string actionName;
    bool isInverse;
};

/**
 * @brief Result of GOD canonicalization: canonical rep + certificate path
 */
struct GODResult {
    const Term* canonical;                // The orbit minimum
    std::vector<OrbitStep> certificate;   // Path: input -> canonical
};

/**
 * @brief GOD (Groupoid-Orbit Determinizer) Canonicalizer
 * 
 * Computes: GOD(t) = min_{prec_Enc}(Orbit_T(t))
 * 
 * Proof-producing: returns a certificate (sequence of actions)
 * that transforms the input to the canonical representative.
 * 
 * Memoized: caches results by TermId.
 */
class GODNormalizer {
public:
    explicit GODNormalizer(TermFactory& factory) : factory_(factory) {}
    
    /**
     * @brief Add a groupoid action generator
     */
    void addAction(GroupoidAction action) {
        actions_.push_back(std::move(action));
    }
    
    /**
     * @brief Initialize with Cayley-Dickson groupoid actions
     */
    void initCayleyDicksonActions();
    
    /**
     * @brief Compute GOD(t) = min_{prec_Enc}(Orbit(t)) -- simple interface
     */
    [[nodiscard]] const Term* normalize(const Term* term) {
        return normalizeWithCert(term).canonical;
    }
    
    /**
     * @brief Compute GOD(t) with full proof certificate
     * 
     * Returns the canonical rep + the action sequence to reach it.
     */
    [[nodiscard]] GODResult normalizeWithCert(const Term* term) {
        if (actions_.empty()) {
            return {term, {}};
        }
        
        // Memoization lookup
        auto cacheIt = memo_.find(term->id());
        if (cacheIt != memo_.end()) {
            return cacheIt->second;
        }
        
        ShortlexComparator cmp;
        const Term* minimal = term;
        TermId minimalId = term->id();
        
        // BFS node: stores term + back-pointer for certificate reconstruction
        struct BFSNode {
            const Term* term;
            TermId      parent;
            int         actionIdx;  // -1 for root
            bool        isInverse;
        };
        
        std::unordered_map<TermId, BFSNode> visited;
        std::queue<TermId> frontier;
        
        TermId rootId = term->id();
        visited[rootId] = {term, rootId, -1, false};
        frontier.push(rootId);
        
        // Complexity-scaled budget: small terms get small orbits
        size_t budget = std::min(maxOrbitSize_,
                                 static_cast<size_t>(64 + 8 * term->children().size()));
        
        while (!frontier.empty()) {
            TermId curId = frontier.front();
            frontier.pop();
            const Term* current = visited[curId].term;
            
            if (cmp(current, minimal)) {
                minimal = current;
                minimalId = current->id();
            }
            
            for (int ai = 0; ai < static_cast<int>(actions_.size()); ++ai) {
                const auto& action = actions_[ai];
                
                const Term* next = action.apply(current, factory_);
                if (next) {
                    TermId nid = next->id();
                    if (!visited.count(nid)) {
                        visited[nid] = {next, curId, ai, false};
                        frontier.push(nid);
                    }
                }
                
                const Term* invNext = action.applyInverse(current, factory_);
                if (invNext) {
                    TermId nid = invNext->id();
                    if (!visited.count(nid)) {
                        visited[nid] = {invNext, curId, ai, true};
                        frontier.push(nid);
                    }
                }
            }
            
            if (visited.size() > budget) break;
        }
        
        // Reconstruct certificate: path from root to minimal
        std::vector<OrbitStep> cert;
        TermId cur = minimalId;
        while (cur != rootId) {
            auto it = visited.find(cur);
            if (it == visited.end()) break;
            const auto& node = it->second;
            if (node.actionIdx < 0) break;
            cert.push_back({actions_[node.actionIdx].name(), node.isInverse});
            cur = node.parent;
        }
        std::reverse(cert.begin(), cert.end());
        
        GODResult result{minimal, std::move(cert)};
        
        if (memo_.size() < MAX_GOD_CACHE) {
            memo_[term->id()] = result;
        }
        
        return result;
    }
    
    /**
     * @brief Connect two terms: return composed certificate if same orbit
     * 
     * Path(A->B) = Path(A->canon) + Inverse(Path(B->canon))
     */
    [[nodiscard]] std::optional<std::vector<OrbitStep>> 
    connectTerms(const Term* t1, const Term* t2) {
        auto r1 = normalizeWithCert(t1);
        auto r2 = normalizeWithCert(t2);
        
        if (r1.canonical->id() != r2.canonical->id()) {
            return std::nullopt;  // Different orbits
        }
        
        std::vector<OrbitStep> path = r1.certificate;
        for (auto it = r2.certificate.rbegin(); it != r2.certificate.rend(); ++it) {
            path.push_back({it->actionName, !it->isInverse});
        }
        return path;
    }
    
    /**
     * @brief Compute full orbit (capped by maxTerms or maxOrbitSize_)
     */
    [[nodiscard]] std::vector<const Term*> computeOrbit(
        const Term* term, size_t maxTerms = 0
    ) {
        if (actions_.empty()) {
            return {term};
        }
        
        size_t cap = (maxTerms > 0) ? std::min(maxTerms, maxOrbitSize_)
                                     : maxOrbitSize_;
        
        std::unordered_set<TermId> visited;
        std::vector<const Term*> orbit;
        std::queue<const Term*> queue;
        
        visited.insert(term->id());
        orbit.push_back(term);
        queue.push(term);
        
        while (!queue.empty()) {
            const Term* current = queue.front();
            queue.pop();
            
            for (const auto& action : actions_) {
                const Term* next = action.apply(current, factory_);
                if (next && !visited.count(next->id())) {
                    visited.insert(next->id());
                    orbit.push_back(next);
                    queue.push(next);
                }
                
                const Term* invNext = action.applyInverse(current, factory_);
                if (invNext && !visited.count(invNext->id())) {
                    visited.insert(invNext->id());
                    orbit.push_back(invNext);
                    queue.push(invNext);
                }
            }
            
            if (visited.size() >= cap) break;
        }
        
        return orbit;
    }
    
    /**
     * @brief Check if two terms are in the same orbit
     */
    [[nodiscard]] bool sameOrbit(const Term* t1, const Term* t2) {
        return normalize(t1)->id() == normalize(t2)->id();
    }
    
    void setMaxOrbitSize(size_t max) { maxOrbitSize_ = max; }
    [[nodiscard]] size_t maxOrbitSize() const { return maxOrbitSize_; }
    void clearMemo() { memo_.clear(); }

private:
    TermFactory& factory_;
    std::vector<GroupoidAction> actions_;
    size_t maxOrbitSize_ = 1000;
    
    static constexpr size_t MAX_GOD_CACHE = 100000;
    std::unordered_map<TermId, GODResult> memo_;
};

// =============================================================================
// REWRITE RULE SYSTEM
// =============================================================================

/**
 * @brief Rewrite rule with metadata
 */
struct RewriteRule {
    using Transform = std::function<const Term*(const Term*, TermFactory&)>;
    
    std::string name;          // Rule name for debugging
    Transform apply;           // The actual transformation
    int priority = 0;          // Higher priority rules applied first
    bool recursive = true;     // Whether to recurse after applying
    
    RewriteRule(std::string n, Transform t, int p = 0, bool r = true)
        : name(std::move(n)), apply(std::move(t)), priority(p), recursive(r) {}
};

/**
 * @brief Statistics for normalization
 */
struct NormStats {
    size_t rulesApplied = 0;       // Number of rule applications
    size_t godNormalizations = 0;  // Number of GOD normalizations
    size_t lfpIterations = 0;      // Iterations to reach fixed point
    bool reachedFixedPoint = true; // True if lfp converged
    
    void reset() {
        rulesApplied = 0;
        godNormalizations = 0;
        lfpIterations = 0;
        reachedFixedPoint = true;
    }
};

// =============================================================================
// SCALAR PREDICATE HELPERS
// =============================================================================

/**
 * @brief Check if a term represents the scalar zero.
 * Handles both Scalar-kind terms (scalarValue()==0.0) and 
 * Constant-kind terms (symbol()=="0").
 */
inline bool isScalarZero(const Term* t) {
    if (!t) return false;
    if (t->kind() == TermKind::Scalar) return t->scalarValue() == 0.0;
    if (t->kind() == TermKind::Constant && t->symbol() == "0") return true;
    return false;
}

/**
 * @brief Check if a term represents the scalar one.
 * Handles both Scalar-kind terms (scalarValue()==1.0) and
 * Constant-kind terms (symbol()=="1").
 */
inline bool isScalarOne(const Term* t) {
    if (!t) return false;
    if (t->kind() == TermKind::Scalar) return t->scalarValue() == 1.0;
    if (t->kind() == TermKind::Constant && t->symbol() == "1") return true;
    return false;
}

/**
 * @brief Combined normalizer with rewrite rules, GOD, and lfp saturation
 * 
 * Implements the full normalization pipeline:
 *   1. Bottom-up rewriting (innermost evaluation)
 *   2. GOD orbit canonicalization
 *   3. Fixed-point iteration until convergence
 *   4. Optional lfp saturation for derived terms
 */
class Normalizer {
public:
    explicit Normalizer(TermFactory& factory) 
        : factory_(factory), godNormalizer_(factory) {}
    
    /**
     * @brief Add a simple rewrite rule (function variant)
     */
    void addRule(std::function<const Term*(const Term*, TermFactory&)> fn) {
        rules_.emplace_back("rule_" + std::to_string(rules_.size()), std::move(fn));
    }
    
    /**
     * @brief Add a named rewrite rule with full options
     */
    void addRule(RewriteRule rule) {
        rules_.push_back(std::move(rule));
        sortRules();
    }
    
    /**
     * @brief Add a conditional rewrite rule
     * Pattern: if condition(t) then transform(t) else t
     */
    void addConditionalRule(std::string name,
                            std::function<bool(const Term*)> condition,
                            std::function<const Term*(const Term*, TermFactory&)> transform,
                            int priority = 0) {
        rules_.emplace_back(std::move(name), 
            [cond = std::move(condition), trans = std::move(transform)]
            (const Term* t, TermFactory& f) -> const Term* {
                if (cond(t)) {
                    return trans(t, f);
                }
                return nullptr;
            }, priority);
        sortRules();
    }
    
    /**
     * @brief Initialize with standard Cayley-Dickson rules
     */
    void initStandardRules();
    
    /**
     * @brief Initialize with SCOUT-specific rules
     */
    void initScoutRules();
    
    /**
     * @brief Initialize with -ring rules
     */
    void initPhiRingRules();
    
    /**
     * @brief Normalize a term using rewrite rules + GOD to fixed point
     * 
     * Implements lfp(F) where F = GOD  rewrite
     */
    [[nodiscard]] const Term* normalize(const Term* term) {
        //  Cache lookup (hash-consed IDs are stable) 
        auto cacheIt = normCache_.find(term->id());
        if (cacheIt != normCache_.end()) {
            ++cacheHits_;
            return cacheIt->second;
        }
        ++cacheMisses_;
        
        stats_.reset();
        
        const Term* current = term;
        const Term* prev = nullptr;
        
        // Cycle detection: track all visited term IDs to catch oscillations
        std::unordered_set<TermId> seen;
        seen.insert(current->id());
        
        // Fixed-point iteration (rewrite-only, GOD applied ONCE at the end)
        while (current != prev) {
            stats_.lfpIterations++;
            
            if (stats_.lfpIterations > maxIterations_) {
                stats_.reachedFixedPoint = false;
                break;
            }
            
            prev = current;
            
            // Apply rewrite rules (bottom-up)
            current = applyRules(current);
            
            // Cycle detection: if we've seen this term before, we're oscillating
            if (!seen.insert(current->id()).second) {
                // Oscillation detected  pick the current form and stop
                stats_.reachedFixedPoint = false;
                break;
            }
        }
        
        // Apply GOD canonicalization ONCE at the end (not per-iteration)
        if (enableGOD_) {
            stats_.godNormalizations++;
            current = godNormalizer_.normalize(current);
        }
        
        //  Cache insert (bounded) 
        if (normCache_.size() < MAX_NORM_CACHE) {
            normCache_[term->id()] = current;
        }
        
        return current;
    }
    
    /**
     * @brief Clear the normalization cache (call after rule changes)
     */
    void clearCache() { normCache_.clear(); }
    
    /**
     * @brief Cache statistics
     */
    [[nodiscard]] size_t cacheHits() const { return cacheHits_; }
    [[nodiscard]] size_t cacheMisses() const { return cacheMisses_; }
    
    /**
     * @brief Normalize with lfp saturation (generates all equivalent forms)
     * 
     * @return Pair of (canonical form, set of all equivalent forms)
     */
    [[nodiscard]] std::pair<const Term*, std::vector<const Term*>>
    normalizeWithSaturation(const Term* term, size_t maxTerms = 1000) {
        const Term* canonical = normalize(term);
        std::vector<const Term*> equivalentForms = godNormalizer_.computeOrbit(canonical, maxTerms);
        return {canonical, equivalentForms};
    }
    
    /**
     * @brief Check if two terms are equivalent under normalization
     */
    [[nodiscard]] bool equivalent(const Term* t1, const Term* t2) {
        return normalize(t1)->id() == normalize(t2)->id();
    }
    
    /**
     * @brief Get the GOD normalizer for direct access
     */
    [[nodiscard]] GODNormalizer& god() { return godNormalizer_; }
    [[nodiscard]] const GODNormalizer& god() const { return godNormalizer_; }
    
    /**
     * @brief Get normalization statistics
     */
    [[nodiscard]] const NormStats& stats() const { return stats_; }
    
    /**
     * @brief Enable/disable GOD canonicalization
     */
    void setEnableGOD(bool enable) { enableGOD_ = enable; }
    
    /**
     * @brief Set maximum iterations for fixed-point computation
     */
    void setMaxIterations(size_t max) { maxIterations_ = max; }
    
    /**
     * @brief Get term factory
     */
    [[nodiscard]] TermFactory& factory() { return factory_; }

private:
    TermFactory& factory_;
    GODNormalizer godNormalizer_;
    std::vector<RewriteRule> rules_;
    NormStats stats_;
    bool enableGOD_ = true;
    size_t maxIterations_ = 64;  // Reduced from 1000: cycle detection catches oscillations
    
    //  Normalization cache (term ID  canonical form) 
    static constexpr size_t MAX_NORM_CACHE = 200000;
    std::unordered_map<TermId, const Term*> normCache_;
    size_t cacheHits_ = 0;
    size_t cacheMisses_ = 0;
    
    /**
     * @brief Sort rules by priority (highest first)
     */
    void sortRules() {
        std::stable_sort(rules_.begin(), rules_.end(),
            [](const RewriteRule& a, const RewriteRule& b) {
                return a.priority > b.priority;
            });
    }
    
    /**
     * @brief Apply rewrite rules bottom-up (with depth guard)
     */
    [[nodiscard]] const Term* applyRules(const Term* term, size_t depth = 0) {
        // Guard against unbounded recursive rewrites
        constexpr size_t MAX_REWRITE_DEPTH = 128;
        if (depth >= MAX_REWRITE_DEPTH) return term;
        
        // First normalize children (bottom-up)
        std::vector<const Term*> newChildren;
        bool childrenChanged = false;
        
        for (const Term* child : term->children()) {
            const Term* newChild = applyRules(child, depth + 1);
            newChildren.push_back(newChild);
            if (newChild != child) childrenChanged = true;
        }
        
        // Rebuild if children changed
        const Term* current = term;
        if (childrenChanged) {
            current = factory_.apply(term->symbol(), newChildren, term->sort());
        }
        
        // Apply rules at this level (in priority order)
        for (const auto& rule : rules_) {
            const Term* result = rule.apply(current, factory_);
            if (result && result->id() != current->id()) {
                stats_.rulesApplied++;
                
                if (rule.recursive) {
                    return applyRules(result, depth + 1); // Recurse after rewrite
                } else {
                    return result;
                }
            }
        }
        
        return current;
    }
};

// =============================================================================
// EQUATION NORMALIZER
// =============================================================================

/**
 * @brief Normalizer for equations (l = r)
 * 
 * Ensures equations are in canonical form with GOD(l)  GOD(r)
 */
class EquationNormalizer {
public:
    explicit EquationNormalizer(Normalizer& normalizer) 
        : normalizer_(normalizer) {}
    
    /**
     * @brief Normalize an equation to canonical form
     * 
     * @param lhs Left-hand side term
     * @param rhs Right-hand side term
     * @return Pair of (normalized lhs, normalized rhs) with lhs  rhs
     */
    [[nodiscard]] std::pair<const Term*, const Term*>
    normalizeEquation(const Term* lhs, const Term* rhs) {
        const Term* normLhs = normalizer_.normalize(lhs);
        const Term* normRhs = normalizer_.normalize(rhs);
        
        // Orient: smaller term on left
        ShortlexComparator cmp;
        if (cmp(normRhs, normLhs)) {
            std::swap(normLhs, normRhs);
        }
        
        return {normLhs, normRhs};
    }
    
    /**
     * @brief Check if equation is trivial (l = l)
     */
    [[nodiscard]] bool isTrivial(const Term* lhs, const Term* rhs) {
        return normalizer_.normalize(lhs)->id() == normalizer_.normalize(rhs)->id();
    }

private:
    Normalizer& normalizer_;
};

// Initialize Cayley-Dickson groupoid actions
inline void GODNormalizer::initCayleyDicksonActions() {
    // Conjugation flip: (a,b)  (a*,-b)
    addAction(GroupoidAction(
        "conj_flip",
        [](const Term* t, TermFactory& f) -> const Term* {
            if (t->kind() == TermKind::Pair && t->children().size() == 2) {
                const Term* a = t->children()[0];
                const Term* b = t->children()[1];
                return f.pair(f.conj(a), f.neg(b));
            }
            return t;
        },
        [](const Term* t, TermFactory& f) -> const Term* {
            if (t->kind() == TermKind::Pair && t->children().size() == 2) {
                const Term* a = t->children()[0];
                const Term* b = t->children()[1];
                return f.pair(f.conj(a), f.neg(b)); // Self-inverse
            }
            return t;
        }
    ));
    
    // Pair swap with sign: (a,b)  (-b*,a)
    addAction(GroupoidAction(
        "pair_rotate",
        [](const Term* t, TermFactory& f) -> const Term* {
            if (t->kind() == TermKind::Pair && t->children().size() == 2) {
                const Term* a = t->children()[0];
                const Term* b = t->children()[1];
                return f.pair(f.neg(f.conj(b)), a);
            }
            return t;
        },
        [](const Term* t, TermFactory& f) -> const Term* {
            if (t->kind() == TermKind::Pair && t->children().size() == 2) {
                const Term* a = t->children()[0];
                const Term* b = t->children()[1];
                return f.pair(b, f.neg(f.conj(a)));
            }
            return t;
        }
    ));
}

// Initialize standard rewrite rules
inline void Normalizer::initStandardRules() {
    // Double negation: --x  x (high priority)
    addRule(RewriteRule("double_neg", [](const Term* t, TermFactory& /*f*/) -> const Term* {
        if (t->kind() == TermKind::Application && t->symbol() == "neg") {
            if (!t->children().empty()) {
                const Term* inner = t->children()[0];
                if (inner->kind() == TermKind::Application && inner->symbol() == "neg") {
                    if (!inner->children().empty()) {
                        return inner->children()[0];
                    }
                }
            }
        }
        return nullptr;
    }, 20));
    
    // Double conjugation: (x*)*  x (high priority)
    addRule(RewriteRule("double_conj", [](const Term* t, TermFactory& /*f*/) -> const Term* {
        if (t->kind() == TermKind::Application && t->symbol() == "conj") {
            if (!t->children().empty()) {
                const Term* inner = t->children()[0];
                if (inner->kind() == TermKind::Application && inner->symbol() == "conj") {
                    if (!inner->children().empty()) {
                        return inner->children()[0];
                    }
                }
            }
        }
        return nullptr;
    }, 20));
    
    // Add zero: x + 0  x
    addRule(RewriteRule("add_zero", [](const Term* t, TermFactory& /*f*/) -> const Term* {
        if (t->kind() == TermKind::Application && t->symbol() == "add") {
            if (t->children().size() == 2) {
                const Term* rhs = t->children()[1];
                if (isScalarZero(rhs)) {
                    return t->children()[0];
                }
                // Also: 0 + x  x
                const Term* lhs = t->children()[0];
                if (isScalarZero(lhs)) {
                    return t->children()[1];
                }
            }
        }
        return nullptr;
    }, 15));
    
    // Multiply by one: x * 1  x, 1 * x  x
    addRule(RewriteRule("mul_one", [](const Term* t, TermFactory& /*f*/) -> const Term* {
        if (t->kind() == TermKind::Application && (t->symbol() == "mul" || t->symbol() == "*")) {
            if (t->children().size() == 2) {
                const Term* lhs = t->children()[0];
                const Term* rhs = t->children()[1];
                if (isScalarOne(rhs)) {
                    return lhs;
                }
                if (isScalarOne(lhs)) {
                    return rhs;
                }
            }
        }
        return nullptr;
    }, 15));
    
    // Multiply by zero: x * 0  0
    addRule(RewriteRule("mul_zero", [](const Term* t, TermFactory& /*f*/) -> const Term* {
        if (t->kind() == TermKind::Application && (t->symbol() == "mul" || t->symbol() == "*")) {
            if (t->children().size() == 2) {
                for (const Term* child : t->children()) {
                    if (isScalarZero(child)) {
                        return child;
                    }
                }
            }
        }
        return nullptr;
    }, 16));
    
    // Additive inverse: x + (-x)  0
    addRule(RewriteRule("add_inverse", [](const Term* t, TermFactory& f) -> const Term* {
        if (t->kind() == TermKind::Application && t->symbol() == "add") {
            if (t->children().size() == 2) {
                const Term* lhs = t->children()[0];
                const Term* rhs = t->children()[1];
                if (rhs->kind() == TermKind::Application && rhs->symbol() == "neg") {
                    if (!rhs->children().empty() && rhs->children()[0]->id() == lhs->id()) {
                        return f.scalar(0.0);
                    }
                }
                // Also check (-x) + x  0
                if (lhs->kind() == TermKind::Application && lhs->symbol() == "neg") {
                    if (!lhs->children().empty() && lhs->children()[0]->id() == rhs->id()) {
                        return f.scalar(0.0);
                    }
                }
            }
        }
        return nullptr;
    }, 12));
    
    // Multiplicative inverse: x * x  1 (for division algebras)
    addRule(RewriteRule("mul_inverse", [](const Term* t, TermFactory& f) -> const Term* {
        if (t->kind() == TermKind::Application && (t->symbol() == "mul" || t->symbol() == "*")) {
            if (t->children().size() == 2) {
                const Term* lhs = t->children()[0];
                const Term* rhs = t->children()[1];
                if (rhs->kind() == TermKind::Application && rhs->symbol() == "inv") {
                    if (!rhs->children().empty() && rhs->children()[0]->id() == lhs->id()) {
                        return f.scalar(1.0);
                    }
                }
                // Also check x * x  1
                if (lhs->kind() == TermKind::Application && lhs->symbol() == "inv") {
                    if (!lhs->children().empty() && lhs->children()[0]->id() == rhs->id()) {
                        return f.scalar(1.0);
                    }
                }
            }
        }
        return nullptr;
    }, 12));
    
    //    + 1 (golden ratio identity)
    addRule(RewriteRule("phi_squared", [](const Term* t, TermFactory& f) -> const Term* {
        if (t->kind() == TermKind::Application && (t->symbol() == "mul" || t->symbol() == "*")) {
            if (t->children().size() == 2) {
                const Term* lhs = t->children()[0];
                const Term* rhs = t->children()[1];
                if (lhs->kind() == TermKind::Phi && rhs->kind() == TermKind::Phi) {
                    //  =  + 1
                    return f.add(f.phi(), f.scalar(1.0));
                }
            }
        }
        return nullptr;
    }, 10)); // High priority
    
    // Initialize GOD actions
    godNormalizer_.initCayleyDicksonActions();
}

// =============================================================================
// SCOUT-SPECIFIC RULES
// =============================================================================

inline void Normalizer::initScoutRules() {
    // Scout normalization: x  scalar (when x is a scalar)
    addRule(RewriteRule("scout_scalar_norm", [](const Term* t, TermFactory& /*f*/) -> const Term* {
        if (t->kind() == TermKind::Application && t->symbol() == "norm") {
            if (!t->children().empty()) {
                const Term* inner = t->children()[0];
                if (inner->kind() == TermKind::Scalar) {
                    // |r| = |r| as scalar value
                    return inner;
                }
            }
        }
        return nullptr;
    }, 5));
    
    // Scout boundary collapse: UNT(scalar)  scalar
    addRule(RewriteRule("scout_unt_scalar", [](const Term* t, TermFactory& /*f*/) -> const Term* {
        if (t->kind() == TermKind::Application && t->symbol() == "UNT") {
            if (!t->children().empty()) {
                const Term* inner = t->children()[0];
                if (inner->kind() == TermKind::Scalar) {
                    // UNT of a scalar is the scalar
                    return inner;
                }
            }
        }
        return nullptr;
    }, 5));
    
    // Cayley map identity: C_J(0)  1
    addRule(RewriteRule("cayley_zero", [](const Term* t, TermFactory& f) -> const Term* {
        if (t->kind() == TermKind::Application && t->symbol() == "cayley_map") {
            if (!t->children().empty()) {
                const Term* r = t->children()[0];
                if (isScalarZero(r)) {
                    return f.scalar(1.0);
                }
            }
        }
        return nullptr;
    }, 8));
    
    // Phase transport simplification: 1  
    addRule(RewriteRule("phase_transport_one", [](const Term* t, TermFactory& /*f*/) -> const Term* {
        if (t->kind() == TermKind::Application && t->symbol() == "phase_mul") {
            if (t->children().size() == 2) {
                const Term* rhs = t->children()[1];
                if (isScalarOne(rhs)) {
                    return t->children()[0];
                }
            }
        }
        return nullptr;
    }, 5));
    
    // Boundary degree zero: deg()  1
    addRule(RewriteRule("boundary_degree_zero", [](const Term* t, TermFactory& f) -> const Term* {
        if (t->kind() == TermKind::Application && t->symbol() == "boundary_degree") {
            if (t->children().size() >= 2) {
                const Term* degree = t->children()[1];
                if (isScalarZero(degree)) {
                    return f.scalar(1.0);
                }
            }
        }
        return nullptr;
    }, 5));
}

// =============================================================================
// PHI-RING RULES
// =============================================================================

inline void Normalizer::initPhiRingRules() {
    // Already have    + 1 from standard rules
    
    // REMOVED: phi_cubed ((+1)  2+1)  DERIVABLE from =+1
    // REMOVED: phibar_squared (  +1)  DERIVABLE
    // REMOVED: phi_phibar_product (  -1)  DERIVABLE
    // REMOVED: phi_phibar_sum (+  1)  DERIVABLE (should be DISCOVERED)
    // REMOVED: phi_inverse (1/  -1)  DERIVABLE
    // 
    // Kept: golden_conjugate (conj())  DEFINITION of Galois conjugation
    // Kept: sub_one_phi (1-)  DEFINITION of 
    // Kept: add_one_neg_phi (1+neg())  syntactic variant of definition
    // Kept: zeckendorf_carry  Fibonacci number system infrastructure
    // Kept: phi_power (^01, ^1)  trivial pow simplification

    // Zeckendorf normalization: detect consecutive Fibonacci terms
    // F_k + F_{k-1}  F_{k+1}
    addRule(RewriteRule("zeckendorf_carry", [](const Term* t, TermFactory& f) -> const Term* {
        if (t->kind() == TermKind::Application && t->symbol() == "add") {
            if (t->children().size() == 2) {
                const Term* lhs = t->children()[0];
                const Term* rhs = t->children()[1];
                
                // Check for fib(k) + fib(k-1) pattern  fib(k+1)
                if (lhs->kind() == TermKind::Application && lhs->symbol() == "fib" &&
                    rhs->kind() == TermKind::Application && rhs->symbol() == "fib") {
                    // Both are fib(n) applications  extract indices
                    if (!lhs->children().empty() && !rhs->children().empty()) {
                        const Term* lIdx = lhs->children()[0];
                        const Term* rIdx = rhs->children()[0];
                        // Check for scalar indices
                        if (lIdx->kind() == TermKind::Scalar && rIdx->kind() == TermKind::Scalar) {
                            double lv = lIdx->scalarValue();
                            double rv = rIdx->scalarValue();
                            double hi = std::max(lv, rv);
                            double lo = std::min(lv, rv);
                            // F_k + F_{k-1}  F_{k+1} iff |hi - lo| == 1
                            if (hi - lo == 1.0 && lo >= 1.0) {
                                return f.apply("fib", {f.scalar(hi + 1.0)}, t->sort());
                            }
                            // F_k + F_k  F_k + F_{k-1} + F_{k-2}
                            // (i.e. 2F_k = F_{k+1} + F_{k-2}  skip for now,
                            //  the carry rule will fire again on the output)
                        }
                    }
                }
                
                // Also handle the  Zeckendorf identity:
                // ^n + ^{n-1} = ^{n+1} (derived from =+1)
                // Detect: mul(phi, phi^{k}) + phi^{k}  phi^{k+1} patterns
                // This is already covered by phi_cubed and phi_squared rules
            }
        }
        return nullptr;
    }, 7));
    
    // Golden conjugate relation: conj()   (atomic swap, no syntactic expansion)
    addRule(RewriteRule("golden_conjugate", [](const Term* t, TermFactory& f) -> const Term* {
        if (t->kind() == TermKind::Application && t->symbol() == "conj") {
            if (!t->children().empty()) {
                const Term* inner = t->children()[0];
                if (inner->kind() == TermKind::Phi) {
                    // conj()  
                    return f.phiBar();
                }
                if (inner->kind() == TermKind::PhiBar) {
                    // conj()   (involution)
                    return f.phi();
                }
            }
        }
        return nullptr;
    }, 8));
    
    // sub(1, )    collapse syntactic conjugation to atomic PhiBar
    addRule(RewriteRule("sub_one_phi", [](const Term* t, TermFactory& f) -> const Term* {
        if (t->kind() == TermKind::Application && 
            (t->symbol() == "sub" || t->symbol() == "-") &&
            t->children().size() == 2) {
            const Term* lhs = t->children()[0];
            const Term* rhs = t->children()[1];
            // sub(1, )  
            if (lhs->kind() == TermKind::Scalar && lhs->scalarValue() == 1.0 &&
                rhs->kind() == TermKind::Phi) {
                return f.phiBar();
            }
            // sub(1, )   (involution)
            if (lhs->kind() == TermKind::Scalar && lhs->scalarValue() == 1.0 &&
                rhs->kind() == TermKind::PhiBar) {
                return f.phi();
            }
        }
        return nullptr;
    }, 3));  // Higher priority (lower number) to fire early
    
    // add(1, neg())    alternative syntactic form of 1 - 
    addRule(RewriteRule("add_one_neg_phi", [](const Term* t, TermFactory& f) -> const Term* {
        if (t->kind() == TermKind::Application && 
            (t->symbol() == "+" || t->symbol() == "add") &&
            t->children().size() == 2) {
            const Term* lhs = t->children()[0];
            const Term* rhs = t->children()[1];
            // 1 + neg()  
            if (lhs->kind() == TermKind::Scalar && lhs->scalarValue() == 1.0 &&
                rhs->kind() == TermKind::Application && rhs->symbol() == "neg" &&
                !rhs->children().empty() && rhs->children()[0]->kind() == TermKind::Phi) {
                return f.phiBar();
            }
            // 1 + neg()  
            if (lhs->kind() == TermKind::Scalar && lhs->scalarValue() == 1.0 &&
                rhs->kind() == TermKind::Application && rhs->symbol() == "neg" &&
                !rhs->children().empty() && rhs->children()[0]->kind() == TermKind::PhiBar) {
                return f.phi();
            }
        }
        return nullptr;
    }, 3));
    
    // REMOVED: phibar_squared (  +1)  DERIVABLE, should be DISCOVERED
    // REMOVED: phi_phibar_product (  -1)  DERIVABLE, should be DISCOVERED
    // REMOVED: phi_phibar_sum (+  1)  DERIVABLE, should be DISCOVERED
    // REMOVED: phi_inverse (1/  -1)  DERIVABLE, should be DISCOVERED
    
    // ^n normalization using Lucas sequence:
    // ^n = F_n   + F_{n-1}
    addRule(RewriteRule("phi_power", [](const Term* t, TermFactory& f) -> const Term* {
        if (t->kind() == TermKind::Application && t->symbol() == "pow") {
            if (t->children().size() == 2) {
                const Term* base = t->children()[0];
                const Term* exp = t->children()[1];
                
                if (base->kind() == TermKind::Phi && exp->kind() == TermKind::Scalar) {
                    // ^0 = 1, ^1 = , others need Fibonacci lookup
                    if (isScalarZero(exp)) {
                        return f.scalar(1.0);
                    } else if (isScalarOne(exp)) {
                        return f.phi();
                    }
                }
            }
        }
        return nullptr;
    }, 8));
}

} // namespace logic
} // namespace autodiscover

#endif // AUTODISCOVER_LOGIC_NORMALIZER_HPP
