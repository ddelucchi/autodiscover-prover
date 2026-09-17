/**
 * @file Encoding.hpp
 * @brief Prefix-free encoding and canonical bytecode generation
 * 
 * =============================================================================
 * PREFIX-FREE ENCODING
 * =============================================================================
 * 
 * A prefix-free (self-delimiting) code has the property that no codeword is
 * a prefix of any other codeword. This allows unique decoding of concatenations.
 * 
 * Elias Gamma Encoding for n >= 1:
 *   1. Write floor(log2(n)) zeros
 *   2. Write n in binary (including leading 1)
 * 
 * Example: (13) = 0001101 (3 zeros, then 1101 = 13 in binary)
 * 
 * For tokens in [0, k-1], encode token x as (x+1):
 *   code(x) = (x + 1)
 * 
 * Prefix-free property guarantees:
 *   code(x)...code(x) = code(y)...code(y)  m=n  i: x=y
 * 
 * =============================================================================
 * CANONICAL BYTECODE
 * =============================================================================
 * 
 * The canonical bytecode is a deterministic serialization of ASTs:
 *   encode(AST)  byte stream B  {0,1,...,255}*
 * 
 * Rules:
 *   - Deterministic preorder traversal
 *   - Explicit arity tags
 *   - No pointers: DAG uses canonical node IDs
 * 
 * Term bytecode schema:
 *   - TAG_TERM (0x10) | kind | sort | symbol_len | symbol | child_count | children
 * 
 * Equation bytecode:
 *   - TAG_EQ (0x20) | encoded_LHS | encoded_RHS
 * 
 * Proof bytecode:
 *   - TAG_PROOF (0x30) | node_count | nodes_in_topo_order | goal_pair
 */

#ifndef AUTODISCOVER_ENCODING_ENCODING_HPP
#define AUTODISCOVER_ENCODING_ENCODING_HPP

#include "../core/Term.hpp"
#include "../logic/Equation.hpp"
#include <vector>
#include <cstdint>
#include <cstring>
#include <string>
#include <memory>
#include <array>
#include <unordered_map>
#include <sstream>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace autodiscover {
namespace encoding {

// Cross-platform count leading zeros for 64-bit
inline size_t countLeadingZeros64(uint64_t n) {
    if (n == 0) return 64;
#ifdef _MSC_VER
    unsigned long index;
    _BitScanReverse64(&index, n);
    return 63 - index;
#else
    return static_cast<size_t>(__builtin_clzll(n));
#endif
}

using core::Term;
using core::TermKind;
using core::Sort;

// =============================================================================
// BIT WRITER / READER
// =============================================================================

/**
 * @brief Bit-level writer for prefix-free encoding
 */
class BitWriter {
public:
    BitWriter() = default;
    
    /**
     * @brief Push a single bit
     */
    void push(bool bit) {
        if (bufferBits_ == 0) {
            bytes_.push_back(0);
        }
        if (bit) {
            bytes_.back() |= (1 << (7 - bufferBits_));
        }
        bufferBits_++;
        if (bufferBits_ == 8) {
            bufferBits_ = 0;
        }
    }
    
    /**
     * @brief Push multiple bits from an integer (MSB first)
     */
    void pushBits(uint64_t value, size_t numBits) {
        for (size_t i = numBits; i-- > 0;) {
            push((value >> i) & 1);
        }
    }
    
    /**
     * @brief Flush and get bytes (pads to byte boundary with zeros)
     */
    [[nodiscard]] std::vector<uint8_t> finish() {
        // Already byte-aligned or add padding
        return bytes_;
    }
    
    /**
     * @brief Get current bit count
     */
    [[nodiscard]] size_t bitCount() const {
        return bytes_.size() * 8 - (bufferBits_ == 0 ? 0 : 8 - bufferBits_);
    }
    
    /**
     * @brief Get bits as a string of '0' and '1'
     */
    [[nodiscard]] std::string toBitString() const {
        std::string result;
        size_t totalBits = bitCount();
        for (size_t i = 0; i < totalBits; ++i) {
            size_t byteIdx = i / 8;
            size_t bitIdx = 7 - (i % 8);
            result += (bytes_[byteIdx] & (1 << bitIdx)) ? '1' : '0';
        }
        return result;
    }
    
