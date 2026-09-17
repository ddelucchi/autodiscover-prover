/**
 * @file MultiRingEval.hpp
 * @brief Multi-ring semantic evaluators for cheap falsification
 * 
 * ARCHITECTURE:
 * =============
 * 
 * These evaluators are the FIRST gate in the discovery pipeline.
 * They perform exact-arithmetic evaluation in multiple rings to quickly
 * falsify candidate conjectures BEFORE expensive proof search.
 * 
 * Chain:
 *   Conjecture  MultiRingGate  [if survives all rings]  Proof Search
 * 
 * If a conjecture fails in ANY ring, it cannot be a universal identity.
 * 
 * SUPPORTED RINGS:
 * 
 *   1. F_p (finite field of prime order p)
 *      - Exact mod-p arithmetic
 *      - Fast falsification: most false identities fail for small p
 *   
 *   2. Z/2^k (modular integers, power-of-2 modulus)
 *      - Catches carry-propagation errors
 *      - Reveals off-by-one bugs in integer identities
 *   
 *   3. Q (rationals as a/b in lowest terms)
 *      - Exact arithmetic with no rounding
 *      - More expensive but catches float-approximation false positives
 *   
 *   4. Z[] (golden ring  already used by fingerprinter)
 *      - Tests that identities hold in the actual target ring
 *      - Uses existing ZPhi infrastructure
 * 
 * WHY MULTIPLE RINGS?
 * -------------------
 * 
 * The Chinese Remainder Theorem intuition:
 * An identity that holds in F_2, F_3, F_5, F_7 AND Q is very likely
 * universally true. Each prime catches a different class of failures.
 * 
 * USAGE:
 * ------
 * 
 *   MultiRingGate gate;
 *   // Add test points for variables x, y, z
 *   gate.addTestPoint({{"x", 2}, {"y", 3}, {"z", 5}});
 *   gate.addTestPoint({{"x", 7}, {"y", 11}, {"z", 13}});
 *   
 *   // Test a conjecture: lhs =? rhs
 *   auto result = gate.test(lhs_term, rhs_term, factory);
 *   if (!result.passed) {
 *       // Falsified! result.failedRing tells you which one
 *   }
 * 
 * @author AutoDiscoverProver Team
 */

#pragma once

#include "../core/Term.hpp"
#include "../ring/ZPhi.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <cassert>
#include <numeric>
#include <sstream>
#include <array>
#include <algorithm>

namespace autodiscover {
namespace domain {

// =========================================================================
// RING 1: F_p (Finite Field of prime order)
// =========================================================================

/**
 * @brief Exact arithmetic in F_p = Z/pZ for prime p
 * 
 * All operations are mod p. Division uses modular inverse via Fermat's little theorem.
 */
class FieldFp {
public:
    using Value = int64_t;
    
    explicit FieldFp(int64_t p) : p_(p) {
        assert(p > 1 && "F_p requires p > 1");
    }
    
    [[nodiscard]] Value zero() const { return 0; }
    [[nodiscard]] Value one() const { return 1; }
    [[nodiscard]] int64_t characteristic() const { return p_; }
    
    [[nodiscard]] Value normalize(Value x) const {
        Value r = x % p_;
        return r < 0 ? r + p_ : r;
    }
    
    [[nodiscard]] Value add(Value a, Value b) const {
        return normalize(a + b);
    }
    
    [[nodiscard]] Value sub(Value a, Value b) const {
        return normalize(a - b);
    }
    
    [[nodiscard]] Value mul(Value a, Value b) const {
        return normalize(a * b);
    }
    
    [[nodiscard]] Value neg(Value a) const {
        return normalize(-a);
    }
    
    /**
     * @brief Modular inverse via Fermat's little theorem: a^{-1} = a^{p-2} mod p
     * @return inverse, or std::nullopt if a  0 (mod p)
     */
    [[nodiscard]] std::optional<Value> inv(Value a) const {
        a = normalize(a);
        if (a == 0) return std::nullopt;
        return powMod(a, p_ - 2);
    }
    
