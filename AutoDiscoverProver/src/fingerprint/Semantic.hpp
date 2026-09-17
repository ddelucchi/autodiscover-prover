#ifndef AUTODISCOVER_FINGERPRINT_SEMANTIC_HPP
#define AUTODISCOVER_FINGERPRINT_SEMANTIC_HPP

/**
 * @file Semantic.hpp
 * @brief Semantic scalarizer using SCOUT evaluation
 * 
 * Mathematical Foundation:
 * ========================
 * 
 * The Semantic Scalarizer provides a mapping:
 * 
 *   Scout : T(,X)  Env   (or , , )
 * 
 * Different from the structural encoder, the semantic scalarizer:
 *   - Evaluates terms numerically at probe points
 *   - Captures semantic equivalence even across different syntactic forms
 *   - May have collisions (non-injective) but is semantically meaningful
 * 
 * Two-Channel Architecture:
 * -------------------------
 * 
 * The complete CanonScalar uses BOTH channels:
 * 
 *   CanonScalar(e) = (StructuralCode(NF(e)), SemanticSignature(e))
 * 
 * Where:
 *   - StructuralCode: Injective, collision-free, for exact deduplication
 *   - SemanticSignature: May collide, but enables similarity search
 * 
 * Probe Families:
 * ---------------
 * 
 * The SCOUT system evaluates terms at multiple probe points:
 * 
 *   P = {p, p, ..., p} where each p : Var  Value
 * 
 * Probe families include:
 *   - -probes: Evaluate at powers of 
 *   - Fibonacci probes: Evaluate at Fib(n)
 *   - Transcendental probes: Evaluate at , e, 2, etc.
 *   - Random probes: High-entropy evaluation points
 * 
 * Signature Vector:
 * -----------------
 * 
 * The semantic signature is a vector:
 * 
 *   sig(t) = (eval(t, p), eval(t, p), ..., eval(t, p))
 * 
 * For hypercomplex algebras, each component may itself be a vector.
 * 
 * Residual Energy:
 * ----------------
 * 
 * For equation t = t, the residual energy measures "how close":
 * 
 *   E(t, t) =  |eval(t, p) - eval(t, p)|
 * 
 * E(t, t) = 0 at ALL probes suggests t  t (but doesn't prove it).
 * 
 * @author AutoDiscover Prover
 * @date 2024
 */

#include "../core/Term.hpp"
#include "../core/Constants.hpp"
#include "../domain/Scout.hpp"
#include "../domain/Closure.hpp"
#include "../ring/ZPhi.hpp"
#include "Fingerprinter.hpp"

#include <vector>
#include <array>
#include <cmath>
#include <complex>
#include <optional>
#include <functional>
#include <unordered_map>
#include <limits>

namespace autodiscover {
namespace fingerprint {

// ===========================================================================
// FORWARD DECLARATIONS
// ===========================================================================

class SemanticScalarizer;
class ProbeFamily;

// ===========================================================================
// PROBE VALUE TYPES
// ===========================================================================

/**
 * @brief A numeric value for evaluation (supports hypercomplex)
 */
class ProbeValue {
public:
    using Complex = std::complex<double>;
    using Quaternion = std::array<double, 4>;
    using Octonion = std::array<double, 8>;
    
    enum class Type { REAL, COMPLEX, QUATERNION, OCTONION };
    
private:
    Type type_;
    std::array<double, 8> components_;  // Max size for octonion
    
public:
    // Constructors
    ProbeValue() : type_(Type::REAL), components_{0} {}
    explicit ProbeValue(double x) : type_(Type::REAL), components_{x} {}
    explicit ProbeValue(Complex c) : type_(Type::COMPLEX), components_{c.real(), c.imag()} {}
    explicit ProbeValue(Quaternion q) : type_(Type::QUATERNION) {
        std::copy(q.begin(), q.end(), components_.begin());
    }
    explicit ProbeValue(Octonion o) : type_(Type::OCTONION) {
        std::copy(o.begin(), o.end(), components_.begin());
    }
    explicit ProbeValue(ring::ZPhi z) : type_(Type::REAL) {
        // Convert Z[] element a + b to double using exact  constant.
        // Previous version only took signs  this uses actual BigInt values
        // truncated to double via string-based mantissa extraction (same as
        // StructuralCode::toDouble).
        auto toDouble = [](const ring::BigInt& n) -> double {
            if (n == ring::BigInt(0)) return 0.0;
            std::string s = n.toString();
            bool neg = false;
            if (!s.empty() && s[0] == '-') { neg = true; s = s.substr(1); }
            double val = 0.0;
            size_t digits = std::min(s.size(), size_t(15));
            for (size_t i = 0; i < digits; ++i) {
                val = val * 10.0 + (s[i] - '0');
            }
            if (s.size() > 15) {
                val *= std::pow(10.0, static_cast<double>(s.size() - 15));
            }
            return neg ? -val : val;
        };
        double a = toDouble(z.a());
        double b = toDouble(z.b());
        components_[0] = a + b * constants::PHI;
    }
    
