/**
 * @file ZeckendorfBigInt.hpp
 * @brief Zeckendorf normal form for arbitrary-precision BigInt
 * 
 * MATHEMATICAL FOUNDATION:
 * =========================
 * 
 * Zeckendorf's Theorem: Every positive integer has a unique representation
 * as a sum of non-consecutive Fibonacci numbers:
 * 
 *   N = F_{k} + F_{k} +  + F_{k}   where k_{i+1}  k_i + 2
 * 
 * The carry rule  =  + 1 gives the rewrite system:
 * 
 *   ...011...  ...100...   (adjacent 1s  carry)
 *   ...0(d2)...  ...1(d-2)...  (digit overflow  carry)
 * 
 * This is the unique normal form of the TRS {x  x + 1}.
 * 
 * This module extends the uint64_t-based Fibonacci/Zeckendorf in Closure.hpp
 * to arbitrary-precision BigInt, enabling Zeckendorf representation for
 * numbers beyond 2^64.
 * 
 * CONNECTION TO Z[]:
 * ====================
 * 
 * The Zeckendorf indices of N give its Z[] "alpha-probe":
 * 
 *   (N) = _{k  Zeck(N)} ^k
 * 
 * This is an injective map N  Z[], which the fingerprinter uses.
 * 
 * @author AutoDiscoverProver Team
 */

#pragma once

#include "ZPhi.hpp"

#include <vector>
#include <utility>
#include <string>
#include <sstream>
#include <algorithm>
#include <cassert>

namespace autodiscover {
namespace ring {

/**
 * @brief BigInt Fibonacci computation
 * 
 * Uses fast-doubling in BigInt arithmetic:
 *   F_{2k} = F_k  (2F_{k+1} - F_k)
 *   F_{2k+1} = F_k + F_{k+1}
 */
class BigFibonacci {
public:
    /**
     * @brief Compute (F_n, F_{n+1}) using fast doubling with BigInt
     */
    static std::pair<BigInt, BigInt> fibPair(uint32_t n) {
        if (n == 0) return {BigInt(0), BigInt(1)};
        
        auto [fk, fk1] = fibPair(n / 2);
        
        BigInt two_fk1 = fk1 + fk1;  // 2F_{k+1}
        BigInt f2k = fk * (two_fk1 - fk);      // F_k(2F_{k+1} - F_k)
        BigInt f2k1 = fk * fk + fk1 * fk1;     // F_k + F_{k+1}
        
        if (n % 2 == 0) {
            return {f2k, f2k1};
        } else {
            return {f2k1, f2k + f2k1};
        }
    }
    
    /**
     * @brief Compute F_n as BigInt
     */
    static BigInt fib(uint32_t n) {
        return fibPair(n).first;
    }
    
    /**
     * @brief Build a table of Fibonacci numbers up to a given value
     * @return Vector of (index, F_index) pairs, in ascending order
     */
    static std::vector<std::pair<uint32_t, BigInt>> fibTableUpTo(const BigInt& limit) {
        std::vector<std::pair<uint32_t, BigInt>> table;
        BigInt a(0), b(1);
        uint32_t idx = 1;
        
        while (b <= limit) {
            table.push_back({idx + 1, b});  // F_2 = 1, F_3 = 2, ...
            BigInt c = a + b;
            a = b;
            b = c;
            ++idx;
        }
        
        return table;
    }
};

/**
 * @brief Zeckendorf representation for BigInt
 * 
 * Represents any positive BigInt as a sum of non-consecutive Fibonacci numbers.
 */
class ZeckendorfBigInt {
public:
    /**
     * @brief Convert a BigInt to its Zeckendorf representation
     * 
     * Returns sorted indices (descending) of Fibonacci numbers used.
     * Guaranteed: no two consecutive indices.
     * 
     * @param n The number to represent (must be non-negative)
     * @return Vector of Fibonacci indices (1-indexed: F_2=1, F_3=2, ...)
     */
    static std::vector<uint32_t> encode(const BigInt& n) {
        if (n.isZero() || n.isNegative()) return {};
        
        // Build Fibonacci table up to n
        auto table = BigFibonacci::fibTableUpTo(n);
        
        // Greedy: subtract largest possible Fibonacci number
        BigInt remaining = n;
        std::vector<uint32_t> indices;
        
        for (int i = static_cast<int>(table.size()) - 1; i >= 0 && !remaining.isZero(); --i) {
            if (table[i].second <= remaining) {
                indices.push_back(table[i].first);
                remaining = remaining - table[i].second;
            }
        }
        
        return indices;
    }
    
    /**
     * @brief Convert Zeckendorf indices back to BigInt
     */
    static BigInt decode(const std::vector<uint32_t>& indices) {
        BigInt result(0);
        for (uint32_t idx : indices) {
            result = result + BigFibonacci::fib(idx);
        }
        return result;
    }
    