    [[nodiscard]] std::optional<Value> div(Value a, Value b) const {
        auto bInv = inv(b);
        if (!bInv) return std::nullopt;
        return mul(a, *bInv);
    }
    
    [[nodiscard]] Value powMod(Value base, int64_t exp) const {
        base = normalize(base);
        Value result = 1;
        while (exp > 0) {
            if (exp & 1) result = mul(result, base);
            base = mul(base, base);
            exp >>= 1;
        }
        return result;
    }
    
    [[nodiscard]] std::string name() const { 
        return "F_" + std::to_string(p_); 
    }
    
private:
    int64_t p_;
};

// =========================================================================
// RING 2: Z/2^k (Modular integers, power-of-2 modulus)
// =========================================================================

/**
 * @brief Exact arithmetic in Z/2^k
 * 
 * NOT a field (no division in general), but a ring.
 * Catches carry-propagation and overflow patterns.
 */
class RingMod2k {
public:
    using Value = uint64_t;
    
    explicit RingMod2k(unsigned k) : k_(k) {
        assert(k > 0 && k <= 63 && "k must be in [1, 63]");
        mask_ = (k == 64) ? UINT64_MAX : ((1ULL << k) - 1);
    }
    
    [[nodiscard]] Value zero() const { return 0; }
    [[nodiscard]] Value one() const { return 1; }
    [[nodiscard]] unsigned bits() const { return k_; }
    
    [[nodiscard]] Value normalize(Value x) const {
        return x & mask_;
    }
    
    [[nodiscard]] Value add(Value a, Value b) const {
        return normalize(a + b);
    }
    
    [[nodiscard]] Value sub(Value a, Value b) const {
        return normalize(a - b);
    }
    
    [[nodiscard]] Value mul(Value a, Value b) const {
        return normalize(a * b);
    }
    
    [[nodiscard]] Value neg(Value a) const {
        return normalize(~a + 1);  // Two's complement negation
    }
    
    /**
     * @brief Inverse exists only for odd elements in Z/2^k
     * @return inverse if it exists, nullopt otherwise
     */
    [[nodiscard]] std::optional<Value> inv(Value a) const {
        a = normalize(a);
        if ((a & 1) == 0) return std::nullopt;  // Even elements not invertible
        
        // Newton's method for modular inverse: x_{n+1} = x_n(2 - a*x_n) mod 2^k
        Value x = 1;
        for (unsigned i = 0; i < 6; ++i) {  // 6 iterations gives 2^64 convergence
            x = normalize(x * normalize(2 - normalize(a * x)));
        }
        return normalize(x);
    }
    
    [[nodiscard]] std::string name() const {
        return "Z/2^" + std::to_string(k_);
    }
    
private:
    unsigned k_;
    uint64_t mask_;
};

// =========================================================================
// RING 3: Q (Rationals)
// =========================================================================

/**
 * @brief Exact rational arithmetic a/b with gcd reduction
 * 
 * Canonical form: b > 0, gcd(|a|, b) = 1, 0 represented as 0/1
 */
class Rational {
public:
    int64_t num;
    int64_t den;  // Always > 0 in canonical form
    
    Rational() : num(0), den(1) {}
    explicit Rational(int64_t n) : num(n), den(1) {}
    Rational(int64_t n, int64_t d) : num(n), den(d) {
        assert(d != 0 && "Rational: denominator cannot be zero");
        canonicalize();
    }
    
    void canonicalize() {
        if (den < 0) { num = -num; den = -den; }
        if (num == 0) { den = 1; return; }
        int64_t g = gcd64(std::abs(num), den);
        num /= g;
        den /= g;
    }
    
    [[nodiscard]] bool isZero() const { return num == 0; }
    
