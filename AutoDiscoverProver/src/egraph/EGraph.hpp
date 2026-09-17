/**
 * @file EGraph.hpp
 * @brief E-Graph Quotient Structure for Term Equivalence and Proof Extraction
 * 
 * MATHEMATICAL FOUNDATION:
 * ========================
 * 
 * An E-Graph (Equality Graph) represents the quotient T/ of terms T under
 * an equivalence relation  induced by a set of equations.
 * 
 * STRUCTURE:
 * ----------
 * - E-Classes: Equivalence classes of terms
 * - E-Nodes: Symbolic function applications with e-class children
 * - Congruence: f(a,...,a)  f(b,...,b) when a  b for all i
 * 
 * FORMAL DEFINITION:
 * ------------------
 * An ENode is a pair (f, [c, ..., c]) where:
 *   - f is a function symbol
 *   - c are e-class identifiers (not terms!)
 * 
 * This representation enables:
 *   1. Efficient congruence closure
 *   2. Compact storage of equivalent terms
 *   3. Proof extraction from merge history
 * 
 * UNION-FIND INVARIANT:
 * ---------------------
 * Every e-class has a canonical representative. Lookups use path compression.
 * 
 * @author AutoDiscoverProver Team
 */

#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <memory>
#include <functional>
#include <queue>
#include <variant>
#include <string>
#include <sstream>
#include <any>
#include <limits>
#include <utility>

namespace autodiscover {
namespace egraph {

// ===========================================================================
// TYPE DEFINITIONS
// ===========================================================================

/**
 * @brief Identifier for an e-class in the e-graph
 */
using EClassId = uint32_t;

/**
 * @brief Reserved invalid e-class identifier
 */
constexpr EClassId INVALID_ECLASS = std::numeric_limits<EClassId>::max();

/**
 * @brief Symbol identifier (interned string)
 */
using SymbolId = uint32_t;

// ===========================================================================
// E-NODE STRUCTURE
// ===========================================================================

/**
 * @brief An e-node represents f(c, ..., c) where c are e-class IDs
 * 
 * INVARIANT: All children are canonical (find-compressed) e-class IDs.
 */
struct ENode {
    SymbolId symbol;
    std::vector<EClassId> children;
    
    ENode() : symbol(0) {}
    ENode(SymbolId s) : symbol(s) {}
    ENode(SymbolId s, std::vector<EClassId> kids) 
        : symbol(s), children(std::move(kids)) {}
    
    bool operator==(const ENode& other) const {
        return symbol == other.symbol && children == other.children;
    }
    
    bool operator!=(const ENode& other) const {
        return !(*this == other);
    }
    
    /**
     * @brief Canonical ordering of e-nodes (for hash table)
     */
    bool operator<(const ENode& other) const {
        if (symbol != other.symbol) return symbol < other.symbol;
        return children < other.children;
    }
    
    /**
     * @brief Check if this is a leaf node (constant or variable)
     */
    bool isLeaf() const { return children.empty(); }
    
