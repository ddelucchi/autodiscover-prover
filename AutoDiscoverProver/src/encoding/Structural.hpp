#ifndef AUTODISCOVER_ENCODING_STRUCTURAL_HPP
#define AUTODISCOVER_ENCODING_STRUCTURAL_HPP

/**
 * @file Structural.hpp
 * @brief Injective structural encoder for terms
 * 
 * Mathematical Foundation:
 * ========================
 * 
 * The Structural Encoder provides a deterministic, injective mapping:
 * 
 *   Encode : NF(T(,X))   (or [])
 * 
 * Key Properties:
 * ---------------
 * 
 * 1. INJECTIVITY: Encode(t1) = Encode(t2)  t1 = t2
 *    - Different normalized terms produce different codes
 *    - Critical for collision-free deduplication
 * 
 * 2. DETERMINISM: Encode is a pure function
 *    - Same input always produces same output
 *    - Independent of hash order, thread timing, etc.
 * 
 * 3. CANONICAL: Works on normalized terms
 *    - Encode(NF(t)) is the canonical scalar for t
 * 
 * Encoding Schemes:
 * -----------------
 * 
 * 1. POSITIONAL (Base-B): 
 *    - Tokenize term to byte sequence
 *    - Interpret as base-B number:  b  B
 *    - Pro: Simple, truly injective
 *    - Con: Codes grow exponentially with term size
 * 
 * 2. PREFIX-FREE (Elias/Huffman):
 *    - Use prefix-free coding for tokens
 *    - Concatenate codes  unique binary string  integer
 *    - Pro: More compact than positional
 *    - Con: Still grows exponentially
 * 
 * 3. ZECKENDORF (-based):
 *    - Map tokens to Fibonacci-weighted positions
 *    - Use superincreasing property: F_{k+2} > _{ik} F_i
 *    - Express result in [] for compact algebraic form
 *    - Pro: Golden ratio semantics, exact in -ring
 *    - Con: More complex to compute
 * 
 * 4. HASH-CHAIN (for large terms):
 *    - Split term into chunks
 *    - Hash each chunk, chain hashes
 *    - Final hash as code (loses injectivity, gains compactness)
 * 
 * The CanonScalar Invariant:
 * --------------------------
 * 
 *   CanonScalar(e) := Encode(NF(e))
 * 
 * This satisfies (with caveats):
 *   - e1  e2  CanonScalar(e1) = CanonScalar(e2)  [completeness  requires confluent NF]
 *   - CanonScalar(e1) = CanonScalar(e2)  e1  e2  [soundness  requires injective Encode]
 * 
 * NOTE: Completeness depends on the normalizer reaching a unique normal form
 * (confluence). Soundness depends on encoding injectivity, which holds for the
 * K=12 byte-dominance shortlex scheme but NOT for the hashed compact code.
 * 
 * @author AutoDiscover Prover
 * @date 2024
 */

#include "../core/Term.hpp"
#include "../ring/ZPhi.hpp"
#include "Encoding.hpp"

#include <vector>
#include <cstdint>
#include <memory>
#include <string>
#include <cmath>
#include <optional>
#include <variant>

namespace autodiscover {
namespace encoding {

// ===========================================================================
// ENCODING STRATEGIES
// ===========================================================================

/**
 * @brief Enumeration of encoding strategies
 */
enum class EncodingStrategy {
    POSITIONAL_256,    // Base-256 positional encoding
    POSITIONAL_65536,  // Base-65536 (wide) positional encoding
    PREFIX_FREE,       // Elias-gamma prefix-free bytecode
    ZECKENDORF,        // Fibonacci-weighted in []
    HYBRID             // Prefix-free bytecode  Zeckendorf
};

// ===========================================================================
// STRUCTURAL CODE
// ===========================================================================

/**
 * @brief A structural code representing a term
 * 
 * Can be either a BigInt (integer code) or ZPhi (golden ring element).
 */
class StructuralCode {
public:
    using IntCode = ring::BigInt;
    using PhiCode = ring::ZPhi;
    using Code = std::variant<IntCode, PhiCode>;
    
private:
    Code code_;
    size_t termSize_;  // Size of encoded term (for diagnostics)
    EncodingStrategy strategy_;
    
public:
    StructuralCode() : code_(IntCode(0)), termSize_(0), 
                       strategy_(EncodingStrategy::POSITIONAL_256) {}
    
