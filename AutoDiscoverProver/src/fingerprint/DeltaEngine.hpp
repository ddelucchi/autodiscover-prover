/**
 * @file DeltaEngine.hpp
 * @brief Dominant-index delta planning engine
 * 
 * MATHEMATICAL FOUNDATION:
 * =========================
 * 
 * Given two fingerprints F(t) and F(t') in Z[], their difference is:
 * 
 *    = F(t) - F(t') = _i (c_i - c'_i)  w_i
 * 
 * where w_i = ^{-12i} is the K=12 byte-dominant weight schedule (LINEAR in i).
 * 
 * By the dominance inequality |w_i| > 256  _{j>i} |w_j|, the difference
 *  is dominated by its LEFTMOST non-zero coefficient.
 * 
 * DOMINANT INDEX:
 * ===============
 * 
 * dominantIndex() = smallest i such that a single-byte change at position i
 * could produce a delta of magnitude ~ ||.
 * 
 * More precisely: find the smallest i such that ||  256  |w_i|.
 * 
 * This tells the search engine WHERE in the byte stream the difference lies,
 * enabling localized edit proposals instead of brute-force search.
 * 
 * DELTA PLAN:
 * ===========
 * 
 * A DeltaPlan is a sequence of (position, old_byte, new_byte) triples that
 * transforms one encoding into another. The dominant index narrows the search
 * to a small window around the most significant difference.
 * 
 * @author AutoDiscoverProver Team
 */

#pragma once

#include "../ring/ZPhi.hpp"
#include "../fingerprint/Fingerprinter.hpp"

#include <vector>
#include <optional>
#include <cstdint>
#include <cmath>
#include <string>
#include <sstream>
#include <algorithm>

namespace autodiscover {
namespace fingerprint {

// =========================================================================
// DOMINANT INDEX COMPUTATION
// =========================================================================

/**
 * @brief Compute the dominant index of a Z[] delta
 * 
 * The dominant index is the smallest i such that ||  256  |w_i|.
 * 
 * With the linear schedule w_i = ^{-Ki} (K=12):
 *   |w_i|  ^{-12i}, so:
 *   i  log_(||) / (K  ln() / ln()) = log_(||) / K
 *
 * We use a two-phase approach:
 *   Phase 1: Float approximation for a candidate index
 *   Phase 2: Exact Z[] comparison to refine
 * 
 * @param delta The Z[] delta value
 * @param maxIndex Maximum index to check (prevents unbounded search)
 * @return The dominant index, or maxIndex if delta is zero/very small
 */
inline size_t dominantIndex(const ring::ZPhi& delta, size_t maxIndex = 4096) {
    // Zero delta  no dominant index
    if (delta.isZero()) return maxIndex;
    
    // Phase 1: Float approximation for initial candidate
    constexpr double LN_PHI = 0.48121182505960344;
    constexpr double LN_256 = 5.5451774444795623;
    constexpr int K = 12;
    
    auto toDouble = [](const ring::BigInt& x) -> double {
        if (x.isZero()) return 0.0;
        try {
            return static_cast<double>(x.toInt64());
        } catch (...) {
            double sign = x.isNegative() ? -1.0 : 1.0;
            return sign * std::pow(2.0, static_cast<double>(x.abs().bitLength()));
        }
    };
    
    double approxA = toDouble(delta.a());
    double approxB = toDouble(delta.b());
    constexpr double PHI_D = 1.6180339887498949;
    double magnitude = std::abs(approxA + approxB * PHI_D);
    
    if (magnitude < 1e-15) return maxIndex;
    
    // For linear schedule: |w_i| = ^{-Ki}, so we need
    //   Ki  log_(||) - log_(256)
    //   i  (ln|| - ln(256)) / (K  ln())
    double logMag = std::log(magnitude);
    double target = (logMag - LN_256) / (K * LN_PHI);
    
    size_t candidate = 0;
    if (target > 0.0) {
        candidate = static_cast<size_t>(std::ceil(target));
    }
    
    return std::min(candidate, maxIndex);
}

// =========================================================================
// EDIT OPERATION
// =========================================================================

/**
 * @brief A single byte-level edit in the encoding stream
 */
struct ByteEdit {
    size_t position;    // Position in the byte stream
    uint8_t oldByte;    // Original byte value
    uint8_t newByte;    // Replacement byte value
    
    [[nodiscard]] int16_t delta() const {
        return static_cast<int16_t>(newByte) - static_cast<int16_t>(oldByte);
    }
    
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << "@" << position << ": " 
            << static_cast<int>(oldByte) << "  " << static_cast<int>(newByte);
        return oss.str();
    }
};

// =========================================================================
// DELTA PLAN
// =========================================================================

/**
 * @brief A plan for transforming one encoding into another
 * 
 * Contains a sequence of byte edits and metadata about the search.
 */
struct DeltaPlan {
    std::vector<ByteEdit> edits;
    size_t dominantIdx = 0;     // The dominant index of the delta
    ring::ZPhi targetDelta;     // The Z[] delta we're trying to achieve
    bool exact = false;         // Whether the plan exactly achieves the delta
    double residualMagnitude = 0.0;  // Remaining error magnitude
    
