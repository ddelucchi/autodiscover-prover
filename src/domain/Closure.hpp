/**
 * @file Closure.hpp
 * @brief Golden ratio closure, Z[] ring arithmetic, and probe generators
 * 
 * =============================================================================
 * Z[] Ring: The Golden Ring
 * =============================================================================
 * 
 * Z[] = {a + b : a, b  } is a ring under:
 *   (a + b) + (c + d) = (a+c) + (b+d)
 *   (a + b)(c + d) = (ac + bd) + (ad + bc + bd)  [using  =  + 1]
 * 
 * Canonical form: a + b with a, b  
 * 
 * Ring norm: N(a + b) = |a + ab - b|
 *   - Multiplicative: N(xy) = N(x)N(y)
 *   - Units: x  Z[]* iff N(x) = 1
 * 
 * =============================================================================
 * Fibonacci Recursion and Zeckendorf
 * =============================================================================
 * 
 * Fibonacci: F_0=0, F_1=1, F_{n+2}=F_{n+1}+F_n
 *   - ^n = F_n + F_{n-1} (golden power formula)
 * 
 * Zeckendorf Representation:
 *   Every n   has unique representation as sum of non-consecutive F_k
 *   Binary: n  word in {0,1}* with no "11" substring
 * 
 * =============================================================================
 * Probe Generators
 * =============================================================================
 * 
 * Integer probes (N), r(N) map integers N to Z[]:
 *   (N) = _{kZ(N)} ^k   (Z(N) = Zeckendorf indices of N)
 *   r(N) = (N) / |(N)|    (normalized probe)
 * 
 * Phase probe: (N) = arg((N)) when viewed in complex plane via   e^{i/5}
 * 
 * =============================================================================
 * Carry Rewriting and Normal Forms
 * =============================================================================
 * 
 * Carry rule: ...011...  ...100... (Zeckendorf normalization)
 * This corresponds to: F_k + F_{k+1} = F_{k+2}
 * 
 * A -ary word is normal if it contains no "11" substring.
 * Normal word count: N(n) = F_{n+2}
 * Entropy: h_top = log()  0.4812
 * 
 */

#ifndef AUTODISCOVER_DOMAIN_CLOSURE_HPP
#define AUTODISCOVER_DOMAIN_CLOSURE_HPP

#include "../core/Term.hpp"
#include "../core/Constants.hpp"
#include "../logic/Equation.hpp"
#include <vector>
#include <cmath>
#include <limits>
#include <string>
#include <algorithm>
#include <stdexcept>

namespace autodiscover {
namespace domain {

using core::Term;
using core::TermFactory;
using logic::Equation;

// =============================================================================
// FUNDAMENTAL CONSTANTS
// =============================================================================

/**
 * @brief Golden ratio constant and operations
 * 
 * Canonical PHI/PHI_INV/SQRT5 values live in core/Constants.hpp.
 * This class re-uses those values for its static members.
 */
class GoldenRatio {
public:
    //  = (1 + 5)/2  (canonical: constants::PHI)
    static constexpr double PHI = constants::PHI;
    
    // 1/ =  - 1  (canonical: constants::PHI_INV)
    static constexpr double PHI_INV = constants::PHI_INV;
    
    // 5  (canonical: constants::SQRT5)
    static constexpr double SQRT5 = constants::SQRT5;
    
    //  = (1 - 5)/2 = -1/ (conjugate root)
    static constexpr double PSI = -constants::PHI_INV;
    
    /**
     * @brief Fibonacci step: R(z) = 1 + 1/z
     */
    [[nodiscard]] static double fibStep(double z) {
        if (std::abs(z) < std::numeric_limits<double>::epsilon()) {
            return std::numeric_limits<double>::infinity();
        }
        return 1.0 + 1.0 / z;
    }
    
