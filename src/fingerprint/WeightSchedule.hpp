#ifndef AUTODISCOVER_FINGERPRINT_WEIGHT_SCHEDULE_HPP
#define AUTODISCOVER_FINGERPRINT_WEIGHT_SCHEDULE_HPP

/**
 * @file WeightSchedule.hpp
 * @brief SINGLE SOURCE OF TRUTH for -weighted fingerprint schedules
 *
 * =============================================================================
 * THE ONE WEIGHT SCHEDULE (all other definitions MUST defer to this)
 * =============================================================================
 *
 * For byte-position i in an encoded stream:
 *
 *   w_i := ^{-Ki}    where K = 12
 *
 * This is the LINEAR schedule (exponent = K*i, not K*2^i).
 *
 * DOMINANCE THEOREM:
 * ------------------
 * Digit coefficients c_i  {0, ..., C_max} (C_max = 257 for byte+1 encoding).
 * Let r := ^{-K}. The geometric tail sum satisfies:
 *
 *   _{j>i} w_j = w_i  r/(1-r)
 *
 * Injectivity holds when:
 *
 *   C_max  r/(1-r) < 1      r < 1/(C_max + 1)
 *
 * With K=12: r = ^{-12}  0.00311 < 1/258  0.00388  
 *
 * Therefore any two distinct bounded-coefficient byte sequences produce
 * distinct fingerprints in Z[], and the dominant index (first mismatch
 * position) can be recovered by greedy decoding.
 *
 * WARNING (historical):
 * The old K=1 schedule (w_i = ^{-2^i}) was NOT injective.
 * Example collision: streams (1,3,1) and (2,1,2) both map to -5+10.
 *
 * @see ZPhi.hpp for the ring arithmetic
 * @see Fingerprinter.hpp for byte-stream fingerprinting
 * @see DeltaEngine.hpp for dominant-index delta recovery
 * @see Structural.hpp for -weighted structural encoding
 */

#include "../ring/ZPhi.hpp"
#include <cstdint>
#include <cstddef>

namespace autodiscover {
namespace fingerprint {

/**
 * @brief Single authoritative weight schedule for -weighted encodings.
 *
 * Every subsystem that needs a position weight MUST call
 * WeightSchedule::weight(i)  never compute its own exponents.
 */
class WeightSchedule {
public:
    /// Byte-dominance constant: K = log_(256) = 12
    static constexpr uint64_t K = 12;

    /// Maximum coefficient in byte+1 encoding: max(byte)+1 = 256, plus EOS  257
    static constexpr uint64_t MAX_DIGIT = 257;

    /**
     * @brief Compute the weight for byte position i.
     *
     * Returns w_i = ^{-Ki} as an exact element of Z[].
     *
     * @param i     Byte/token position index (0-based)
     * @param cache Fibonacci cache for efficient computation
     * @return      Exact weight in Z[]
     */
    [[nodiscard]] static ring::ZPhi weight(std::size_t i, const ring::FibPow2Cache& cache) {
        return ring::ZPhi::weight(i, cache);
    }

    /**
     * @brief Compute the weight using the global Fibonacci cache.
     */
    [[nodiscard]] static ring::ZPhi weight(std::size_t i) {
        return ring::ZPhi::weight(i, ring::globalFibCache());
    }

    /**
     * @brief Compute the exponent for position i: K * i
     */
    [[nodiscard]] static constexpr uint64_t exponent(std::size_t i) noexcept {
        return K * static_cast<uint64_t>(i);
    }

    /**
     * @brief Verify the dominance inequality for a given max coefficient.
     *
     * Returns true iff ^K > C_max + 1  (equivalently ^{-K} < 1/(C_max+1)),
     * guaranteeing byte-dominance injectivity.
     *
     * EXACT INTEGER CHECK (no floating point):
     *   ^K = F_{K-1} + F_K    in Z[]
     *   Numerical value = F_{K-1} + F_K  (1+5)/2
     *   We need this > M  (where M = maxCoeff + 1)
     *     F_K  5 > 2M - 2F_{K-1} - F_K   (call RHS "d")
     *   If d  0  trivially true.
     *   If d > 0  check 5F_K > d.
     *
     * For K=12: F_12=144, F_11=89, M=258.
     *   d = 516  178  144 = 194.  5144 = 103680 > 194 = 37636  
     */
    [[nodiscard]] static bool verifyDominance(uint64_t maxCoeff = MAX_DIGIT) {
        auto [Fk, Fk1] = ring::fibDoubling(K);
        // fibDoubling(K) returns (F_K, F_{K+1})
        uint64_t FkVal  = static_cast<uint64_t>(Fk);   // F_K
        uint64_t Fk1Val = static_cast<uint64_t>(Fk1);  // F_{K+1}
        uint64_t FkMinus1 = Fk1Val - FkVal;            // F_{K-1} = F_{K+1} - F_K

        uint64_t M = maxCoeff + 1;

        // d = 2M - 2F_{K-1} - F_K  (may be negative)
        int64_t d = static_cast<int64_t>(2 * M)
                  - static_cast<int64_t>(2 * FkMinus1)
                  - static_cast<int64_t>(FkVal);

        if (d <= 0) return true;  // ^K > M trivially since F_K5 > 0 > d

        // Otherwise verify 5F_K > d  (exact integer comparison)
        uint64_t lhs = 5ULL * FkVal * FkVal;
        uint64_t rhs = static_cast<uint64_t>(d) * static_cast<uint64_t>(d);
        return lhs > rhs;
    }
};

} // namespace fingerprint
} // namespace autodiscover

#endif // AUTODISCOVER_FINGERPRINT_WEIGHT_SCHEDULE_HPP