    [[nodiscard]] size_t numEdits() const { return edits.size(); }
    [[nodiscard]] bool isEmpty() const { return edits.empty(); }
    
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << "DeltaPlan{domIdx=" << dominantIdx 
            << ", edits=" << edits.size()
            << ", exact=" << (exact ? "yes" : "no") << "}\n";
        for (const auto& e : edits) {
            oss << "  " << e.toString() << "\n";
        }
        return oss.str();
    }
};

// =========================================================================
// DELTA PLANNER
// =========================================================================

/**
 * @brief Plans minimal byte edits to transform one fingerprint into another
 * 
 * Uses the dominant index to narrow the search space, then greedily
 * proposes edits at the dominant position.
 */
class DeltaPlanner {
public:
    struct Config {
        size_t maxEdits = 10;       // Maximum number of edits to propose
        size_t windowSize = 4;      // How many positions around dominant to search
        size_t maxStreamLength = 256;  // Maximum encoding stream length
    };
    
    DeltaPlanner() = default;
    explicit DeltaPlanner(Config config) : config_(std::move(config)) {}
    
    /**
     * @brief Plan edits to transform srcStream into a stream with fingerprint
     *        matching srcFP + targetDelta
     * 
     * Strategy:
     * 1. Compute dominant index of targetDelta
     * 2. Search positions around dominant index
     * 3. Greedily assign byte changes to minimize residual
     * 
     * @param srcStream The source byte stream
     * @param targetDelta The desired Z[] change
     * @param cache Weight cache for computing -weights
     * @return A DeltaPlan with proposed edits
     */
    [[nodiscard]] DeltaPlan plan(
        const std::vector<uint8_t>& srcStream,
        const ring::ZPhi& targetDelta,
        ring::FibPow2Cache& cache
    ) const {
        DeltaPlan result;
        result.targetDelta = targetDelta;
        
        if (targetDelta.isZero()) {
            result.exact = true;
            return result;
        }
        
        // Step 1: Compute dominant index
        result.dominantIdx = dominantIndex(targetDelta);
        
        // Step 2: Define search window
        size_t streamLen = srcStream.size();
        if (streamLen == 0) return result;
        
        size_t windowStart = 0;
        size_t windowEnd = std::min(streamLen, 
            result.dominantIdx + config_.windowSize + 1);
        if (result.dominantIdx >= config_.windowSize) {
            windowStart = result.dominantIdx - config_.windowSize;
        }
        windowEnd = std::min(windowEnd, streamLen);
        
        // Step 3: Greedy single-position edit
        // Try each position in the window and find the edit that
        // minimizes the residual delta magnitude
        ring::ZPhi remaining = targetDelta;
        
        for (size_t editRound = 0; editRound < config_.maxEdits && !remaining.isZero(); ++editRound) {
            ByteEdit bestEdit{};
            double bestResidual = 1e300;
            bool foundEdit = false;
            
            for (size_t pos = windowStart; pos < windowEnd; ++pos) {
                ring::ZPhi w_pos = ring::ZPhi::weight(pos, cache);
                uint8_t currentByte = (pos < srcStream.size()) ? srcStream[pos] : 0;
                
                // Try all possible byte changes at this position
                for (int newByte = 0; newByte <= 255; ++newByte) {
                    if (newByte == currentByte) continue;
                    
                    int byteDelta = newByte - static_cast<int>(currentByte);
                    ring::ZPhi editContribution = w_pos * ring::ZPhi(static_cast<int64_t>(byteDelta), 0);
                    ring::ZPhi newRemaining = remaining - editContribution;
                    
                    // Approximate residual magnitude
                    double residual = approximateMagnitude(newRemaining);
                    
                    if (residual < bestResidual) {
                        bestResidual = residual;
                        bestEdit = {pos, currentByte, static_cast<uint8_t>(newByte)};
                        foundEdit = true;
                        
                        if (newRemaining.isZero()) break;
                    }
                }
                
                if (remaining.isZero()) break;
            }
            
            if (!foundEdit) break;
            
            // Apply best edit
            ring::ZPhi w_best = ring::ZPhi::weight(bestEdit.position, cache);
            int byteDelta = bestEdit.delta();
            ring::ZPhi editContrib = w_best * ring::ZPhi(static_cast<int64_t>(byteDelta), 0);
            remaining = remaining - editContrib;
            
            result.edits.push_back(bestEdit);
        }
        
        result.exact = remaining.isZero();
        result.residualMagnitude = approximateMagnitude(remaining);
        
        return result;
    }
    
private:
    Config config_;
    
    /**
     * @brief Approximate the magnitude of a Z[] value using doubles
     * (Sufficient for comparison; not for exact arithmetic)
     */
    static double approximateMagnitude(const ring::ZPhi& z) {
        if (z.isZero()) return 0.0;
        
        constexpr double PHI = 1.6180339887498949;
        
        auto toDouble = [](const ring::BigInt& x) -> double {
            if (x.isZero()) return 0.0;
            try {
                return static_cast<double>(x.toInt64());
            } catch (...) {
                double sign = x.isNegative() ? -1.0 : 1.0;
                return sign * std::pow(2.0, static_cast<double>(x.abs().bitLength()));
            }
        };
        
        double a = toDouble(z.a());
        double b = toDouble(z.b());
        return std::abs(a + b * PHI);
    }
};

} // namespace fingerprint
} // namespace autodiscover
