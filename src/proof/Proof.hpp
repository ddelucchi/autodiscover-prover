/**
 * @file Proof.hpp
 * @brief DAG-structured proof objects for equational reasoning
 * 
 * Proofs are represented as directed acyclic graphs where:
 * - Nodes are proof steps (inference applications)
 * - Edges represent dependencies (premises)
 * - Leaves are axioms or hypotheses
 * - The root is the proven goal
 * 
 * Each proof step records:
 * - The inference rule applied
 * - The premises used
 * - The conclusion derived
 * - The substitution/unifier used
 */

#ifndef AUTODISCOVER_PROOF_PROOF_HPP
#define AUTODISCOVER_PROOF_PROOF_HPP

#include "../logic/Equation.hpp"
#include "../logic/MatcherUnifier.hpp"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <sstream>
#include <queue>

namespace autodiscover {
namespace proof {

using logic::Equation;
using logic::Substitution;
using core::Term;

/**
 * @brief Inference rule identifier
 */
enum class InferenceRule : uint8_t {
    Axiom,              // Leaf: axiom from theory
    Hypothesis,         // Leaf: goal negation
    SuperpositionLeft,  // l=r, s=t  s[r]_p=t
    SuperpositionRight, // l=r, s=t  s=t[r]_p
    EqualityResolution, // s=t with (s)=(t)  
    Demodulation,       // Simplification by oriented rule
    GODNormalization,   // Canonicalization by GOD operator
    Reflexivity,        // t = t
    Symmetry,           // s = t  t = s
    Transitivity,       // s = t, t = u  s = u
    Congruence,         // s=t, ..., s=t  f(s,...,s)=f(t,...,t)
    Substitution,       // Apply substitution to equation
};

/**
 * @brief String name of inference rule
 */
inline std::string ruleName(InferenceRule rule) {
    switch (rule) {
        case InferenceRule::Axiom: return "Axiom";
        case InferenceRule::Hypothesis: return "Hypothesis";
        case InferenceRule::SuperpositionLeft: return "Superposition";
        case InferenceRule::SuperpositionRight: return "Superposition";
        case InferenceRule::EqualityResolution: return "EqRes";
        case InferenceRule::Demodulation: return "Demod";
        case InferenceRule::GODNormalization: return "GOD";
        case InferenceRule::Reflexivity: return "Refl";
        case InferenceRule::Symmetry: return "Symm";
        case InferenceRule::Transitivity: return "Trans";
        case InferenceRule::Congruence: return "Cong";
        case InferenceRule::Substitution: return "Subst";
    }
    return "?";
}

/**
 * @brief Superposition certificate  replay data for deterministic verification
 *
 * Carries all metadata needed to reconstruct a superposition inference step
 * without searching. The proof checker uses these fields to replay the step
 * exactly: fetch the correct premise sides, locate the matched subterm via
 * the position path, verify the MGU, and confirm the conclusion matches.
 */
struct SuperpositionCert {
    std::vector<uint32_t> positionPath;  ///< Path from root to rewritten subterm
    bool rewriteIntoLhs = true;          ///< true: rewrite happened in LHS of premise2
    bool eqUsedLtoR     = true;          ///< true: used l->r from premise1; false: r->l
};

/**
 * @brief Demodulation certificate  replay data for simplification steps
 */
struct DemodulationCert {
    std::vector<uint32_t> positionPath;  ///< Path to rewritten subterm in target
    bool ruleLtoR = true;                ///< Direction of the rewrite rule used
};

/**
 * @brief A single proof step
 *
 * Each step records the inference rule, premises, conclusion, substitution,
 * and (for superposition/demodulation) explicit replay certificates that
 * allow the proof checker to deterministically reconstruct the conclusion.
 */
class ProofStep {
public:
    using Id = uint32_t;
    
    ProofStep(Id id, InferenceRule rule, const Equation* conclusion)
        : id_(id), rule_(rule), conclusion_(conclusion) {}
    
    [[nodiscard]] Id id() const { return id_; }
    [[nodiscard]] InferenceRule rule() const { return rule_; }
    [[nodiscard]] const Equation* conclusion() const { return conclusion_; }
    [[nodiscard]] const std::vector<Id>& premises() const { return premises_; }
    [[nodiscard]] const Substitution& substitution() const { return substitution_; }
    [[nodiscard]] const std::string& annotation() const { return annotation_; }
    
    // --- Certificate accessors ---
    [[nodiscard]] const SuperpositionCert& superpositionCert() const { return supCert_; }
    [[nodiscard]] const DemodulationCert& demodCert() const { return demodCert_; }
    [[nodiscard]] bool hasSuperpositionCert() const { return !supCert_.positionPath.empty(); }
    [[nodiscard]] bool hasDemodulationCert() const { return !demodCert_.positionPath.empty(); }
    
