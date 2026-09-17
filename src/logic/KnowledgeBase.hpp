/**
 * @file KnowledgeBase.hpp
 * @brief Storage and retrieval of equations for equational reasoning
 * 
 * The knowledge base maintains:
 * - Active equations (for inference)
 * - Passive equations (waiting for processing)
 * - Indexing structures for efficient retrieval
 */

#ifndef AUTODISCOVER_LOGIC_KNOWLEDGEBASE_HPP
#define AUTODISCOVER_LOGIC_KNOWLEDGEBASE_HPP

#include "Equation.hpp"
#include "MatcherUnifier.hpp"
#include "../core/Term.hpp"
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <memory>
#include <vector>
#include <functional>
#include <optional>
#include <sstream>

namespace autodiscover {
namespace logic {

using core::Term;
using core::TermId;
using core::TermKind;

/**
 * @brief Index entry for term retrieval
 */
struct IndexEntry {
    Equation::Id equationId;
    bool isLhs;           // true if indexed term is on LHS
    const Term* term;     // The indexed term
};

// =============================================================================
// DISCRIMINATION TREE
// =============================================================================

/**
 * @brief Discrimination tree for O(|query|) term indexing
 * 
 * A discrimination tree indexes terms by their flattened structure.
 * Each node branches on the TermKind + symbol at the corresponding position
 * in a pre-order traversal of the term. Leaves store sets of IndexEntry.
 * 
 * Retrieval: given a query term t, returns all indexed terms whose
 * top-level structure matches t (generalizations of t for forward matching,
 * exact structural matches for retrieval). Much faster than linear scan
 * for large knowledge bases.
 * 
 * The flattened key for a term is its pre-order traversal where each node
 * is represented by (kind, symbol, arity). Variables match any subtree
 * (acting as wildcards during retrieval).
 */
class DiscriminationTree {
public:
    DiscriminationTree() : root_(std::make_unique<DTNode>()) {}

    /**
     * @brief Insert a term with its associated index entry
     */
    void insert(const Term* term, const IndexEntry& entry) {
        std::vector<DTKey> path;
        flattenTerm(term, path);
        
        DTNode* node = root_.get();
        for (const auto& key : path) {
            auto it = node->children.find(key);
            if (it == node->children.end()) {
                node->children[key] = std::make_unique<DTNode>();
            }
            node = node->children[key].get();
        }
        node->entries.push_back(entry);
    }

    /**
     * @brief Retrieve all entries whose indexed term unifies with the query
     * 
     * Performs a pre-order traversal of the query term, following the
     * corresponding branches in the discrimination tree. When the query
     * has a variable at a position, ALL branches at that tree node are
     * explored (wildcard matching).
     */
    [[nodiscard]] std::vector<IndexEntry> retrieve(const Term* query) const {
        std::vector<IndexEntry> results;
        std::vector<DTKey> path;
        flattenTerm(query, path);
        
        // Walk the tree, collecting entries at matching leaves
        std::vector<const DTNode*> frontier = {root_.get()};
        
        for (size_t i = 0; i < path.size() && !frontier.empty(); ++i) {
            std::vector<const DTNode*> next;
            const auto& key = path[i];
            
            for (const DTNode* node : frontier) {
                if (key.kind == TermKind::Variable) {
                    // Variable in query: match ALL branches at this position
                    // We need to skip the subtree in each branch's key sequence
                    // For simplicity, match the exact variable key AND all others
                    for (const auto& [childKey, childNode] : node->children) {
                        next.push_back(childNode.get());
                    }
                } else {
                    // Exact match: follow the specific branch
                    auto it = node->children.find(key);
                    if (it != node->children.end()) {
                        next.push_back(it->second.get());
                    }
                    // Also follow variable branches in the INDEXED terms
                    DTKey varKey{TermKind::Variable, "*", 0};
                    auto vit = node->children.find(varKey);
                    if (vit != node->children.end()) {
                        next.push_back(vit->second.get());
                    }
                }
            }
            frontier = std::move(next);
        }
        
        // Collect entries from all reached leaf nodes
        for (const DTNode* node : frontier) {
            collectEntries(node, results);
        }
        
        return results;
    }

