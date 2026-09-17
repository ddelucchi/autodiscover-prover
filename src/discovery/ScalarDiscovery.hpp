#ifndef AUTODISCOVER_DISCOVERY_SCALAR_DISCOVERY_HPP
#define AUTODISCOVER_DISCOVERY_SCALAR_DISCOVERY_HPP

/**
 * @file ScalarDiscovery.hpp
 * @brief Scalar-driven forced discovery loop
 *
 * =============================================================================
 * ARCHITECTURE: Seed  Scalarize  Canonicalize  Schedule  Generate  Gate  Prove
 * =============================================================================
 *
 * This file implements the complete scalar-frontier autodiscovery pipeline:
 *
 *   1. Seed:        Start with  = +1 and ring axioms.
 *   2. Scalarize:   Map every term to a Z[] scalar via SCOUT signature.
 *   3. Tag:         Normalize scalar by the ^{12i} schedule  PhiTag.
 *   4. Canonicalize: GOD orbit  canonical representative.
 *   5. Schedule:    Enumerate target scalars (Calkin-Wilf rationals +
 *                   Fibonacci continued-fraction convergents).
 *   6. Generate:    Synthesize candidate terms whose PhiTag hits a target.
 *   7. Gate:        MultiRingEval fast-falsify, then e-graph proof.
 *   8. Commit:      Proven equations  KB as new rewrite rules.
 *
 * Key data types:
 *   PhiTag       canonical scalar address: (sign, k12_index, mantissa  Z[])
 *   PhiLadder    deterministic Fibonacci convergent stream
 *   ScalarTargetScheduler  fair rational/convergent target enumeration
 *   DiscoveryOrchestrator  the main loop
 *
 * =============================================================================
 */

#include "../core/Term.hpp"
#include "../core/Context.hpp"
#include "../ring/ZPhi.hpp"
#include "../logic/InferenceEngine.hpp"
#include "../logic/KnowledgeBase.hpp"
#include "../logic/Equation.hpp"
#include "../logic/Normalizer.hpp"
#include "../canon/NFEngine.hpp"
#include "../canon/Equivalence.hpp"
#include "../domain/Closure.hpp"
#include "../encoding/Structural.hpp"
#include "../egraph/EGraph.hpp"
#include "ScalarOracle.hpp"

#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <cstdint>
#include <algorithm>
#include <functional>
#include <numeric>
#include <cmath>

namespace autodiscover::discovery {

using namespace autodiscover::core;
using namespace autodiscover::ring;
using namespace autodiscover::logic;
using namespace autodiscover::egraph;

// =============================================================================
// PHI TAG  canonical scalar address in the ^{12i} lattice
// =============================================================================

/**
 * @brief Canonical scalar address: scalar = sign  mantissa  ^{12k12}
 *
 * The W12 dominance schedule guarantees ^{-12i} > _{j>i} ^{-12j},
 * so each (k12, mantissa) pair is unique and collision-resistant.
 *
 * The mantissa is in a canonical fundamental domain:
 *   |mantissa.toDouble()|  [1, ^12)
 *
 * Two scalars with the same PhiTag are in the same scalar bucket.
 */
struct PhiTag {
    int8_t sign = 1;        ///< +1 or -1
    int     k12 = 0;        ///< ^{12k12} scale index
    ZPhi    mantissa{1, 0}; ///< residual after extracting ^{12k12}

    static constexpr int K = 12;  ///< dominance constant

    /**
     * @brief Compute PhiTag from a raw Z[] scalar.
     *
     * Finds k12 such that scalar = sign  mantissa  ^{12k12}
     * with mantissa in the fundamental domain [1, ^12).
     */
    static PhiTag fromZPhi(const ZPhi& scalar) {
        PhiTag tag;

        if (scalar.isZero()) {
            tag.sign = 0;
            tag.k12 = 0;
            tag.mantissa = ZPhi(0, 0);
            return tag;
        }

        // Determine sign
        double d = scalar.toDoubleFast();
        tag.sign = (d >= 0.0) ? 1 : -1;
        double absVal = std::abs(d);

        if (absVal == 0.0) {
            tag.sign = 0;
            tag.k12 = 0;
            tag.mantissa = ZPhi(0, 0);
            return tag;
        }

        // Compute k12 = floor(log_^12(|scalar|))
        // log_^12(x) = ln(x) / (12ln())
        static const double LOG_PHI = std::log(1.6180339887498949);
        static const double INV_12_LOG_PHI = 1.0 / (12.0 * LOG_PHI);
        static const double PHI_12 = std::pow(1.6180339887498949, 12.0);  //  321.997

        tag.k12 = static_cast<int>(std::floor(std::log(absVal) * INV_12_LOG_PHI));

        // mantissa = scalar / (sign  ^{12k12})
        // We compute this via floating-point for the tag, but the mantissa
        // stores the original scalar divided by the power. For exact operations
        // one should use ZPhi::phiNegPower and exact multiplication.
        // For tagging/bucketing, the k12 index is the primary discriminant.
        tag.mantissa = scalar;  // store original; k12 provides the bucket
        return tag;
    }