    void addPremise(Id premiseId) { premises_.push_back(premiseId); }
    void setSubstitution(Substitution sub) { substitution_ = std::move(sub); }
    void setAnnotation(std::string ann) { annotation_ = std::move(ann); }
    void setSuperpositionCert(SuperpositionCert cert) { supCert_ = std::move(cert); }
    void setDemodulationCert(DemodulationCert cert) { demodCert_ = std::move(cert); }

private:
    Id id_;
    InferenceRule rule_;
    const Equation* conclusion_;
    std::vector<Id> premises_;
    Substitution substitution_;
    std::string annotation_;
    SuperpositionCert supCert_;     ///< Filled for Superposition steps
    DemodulationCert demodCert_;    ///< Filled for Demodulation steps
};

/**
 * @brief Complete proof as a DAG
 */
class Proof {
public:
    Proof() = default;
    
    /**
     * @brief Add an axiom step (leaf)
     */
    ProofStep::Id addAxiom(const Equation* eq, const std::string& name = "") {
        return addStep(InferenceRule::Axiom, eq, {}, name);
    }
    
    /**
     * @brief Add a hypothesis step (negated goal)
     */
    ProofStep::Id addHypothesis(const Equation* eq, const std::string& name = "") {
        return addStep(InferenceRule::Hypothesis, eq, {}, name);
    }
    
    /**
     * @brief Add a superposition step with full replay certificate
     */
    ProofStep::Id addSuperposition(
        const Equation* conclusion,
        ProofStep::Id premise1,
        ProofStep::Id premise2,
        bool leftSide,
        const Substitution& sub = {},
        SuperpositionCert cert = {}) {
        
        auto rule = leftSide ? InferenceRule::SuperpositionLeft 
                             : InferenceRule::SuperpositionRight;
        auto id = addStep(rule, conclusion, {premise1, premise2});
        steps_[id]->setSubstitution(sub);
        steps_[id]->setSuperpositionCert(std::move(cert));
        return id;
    }
    
    /**
     * @brief Add equality resolution (derives )
     */
    ProofStep::Id addEqualityResolution(
        ProofStep::Id premise,
        const Substitution& unifier) {
        
        auto id = addStep(InferenceRule::EqualityResolution, nullptr, {premise});
        steps_[id]->setSubstitution(unifier);
        contradictionStep_ = id;
        return id;
    }
    
    /**
     * @brief Add GOD normalization step
     */
    ProofStep::Id addGODNormalization(
        const Equation* normalized,
        ProofStep::Id premise) {
        return addStep(InferenceRule::GODNormalization, normalized, {premise}, "GOD canonicalization");
    }
    
    /**
     * @brief Add demodulation (simplification) step
     */
    ProofStep::Id addDemodulation(
        const Equation* simplified,
        ProofStep::Id target,
        ProofStep::Id rule) {
        return addStep(InferenceRule::Demodulation, simplified, {target, rule});
    }
    
    /**
     * @brief Add transitivity step: s=t, t=u  s=u
     */
    ProofStep::Id addTransitivity(
        const Equation* conclusion,
        ProofStep::Id eq1,
        ProofStep::Id eq2) {
        return addStep(InferenceRule::Transitivity, conclusion, {eq1, eq2});
    }
    
    /**
     * @brief Add congruence step
     */
    ProofStep::Id addCongruence(
        const Equation* conclusion,
        const std::vector<ProofStep::Id>& premises) {
        return addStep(InferenceRule::Congruence, conclusion, premises);
    }
    
    /**
     * @brief Get a proof step by ID
     */
    [[nodiscard]] const ProofStep* getStep(ProofStep::Id id) const {
        auto it = steps_.find(id);
        return it != steps_.end() ? it->second.get() : nullptr;
    }
    
    /**
     * @brief Get root step (contradiction or final goal)
     */
    [[nodiscard]] ProofStep::Id root() const { return contradictionStep_; }
    
    /**
     * @brief Check if proof is complete (derives )
     */
    [[nodiscard]] bool isComplete() const { return contradictionStep_ != 0; }
    
    /**
     * @brief Get all steps in proof (deterministic: topological order)
     * 
     * Returns steps in topological order (leaves first) for deterministic
     * iteration. Never iterate the internal unordered_map directly.
     */
    [[nodiscard]] std::vector<const ProofStep*> allSteps() const {
        auto order = topologicalOrder();
        std::vector<const ProofStep*> result;
        result.reserve(order.size());
        for (auto id : order) {
            const ProofStep* s = getStep(id);
            if (s) result.push_back(s);
        }
        return result;
    }
    