    /**
     * @brief Retrieve entries matching by root symbol only (fast path)
     */
    [[nodiscard]] std::vector<IndexEntry> retrieveByRoot(const std::string& rootSymbol) const {
        std::vector<IndexEntry> results;
        
        // Look for Application nodes with this symbol at the root
        for (const auto& [key, child] : root_->children) {
            if (key.symbol == rootSymbol) {
                collectEntries(child.get(), results);
            }
        }
        return results;
    }

    /**
     * @brief Number of indexed terms
     */
    [[nodiscard]] size_t size() const {
        return countEntries(root_.get());
    }

    /**
     * @brief Clear all indexed entries
     */
    void clear() {
        root_ = std::make_unique<DTNode>();
    }

private:
    /**
     * @brief Key for a discrimination tree branch
     */
    struct DTKey {
        TermKind kind;
        std::string symbol;  // symbol or "*" for variables
        size_t arity;         // number of children

        bool operator==(const DTKey& other) const {
            return kind == other.kind && symbol == other.symbol && arity == other.arity;
        }
    };

    struct DTKeyHash {
        size_t operator()(const DTKey& k) const {
            size_t h = std::hash<uint8_t>()(static_cast<uint8_t>(k.kind));
            h ^= std::hash<std::string>()(k.symbol) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<size_t>()(k.arity) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    /**
     * @brief Node in the discrimination tree
     */
    struct DTNode {
        std::unordered_map<DTKey, std::unique_ptr<DTNode>, DTKeyHash> children;
        std::vector<IndexEntry> entries;  // Non-empty only at leaves
    };

    std::unique_ptr<DTNode> root_;

    /**
     * @brief Flatten a term into a sequence of DTKeys via pre-order traversal
     */
    static void flattenTerm(const Term* term, std::vector<DTKey>& path) {
        if (term->isVariable()) {
            path.push_back(DTKey{TermKind::Variable, "*", 0});
            return;
        }
        
        std::string sym = term->symbol();
        if (sym.empty()) {
            // Use kind-based symbol for structural terms
            switch (term->kind()) {
                case TermKind::Pair:     sym = "__pair"; break;
                case TermKind::Scalar:   sym = "__scalar"; break;
                case TermKind::Phi:      sym = "__phi"; break;
                case TermKind::PhiBar:   sym = "__phibar"; break;
                case TermKind::J:        sym = "__J"; break;
                default:                 sym = "__app"; break;
            }
        }
        
        path.push_back(DTKey{term->kind(), sym, term->children().size()});
        
        for (const Term* child : term->children()) {
            flattenTerm(child, path);
        }
    }

    /**
     * @brief Recursively collect all entries in a subtree
     */
    static void collectEntries(const DTNode* node, std::vector<IndexEntry>& results) {
        for (const auto& entry : node->entries) {
            results.push_back(entry);
        }
        for (const auto& [key, child] : node->children) {
            collectEntries(child.get(), results);
        }
    }

    /**
     * @brief Count entries in subtree
     */
    static size_t countEntries(const DTNode* node) {
        size_t count = node->entries.size();
        for (const auto& [key, child] : node->children) {
            count += countEntries(child.get());
        }
        return count;
    }
};

/**
 * @brief Priority for equation selection (given clause selection)
 */
struct EquationPriority {
    uint32_t weight;      // Term complexity weight
    uint32_t age;         // Clause age (FIFO component)
    
    bool operator<(const EquationPriority& other) const {
        // Lower weight is better, use age as tiebreaker
        if (weight != other.weight) return weight > other.weight;
        return age > other.age;
    }
};

/**
 * @brief Knowledge base for equational reasoning
 */
class KnowledgeBase {
public:
    /**
     * @brief Construct with a reference to the shared TermFactory.
     *
     * All subsumption matching now uses the same factory that created the
     * terms being compared, eliminating cross-factory ID comparison bugs.
     */
    explicit KnowledgeBase(core::TermFactory& factory) : factory_(factory) {}
    
    /**
     * @brief Add an axiom equation
     */
    Equation::Id addAxiom(const Term* lhs, const Term* rhs) {
        auto eq = std::make_unique<Equation>(lhs, rhs, EquationSource::Axiom);
        return addEquation(std::move(eq));
    }
    
