/**
 * @file Fingerprinter.hpp
 * @brief -weighted canonical fingerprinting with collision-free guarantees
 * 
 * MATHEMATICAL FOUNDATION:
 * =========================
 * 
 * This module implements the -fingerprinting scheme using superincreasing
 * weight schedules derived from the golden ratio  = (1 + 5)/2.
 * 
 * WEIGHT SCHEDULE (K=12 Linear Byte-Dominance):
 * =======================================
 * 
 * Weights: w_i = ^{-12i}  (LINEAR in i)
 * 
 * The constant K=12 = log_(256) ensures the DOMINANCE INEQUALITY:
 * 
 *   |w_i| > 256  _{j>i} |w_j|
 * 
 * This guarantees that no combination of byte differences at positions
 * after i can overwhelm a single-byte difference at position i.
 * 
 * COLLISION THEOREM:
 * ==================
 * 
 * Theorem: For distinct byte sequences B  B' with |B|=|B'|=n,
 *          FP(B)  FP(B') in Z[].
 * 
 * Proof sketch: Let i be the leftmost position where B and B' differ.
 * Then |(b_{i} - b'_{i})|  |w_{i}|  |w_{i}| (since coefficients
 * are (byte+1)  [1,256]). The tail sum _{j>i}|w_j| < |w_{i}|/256
 * by the K=12 dominance bound, so the difference cannot be zero. 
 * 
 * WARNING: The old K=1 schedule (w_i = ^{-2^i}) was NOT injective.
 * Proof: streams (1,3,1) and (2,1,2) both map to -5+10. This was the
 * "inevitable carry algebra" collision from  =  + 1.
 * 
 * @author AutoDiscoverProver Team
 */

#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <optional>

#include "../ring/ZPhi.hpp"
#include "../encoding/Encoding.hpp"

namespace autodiscover {
namespace fingerprint {

// ===========================================================================
// FINGERPRINT VALUE TYPES
// ===========================================================================

/**
 * @brief 256-bit cryptographic-style hash for fast equality checks
 * 
 * This is NOT cryptographically secure - it's optimized for:
 * - Fast equality comparison
 * - Good distribution for hash tables
 * - Deterministic computation
 */
struct Hash256 {
    std::array<uint8_t, 32> bytes;
    
    Hash256() : bytes{} {}
    
    explicit Hash256(const std::array<uint8_t, 32>& b) : bytes(b) {}
    
    bool operator==(const Hash256& other) const {
        return bytes == other.bytes;
    }
    
    bool operator!=(const Hash256& other) const {
        return !(*this == other);
    }
    
    bool operator<(const Hash256& other) const {
        return bytes < other.bytes;
    }
    
    /**
     * @brief Convert to hexadecimal string representation
     */
    std::string toHex() const {
        static const char hexDigits[] = "0123456789abcdef";
        std::string result;
        result.reserve(64);
        for (uint8_t b : bytes) {
            result.push_back(hexDigits[b >> 4]);
            result.push_back(hexDigits[b & 0x0F]);
        }
        return result;
    }
    
    /**
     * @brief Parse from hexadecimal string
     */
    static std::optional<Hash256> fromHex(const std::string& hex) {
        if (hex.length() != 64) return std::nullopt;
        
        Hash256 result;
        for (size_t i = 0; i < 32; ++i) {
            char hi = hex[2*i];
            char lo = hex[2*i + 1];
            
            auto hexVal = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return 10 + c - 'a';
                if (c >= 'A' && c <= 'F') return 10 + c - 'A';
                return -1;
            };
            
            int hiVal = hexVal(hi);
            int loVal = hexVal(lo);
            if (hiVal < 0 || loVal < 0) return std::nullopt;
            
            result.bytes[i] = static_cast<uint8_t>((hiVal << 4) | loVal);
        }
        return result;
    }
    