    explicit StructuralCode(IntCode code, size_t size = 0,
                           EncodingStrategy strat = EncodingStrategy::POSITIONAL_256)
        : code_(std::move(code)), termSize_(size), strategy_(strat) {}
    
    explicit StructuralCode(PhiCode code, size_t size = 0)
        : code_(std::move(code)), termSize_(size), 
          strategy_(EncodingStrategy::ZECKENDORF) {}
    
    // -----------------------------------------------------------------------
    // ACCESSORS
    // -----------------------------------------------------------------------
    
    [[nodiscard]] bool isInt() const { return std::holds_alternative<IntCode>(code_); }
    [[nodiscard]] bool isPhi() const { return std::holds_alternative<PhiCode>(code_); }
    
    [[nodiscard]] const IntCode& asInt() const { return std::get<IntCode>(code_); }
    [[nodiscard]] const PhiCode& asPhi() const { return std::get<PhiCode>(code_); }
    
    [[nodiscard]] size_t termSize() const { return termSize_; }
    [[nodiscard]] EncodingStrategy strategy() const { return strategy_; }
    
    // -----------------------------------------------------------------------
    // COMPARISON (for deduplication)
    // -----------------------------------------------------------------------
    
    [[nodiscard]] bool operator==(const StructuralCode& other) const {
        return code_ == other.code_;
    }
    
    [[nodiscard]] bool operator!=(const StructuralCode& other) const {
        return !(*this == other);
    }
    
    [[nodiscard]] bool operator<(const StructuralCode& other) const {
        if (isInt() && other.isInt()) {
            return asInt() < other.asInt();
        }
        if (isPhi() && other.isPhi()) {
            return asPhi() < other.asPhi();
        }
        // Int < Phi arbitrarily
        return isInt();
    }
    
    // -----------------------------------------------------------------------
    // CONVERSION
    // -----------------------------------------------------------------------
    
    /// Convert to string representation
    [[nodiscard]] std::string toString() const {
        if (isInt()) {
            return asInt().toString();
        } else {
            return asPhi().toString();
        }
    }
    
    /// Convert to hex string
    [[nodiscard]] std::string toHex() const {
        if (isInt()) {
            // Use toString and convert to hex
            return asInt().toString();  // Already provides a decimal string
        } else {
            // For ZPhi, show both components
            return "ZPhi(" + asPhi().a().toString() + "," + asPhi().b().toString() + ")";
        }
    }
    
    /// Approximate numeric value (for ordering/comparison)
    /// Uses limb-based approximation directly  no toString() allocation.
    [[nodiscard]] double toDouble() const {
        if (isInt()) {
            return asInt().toDoubleFast();
        } else {
            return asPhi().toDoubleFast();
        }
    }
    
    /// Hash for use in containers  limb-based, O(limbs) not O(digits).
    [[nodiscard]] size_t hash() const {
        if (isInt()) {
            return asInt().hash64();
        } else {
            return asPhi().hash64();
        }
    }
};

} // namespace encoding 
} // namespace autodiscover

// Hash specialization
namespace std {
    template<>
    struct hash<autodiscover::encoding::StructuralCode> {
        size_t operator()(const autodiscover::encoding::StructuralCode& c) const {
            return c.hash();
        }
    };
}

namespace autodiscover {
namespace encoding {

// ===========================================================================
// STRUCTURAL ENCODER
// ===========================================================================

/**
 * @brief Injective encoder from terms to structural codes
 */
class StructuralEncoder {
private:
    EncodingStrategy strategy_;
    TermEncoder termEncoder_;  // Term encoder instance
    mutable ring::FibPow2Cache fibCache_;  // Cache for -weight computation
    
public:
    explicit StructuralEncoder(EncodingStrategy strat = EncodingStrategy::POSITIONAL_256)
        : strategy_(strat) {
        fibCache_.build(20);  // Covers K*maxIndex up to K*2^20
    }
    
    // -----------------------------------------------------------------------
    // MAIN ENCODING API
    // -----------------------------------------------------------------------
    