    /**
     * @brief Add a goal (negated for refutation)
     */
    Equation::Id addGoal(const Term* lhs, const Term* rhs) {
        auto eq = std::make_unique<Equation>(lhs, rhs, EquationSource::Goal);
        return addEquation(std::move(eq));
    }
    
    /**
     * @brief Add a derived equation
     */
    Equation::Id addDerived(const Term* lhs, const Term* rhs, 
                            EquationSource source = EquationSource::Inference) {
        // Check for trivial equation
        if (lhs->id() == rhs->id()) {
            return 0; // Don't add trivial equations
        }
        
        auto eq = std::make_unique<Equation>(lhs, rhs, source);
        return addEquation(std::move(eq));
    }
    
    /**
     * @brief Select the next equation for processing (given clause selection)
     */
    [[nodiscard]] std::optional<Equation::Id> selectNext() {
        while (!passive_.empty()) {
            auto [priority, id] = passive_.top();
            passive_.pop();
            
            // Check if still valid (not subsumed)
            if (valid_.count(id)) {
                activate(id);
                return id;
            }
        }
        return std::nullopt;
    }
    
    /**
     * @brief Mark an equation as subsumed (removed)
     */
    void markSubsumed(Equation::Id id) {
        valid_.erase(id);
        active_.erase(id);
        demodCacheDirty_ = true;
    }
    
    /**
     * @brief Get equation by ID
     */
    [[nodiscard]] const Equation* get(Equation::Id id) const {
        auto it = equations_.find(id);
        return it != equations_.end() ? it->second.get() : nullptr;
    }
    
    /**
     * @brief Get all active equations (returns copy  prefer forEachActive)
     */
    [[nodiscard]] std::vector<Equation::Id> activeEquations() const {
        return std::vector<Equation::Id>(active_.begin(), active_.end());
    }
    
    /**
     * @brief Iterate over active equations without copying the set
     * 
     * The callback receives (Equation::Id, const Equation&).
     * Much cheaper than activeEquations() which copies the entire set.
     */
    template <typename Fn>
    void forEachActive(Fn&& fn) const {
        for (auto id : active_) {
            auto it = equations_.find(id);
            if (it != equations_.end() && valid_.count(id)) {
                fn(id, *it->second);
            }
        }
    }
    
    /**
     * @brief Number of active equations (O(1))
     */
    [[nodiscard]] size_t activeCount() const { return active_.size(); }
    
    /**
     * @brief Check if an equation is subsumed by existing equations
     * 
     * An equation s=t is subsumed if there exists an active equation l=r
     * and a substitution  such that (l)=s and (r)=t (or symmetric).
     * 
     * Uses discrimination tree to find candidate equations whose LHS/RHS
     * structurally matches s or t, instead of scanning ALL active equations.
     */
    [[nodiscard]] bool isSubsumed(const Equation& eq) {
        // Collect candidate equation IDs from the discrimination tree
        // These are equations whose indexed terms structurally match eq's sides
        std::unordered_set<Equation::Id> candidateIds;
        
        auto addCandidates = [&](const Term* query) {
            auto entries = dtIndex_.retrieve(query);
            for (const auto& entry : entries) {
                if (valid_.count(entry.equationId) && active_.count(entry.equationId)) {
                    candidateIds.insert(entry.equationId);
                }
            }
        };
        
        addCandidates(eq.lhs());
        addCandidates(eq.rhs());
        
        for (auto id : candidateIds) {
            const Equation* existing = get(id);
            if (!existing) continue;
            
            // Exact match (both orientations)
            if (existing->lhs()->id() == eq.lhs()->id() 
                && existing->rhs()->id() == eq.rhs()->id()) {
                return true;
            }
            if (existing->lhs()->id() == eq.rhs()->id() 
                && existing->rhs()->id() == eq.lhs()->id()) {
                return true;
            }
            
            // Proper subsumption: existing l=r subsumes new s=t iff
            // there exists  such that (l)=s  (r)=t or (l)=t  (r)=s.
            // A larger existing equation cannot subsume a smaller one.
            size_t existingSize = existing->lhs()->size() + existing->rhs()->size();
            size_t newSize = eq.lhs()->size() + eq.rhs()->size();
            if (existingSize > newSize) continue;
            
            // Try pattern matching in both orientations using the shared factory.
            Unifier tempUnifier(factory_);
            
            // Orientation 1: (l) = s and (r) = t
            {
                auto m1 = tempUnifier.match(existing->lhs(), eq.lhs());
                if (m1.success) {
                    const core::Term* rhsApplied = 
                        m1.substitution.apply(existing->rhs(), factory_);
                    if (rhsApplied && rhsApplied->id() == eq.rhs()->id()) {
                        return true;
                    }
                }
            }
            // Orientation 2: (l) = t and (r) = s
            {
                auto m1 = tempUnifier.match(existing->lhs(), eq.rhs());
                if (m1.success) {
                    const core::Term* rhsApplied = 
                        m1.substitution.apply(existing->rhs(), factory_);
                    if (rhsApplied && rhsApplied->id() == eq.lhs()->id()) {
                        return true;
                    }
                }
            }
        }
        return false;
    }
    