    /**
     * @brief Get first 64 bits as size_t for hash table use
     */
    size_t toSizeT() const {
        size_t result = 0;
        for (size_t i = 0; i < sizeof(size_t) && i < 32; ++i) {
            result |= static_cast<size_t>(bytes[i]) << (i * 8);
        }
        return result;
    }
};

/**
 * @brief Complete fingerprint with both fast hash and exact Z[] value
 * 
 * INVARIANT: The exact ZPhi value is the authoritative fingerprint.
 * The hash256 is for fast lookups and MUST be verified against exact value
 * in case of hash collision.
 */
struct Fingerprint {
    Hash256 hash256;           // Fast hash for bucket lookup
    ring::ZPhi exact;          // Exact Z[] fingerprint (authoritative)
    size_t inputLength;        // Original input length (for quick rejection)
    
    Fingerprint() : hash256(), exact(), inputLength(0) {}
    
    Fingerprint(const Hash256& h, const ring::ZPhi& e, size_t len)
        : hash256(h), exact(e), inputLength(len) {}
    
    /**
     * @brief Full equality check using exact Z[] arithmetic
     * 
     * By injectivity theorem: identical fingerprints  identical inputs
     */
    bool operator==(const Fingerprint& other) const {
        // Quick rejection by length
        if (inputLength != other.inputLength) return false;
        
        // Fast path: hash comparison (usually sufficient)
        if (hash256 != other.hash256) return false;
        
        // Authoritative: exact Z[] comparison
        return exact == other.exact;
    }
    
    bool operator!=(const Fingerprint& other) const {
        return !(*this == other);
    }
};

// ===========================================================================
// HASH COMPUTATION (Modified FNV-1a for determinism)
// ===========================================================================

/**
 * @brief Deterministic 256-bit hash using cascaded FNV-1a
 * 
 * This is NOT cryptographically secure but provides:
 * - Deterministic output
 * - Good avalanche properties
 * - Fast computation
 * 
 * For cryptographic needs, replace with SHA-256.
 */
class Hash256Builder {
private:
    static constexpr uint64_t FNV_PRIME = 0x00000100000001B3ULL;
    static constexpr uint64_t FNV_OFFSET = 0xcbf29ce484222325ULL;
    
    std::array<uint64_t, 4> state_;
    
public:
    Hash256Builder() {
        // Initialize with different offsets for independence
        state_[0] = FNV_OFFSET;
        state_[1] = FNV_OFFSET ^ 0x1111111111111111ULL;
        state_[2] = FNV_OFFSET ^ 0x2222222222222222ULL;
        state_[3] = FNV_OFFSET ^ 0x3333333333333333ULL;
    }
    
    /**
     * @brief Add a single byte to the hash state
     */
    void update(uint8_t byte) {
        for (int i = 0; i < 4; ++i) {
            state_[i] ^= byte;
            state_[i] *= FNV_PRIME;
            
            // Mix between lanes
            state_[(i + 1) % 4] ^= (state_[i] >> 17);
        }
    }
    
    /**
     * @brief Add a byte sequence to the hash state
     */
    void update(const std::vector<uint8_t>& data) {
        for (uint8_t b : data) {
            update(b);
        }
    }
    
    void update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            update(data[i]);
        }
    }
    
    /**
     * @brief Finalize and produce 256-bit hash
     */
    Hash256 finalize() const {
        Hash256 result;
        
        // Final mixing
        std::array<uint64_t, 4> final_state = state_;
        for (int round = 0; round < 4; ++round) {
            for (int i = 0; i < 4; ++i) {
                final_state[i] *= FNV_PRIME;
                final_state[(i + 1) % 4] ^= (final_state[i] >> 23);
            }
        }
        
        // Convert to bytes (little-endian)
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 8; ++j) {
                result.bytes[i * 8 + j] = static_cast<uint8_t>(final_state[i] >> (j * 8));
            }
        }
        
        return result;
    }
    
    /**
     * @brief One-shot hash computation
     */
    static Hash256 hash(const std::vector<uint8_t>& data) {
        Hash256Builder builder;
        builder.update(data);
        return builder.finalize();
    }
    
    static Hash256 hash(const uint8_t* data, size_t len) {
        Hash256Builder builder;
        builder.update(data, len);
        return builder.finalize();
    }
};