    /**
     * @brief Iterate R until convergence to 
     */
    [[nodiscard]] static double convergeToGolden(double initial = 1.0, int maxIter = 100) {
        double z = initial;
        for (int i = 0; i < maxIter; ++i) {
            double next = fibStep(z);
            if (std::abs(next - z) < 1e-15) {
                return next;
            }
            z = next;
        }
        return z;
    }
    
    /**
     * @brief Check if a value is (approximately) 
     */
    [[nodiscard]] static bool isGolden(double x, double tolerance = 1e-10) {
        return std::abs(x - PHI) < tolerance;
    }
    
    /**
     * @brief Verify the golden ratio identity:  =  + 1
     */
    [[nodiscard]] static bool verifyIdentity(double x, double tolerance = 1e-10) {
        return std::abs(x * x - x - 1.0) < tolerance;
    }
    
    /**
     * @brief Compute ^n using Fibonacci recurrence
     * ^n = F_n   + F_{n-1}
     */
    [[nodiscard]] static double phiPower(int n);
};

// =============================================================================
// FIBONACCI SEQUENCE
// =============================================================================

/**
 * @brief Fibonacci sequence and Zeckendorf representation
 */
class Fibonacci {
public:
    /**
     * @brief Compute F_n using fast iteration
     */
    [[nodiscard]] static uint64_t fib(uint32_t n) {
        if (n == 0) return 0;
        if (n == 1) return 1;
        
        uint64_t a = 0, b = 1;
        for (uint32_t i = 2; i <= n; ++i) {
            uint64_t c = a + b;
            a = b;
            b = c;
        }
        return b;
    }
    
    /**
     * @brief Compute F_n using fast doubling O(log n)
     * 
     * Uses identities:
     *   F_{2k} = F_k (2F_{k+1} - F_k)
     *   F_{2k+1} = F_k + F_{k+1}
     */
    [[nodiscard]] static uint64_t fibFast(uint32_t n) {
        if (n == 0) return 0;
        return fibFastHelper(n).first;
    }
    
    /**
     * @brief Binet's formula: F_n = ( - )/5
     */
    [[nodiscard]] static double binetFormula(uint32_t n) {
        return (std::pow(GoldenRatio::PHI, n) - std::pow(GoldenRatio::PSI, n)) 
               / GoldenRatio::SQRT5;
    }
    
    /**
     * @brief Count of normal words of length n: N(n) = F_{n+2}
     */
    [[nodiscard]] static uint64_t normalWordCount(uint32_t n) {
        return fib(n + 2);
    }
    
    /**
     * @brief Convert integer to Zeckendorf representation
     * Returns indices of Fibonacci numbers used (1-indexed: F_1, F_2, ...)
     * Guarantee: no two consecutive indices (Zeckendorf property)
     */
    [[nodiscard]] static std::vector<uint32_t> toZeckendorf(uint64_t n) {
        if (n == 0) return {};
        
        // Build Fibonacci table up to n
        std::vector<uint64_t> fibs = {1, 2};
        while (fibs.back() <= n) {
            size_t sz = fibs.size();
            fibs.push_back(fibs[sz-1] + fibs[sz-2]);
        }
        
        std::vector<uint32_t> indices;
        for (size_t i = fibs.size(); i-- > 0 && n > 0; ) {
            if (fibs[i] <= n) {
                indices.push_back(static_cast<uint32_t>(i + 2)); // F_2 = 1, F_3 = 2, ...
                n -= fibs[i];
            }
        }
        
        return indices;
    }
    
    /**
     * @brief Convert Zeckendorf indices to binary word
     * Index k  position k-2 is 1
     */
    [[nodiscard]] static std::string zeckendorfToBinary(const std::vector<uint32_t>& indices,
                                                          uint32_t length = 0) {
        if (indices.empty()) return std::string(length, '0');
        
        uint32_t maxIdx = *std::max_element(indices.begin(), indices.end());
        uint32_t wordLen = std::max(length, maxIdx);
        std::string result(wordLen, '0');
        
        for (uint32_t idx : indices) {
            if (idx >= 2 && idx <= wordLen + 1) {
                result[wordLen - (idx - 1)] = '1';
            }
        }
        
        return result;
    }
    