    /**
     * @brief Encode a term to a structural code
     * 
     * GUARANTEE: This is injective on normalized terms.
     * encode(t1) = encode(t2)  t1 = t2 (structurally identical)
     */
    [[nodiscard]] StructuralCode encode(const core::Term& term) const {
        switch (strategy_) {
            case EncodingStrategy::POSITIONAL_256:
                return encodePositional256(term);
            case EncodingStrategy::POSITIONAL_65536:
                return encodePositional65536(term);
            case EncodingStrategy::PREFIX_FREE:
                return encodePrefixFree(term);
            case EncodingStrategy::ZECKENDORF:
                return encodeZeckendorf(term);
            case EncodingStrategy::HYBRID:
                return encodeHybrid(term);
            default:
                return encodePositional256(term);
        }
    }
    
    /**
     * @brief Encode a term to a structural code (shared_ptr version)
     */
    [[nodiscard]] StructuralCode encode(const std::shared_ptr<core::Term>& term) const {
        return encode(*term);
    }
    
    // -----------------------------------------------------------------------
    // ENCODING STRATEGIES
    // -----------------------------------------------------------------------
    
    /**
     * @brief Base-256 positional encoding
     * 
     * Interprets bytecode as a base-256 number:
     *   code =  byte[i]  256^i
     */
    [[nodiscard]] StructuralCode encodePositional256(const core::Term& term) const {
        auto bytes = termEncoder_.encode(&term);
        
        // Convert bytes to BigInt (little-endian)
        ring::BigInt result(0);
        ring::BigInt base(256);
        ring::BigInt multiplier(1);
        
        for (uint8_t b : bytes) {
            result = result + ring::BigInt(b) * multiplier;
            multiplier = multiplier * base;
        }
        
        return StructuralCode(result, bytes.size(), EncodingStrategy::POSITIONAL_256);
    }
    
    /**
     * @brief Base-65536 positional encoding (for wider tokens)
     */
    [[nodiscard]] StructuralCode encodePositional65536(const core::Term& term) const {
        auto bytes = termEncoder_.encode(&term);
        
        // Pad to even length
        if (bytes.size() % 2 != 0) {
            bytes.push_back(0);
        }
        
        ring::BigInt result(0);
        ring::BigInt base(65536);
        ring::BigInt multiplier(1);
        
        for (size_t i = 0; i < bytes.size(); i += 2) {
            uint16_t wide = static_cast<uint16_t>(bytes[i]) | 
                           (static_cast<uint16_t>(bytes[i+1]) << 8);
            result = result + ring::BigInt(wide) * multiplier;
            multiplier = multiplier * base;
        }
        
        return StructuralCode(result, bytes.size(), EncodingStrategy::POSITIONAL_65536);
    }
    
    /**
     * @brief Prefix-free encoding
     * 
     * Uses the bytecode directly  BigInt (since bytecode is already prefix-free)
     */
    [[nodiscard]] StructuralCode encodePrefixFree(const core::Term& term) const {
        auto bytes = termEncoder_.encode(&term);
        
        // Prefix with length to ensure uniqueness
        ByteWriter writer;
        writer.writeVarInt(bytes.size());
        for (uint8_t b : bytes) {
            writer.writeByte(b);
        }
        
        auto fullBytes = writer.bytes();
        
        ring::BigInt result(0);
        ring::BigInt base(256);
        ring::BigInt multiplier(1);
        
        for (uint8_t b : fullBytes) {
            result = result + ring::BigInt(b) * multiplier;
            multiplier = multiplier * base;
        }
        
        return StructuralCode(result, fullBytes.size(), EncodingStrategy::PREFIX_FREE);
    }
    
    /**
     * @brief -weighted encoding in []
     * 
     * Maps bytecode to a [] element using the K=12 dominance weights:
     *   code =  byte[i]  w_i   where w_i = ^{-12i}
     * 
     * Uses ZPhi::weight() which implements the linear Ki schedule,
     * consistent with the Fingerprinter's weight computation.
     */
    [[nodiscard]] StructuralCode encodeZeckendorf(const core::Term& term) const {
        auto bytes = termEncoder_.encode(&term);
        
        // Compute weighted sum using K=12 dominance weights
        ring::ZPhi result(0, 0);
        
        for (size_t i = 0; i < bytes.size(); ++i) {
            // Weight for position i: w_i = ^{-Ki} via ZPhi::weight
            ring::ZPhi weight = ring::ZPhi::weight(i, fibCache_);
            ring::ZPhi contribution = weight * ring::ZPhi(bytes[i], 0);
            result = result + contribution;
        }
        
        return StructuralCode(result, bytes.size());
    }
    