    [[nodiscard]] Rational operator+(const Rational& o) const {
        // Cross-reduce before multiplying to prevent int64_t overflow:
        //   a/b + c/d = (a(d/g) + c(b/g)) / (b/gd)   where g = gcd(b, d)
        int64_t g = gcd64(den, o.den);
        int64_t b_g = den / g;      // den / gcd
        int64_t d_g = o.den / g;    // o.den / gcd
        return Rational(num * d_g + o.num * b_g, b_g * o.den);
    }
    
    [[nodiscard]] Rational operator-(const Rational& o) const {
        int64_t g = gcd64(den, o.den);
        int64_t b_g = den / g;
        int64_t d_g = o.den / g;
        return Rational(num * d_g - o.num * b_g, b_g * o.den);
    }
    
    [[nodiscard]] Rational operator*(const Rational& o) const {
        // Cross-reduce before multiplying to prevent int64_t overflow:
        //   (a/b)  (c/d) = (a/g1)(c/g2) / (b/g2)(d/g1)
        //   where g1 = gcd(|a|,d), g2 = gcd(|c|,b)
        int64_t g1 = gcd64(std::abs(num), o.den);
        int64_t g2 = gcd64(std::abs(o.num), den);
        return Rational((num / g1) * (o.num / g2), (den / g2) * (o.den / g1));
    }
    
    [[nodiscard]] std::optional<Rational> reciprocal() const {
        if (num == 0) return std::nullopt;
        return Rational(den, num);
    }
    
    [[nodiscard]] Rational operator-() const {
        return Rational(-num, den);
    }
    
    [[nodiscard]] bool operator==(const Rational& o) const {
        return num == o.num && den == o.den;
    }
    
    [[nodiscard]] bool operator!=(const Rational& o) const {
        return !(*this == o);
    }
    