    /**
     * @brief Verify Zeckendorf property: no consecutive indices
     */
    [[nodiscard]] static bool isValidZeckendorf(const std::vector<uint32_t>& indices) {
        if (indices.size() <= 1) return true;
        
        std::vector<uint32_t> sorted = indices;
        std::sort(sorted.begin(), sorted.end());
        
        for (size_t i = 0; i + 1 < sorted.size(); ++i) {
            if (sorted[i] + 1 == sorted[i + 1]) {
                return false; // Consecutive found
            }
        }
        return true;
    }
    
    /**
     * @brief Convert from Zeckendorf back to integer
     */
    [[nodiscard]] static uint64_t fromZeckendorf(const std::vector<uint32_t>& indices) {
        uint64_t result = 0;
        for (uint32_t idx : indices) {
            result += fib(idx);
        }
        return result;
    }

private:
    // Helper for fast doubling: returns (F_n, F_{n+1})
    [[nodiscard]] static std::pair<uint64_t, uint64_t> fibFastHelper(uint32_t n) {
        if (n == 0) return {0, 1};
        
        auto [fk, fk1] = fibFastHelper(n / 2);
        uint64_t f2k = fk * (2 * fk1 - fk);
        uint64_t f2k1 = fk * fk + fk1 * fk1;
        
        if (n % 2 == 0) {
            return {f2k, f2k1};
        } else {
            return {f2k1, f2k + f2k1};
        }
    }
};

// Implementation of phiPower  exact Fibonacci identity for ALL exponents
inline double GoldenRatio::phiPower(int n) {
    if (n == 0) return 1.0;
    if (n > 0) {
        uint64_t fn = Fibonacci::fib(static_cast<uint32_t>(n));
        uint64_t fn1 = Fibonacci::fib(static_cast<uint32_t>(n - 1));
        return fn * PHI + fn1;
    } else {
        // Exact: ^{-m} = (-1)^m (F_{m+1} - F_m  )  for m = -n > 0
        // This avoids std::pow(PHI_INV, ...) which accumulates float error.
        uint32_t m = static_cast<uint32_t>(-n);
        uint64_t fm  = Fibonacci::fib(m);
        uint64_t fm1 = Fibonacci::fib(m + 1);
        double val = static_cast<double>(fm1) - static_cast<double>(fm) * PHI;
        return (m % 2 == 0) ? val : -val;
    }
}

// =============================================================================
// Z[] RING ELEMENT (lightweight, int64-based)
// =============================================================================

/**
 * @brief Lightweight element of Z[] = {a + b : a, b  }
 * 
 * Represents numbers in the golden ring with exact int64_t arithmetic.
 * Suitable for small-coefficient computations where overflow is not
 * a concern (Fibonacci indices < ~90, golden integer probes, etc.).
 * 
 * @note For arbitrary-precision Z[] with BigInt coefficients,
 *       see ring::ZPhi in ring/ZPhi.hpp. That class provides the
 *       full-fidelity representation used by the encoding pipeline.
 */
class GoldenInt {
public:
    int64_t a;  // Constant coefficient
    int64_t b;  //  coefficient
    
    // Canonical form: a + b
    
    GoldenInt() : a(0), b(0) {}
    GoldenInt(int64_t a_) : a(a_), b(0) {}
    GoldenInt(int64_t a_, int64_t b_) : a(a_), b(b_) {}
    
    // Addition: (a + b) + (c + d) = (a+c) + (b+d)
    [[nodiscard]] GoldenInt operator+(const GoldenInt& o) const {
        return GoldenInt(a + o.a, b + o.b);
    }
    
    // Subtraction
    [[nodiscard]] GoldenInt operator-(const GoldenInt& o) const {
        return GoldenInt(a - o.a, b - o.b);
    }
    