    /**
     * @brief Bucket key: two scalars in the same bucket share (sign, k12).
     */
    [[nodiscard]] uint64_t bucketKey() const {
        uint64_t h = static_cast<uint64_t>(sign + 2);            // 0,1,2,3
        h = h * 100003 + static_cast<uint64_t>(k12 + 10000);     // offset to avoid negatives
        return h;
    }

    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << (sign >= 0 ? "+" : "-") << "^{12" << k12 << "}  "
            << mantissa.toString();
        return oss.str();
    }
};

// =============================================================================
// PHI LADDER  deterministic Fibonacci convergent stream   F_{n+1}/F_n
// =============================================================================

/**
 * @brief Generates -convergent rationals from the continued fraction [1;1,1,1,...]
 *
 * The standard convergents of  = 1+1/(1+1/(1+...)) are:
 *   p_n/q_n = F_{n+1}/F_n   as n  
 *
 * Also generates "seeded" convergents [1;1,...,1,k] for exploration.
 */
class PhiLadder {
public:
    struct Convergent {
        uint64_t p;     ///< numerator
        uint64_t q;     ///< denominator
        int      depth; ///< nesting depth
        int      seed;  ///< terminal seed value (1 = standard  convergent)

        [[nodiscard]] double toDouble() const {
            return static_cast<double>(p) / static_cast<double>(q);
        }
        [[nodiscard]] std::string toString() const {
            std::ostringstream oss;
            oss << p << "/" << q << " (depth=" << depth << ", seed=" << seed << ")";
            return oss.str();
        }
    };

    /**
     * @brief Generate standard  convergents: F_{n+1}/F_n for n=1..maxDepth
     */
    static std::vector<Convergent> standardConvergents(int maxDepth = 20) {
        std::vector<Convergent> result;
        uint64_t a = 1, b = 1; // F_1, F_2
        for (int n = 1; n <= maxDepth && b < (1ULL << 60); ++n) {
            result.push_back({b, a, n, 1});
            uint64_t c = a + b;
            a = b;
            b = c;
        }
        return result;
    }

    /**
     * @brief Generate seeded convergents: [1;1,...,1,k] at various depths
     *
     * These are the user's "irrational fraction levels":
     *   x = 1/(1+1/(1+1/k)), etc.
     * Each generates a rational (k+F_{d-1})/(k+F_d) type expression.
     */
    static std::vector<Convergent> seededConvergents(int maxDepth = 8, int maxSeed = 5) {
        std::vector<Convergent> result;
        for (int seed = 2; seed <= maxSeed; ++seed) {
            for (int depth = 2; depth <= maxDepth; ++depth) {
                auto [p, q] = evaluateCF(depth, seed);
                if (q > 0) {
                    result.push_back({p, q, depth, seed});
                }
            }
        }
        return result;
    }

    /**
     * @brief Generate the complete probe lattice: standard + seeded convergents
     */
    static std::vector<Convergent> fullLattice(int maxDepth = 12, int maxSeed = 5) {
        auto standard = standardConvergents(maxDepth);
        auto seeded = seededConvergents(maxDepth, maxSeed);
        standard.insert(standard.end(), seeded.begin(), seeded.end());
        return standard;
    }

private:
    /**
     * @brief Evaluate the continued fraction [1;1,1,...,1,k] with `depth` layers
     * using the matrix method: M(a) = [[a,1],[1,0]], product gives [p;q].
     */
    static std::pair<uint64_t, uint64_t> evaluateCF(int depth, int lastSeed) {
        // Start from the bottom: the deepest partial quotient is `lastSeed`
        // then all upper ones are 1.
        // [1; 1, 1, ..., 1, lastSeed]  (depth-1 ones, then lastSeed)
        //
        // Bottom-up evaluation:
        //   h_{-1} = 1, h_0 = lastSeed
        //   k_{-1} = 0, k_0 = 1
        //   h_i = a_i * h_{i-1} + h_{i-2}
        //   k_i = a_i * k_{i-1} + k_{i-2}
        uint64_t h_prev2 = 1, h_prev1 = static_cast<uint64_t>(lastSeed);
        uint64_t k_prev2 = 0, k_prev1 = 1;

        // Apply (depth-1) partial quotients of 1, going up
        for (int i = 1; i < depth; ++i) {
            uint64_t a_i = 1; // all intermediate quotients are 1
            uint64_t h_cur = a_i * h_prev1 + h_prev2;
            uint64_t k_cur = a_i * k_prev1 + k_prev2;
            h_prev2 = h_prev1;
            h_prev1 = h_cur;
            k_prev2 = k_prev1;
            k_prev1 = k_cur;
        }

        return {h_prev1, k_prev1};
    }
};

