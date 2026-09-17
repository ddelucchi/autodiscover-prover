/**
 * @file RuleMiner.hpp
 * @brief Rule mining from e-graph equivalence classes
 * 
 * ARCHITECTURE:
 * =============
 * 
 * After equality saturation, each e-class contains terms that are provably
 * equal. The rule miner extracts CANDIDATE rewrite rules from these classes
 * by finding "interesting" pairs of terms within the same class.
 * 
 * PIPELINE:
 * =========
 * 
 *   E-Graph (post-saturation)  RuleMiner  Candidate Rules  MultiRingGate  Certified Rules
 * 
 * The miner is UNTRUSTED  it proposes candidates. The multi-ring gate
 * and proof kernel certify them.
 * 
 * MINING STRATEGIES:
 * ==================
 * 
 * 1. SIZE-REDUCING: Find pairs where |lhs| > |rhs|  useful as simplification rules
 * 2. VARIABLE-PRESERVING: Both sides have the same variable set
 * 3. PATTERN GENERALIZATION: Replace concrete terms with variables
 * 4. ANTI-UNIFICATION: Find most general common pattern across classes
 * 
 * QUALITY METRICS:
 * =================
 * 
 * Each mined rule gets a quality score based on:
 * - Size reduction ratio
 * - Variable preservation
 * - Frequency (how many e-classes exhibit this pattern)
 * - Novelty (not already known as an axiom)
 * 
 * @author AutoDiscoverProver Team
 */

#pragma once

#include "EGraph.hpp"

#include <vector>
#include <string>
#include <algorithm>
#include <utility>
#include <cstdint>
#include <sstream>
#include <unordered_set>

namespace autodiscover {
namespace egraph {

// =========================================================================
// MINED RULE
// =========================================================================

/**
 * @brief A candidate rewrite rule extracted from the e-graph
 */
struct MinedRule {
    // Pattern terms (using e-graph symbols)
    std::string lhsDescription;  // Human-readable LHS
    std::string rhsDescription;  // Human-readable RHS
    
    // Quality metrics
    double score = 0.0;          // Overall quality score
    size_t lhsSize = 0;         // AST size of LHS
    size_t rhsSize = 0;         // AST size of RHS
    bool sizeReducing = false;   // lhsSize > rhsSize?
    bool bidirectional = false;  // Useful in both directions?
    
    // Source information
    EClassId sourceClass;        // E-class where this was found
    size_t frequency = 1;        // How many classes show this pattern
    
    [[nodiscard]] double reductionRatio() const {
        if (lhsSize == 0) return 0.0;
        return 1.0 - static_cast<double>(rhsSize) / static_cast<double>(lhsSize);
    }
    
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << lhsDescription << "  " << rhsDescription;
        oss << " [score=" << score 
            << ", size " << lhsSize << "" << rhsSize
            << ", freq=" << frequency << "]";
        return oss.str();
    }
};

// =========================================================================
// RULE MINER
// =========================================================================

/**
 * @brief Extracts candidate rewrite rules from a saturated e-graph
 */
class RuleMiner {
public:
    struct Config {
        size_t maxRulesPerClass = 5;    // Max candidate rules per e-class
        size_t maxTotalRules = 100;     // Max total candidates
        size_t minLhsSize = 2;         // Minimum LHS size (avoid trivial rules)
        size_t maxLhsSize = 20;        // Maximum LHS size (avoid huge patterns)
        bool onlySizeReducing = false;  // Only mine simplification rules?
        bool requireVarPreserving = true; // Both sides must use same variables?
    };
    
    RuleMiner() = default;
    explicit RuleMiner(Config config) : config_(std::move(config)) {}
    