    // Negation
    [[nodiscard]] GoldenInt operator-() const {
        return GoldenInt(-a, -b);
    }
    
    /**
     * @brief Multiplication using  =  + 1
     * 
     * (a + b)(c + d) = ac + ad + bc + bd
     *                  = ac + ad + bc + bd( + 1)
     *                  = (ac + bd) + (ad + bc + bd)
     */
    [[nodiscard]] GoldenInt operator*(const GoldenInt& o) const {
        return GoldenInt(
            a * o.a + b * o.b,           // constant term
            a * o.b + b * o.a + b * o.b  //  coefficient
        );
    }
    
    // Scalar multiplication
    [[nodiscard]] GoldenInt operator*(int64_t k) const {
        return GoldenInt(a * k, b * k);
    }
    
    // Equality
    [[nodiscard]] bool operator==(const GoldenInt& o) const {
        return a == o.a && b == o.b;
    }
    
    [[nodiscard]] bool operator!=(const GoldenInt& o) const {
        return !(*this == o);
    }
    
    /**
     * @brief Ring norm: N(a + b) = |a + ab - b|
     * 
     * This is the absolute value of the algebraic norm.
     * N(xy) = N(x)N(y) (multiplicative)
     */
    [[nodiscard]] int64_t norm() const {
        return std::abs(a * a + a * b - b * b);
    }
    
    /**
     * @brief Signed norm (can be negative): a + ab - b
     */
    [[nodiscard]] int64_t signedNorm() const {
        return a * a + a * b - b * b;
    }
    
    /**
     * @brief Conjugate in Z[]: conjugate(a + b) = a + b - b = (a+b) - b
     * 
     * More precisely, (a + b) = a + b = a + b(1-) = (a+b) - b
     * where  = 1 -  = -1/ is the Galois conjugate of 
     */
    [[nodiscard]] GoldenInt conjugate() const {
        return GoldenInt(a + b, -b);
    }
    
    /**
     * @brief Check if this is a unit (invertible in Z[])
     * Units are exactly elements with norm 1
     * They are: ^n for all n  
     */
    [[nodiscard]] bool isUnit() const {
        return norm() == 1;
    }
    
    /**
     * @brief Check if zero
     */
    [[nodiscard]] bool isZero() const {
        return a == 0 && b == 0;
    }
    
    /**
     * @brief Evaluate to double
     */
    [[nodiscard]] double toDouble() const {
        return static_cast<double>(a) + static_cast<double>(b) * GoldenRatio::PHI;
    }
    
    /**
     * @brief Convert to signed integer if possible
     */
    [[nodiscard]] bool isInteger() const {
        return b == 0;
    }
    
    /**
     * @brief Create 
     */
    static GoldenInt phi() { return GoldenInt(0, 1); }
    
    /**
     * @brief Create ^n
     */
    static GoldenInt phiPower(int n) {
        if (n == 0) return GoldenInt(1, 0);
        if (n == 1) return GoldenInt(0, 1);
        
        if (n > 0) {
            // ^n = F_n + F_{n-1}
            uint64_t fn = Fibonacci::fib(static_cast<uint32_t>(n));
            uint64_t fn1 = Fibonacci::fib(static_cast<uint32_t>(n - 1));
            return GoldenInt(static_cast<int64_t>(fn1), static_cast<int64_t>(fn));
        } else {
            // ^{-n} = (-1)^n (F_{n-1} - F_n) / ... 
            // Simpler: just compute by repeated division
            GoldenInt result(1, 0);
            GoldenInt phiInv(1, -1); // 1/ =  - 1 in different form... 
            // Actually: 1/ =  - 1, so we need: ^{-1} represented as -1 + ... wait
            // ^{-1} =  - 1 which means: if we have ^{-1} = (a + b)
            // then (a + b) = 1, so a + b = 1, a + b(+1) = 1
            // b + (a+b) = 1  b = 1, a+b = 0  a = -1
            // So ^{-1} = -1 + 
            phiInv = GoldenInt(-1, 1);
            for (int i = 0; i > n; --i) {
                result = result * phiInv;
            }
            return result;
        }
    }
    