    // Accessors
    [[nodiscard]] Type type() const { return type_; }
    [[nodiscard]] size_t dimension() const {
        switch (type_) {
            case Type::REAL: return 1;
            case Type::COMPLEX: return 2;
            case Type::QUATERNION: return 4;
            case Type::OCTONION: return 8;
        }
        return 1;
    }
    
    [[nodiscard]] double operator[](size_t i) const { return components_[i]; }
    [[nodiscard]] double real() const { return components_[0]; }
    [[nodiscard]] Complex asComplex() const { return {components_[0], components_[1]}; }
    [[nodiscard]] Quaternion asQuaternion() const {
        return {components_[0], components_[1], components_[2], components_[3]};
    }
    [[nodiscard]] Octonion asOctonion() const {
        Octonion o;
        std::copy(components_.begin(), components_.begin() + 8, o.begin());
        return o;
    }
    
    // Norm (squared)
    [[nodiscard]] double normSquared() const {
        double sum = 0.0;
        for (size_t i = 0; i < dimension(); ++i) {
            sum += components_[i] * components_[i];
        }
        return sum;
    }
    
    [[nodiscard]] double norm() const { return std::sqrt(normSquared()); }
    
    // Distance
    [[nodiscard]] double distanceTo(const ProbeValue& other) const {
        double sum = 0.0;
        size_t dim = std::max(dimension(), other.dimension());
        for (size_t i = 0; i < dim; ++i) {
            double diff = (i < dimension() ? components_[i] : 0.0) -
                         (i < other.dimension() ? other.components_[i] : 0.0);
            sum += diff * diff;
        }
        return std::sqrt(sum);
    }
    
    // -----------------------------------------------------------------------
    // CAYLEY-DICKSON ARITHMETIC  
    // Proper hypercomplex algebra:       
    // -----------------------------------------------------------------------
    
    /// Promote to the wider type of this and other
    [[nodiscard]] static Type promoteType(Type a, Type b) {
        return static_cast<Type>(std::max(static_cast<int>(a), static_cast<int>(b)));
    }

    /// Cayley-Dickson conjugate: conj(a,b) = (conj(a), -b)
    [[nodiscard]] ProbeValue conjugate() const {
        ProbeValue result;
        result.type_ = type_;
        size_t dim = dimension();
        cdConj(components_.data(), result.components_.data(), dim);
        return result;
    }
    
    /// Component-wise addition
    [[nodiscard]] ProbeValue operator+(const ProbeValue& rhs) const {
        ProbeValue result;
        result.type_ = promoteType(type_, rhs.type_);
        size_t dim = result.dimension();
        for (size_t i = 0; i < dim; ++i) {
            result.components_[i] = 
                (i < dimension() ? components_[i] : 0.0) +
                (i < rhs.dimension() ? rhs.components_[i] : 0.0);
        }
        return result;
    }
    
    /// Component-wise subtraction
    [[nodiscard]] ProbeValue operator-(const ProbeValue& rhs) const {
        ProbeValue result;
        result.type_ = promoteType(type_, rhs.type_);
        size_t dim = result.dimension();
        for (size_t i = 0; i < dim; ++i) {
            result.components_[i] = 
                (i < dimension() ? components_[i] : 0.0) -
                (i < rhs.dimension() ? rhs.components_[i] : 0.0);
        }
        return result;
    }
    
    /// Unary negation
    [[nodiscard]] ProbeValue operator-() const {
        ProbeValue result;
        result.type_ = type_;
        for (size_t i = 0; i < dimension(); ++i) {
            result.components_[i] = -components_[i];
        }
        return result;
    }
    
    /// Cayley-Dickson multiplication: (a,b)(c,d) = (ac - db, da + bc)
    [[nodiscard]] ProbeValue operator*(const ProbeValue& rhs) const {
        ProbeValue result;
        result.type_ = promoteType(type_, rhs.type_);
        size_t dim = result.dimension();
        
        // Embed both into the wider dimension
        std::array<double, 8> a{}, b{};
        for (size_t i = 0; i < dimension(); ++i) a[i] = components_[i];
        for (size_t i = 0; i < rhs.dimension(); ++i) b[i] = rhs.components_[i];
        
        cdMul(a.data(), b.data(), result.components_.data(), dim);
        return result;
    }
    
    /// Scalar multiplication
    [[nodiscard]] ProbeValue operator*(double s) const {
        ProbeValue result;
        result.type_ = type_;
        for (size_t i = 0; i < dimension(); ++i) {
            result.components_[i] = components_[i] * s;
        }
        return result;
    }
    