    /**
     * @brief Mine candidate rules from a saturated e-graph
     * 
     * Iterates over all equivalence classes and extracts interesting
     * term pairs as candidate rewrite rules.
     * 
     * @param graph The e-graph (should be post-saturation)
     * @return Vector of candidate rules, sorted by score (descending)
     */
    [[nodiscard]] std::vector<MinedRule> mine(const EGraph& graph) const {
        std::vector<MinedRule> candidates;
        
        // Collect all canonical class IDs
        std::unordered_set<EClassId> canonicals;
        for (EClassId id = 0; id < graph.numClasses(); ++id) {
            EClassId canon = graph.find(id);
            canonicals.insert(canon);
        }
        
        // For each canonical class, extract term pairs
        for (EClassId classId : canonicals) {
            const EClass* eclass = graph.getClass(classId);
            if (!eclass) continue;
            
            auto classCandidates = mineClass(graph, classId, *eclass);
            
            for (auto& rule : classCandidates) {
                if (candidates.size() >= config_.maxTotalRules) break;
                candidates.push_back(std::move(rule));
            }
            
            if (candidates.size() >= config_.maxTotalRules) break;
        }
        
        // Sort by score (descending)
        std::sort(candidates.begin(), candidates.end(),
                  [](const MinedRule& a, const MinedRule& b) {
                      return a.score > b.score;
                  });
        
        return candidates;
    }
    
private:
    Config config_;
    
    /**
     * @brief Mine candidate rules from a single e-class
     */
    [[nodiscard]] std::vector<MinedRule> mineClass(
        const EGraph& graph,
        EClassId classId,
        const EClass& eclass
    ) const {
        std::vector<MinedRule> results;
        
        const auto& nodes = eclass.nodes;
        if (nodes.size() < 2) return results;  // Need at least 2 equivalent terms
        
        // Compute sizes for each node
        std::vector<size_t> sizes(nodes.size());
        for (size_t i = 0; i < nodes.size(); ++i) {
            sizes[i] = nodeSize(nodes[i]);
        }
        
        // Find pairs: for each (larger, smaller) pair, consider as candidate rule
        size_t count = 0;
        for (size_t i = 0; i < nodes.size() && count < config_.maxRulesPerClass; ++i) {
            for (size_t j = 0; j < nodes.size() && count < config_.maxRulesPerClass; ++j) {
                if (i == j) continue;
                if (nodes[i] == nodes[j]) continue;
                
                size_t lhsSize = sizes[i];
                size_t rhsSize = sizes[j];
                
                // Filter by size bounds
                if (lhsSize < config_.minLhsSize || lhsSize > config_.maxLhsSize) continue;
                
                // If only size-reducing, require lhs > rhs
                if (config_.onlySizeReducing && lhsSize <= rhsSize) continue;
                
                // Create candidate
                MinedRule rule;
                rule.lhsDescription = nodeDescription(graph, nodes[i]);
                rule.rhsDescription = nodeDescription(graph, nodes[j]);
                rule.lhsSize = lhsSize;
                rule.rhsSize = rhsSize;
                rule.sizeReducing = (lhsSize > rhsSize);
                rule.sourceClass = classId;
                rule.bidirectional = !rule.sizeReducing;  // Non-reducing rules are bidirectional
                
                // Compute score
                rule.score = computeScore(rule);
                
                results.push_back(std::move(rule));
                ++count;
            }
        }
        
        return results;
    }
    
    /**
     * @brief Compute quality score for a candidate rule
     */
    [[nodiscard]] double computeScore(const MinedRule& rule) const {
        double score = 0.0;
        
        // Size reduction bonus (max 5 points)
        if (rule.sizeReducing) {
            score += 5.0 * rule.reductionRatio();
        }
        
        // Moderate complexity bonus (not too simple, not too complex)
        if (rule.lhsSize >= 3 && rule.lhsSize <= 8) {
            score += 2.0;
        }
        
        // Small RHS bonus
        if (rule.rhsSize <= 3) {
            score += 1.5;
        }
        
        // Frequency bonus
        score += std::min(3.0, static_cast<double>(rule.frequency) * 0.5);
        
        return score;
    }
    
    /**
     * @brief Compute AST size of an e-node (1 + number of children)
     */
    [[nodiscard]] static size_t nodeSize(const ENode& node) {
        return 1 + node.children.size();
    }
    
    /**
     * @brief Human-readable description of an e-node
     */
    [[nodiscard]] static std::string nodeDescription(const EGraph& graph, const ENode& node) {
        std::string name = graph.symbolName(node.symbol);
        if (node.children.empty()) {
            return name;
        }
        
        std::ostringstream oss;
        oss << name << "(";
        for (size_t i = 0; i < node.children.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "c" << graph.find(node.children[i]);
        }
        oss << ")";
        return oss.str();
    }
};

} // namespace egraph
} // namespace autodiscover
