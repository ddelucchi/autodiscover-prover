/**
 * @file ZPhi.hpp
 * @brief Exact arithmetic in Z[] - the golden ratio ring
 * 
 * =============================================================================
 * MATHEMATICAL FOUNDATION
 * =============================================================================
 * 
 * The ring Z[] = {a + b : a, b  Z} where  = (1 + 5)/2 is the golden ratio.
 * 
 * Key identity:  =  + 1
 * 
 * Arithmetic:
 *   (a + b) + (c + d) = (a+c) + (b+d)
 *   (a + b)(c + d) = (ac + bd) + (ad + bc + bd)
 *       because: b  d = bd = bd(+1) = bd + bd
 * 
 * Sign determination (exact, no floats):
 *   sign(a + b) via: x = (2a + b) + b5, so compare squares when needed
 * 
 * Fibonacci connection:
 *   ^n = F_n + F_{n-1}  (where F_n is the n-th Fibonacci number)
 *   ^{-n} = (-1)^n(F_{n+1} - F_n)
 * 
 * =============================================================================
 * IMPLEMENTATION
 * =============================================================================
 * 
 * Uses arbitrary-precision integers for exact arithmetic.
 * Provides fast-doubling Fibonacci for weight computation.
 * Supports injective fingerprinting via superincreasing schedules.
 */

#ifndef AUTODISCOVER_RING_ZPHI_HPP
#define AUTODISCOVER_RING_ZPHI_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <utility>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <mutex>
#include <cmath>

// Cross-platform 128-bit integer support
#ifdef _MSC_VER
    // MSVC doesn't have __uint128_t, use custom struct
    #include <intrin.h>
    struct uint128_t {
        uint64_t lo, hi;
        uint128_t() : lo(0), hi(0) {}
        uint128_t(uint64_t l) : lo(l), hi(0) {}
        uint128_t(uint64_t h, uint64_t l) : lo(l), hi(h) {}
        explicit operator uint64_t() const { return lo; }
        uint128_t operator+(const uint128_t& r) const {
            uint128_t result;
            result.lo = lo + r.lo;
            result.hi = hi + r.hi + (result.lo < lo ? 1 : 0);
            return result;
        }
        uint128_t operator>>(int n) const {
            if (n == 64) return uint128_t(0, hi);
            if (n == 0) return *this;
            return uint128_t(hi >> n, (lo >> n) | (hi << (64 - n)));
        }
        uint128_t operator<<(int n) const {
            if (n == 64) return uint128_t(lo, 0);
            if (n == 0) return *this;
            return uint128_t((hi << n) | (lo >> (64 - n)), lo << n);
        }
        bool operator>=(const uint128_t& r) const {
            return hi > r.hi || (hi == r.hi && lo >= r.lo);
        }
    };
    // Multiply two 64-bit integers to get 128-bit result
    inline uint128_t mul64x64(uint64_t a, uint64_t b) {
        uint64_t high;
        uint64_t low = _umul128(a, b, &high);
        return uint128_t(high, low);
    }
    // Add with carry detection
    inline uint128_t add128(uint64_t a, uint64_t b, uint64_t carry) {
        uint128_t result;
        result.lo = a + b;
        result.hi = (result.lo < a) ? 1 : 0;
        result.lo += carry;
        result.hi += (result.lo < carry) ? 1 : 0;
        return result;
    }
    #define UINT128_T uint128_t
    #define MUL64x64(a, b) mul64x64(a, b)
    #define ADD128(a, b, c) add128(a, b, c)
#else
    // GCC/Clang have native __uint128_t
    typedef __uint128_t uint128_t;
    #define UINT128_T __uint128_t
    #define MUL64x64(a, b) (static_cast<__uint128_t>(a) * b)
    #define ADD128(a, b, c) (static_cast<__uint128_t>(a) + b + c)
#endif

namespace autodiscover {
namespace ring {

// =============================================================================
// BIGINT - Arbitrary precision integer (minimal implementation)
// =============================================================================

/**
 * @brief Arbitrary precision signed integer
 * 
 * Limb-based representation for exact Fibonacci arithmetic.
 * Sufficient for fingerprinting with Zeckendorf indices up to ~10000.
 */
class BigInt {
public:
    using Limb = uint64_t;
    using SignedLimb = int64_t;
    static constexpr int LIMB_BITS = 64;
    
    // Constructors
    BigInt() : negative_(false) { limbs_.push_back(0); }
    BigInt(int val) : negative_(val < 0) {
        if (val == 0) {
            limbs_.push_back(0);
        } else {
            limbs_.push_back(static_cast<Limb>(val < 0 ? -val : val));
        }
    }  // Disambiguate int literals
    BigInt(int64_t val);
    BigInt(uint64_t val);
    BigInt(const std::string& decimal);
    BigInt(const BigInt&) = default;
    BigInt(BigInt&&) = default;
    BigInt& operator=(const BigInt&) = default;
    BigInt& operator=(BigInt&&) = default;
    