    /**
     * @brief Clear the writer
     */
    void clear() {
        bytes_.clear();
        bufferBits_ = 0;
    }

private:
    std::vector<uint8_t> bytes_;
    size_t bufferBits_ = 0; // Bits used in current partial byte
};

/**
 * @brief Bit-level reader for decoding
 */
class BitReader {
public:
    explicit BitReader(const std::vector<uint8_t>& bytes) : bytes_(bytes) {}
    
    /**
     * @brief Read a single bit
     */
    [[nodiscard]] bool read() {
        if (byteIdx_ >= bytes_.size()) return false;
        bool bit = (bytes_[byteIdx_] >> (7 - bitIdx_)) & 1;
        bitIdx_++;
        if (bitIdx_ == 8) {
            bitIdx_ = 0;
            byteIdx_++;
        }
        return bit;
    }
    
    /**
     * @brief Read multiple bits as an integer (MSB first)
     */
    [[nodiscard]] uint64_t readBits(size_t numBits) {
        uint64_t result = 0;
        for (size_t i = 0; i < numBits; ++i) {
            result = (result << 1) | (read() ? 1 : 0);
        }
        return result;
    }
    
    /**
     * @brief Check if there are more bits
     */
    [[nodiscard]] bool hasMore() const {
        return byteIdx_ < bytes_.size();
    }
    
    /**
     * @brief Get current bit position
     */
    [[nodiscard]] size_t position() const {
        return byteIdx_ * 8 + bitIdx_;
    }

private:
    const std::vector<uint8_t>& bytes_;
    size_t byteIdx_ = 0;
    size_t bitIdx_ = 0;
};

// =============================================================================
// ELIAS GAMMA ENCODING
// =============================================================================

/**
 * @brief Elias Gamma prefix-free encoding
 * 
 * For n >= 1:
 *   (n) = (floor(log2(n)) zeros) || (n in binary)
 */
class EliasGamma {
public:
    /**
     * @brief Encode n >= 1 using Elias Gamma
     */
    static void encode(BitWriter& w, uint64_t n) {
        if (n == 0) {
            throw std::invalid_argument("EliasGamma requires n >= 1");
        }
        
        // Find floor(log2(n))
        size_t numBits = 64 - countLeadingZeros64(n); // Position of highest set bit
        size_t numZeros = numBits - 1;
        
        // Write numZeros zeros
        for (size_t i = 0; i < numZeros; ++i) {
            w.push(false);
        }
        
        // Write n in binary (numBits bits, including leading 1)
        w.pushBits(n, numBits);
    }
    
    /**
     * @brief Encode token x in [0, k-1] as (x+1)
     */
    static void encodeToken(BitWriter& w, uint32_t token) {
        encode(w, static_cast<uint64_t>(token) + 1);
    }
    
    /**
     * @brief Decode Elias Gamma
     */
    static uint64_t decode(BitReader& r) {
        // Count leading zeros
        size_t numZeros = 0;
        while (!r.read()) {
            numZeros++;
            if (numZeros > 63) {
                throw std::runtime_error("EliasGamma decode overflow");
            }
        }
        
        // We just read the leading 1; read remaining bits
        uint64_t result = 1;
        for (size_t i = 0; i < numZeros; ++i) {
            result = (result << 1) | (r.read() ? 1 : 0);
        }
        
        return result;
    }
    
    /**
     * @brief Decode token (returns x from (x+1))
     */
    static uint32_t decodeToken(BitReader& r) {
        return static_cast<uint32_t>(decode(r) - 1);
    }
};

/**
 * @brief Elias Delta encoding (for larger numbers)
 */
class EliasDelta {
public:
    static void encode(BitWriter& w, uint64_t n) {
        if (n == 0) {
            throw std::invalid_argument("EliasDelta requires n >= 1");
        }
        
        size_t numBits = 64 - countLeadingZeros64(n);
        
        // Encode (numBits) using Elias Gamma
        EliasGamma::encode(w, numBits);
        
        // Write n without the leading 1 (numBits - 1 bits)
        if (numBits > 1) {
            w.pushBits(n, numBits - 1);
        }
    }
    