    /**
     * @brief Check if the empty clause has been derived
     */
    [[nodiscard]] bool hasContradiction() const {
        return contradictionFound_;
    }
    
    /**
     * @brief Record that contradiction was found
     */
    void recordContradiction(Equation::Id eq1, Equation::Id eq2) {
        contradictionFound_ = true;
        contradictionEq1_ = eq1;
        contradictionEq2_ = eq2;
    }
    
    /**
     * @brief Get equations that might unify with a term at root (symbol-based fast path)
     */
    [[nodiscard]] std::vector<IndexEntry> getCandidates(const std::string& rootSymbol) const {
        auto it = symbolIndex_.find(rootSymbol);
        if (it != symbolIndex_.end()) {
            std::vector<IndexEntry> result;
            for (const auto& entry : it->second) {
                if (valid_.count(entry.equationId)) {
                    result.push_back(entry);
                }
            }
            return result;
        }
        return {};
    }
    
    /**
     * @brief Get equations whose indexed term structurally matches a query term
     * 
     * Uses the discrimination tree for O(|query|) retrieval instead of
     * linear scan. Returns entries for terms that could unify with the query.
     */
    [[nodiscard]] std::vector<IndexEntry> getCandidatesForTerm(const Term* query) const {
        auto candidates = dtIndex_.retrieve(query);
        // Filter to only valid equations
        std::vector<IndexEntry> result;
        result.reserve(candidates.size());
        for (const auto& entry : candidates) {
            if (valid_.count(entry.equationId)) {
                result.push_back(entry);
            }
        }
        return result;
    }
    
    /**
     * @brief Get discrimination tree index size
     */
    [[nodiscard]] size_t indexSize() const { return dtIndex_.size(); }
    
    /**
     * @brief Get statistics
     */
    [[nodiscard]] size_t numActive() const { return active_.size(); }
    [[nodiscard]] size_t numPassive() const { return passive_.size(); }
    [[nodiscard]] size_t numTotal() const { return equations_.size(); }
    [[nodiscard]] size_t numValid() const { return valid_.size(); }
    
    /**
     * @brief Get oriented equations for demodulation (cached)
     */
    [[nodiscard]] const std::vector<const Equation*>& getDemodulators() const {
        if (demodCacheDirty_) {
            demodCache_.clear();
            for (auto id : active_) {
                auto it = equations_.find(id);
                if (it != equations_.end()) {
                    const Equation* eq = it->second.get();
                    if (eq->orientation() != Orientation::Unoriented) {
                        demodCache_.push_back(eq);
                    }
                }
            }
            demodCacheDirty_ = false;
        }
        return demodCache_;
    }
    
    /**
     * @brief Forward simplify a new equation using existing demodulators
     * 
     * Attempts to rewrite both sides of `eq` using oriented active equations
     * (demodulators). Applies matching to find lr that matches a subterm,
     * then replaces that subterm with r. Repeats until fixpoint.
     * 
     * @param eq The equation to simplify (modified in place)
     * @param factory The term factory for building rewritten terms
     * @param unifier The unifier for pattern matching
     * @return true if any simplification occurred
     */
    bool forwardSimplify(Equation& eq, core::TermFactory& factory, logic::Unifier& unifier) {
        bool changed = false;
        const auto& demods = getDemodulators();
        if (demods.empty()) return false;
        
        // Rewrite LHS and RHS to fixpoint
        const Term* newLhs = rewriteToFixpoint(eq.lhs(), demods, factory, unifier);
        const Term* newRhs = rewriteToFixpoint(eq.rhs(), demods, factory, unifier);
        
        if (newLhs->id() != eq.lhs()->id() || newRhs->id() != eq.rhs()->id()) {
            // Rebuild equation with simplified sides
            // We cannot mutate lhs_/rhs_ directly, so we return true
            // and let the caller rebuild with the new terms.
            // Store the simplified terms for retrieval:
            lastSimplifiedLhs_ = newLhs;
            lastSimplifiedRhs_ = newRhs;
            changed = true;
        }
        return changed;
    }
    