    /**
     * @brief Arity of the function symbol
     */
    size_t arity() const { return children.size(); }
};

/**
 * @brief Hash function for ENode
 */
struct ENodeHash {
    size_t operator()(const ENode& node) const {
        size_t h = std::hash<SymbolId>{}(node.symbol);
        for (EClassId kid : node.children) {
            h ^= std::hash<EClassId>{}(kid) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};

// ===========================================================================
// E-CLASS STRUCTURE
// ===========================================================================

/**
 * @brief An e-class contains all e-nodes representing equivalent terms
 */
struct EClass {
    // The set of e-nodes in this e-class
    std::vector<ENode> nodes;
    
    // Parent e-classes (for congruence propagation)  UNIQUE, canonical
    std::vector<EClassId> parents;
    std::unordered_set<EClassId> parentSet;  // membership guard for O(1) dedup
    
    // User data (for storing extracted terms, proofs, etc.)
    std::any userData;
    
    EClass() = default;
    
    void addNode(const ENode& node) {
        nodes.push_back(node);
    }
    
    /**
     * @brief Add a parent with deduplication + canonicalization.
     * @param parent The parent e-class ID
     * @param findFn A callable that returns the canonical root of an e-class
     */
    template<typename FindFn>
    void addParentDedup(EClassId parent, FindFn&& findFn) {
        parent = findFn(parent);  // canonicalize
        if (parentSet.insert(parent).second) {
            parents.push_back(parent);
        }
    }
    
    /**
     * @brief Legacy addParent  still deduplicates but without canonicalization.
     * Prefer addParentDedup when a find function is available.
     */
    void addParent(EClassId parent) {
        if (parentSet.insert(parent).second) {
            parents.push_back(parent);
        }
    }
};

// ===========================================================================
// UNION-FIND STRUCTURE
// ===========================================================================

/**
 * @brief Union-Find with path compression for e-class management
 */
class UnionFind {
private:
    struct Entry {
        EClassId parent;
        uint32_t rank;
    };
    
    mutable std::vector<Entry> data_;
    
public:
    UnionFind() = default;
    
    /**
     * @brief Create a new singleton set
     * @return The ID of the new set
     */
    EClassId makeSet() {
        EClassId id = static_cast<EClassId>(data_.size());
        data_.push_back({id, 0});
        return id;
    }
    
    /**
     * @brief Find the canonical representative of a set (with path compression)
     */
    EClassId find(EClassId id) const {
        if (id >= data_.size()) return INVALID_ECLASS;
        
        // Path compression: make all nodes point directly to root
        EClassId root = id;
        while (data_[root].parent != root) {
            root = data_[root].parent;
        }
        
        // Compress path
        EClassId current = id;
        while (data_[current].parent != root) {
            EClassId next = data_[current].parent;
            data_[current].parent = root;
            current = next;
        }
        
        return root;
    }
    
    /**
     * @brief Union two sets by rank, with deterministic tie-breaking
     * @return The canonical representative of the merged set
     *
     * When ranks are equal, the SMALLER id becomes the root.
     * This ensures merge(a,b) == merge(b,a) for determinism.
     */
    EClassId unite(EClassId a, EClassId b) {
        EClassId rootA = find(a);
        EClassId rootB = find(b);
        
        if (rootA == rootB) return rootA;  // Already in same set
        
        // Union by rank
        if (data_[rootA].rank < data_[rootB].rank) {
            data_[rootA].parent = rootB;
            return rootB;
        } else if (data_[rootA].rank > data_[rootB].rank) {
            data_[rootB].parent = rootA;
            return rootA;
        } else {
            // Equal rank: deterministic tie-break by smaller id
            EClassId winner = std::min(rootA, rootB);
            EClassId loser  = std::max(rootA, rootB);
            data_[loser].parent = winner;
            data_[winner].rank++;
            return winner;
        }
    }
    
    /**
     * @brief Check if two elements are in the same set
     */
    bool connected(EClassId a, EClassId b) const {
        return find(a) == find(b);
    }
    
    /**
     * @brief Get the number of elements
     */
    size_t size() const { return data_.size(); }
};

// ===========================================================================
// PROOF JUSTIFICATION
// ===========================================================================

/**
 * @brief Justification for why two e-classes were merged
 */
struct MergeReason {
    enum class Kind {
        Axiom,          // Merged due to user-provided axiom
        Congruence,     // Merged due to congruence closure
        Transitive,     // Merged through transitivity chain
        External        // Merged by external proof
    };
    
    Kind kind;
    
    // For Axiom: index into axiom table
    size_t axiomIndex;
    
    // For Congruence: the parent e-nodes that became congruent
    std::optional<std::pair<ENode, ENode>> congruentNodes;
    
    // For Transitive: intermediate e-class
    std::optional<EClassId> intermediate;
    
    // Description for debugging
    std::string description;
    
    static MergeReason fromAxiom(size_t idx, const std::string& desc = "") {
        MergeReason r;
        r.kind = Kind::Axiom;
        r.axiomIndex = idx;
        r.description = desc;
        return r;
    }
    
    static MergeReason fromCongruence(const ENode& a, const ENode& b) {
        MergeReason r;
        r.kind = Kind::Congruence;
        r.congruentNodes = {a, b};
        r.description = "congruence";
        return r;
    }
    
    static MergeReason fromTransitive(EClassId via) {
        MergeReason r;
        r.kind = Kind::Transitive;
        r.intermediate = via;
        r.description = "transitivity";
        return r;
    }
    
    static MergeReason external(const std::string& desc) {
        MergeReason r;
        r.kind = Kind::External;
        r.description = desc;
        return r;
    }
};

/**
 * @brief Record of a single merge operation (for proof extraction)
 */
struct MergeRecord {
    EClassId classA;
    EClassId classB;
    EClassId result;
    MergeReason reason;
};

// ===========================================================================
// SYMBOL TABLE
// ===========================================================================

/**
 * @brief Interned symbol table for e-graph
 */
class EGraphSymbolTable {
private:
    std::unordered_map<std::string, SymbolId> nameToId_;
    std::vector<std::string> idToName_;
    
public:
    /**
     * @brief Intern a symbol name
     */
    SymbolId intern(const std::string& name) {
        auto it = nameToId_.find(name);
        if (it != nameToId_.end()) {
            return it->second;
        }
        SymbolId id = static_cast<SymbolId>(idToName_.size());
        nameToId_[name] = id;
        idToName_.push_back(name);
        return id;
    }
    
    /**
     * @brief Get name for symbol ID
     */
    const std::string& name(SymbolId id) const {
        static const std::string empty;
        if (id >= idToName_.size()) return empty;
        return idToName_[id];
    }
    
    /**
     * @brief Check if symbol exists
     */
    bool contains(const std::string& name) const {
        return nameToId_.find(name) != nameToId_.end();
    }
    
    /**
     * @brief Get number of symbols
     */
    size_t size() const { return idToName_.size(); }
};

// ===========================================================================
// E-GRAPH MAIN CLASS
// ===========================================================================

/**
 * @brief E-Graph for efficient equivalence reasoning
 * 
 * Supports:
 * - Adding terms
 * - Asserting equalities
 * - Congruence closure
 * - Proof extraction
 */
class EGraph {
private:
    // Union-find for e-class equivalence
    UnionFind uf_;
    
    // Symbol table
    EGraphSymbolTable symbols_;
    
    // E-class storage
    std::vector<EClass> classes_;
    
    // Hashcons: ENode -> owning e-class
    std::unordered_map<ENode, EClassId, ENodeHash> hashcons_;
    
    // Pending repair queue: e-classes whose parents need congruence checking
    std::queue<EClassId> pendingRepairs_;
    
    // Epoch-based dedup for repair queue: prevent the same class from being enqueued
    // thousands of times in a single rebuild pass.
    std::vector<uint32_t> repairMark_;
    uint32_t repairEpoch_ = 1;
    
    // Pending congruence merges discovered during repair (carries ENode witnesses)
    struct PendingCongruence {
        EClassId a, b;
        ENode nodeA, nodeB;  // The congruent ENodes that triggered the merge
    };
    std::queue<PendingCongruence> pendingCongruences_;
    
    // Merge history (for proof extraction)
    std::vector<MergeRecord> mergeHistory_;
    
    // Dirty flag: true if there are unprocessed merges
    bool dirty_ = false;
    
    // Budget limit for rebuild operations (prevents infinite repair churn)
    static constexpr uint64_t REBUILD_OPS_LIMIT = 50'000'000;
    
    /**
     * @brief Enqueue a class for repair with epoch-based dedup.
     * Each class can only be enqueued once per rebuild epoch.
     */
    void enqueueRepair(EClassId id) {
        id = find(id);
        if (id >= repairMark_.size()) repairMark_.resize(id + 1, 0);
        if (repairMark_[id] != repairEpoch_) {
            repairMark_[id] = repairEpoch_;
            pendingRepairs_.push(id);
        }
    }
    
public:
    EGraph() = default;
    
    // -----------------------------------------------------------------------
    // SYMBOL MANAGEMENT
    // -----------------------------------------------------------------------
    
    /**
     * @brief Intern a symbol name
     */
    SymbolId internSymbol(const std::string& name) {
        return symbols_.intern(name);
    }
    
    /**
     * @brief Get symbol name
     */
    const std::string& symbolName(SymbolId id) const {
        return symbols_.name(id);
    }
    
    // -----------------------------------------------------------------------
    // E-CLASS LOOKUP
    // -----------------------------------------------------------------------
    
    /**
     * @brief Find canonical e-class for an ID
     */
    EClassId find(EClassId id) const {
        return uf_.find(id);
    }
    
    /**
     * @brief Get e-class by ID
     */
    const EClass* getClass(EClassId id) const {
        EClassId canonical = find(id);
        if (canonical >= classes_.size()) return nullptr;
        return &classes_[canonical];
    }
    
    EClass* getClass(EClassId id) {
        EClassId canonical = find(id);
        if (canonical >= classes_.size()) return nullptr;
        return &classes_[canonical];
    }
    
    /**
     * @brief Check if two IDs are in the same e-class
     */
    bool equivalent(EClassId a, EClassId b) const {
        return find(a) == find(b);
    }
    
    // -----------------------------------------------------------------------
    // TERM ADDITION
    // -----------------------------------------------------------------------
    
    /**
     * @brief Add a leaf term (constant or variable)
     */
    EClassId addLeaf(const std::string& name) {
        SymbolId sym = internSymbol(name);
        return addNode(ENode(sym));
    }
    
    /**
     * @brief Add a function application f(c, ..., c)
     */
    EClassId addApp(const std::string& funcName, const std::vector<EClassId>& args) {
        SymbolId sym = internSymbol(funcName);
        
        // Canonicalize children
        std::vector<EClassId> canonArgs;
        canonArgs.reserve(args.size());
        for (EClassId arg : args) {
            canonArgs.push_back(find(arg));
        }
        
        return addNode(ENode(sym, std::move(canonArgs)));
    }
    
    /**
     * @brief Add an e-node, returning its e-class
     * 
     * If the node already exists (up to canonicalization), returns existing class.
     */
    EClassId addNode(ENode node) {
        // Canonicalize children
        for (EClassId& kid : node.children) {
            kid = find(kid);
        }
        
        // Check hashcons
        auto it = hashcons_.find(node);
        if (it != hashcons_.end()) {
            return find(it->second);
        }
        
        // Create new e-class
        EClassId id = uf_.makeSet();
        
        // Ensure classes_ vector is large enough
        if (id >= classes_.size()) {
            classes_.resize(id + 1);
        }
        
        // Add node to new class
        classes_[id].addNode(node);
        
        // Register in hashcons
        hashcons_[node] = id;
        
        // Register as parent of children (deduplicated + canonical)
        for (EClassId kid : node.children) {
            EClassId kidRoot = find(kid);
            classes_[kidRoot].addParentDedup(id, [&](EClassId x){ return find(x); });
        }
        
        return id;
    }
    
    // -----------------------------------------------------------------------
    // MERGE OPERATIONS
    // -----------------------------------------------------------------------
    
    /**
     * @brief Assert that two e-classes are equal
     * 
     * @param a First e-class
     * @param b Second e-class
     * @param reason Justification for the merge
     * @return The canonical e-class after merge
     */
    EClassId merge(EClassId a, EClassId b, MergeReason reason = MergeReason::external("merge")) {
        EClassId rootA = find(a);
        EClassId rootB = find(b);
        
        if (rootA == rootB) return rootA;  // Already equivalent
        
        // Perform union
        EClassId newRoot = uf_.unite(rootA, rootB);
        EClassId oldRoot = (newRoot == rootA) ? rootB : rootA;
        
        // Record merge
        mergeHistory_.push_back({rootA, rootB, newRoot, std::move(reason)});
        
        // Merge e-class contents
        EClass& newClass = classes_[newRoot];
        EClass& oldClass = classes_[oldRoot];
        
        // Move nodes from old to new
        for (const ENode& node : oldClass.nodes) {
            newClass.addNode(node);
        }
        oldClass.nodes.clear();
        
        // Move parents from old to new (deduplicated + canonical)
        for (EClassId parent : oldClass.parents) {
            newClass.addParentDedup(parent, [&](EClassId x){ return find(x); });
        }
        oldClass.parents.clear();
        oldClass.parentSet.clear();
        
        // Schedule parents for congruence check (epoch-deduped)
        for (EClassId parent : newClass.parents) {
            enqueueRepair(parent);
        }
        
        dirty_ = true;
        return newRoot;
    }
    
    /**
     * @brief Assert equality from an axiom
     */
    EClassId assertEquality(EClassId a, EClassId b, size_t axiomIndex, 
                            const std::string& desc = "") {
        return merge(a, b, MergeReason::fromAxiom(axiomIndex, desc));
    }
    
    // -----------------------------------------------------------------------
    // CONGRUENCE CLOSURE
    // -----------------------------------------------------------------------
    
    /**
     * @brief Rebuild the e-graph to restore invariants
     * 
     * This performs congruence closure: if f(a,...,a) and f(b,...,b) exist
     * with a  b for all i, then they must be in the same e-class.
     *
     * FIXED-POINT: The hashcons rebuild can discover new congruence merges.
     * We re-enter the worklist loop when that happens, rather than losing
     * merges by setting dirty_=false prematurely.
     */
    void rebuild() {
        if (!dirty_) return;
        
        // Advance repair epoch (so each class is enqueued at most once per rebuild)
        ++repairEpoch_;
        if (repairEpoch_ == 0) {
            repairEpoch_ = 1;
            std::fill(repairMark_.begin(), repairMark_.end(), 0);
        }
        
        uint64_t ops = 0;
        
        // Outer fixed-point: repeat until no new merges from hashcons rebuild
        bool anyHashconsMerge = true;
        while (anyHashconsMerge) {
            anyHashconsMerge = false;
            
            // Inner worklist: process pending repairs and congruence merges
            bool progress = true;
            while (progress) {
                progress = false;
                
                // Phase 1: Process all pending repairs
                while (!pendingRepairs_.empty()) {
                    if (++ops > REBUILD_OPS_LIMIT) {
                        // Drain queues and bail  prevent infinite churn
                        while (!pendingRepairs_.empty()) pendingRepairs_.pop();
                        while (!pendingCongruences_.empty()) pendingCongruences_.pop();
                        dirty_ = false;
                        return;
                    }
                    EClassId classId = pendingRepairs_.front();
                    pendingRepairs_.pop();
                    repairCongruence(find(classId));
                    progress = true;
                }
                
                // Phase 2: Process all pending congruence merges
                while (!pendingCongruences_.empty()) {
                    if (++ops > REBUILD_OPS_LIMIT) {
                        while (!pendingCongruences_.empty()) pendingCongruences_.pop();
                        dirty_ = false;
                        return;
                    }
                    auto pc = pendingCongruences_.front();
                    pendingCongruences_.pop();
                    EClassId ca = find(pc.a);
                    EClassId cb = find(pc.b);
                    if (ca != cb) {
                        merge(ca, cb, MergeReason::fromCongruence(pc.nodeA, pc.nodeB));
                        progress = true;
                    }
                }
            }
            
            // Rebuild hashcons with canonical keys
            std::unordered_map<ENode, EClassId, ENodeHash> newHashcons;
            
            for (auto& [node, classId] : hashcons_) {
                ENode canonNode = node;
                for (EClassId& kid : canonNode.children) {
                    kid = find(kid);
                }
                
                EClassId canonClass = find(classId);
                
                auto it = newHashcons.find(canonNode);
                if (it != newHashcons.end()) {
                    // Congruence discovered during hashcons rebuild!
                    if (find(it->second) != canonClass) {
                        EClassId other = find(it->second);
                        merge(canonClass, other, 
                              MergeReason::fromCongruence(node, canonNode));
                        anyHashconsMerge = true;  // must re-enter worklist loop
                    }
                } else {
                    newHashcons[canonNode] = canonClass;
                }
            }
            
            hashcons_ = std::move(newHashcons);
        }
        
        dirty_ = false;
    }
    
private:
    /**
     * @brief Repair congruence for parents of a modified e-class
     */
    void repairCongruence(EClassId classId) {
        EClass* eclass = getClass(classId);
        if (!eclass) return;
        
        // Check each parent for potential congruence merges
        for (EClassId parentId : eclass->parents) {
            EClass* parent = getClass(parentId);
            if (!parent) continue;
            
            for (const ENode& node : parent->nodes) {
                // Canonicalize node
                ENode canonNode = node;
                for (EClassId& kid : canonNode.children) {
                    kid = find(kid);
                }
                
                // Check if congruent node exists
                auto it = hashcons_.find(canonNode);
                if (it != hashcons_.end() && find(it->second) != find(parentId)) {
                    // Schedule as congruence merge with actual ENode witnesses
                    // for auditability of proof extraction
                    pendingCongruences_.push({find(parentId), find(it->second), 
                                              node, it->first});
                }
            }
        }
    }
    
public:
    // -----------------------------------------------------------------------
    // PROOF EXTRACTION
    // -----------------------------------------------------------------------
    
    /**
     * @brief Extract a proof path between two e-classes
     * 
     * Returns the sequence of merge records that connect a to b.
     * Uses BFS over the merge history graph, where nodes are original
     * (pre-merge) class IDs and edges are merge records.
     * 
     * @param a Source e-class
     * @param b Target e-class
     * @return Vector of merge records forming the proof, or empty if not connected
     */
    std::vector<MergeRecord> extractProofPath(EClassId a, EClassId b) const {
        if (find(a) != find(b)) {
            return {};  // Not equivalent
        }
        
        if (find(a) == find(b) && a == b) {
            return {};  // Same class originally  trivial
        }
        
        // Build an adjacency list over original merge participants.
        // Each merge record (classA, classB, result, reason) creates
        // an edge between classA and classB.
        // We search for a path from any class equivalent to `a`
        // to any class equivalent to `b`.
        
        // Collect all distinct class IDs mentioned in merge history
        std::unordered_map<EClassId, std::vector<size_t>> adjacency; // classId -> record indices
        for (size_t i = 0; i < mergeHistory_.size(); ++i) {
            const auto& rec = mergeHistory_[i];
            adjacency[rec.classA].push_back(i);
            adjacency[rec.classB].push_back(i);
        }
        
        // BFS from `a` searching for `b` (or any class with find(x)==find(b))
        EClassId target = find(b);
        // Use parent-pointer chain instead of storing full path vectors
        // to avoid quadratic memory usage.
        std::unordered_map<EClassId, std::pair<EClassId, size_t>> parent; // node -> (predecessor, record_idx)
        parent[a] = {INVALID_ECLASS, SIZE_MAX}; // sentinel for start node
        
        std::queue<EClassId> worklist;
        worklist.push(a);
        
        while (!worklist.empty()) {
            EClassId current = worklist.front();
            worklist.pop();
            
            // Check against canonical target, not raw b (which may be stale after merges)
            if (current == b || find(current) == target) {
                // Reconstruct path from parent-pointer chain
                std::vector<MergeRecord> path;
                EClassId node = current;
                while (parent[node].second != SIZE_MAX) {
                    path.push_back(mergeHistory_[parent[node].second]);
                    node = parent[node].first;
                }
                std::reverse(path.begin(), path.end());
                return path;
            }
            
            // Explore merge records involving `current`
            auto it = adjacency.find(current);
            if (it == adjacency.end()) continue;
            
            for (size_t recIdx : it->second) {
                const MergeRecord& record = mergeHistory_[recIdx];
                EClassId neighbor = INVALID_ECLASS;
                
                if (record.classA == current && parent.find(record.classB) == parent.end()) {
                    neighbor = record.classB;
                } else if (record.classB == current && parent.find(record.classA) == parent.end()) {
                    neighbor = record.classA;
                }
                // Also check the result node for reachability
                if (neighbor == INVALID_ECLASS && record.result == current && 
                    parent.find(record.classA) == parent.end() && record.classA != current) {
                    neighbor = record.classA;
                }
                if (neighbor == INVALID_ECLASS && record.result == current && 
                    parent.find(record.classB) == parent.end() && record.classB != current) {
                    neighbor = record.classB;
                }
                
                if (neighbor != INVALID_ECLASS) {
                    parent[neighbor] = {current, recIdx};
                    worklist.push(neighbor);
                }
            }
        }
        
        return {};  // Should not reach here if find(a) == find(b)
    }
    
    /**
     * @brief Produce a human-readable explanation chain for why a  b
     * 
     * Returns a vector of strings, each describing one step in the
     * equational derivation: "class X merged with class Y because <reason>"
     */
    [[nodiscard]] std::vector<std::string> explain(EClassId a, EClassId b) const {
        auto path = extractProofPath(a, b);
        std::vector<std::string> explanation;
        explanation.reserve(path.size());
        
        for (const auto& rec : path) {
            std::ostringstream oss;
            oss << "class " << rec.classA << " = class " << rec.classB << " by ";
            switch (rec.reason.kind) {
                case MergeReason::Kind::Axiom:
                    oss << "axiom";
                    break;
                case MergeReason::Kind::Congruence:
                    oss << "congruence closure";
                    break;
                case MergeReason::Kind::Transitive:
                    oss << "transitivity";
                    break;
                case MergeReason::Kind::External:
                    oss << "external justification";
                    break;
            }
            if (!rec.reason.description.empty()) {
                oss << " (" << rec.reason.description << ")";
            }
            explanation.push_back(oss.str());
        }
        
        return explanation;
    }
    
    /**
     * @brief Get full merge history
     */
    const std::vector<MergeRecord>& mergeHistory() const {
        return mergeHistory_;
    }
    
    // -----------------------------------------------------------------------
    // EXTRACTION (SELECTING BEST TERM)
    // -----------------------------------------------------------------------
    
    /**
     * @brief Cost function type for extraction
     */
    using CostFn = std::function<double(const ENode&, const std::vector<double>&)>;
    
    /**
     * @brief Extract minimal-cost term from an e-class
     * 
     * Uses dynamic programming to find the cheapest term.
     * 
     * @param classId E-class to extract from
     * @param costFn Function computing cost of node given children costs
     * @return Pair of (ENode, cost), or nullopt if class is empty
     */
    std::optional<std::pair<ENode, double>> extract(EClassId classId, const CostFn& costFn) {
        rebuild();  // Ensure invariants
        
        classId = find(classId);
        
        // Memoization table: e-class -> (best node, cost)
        std::unordered_map<EClassId, std::pair<ENode, double>> memo;
        
        return extractImpl(classId, costFn, memo);
    }
    
private:
    /**
     * @brief Extract implementation with cycle detection.
     * 
     * Uses a 'visiting' set to break cycles: if we encounter a class
     * that we are currently in the process of extracting (cycle), we
     * return nullopt for that branch, which prevents infinite recursion.
     */
    std::optional<std::pair<ENode, double>> extractImpl(
        EClassId classId,
        const CostFn& costFn,
        std::unordered_map<EClassId, std::pair<ENode, double>>& memo
    ) {
        // Thread-local visiting set for cycle detection
        // (safe because extraction is single-threaded)
        static thread_local std::unordered_set<EClassId> visiting;
        
        classId = find(classId);
        
        // Check memo
        auto it = memo.find(classId);
        if (it != memo.end()) {
            return it->second;
        }
        
        // Cycle break: if we're already visiting this class, return nullopt
        if (visiting.count(classId)) {
            return std::nullopt;
        }
        
        const EClass* eclass = getClass(classId);
        if (!eclass || eclass->nodes.empty()) {
            return std::nullopt;
        }
        
        visiting.insert(classId);
        
        ENode bestNode;
        double bestCost = std::numeric_limits<double>::infinity();
        
        for (const ENode& node : eclass->nodes) {
            // Get costs of children
            std::vector<double> childCosts;
            bool valid = true;
            
            for (EClassId kid : node.children) {
                auto childResult = extractImpl(find(kid), costFn, memo);
                if (!childResult) {
                    valid = false;
                    break;
                }
                childCosts.push_back(childResult->second);
            }
            
            if (!valid) continue;
            
            double cost = costFn(node, childCosts);
            if (cost < bestCost) {
                bestCost = cost;
                bestNode = node;
            }
        }
        
        visiting.erase(classId);
        
        if (bestCost == std::numeric_limits<double>::infinity()) {
            return std::nullopt;
        }
        
        memo[classId] = {bestNode, bestCost};
        return memo[classId];
    }
    
public:
    // -----------------------------------------------------------------------
    // STATISTICS
    // -----------------------------------------------------------------------
    
    /**
     * @brief Get number of e-classes (including merged)
     */
    size_t numClasses() const { return classes_.size(); }
    
    /**
     * @brief Get number of canonical e-classes
     */
    size_t numCanonicalClasses() const {
        std::unordered_set<EClassId> canonicals;
        for (EClassId i = 0; i < classes_.size(); ++i) {
            canonicals.insert(find(i));
        }
        return canonicals.size();
    }
    
    /**
     * @brief Get number of e-nodes
     */
    size_t numNodes() const { return hashcons_.size(); }
    
    /**
     * @brief Get number of symbols
     */
    size_t numSymbols() const { return symbols_.size(); }
    
    // -----------------------------------------------------------------------
    // DEBUG OUTPUT
    // -----------------------------------------------------------------------
    
    /**
     * @brief Debug string representation
     */
    std::string debugString() const {
        std::ostringstream oss;
        oss << "EGraph {\n";
        oss << "  symbols: " << numSymbols() << "\n";
        oss << "  classes: " << numCanonicalClasses() << " (total: " << numClasses() << ")\n";
        oss << "  nodes: " << numNodes() << "\n";
        oss << "  merges: " << mergeHistory_.size() << "\n";
        
        // Print some classes
        std::unordered_set<EClassId> seen;
        for (EClassId i = 0; i < std::min<size_t>(classes_.size(), 10); ++i) {
            EClassId canonical = find(i);
            if (seen.count(canonical)) continue;
            seen.insert(canonical);
            
            const EClass& ec = classes_[canonical];
            oss << "  class " << canonical << ": ";
            for (size_t j = 0; j < std::min<size_t>(ec.nodes.size(), 3); ++j) {
                if (j > 0) oss << ", ";
                oss << symbols_.name(ec.nodes[j].symbol);
                if (!ec.nodes[j].children.empty()) {
                    oss << "(";
                    for (size_t k = 0; k < ec.nodes[j].children.size(); ++k) {
                        if (k > 0) oss << ",";
                        oss << ec.nodes[j].children[k];
                    }
                    oss << ")";
                }
            }
            if (ec.nodes.size() > 3) oss << ", ...";
            oss << "\n";
        }
        if (seen.size() < numCanonicalClasses()) {
            oss << "  ... (" << (numCanonicalClasses() - seen.size()) << " more)\n";
        }
        
        oss << "}\n";
        return oss.str();
    }
};

// ===========================================================================
// E-GRAPH PATTERN MATCHING
// ===========================================================================

/**
 * @brief Pattern for matching against e-graph
 * 
 * A pattern is either:
 *   - A variable (matches any e-class)
 *   - A function application with pattern children
 */
struct Pattern {
    bool isVariable;
    std::string name;               // Variable name or function symbol
    std::vector<Pattern> children;  // For function applications
    
    /**
     * @brief Create a variable pattern
     */
    static Pattern var(const std::string& name) {
        Pattern p;
        p.isVariable = true;
        p.name = name;
        return p;
    }
    
    /**
     * @brief Create a function application pattern
     */
    static Pattern app(const std::string& func, std::vector<Pattern> kids) {
        Pattern p;
        p.isVariable = false;
        p.name = func;
        p.children = std::move(kids);
        return p;
    }
    
    /**
     * @brief Create a leaf pattern (constant)
     */
    static Pattern leaf(const std::string& name) {
        return app(name, {});
    }
};

/**
 * @brief Substitution: variable name -> e-class ID
 */
using Substitution = std::unordered_map<std::string, EClassId>;

/**
 * @brief Match a pattern against an e-graph class
 * 
 * @param egraph The e-graph to search
 * @param classId The class to match against
 * @param pattern The pattern to match
 * @return Vector of all matching substitutions
 */
inline std::vector<Substitution> matchPattern(
    const EGraph& egraph,
    EClassId classId,
    const Pattern& pattern
) {
    classId = egraph.find(classId);
    const EClass* eclass = egraph.getClass(classId);
    if (!eclass) return {};
    
    std::vector<Substitution> results;
    
    if (pattern.isVariable) {
        // Variable matches any class
        Substitution sub;
        sub[pattern.name] = classId;
        results.push_back(std::move(sub));
    } else {
        // Try to match each node in the class
        for (const ENode& node : eclass->nodes) {
            if (egraph.symbolName(node.symbol) != pattern.name) continue;
            if (node.children.size() != pattern.children.size()) continue;
            
            // Try to match all children
            std::vector<std::vector<Substitution>> childSubs(node.children.size());
            bool allMatched = true;
            
            for (size_t i = 0; i < node.children.size() && allMatched; ++i) {
                childSubs[i] = matchPattern(egraph, node.children[i], pattern.children[i]);
                if (childSubs[i].empty()) {
                    allMatched = false;
                }
            }
            
            if (!allMatched) continue;
            
            // Cap Cartesian product to prevent combinatorial explosion
            constexpr size_t MAX_CROSS = 50'000;
            
            // Compute cross product of child substitutions
            std::vector<Substitution> combined = {{}};
            for (const auto& subs : childSubs) {
                std::vector<Substitution> newCombined;
                newCombined.reserve(std::min(combined.size() * subs.size(), MAX_CROSS));
                for (const Substitution& existing : combined) {
                    for (const Substitution& childSub : subs) {
                        Substitution merged = existing;
                        bool compatible = true;
                        
                        for (const auto& [var, cid] : childSub) {
                            auto it = merged.find(var);
                            if (it != merged.end()) {
                                if (egraph.find(it->second) != egraph.find(cid)) {
                                    compatible = false;
                                    break;
                                }
                            } else {
                                merged[var] = cid;
                            }
                        }
                        
                        if (compatible) {
                            newCombined.push_back(std::move(merged));
                            if (newCombined.size() >= MAX_CROSS) goto crossDone;
                        }
                    }
                }
                crossDone:
                combined = std::move(newCombined);
            }
            
            for (auto& sub : combined) {
                results.push_back(std::move(sub));
            }
        }
    }
    
    return results;
}

} // namespace egraph
} // namespace autodiscover

// ===========================================================================
// REWRITE RULES & EQUALITY SATURATION ENGINE
// ===========================================================================

namespace autodiscover {
namespace egraph {

/**
 * @brief A rewrite rule: pattern LHS  pattern RHS
 * 
 * When the LHS pattern matches an e-class with substitution ,
 * instantiate RHS under , add to e-graph, and merge with the
 * matched e-class.
 * 
 * Provenance is tracked for proof extraction.
 */
struct RewriteRule {
    std::string name;         // Human-readable rule name
    Pattern lhs;              // Left-hand side pattern
    Pattern rhs;              // Right-hand side pattern
    size_t axiomIndex;        // Index into axiom table (for proof certificates)
    bool bidirectional;       // If true, also apply rhs  lhs
    