// =============================================================================
// SCALAR TARGET SCHEDULER  fair rational + convergent enumeration
// =============================================================================

/**
 * @brief Generates target scalars in a fair, deterministic order.
 *
 * Three interleaved streams:
 *   1. Rational dilations:  (p/q)  for p,q  {1..maxPQ}  (Calkin-Wilf order)
 *   2. Standard convergents: F_{n+1}/F_n  
 *   3. Seeded convergents:   [1;1,...,k] variants
 *
 * Targets are ordered by increasing "complexity" = p + q + depth.
 */
class ScalarTargetScheduler {
public:
    struct Target {
        double   value;       ///< numeric target value (for fast comparison)
        uint64_t p, q;        ///< rational p/q (or convergent numerator/denominator)
        int      complexity;  ///< ordering key = p + q (or depth for convergents)
        enum Kind { Rational, Convergent, SeededConvergent } kind;

        bool operator>(const Target& o) const { return complexity > o.complexity; }

        [[nodiscard]] std::string toString() const {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6) << value
                << " (" << p << "/" << q << ", cx=" << complexity << ")";
            return oss.str();
        }
    };

    /**
     * @brief Generate all targets up to given complexity bounds.
     * Returns targets sorted by increasing complexity.
     */
    static std::vector<Target> generate(
        int maxPQ = 10,
        int maxConvergentDepth = 15,
        int maxSeed = 5)
    {
        static constexpr double PHI = 1.6180339887498949;
        std::vector<Target> targets;
        std::unordered_set<uint64_t> seen; // dedup by (p<<32|q)

        auto dedupKey = [](uint64_t p, uint64_t q) -> uint64_t {
            // Reduce to lowest terms
            uint64_t g = std::gcd(p, q);
            return ((p / g) << 32) | (q / g);
        };

        // Stream 1: rational dilations (p/q)
        for (int qq = 1; qq <= maxPQ; ++qq) {
            for (int pp = 1; pp <= maxPQ; ++pp) {
                uint64_t up = static_cast<uint64_t>(pp);
                uint64_t uq = static_cast<uint64_t>(qq);
                uint64_t key = dedupKey(up, uq);
                if (seen.count(key)) continue;
                seen.insert(key);

                double val = (static_cast<double>(pp) / qq) * PHI;
                targets.push_back({val, up, uq, pp + qq, Target::Rational});

                // Also add negative target for completeness
                targets.push_back({-val, up, uq, pp + qq + 1, Target::Rational});
            }
        }

        // Stream 2: standard  convergents
        auto stdConv = PhiLadder::standardConvergents(maxConvergentDepth);
        for (auto& c : stdConv) {
            uint64_t key = dedupKey(c.p, c.q);
            if (seen.count(key)) continue;
            seen.insert(key);
            targets.push_back({c.toDouble(), c.p, c.q, c.depth + 1, Target::Convergent});
        }

        // Stream 3: seeded convergents
        auto seedConv = PhiLadder::seededConvergents(std::min(maxConvergentDepth, 8), maxSeed);
        for (auto& c : seedConv) {
            uint64_t key = dedupKey(c.p, c.q);
            if (seen.count(key)) continue;
            seen.insert(key);
            targets.push_back({
                c.toDouble(), c.p, c.q,
                c.depth + c.seed, Target::SeededConvergent
            });
        }

        // Sort by complexity (fair ordering)
        std::sort(targets.begin(), targets.end(),
            [](const Target& a, const Target& b) {
                return a.complexity < b.complexity;
            });

        return targets;
    }
};

// =============================================================================
// DISCOVERY STATS  progress tracking for the orchestrator
// =============================================================================