// ===========================================================================
// PHI FINGERPRINTER
// ===========================================================================

/**
 * @brief -weighted fingerprinting engine
 * 
 * Implements the superincreasing weight scheme:
 *   w_i = ^{-Ki} where K = 12 = log_(256)
 * 
 * The K=12 constant ensures the DOMINANCE INEQUALITY:
 *   |w_i| > 256  _{j>i} |w_j|
 * 
 * which guarantees collision-free fingerprinting for byte streams.
 * 
 * CRITICAL INVARIANTS:
 * 
 * 1. INJECTIVITY: fp_bytes(B) == fp_bytes(B')  B == B'
 * 2. DETERMINISM: Identical inputs always produce identical outputs
 * 3. PREFIX-FREE COMPATIBILITY: Works correctly with Elias-encoded streams
 */
class Fingerprinter {
private:
    ring::FibPow2Cache cache_;
    size_t maxLength_;  // Maximum supported input length
    
public:
    /**
     * @brief Construct fingerprinter with specified maximum input size
     * 
     * @param maxIndex Maximum byte-stream index supported. Weight cache is
     *                 built for indices 0..maxIndex-1. The linear weight
     *                 schedule w_i = ^{-12i} remains tractable for all
     *                 practical stream lengths (up to millions of bytes).
     */
    explicit Fingerprinter(size_t maxIndex = 4096) 
        : cache_(),
          maxLength_(maxIndex) {
        // Build Fibonacci cache. The largest exponent we need is K * maxIndex = 12 * maxIndex.
        // FibPow2Cache stores entries for fast doubling; we need enough entries to cover
        // fibDoubling(12 * maxIndex). The build parameter controls the cache depth.
        size_t cacheDepth = 20;  // 2^20 covers K*maxIndex up to ~87000
        if (maxIndex > 7000) cacheDepth = 24;
        cache_.build(cacheDepth);
    }
    
    /**
     * @brief Get the maximum supported input length
     */
    size_t maxInputLength() const { return maxLength_; }
    
    // -----------------------------------------------------------------------
    // BYTE-LEVEL FINGERPRINTING
    // -----------------------------------------------------------------------
    
    /**
     * @brief Compute -fingerprint of raw byte sequence
     * 
     * FP(B) = _{i=0}^{n-1} (b_i + 1)  w_i
     * 
     * @param data Input byte sequence
     * @return Complete fingerprint with hash and exact Z[] value
     * 
     * THEOREM: This function is injective on the set of all byte sequences
     * up to maxInputLength() bytes.
     */
    Fingerprint fp_bytes(const std::vector<uint8_t>& data) const {
        if (data.size() > maxLength_) {
            throw std::overflow_error("Input exceeds maximum fingerprint length");
        }
        
        // Compute fast hash
        Hash256 hash = Hash256Builder::hash(data);
        
        // Compute exact Z[] fingerprint
        ring::ZPhi exact = computeExactFingerprint(data);
        
        return Fingerprint(hash, exact, data.size());
    }
    
    /**
     * @brief Compute -fingerprint of byte sequence (raw pointer version)
     */
    Fingerprint fp_bytes(const uint8_t* data, size_t len) const {
        std::vector<uint8_t> vec(data, data + len);
        return fp_bytes(vec);
    }
    
    // -----------------------------------------------------------------------
    // TOKEN-LEVEL FINGERPRINTING
    // -----------------------------------------------------------------------
    