    // Statistics
    mutable size_t applications = 0;
    mutable size_t mergesTriggered = 0;
    
    RewriteRule() : axiomIndex(0), bidirectional(false) {}
    
    RewriteRule(const std::string& n, Pattern l, Pattern r, 
                size_t axIdx = 0, bool bidir = false)
        : name(n), lhs(std::move(l)), rhs(std::move(r)), 
          axiomIndex(axIdx), bidirectional(bidir) {}
    
    /**
     * @brief Validate that Var(RHS) is a subset of Var(LHS)
     * 
     * A rewrite rule is well-formed iff every variable appearing in the RHS
     * also appears in the LHS. Otherwise, instantiation would require creating
     * unbound fresh nodes, breaking soundness.
     * 
     * @return true if the rule is well-formed
     */
    [[nodiscard]] bool validate() const {
        std::unordered_set<std::string> lhsVars;
        collectVars(lhs, lhsVars);
        
        std::unordered_set<std::string> rhsVars;
        collectVars(rhs, rhsVars);
        
        for (const auto& v : rhsVars) {
            if (!lhsVars.count(v)) return false;
        }
        
        // If bidirectional, also check reverse direction
        if (bidirectional) {
            for (const auto& v : lhsVars) {
                if (!rhsVars.count(v)) return false;
            }
        }
        
        return true;
    }
    