    /**
     * @brief Get the last simplified LHS (valid only after forwardSimplify returns true)
     */
    [[nodiscard]] const Term* lastSimplifiedLhs() const { return lastSimplifiedLhs_; }
    
    /**
     * @brief Get the last simplified RHS (valid only after forwardSimplify returns true)
     */
    [[nodiscard]] const Term* lastSimplifiedRhs() const { return lastSimplifiedRhs_; }
    
    /**
     * @brief Backward simplify existing active equations using a new demodulator
     * 
     * If the newly activated equation is oriented (lr), scan all other active
     * equations and attempt to rewrite their sides using this rule. Any equation
     * that simplifies is replaced.
     * 
     * @param newId The ID of the newly added oriented equation
     * @param factory The term factory
     * @param unifier The unifier for pattern matching
     */
    void backwardSimplify(Equation::Id newId, core::TermFactory& factory, logic::Unifier& unifier) {
        auto it = equations_.find(newId);
        if (it == equations_.end()) return;
        
        const Equation* newEq = it->second.get();
        // Only oriented equations can be used as demodulators
        if (newEq->orientation() == Orientation::Unoriented) return;
        
        const Term* ruleL = newEq->largerSide();
        const Term* ruleR = newEq->smallerSide();
        if (!ruleL || !ruleR) return;
        
        // Collect IDs to avoid modifying during iteration
        std::vector<Equation::Id> toSimplify;
        for (auto activeId : active_) {
            if (activeId == newId) continue;
            if (!valid_.count(activeId)) continue;
            toSimplify.push_back(activeId);
        }
        
        for (auto activeId : toSimplify) {
            auto ait = equations_.find(activeId);
            if (ait == equations_.end()) continue;
            Equation* existing = ait->second.get();
            
            const Term* simpLhs = rewriteOnce(existing->lhs(), ruleL, ruleR, factory, unifier);
            const Term* simpRhs = rewriteOnce(existing->rhs(), ruleL, ruleR, factory, unifier);
            
            if (simpLhs->id() != existing->lhs()->id() || 
                simpRhs->id() != existing->rhs()->id()) {
                // The existing equation was simplified: remove old, add new
                if (simpLhs->id() == simpRhs->id()) {
                    // Simplified to trivial  just remove
                    markSubsumed(activeId);
                } else {
                    // Replace with simplified version
                    markSubsumed(activeId);
                    addDerived(simpLhs, simpRhs, EquationSource::Simplification);
                }
            }
        }
    }
    
    /**
     * @brief Get statistics string
     */
    [[nodiscard]] std::string statistics() const {
        std::ostringstream oss;
        oss << "KnowledgeBase statistics:\n"
            << "  Active equations:  " << active_.size() << "\n"
            << "  Passive equations: " << passive_.size() << "\n"
            << "  Total equations:   " << equations_.size() << "\n"
            << "  Valid equations:   " << valid_.size() << "\n"
            << "  DT index entries:  " << dtIndex_.size() << "\n"
            << "  Contradiction:     " << (contradictionFound_ ? "YES" : "no") << "\n";
        return oss.str();
    }
    
private:
    // Equation storage
    std::unordered_map<Equation::Id, std::unique_ptr<Equation>> equations_;
    Equation::Id nextId_ = 1;
    
    // Active/passive sets
    std::unordered_set<Equation::Id> active_;
    std::unordered_set<Equation::Id> valid_;
    
    // Priority queue for passive equations
    using PriorityPair = std::pair<EquationPriority, Equation::Id>;
    std::priority_queue<PriorityPair, std::vector<PriorityPair>, 
                        std::greater<PriorityPair>> passive_;
    