    static uint64_t decode(BitReader& r) {
        size_t numBits = static_cast<size_t>(EliasGamma::decode(r));
        
        uint64_t result = 1;
        for (size_t i = 1; i < numBits; ++i) {
            result = (result << 1) | (r.read() ? 1 : 0);
        }
        
        return result;
    }
};

// =============================================================================
// BYTECODE TAGS
// =============================================================================

namespace BytecodeTags {
    // Object type markers
    constexpr uint8_t TERM      = 0x10;
    constexpr uint8_t EQUATION  = 0x20;
    constexpr uint8_t PROOF     = 0x30;
    constexpr uint8_t SUBST     = 0x40;
    
    // Term kind markers
    constexpr uint8_t TERM_VAR      = 0x01;
    constexpr uint8_t TERM_CONST    = 0x02;
    constexpr uint8_t TERM_APP      = 0x03;
    constexpr uint8_t TERM_PAIR     = 0x04;
    constexpr uint8_t TERM_SCALAR   = 0x05;
    constexpr uint8_t TERM_PHI      = 0x06;
    constexpr uint8_t TERM_J        = 0x07;
    constexpr uint8_t TERM_PHIBAR   = 0x08;
    
    // Proof step markers
    constexpr uint8_t PROOF_AXIOM       = 0x01;
    constexpr uint8_t PROOF_REWRITE     = 0x02;
    constexpr uint8_t PROOF_CONG        = 0x03;
    constexpr uint8_t PROOF_TRANS       = 0x04;
    constexpr uint8_t PROOF_SYM         = 0x05;
    constexpr uint8_t PROOF_REFL        = 0x06;
    constexpr uint8_t PROOF_GOD         = 0x07;
    
    // End markers
    constexpr uint8_t EOS = 0xFF;
}

// =============================================================================
// BYTE STREAM WRITER
// =============================================================================

/**
 * @brief Byte-level writer for canonical bytecode
 */
class ByteWriter {
public:
    /**
     * @brief Write a single byte
     */
    void writeByte(uint8_t b) {
        bytes_.push_back(b);
    }
    
    /**
     * @brief Write variable-length integer (LEB128-style)
     */
    void writeVarInt(uint64_t n) {
        do {
            uint8_t byte = n & 0x7F;
            n >>= 7;
            if (n != 0) byte |= 0x80;
            bytes_.push_back(byte);
        } while (n != 0);
    }
    
    /**
     * @brief Write fixed-width 64-bit integer
     */
    void writeU64(uint64_t n) {
        for (int i = 0; i < 8; ++i) {
            bytes_.push_back(static_cast<uint8_t>(n & 0xFF));
            n >>= 8;
        }
    }
    
    /**
     * @brief Write double (IEEE 754)
     */
    void writeDouble(double val) {
        // C++17-safe type-punning via memcpy (union-based pun is UB in C++)
        uint64_t bits;
        static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64-bit");
        std::memcpy(&bits, &val, sizeof(bits));
        writeU64(bits);
    }
    
    /**
     * @brief Write string (length-prefixed)
     */
    void writeString(const std::string& s) {
        writeVarInt(s.size());
        for (char c : s) {
            bytes_.push_back(static_cast<uint8_t>(c));
        }
    }
    
    /**
     * @brief Get the byte stream
     */
    [[nodiscard]] const std::vector<uint8_t>& bytes() const { return bytes_; }
    
    /**
     * @brief Move out the byte stream
     */
    [[nodiscard]] std::vector<uint8_t> finish() { return std::move(bytes_); }
    
    /**
     * @brief Clear the writer
     */
    void clear() { bytes_.clear(); }
    
    /**
     * @brief Get current size
     */
    [[nodiscard]] size_t size() const { return bytes_.size(); }

private:
    std::vector<uint8_t> bytes_;
};

/**
 * @brief Byte-level reader for decoding
 */
class ByteReader {
public:
    explicit ByteReader(const std::vector<uint8_t>& bytes) : bytes_(bytes) {}
    
    [[nodiscard]] uint8_t readByte() {
        if (pos_ >= bytes_.size()) throw std::runtime_error("ByteReader underflow");
        return bytes_[pos_++];
    }
    