    /**
     * @brief Get proof size (number of steps)
     */
    [[nodiscard]] size_t size() const { return steps_.size(); }
    
    /**
     * @brief Get proof depth (longest path from leaf to root)
     */
    [[nodiscard]] size_t depth() const {
        if (steps_.empty()) return 0;
        
        std::unordered_map<ProofStep::Id, size_t> depths;
        
        for (auto stepId : topologicalOrder()) {
            const ProofStep* step = getStep(stepId);
            if (!step) continue;
            
            size_t maxPremiseDepth = 0;
            for (auto premiseId : step->premises()) {
                if (depths.count(premiseId)) {
                    maxPremiseDepth = std::max(maxPremiseDepth, depths[premiseId]);
                }
            }
            depths[stepId] = maxPremiseDepth + 1;
        }
        
        size_t maxDepth = 0;
        for (const auto& [id, d] : depths) {
            maxDepth = std::max(maxDepth, d);
        }
        return maxDepth;
    }
    
    /**
     * @brief Get steps in topological order (leaves first, then derived)
     */
    [[nodiscard]] std::vector<ProofStep::Id> topologicalOrder() const {
        std::vector<ProofStep::Id> result;
        result.reserve(steps_.size());
        
        // Compute in-degrees (number of premises)
        std::unordered_map<ProofStep::Id, size_t> inDegree;
        std::unordered_map<ProofStep::Id, std::vector<ProofStep::Id>> children;
        
        for (const auto& [id, step] : steps_) {
            inDegree[id] = step->premises().size();
            for (auto premiseId : step->premises()) {
                children[premiseId].push_back(id);
            }
        }
        
        // Start with leaves (steps with no premises), SORTED BY ID for determinism.
        // Using a min-heap (priority_queue with greater<>) ensures deterministic
        // ordering regardless of unordered_map iteration order.
        std::priority_queue<ProofStep::Id, std::vector<ProofStep::Id>, 
                            std::greater<ProofStep::Id>> ready;
        for (const auto& [id, degree] : inDegree) {
            if (degree == 0) {
                ready.push(id);
            }
        }
        
        // Process in deterministic topological order (smallest ID first at each level)
        while (!ready.empty()) {
            auto id = ready.top();
            ready.pop();
            result.push_back(id);
            
            auto it = children.find(id);
            if (it != children.end()) {
                for (auto childId : it->second) {
                    if (--inDegree[childId] == 0) {
                        ready.push(childId);
                    }
                }
            }
        }
        
        return result;
    }
    
    /**
     * @brief Format proof as string
     */
    [[nodiscard]] std::string toString() const {
        std::ostringstream ss;
        ss << "=== Proof (" << steps_.size() << " steps) ===\n";
        
        // Use topological order for deterministic output
        for (auto stepId : topologicalOrder()) {
            const ProofStep* step = getStep(stepId);
            if (step) ss << formatStep(*step) << "\n";
        }
        
        if (contradictionStep_ != 0) {
            ss << "QED (contradiction at step " << contradictionStep_ << ")\n";
        }
        
        return ss.str();
    }

private:
    std::unordered_map<ProofStep::Id, std::unique_ptr<ProofStep>> steps_;
    ProofStep::Id nextId_ = 1;
    ProofStep::Id contradictionStep_ = 0;
    
    ProofStep::Id addStep(InferenceRule rule, const Equation* conclusion,
                          const std::vector<ProofStep::Id>& premises,
                          const std::string& annotation = "") {
        ProofStep::Id id = nextId_++;
        auto step = std::make_unique<ProofStep>(id, rule, conclusion);
        for (auto premiseId : premises) {
            step->addPremise(premiseId);
        }
        if (!annotation.empty()) {
            step->setAnnotation(annotation);
        }
        steps_[id] = std::move(step);
        return id;
    }
    
    [[nodiscard]] std::string formatStep(const ProofStep& step) const {
        std::ostringstream ss;
        ss << step.id() << ". ";
        
        if (step.conclusion()) {
            ss << step.conclusion()->toString();
        } else {
            ss << " (contradiction)";
        }
        
        ss << "  [" << ruleName(step.rule());
        
        if (!step.premises().empty()) {
            ss << ": ";
            for (size_t i = 0; i < step.premises().size(); ++i) {
                if (i > 0) ss << ", ";
                ss << step.premises()[i];
            }
        }
        
        ss << "]";
        
        if (!step.annotation().empty()) {
            ss << " // " << step.annotation();
        }
        
        return ss.str();
    }
};

} // namespace proof
} // namespace autodiscover

#endif // AUTODISCOVER_PROOF_PROOF_HPP