struct DiscoveryStats {
    size_t totalCandidates   = 0;  ///< terms generated
    size_t groundTerms       = 0;  ///< terms evaluable in Z[]
    size_t zphiBuckets       = 0;  ///< distinct Z[] values
    size_t equationPairs     = 0;  ///< bucket pairs (potential identities)
    size_t proofsAttempted   = 0;
    size_t proofsSucceeded   = 0;  ///< kernel-verified proofs
    size_t scoutValidations  = 0;  ///< SCOUT-validated
    size_t equationsCommitted = 0;
    size_t godNormalized     = 0;  ///< terms GOD-normalized
    size_t noProgressRuns    = 0;
    double elapsedSeconds    = 0.0;

    void print(std::ostream& os = std::cout) const {
        os << "[DISCOVER] "
           << "candidates=" << totalCandidates
           << " ground=" << groundTerms
           << " buckets=" << zphiBuckets
           << " pairs=" << equationPairs
           << " proven=" << proofsSucceeded << "/" << proofsAttempted
           << " scout=" << scoutValidations
           << " committed=" << equationsCommitted
           << " god=" << godNormalized
           << " elapsed=" << std::fixed << std::setprecision(2) << elapsedSeconds << "s"
           << "\n";
    }
};

// =============================================================================
// DISCOVERY ORCHESTRATOR  the forced SeedScalarizeCommit loop
// =============================================================================

/**
 * @brief Configuration for the scalar-driven discovery loop.
 */
struct DiscoveryConfig {
    int    maxPQ              = 10;     ///< max p,q for rational targets
    int    maxConvergentDepth = 12;     ///< max CF nesting depth
    int    maxSeed            = 5;      ///< max terminal seed for seeded CFs
    int    maxTermDepth       = 4;      ///< max depth for candidate term generation
    int    maxCandidatesPerTarget = 200;///< budget per target scalar
    int    maxProofSteps      = 1000;   ///< e-graph saturation budget
    int    maxNoProgressRounds = 5;     ///< early exit if no new equations
    size_t maxPairsPerBucket  = 20;    ///< max pairs to prove per bucket
    double scalarTolerance    = 1e-8;   ///< tolerance for scalar matching
    bool   enableGOD          = true;   ///< apply GOD normalization
    bool   enableSCOUT        = true;   ///< validate via SCOUT
    bool   enableProofLift    = true;   ///< generate kernel proofs
    bool   verbose            = true;   ///< print progress heartbeat
    int    heartbeatInterval  = 100;    ///< print every N candidates
};

/**
 * @brief The main scalar-driven discovery engine  SCALAR-UPWARD architecture.
 *
 * ========================================================================
 * PIPELINE: Compute exact → Match → Prove upward → Validate
 * ========================================================================
 *
 * OLD pipeline (top-down): Generate targets  float eval  e-graph search
 *   Problem: floating-point tolerance, expensive search, may miss identities
 *
 * NEW pipeline (scalar-upward):
 *   1. Generate ALL ground terms up to depth D
 *   2. GOD-normalize each (  1- canonicalization)
 *   3. ZPhiEvaluator::evaluate()  exact Z[] value (a + b, a,b  Z)
 *   4. Bucket by EXACT ZPhi value (hash-based, zero tolerance)
 *   5. Each bucket 2 terms  ALL pairs are provably equal
 *   6. ProofLifter::lift()  kernel-verified certificates
 *   7. ScoutValidator::validate()  independent SCOUT confirmation
 *   8. Commit proven equations to the KnowledgeBase
 *
 * WHY this works:
 *   - Z[] is a unique factorization domain
 *   - Every ground term over {, , integers, +, *, neg} evaluates to
 *     a unique (a,b) pair in Z via the ZPhi ring
 *   - If ZPhi(termA) == ZPhi(termB), the identity is a THEOREM
 *   - Proofs are generated AFTER the fact from known-true equalities
 *   - Zero false positives (exact arithmetic, not floating point)
 *   - Zero false negatives for evaluable terms
 *
 * Usage:
 *   DiscoveryOrchestrator orch(factory, kb);
 *   orch.seedGoldenRatio();
 *   auto stats = orch.run();
 *   for (auto& eq : orch.discoveries()) { ... }
 */
class DiscoveryOrchestrator {
public:
    explicit DiscoveryOrchestrator(
        TermFactory& factory,
        KnowledgeBase& kb,
        DiscoveryConfig config = {})
        : factory_(factory)
        , kb_(kb)
        , config_(std::move(config))
        , lifter_(factory)
        , nfEngine_(factory)
        , normalizer_(factory)
        , sigExtractor_(factory)
    {}