    // Comparison
    [[nodiscard]] int compare(const BigInt& other) const;
    [[nodiscard]] bool operator==(const BigInt& other) const { return compare(other) == 0; }
    [[nodiscard]] bool operator!=(const BigInt& other) const { return compare(other) != 0; }
    [[nodiscard]] bool operator<(const BigInt& other) const { return compare(other) < 0; }
    [[nodiscard]] bool operator<=(const BigInt& other) const { return compare(other) <= 0; }
    [[nodiscard]] bool operator>(const BigInt& other) const { return compare(other) > 0; }
    [[nodiscard]] bool operator>=(const BigInt& other) const { return compare(other) >= 0; }
    
    // Arithmetic
    [[nodiscard]] BigInt operator+(const BigInt& other) const;
    [[nodiscard]] BigInt operator-(const BigInt& other) const;
    [[nodiscard]] BigInt operator*(const BigInt& other) const;
    [[nodiscard]] BigInt operator/(const BigInt& other) const;
    [[nodiscard]] BigInt operator%(const BigInt& other) const;
    [[nodiscard]] BigInt operator-() const;
    BigInt& operator+=(const BigInt& other);
    BigInt& operator-=(const BigInt& other);
    BigInt& operator*=(const BigInt& other);
    
    // Division with remainder
    [[nodiscard]] std::pair<BigInt, BigInt> divmod(const BigInt& divisor) const;
    
    // Conversion to primitive (for small values)
    [[nodiscard]] int64_t toInt64() const;
    
    // Queries
    [[nodiscard]] bool isZero() const { return limbs_.size() == 1 && limbs_[0] == 0; }
    [[nodiscard]] bool isNegative() const { return negative_ && !isZero(); }
    [[nodiscard]] bool isPositive() const { return !negative_ && !isZero(); }
    [[nodiscard]] int sign() const { return isZero() ? 0 : (negative_ ? -1 : 1); }
    [[nodiscard]] BigInt abs() const;
    
    // String conversion
    [[nodiscard]] std::string toString() const;
    
    // Bit operations (for fingerprinting)
    [[nodiscard]] size_t bitLength() const;
    [[nodiscard]] bool getBit(size_t idx) const;
    
    /// Limb-based hash  O(limbs) instead of O(digits) via toString().
    /// Uses FNV-1a on the raw little-endian limb array + sign.
    [[nodiscard]] size_t hash64() const noexcept {
        // FNV-1a 64-bit
        size_t h = 14695981039346656037ULL;
        auto mix = [&](uint8_t byte) {
            h ^= byte;
            h *= 1099511628211ULL;
        };
        mix(negative_ ? 1 : 0);
        for (const auto& limb : limbs_) {
            for (int i = 0; i < 8; ++i) {
                mix(static_cast<uint8_t>((limb >> (i * 8)) & 0xFF));
            }
        }
        return h;
    }
    
    /// Approximate double value directly from limbs  no toString().
    [[nodiscard]] double toDoubleFast() const noexcept {
        if (isZero()) return 0.0;
        double val = 0.0;
        double base = 1.0;
        // Process limbs in little-endian order
        for (const auto& limb : limbs_) {
            val += static_cast<double>(limb) * base;
            base *= 18446744073709551616.0; // 2^64
        }
        return negative_ ? -val : val;
    }
    
private:
    std::vector<Limb> limbs_; // Little-endian limbs
    bool negative_;
    
    void normalize();
    [[nodiscard]] int compareMagnitude(const BigInt& other) const;
    void addMagnitude(const BigInt& other);
    void subMagnitude(const BigInt& other); // Assumes |this| >= |other|
    [[nodiscard]] BigInt mulMagnitude(const BigInt& other) const;
};

// =============================================================================
// FIBONACCI FAST DOUBLING
// =============================================================================

/**
 * @brief Compute (F_n, F_{n+1}) using fast-doubling in O(log n)
 * 
 * Recurrence:
 *   F_{2k} = F_k  (2F_{k+1} - F_k)
 *   F_{2k+1} = F_k + F_{k+1}
 */
[[nodiscard]] std::pair<BigInt, BigInt> fibDoubling(uint64_t n);

/**
 * @brief Compute F_n using fast-doubling O(log n), arbitrary precision
 * 
 * @note For a simpler uint64_t version (small n only), see
 *       domain::fibonacci() in domain/Algebra.hpp.
 */
[[nodiscard]] BigInt fibonacci(uint64_t n);

/**
 * @brief Cache of Fibonacci numbers at power-of-2 indices
 * 
 * Stores (F_{2^i}, F_{2^i + 1}) for efficient weight computation.
 */
class FibPow2Cache {
public:
    FibPow2Cache() = default;
    
    /**
     * @brief Build cache up to index 2^maxExp
     * @param maxExp Maximum exponent (default 20 covers most cases)
     */
    void build(size_t maxExp = 20);
    
    /**
     * @brief Get F_{2^i}
     */
    [[nodiscard]] const BigInt& F_2i(size_t i) const;
    
    /**
     * @brief Get F_{2^i + 1}
     */
    [[nodiscard]] const BigInt& F_2i_plus1(size_t i) const;
    
    /**
     * @brief Check if index is cached
     */
    [[nodiscard]] bool hasCached(size_t i) const { return i < cache_.size(); }
    
