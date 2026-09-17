#ifndef AUTODISCOVER_CANON_GOD_FINAL_HPP
#define AUTODISCOVER_CANON_GOD_FINAL_HPP

/**
 * @file GODfinal.hpp
 * @brief GOD^GOD iterated canonical fixed-point with cycle detection
 * 
 * MATHEMATICAL FOUNDATION
 * ========================
 * 
 * The GOD operator (Galois Orbit Descent) picks the shortlex-minimal
 * representative from the Galois orbit of a term.  However, a single
 * pass of GOD may produce a term that is not yet in full normal form
 * because:
 * 
 *   1. GOD may create new -reduction opportunities  Phi pass runs
 *   2. Phi reduction may change AC structure  AC pass runs
 *   3. AC may reveal new Alpha opportunities  Alpha pass runs
 *   4. After all sub-passes, GOD orbit may have changed  loop
 * 
 * GODfinal implements the iterated fixed-point:
 * 
 *   GOD^(t) = lim_{n} (GOD  NF_sub)^n (t)
 * 
 * with cycle detection to guarantee termination.
 * 
 * TERMINATION ARGUMENT
 * =====================
 * 
 * Each iteration maps t to some t' where encode(t') _{shortlex} encode(t).
 * Since shortlex on finite strings is a well-order, this descending chain
 * must stabilize.  We also maintain a set of seen encodings to detect
 * cycles caused by hash-consing identity subtleties.
 * 
 * @author AutoDiscoverProver Team
 * @date 2024  
 */

#include "NFEngine.hpp"
#include "Equivalence.hpp"
#include "../core/Term.hpp"

#include <unordered_set>
#include <string>

namespace autodiscover {
namespace canon {

/**
 * @brief GOD^GOD iterated canonicalization with cycle detection
 */
class GODFinalCanonicalizer {
public:
    struct Stats {
        size_t totalCalls = 0;
        size_t totalIterations = 0;
        size_t maxIterations = 0;
        size_t cyclesDetected = 0;
    };

    /// Maximum iterations before forced termination.
    /// In practice, convergence happens in 2-5 iterations.
    static constexpr size_t MAX_ITERATIONS = 50;

    explicit GODFinalCanonicalizer(NFEngine& engine)
        : engine_(engine) {}

    /**
     * @brief Compute the fully-iterated GOD fixed point
     * 
     * Applies NF (including GOD pass) until the result stabilizes.
     * Uses encode()-based cycle detection.
     * 
     * @param term Input term
     * @return GOD-canonical term (pointer-stable via hash-consing)
     */
    [[nodiscard]] const core::Term* canonicalize(const core::Term* term) {
        if (!term) return nullptr;

        ++stats_.totalCalls;
        
        std::unordered_set<std::string> seen;
        const core::Term* current = term;
        size_t iters = 0;

        for (; iters < MAX_ITERATIONS; ++iters) {
            std::string enc = current->encode();
            if (!seen.insert(enc).second) {
                // Cycle detected  current encoding was already visited
                ++stats_.cyclesDetected;
                break;
            }

            const core::Term* next = engine_.normalize(current);
            if (next == current) {
                // Fixed point reached (pointer equality via hash-consing)
                break;
            }
            current = next;
        }

        stats_.totalIterations += iters;
        if (iters > stats_.maxIterations) {
            stats_.maxIterations = iters;
        }

        return current;
    }

    /**
     * @brief Check if two terms are GOD-equivalent
     */
    [[nodiscard]] bool areEquivalent(const core::Term* t1, const core::Term* t2) {
        return canonicalize(t1) == canonicalize(t2);
    }

    [[nodiscard]] const Stats& stats() const { return stats_; }
    void resetStats() { stats_ = Stats{}; }

private:
    NFEngine& engine_;
    Stats stats_;
};

} // namespace canon
} // namespace autodiscover

#endif // AUTODISCOVER_CANON_GOD_FINAL_HPP