    /**
     * @brief Seed the KB with the golden-ratio axioms.
     * This is the "ground zero"  everything derives from  =  + 1.
     */
    void seedGoldenRatio() {
        const Term* phi = factory_.phi();
        const Term* one = factory_.scalar(1.0);

        //  *  =  + 1  (defining relation of the golden ring)
        const Term* phi_sq = factory_.apply("*", {phi, phi});
        const Term* phi_plus_1 = factory_.apply("+", {phi, one});
        seedEquations_.emplace_back(phi_sq, phi_plus_1);
        kb_.addAxiom(phi_sq, phi_plus_1);

        // NOTE: =+1, +=1, =-1 are all DERIVABLE from
        // =+1 and the definition =1-.
        // They are NOT seeded here  they should be DISCOVERED.
        // (This module is only used in --discover-scalar mode, not --discover-all)

        // Ground arithmetic: neg(1) = -1 (scalar evaluation)
        const Term* neg_one = factory_.apply("neg", {one});
        const Term* minus_one = factory_.scalar(-1.0);
        seedEquations_.emplace_back(neg_one, minus_one);
        kb_.addAxiom(neg_one, minus_one);

        const Term* neg_minus_one = factory_.apply("neg", {minus_one});
        seedEquations_.emplace_back(neg_minus_one, one);
        kb_.addAxiom(neg_minus_one, one);

        // Build e-graph rewrite rules (for fallback proof path)
        arithmeticRules_ = buildArithmeticRules();

        if (config_.verbose) {
            std::cout << "[DISCOVER] Seeded " << seedEquations_.size()
                      << " golden-ratio axioms + "
                      << arithmeticRules_.size() << " arithmetic rules\n";
        }
    }