    /**
     * @brief Create from Fibonacci index: F_k as Z[] element
     * Note: F_k    Z[]
     */
    static GoldenInt fibElement(uint32_t k) {
        return GoldenInt(static_cast<int64_t>(Fibonacci::fib(k)), 0);
    }
};

/**
 * @brief Arithmetic operations in Z[] (int64-based GoldenInt)
 * 
 * @note For arbitrary-precision Z[] arithmetic with BigInt,
 *       see ring::ZPhi in ring/ZPhi.hpp.
 */
class GoldenArithmetic {
public:
    /**
     * @brief GCD in Z[] using Euclidean algorithm
     * 
     * Z[] is a Euclidean domain with norm function.
     */
    [[nodiscard]] static GoldenInt gcd(GoldenInt x, GoldenInt y) {
        while (!y.isZero()) {
            GoldenInt q = divide(x, y);
            GoldenInt r = x - q * y;
            x = y;
            y = r;
        }
        return x;
    }
    
    /**
     * @brief Division with remainder in Z[]
     * 
     * Returns q such that N(x - qy) is minimized
     */
    [[nodiscard]] static GoldenInt divide(const GoldenInt& x, const GoldenInt& y) {
        if (y.isZero()) {
            throw std::domain_error("Division by zero in Z[]");
        }
        
        // Exact division in Z[] using the Galois conjugate and norm.
        // x/y = x / (y) = x / N(y)
        // where  = conjugate(y) = (y.a + y.b) - y.b
        // and N(y) = y.a + y.ay.b - y.b  
        GoldenInt yBar(y.a + y.b, -y.b);   // Galois conjugate
        GoldenInt num = x * yBar;            // numerator in Z[]
        int64_t denom = y.a * y.a + y.a * y.b - y.b * y.b;  // N(y)  
        
        if (denom == 0) {
            throw std::domain_error("Division by zero norm in Z[]");
        }
        
        // Round each coefficient to nearest integer (Euclidean division)
        // Using round-half-to-even for consistency
        auto divRound = [](int64_t n, int64_t d) -> int64_t {
            // Round n/d to nearest integer
            if (d < 0) { n = -n; d = -d; } // normalize denominator positive
            int64_t q = n / d;
            int64_t r = n % d;
            // If remainder > half denominator, round up (away from zero towards +)
            if (2 * std::abs(r) > d) {
                q += (r > 0) ? 1 : -1;
            } else if (2 * std::abs(r) == d) {
                // Tie-break: round to even
                if (q % 2 != 0) q += (r > 0) ? 1 : -1;
            }
            return q;
        };
        
        int64_t qa = divRound(num.a, denom);
        int64_t qb = divRound(num.b, denom);
        return GoldenInt(qa, qb);
    }
    
    /**
     * @brief Check if x divides y in Z[]
     */
    [[nodiscard]] static bool divides(const GoldenInt& x, const GoldenInt& y) {
        if (x.isZero()) return y.isZero();
        GoldenInt q = divide(y, x);
        return (q * x) == y;
    }
    
    /**
     * @brief Prime check in Z[]
     * p is prime if p is not a unit and p | ab  p | a or p | b
     */
    [[nodiscard]] static bool isPrime(const GoldenInt& p) {
        if (p.isUnit() || p.isZero()) return false;
        int64_t n = p.norm();
        // Element is prime in Z[] iff its norm is a prime integer
        // that remains prime in Z (doesn't split)
        return isPrimeInteger(n) && !splitsPrime(n);
    }
    
private:
    [[nodiscard]] static bool isPrimeInteger(int64_t n) {
        if (n < 2) return false;
        if (n == 2) return true;
        if (n % 2 == 0) return false;
        for (int64_t i = 3; i * i <= n; i += 2) {
            if (n % i == 0) return false;
        }
        return true;
    }
    