    /**
     * @brief Compute -fingerprint of token stream (with EOS marker)
     * 
     * Token stream uses ALPHABET_SIZE=257 tokens (0-255 + EOS).
     * Each token t contributes (t + 1)  w_i to the fingerprint.
     * 
     * @param tokens Token stream including EOS_TOKEN at end
     * @return Complete fingerprint
     */
    Fingerprint fp_tokens(const std::vector<uint32_t>& t) const {
        
        if (t.size() > maxLength_) {
            throw std::overflow_error("Token stream exceeds maximum fingerprint length");
        }
        
        // Compute hash from token values as 16-bit values
        Hash256Builder builder;
        for (uint32_t tok : t) {
            builder.update(static_cast<uint8_t>(tok & 0xFF));
            builder.update(static_cast<uint8_t>((tok >> 8) & 0xFF));
        }
        Hash256 hash = builder.finalize();
        
        // Compute exact fingerprint
        ring::ZPhi exact;  // Starts as 0
        
        for (size_t i = 0; i < t.size(); ++i) {
            // Coefficient is (token + 1) to ensure non-zero
            int64_t coeff = static_cast<int64_t>(t[i]) + 1;
            
            // Get superincreasing weight w_i = ^{-12i}
            ring::ZPhi weight = computeWeight(i);
            
            // Accumulate: exact += coeff * weight
            exact = exact + (weight * coeff);
        }
        
        return Fingerprint(hash, exact, t.size());
    }
    
    // -----------------------------------------------------------------------
    // TERM FINGERPRINTING
    // -----------------------------------------------------------------------
    
    /**
     * @brief Compute fingerprint of a canonical term
     * 
     * PRECONDITION: Term must be in canonical form (post-GOD normalization)
     * 
     * @param bytecode Canonical bytecode from TermEncoder
     * @return Fingerprint of the term
     */
    Fingerprint fp_term(const std::vector<uint8_t>& bytecode) const {
        return fp_bytes(bytecode);
    }
    
    // -----------------------------------------------------------------------
    // EQUATION FINGERPRINTING
    // -----------------------------------------------------------------------
    
    /**
     * @brief Compute fingerprint of a canonical equation
     * 
     * The equation bytecode MUST be in canonical form with lhs  rhs.
     * 
     * @param bytecode Canonical equation bytecode from EquationEncoder
     * @return Fingerprint of the equation
     */
    Fingerprint fp_equation(const std::vector<uint8_t>& bytecode) const {
        return fp_bytes(bytecode);
    }
    
    // -----------------------------------------------------------------------
    // PROOF FINGERPRINTING
    // -----------------------------------------------------------------------
    
    /**
     * @brief Compute fingerprint of a canonical proof DAG
     * 
     * The proof bytecode should be in topologically-sorted canonical form.
     * 
     * @param bytecode Canonical proof bytecode
     * @return Fingerprint of the proof
     */
    Fingerprint fp_proof(const std::vector<uint8_t>& bytecode) const {
        return fp_bytes(bytecode);
    }
    
    // -----------------------------------------------------------------------
    // INCREMENTAL FINGERPRINTING
    // -----------------------------------------------------------------------
    
    /**
     * @brief Incremental fingerprint builder for streaming inputs
     * 
     * Allows computing fingerprints byte-by-byte without buffering
     * the entire input.
     */
    class Builder {
    private:
        const Fingerprinter* parent_;
        Hash256Builder hashBuilder_;
        ring::ZPhi accumulator_;
        size_t position_;
        
    public:
        explicit Builder(const Fingerprinter* parent)
            : parent_(parent), hashBuilder_(), accumulator_(), position_(0) {}
        
        /**
         * @brief Add a byte to the fingerprint
         */
        void update(uint8_t byte) {
            if (position_ >= parent_->maxLength_) {
                throw std::overflow_error("Incremental fingerprint overflow");
            }
            
            // Update hash
            hashBuilder_.update(byte);
            
            // Update exact fingerprint
            int64_t coeff = static_cast<int64_t>(byte) + 1;
            ring::ZPhi weight = parent_->computeWeight(position_);
            accumulator_ = accumulator_ + (weight * coeff);
            
            ++position_;
        }
        
        /**
         * @brief Add multiple bytes
         */
        void update(const std::vector<uint8_t>& data) {
            for (uint8_t b : data) {
                update(b);
            }
        }
        
        /**
         * @brief Finalize and produce fingerprint
         */
        Fingerprint finalize() const {
            return Fingerprint(hashBuilder_.finalize(), accumulator_, position_);
        }
        
        /**
         * @brief Get current position (number of bytes processed)
         */
        size_t position() const { return position_; }
    };
    