    [[nodiscard]] uint64_t readVarInt() {
        uint64_t result = 0;
        int shift = 0;
        uint8_t byte;
        do {
            byte = readByte();
            result |= static_cast<uint64_t>(byte & 0x7F) << shift;
            shift += 7;
        } while (byte & 0x80);
        return result;
    }
    
    [[nodiscard]] uint64_t readU64() {
        uint64_t result = 0;
        for (int i = 0; i < 8; ++i) {
            result |= static_cast<uint64_t>(readByte()) << (i * 8);
        }
        return result;
    }
    
    [[nodiscard]] double readDouble() {
        // C++17-safe type-punning via memcpy (union-based pun is UB in C++)
        uint64_t bits = readU64();
        double result;
        static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64-bit");
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }
    
    [[nodiscard]] std::string readString() {
        size_t len = static_cast<size_t>(readVarInt());
        std::string result;
        result.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            result.push_back(static_cast<char>(readByte()));
        }
        return result;
    }
    
    [[nodiscard]] bool hasMore() const { return pos_ < bytes_.size(); }
    [[nodiscard]] size_t position() const { return pos_; }
    [[nodiscard]] size_t remaining() const { return bytes_.size() - pos_; }

private:
    const std::vector<uint8_t>& bytes_;
    size_t pos_ = 0;
};

// =============================================================================
// CANONICAL TERM ENCODER
// =============================================================================

/**
 * @brief Encodes terms to canonical bytecode
 * 
 * Produces deterministic, prefix-free bytecode for terms.
 * Hash-consed terms encode to identical bytecode.
 */
class TermEncoder {
public:
    /**
     * @brief Encode a term to canonical bytecode
     * 
     * Stateless  can be called as TermEncoder::encode(t) or on an instance.
     */
    [[nodiscard]] static std::vector<uint8_t> encode(const Term* term) {
        ByteWriter w;
        encodeTerm(w, term);
        return w.finish();
    }
    
    /**
     * @brief Encode term to bit stream (prefix-free)
     */
    [[nodiscard]] static std::vector<uint8_t> encodePrefixFree(const Term* term) {
        BitWriter w;
        encodeTermPrefixFree(w, term);
        return w.finish();
    }
    
    /**
     * @brief Get the bit string representation
     */
    [[nodiscard]] static std::string toBitString(const Term* term) {
        BitWriter w;
        encodeTermPrefixFree(w, term);
        return w.toBitString();
    }

private:
    static void encodeTerm(ByteWriter& w, const Term* term) {
        if (!term) {
            w.writeByte(0);
            return;
        }
        
        // Write term kind
        w.writeByte(static_cast<uint8_t>(term->kind()));
        
        // Write sort
        w.writeByte(static_cast<uint8_t>(term->sort()));
        
        // Write symbol
        w.writeString(term->symbol());
        
        // Kind-specific data
        switch (term->kind()) {
            case TermKind::Scalar:
                w.writeDouble(term->scalarValue());
                break;
            default:
                break;
        }
        
        // Write children count and children
        const auto& children = term->children();
        w.writeVarInt(children.size());
        for (const Term* child : children) {
            encodeTerm(w, child);
        }
    }
    
    static void encodeTermPrefixFree(BitWriter& w, const Term* term) {
        if (!term) {
            EliasGamma::encodeToken(w, 0); // Null marker
            return;
        }
        
        // Encode kind (1-7)
        EliasGamma::encodeToken(w, static_cast<uint32_t>(term->kind()) + 1);
        
        // Encode sort
        EliasGamma::encodeToken(w, static_cast<uint32_t>(term->sort()));
        
        // Encode symbol length and characters
        const std::string& sym = term->symbol();
        EliasGamma::encodeToken(w, static_cast<uint32_t>(sym.size()));
        for (char c : sym) {
            EliasGamma::encodeToken(w, static_cast<uint32_t>(static_cast<uint8_t>(c)));
        }
        
        // Encode children
        const auto& children = term->children();
        EliasGamma::encodeToken(w, static_cast<uint32_t>(children.size()));
        for (const Term* child : children) {
            encodeTermPrefixFree(w, child);
        }
    }
};

// =============================================================================
// CANONICAL EQUATION ENCODER
// =============================================================================