    [[nodiscard]] std::string toString() const {
        if (den == 1) return std::to_string(num);
        return std::to_string(num) + "/" + std::to_string(den);
    }
    
private:
    static int64_t gcd64(int64_t a, int64_t b) {
        while (b) { a %= b; std::swap(a, b); }
        return a;
    }
};

// =========================================================================
// TERM EVALUATOR TEMPLATES
// =========================================================================

/**
 * @brief Evaluate a term in a given ring
 * 
 * Template parameter R must provide zero(), one(), add(), sub(), mul(), neg()
 * and have a Value type.
 * 
 * @tparam R Ring type
 * @tparam V Value type in the ring
 */
template<typename V>
struct RingAssignment {
    std::unordered_map<std::string, V> vars;
};

/**
 * @brief Evaluate a term in F_p
 */
inline std::optional<FieldFp::Value> evalFp(
    const core::Term* term,
    const FieldFp& field,
    const RingAssignment<FieldFp::Value>& env
) {
    if (!term) return std::nullopt;
    
    switch (term->kind()) {
        case core::TermKind::Scalar: {
            // Truncate double to integer, then mod p
            int64_t val = static_cast<int64_t>(term->scalarValue());
            return field.normalize(val);
        }
        
        case core::TermKind::Variable: {
            auto it = env.vars.find(term->symbol());
            if (it != env.vars.end()) return it->second;
            return std::nullopt;  // Unbound variable  cannot evaluate
        }
        
        case core::TermKind::Constant: {
            if (term->symbol() == "0") return field.zero();
            if (term->symbol() == "1") return field.one();
            return std::nullopt;  // Unknown constant
        }
        
        case core::TermKind::Phi: {
            //  is not generally representable in F_p, skip
            return std::nullopt;
        }
        
        case core::TermKind::PhiBar: {
            //  is not generally representable in F_p, skip
            return std::nullopt;
        }
        
        case core::TermKind::Application: {
            const auto& sym = term->symbol();
            const auto& children = term->children();
            
            // Evaluate children
            std::vector<FieldFp::Value> args;
            for (const core::Term* child : children) {
                auto val = evalFp(child, field, env);
                if (!val) return std::nullopt;
                args.push_back(*val);
            }
            
            if ((sym == "add" || sym == "+") && args.size() >= 2) {
                FieldFp::Value sum = args[0];
                for (size_t i = 1; i < args.size(); ++i)
                    sum = field.add(sum, args[i]);
                return sum;
            }
            if ((sym == "sub" || sym == "-") && args.size() >= 2)
                return field.sub(args[0], args[1]);
            if ((sym == "mul" || sym == "*") && args.size() >= 2) {
                FieldFp::Value prod = args[0];
                for (size_t i = 1; i < args.size(); ++i)
                    prod = field.mul(prod, args[i]);
                return prod;
            }
            if (sym == "neg" && args.size() == 1)
                return field.neg(args[0]);
            if (sym == "inv" && args.size() == 1)
                return field.inv(args[0]);  // may return nullopt
            if ((sym == "div" || sym == "/") && args.size() == 2)
                return field.div(args[0], args[1]);
            if (sym == "pow" && args.size() == 2) {
                if (args[1] >= 0) return field.powMod(args[0], args[1]);
                auto invBase = field.inv(args[0]);
                if (!invBase) return std::nullopt;
                return field.powMod(*invBase, -args[1]);
            }
            
            return std::nullopt;  // Unknown function
        }
        
        default:
            return std::nullopt;
    }
}

/**
 * @brief Evaluate a term in Z/2^k
 */
inline std::optional<RingMod2k::Value> evalMod2k(
    const core::Term* term,
    const RingMod2k& ring,
    const RingAssignment<RingMod2k::Value>& env
) {
    if (!term) return std::nullopt;
    
    switch (term->kind()) {
        case core::TermKind::Scalar: {
            uint64_t val = static_cast<uint64_t>(static_cast<int64_t>(term->scalarValue()));
            return ring.normalize(val);
        }
        
        case core::TermKind::Variable: {
            auto it = env.vars.find(term->symbol());
            if (it != env.vars.end()) return it->second;
            return std::nullopt;  // Unbound variable  cannot evaluate
        }
        
        case core::TermKind::Constant: {
            if (term->symbol() == "0") return ring.zero();
            if (term->symbol() == "1") return ring.one();
            return std::nullopt;  // Unknown constant
        }
        
        case core::TermKind::Application: {
            const auto& sym = term->symbol();
            const auto& children = term->children();
            
            std::vector<RingMod2k::Value> args;
            for (const core::Term* child : children) {
                auto val = evalMod2k(child, ring, env);
                if (!val) return std::nullopt;
                args.push_back(*val);
            }
            
            if ((sym == "add" || sym == "+") && args.size() >= 2) {
                RingMod2k::Value sum = args[0];
                for (size_t i = 1; i < args.size(); ++i)
                    sum = ring.add(sum, args[i]);
                return sum;
            }
            if ((sym == "sub" || sym == "-") && args.size() >= 2)
                return ring.sub(args[0], args[1]);
            if ((sym == "mul" || sym == "*") && args.size() >= 2) {
                RingMod2k::Value prod = args[0];
                for (size_t i = 1; i < args.size(); ++i)
                    prod = ring.mul(prod, args[i]);
                return prod;
            }
            if (sym == "neg" && args.size() == 1)
                return ring.neg(args[0]);
            if (sym == "inv" && args.size() == 1)
                return ring.inv(args[0]);
            
            return std::nullopt;
        }
        
        default:
            return std::nullopt;
    }
}

/**
 * @brief Evaluate a term in Q (rationals)
 */
inline std::optional<Rational> evalQ(
    const core::Term* term,
    const RingAssignment<Rational>& env
) {
    if (!term) return std::nullopt;
    
    switch (term->kind()) {
        case core::TermKind::Scalar: {
            // Approximate double as rational (limited precision)
            double d = term->scalarValue();
            if (d == std::floor(d)) {
                return Rational(static_cast<int64_t>(d));
            }
            // Simple conversion: multiply by 10^6, reduce
            int64_t n = static_cast<int64_t>(d * 1000000.0);
            return Rational(n, 1000000);
        }
        
        case core::TermKind::Variable: {
            auto it = env.vars.find(term->symbol());
            if (it != env.vars.end()) return it->second;
            return std::nullopt;  // Unbound variable  cannot evaluate
        }
        
        case core::TermKind::Constant: {
            if (term->symbol() == "0") return Rational(0);
            if (term->symbol() == "1") return Rational(1);
            return std::nullopt;  // Unknown constant
        }
        
        case core::TermKind::Phi: {
            //  is irrational, not representable in Q
            return std::nullopt;
        }
        
        case core::TermKind::PhiBar: {
            //  is irrational, not representable in Q
            return std::nullopt;
        }
        
        case core::TermKind::Application: {
            const auto& sym = term->symbol();
            const auto& children = term->children();
            
            std::vector<Rational> args;
            for (const core::Term* child : children) {
                auto val = evalQ(child, env);
                if (!val) return std::nullopt;
                args.push_back(*val);
            }
            
            if ((sym == "add" || sym == "+") && args.size() >= 2) {
                Rational sum = args[0];
                for (size_t i = 1; i < args.size(); ++i)
                    sum = sum + args[i];
                return sum;
            }
            if ((sym == "sub" || sym == "-") && args.size() >= 2)
                return args[0] - args[1];
            if ((sym == "mul" || sym == "*") && args.size() >= 2) {
                Rational prod = args[0];
                for (size_t i = 1; i < args.size(); ++i)
                    prod = prod * args[i];
                return prod;
            }
            if (sym == "neg" && args.size() == 1)
                return -args[0];
            if (sym == "inv" && args.size() == 1)
                return args[0].reciprocal();
            if ((sym == "div" || sym == "/") && args.size() == 2) {
                auto recip = args[1].reciprocal();
                if (!recip) return std::nullopt;
                return args[0] * (*recip);
            }
            
            return std::nullopt;
        }
        
        default:
            return std::nullopt;
    }
}

// =========================================================================
// MULTI-RING GATE
// =========================================================================

/**
 * @brief Result of multi-ring falsification gate
 */
struct GateResult {
    bool passed = true;         // Did it survive all rings?
    std::string failedRing;     // Which ring falsified it?
    std::string failedDetail;   // Details of the counterexample
    size_t ringsTestedTotal = 0;
    size_t ringsPassed = 0;
    