    /// Multiplicative inverse: x^{-1} = conj(x) / |x|
    [[nodiscard]] ProbeValue inverse() const {
        double ns = normSquared();
        if (ns < 1e-30) return ProbeValue(0.0); // degenerate
        ProbeValue c = conjugate();
        ProbeValue result;
        result.type_ = type_;
        for (size_t i = 0; i < dimension(); ++i) {
            result.components_[i] = c.components_[i] / ns;
        }
        return result;
    }
    
    /// Division: a / b = a  b^{-1}
    [[nodiscard]] ProbeValue operator/(const ProbeValue& rhs) const {
        return (*this) * rhs.inverse();
    }

private:
    // -- Cayley-Dickson helpers (recursive on dimension) --------------------
    
    /// Recursive conjugation: conj(a,b) = (conj(a), -b),  conj(real) = real
    static void cdConj(const double* in, double* out, size_t dim) {
        if (dim == 1) { out[0] = in[0]; return; }
        size_t half = dim / 2;
        cdConj(in, out, half);                            // conj(a)
        for (size_t i = 0; i < half; ++i)
            out[half + i] = -in[half + i];                // -b
    }
    
    /// Recursive Cayley-Dickson multiplication
    /// (a,b)(c,d) = (ac  conj(d)b,  da + bconj(c))
    static void cdMul(const double* a, const double* b, double* out, size_t dim) {
        if (dim == 1) { out[0] = a[0] * b[0]; return; }
        
        size_t half = dim / 2;
        const double* a1 = a;           // "a" part
        const double* a2 = a + half;    // "b" part
        const double* b1 = b;           // "c" part
        const double* b2 = b + half;    // "d" part
        
        // Temporaries (max half = 4 for octonion)
        double conjD[4]{}, conjC[4]{};
        double t1[4]{}, t2[4]{}, t3[4]{}, t4[4]{};
        
        cdConj(b2, conjD, half);   // conj(d)
        cdConj(b1, conjC, half);   // conj(c)
        
        // out_lo = ac  conj(d)b
        cdMul(a1, b1, t1, half);         // ac
        cdMul(conjD, a2, t2, half);      // conj(d)b
        for (size_t i = 0; i < half; ++i)
            out[i] = t1[i] - t2[i];
        
        // out_hi = da + bconj(c)
        cdMul(b2, a1, t3, half);         // da
        cdMul(a2, conjC, t4, half);      // bconj(c)
        for (size_t i = 0; i < half; ++i)
            out[half + i] = t3[i] + t4[i];
    }
};

// ===========================================================================
// PROBE POINT
// ===========================================================================

/**
 * @brief A single probe point: variable assignments
 */
class ProbePoint {
public:
    using Assignment = std::unordered_map<std::string, ProbeValue>;
    
private:
    Assignment assignments_;
    std::string name_;
    
public:
    ProbePoint() = default;
    explicit ProbePoint(std::string name) : name_(std::move(name)) {}
    
    void set(const std::string& var, ProbeValue value) {
        assignments_[var] = std::move(value);
    }
    
    [[nodiscard]] std::optional<ProbeValue> get(const std::string& var) const {
        auto it = assignments_.find(var);
        if (it != assignments_.end()) return it->second;
        return std::nullopt;
    }
    
    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] const Assignment& assignments() const { return assignments_; }
    [[nodiscard]] size_t size() const { return assignments_.size(); }
};

// ===========================================================================
// PROBE FAMILY  
// ===========================================================================

/**
 * @brief A family of probe points for evaluation
 */
class ProbeFamily {
private:
    std::string name_;
    std::vector<ProbePoint> probes_;
    
public:
    ProbeFamily() : name_("empty") {}
    explicit ProbeFamily(std::string name) : name_(std::move(name)) {}
    
    void addProbe(ProbePoint probe) { probes_.push_back(std::move(probe)); }
    
    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] const std::vector<ProbePoint>& probes() const { return probes_; }
    [[nodiscard]] size_t size() const { return probes_.size(); }
    
    // -----------------------------------------------------------------------
    // PREDEFINED FAMILIES
    // -----------------------------------------------------------------------
    