    /**
     * @brief Hybrid encoding: prefix-free bytecode  -weighted projection
     */
    [[nodiscard]] StructuralCode encodeHybrid(const core::Term& term) const {
        auto bytes = termEncoder_.encode(&term);
        
        // First get prefix-free bytecode
        ByteWriter writer;
        writer.writeVarInt(bytes.size());
        for (uint8_t b : bytes) {
            writer.writeByte(b);
        }
        auto fullBytes = writer.bytes();
        
        // Then encode using K=12 dominance weights
        ring::ZPhi result(0, 0);
        for (size_t i = 0; i < fullBytes.size(); ++i) {
            ring::ZPhi weight = ring::ZPhi::weight(i, fibCache_);
            ring::ZPhi contribution = weight * ring::ZPhi(fullBytes[i], 0);
            result = result + contribution;
        }
        
        return StructuralCode(result, fullBytes.size());
    }
    
    // -----------------------------------------------------------------------
    // DECODING (INVERSE)
    // -----------------------------------------------------------------------
    
    /**
     * @brief Decode a positional-256 code back to bytes
     * 
     * This is the inverse of encodePositional256.
     */
    [[nodiscard]] std::vector<uint8_t> decodePositional256(const ring::BigInt& code) const {
        std::vector<uint8_t> bytes;
        ring::BigInt remaining = code;
        ring::BigInt base(256);
        ring::BigInt zero(0);
        
        while (remaining > zero) {
            auto [quotient, remainder] = remaining.divmod(base);
            bytes.push_back(static_cast<uint8_t>(remainder.toInt64()));
            remaining = quotient;
        }
        
        return bytes;
    }
    
    // -----------------------------------------------------------------------
    // UTILITY
    // -----------------------------------------------------------------------
    
    [[nodiscard]] EncodingStrategy strategy() const { return strategy_; }
    
    void setStrategy(EncodingStrategy strat) { strategy_ = strat; }
    
    /**
     * @brief Verify injectivity by comparing codes
     */
    [[nodiscard]] static bool verifyDistinct(
        const StructuralCode& c1, 
        const StructuralCode& c2
    ) {
        return c1 != c2;
    }
};

// ===========================================================================
// EQUATION ENCODER
// ===========================================================================

/**
 * @brief Encoder for equations (pairs of terms)
 */
class StructuralEquationEncoder {
private:
    StructuralEncoder termEncoder_;
    
public:
    explicit StructuralEquationEncoder(EncodingStrategy strat = EncodingStrategy::POSITIONAL_256)
        : termEncoder_(strat) {}
    