    /**
     * @brief Get cache size
     */
    [[nodiscard]] size_t size() const { return cache_.size(); }

private:
    struct Entry {
        BigInt F_2i;
        BigInt F_2i_plus1;
    };
    std::vector<Entry> cache_;
};

// =============================================================================
// Z[] RING ELEMENT
// =============================================================================

/**
 * @brief Element of Z[] = {a + b : a, b  Z}
 * 
 * Exact arithmetic with integer coordinates.
 * No floating-point approximations.
 */
class ZPhi {
public:
    // Constructors
    ZPhi() : a_(0), b_(0) {}
    ZPhi(const BigInt& a, const BigInt& b) : a_(a), b_(b) {}
    ZPhi(int64_t a, int64_t b = 0) : a_(a), b_(b) {}
    ZPhi(const ZPhi&) = default;
    ZPhi(ZPhi&&) = default;
    ZPhi& operator=(const ZPhi&) = default;
    ZPhi& operator=(ZPhi&&) = default;
    
    // Accessors
    [[nodiscard]] const BigInt& a() const { return a_; }
    [[nodiscard]] const BigInt& b() const { return b_; }
    
    // Arithmetic
    [[nodiscard]] ZPhi operator+(const ZPhi& other) const;
    [[nodiscard]] ZPhi operator-(const ZPhi& other) const;
    [[nodiscard]] ZPhi operator*(const ZPhi& other) const;
    [[nodiscard]] ZPhi operator-() const;
    ZPhi& operator+=(const ZPhi& other);
    ZPhi& operator-=(const ZPhi& other);
    ZPhi& operator*=(const ZPhi& other);
    
    // Comparison
    [[nodiscard]] bool operator==(const ZPhi& other) const;
    [[nodiscard]] bool operator!=(const ZPhi& other) const { return !(*this == other); }
    
    /**
     * @brief Total ordering on Z[] based on real value
     * 
     * Uses exact integer arithmetic to compare a + b values.
     * sign(a + b) = sign((2a + b) + b5)
     */
    [[nodiscard]] int compare(const ZPhi& other) const;
    [[nodiscard]] bool operator<(const ZPhi& other) const { return compare(other) < 0; }
    [[nodiscard]] bool operator<=(const ZPhi& other) const { return compare(other) <= 0; }
    [[nodiscard]] bool operator>(const ZPhi& other) const { return compare(other) > 0; }
    [[nodiscard]] bool operator>=(const ZPhi& other) const { return compare(other) >= 0; }
    
    // Queries
    [[nodiscard]] bool isZero() const { return a_.isZero() && b_.isZero(); }
    
    /**
     * @brief Exact sign determination using integer comparisons
     * 
     * sign(a + b) where  = (1 + 5)/2
     * = sign((2a + b)/2 + b5/2)
     * = sign(2a + b + b5)
     * 
     * Compare p = 2a + b with -q5 where q = b
     */
    [[nodiscard]] int sign() const;
    
    /**
     * @brief Compute the conjugate: (a + b)  (a + b - b) = (a + b) - b
     * Using ' = 1 -  = -1/ (the Galois conjugate)
     */
    [[nodiscard]] ZPhi conjugate() const;
    
    /**
     * @brief Compute the norm: N(a + b) = (a + b)(a + b') = a + ab - b
     */
    [[nodiscard]] BigInt norm() const;
    
    // String representation
    [[nodiscard]] std::string toString() const;
    
    /// Limb-based hash  avoids toString() allocation entirely.
    [[nodiscard]] size_t hash64() const noexcept {
        size_t h = a_.hash64();
        h ^= b_.hash64() + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
    
    /// Approximate double via limbs: a + b
    [[nodiscard]] double toDoubleFast() const noexcept {
        constexpr double PHI = 1.6180339887498949;
        return a_.toDoubleFast() + b_.toDoubleFast() * PHI;
    }

    /**
     * @brief Create ^{-n} for Zeckendorf weights
     * ^{-n} = (-1)^n (F_{n+1} - F_n  )
     */
    [[nodiscard]] static ZPhi phiNegPower(uint64_t n, const FibPow2Cache& cache);
    
    /**
     * @brief Create the weight w_i = ^{-Ki} for fingerprinting
     * 
     * CRITICAL: For byte-level fingerprinting with digits in [1,256],
     * the weight schedule must satisfy the DOMINANCE INEQUALITY:
     * 
     *   |w_i| > M  _{j>i} |w_j|
     * 
     * where M = 256 is the maximum digit value.
     * 
     * With K = 12 (because log_(256) = 12), the weights
     * w_i = ^{-12i} decrease fast enough that the tail sum
     * is dominated by the leading term.
     * 
     * NOTE: The schedule is LINEAR in i (exponent = Ki), NOT
     * exponential. The old K2^i schedule overflowed for i  64
     * and was replaced with Ki which gives the same dominance
     * guarantee while remaining tractable for all practical stream
     * lengths.
     */
    [[nodiscard]] static ZPhi weight(size_t i, const FibPow2Cache& cache);

private:
    BigInt a_; // Coefficient of 1
    BigInt b_; // Coefficient of 
};

// =============================================================================
// ZECKENDORF REPRESENTATION
// =============================================================================

/**
 * @brief Zeckendorf representation utilities
 * 
 * Every positive integer has a unique representation as a sum of
 * non-consecutive Fibonacci numbers (Zeckendorf's theorem).
 */
class Zeckendorf {
public:
    /**
     * @brief Convert integer to Zeckendorf representation
     * @return Vector of Fibonacci indices (no consecutive indices)
     */
    [[nodiscard]] static std::vector<uint32_t> encode(uint64_t n);
    