    /**
     * @brief Verify the Zeckendorf property: no consecutive indices
     */
    static bool isValid(const std::vector<uint32_t>& indices) {
        if (indices.size() <= 1) return true;
        
        std::vector<uint32_t> sorted = indices;
        std::sort(sorted.begin(), sorted.end());
        
        for (size_t i = 0; i + 1 < sorted.size(); ++i) {
            if (sorted[i] + 1 == sorted[i + 1]) return false;
        }
        return true;
    }
    
    /**
     * @brief Convert to binary word (string of 0s and 1s)
     * 
     * Position i in the word is 1 iff F_{i+2} appears in the representation.
     */
    static std::string toBinary(const std::vector<uint32_t>& indices) {
        if (indices.empty()) return "0";
        
        uint32_t maxIdx = *std::max_element(indices.begin(), indices.end());
        std::string result(maxIdx, '0');  // length = maxIdx
        
        for (uint32_t idx : indices) {
            if (idx >= 2 && idx <= maxIdx + 1) {
                result[maxIdx - idx + 1] = '1';
            }
        }
        
        return result;
    }
    
    /**
     * @brief Normalize a raw -digit sequence using carry rules
     * 
     * Input: a sequence of (possibly non-normal) digit values d_0, d_1, ...
     * where the value is  d_i  F_{i+2}
     * 
     * Carry rules:
     *   d_i  2    d_i -= 2, d_{i+1} += 1, d_{i-1} += 1 (if i>0) or d_i -= 2, d_{i+1} += 1 (if i=0)
     *   Actually, the correct carry: d_i  2 means we use F_{i+2} twice,
     *   and since F_{i+2} = F_{i+1} + F_i (for i0):
     *     ...d_i...  ...(d_i - 2)... with d_{i+1} += 1, d_{i-1} += 1
     *   For adjacent 1s (d_i=1, d_{i+1}=1):
     *     F_{i+2} + F_{i+3} = F_{i+4}, so d_i -= 1, d_{i+1} -= 1, d_{i+2} += 1
     * 
     * @param digits Mutable digit sequence d_0, d_1, ... (will be normalized in place)
     */
    static void normalizeDigits(std::vector<int>& digits) {
        bool changed = true;
        while (changed) {
            changed = false;
            
            // Ensure capacity
            while (digits.size() < 2 || digits.back() >= 2 || 
                   (digits.size() >= 2 && digits[digits.size()-1] >= 1 && digits[digits.size()-2] >= 1)) {
                digits.push_back(0);
            }
            
            // Rule 1: d_i >= 2  carry using 2*F_n = F_{n+1} + F_{n-2}
            // digits[i] represents coefficient of F_{i+2}
            // So 2*F_{i+2} = F_{i+3} + F_i
            //   F_{i+3} is at digits[i+1], F_i is at digits[i-2]
            for (size_t i = 0; i < digits.size(); ++i) {
                if (digits[i] >= 2) {
                    digits[i] -= 2;
                    if (i + 1 >= digits.size()) digits.push_back(0);
                    digits[i + 1] += 1;  // F_{i+3}
                    if (i >= 2) {
                        digits[i - 2] += 1;  // F_i
                    } else if (i == 1) {
                        // F_1 = F_2 = 1, maps to digits[0]
                        digits[0] += 1;
                    }
                    // For i=0: 2*F_2 = F_3 (F_0 = 0, no lower carry)
                    changed = true;
                }
            }
            
            // Rule 2: Adjacent 1s  carry  
            // d_i  1 and d_{i+1}  1  d_i -= 1, d_{i+1} -= 1, d_{i+2} += 1
            for (size_t i = 0; i + 1 < digits.size(); ++i) {
                if (digits[i] >= 1 && digits[i + 1] >= 1) {
                    digits[i] -= 1;
                    digits[i + 1] -= 1;
                    if (i + 2 >= digits.size()) digits.push_back(0);
                    digits[i + 2] += 1;
                    changed = true;
                }
            }
        }
        
        // Remove trailing zeros
        while (digits.size() > 1 && digits.back() == 0) {
            digits.pop_back();
        }
    }
    
    /**
     * @brief Compute the -probe: (N) = _{k  Zeck(N)} ^k
     * 
     * Maps N  Z[] via its Zeckendorf representation.
     * This is injective (a key property for fingerprinting).
     * 
     * @param indices Zeckendorf indices of N
     * @return (N) as an element of Z[]
     */
    static ZPhi alphaProbe(const std::vector<uint32_t>& indices) {
        ZPhi result;
        FibPow2Cache cache;
        
        for (uint32_t k : indices) {
            // ^k = F_k + F_{k-1}
            auto [fk, fk1] = BigFibonacci::fibPair(k);
            // ^k = fk + fk_minus_1 where fk_minus_1 = fk1 - fk
            BigInt fk_minus_1 = fk1 - fk;  // F_{k-1} = F_{k+1} - F_k
            // Wait, (F_k, F_{k+1}) = fibPair(k), so F_{k-1} = F_{k+1} - F_k
            result = result + ZPhi(fk_minus_1, fk);  // F_{k-1} + F_k  
        }
        
        return result;
    }
};

} // namespace ring
} // namespace autodiscover