    [[nodiscard]] static bool splitsPrime(int64_t p) {
        // A prime p splits in Z[] iff p  1 (mod 5)
        int mod5 = static_cast<int>(p % 5);
        return (mod5 == 1 || mod5 == 4);
    }
};

// =============================================================================
// PROBE GENERATORS
// =============================================================================

/**
 * @brief Probe generators for SCOUT
 * 
 * Maps integers to Z[] or phase values for use in SCOUT computations.
 */
class ProbeGenerator {
public:
    /**
     * @brief (N) = _{k  Z(N)} ^k
     * 
     * Sum of -powers for Zeckendorf indices of N
     */
    [[nodiscard]] static GoldenInt alpha(uint64_t N) {
        auto zeck = Fibonacci::toZeckendorf(N);
        GoldenInt result(0, 0);
        for (uint32_t k : zeck) {
            result = result + GoldenInt::phiPower(static_cast<int>(k));
        }
        return result;
    }
    
    /**
     * @brief Numeric (N) as double
     */
    [[nodiscard]] static double alphaNumeric(uint64_t N) {
        return alpha(N).toDouble();
    }
    
    /**
     * @brief r(N) = (N) / |(N)| (normalized probe, as double)
     */
    [[nodiscard]] static double rProbe(uint64_t N) {
        double a = alphaNumeric(N);
        return (a > 0) ? 1.0 : ((a < 0) ? -1.0 : 0.0);
    }
    
    /**
     * @brief (N) = arg((N)) when embedded in complex plane
     * 
     * Embeds   e^{i/5} (fifth root of unity rotation)
     */
    [[nodiscard]] static double thetaProbe(uint64_t N) {
        auto zeck = Fibonacci::toZeckendorf(N);
        double angle = 0.0;
        const double PHI_ANGLE = M_PI / 5.0; //   e^{i/5}
        
        for (uint32_t k : zeck) {
            angle += k * PHI_ANGLE;
        }
        
        // Normalize to [0, 2)
        while (angle >= 2.0 * M_PI) angle -= 2.0 * M_PI;
        while (angle < 0) angle += 2.0 * M_PI;
        
        return angle;
    }
    
    /**
     * @brief Fibonacci weight: W(N) = _{k  Z(N)} F_k
     */
    [[nodiscard]] static uint64_t fibWeight(uint64_t N) {
        auto zeck = Fibonacci::toZeckendorf(N);
        uint64_t weight = 0;
        for (uint32_t k : zeck) {
            weight += Fibonacci::fib(k);
        }
        return weight;
    }
    
    /**
     * @brief Zeckendorf length (number of 1s in binary Zeckendorf)
     */
    [[nodiscard]] static size_t zeckendorfLength(uint64_t N) {
        return Fibonacci::toZeckendorf(N).size();
    }
    
    /**
     * @brief Probe for SCOUT phase transport
     * Returns r_N = (N+1)/(N) approximation
     */
    [[nodiscard]] static double phaseRatio(uint64_t N) {
        if (N == 0) return GoldenRatio::PHI;
        double aN = alphaNumeric(N);
        double aN1 = alphaNumeric(N + 1);
        if (std::abs(aN) < 1e-15) return GoldenRatio::PHI;
        return aN1 / aN;
    }
};

// =============================================================================
// CLOSURE DETECTOR
// =============================================================================

/**
 * @brief Closure detection for golden ratio fixed point
 */
class ClosureDetector {
public:
    explicit ClosureDetector(TermFactory& factory) : factory_(factory) {}
    
    /**
     * @brief Check if a term represents the golden ratio
     */
    [[nodiscard]] bool isGoldenRatio(const Term* term) const {
        if (term->kind() == core::TermKind::Phi ||
            term->kind() == core::TermKind::PhiBar) {
            return true;
        }
        
        if (isRecursiveGolden(term)) {
            return true;
        }
        
        if (satisfiesQuadratic(term)) {
            return true;
        }
        
        return false;
    }
    