    /**
     * @brief Convert Zeckendorf representation to integer
     */
    [[nodiscard]] static uint64_t decode(const std::vector<uint32_t>& indices);
    
    /**
     * @brief Check if representation is valid (no consecutive indices)
     */
    [[nodiscard]] static bool isValid(const std::vector<uint32_t>& indices);
    
    /**
     * @brief Normalize a representation (remove consecutive indices)
     */
    [[nodiscard]] static std::vector<uint32_t> normalize(std::vector<uint32_t> indices);
};

// =============================================================================
// IMPLEMENTATION - BigInt
// =============================================================================

inline BigInt::BigInt(int64_t val) : negative_(val < 0) {
    if (val == 0) {
        limbs_.push_back(0);
    } else {
        limbs_.push_back(static_cast<Limb>(val < 0 ? -val : val));
    }
}

inline BigInt::BigInt(uint64_t val) : negative_(false) {
    limbs_.push_back(val);
}

inline BigInt::BigInt(const std::string& decimal) : negative_(false) {
    limbs_.push_back(0);
    
    size_t start = 0;
    if (!decimal.empty() && decimal[0] == '-') {
        negative_ = true;
        start = 1;
    } else if (!decimal.empty() && decimal[0] == '+') {
        start = 1;
    }
    
    for (size_t i = start; i < decimal.size(); ++i) {
        char c = decimal[i];
        if (c >= '0' && c <= '9') {
            // Multiply by 10
            Limb carry = 0;
            for (auto& limb : limbs_) {
                UINT128_T prod = MUL64x64(limb, 10) + UINT128_T(carry);
                limb = static_cast<Limb>(prod);
                carry = static_cast<Limb>(prod >> 64);
            }
            if (carry) limbs_.push_back(carry);
            
            // Add digit
            carry = static_cast<Limb>(c - '0');
            for (auto& limb : limbs_) {
                UINT128_T sum = ADD128(limb, 0, carry);
                limb = static_cast<Limb>(sum);
                carry = static_cast<Limb>(sum >> 64);
                if (!carry) break;
            }
            if (carry) limbs_.push_back(carry);
        }
    }
    
    normalize();
}

inline void BigInt::normalize() {
    while (limbs_.size() > 1 && limbs_.back() == 0) {
        limbs_.pop_back();
    }
    if (limbs_.size() == 1 && limbs_[0] == 0) {
        negative_ = false;
    }
}

inline int BigInt::compareMagnitude(const BigInt& other) const {
    if (limbs_.size() != other.limbs_.size()) {
        return limbs_.size() < other.limbs_.size() ? -1 : 1;
    }
    for (size_t i = limbs_.size(); i-- > 0;) {
        if (limbs_[i] != other.limbs_[i]) {
            return limbs_[i] < other.limbs_[i] ? -1 : 1;
        }
    }
    return 0;
}

inline int BigInt::compare(const BigInt& other) const {
    if (isZero() && other.isZero()) return 0;
    if (negative_ != other.negative_) {
        return negative_ ? -1 : 1;
    }
    int cmp = compareMagnitude(other);
    return negative_ ? -cmp : cmp;
}

inline void BigInt::addMagnitude(const BigInt& other) {
    Limb carry = 0;
    size_t maxLen = std::max(limbs_.size(), other.limbs_.size());
    limbs_.resize(maxLen, 0);
    
    for (size_t i = 0; i < maxLen; ++i) {
        Limb a = limbs_[i];
        Limb b = (i < other.limbs_.size()) ? other.limbs_[i] : 0;
        UINT128_T sum = ADD128(a, b, carry);
        limbs_[i] = static_cast<Limb>(sum);
        carry = static_cast<Limb>(sum >> 64);
    }
    if (carry) limbs_.push_back(carry);
}

inline void BigInt::subMagnitude(const BigInt& other) {
    Limb borrow = 0;
    for (size_t i = 0; i < limbs_.size(); ++i) {
        Limb a = limbs_[i];
        Limb b = (i < other.limbs_.size()) ? other.limbs_[i] : 0;
        // Correct borrow logic that avoids b + borrow overflow.
        // We need to compute a - b - borrow and detect if a < b + borrow
        // WITHOUT computing b + borrow (which can wrap).
        //
        // Case analysis:
        //   borrow == 0: need_borrow = (a < b)
        //   borrow == 1: need_borrow = (a <= b)   [since a < b + 1  a  b]
        Limb need_borrow = borrow ? (a <= b ? 1 : 0) : (a < b ? 1 : 0);
        limbs_[i] = a - b - borrow;
        borrow = need_borrow;
    }
    normalize();
}

inline BigInt BigInt::operator+(const BigInt& other) const {
    BigInt result = *this;
    result += other;
    return result;
}

inline BigInt& BigInt::operator+=(const BigInt& other) {
    if (negative_ == other.negative_) {
        addMagnitude(other);
    } else {
        int cmp = compareMagnitude(other);
        if (cmp >= 0) {
            subMagnitude(other);
        } else {
            BigInt temp = other;
            temp.subMagnitude(*this);
            *this = std::move(temp);
            negative_ = other.negative_;
        }
    }
    normalize();
    return *this;
}

inline BigInt BigInt::operator-(const BigInt& other) const {
    BigInt result = *this;
    result -= other;
    return result;
}

inline BigInt& BigInt::operator-=(const BigInt& other) {
    BigInt neg = -other;
    return *this += neg;
}

inline BigInt BigInt::operator-() const {
    BigInt result = *this;
    if (!result.isZero()) {
        result.negative_ = !result.negative_;
    }
    return result;
}

inline BigInt BigInt::mulMagnitude(const BigInt& other) const {
    size_t m = limbs_.size();
    size_t n = other.limbs_.size();
    BigInt result;
    result.limbs_.resize(m + n, 0);
    result.negative_ = false;
    
    for (size_t i = 0; i < m; ++i) {
        Limb carry = 0;
        for (size_t j = 0; j < n; ++j) {
            UINT128_T prod = MUL64x64(limbs_[i], other.limbs_[j]);
            prod = prod + UINT128_T(result.limbs_[i + j]);
            prod = prod + UINT128_T(carry);
            result.limbs_[i + j] = static_cast<Limb>(prod);
            carry = static_cast<Limb>(prod >> 64);
        }
        result.limbs_[i + n] += carry;
    }
    
    result.normalize();
    return result;
}

inline BigInt BigInt::operator*(const BigInt& other) const {
    BigInt result = mulMagnitude(other);
    result.negative_ = (negative_ != other.negative_) && !result.isZero();
    return result;
}

inline BigInt& BigInt::operator*=(const BigInt& other) {
    *this = *this * other;
    return *this;
}

inline BigInt BigInt::abs() const {
    BigInt result = *this;
    result.negative_ = false;
    return result;
}

// Division implementation - simple binary long division
inline std::pair<BigInt, BigInt> BigInt::divmod(const BigInt& divisor) const {
    if (divisor.isZero()) {
        throw std::runtime_error("Division by zero");
    }
    
    BigInt dividend_abs = this->abs();
    BigInt divisor_abs = divisor.abs();
    
    // Handle case where dividend < divisor
    if (dividend_abs.compareMagnitude(divisor_abs) < 0) {
        return {BigInt(0), *this};
    }
    
    // Binary long division
    BigInt quotient(0);
    BigInt remainder(0);
    
    // Process bits from high to low
    size_t bits = dividend_abs.bitLength();
    for (size_t i = bits; i-- > 0;) {
        // Shift remainder left by 1 (multiply by 2)
        remainder = remainder + remainder;
        // Add next bit of dividend
        if (dividend_abs.getBit(i)) {
            remainder = remainder + BigInt(1);
        }
        // Check if remainder >= divisor
        if (remainder.compareMagnitude(divisor_abs) >= 0) {
            remainder = remainder - divisor_abs;
            // Set bit i in quotient
            if (i < 64) {
                quotient.limbs_[0] |= (Limb(1) << i);
            } else {
                size_t limbIdx = i / 64;
                size_t bitIdx = i % 64;
                while (quotient.limbs_.size() <= limbIdx) {
                    quotient.limbs_.push_back(0);
                }
                quotient.limbs_[limbIdx] |= (Limb(1) << bitIdx);
            }
        }
    }
    quotient.normalize();
    remainder.normalize();
    
    // Adjust signs
    quotient.negative_ = (negative_ != divisor.negative_) && !quotient.isZero();
    remainder.negative_ = negative_ && !remainder.isZero();
    
    return {quotient, remainder};
}

inline BigInt BigInt::operator/(const BigInt& other) const {
    return divmod(other).first;
}

inline BigInt BigInt::operator%(const BigInt& other) const {
    return divmod(other).second;
}

inline int64_t BigInt::toInt64() const {
    if (limbs_.empty() || isZero()) return 0;
    int64_t val = static_cast<int64_t>(limbs_[0]);
    return negative_ ? -val : val;
}

inline size_t BigInt::bitLength() const {
    if (isZero()) return 0;
    size_t bits = (limbs_.size() - 1) * 64;
    Limb top = limbs_.back();
    while (top) {
        bits++;
        top >>= 1;
    }
    return bits;
}

inline bool BigInt::getBit(size_t idx) const {
    size_t limbIdx = idx / 64;
    size_t bitIdx = idx % 64;
    if (limbIdx >= limbs_.size()) return false;
    return (limbs_[limbIdx] >> bitIdx) & 1;
}

inline std::string BigInt::toString() const {
    if (isZero()) return "0";
    
    // Simple conversion - divide by 10 repeatedly
    std::string digits;
    BigInt temp = abs();
    
    while (!temp.isZero()) {
        // Divide by 10
        Limb remainder = 0;
        for (size_t i = temp.limbs_.size(); i-- > 0;) {
            // Divide (remainder * 2^64 + limbs_[i]) by 10.
            // Since remainder < 10, the value is < 10 * 2^64 and the quotient
            // fits in 64 bits. Use schoolbook division in base-2^32 to avoid
            // needing uint128_t division operators (unavailable on MSVC).
            uint64_t hi32 = temp.limbs_[i] >> 32;
            uint64_t lo32 = temp.limbs_[i] & 0xFFFFFFFFULL;
            uint64_t mid = (static_cast<uint64_t>(remainder) << 32) | hi32;
            uint64_t q_hi = mid / 10;
            uint64_t r_mid = mid % 10;
            uint64_t bot = (r_mid << 32) | lo32;
            uint64_t q_lo = bot / 10;
            remainder = static_cast<Limb>(bot % 10);
            temp.limbs_[i] = static_cast<Limb>((q_hi << 32) | q_lo);
        }
        temp.normalize();
        digits.push_back(static_cast<char>('0' + remainder));
    }
    
    if (negative_) digits.push_back('-');
    std::reverse(digits.begin(), digits.end());
    return digits;
}

// =============================================================================
// IMPLEMENTATION - Fibonacci
// =============================================================================

inline std::pair<BigInt, BigInt> fibDoubling(uint64_t n) {
    if (n == 0) return {BigInt(0), BigInt(1)};
    
    auto [Fk, Fk1] = fibDoubling(n >> 1);
    
    // F_{2k} = F_k  (2F_{k+1} - F_k)
    BigInt twoFk1 = Fk1 + Fk1;
    BigInt twoFk1_minus_Fk = twoFk1 - Fk;
    BigInt F2k = Fk * twoFk1_minus_Fk;
    
    // F_{2k+1} = F_k + F_{k+1}
    BigInt F2k1 = Fk * Fk + Fk1 * Fk1;
    
    if (n & 1) {
        // n is odd: return (F_{2k+1}, F_{2k+2})
        return {F2k1, F2k + F2k1};
    } else {
        // n is even: return (F_{2k}, F_{2k+1})
        return {F2k, F2k1};
    }
}

inline BigInt fibonacci(uint64_t n) {
    return fibDoubling(n).first;
}

inline void FibPow2Cache::build(size_t maxExp) {
    cache_.clear();
    cache_.reserve(maxExp + 1);
    
    for (size_t i = 0; i <= maxExp; ++i) {
        // Compute n = 2^i as a uint64_t (safe for i < 64)
        // For i >= 64, we'd overflow uint64_t. With default maxExp=20,
        // this guard is never hit, but protect anyway.
        if (i >= 63) break;  // 2^63 would overflow, and F_{2^63} is astronomical
        
        uint64_t n = uint64_t(1) << i;
        auto [Fn, Fn1] = fibDoubling(n);
        cache_.push_back({std::move(Fn), std::move(Fn1)});
    }
}

inline const BigInt& FibPow2Cache::F_2i(size_t i) const {
    if (i >= cache_.size()) {
        throw std::out_of_range("FibPow2Cache index out of range");
    }
    return cache_[i].F_2i;
}

inline const BigInt& FibPow2Cache::F_2i_plus1(size_t i) const {
    if (i >= cache_.size()) {
        throw std::out_of_range("FibPow2Cache index out of range");
    }
    return cache_[i].F_2i_plus1;
}

// =============================================================================
// IMPLEMENTATION - ZPhi
// =============================================================================

inline ZPhi ZPhi::operator+(const ZPhi& other) const {
    return ZPhi(a_ + other.a_, b_ + other.b_);
}

inline ZPhi ZPhi::operator-(const ZPhi& other) const {
    return ZPhi(a_ - other.a_, b_ - other.b_);
}

inline ZPhi ZPhi::operator*(const ZPhi& other) const {
    // (a + b)(c + d) = ac + ad + bc + bd
    //                  = ac + ad + bc + bd( + 1)
    //                  = (ac + bd) + (ad + bc + bd)
    BigInt ac = a_ * other.a_;
    BigInt bd = b_ * other.b_;
    BigInt ad_bc = a_ * other.b_ + b_ * other.a_;
    
    return ZPhi(ac + bd, ad_bc + bd);
}

inline ZPhi ZPhi::operator-() const {
    return ZPhi(-a_, -b_);
}

inline ZPhi& ZPhi::operator+=(const ZPhi& other) {
    a_ += other.a_;
    b_ += other.b_;
    return *this;
}

inline ZPhi& ZPhi::operator-=(const ZPhi& other) {
    a_ -= other.a_;
    b_ -= other.b_;
    return *this;
}

inline ZPhi& ZPhi::operator*=(const ZPhi& other) {
    *this = *this * other;
    return *this;
}

inline bool ZPhi::operator==(const ZPhi& other) const {
    return a_ == other.a_ && b_ == other.b_;
}

inline int ZPhi::sign() const {
    if (a_.isZero() && b_.isZero()) return 0;
    
    // sign(a + b) where  = (1 + 5)/2  1.618
    // = sign((2a + b) + b5) / 2
    // = sign(2a + b + b5) since /2 doesn't change sign
    
    BigInt p = a_ + a_ + b_;  // 2a + b
    const BigInt& q = b_;      // coefficient of 5
    
    if (q.isZero()) {
        return p.sign();
    }
    
    // We need sign of p + q5
    // If q > 0:
    //   If p >= 0: positive (since 5 > 0)
    //   If p < 0: compare |p| with 5q 
    // If q < 0:
    //   If p <= 0: negative
    //   If p > 0: compare p with 5q
    
    if (q.isPositive()) {
        if (!p.isNegative()) {
            return 1; // p >= 0, q > 0  positive
        }
        // p < 0, q > 0: need q5 > |p|, i.e., 5q > p
        BigInt lhs = BigInt(5) * q * q;
        BigInt rhs = p * p;
        return lhs > rhs ? 1 : -1;
    } else { // q < 0
        if (!p.isPositive()) {
            return -1; // p <= 0, q < 0  negative
        }
        // p > 0, q < 0: need p > |q|5, i.e., p > 5q
        BigInt lhs = p * p;
        BigInt rhs = BigInt(5) * q * q;
        return lhs > rhs ? 1 : -1;
    }
}

inline int ZPhi::compare(const ZPhi& other) const {
    ZPhi diff = *this - other;
    return diff.sign();
}

inline ZPhi ZPhi::conjugate() const {
    // Galois conjugate: ' = (1 - 5)/2 = 1 - 
    // (a + b)' = a + b(1 - ) = (a + b) - b
    return ZPhi(a_ + b_, -b_);
}

inline BigInt ZPhi::norm() const {
    // N(a + b) = (a + b)(a + b') = (a + b)((a+b) - b)
    // = a(a+b) - ab + b(a+b) - b
    // = a(a+b) + b(a+b) - ab - b(+1)
    // = a + ab + ab + b - ab - b - b
    // = a + ab - b
    return a_*a_ + a_*b_ - b_*b_;
}

inline std::string ZPhi::toString() const {
    std::ostringstream oss;
    
    if (b_.isZero()) {
        oss << a_.toString();
    } else if (a_.isZero()) {
        if (b_ == BigInt(1)) {
            oss << "";
        } else if (b_ == BigInt(-1)) {
            oss << "-";
        } else {
            oss << b_.toString() << "";
        }
    } else {
        oss << "(" << a_.toString();
        if (b_.isPositive()) {
            oss << " + " << b_.toString() << "";
        } else {
            oss << " - " << (-b_).toString() << "";
        }
        oss << ")";
    }
    
    return oss.str();
}

inline ZPhi ZPhi::phiNegPower(uint64_t n, const FibPow2Cache& cache) {
    // ^{-n} = (-1)^n (F_{n+1} - F_n  )
    //
    // The cache parameter stores (F_{2^i}, F_{2^i+1}) for i = 0..maxExp.
    // For the LINEAR schedule (exponent = Ki), we use fibDoubling which
    // is O(log n) via fast doubling. The cache accelerates only when the
    // exponent IS a power of 2  for general n, fibDoubling is used.
    //
    // PERF: For repeated calls with the same n (which the weight schedule
    // does for each byte position), callers should memoize externally.
    
    BigInt Fn, Fn1;
    
    // Check if n is a power of 2 and in the cache range
    if (n > 0 && (n & (n - 1)) == 0) {
        // n is a power of 2  n = 2^i
        size_t i = 0;
        uint64_t tmp = n;
        while (tmp > 1) { tmp >>= 1; ++i; }
        if (i < cache.size()) {
            Fn  = cache.F_2i(i);
            Fn1 = cache.F_2i_plus1(i);
        } else {
            auto [f, f1] = fibDoubling(n);
            Fn = std::move(f);
            Fn1 = std::move(f1);
        }
    } else {
        auto [f, f1] = fibDoubling(n);
        Fn = std::move(f);
        Fn1 = std::move(f1);
    }
    
    ZPhi result(Fn1, -Fn);  // F_{n+1} - F_n  
    
    if (n % 2 == 1) {
        result = -result;  // (-1)^n factor
    }
    
    return result;
}

inline ZPhi ZPhi::weight(size_t i, const FibPow2Cache& cache) {
    // w_i = ^{-Ki} where K = 12 (byte-dominance constant)
    //
    // K = log_(256) = 12 ensures the DOMINANCE INEQUALITY:
    //   |w_i| > 256  _{j>i} |w_j|
    //
    // which guarantees that no combination of byte differences at later
    // positions can overwhelm a single-byte difference at position i.
    //
    // The exponent is n = K  i = 12i (LINEAR in i, not exponential).
    // The old K2^i schedule was exponential and overflowed for i  64.
    //
    // ^{-n} = (-1)^n (F_{n+1} - F_n  )
    
    static constexpr uint64_t K = 12;  // Byte-dominance constant
    uint64_t exponent = K * i;         // LINEAR schedule: Ki
    
    // Delegate to phiNegPower which handles the Fibonacci computation
    return phiNegPower(exponent, cache);
}

// =============================================================================
// IMPLEMENTATION - Zeckendorf
// =============================================================================

inline std::vector<uint32_t> Zeckendorf::encode(uint64_t n) {
    if (n == 0) return {};
    
    std::vector<uint32_t> indices;
    
    // Find largest Fibonacci <= n
    std::vector<uint64_t> fibs = {1, 2};
    while (fibs.back() <= n) {
        uint64_t next = fibs[fibs.size()-1] + fibs[fibs.size()-2];
        if (next > n) break;
        fibs.push_back(next);
    }
    
    // Greedy decomposition
    for (size_t i = fibs.size(); i-- > 0;) {
        if (fibs[i] <= n) {
            n -= fibs[i];
            indices.push_back(static_cast<uint32_t>(i + 2)); // F_{i+2} is at index i
        }
    }
    
    std::reverse(indices.begin(), indices.end());
    return indices;
}

inline uint64_t Zeckendorf::decode(const std::vector<uint32_t>& indices) {
    uint64_t sum = 0;
    
    std::vector<uint64_t> fibs = {0, 1, 1};
    for (uint32_t k : indices) {
        while (fibs.size() <= k) {
            fibs.push_back(fibs[fibs.size()-1] + fibs[fibs.size()-2]);
        }
        sum += fibs[k];
    }
    
    return sum;
}

inline bool Zeckendorf::isValid(const std::vector<uint32_t>& indices) {
    for (size_t i = 1; i < indices.size(); ++i) {
        if (indices[i] == indices[i-1] + 1) {
            return false; // Consecutive indices
        }
    }
    return true;
}

inline std::vector<uint32_t> Zeckendorf::normalize(std::vector<uint32_t> indices) {
    // Convert to digit array, normalize, convert back
    if (indices.empty()) return indices;
    
    uint32_t maxIdx = *std::max_element(indices.begin(), indices.end());
    std::vector<int> digits(maxIdx + 3, 0);
    
    for (uint32_t idx : indices) {
        digits[idx]++;
    }
    
    // Carry rewriting
    bool changed = true;
    while (changed) {
        changed = false;
        
        // Handle digits >= 2 using CORRECT Fibonacci carry law:
        //   2F_i = F_{i+1} + F_{i-2}   (for i  2)
        //   2F_1 = F_3 = 2              (special case)
        //   2F_0 = 0                    (trivial)
        for (size_t i = 0; i < digits.size(); ++i) {
            while (digits[i] >= 2) {
                digits[i] -= 2;
                changed = true;
                
                if (i >= 2) {
                    // Standard carry: 2F_i = F_{i+1} + F_{i-2}
                    if (i + 1 >= digits.size()) digits.resize(i + 2, 0);
                    digits[i + 1]++;
                    digits[i - 2]++;
                } else if (i == 1) {
                    // 2F_1 = 2 = F_3
                    if (digits.size() < 4) digits.resize(4, 0);
                    digits[3]++;
                }
                // i == 0: 2F_0 = 0, no carry needed
            }
        }
        
        // Handle consecutive 1s
        for (size_t i = 0; i + 1 < digits.size(); ++i) {
            if (digits[i] >= 1 && digits[i + 1] >= 1) {
                digits[i]--;
                digits[i + 1]--;
                if (i + 2 >= digits.size()) digits.resize(i + 3, 0);
                digits[i + 2]++;
                changed = true;
                break;
            }
        }
    }
    
    // Convert back to indices
    std::vector<uint32_t> result;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (digits[i] > 0) {
            result.push_back(static_cast<uint32_t>(i));
        }
    }
    