/**
 * @brief Encodes equations with symmetric normalization
 * 
 * canon_eq(t = u) = min(canon_term(t) = canon_term(u), canon_term(u) = canon_term(t))
 * where min uses lexicographic order on bytecode.
 */
class EquationEncoder {
public:
    explicit EquationEncoder(const TermEncoder& termEnc) : termEncoder_(termEnc) {}
    
    /**
     * @brief Encode equation (lhs = rhs) from Term pointers
     * 
     * Normalizes symmetric equations to same bytecode.
     */
    [[nodiscard]] std::vector<uint8_t> encode(const Term* lhs, const Term* rhs) const {
        auto lhsBytes = TermEncoder::encode(lhs);
        auto rhsBytes = TermEncoder::encode(rhs);
        return encode(lhsBytes, rhsBytes);
    }
    
    /**
     * @brief Encode equation from pre-computed bytecodes (static)
     * 
     * Canonical ordering: smaller bytecode first (lexicographic).
     * Can be called without an instance: EquationEncoder::encode(lhs, rhs).
     */
    [[nodiscard]] static std::vector<uint8_t> encode(
        const std::vector<uint8_t>& lhsBytes,
        const std::vector<uint8_t>& rhsBytes
    ) {
        ByteWriter w;
        w.writeByte(BytecodeTags::EQUATION);
        
        // Canonical ordering: smaller term first (lexicographic on bytecode)
        if (lhsBytes <= rhsBytes) {
            for (uint8_t b : lhsBytes) w.writeByte(b);
            for (uint8_t b : rhsBytes) w.writeByte(b);
        } else {
            for (uint8_t b : rhsBytes) w.writeByte(b);
            for (uint8_t b : lhsBytes) w.writeByte(b);
        }
        
        return w.finish();
    }
    
    /**
     * @brief Encode equation with prefix-free bit encoding
     */
    [[nodiscard]] std::string encodePrefixFreeBits(const Term* lhs, const Term* rhs) const {
        std::string lhsBits = TermEncoder::toBitString(lhs);
        std::string rhsBits = TermEncoder::toBitString(rhs);
        
        // Canonical ordering
        if (lhsBits <= rhsBits) {
            return lhsBits + rhsBits;
        } else {
            return rhsBits + lhsBits;
        }
    }

private:
    const TermEncoder& termEncoder_;  // Retained for API compatibility
};

// =============================================================================
// TOKEN STREAM FOR FINGERPRINTING
// =============================================================================

/**
 * @brief Converts bytecode to token stream for fingerprinting
 * 
 * Maps bytes to tokens in [0, K-1] with explicit EOS marker.
 */
class TokenStream {
public:
    static constexpr uint32_t ALPHABET_SIZE = 257; // 0-255 bytes + EOS
    static constexpr uint32_t EOS_TOKEN = 256;     // End-of-stream marker
    
    /**
     * @brief Create token stream from bytes
     */
    [[nodiscard]] static std::vector<uint32_t> fromBytes(const std::vector<uint8_t>& bytes) {
        std::vector<uint32_t> tokens;
        tokens.reserve(bytes.size() + 1);
        
        for (uint8_t b : bytes) {
            tokens.push_back(static_cast<uint32_t>(b));
        }
        tokens.push_back(EOS_TOKEN);
        
        return tokens;
    }
    
    /**
     * @brief Convert token stream back to bytes (removes EOS)
     */
    [[nodiscard]] static std::vector<uint8_t> toBytes(const std::vector<uint32_t>& tokens) {
        std::vector<uint8_t> bytes;
        bytes.reserve(tokens.size());
        
        for (uint32_t t : tokens) {
            if (t == EOS_TOKEN) break;
            if (t > 255) throw std::runtime_error("Invalid token");
            bytes.push_back(static_cast<uint8_t>(t));
        }
        
        return bytes;
    }
    
    /**
     * @brief Encode token stream as prefix-free bits
     */
    [[nodiscard]] static std::string encodeAsBits(const std::vector<uint32_t>& tokens) {
        BitWriter w;
        for (uint32_t t : tokens) {
            EliasGamma::encodeToken(w, t);
        }
        return w.toBitString();
    }
};

} // namespace encoding
} // namespace autodiscover

#endif // AUTODISCOVER_ENCODING_ENCODING_HPP