    /**
     * @brief Validate and throw if malformed
     */
    void validateOrThrow() const {
        if (!validate()) {
            std::unordered_set<std::string> lhsVars, rhsVars;
            collectVars(lhs, lhsVars);
            collectVars(rhs, rhsVars);
            std::string extra;
            for (const auto& v : rhsVars) {
                if (!lhsVars.count(v)) {
                    if (!extra.empty()) extra += ", ";
                    extra += v;
                }
            }
            throw std::logic_error(
                "RewriteRule '" + name + "' is malformed: "
                "RHS variables {" + extra + "} not bound by LHS");
        }
    }
    
private:
    static void collectVars(const Pattern& p, std::unordered_set<std::string>& vars) {
        if (p.isVariable) {
            vars.insert(p.name);
        }
        for (const auto& child : p.children) {
            collectVars(child, vars);
        }
    }
};

/**
 * @brief Instantiate a pattern under a substitution in the e-graph
 * 
 * Recursively builds the RHS term in the e-graph using the variable
 * bindings from e-matching.
 * 
 * @param eg The e-graph to add nodes to
 * @param pattern The pattern to instantiate
 * @param subst Variable  EClassId mapping
 * @return The e-class ID of the instantiated term
 */
inline EClassId instantiatePattern(
    EGraph& eg,
    const Pattern& pattern,
    const Substitution& subst
) {
    if (pattern.isVariable) {
        auto it = subst.find(pattern.name);
        if (it != subst.end()) {
            return it->second;
        }
        // Unbound variable on RHS  this is a soundness hole.
        // A well-formed rewrite rule must have Var(RHS)  Var(LHS).
        // If we reach here, the rule is malformed. Return INVALID_ECLASS
        // and let the caller handle it, rather than silently introducing
        // a fresh e-class that is not justified by matching.
        throw std::logic_error(
            "instantiatePattern: unbound variable '" + pattern.name + 
            "' on RHS  Var(RHS)  Var(LHS), rule is malformed");
    }
    
    // Function application: instantiate children, then build node
    std::vector<EClassId> childIds;
    childIds.reserve(pattern.children.size());
    for (const auto& child : pattern.children) {
        childIds.push_back(instantiatePattern(eg, child, subst));
    }
    
    return eg.addApp(pattern.name, childIds);
}

/**
 * @brief Equality saturation scheduler
 * 
 * Repeatedly applies rewrite rules to an e-graph until either:
 * - No new merges are triggered (fixpoint)
 * - Iteration limit is reached
 * - Node limit is exceeded
 * 
 * This is the core "egg-style" equality saturation loop.
 */
class Saturator {
public:
    struct Config {
        size_t maxIterations = 30;     // Maximum saturation iterations
        size_t maxNodes = 100000;      // E-graph node cap (prevents blowup)
        size_t maxAppliesPerRule = 5000; // Max rule applications per rule per iteration
        bool deterministic = true;     // Deterministic iteration order
    };
    