    return result;
}

// =============================================================================
// CONVENIENCE FREE FUNCTIONS (for use outside the ZPhi class)
// =============================================================================

/**
 * @brief Global Fibonacci cache for phiNegPower computations
 * 
 * Thread-safe: uses std::call_once for one-time initialization.
 * The old version used a non-atomic `bool initialized` which was a
 * data race on first concurrent access.
 */
inline FibPow2Cache& globalFibCache() {
    static FibPow2Cache cache;
    static std::once_flag initFlag;
    std::call_once(initFlag, [&]() {
        cache.build(20);
    });
    return cache;
}

/**
 * @brief Free function to compute ^{-n}
 * Uses the global Fibonacci cache for convenience.
 */
inline ZPhi phiNegPower(int n) {
    if (n < 0) {
        // ^n for positive exponent
        // ^n = F_n* + F_{n-1}
        uint64_t posN = static_cast<uint64_t>(-n);
        auto [Fn, Fn1] = fibDoubling(posN);
        if (posN == 0) return ZPhi(1, 0);
        auto [Fn_1, Fn_] = fibDoubling(posN - 1);
        return ZPhi(Fn_1, Fn);
    }
    return ZPhi::phiNegPower(static_cast<uint64_t>(n), globalFibCache());
}

/**
 * @brief ^{-n} using uint64_t for positive exponents only
 */
inline ZPhi phiNegPower(uint64_t n) {
    return ZPhi::phiNegPower(n, globalFibCache());
}

} // namespace ring
} // namespace autodiscover

#endif // AUTODISCOVER_RING_ZPHI_HPP