    /**
     * @brief Golden ratio probe family
     * 
     * Evaluates at ^k for k = -5, -4, ..., 5.
     * Each variable gets a DISTINCT power to avoid collisions:
     *   var_j gets ^{k + jstride}  (stride = numProbes to avoid overlap)
     * Uses hardcoded constants (not std::pow) for cross-platform determinism.
     */
    static ProbeFamily goldenFamily(const std::vector<std::string>& vars) {
        ProbeFamily family("golden");
        
        // Precomputed ^k values for k = -10, ..., +15
        // Extended range to accommodate per-variable offsets
        //  = 1.6180339887498948482...
        static const double phiPow[] = {
            0.008130170654054559,   // ^{-10}
            0.013155617496424835,   // ^{-9}
            0.021286788150479394,   // ^{-8}
            0.034442405646904235,   // ^{-7}
            0.055728153797383630,   // ^{-6}
            0.09016994374947424,    // ^{-5}
            0.14589803375031546,    // ^{-4}
            0.23606797749978970,    // ^{-3}
            0.38196601125010515,    // ^{-2}
            0.61803398874989485,    // ^{-1}
            1.0,                    // ^{0}   [index 10]
            1.6180339887498949,     // ^{1}
            2.6180339887498949,     // ^{2}
            4.2360679774997898,     // ^{3}
            6.8541019662496847,     // ^{4}
            11.090169943749474,     // ^{5}
            17.944271909999159,     // ^{6}
            29.034441853748633,     // ^{7}
            46.978713763747791,     // ^{8}
            76.013155617496424,     // ^{9}
            122.99186938124422,     // ^{10}
            199.00502499874064,     // ^{11}
            321.99689437998486,     // ^{12}
            521.00191937872550,     // ^{13}
            842.99881375871036,     // ^{14}
            1364.0007331374359      // ^{15}
        };
        static constexpr int phiPowOffset = 10; // index for ^0
        static constexpr int numProbes = 11;     // k from -5 to +5
        
        for (int k = -5; k <= 5; ++k) {
            ProbePoint probe("phi^" + std::to_string(k));
            int varIdx = 0;
            for (const auto& var : vars) {
                // Each variable j gets ^{k + jnumProbes}
                // This guarantees no two variables share the same value
                // within a probe, and no two probes share variable assignments
                int exponent = k + varIdx * numProbes;
                int arrayIdx = exponent + phiPowOffset;
                // Clamp to table bounds
                if (arrayIdx >= 0 && arrayIdx < static_cast<int>(sizeof(phiPow)/sizeof(phiPow[0]))) {
                    probe.set(var, ProbeValue(phiPow[arrayIdx]));
                } else {
                    // Fall back to runtime computation for out-of-range
                    probe.set(var, ProbeValue(std::pow(constants::PHI, static_cast<double>(exponent))));
                }
                ++varIdx;
            }
            family.addProbe(std::move(probe));
        }
        
        return family;
    }
    
    /**
     * @brief Fibonacci probe family
     * 
     * Evaluates at F(n) for n = 1, 2, ..., 12.
     * Each variable gets a distinct Fibonacci value to avoid collisions:
     *   var_j gets F(n + j12)
     */
    static ProbeFamily fibonacciFamily(const std::vector<std::string>& vars) {
        ProbeFamily family("fibonacci");
        
        // Extended Fibonacci values: F(1)..F(48) to support up to 3 variables
        static const double fibs[] = {
            1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144,       // F(1)..F(12)
            233, 377, 610, 987, 1597, 2584, 4181, 6765,        // F(13)..F(20)
            10946, 17711, 28657, 46368,                         // F(21)..F(24)
            75025, 121393, 196418, 317811, 514229, 832040,      // F(25)..F(30)
            1346269, 2178309, 3524578, 5702887, 9227465,        // F(31)..F(35)
            14930352, 24157817, 39088169, 63245986, 102334155,  // F(36)..F(40)
            165580141, 267914296, 433494437, 701408733,         // F(41)..F(44)
            1134903170, 1836311903, 2971215073, 4807526976      // F(45)..F(48)
        };
        static constexpr size_t stride = 12;
        
        for (size_t i = 0; i < 12; ++i) {
            ProbePoint probe("fib" + std::to_string(i + 1));
            int varIdx = 0;
            for (const auto& var : vars) {
                size_t idx = i + static_cast<size_t>(varIdx) * stride;
                if (idx < sizeof(fibs)/sizeof(fibs[0])) {
                    probe.set(var, ProbeValue(fibs[idx]));
                } else {
                    probe.set(var, ProbeValue(fibs[i])); // fallback
                }
                ++varIdx;
            }
            family.addProbe(std::move(probe));
        }
        
        return family;
    }
    
    /**
     * @brief Transcendental probe family
     * 
     * Evaluates at , e, 2, 3, 5, ln(2)
     */
    /**
     * @brief Transcendental probe family
     *
     * Each probe assigns DISTINCT values per variable:
     *   var_j gets baseValue * (j+1) + small_offset_j
     * This prevents x=y=z=const collapse that destroys discrimination.
     */
    static ProbeFamily transcendentalFamily(const std::vector<std::string>& vars) {
        ProbeFamily family("transcendental");
        
        std::vector<std::pair<std::string, double>> bases = {
            {"pi", 3.14159265358979},
            {"e", 2.71828182845905},
            {"sqrt2", 1.41421356237310},
            {"sqrt3", 1.73205080756888},
            {"sqrt5", 2.23606797749979},
            {"ln2", 0.69314718055995}
        };
        
        // Small irrational offsets to separate variables further
        static const double offsets[] = {
            0.0, 0.57721566490153, 1.20205690315959,  // , (3)
            0.91596559417722, 0.26149721284764, 1.78107241799020
        };
        
        for (const auto& [name, baseVal] : bases) {
            ProbePoint probe(name);
            int varIdx = 0;
            for (const auto& var : vars) {
                double offset = offsets[varIdx % (sizeof(offsets)/sizeof(offsets[0]))];
                probe.set(var, ProbeValue(baseVal * (varIdx + 1) + offset));
                ++varIdx;
            }
            family.addProbe(std::move(probe));
        }
        
        return family;
    }
    