    /**
     * @brief Detect if iteration will converge to 
     */
    [[nodiscard]] bool willConvergeToGolden(const Term* term) const {
        if (isFibStepForm(term)) {
            return true;
        }
        return false;
    }
    
    /**
     * @brief Generate closure axiom: x = x + 1  x > 0  x = 
     */
    [[nodiscard]] std::unique_ptr<Equation> goldenClosureAxiom() {
        auto phi = factory_.phi();
        
        return std::make_unique<Equation>(
            factory_.add(factory_.scalar(1.0), factory_.inv(phi)),
            phi
        );
    }
    
    /**
     * @brief Check if Z[] element equals an expected value
     */
    [[nodiscard]] static bool verifyGoldenInt(const GoldenInt& x, int64_t expectedA, int64_t expectedB) {
        return x.a == expectedA && x.b == expectedB;
    }
    
private:
    TermFactory& factory_;
    
    [[nodiscard]] bool isRecursiveGolden(const Term* term) const {
        if (term->kind() != core::TermKind::Application) return false;
        if (term->symbol() != "add") return false;
        if (term->children().size() != 2) return false;
        
        const Term* lhs = term->children()[0];
        const Term* rhs = term->children()[1];
        
        if (lhs->kind() != core::TermKind::Scalar || lhs->symbol() != "1") {
            return false;
        }
        
        if (rhs->kind() != core::TermKind::Application) return false;
        if (rhs->symbol() != "inv") return false;
        
        return true;
    }
    
    [[nodiscard]] bool satisfiesQuadratic(const Term* term) const {
        return term->kind() == core::TermKind::Phi ||
               term->kind() == core::TermKind::PhiBar;
    }
    
    [[nodiscard]] bool isFibStepForm(const Term* term) const {
        if (term->kind() != core::TermKind::Application) return false;
        return term->symbol() == "FibStep" || term->symbol() == "fibStep" || term->symbol() == "R";
    }
};

// =============================================================================
// GOLDEN MEAN SHIFT
// =============================================================================

/**
 * @brief Entropy calculator for golden-mean shift
 */
class GoldenMeanShift {
public:
    /**
     * @brief Topological entropy: h_top = log 
     */
    [[nodiscard]] static double topologicalEntropy() {
        return std::log(GoldenRatio::PHI);
    }
    
    /**
     * @brief Growth rate of normal words: (log N(n)) / n  log  as n  
     */
    [[nodiscard]] static double growthRate(uint32_t n) {
        if (n == 0) return 0;
        return std::log(static_cast<double>(Fibonacci::normalWordCount(n))) / n;
    }
    
    /**
     * @brief Verify entropy converges to log 
     */
    [[nodiscard]] static bool verifyEntropy(uint32_t n, double tolerance = 1e-3) {
        double computed = growthRate(n);
        double expected = topologicalEntropy();
        return std::abs(computed - expected) < tolerance;
    }
    
    /**
     * @brief Enumerate all normal words of length n
     */
    /**
     * @brief Enumerate all normal words of length n (no consecutive 1s)
     *
     * Uses pure recursion  O(F_{n+2}) output, NO 2^n brute-force.
     * Safe for any n (output count is Fibonacci, not exponential in masks).
     * Hard-capped at n=60 to prevent runaway memory.
     */
    [[nodiscard]] static std::vector<std::string> enumerateNormalWords(uint32_t n) {
        static constexpr uint32_t MAX_N = 60; // F_62  4e12  already insane
        if (n > MAX_N) {
            // Return empty rather than hang
            return {};
        }
        std::vector<std::string> result;
        if (n == 0) {
            result.push_back("");
            return result;
        }
        // Recursive construction: a normal word of length n is either
        //   (normal word of length n-1) + '0', or
        //   (normal word of length n-2) + "10"
        // This generates exactly F_{n+2} words with no mask scanning.
        auto endWith0 = enumerateNormalWords(n - 1);
        for (auto& w : endWith0) {
            result.push_back(std::move(w) + '0');
        }
        if (n >= 2) {
            auto endWith10 = enumerateNormalWords(n - 2);
            for (auto& w : endWith10) {
                result.push_back(std::move(w) + "10");
            }
        } else {
            // n == 1: "1" is a valid single-char normal word
            result.push_back("1");
        }
        return result;
    }
    