    static GateResult pass(size_t total) {
        return {true, "", "", total, total};
    }
    
    static GateResult fail(const std::string& ring, const std::string& detail, 
                          size_t total, size_t passed) {
        return {false, ring, detail, total, passed};
    }
    
    [[nodiscard]] std::string toString() const {
        if (passed) {
            return "PASSED all " + std::to_string(ringsTestedTotal) + " ring tests";
        }
        return "FALSIFIED in " + failedRing + ": " + failedDetail;
    }
};

/**
 * @brief Multi-ring falsification gate
 * 
 * Tests a candidate identity lhs = rhs in multiple rings.
 * If it fails in ANY ring, the conjecture is false.
 */
class MultiRingGate {
public:
    struct Config {
        std::vector<int64_t> primes = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29};
        std::vector<unsigned> mod2kBits = {8, 16, 32};
        bool testRationals = true;
        bool verbose = false;
    };
    
    MultiRingGate() = default;
    explicit MultiRingGate(Config config) : config_(std::move(config)) {}
    
    /**
     * @brief Add a test point (variable assignments as integers)
     */
    void addTestPoint(std::unordered_map<std::string, int64_t> point) {
        testPoints_.push_back(std::move(point));
    }
    
    /**
     * @brief Add standard test points for given variable names
     */
    void addStandardTestPoints(const std::vector<std::string>& vars) {
        // Small values: often catch off-by-one
        std::vector<std::vector<int64_t>> smallVals = {
            {0, 1, 2},
            {1, 0, 1},
            {2, 3, 5},
            {1, 1, 1},
            {3, 7, 11},
            {5, 2, 8},
            {0, 0, 0},
            {1, -1, 1},
            {-2, 3, -5}
        };
        
        for (const auto& vals : smallVals) {
            std::unordered_map<std::string, int64_t> point;
            for (size_t i = 0; i < vars.size() && i < vals.size(); ++i) {
                point[vars[i]] = vals[i];
            }
            testPoints_.push_back(std::move(point));
        }
    }
    