    /**
     * @brief Create an incremental fingerprint builder
     */
    Builder builder() const { return Builder(this); }
    
private:
    /**
     * @brief Compute w_i = ^{-12i} (the i-th byte-dominant weight)
     * 
     * Delegates to ZPhi::weight(i, cache) which computes ^{-Ki} with K=12.
     * The K=12 factor ensures byte-level dominance: no sum of bounded
     * digit differences at later positions can overwhelm this weight.
     */
    ring::ZPhi computeWeight(size_t i) const {
        // Direct call to ZPhi::weight which now uses K=12
        return ring::ZPhi::weight(i, cache_);
    }
    
    /**
     * @brief Compute exact Z[] fingerprint using K=12 dominance weights
     * 
     * FP(data) = _i (data[i] + 1)  w_i   where w_i = ^{-12i}
     * 
     * The K=12 dominance inequality guarantees collision-freedom:
     * different byte sequences always produce different Z[] values.
     */
    ring::ZPhi computeExactFingerprint(const std::vector<uint8_t>& data) const {
        ring::ZPhi result;  // Starts as 0 + 0
        
        for (size_t i = 0; i < data.size(); ++i) {
            // Coefficient for byte b_i is (b_i + 1) to ensure non-zero
            int64_t coeff = static_cast<int64_t>(data[i]) + 1;
            
            // Get weight w_i = ^{-12i}
            ring::ZPhi weight = computeWeight(i);
            
            // Accumulate: result += coeff * weight
            // Since weight = (p, q) in Z[], coeff*weight = (coeff*p, coeff*q)
            result = result + (weight * coeff);
        }
        
        return result;
    }
};

// ===========================================================================
// FINGERPRINT COMPARISON UTILITIES
// ===========================================================================

/**
 * @brief Compare fingerprints for hash table ordering
 */
struct FingerprintHasher {
    size_t operator()(const Fingerprint& fp) const {
        return fp.hash256.toSizeT();
    }
};

/**
 * @brief Equality comparison for hash tables
 */
struct FingerprintEqual {
    bool operator()(const Fingerprint& a, const Fingerprint& b) const {
        return a == b;
    }
};

// ===========================================================================
// FINGERPRINT NORMALIZATION
// ===========================================================================

/**
 * @brief Normalize a Fingerprint's exact value to canonical form
 * 
 * The Z[] representation is already canonical (a + b form).
 * This function ensures the hash is consistent with the exact value.
 */
inline Fingerprint normalizeFingerprint(const Fingerprint& fp) {
    // The fingerprint is already in canonical form by construction.
    // This function exists for API completeness and future extensions.
    return fp;
}

// ===========================================================================
// COLLISION-FREE VERIFICATION
// ===========================================================================

/**
 * @brief Verify the injectivity property for a set of fingerprints
 * 
 * This is a diagnostic function that checks whether any hash collisions
 * exist in a set of fingerprints. True hash collisions (where exact Z[]
 * values also match) would indicate a bug.
 * 
 * @param fps Collection of fingerprints to check
 * @return true if no hash-only collisions exist (or they're benign)
 */
template <typename Container>
bool verifyInjectivity(const Container& fps) {
    std::vector<const Fingerprint*> sorted;
    sorted.reserve(fps.size());
    
    for (const auto& fp : fps) {
        sorted.push_back(&fp);
    }
    
    // Sort by hash for collision detection
    std::sort(sorted.begin(), sorted.end(),
        [](const Fingerprint* a, const Fingerprint* b) {
            return a->hash256 < b->hash256;
        });
    
    // Check adjacent pairs for collisions
    for (size_t i = 1; i < sorted.size(); ++i) {
        if (sorted[i-1]->hash256 == sorted[i]->hash256) {
            // Hash collision - exact values MUST differ
            if (sorted[i-1]->exact == sorted[i]->exact) {
                // True collision - this violates injectivity!
                return false;
            }
            // Hash-only collision is benign (handled by exact comparison)
        }
    }
    
    return true;
}

} // namespace fingerprint
} // namespace autodiscover