    // Symbol-based index for superposition candidates (fast path by root symbol)
    std::unordered_map<std::string, std::vector<IndexEntry>> symbolIndex_;
    
    // Discrimination tree index for structural term retrieval
    DiscriminationTree dtIndex_;
    
    // Contradiction tracking
    bool contradictionFound_ = false;
    Equation::Id contradictionEq1_ = 0;
    Equation::Id contradictionEq2_ = 0;
    
    // Stored results from forward simplification
    const Term* lastSimplifiedLhs_ = nullptr;
    const Term* lastSimplifiedRhs_ = nullptr;
    
    // Demodulator cache (invalidated when active set changes)
    mutable std::vector<const Equation*> demodCache_;
    mutable bool demodCacheDirty_ = true;
    
    // Shared factory reference (replaces the unsound subsumptionFactory_)
    core::TermFactory& factory_;
    
    uint32_t age_ = 0;
    
    Equation::Id addEquation(std::unique_ptr<Equation> eq) {
        Equation::Id id = nextId_++;
        eq->setId(id);
        
        // Index terms by root symbol
        indexTerm(eq->lhs(), id, true);
        indexTerm(eq->rhs(), id, false);
        
        // Compute priority
        uint32_t weight = static_cast<uint32_t>(eq->lhs()->size() + eq->rhs()->size());
        EquationPriority priority{weight, age_++};
        
        valid_.insert(id);
        passive_.push({priority, id});
        equations_[id] = std::move(eq);
        
        return id;
    }
    
    void activate(Equation::Id id) {
        active_.insert(id);
        demodCacheDirty_ = true;
    }
    
    void indexTerm(const Term* term, Equation::Id eqId, bool isLhs) {
        // Index by root symbol
        std::string rootSym = term->symbol();
        if (rootSym.empty()) {
            rootSym = "_app"; // Application terms
        }
        IndexEntry entry{eqId, isLhs, term};
        symbolIndex_[rootSym].push_back(entry);
        dtIndex_.insert(term, entry);
    }
    
    /**
     * @brief Attempt to rewrite a term once at any position using rule lr
     * @return The (possibly rewritten) term
     */
    const Term* rewriteOnce(const Term* term, const Term* ruleL, const Term* ruleR,
                             core::TermFactory& factory, logic::Unifier& unifier) {
        // Try matching l against the root
        auto result = unifier.match(ruleL, term);
        if (result.success) {
            return result.substitution.apply(ruleR, factory);
        }
        
        // Recurse into children
        if (term->children().empty()) return term;
        
        bool changed = false;
        std::vector<const Term*> newChildren;
        newChildren.reserve(term->children().size());
        for (const Term* child : term->children()) {
            const Term* nc = rewriteOnce(child, ruleL, ruleR, factory, unifier);
            newChildren.push_back(nc);
            if (nc->id() != child->id()) changed = true;
        }
        
        if (!changed) return term;
        return factory.apply(term->symbol(), newChildren);
    }
    
    /**
     * @brief Rewrite a term to fixpoint using all demodulators
     * 
     * Repeatedly traverses the term, applying the first matching demodulator
     * at each position, until no more rewrites apply. Bounded by maxIters
     * to guarantee termination.
     */
    const Term* rewriteToFixpoint(const Term* term,
                                   const std::vector<const Equation*>& demods,
                                   core::TermFactory& factory,
                                   logic::Unifier& unifier,
                                   size_t maxIters = 100) {
        for (size_t iter = 0; iter < maxIters; ++iter) {
            bool changed = false;
            for (const Equation* demod : demods) {
                const Term* ruleL = demod->largerSide();
                const Term* ruleR = demod->smallerSide();
                if (!ruleL || !ruleR) continue;
                
                const Term* result = rewriteOnce(term, ruleL, ruleR, factory, unifier);
                if (result->id() != term->id()) {
                    term = result;
                    changed = true;
                    break; // restart with first demodulator
                }
            }
            if (!changed) break;
        }
        return term;
    }
};

} // namespace logic
} // namespace autodiscover

#endif // AUTODISCOVER_LOGIC_KNOWLEDGEBASE_HPP