    /**
     * @brief Check if binary word is normal (no consecutive 1s)
     */
    [[nodiscard]] static bool isNormalWord(const std::string& word) {
        for (size_t i = 0; i + 1 < word.length(); ++i) {
            if (word[i] == '1' && word[i + 1] == '1') {
                return false;
            }
        }
        return true;
    }
};

// =============================================================================
// CARRY REWRITER (ZECKENDORF NORMALIZATION)  
// =============================================================================
// NOTE: The canonical CarryRewriter implementation used throughout the codebase
// lives in logic/Normalizer.hpp (autodiscover::logic::CarryRewriter).
// It operates on std::vector<uint8_t> and provides isNormal(), normalize(),
// and countNormal(). The string-based variant below is provided as a
// convenience wrapper for Zeckendorf string representations.
// =============================================================================

/**
 * @brief String-based carry rewriting for Zeckendorf normalization
 * 
 * Implements the carry rule: ...011...  ...100...
 * This corresponds to: F_k + F_{k+1} = F_{k+2}
 * 
 * @note For the primary vector<uint8_t> implementation used by the
 *       prover core, see logic::CarryRewriter in Normalizer.hpp.
 */
class ZeckendorfStringNormalizer {
public:
    /**
     * @brief Normalize a binary word string to Zeckendorf form
     * 
     * Repeatedly applies: 11  100 (with carry propagation)
     * Until no consecutive 1s remain.
     */
    [[nodiscard]] static std::string normalize(const std::string& word) {
        if (word.empty()) return word;
        
        // Convert to mutable integer representation for easier carry
        std::vector<int> digits;
        for (char c : word) {
            digits.push_back(c == '1' ? 1 : 0);
        }
        
        // Normalize: eliminate consecutive 1s by carrying
        bool changed = true;
        while (changed) {
            changed = false;
            
            // Also normalize any digit > 1 first
            for (size_t i = 0; i < digits.size(); ++i) {
                while (digits[i] >= 2) {
                    digits[i] -= 2;
                    if (i > 0) {
                        digits[i - 1] += 1;
                    } else {
                        digits.insert(digits.begin(), 1);
                    }
                    if (i + 1 < digits.size()) {
                        digits[i + 1] += 1;
                    } else {
                        digits.push_back(1);
                    }
                    changed = true;
                }
            }
            
            // Carry rule: 11 at positions i, i+1  1 at position i-1, 00 at i, i+1
            for (size_t i = 0; i + 1 < digits.size(); ++i) {
                if (digits[i] >= 1 && digits[i + 1] >= 1) {
                    digits[i] -= 1;
                    digits[i + 1] -= 1;
                    if (i > 0) {
                        digits[i - 1] += 1;
                    } else {
                        digits.insert(digits.begin(), 1);
                    }
                    changed = true;
                }
            }
        }
        
        // Convert back to string, removing leading zeros
        std::string result;
        bool leadingZero = true;
        for (int d : digits) {
            if (d != 0) leadingZero = false;
            if (!leadingZero) {
                result += static_cast<char>('0' + d);
            }
        }
        
        return result.empty() ? "0" : result;
    }
    
    /**
     * @brief Check if normalization is needed
     */
    [[nodiscard]] static bool needsNormalization(const std::string& word) {
        return !GoldenMeanShift::isNormalWord(word);
    }
};

} // namespace domain
} // namespace autodiscover

#endif // AUTODISCOVER_DOMAIN_CLOSURE_HPP