    /**
     * @brief Combined standard probe family
     */
    static ProbeFamily standardFamily(const std::vector<std::string>& vars) {
        ProbeFamily family("standard");
        
        // Add golden probes
        auto golden = goldenFamily(vars);
        for (const auto& p : golden.probes()) {
            family.addProbe(p);
        }
        
        // Add transcendental probes
        auto trans = transcendentalFamily(vars);
        for (const auto& p : trans.probes()) {
            family.addProbe(p);
        }
        
        return family;
    }
    
    /**
     * @brief Complex probe family (for complex-valued terms)
     */
    /**
     * @brief Complex probe family  
     *
     * Each variable gets a DISTINCT complex value at each probe:
     *   var_j gets base * e^{ij2/7} (rotation in the complex plane)
     * This prevents diagonal-only evaluation.
     */
    static ProbeFamily complexFamily(const std::vector<std::string>& vars) {
        ProbeFamily family("complex");
        
        std::vector<std::pair<std::string, std::complex<double>>> bases = {
            {"i", {0, 1}},
            {"1+i", {1, 1}},
            {"phi+i", {1.618, 1}},
            {"e^(i*pi/4)", {0.707, 0.707}},
            {"e^(i*pi/3)", {0.5, 0.866}}
        };
        
        // Rotation angle per variable index: 2/7 (irrational fraction of )
        static constexpr double ROT = 2.0 * 3.14159265358979 / 7.0;
        
        for (const auto& [name, baseVal] : bases) {
            ProbePoint probe(name);
            int varIdx = 0;
            for (const auto& var : vars) {
                // Rotate base value by varIdx * ROT and scale
                double angle = varIdx * ROT;
                std::complex<double> rotated(
                    baseVal.real() * std::cos(angle) - baseVal.imag() * std::sin(angle),
                    baseVal.real() * std::sin(angle) + baseVal.imag() * std::cos(angle)
                );
                // Scale by (varIdx+1) to also separate magnitudes
                rotated *= static_cast<double>(varIdx + 1);
                probe.set(var, ProbeValue(rotated));
                ++varIdx;
            }
            family.addProbe(std::move(probe));
        }
        
        return family;
    }
};

// ===========================================================================
// SEMANTIC SIGNATURE
// ===========================================================================

/**
 * @brief A semantic signature: vector of evaluation results
 */
class SemanticSignature {
private:
    std::vector<ProbeValue> values_;
    std::string familyName_;
    bool valid_ = true;
    std::string errorMsg_;
    
public:
    SemanticSignature() = default;
    
    explicit SemanticSignature(std::vector<ProbeValue> values, std::string family = "")
        : values_(std::move(values)), familyName_(std::move(family)) {}
    
    static SemanticSignature invalid(std::string error) {
        SemanticSignature sig;
        sig.valid_ = false;
        sig.errorMsg_ = std::move(error);
        return sig;
    }
    
    // Accessors
    [[nodiscard]] bool isValid() const { return valid_; }
    [[nodiscard]] const std::string& error() const { return errorMsg_; }
    [[nodiscard]] const std::vector<ProbeValue>& values() const { return values_; }
    [[nodiscard]] size_t dimension() const { return values_.size(); }
    [[nodiscard]] const std::string& familyName() const { return familyName_; }
    
    [[nodiscard]] const ProbeValue& operator[](size_t i) const { return values_[i]; }
    
    // -----------------------------------------------------------------------
    // SIMILARITY METRICS
    // -----------------------------------------------------------------------
    
    /**
     * @brief L2 distance to another signature
     */
    [[nodiscard]] double distanceTo(const SemanticSignature& other) const {
        if (!valid_ || !other.valid_) return std::numeric_limits<double>::infinity();
        if (values_.size() != other.values_.size()) {
            return std::numeric_limits<double>::infinity();
        }
        
        double sum = 0.0;
        for (size_t i = 0; i < values_.size(); ++i) {
            double d = values_[i].distanceTo(other.values_[i]);
            sum += d * d;
        }
        return std::sqrt(sum);
    }
    