    struct Stats {
        size_t iterations = 0;
        size_t totalMatches = 0;
        size_t totalMerges = 0;
        size_t totalNewNodes = 0;
        bool reachedFixpoint = false;
        bool hitNodeLimit = false;
    };
    
    Saturator() : config_() {}
    explicit Saturator(Config cfg) : config_(std::move(cfg)) {}
    
    /**
     * @brief Run equality saturation on the given e-graph with the given rules
     * 
     * @param eg The e-graph (modified in place)
     * @param rules The rewrite rules to apply
     * @return Saturation statistics
     */
    Stats saturate(EGraph& eg, const std::vector<RewriteRule>& rules) {
        Stats stats;
        
        for (size_t iter = 0; iter < config_.maxIterations; ++iter) {
            stats.iterations = iter + 1;
            
            if (eg.numNodes() > config_.maxNodes) {
                stats.hitNodeLimit = true;
                break;
            }
            
            size_t mergesBefore = eg.mergeHistory().size();
            size_t nodesBefore = eg.numNodes();
            
            // Apply each rule across all e-classes
            for (const auto& rule : rules) {
                applyRule(eg, rule, stats);
                
                // Also apply reverse direction if bidirectional
                if (rule.bidirectional) {
                    RewriteRule reverse(rule.name + "_rev", rule.rhs, rule.lhs, 
                                       rule.axiomIndex, false);
                    applyRule(eg, reverse, stats);
                }
                
                // Check node limit after EACH rule (catch mid-iteration blowup)
                if (eg.numNodes() > config_.maxNodes) {
                    stats.hitNodeLimit = true;
                    break;
                }
            }
            
            if (stats.hitNodeLimit) break;
            
            // Rebuild to restore congruence closure
            eg.rebuild();
            
            size_t newMerges = eg.mergeHistory().size() - mergesBefore;
            size_t newNodes = eg.numNodes() - nodesBefore;
            
            stats.totalMerges += newMerges;
            stats.totalNewNodes += newNodes;
            
            // Check fixpoint: no new merges means we've saturated
            if (newMerges == 0 && newNodes == 0) {
                stats.reachedFixpoint = true;
                break;
            }
        }
        
        return stats;
    }
    
private:
    Config config_;
    