    /**
     * @brief Encode an equation (lhs = rhs) to a structural code
     * 
     * Uses Cantor pairing to combine two codes into one.
     */
    [[nodiscard]] StructuralCode encode(
        const core::Term& lhs, 
        const core::Term& rhs
    ) const {
        auto lhsCode = termEncoder_.encode(lhs);
        auto rhsCode = termEncoder_.encode(rhs);
        
        // Cantor pairing: (k1, k2) = (k1 + k2)(k1 + k2 + 1)/2 + k2
        // For BigInt codes
        if (lhsCode.isInt() && rhsCode.isInt()) {
            const auto& k1 = lhsCode.asInt();
            const auto& k2 = rhsCode.asInt();
            
            ring::BigInt sum = k1 + k2;
            ring::BigInt sumPlus1 = sum + ring::BigInt(1);
            ring::BigInt product = sum * sumPlus1;
            ring::BigInt half = product / ring::BigInt(2);
            ring::BigInt result = half + k2;
            
            return StructuralCode(result, 
                lhsCode.termSize() + rhsCode.termSize(),
                termEncoder_.strategy());
        }
        
        // For ZPhi codes, use Cantor pairing on zigzag-encoded components.
        // CRITICAL: The old approach (a1 + a2*2, b1 + b2*2) was NOT injective.
        // Example: (a1=3,a2=0) and (a1=1,a2=1) both map to 3.
        // Fix: zigzag-encode all four components to non-negative BigInts,
        // then use nested Cantor pairing to produce a single injective BigInt.
        if (lhsCode.isPhi() && rhsCode.isPhi()) {
            const auto& z1 = lhsCode.asPhi();
            const auto& z2 = rhsCode.asPhi();
            
            // Zigzag encoding: map signed integers to non-negative
            auto zigzag = [](const ring::BigInt& x) -> ring::BigInt {
                if (x >= ring::BigInt(0)) return x * ring::BigInt(2);
                ring::BigInt abs_x = x * ring::BigInt(-1);
                return abs_x * ring::BigInt(2) + ring::BigInt(1);
            };
            
            // Cantor pairing: (k1, k2) = (k1 + k2)(k1 + k2 + 1)/2 + k2
            auto cantorPair = [](const ring::BigInt& k1, const ring::BigInt& k2) -> ring::BigInt {
                ring::BigInt sum = k1 + k2;
                ring::BigInt sumPlus1 = sum + ring::BigInt(1);
                return sum * sumPlus1 / ring::BigInt(2) + k2;
            };
            
            // Pair the two ZPhi values: Cantor(Cantor(za1, zb1), Cantor(za2, zb2))
            ring::BigInt pairLhs = cantorPair(zigzag(z1.a()), zigzag(z1.b()));
            ring::BigInt pairRhs = cantorPair(zigzag(z2.a()), zigzag(z2.b()));
            ring::BigInt result = cantorPair(pairLhs, pairRhs);
            
            return StructuralCode(result, 
                lhsCode.termSize() + rhsCode.termSize(),
                termEncoder_.strategy());
        }
        
        // Mixed case: convert both sides to positional BigInt for Cantor pairing.
        // If a code is ZPhi, flatten to a single BigInt via Cantor pairing of
        // its (a, b) components first, then use the standard Cantor pair.
        auto toSingleInt = [](const StructuralCode& c) -> ring::BigInt {
            if (c.isInt()) return c.asInt();
            // ZPhi (a + b)  Cantor(|a|, |b|) with sign prefix
            // Encode sign info: map x to 2x if x>=0, -2x-1 if x<0
            auto zigzag = [](const ring::BigInt& x) -> ring::BigInt {
                if (x >= ring::BigInt(0)) return x * ring::BigInt(2);
                // -(|x|*2+1)  but we need non-negative, so compute |x|*2+1
                ring::BigInt abs_x = x * ring::BigInt(-1);
                return abs_x * ring::BigInt(2) + ring::BigInt(1);
            };
            ring::BigInt za = zigzag(c.asPhi().a());
            ring::BigInt zb = zigzag(c.asPhi().b());
            // Cantor pairing
            ring::BigInt s = za + zb;
            ring::BigInt sp1 = s + ring::BigInt(1);
            return s * sp1 / ring::BigInt(2) + zb;
        };
        
        ring::BigInt k1 = toSingleInt(lhsCode);
        ring::BigInt k2 = toSingleInt(rhsCode);
        
        // Cantor pairing: (k1, k2) = (k1 + k2)(k1 + k2 + 1)/2 + k2
        ring::BigInt sum = k1 + k2;
        ring::BigInt sumPlus1 = sum + ring::BigInt(1);
        ring::BigInt result = sum * sumPlus1 / ring::BigInt(2) + k2;
        
        return StructuralCode(result);
    }
    
    [[nodiscard]] StructuralCode encode(
        const std::shared_ptr<core::Term>& lhs,
        const std::shared_ptr<core::Term>& rhs
    ) const {
        return encode(*lhs, *rhs);
    }
};

// ===========================================================================
// GLOBAL ENCODER
// ===========================================================================

/**
 * @brief Global structural encoder instance
 */
inline StructuralEncoder& globalStructuralEncoder() {
    static StructuralEncoder encoder(EncodingStrategy::POSITIONAL_256);
    return encoder;
}

/**
 * @brief Quick structural code computation
 */
inline StructuralCode structCode(const core::Term& t) {
    return globalStructuralEncoder().encode(t);
}

inline StructuralCode structCode(const std::shared_ptr<core::Term>& t) {
    return globalStructuralEncoder().encode(t);
}

} // namespace encoding
} // namespace autodiscover

#endif // AUTODISCOVER_ENCODING_STRUCTURAL_HPP