    /**
     * @brief Cosine similarity to another signature
     */
    [[nodiscard]] double cosineSimilarity(const SemanticSignature& other) const {
        if (!valid_ || !other.valid_) return 0.0;
        if (values_.size() != other.values_.size()) return 0.0;
        
        double dot = 0.0, normA = 0.0, normB = 0.0;
        for (size_t i = 0; i < values_.size(); ++i) {
            double a = values_[i].real();
            double b = other.values_[i].real();
            dot += a * b;
            normA += a * a;
            normB += b * b;
        }
        
        if (normA < 1e-15 || normB < 1e-15) return 0.0;
        return dot / (std::sqrt(normA) * std::sqrt(normB));
    }
    
    /**
     * @brief Check if approximately equal (all probes match within tolerance)
     */
    [[nodiscard]] bool approximatelyEquals(const SemanticSignature& other, 
                                           double tol = 1e-10) const {
        return distanceTo(other) < tol;
    }
    
    // -----------------------------------------------------------------------
    // HASH FOR INDEXING
    // -----------------------------------------------------------------------
    
    /**
     * @brief Quantized hash for approximate matching
     * 
     * Quantizes each coordinate to nearest multiple of `quantum` and 
     * hashes the quantized values.  Values within quantum/2 of zero
     * are flushed to zero to prevent 0 hash instability.
     */
    [[nodiscard]] size_t quantizedHash(double quantum = 0.01) const {
        size_t h = 0;
        for (const auto& v : values_) {
            double r = v.real();
            // Flush near-zero to exact zero for stability
            if (std::abs(r) < quantum * 0.5) r = 0.0;
            int64_t quantized = static_cast<int64_t>(std::floor(r / quantum + 0.5));
            h ^= std::hash<int64_t>{}(quantized) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
    
    /**
     * @brief Convert to fingerprint (for storage)
     */
    [[nodiscard]] Fingerprint toFingerprint() const {
        // Hash the signature values
        Hash256Builder builder;
        for (const auto& v : values_) {
            // Add each component as bytes
            for (size_t i = 0; i < v.dimension(); ++i) {
                double comp = v[i];
                builder.update(reinterpret_cast<const uint8_t*>(&comp), sizeof(double));
            }
        }
        // Create fingerprint with hash, zero exact value, and size
        return Fingerprint(builder.finalize(), ring::ZPhi(), values_.size());
    }
};

// ===========================================================================
// TERM EVALUATOR
// ===========================================================================

/**
 * @brief Evaluates terms at probe points
 */
class TermEvaluator {
public:
    /**
     * @brief Evaluate a term at a probe point
     */
    [[nodiscard]] ProbeValue evaluate(
        const core::Term& term, 
        const ProbePoint& probe
    ) const {
        return evaluateImpl(&term, probe);
    }
    
    [[nodiscard]] ProbeValue evaluate(
        const std::shared_ptr<core::Term>& term,
        const ProbePoint& probe
    ) const {
        return evaluate(*term, probe);
    }
    
private:
    ProbeValue evaluateImpl(const core::Term* term, const ProbePoint& probe) const {
        if (!term) return ProbeValue(0.0);
        
        switch (term->kind()) {
            case core::TermKind::Scalar:
                return ProbeValue(term->scalarValue());
                
            case core::TermKind::Variable: {
                auto val = probe.get(term->symbol());
                return val.value_or(ProbeValue(0.0));
            }
                
            case core::TermKind::Constant: {
                // Known constants
                if (term->symbol() == "0") return ProbeValue(0.0);
                if (term->symbol() == "1") return ProbeValue(1.0);
                if (term->symbol() == "phi") return ProbeValue(1.6180339887498949);
                if (term->symbol() == "pi") return ProbeValue(3.14159265358979);
                if (term->symbol() == "e") return ProbeValue(2.71828182845905);
                return ProbeValue(0.0);
            }
                
            case core::TermKind::Phi:
                return ProbeValue(1.6180339887498949);
                
            case core::TermKind::PhiBar:
                return ProbeValue(1.0 - 1.6180339887498949); //   0.618
                
            case core::TermKind::J:
                return ProbeValue(std::complex<double>(0, 1));
                
            case core::TermKind::Application:
                return evaluateApplication(term, probe);
                
            case core::TermKind::Pair: {
                // Cayley-Dickson pair (a, b)  evaluate as complex-like: a + Jb
                if (term->children().size() >= 2) {
                    auto a = evaluateImpl(term->children()[0], probe);
                    auto b = evaluateImpl(term->children()[1], probe);
                    return ProbeValue(std::complex<double>(a.real(), b.real()));
                }
                return ProbeValue(0.0);
            }
                
            default:
                return ProbeValue(0.0);
        }
    }
    
    ProbeValue evaluateApplication(const core::Term* term, const ProbePoint& probe) const {
        const auto& sym = term->symbol();
        const auto& children = term->children();
        
        // Evaluate children
        std::vector<ProbeValue> args;
        for (const core::Term* child : children) {
            args.push_back(evaluateImpl(child, probe));
        }
        
        // Apply operation using full Cayley-Dickson arithmetic
        // (no longer discards imaginary/hypercomplex components)
        
        if ((sym == "add" || sym == "+") && args.size() >= 2) {
            ProbeValue sum = args[0];
            for (size_t i = 1; i < args.size(); ++i) sum = sum + args[i];
            return sum;
        }
        
        if ((sym == "sub" || sym == "-") && args.size() >= 2) {
            return args[0] - args[1];
        }
        
        if ((sym == "mul" || sym == "*") && args.size() >= 2) {
            ProbeValue prod = args[0];
            for (size_t i = 1; i < args.size(); ++i) prod = prod * args[i];
            return prod;
        }
        
        if ((sym == "div" || sym == "/") && args.size() >= 2) {
            if (args[1].normSquared() < 1e-30) return ProbeValue(0.0);
            return args[0] / args[1];
        }
        
        if (sym == "neg" && args.size() >= 1) {
            return -args[0];
        }
        
        if ((sym == "inv") && args.size() >= 1) {
            return args[0].inverse();
        }
        
        if (sym == "conj" && args.size() >= 1) {
            return args[0].conjugate();
        }
        
        if (sym == "norm" && args.size() >= 1) {
            return ProbeValue(args[0].norm());
        }
        
        // These functions only make sense for real arguments
        if (sym == "pow" && args.size() >= 2) {
            return ProbeValue(std::pow(args[0].real(), args[1].real()));
        }
        
        if (sym == "sqrt" && args.size() >= 1) {
            return ProbeValue(std::sqrt(std::abs(args[0].real())));
        }
        
        if (sym == "sin" && args.size() >= 1) {
            return ProbeValue(std::sin(args[0].real()));
        }
        
        if (sym == "cos" && args.size() >= 1) {
            return ProbeValue(std::cos(args[0].real()));
        }
        
        if (sym == "exp" && args.size() >= 1) {
            return ProbeValue(std::exp(args[0].real()));
        }
        
        if (sym == "log" && args.size() >= 1) {
            return ProbeValue(std::log(std::abs(args[0].real()) + 1e-15));
        }
        
        // --- SCOUT / domain operators ---
        
        // Scal(x) = Re(x) = (x + conj(x)) / 2
        if ((sym == "Scal" || sym == "scal") && args.size() >= 1) {
            return ProbeValue(args[0].real());
        }
        
        // N(x) = norm(x)  (alias)
        if (sym == "N" && args.size() >= 1) {
            return ProbeValue(args[0].norm());
        }
        
        // Align(U, q) = Scal(conj(U) * q)
        if (sym == "Align" && args.size() >= 2) {
            auto conjU = args[0].conjugate();
            auto prod = conjU * args[1];
            return ProbeValue(prod.real());
        }
        
        // PhaseTransport(a, b) = a * b  (semantic interp)
        if (sym == "PhaseTransport" && args.size() >= 2) {
            return args[0] * args[1];
        }
        
        // FibStep(z) = 1 + 1/z
        if (sym == "FibStep" && args.size() >= 1) {
            if (args[0].normSquared() < 1e-30) return ProbeValue(std::numeric_limits<double>::quiet_NaN());
            return ProbeValue(1.0) + ProbeValue(1.0) / args[0];
        }
        
        // Vec(x) = x - Scal(x)
        if (sym == "Vec" && args.size() >= 1) {
            return args[0] - ProbeValue(args[0].real());
        }
        
        // GOD / Enc  purely structural, no numeric semantics; return NaN
        // (these should not appear in probe-evaluable subterms)
        if (sym == "GOD" || sym == "Enc") {
            return ProbeValue(std::numeric_limits<double>::quiet_NaN());
        }
        
        // --- PDE / calculus operators ---
        // Derivative operators cannot be evaluated on raw scalar probes
        // without automatic differentiation.  Return NaN (invalid) so the
        // falsifier correctly treats terms containing derivatives as
        // unevaluable, rather than silently collapsing them to identity.
        if (sym == "Dt" || sym == "Dx" || sym == "Dy" || sym == "Dz" ||
            sym == "Grad" || sym == "Div" || sym == "Lap" || sym == "Curl") {
            return ProbeValue(std::numeric_limits<double>::quiet_NaN());
        }
        
        // Unknown function  NaN (invalid evaluation, never identity/zero).
        // Returning args[0] or 0 would create ghost invariants that
        // pass falsification and pollute the discovery pipeline.
        return ProbeValue(std::numeric_limits<double>::quiet_NaN());
    }
};

// ===========================================================================
// SEMANTIC SCALARIZER
// ===========================================================================

/**
 * @brief Main semantic scalarizer using SCOUT evaluation
 */
class SemanticScalarizer {
private:
    TermEvaluator evaluator_;
    ProbeFamily defaultFamily_;
    
public:
    SemanticScalarizer() {
        // Default family with common variable names
        defaultFamily_ = ProbeFamily::standardFamily({"x", "y", "z", "w", "a", "b", "c"});
    }
    
    explicit SemanticScalarizer(ProbeFamily family) 
        : defaultFamily_(std::move(family)) {}
    
    // -----------------------------------------------------------------------
    // MAIN API
    // -----------------------------------------------------------------------
    
    /**
     * @brief Compute semantic signature of a term
     */
    [[nodiscard]] SemanticSignature signature(const core::Term& term) const {
        return signature(term, defaultFamily_);
    }
    
    [[nodiscard]] SemanticSignature signature(
        const core::Term& term,
        const ProbeFamily& family
    ) const {
        std::vector<ProbeValue> values;
        values.reserve(family.size());
        
        for (const auto& probe : family.probes()) {
            try {
                values.push_back(evaluator_.evaluate(term, probe));
            } catch (...) {
                return SemanticSignature::invalid("Evaluation failed at probe: " + probe.name());
            }
        }
        
        return SemanticSignature(std::move(values), family.name());
    }
    
    [[nodiscard]] SemanticSignature signature(
        const std::shared_ptr<core::Term>& term
    ) const {
        return signature(*term);
    }
    
    [[nodiscard]] SemanticSignature signature(
        const std::shared_ptr<core::Term>& term,
        const ProbeFamily& family
    ) const {
        return signature(*term, family);
    }
    
    /**
     * @brief Compute residual energy for an equation
     * 
     * E(lhs, rhs) =  |eval(lhs, p) - eval(rhs, p)|
     */
    [[nodiscard]] double residualEnergy(
        const core::Term& lhs,
        const core::Term& rhs
    ) const {
        auto sigL = signature(lhs);
        auto sigR = signature(rhs);
        
        if (!sigL.isValid() || !sigR.isValid()) {
            return std::numeric_limits<double>::infinity();
        }
        
        double d = sigL.distanceTo(sigR);
        return d * d;
    }
    
    [[nodiscard]] double residualEnergy(
        const std::shared_ptr<core::Term>& lhs,
        const std::shared_ptr<core::Term>& rhs
    ) const {
        return residualEnergy(*lhs, *rhs);
    }
    
    /**
     * @brief Heuristic semantic gate: checks if two terms evaluate to
     *        similar values at all probe points.
     * 
     * IMPORTANT: This is a NECESSARY but NOT SUFFICIENT condition for
     * equivalence.  It only checks that |residual| < tol at the probe
     * family.  Two genuinely non-equivalent terms with matching probe
     * values would pass this gate (false positive).  Always follow up
     * with a structural proof.
     * 
     * @deprecated Use passSemanticGate() instead (clearer naming).
     */
    [[nodiscard]] bool areSemanticallySimilar(
        const core::Term& t1,
        const core::Term& t2,
        double tol = 1e-10
    ) const {
        return passSemanticGate(t1, t2, tol);
    }
    
    /**
     * @brief Heuristic semantic gate: returns true if residual energy < tol
     * 
     * Use this as a FILTER before attempting expensive structural proof.
     * If passSemanticGate returns false, the terms are provably inequivalent.
     * If it returns true, they MIGHT be equivalent (requires proof).
     */
    [[nodiscard]] bool passSemanticGate(
        const core::Term& t1,
        const core::Term& t2,
        double tol = 1e-10
    ) const {
        return residualEnergy(t1, t2) < tol * tol;
    }
    
    // -----------------------------------------------------------------------
    // CONFIGURATION
    // -----------------------------------------------------------------------
    
    void setDefaultFamily(ProbeFamily family) {
        defaultFamily_ = std::move(family);
    }
    
    [[nodiscard]] const ProbeFamily& defaultFamily() const {
        return defaultFamily_;
    }
};

// ===========================================================================
// GLOBAL SCALARIZER
// ===========================================================================

/**
 * @brief Global semantic scalarizer instance
 */
inline SemanticScalarizer& globalSemanticScalarizer() {
    static SemanticScalarizer scalarizer;
    return scalarizer;
}

/**
 * @brief Quick semantic signature computation
 */
inline SemanticSignature semSig(const core::Term& t) {
    return globalSemanticScalarizer().signature(t);
}

inline SemanticSignature semSig(const std::shared_ptr<core::Term>& t) {
    return globalSemanticScalarizer().signature(t);
}

/**
 * @brief Quick residual energy computation
 */
inline double residual(const core::Term& lhs, const core::Term& rhs) {
    return globalSemanticScalarizer().residualEnergy(lhs, rhs);
}

} // namespace fingerprint
} // namespace autodiscover

#endif // AUTODISCOVER_FINGERPRINT_SEMANTIC_HPP