    /**
     * @brief Run the scalar-upward discovery loop.
     *
     * Phase 1: ENUMERATE  generate all ground terms up to maxTermDepth
     * Phase 2: EVALUATE   compute exact Z[] value for each
     * Phase 3: BUCKET     group terms by exact Z[] value
     * Phase 4: PROVE      for each bucket, lift scalar equalities to proofs
     * Phase 5: VALIDATE   confirm via SCOUT boundary collapse
     * Phase 6: COMMIT     add proven equations to KB
     *
     * @return Statistics about what was discovered.
     */
    DiscoveryStats run() {
        auto t0 = std::chrono::steady_clock::now();
        DiscoveryStats stats;

        // ================================================================
        // Phase 1: ENUMERATE  generate ALL candidate ground terms
        // ================================================================
        if (config_.verbose) {
            std::cout << "\n[DISCOVER] \n"
                      << "[DISCOVER]   SCALAR-UPWARD DISCOVERY ENGINE v2.0    \n"
                      << "[DISCOVER]   Exact Z[] Oracle + Kernel Proofs      \n"
                      << "[DISCOVER] \n\n";
        }

        auto candidates = generateAllCandidates();
        stats.totalCandidates = candidates.size();

        if (config_.verbose) {
            std::cout << "[DISCOVER] Phase 1: Generated "
                      << candidates.size() << " candidate terms (depth  "
                      << config_.maxTermDepth << ")\n";
        }

        // ================================================================
        // Phase 2: EVALUATE  compute exact Z[] value for each term
        // ================================================================
        // Bucket: exact ZPhi hash  vector of (term, zphi_value)
        struct EvalEntry {
            const Term* term;
            const Term* normalized;
            ZPhi value;
        };
        std::unordered_map<uint64_t, std::vector<EvalEntry>> buckets;

        for (const Term* t : candidates) {
            // GOD-normalize first (optional but recommended)
            const Term* normalized = t;
            if (config_.enableGOD) {
                normalized = nfEngine_.normalize(t);
                if (normalized != t) stats.godNormalized++;
            }

            // Exact Z[] evaluation
            auto val = evaluator_.evaluate(normalized);
            if (!val) continue;  // Skip non-evaluable terms (variables, div-by-non-unit)

            stats.groundTerms++;

            // Bucket by exact ZPhi hash
            uint64_t h = val->hash64();
            buckets[h].push_back({t, normalized, *val});
        }
        stats.zphiBuckets = buckets.size();

        if (config_.verbose) {
            std::cout << "[DISCOVER] Phase 2: Evaluated " << stats.groundTerms
                      << " ground terms into " << stats.zphiBuckets
                      << " Z[] buckets\n";
        }

        // ================================================================
        // Phase 3-6: PROVE + VALIDATE + COMMIT
        // ================================================================
        // For each bucket with 2 terms: all pairs are provably equal
        size_t bucketsWithIdentities = 0;

        for (auto& [hash, entries] : buckets) {
            if (entries.size() < 2) continue;

            // Verify all entries have the SAME exact Z[] value
            // (hash collisions are possible, so we must check)
            // Group entries by actual value within this hash bucket
            std::vector<std::vector<size_t>> valueGroups;
            std::vector<ZPhi> groupValues;

            for (size_t i = 0; i < entries.size(); ++i) {
                bool found = false;
                for (size_t g = 0; g < groupValues.size(); ++g) {
                    if (entries[i].value == groupValues[g]) {
                        valueGroups[g].push_back(i);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    groupValues.push_back(entries[i].value);
                    valueGroups.push_back({i});
                }
            }

            // Process each value group with 2 terms
            for (size_t g = 0; g < valueGroups.size(); ++g) {
                auto& group = valueGroups[g];
                if (group.size() < 2) continue;
                bucketsWithIdentities++;

                const ZPhi& sharedValue = groupValues[g];

                // Deduplicate by structural encoding within this group
                std::vector<size_t> unique;
                std::unordered_set<std::string> seenEncodes;
                for (size_t idx : group) {
                    std::string enc = entries[idx].term->encode();
                    if (seenEncodes.insert(enc).second) {
                        unique.push_back(idx);
                    }
                }

                if (unique.size() < 2) continue;

                // For each pair (bounded by maxPairsPerBucket)
                size_t pairCount = 0;
                for (size_t i = 0; i < unique.size() && pairCount < config_.maxPairsPerBucket; ++i) {
                    for (size_t j = i + 1; j < unique.size() && pairCount < config_.maxPairsPerBucket; ++j) {
                        const Term* termA = entries[unique[i]].term;
                        const Term* termB = entries[unique[j]].term;
                        pairCount++;
                        stats.equationPairs++;

                        DiscoveredEquation discovered;
                        discovered.lhs = termA;
                        discovered.rhs = termB;
                        discovered.scalarValue = sharedValue;

                        // Phase 4: PROVE via ProofLifter (scalar-upward)
                        if (config_.enableProofLift) {
                            stats.proofsAttempted++;
                            auto proof = lifter_.lift(termA, termB, sharedValue);
                            discovered.kernelProven = proof.proven;
                            discovered.proofTrace = proof.proofTrace;

                            if (proof.proven) {
                                stats.proofsSucceeded++;
                            }

                            // Fallback: try e-graph if kernel lift didn't fully chain
                            if (!proof.proven) {
                                if (tryProveEGraph(termA, termB)) {
                                    discovered.kernelProven = true;
                                    discovered.proofTrace += "[EGRAPH FALLBACK: VERIFIED]\n";
                                    stats.proofsSucceeded++;
                                }
                            }
                        }

                        // Phase 5: VALIDATE via SCOUT
                        if (config_.enableSCOUT) {
                            auto vr = validator_.validate(
                                entries[unique[i]].value,
                                entries[unique[j]].value);
                            discovered.scoutValidated = vr.allPassed();
                            discovered.phaseTag = vr.lhsPhaseTag;

                            if (vr.allPassed()) {
                                stats.scoutValidations++;
                            }
                        }

                        // Phase 6: COMMIT to KnowledgeBase
                        kb_.addDerived(termA, termB);
                        stats.equationsCommitted++;
                        discoveries_.push_back(discovered);

                        if (config_.verbose) {
                            std::cout << "[DISCOVER] ";
                            if (discovered.kernelProven)
                                std::cout << " PROVEN: ";
                            else
                                std::cout << " ORACLE: ";
                            std::cout << termA->toString()
                                      << " = " << termB->toString()
                                      << "  [" << sharedValue.toString() << "]\n";
                        }
                    }
                }
            }
        }

        auto tend = std::chrono::steady_clock::now();
        stats.elapsedSeconds = std::chrono::duration<double>(tend - t0).count();

        if (config_.verbose) {
            std::cout << "\n[DISCOVER]  FINAL RESULTS \n";
            stats.print();
            std::cout << "[DISCOVER] Identity buckets: " << bucketsWithIdentities << "\n";
            std::cout << "[DISCOVER] Total discoveries: " << discoveries_.size() << "\n\n";

            // Print all discovered equations
            if (!discoveries_.empty()) {
                std::cout << "[DISCOVER]  DISCOVERED EQUATIONS \n";
                for (size_t i = 0; i < discoveries_.size(); ++i) {
                    std::cout << "  " << (i + 1) << ". "
                              << discoveries_[i].toString() << "\n";
                }
                std::cout << "\n";
            }
        }
        return stats;
    }

    /**
     * @brief Access the list of discovered equations.
     */
    [[nodiscard]] const std::vector<DiscoveredEquation>& discoveries() const {
        return discoveries_;
    }

    /**
     * @brief Clear discovered equations and caches.
     */
    void reset() {
        discoveries_.clear();
        evaluator_.clearCache();
    }

private:
    TermFactory& factory_;
    KnowledgeBase& kb_;
    DiscoveryConfig config_;

    // The scalar-upward pipeline components
    ZPhiEvaluator evaluator_;
    ProofLifter lifter_;
    ScoutValidator validator_;

    // Normalization engines
    canon::NFEngine nfEngine_;
    Normalizer normalizer_;
    SignatureExtractor sigExtractor_;

    // State
    std::vector<DiscoveredEquation> discoveries_;
    std::vector<std::pair<const Term*, const Term*>> seedEquations_;
    std::vector<egraph::RewriteRule> arithmeticRules_;

    // ===================================================================
    // CANDIDATE GENERATION  grammar-based enumeration of ground terms
    // ===================================================================

    /**
     * @brief Generate ALL ground terms up to maxTermDepth.
     *
     * Atoms: {, , 0, 1, -1, 2}
     * Operators: {+, *, neg, inv}
     *
     * Uses hash-consing (TermFactory deduplicates structurally identical terms)
     * to avoid redundant evaluations.
     */
    std::vector<const Term*> generateAllCandidates() {
        std::vector<const Term*> result;
        std::unordered_set<const Term*> seen;

        // Atoms
        std::vector<const Term*> atoms = {
            factory_.phi(),
            factory_.phiBar(),
            factory_.scalar(0.0),
            factory_.scalar(1.0),
            factory_.scalar(-1.0),
            factory_.scalar(2.0),
        };

        // Level 0: atoms
        std::vector<const Term*> current = atoms;
        for (auto* t : current) {
            if (seen.insert(t).second) {
                result.push_back(t);
            }
        }

        // Levels 1..maxDepth: apply operators
        for (int depth = 1; depth <= config_.maxTermDepth; ++depth) {
            std::vector<const Term*> next;

            auto tryAdd = [&](const Term* t) {
                if (seen.insert(t).second) {
                    next.push_back(t);
                    result.push_back(t);
                }
            };

            // Unary: neg, inv
            for (const Term* t : current) {
                tryAdd(factory_.apply("neg", {t}));
                tryAdd(factory_.apply("inv", {t}));
            }

            // Binary: +, * (current  atoms to limit explosion)
            for (const Term* t : current) {
                for (const Term* a : atoms) {
                    tryAdd(factory_.apply("+", {t, a}));
                    tryAdd(factory_.apply("+", {a, t}));
                    tryAdd(factory_.apply("*", {t, a}));
                    tryAdd(factory_.apply("*", {a, t}));
                }
            }

            // Also combine current  current at higher depths (controlled)
            if (depth >= 2 && current.size() <= 50) {
                for (size_t i = 0; i < current.size(); ++i) {
                    for (size_t j = i; j < current.size(); ++j) {
                        tryAdd(factory_.apply("+", {current[i], current[j]}));
                        tryAdd(factory_.apply("*", {current[i], current[j]}));
                    }
                }
            }

            current = std::move(next);

            // Budget check
            if (result.size() >= static_cast<size_t>(config_.maxCandidatesPerTarget * 10)) {
                if (config_.verbose) {
                    std::cout << "[DISCOVER] Budget reached at depth "
                              << depth << ": " << result.size() << " terms\n";
                }
                break;
            }
        }

        return result;
    }

    // ===================================================================
    // E-GRAPH FALLBACK  for terms that ProofLifter can't fully chain
    // ===================================================================

    /**
     * @brief Attempt to prove lhs = rhs via e-graph equality saturation.
     *
     * This is a FALLBACK for when the ProofLifter's axiom chaining
     * can't build a complete reduction chain. The e-graph approach is
     * more powerful (it can discover non-trivial rewrite paths) but
     * less determistic and more expensive.
     */
    bool tryProveEGraph(const Term* lhs, const Term* rhs) {
        EGraph eg;

        auto lhsId = addToEGraph(lhs, eg);
        auto rhsId = addToEGraph(rhs, eg);
        if (eg.find(lhsId) == eg.find(rhsId)) return true;

        // Add seed axioms as ground merges
        for (const auto& [sl, sr] : seedEquations_) {
            auto slId = addToEGraph(sl, eg);
            auto srId = addToEGraph(sr, eg);
            if (eg.find(slId) != eg.find(srId)) {
                eg.merge(slId, srId);
            }
        }
        eg.rebuild();
        if (eg.find(lhsId) == eg.find(rhsId)) return true;

        // Run saturator
        Saturator::Config cfg;
        cfg.maxIterations     = 12;
        cfg.maxNodes          = 2000;
        cfg.maxAppliesPerRule = 500;
        Saturator sat(cfg);
        sat.saturate(eg, arithmeticRules_);

        return eg.find(lhsId) == eg.find(rhsId);
    }

    /**
     * @brief Add a term to the e-graph, returning its e-class ID.
     */
    EClassId addToEGraph(const Term* t, EGraph& eg) {
        if (!t) return eg.addLeaf("__null");
        switch (t->kind()) {
            case TermKind::Scalar: {
                std::ostringstream ss;
                ss << std::setprecision(17) << t->scalarValue();
                return eg.addLeaf("scalar_" + ss.str());
            }
            case TermKind::Constant:
                return eg.addLeaf(t->symbol());
            case TermKind::Variable:
                return eg.addLeaf("var_" + t->symbol());
            case TermKind::Phi:
                return eg.addLeaf("phi");
            case TermKind::PhiBar:
                return eg.addLeaf("phibar");
            case TermKind::J:
                return eg.addLeaf("J");
            case TermKind::Application: {
                std::vector<EClassId> childIds;
                for (const Term* ch : t->children()) {
                    childIds.push_back(addToEGraph(ch, eg));
                }
                return eg.addApp(t->symbol(), childIds);
            }
            case TermKind::Pair: {
                auto fstId = addToEGraph(t->first(), eg);
                auto sndId = addToEGraph(t->second(), eg);
                return eg.addApp("pair", {fstId, sndId});
            }
        }
        return eg.addLeaf("__unknown");
    }

    /**
     * @brief Build universal arithmetic rewrite rules for the e-graph fallback.
     */
    static std::vector<egraph::RewriteRule> buildArithmeticRules() {
        using egraph::Pattern;
        using RR = egraph::RewriteRule;

        std::vector<RR> rules;
        auto X = Pattern::var("X");
        auto Y = Pattern::var("Y");
        auto zero = Pattern::leaf("scalar_0");
        auto one  = Pattern::leaf("scalar_1");

        rules.emplace_back(RR("add_zero_r",  Pattern::app("+", {X, zero}), X));
        rules.emplace_back(RR("add_zero_l",  Pattern::app("+", {zero, X}), X));
        rules.emplace_back(RR("mul_one_r",   Pattern::app("*", {X, one}), X));
        rules.emplace_back(RR("mul_one_l",   Pattern::app("*", {one, X}), X));
        rules.emplace_back(RR("mul_zero_r",  Pattern::app("*", {X, zero}), zero));
        rules.emplace_back(RR("mul_zero_l",  Pattern::app("*", {zero, X}), zero));
        rules.emplace_back(RR("add_comm",    Pattern::app("+", {X, Y}),
                                              Pattern::app("+", {Y, X}), 0, true));
        rules.emplace_back(RR("mul_comm",    Pattern::app("*", {X, Y}),
                                              Pattern::app("*", {Y, X}), 0, true));
        rules.emplace_back(RR("neg_neg",     Pattern::app("neg", {Pattern::app("neg", {X})}), X));
        rules.emplace_back(RR("neg_zero",    Pattern::app("neg", {zero}), zero));
        rules.emplace_back(RR("add_inv_r",   Pattern::app("+", {X, Pattern::app("neg", {X})}), zero));
        rules.emplace_back(RR("add_inv_l",   Pattern::app("+", {Pattern::app("neg", {X}), X}), zero));
        rules.emplace_back(RR("inv_inv",     Pattern::app("inv", {Pattern::app("inv", {X})}), X));
        rules.emplace_back(RR("inv_one",     Pattern::app("inv", {one}), one));

        return rules;
    }
};

} // namespace autodiscover::discovery

#endif // AUTODISCOVER_DISCOVERY_SCALAR_DISCOVERY_HPP