    /**
     * @brief Test a conjecture: lhs =? rhs
     * 
     * Returns GateResult indicating whether the conjecture survived.
     */
    [[nodiscard]] GateResult test(
        const core::Term* lhs, 
        const core::Term* rhs
    ) const {
        size_t totalTests = 0;
        
        // Test in each F_p
        for (int64_t p : config_.primes) {
            FieldFp field(p);
            
            for (const auto& point : testPoints_) {
                RingAssignment<FieldFp::Value> env;
                for (const auto& [var, val] : point) {
                    env.vars[var] = field.normalize(val);
                }
                
                auto lhsVal = evalFp(lhs, field, env);
                auto rhsVal = evalFp(rhs, field, env);
                
                if (lhsVal && rhsVal) {
                    ++totalTests;
                    if (*lhsVal != *rhsVal) {
                        std::ostringstream detail;
                        detail << "lhs=" << *lhsVal << " rhs=" << *rhsVal;
                        detail << " at {";
                        for (const auto& [v, val] : point) {
                            detail << v << "=" << val << " ";
                        }
                        detail << "}";
                        return GateResult::fail(field.name(), detail.str(), 
                                               totalTests, totalTests - 1);
                    }
                }
            }
        }
        
        // Test in each Z/2^k
        for (unsigned k : config_.mod2kBits) {
            RingMod2k ring(k);
            
            for (const auto& point : testPoints_) {
                RingAssignment<RingMod2k::Value> env;
                for (const auto& [var, val] : point) {
                    env.vars[var] = ring.normalize(static_cast<uint64_t>(val));
                }
                
                auto lhsVal = evalMod2k(lhs, ring, env);
                auto rhsVal = evalMod2k(rhs, ring, env);
                
                if (lhsVal && rhsVal) {
                    ++totalTests;
                    if (*lhsVal != *rhsVal) {
                        std::ostringstream detail;
                        detail << "lhs=" << *lhsVal << " rhs=" << *rhsVal;
                        return GateResult::fail(ring.name(), detail.str(),
                                               totalTests, totalTests - 1);
                    }
                }
            }
        }
        
        // Test in Q
        if (config_.testRationals) {
            for (const auto& point : testPoints_) {
                RingAssignment<Rational> env;
                for (const auto& [var, val] : point) {
                    env.vars[var] = Rational(val);
                }
                
                auto lhsVal = evalQ(lhs, env);
                auto rhsVal = evalQ(rhs, env);
                
                if (lhsVal && rhsVal) {
                    ++totalTests;
                    if (*lhsVal != *rhsVal) {
                        std::ostringstream detail;
                        detail << "lhs=" << lhsVal->toString() 
                               << " rhs=" << rhsVal->toString();
                        return GateResult::fail("Q", detail.str(),
                                               totalTests, totalTests - 1);
                    }
                }
            }
        }
        
        return GateResult::pass(totalTests);
    }
    
    /**
     * @brief Get the number of test points
     */
    [[nodiscard]] size_t numTestPoints() const { return testPoints_.size(); }
    
private:
    Config config_;
    std::vector<std::unordered_map<std::string, int64_t>> testPoints_;
};

} // namespace domain
} // namespace autodiscover