    /**
     * @brief Apply a single rewrite rule across all e-classes
     */
    void applyRule(EGraph& eg, const RewriteRule& rule, Stats& stats) {
        // Collect all canonical class IDs (snapshot to avoid invalidation)
        std::vector<EClassId> classIds;
        for (EClassId i = 0; i < static_cast<EClassId>(eg.numClasses()); ++i) {
            if (eg.find(i) == i) {
                classIds.push_back(i);
            }
        }
        
        size_t appliesThisRule = 0;
        
        for (EClassId classId : classIds) {
            // E-match the LHS pattern against this class
            auto matches = matchPattern(eg, classId, rule.lhs);
            stats.totalMatches += matches.size();
            
            for (const auto& subst : matches) {
                // Per-rule-per-iteration budget
                if (appliesThisRule >= config_.maxAppliesPerRule) break;
                
                // Instantiate RHS under this substitution
                EClassId rhsClass = instantiatePattern(eg, rule.rhs, subst);
                
                // Merge the matched class with the RHS class
                EClassId lhsClass = eg.find(classId);
                if (eg.find(lhsClass) != eg.find(rhsClass)) {
                    eg.merge(lhsClass, rhsClass, 
                             MergeReason::fromAxiom(rule.axiomIndex, rule.name));
                    rule.applications++;
                    rule.mergesTriggered++;
                    appliesThisRule++;
                }
            }
            
            if (appliesThisRule >= config_.maxAppliesPerRule) break;
        }
    }
};

} // namespace egraph
} // namespace autodiscover
