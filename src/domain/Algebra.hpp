/**
 * @file Algebra.hpp
 * @brief Cayley-Dickson algebra axioms, numeric tower, and SCOUT algebra operations
 * 
 * =============================================================================
 * Mathematical Foundation
 * =============================================================================
 * 
 * Cayley-Dickson Construction:
 *   A = 
 *   A_{n+1} = A  A with:
 *     (a,b) + (c,d) = (a+c, b+d)
 *     (a,b)(c,d) = (ac - d*b, da + bc*)
 *     (a,b)* = (a*, -b)
 * 
 * Properties by level:
 *   A (): ordered field, all algebraic properties
 *   A (): commutative, associative, division algebra
 *   A (): associative, division algebra (non-commutative)
 *   A (): alternative, division algebra (non-associative)
 *   A (): power-associative (has zero divisors)
 * 
 * Key Elements:
 *   J = (0,1) - canonical imaginary unit at each level
 *   J = -1 (at levels  3)
 * 
 * =============================================================================
 * Cayley Circle Map
 * =============================================================================
 * 
 *   _J(r) = (1 + Jr)(1 - Jr)^{-1} = exp(2Jarctan(r))
 * 
 * Properties:
 *   - Maps   {}  S (unit circle in J-plane)
 *   - _J(r)_J(r^{-1}) = -1 (inversion-pair identity)
 *   - _J(0) = 1, _J() = -1
 *   - _J(1) = J, _J(-1) = -J
 * 
 * Explicit form:
 *   _J(r) = (1-r)/(1+r) + J(2r)/(1+r)
 *          = cos(2arctan(r)) + Jsin(2arctan(r))
 * 
 * =============================================================================
 * Phase Transport
 * =============================================================================
 * 
 *    = 1
 *   _{k+1} = _k  _J(r_k)
 * 
 * Accumulated phase:
 *   _k = _{i=0}^{k-1} _J(r_i) = exp(2J  _{i=0}^{k-1} arctan(r_i))
 * 
 * =============================================================================
 * Scalar Extraction and Alignment
 * =============================================================================
 * 
 *   Scal(q) = (q + q*)  
 *   Align_U(q) = Scal(U*  q)
 * 
 * Key property: If q = a then Align(, a) = Scal(a)
 * 
 */

#ifndef AUTODISCOVER_DOMAIN_ALGEBRA_HPP
#define AUTODISCOVER_DOMAIN_ALGEBRA_HPP

#include "../core/Term.hpp"
#include "../core/Type.hpp"
#include "../core/Constants.hpp"
#include "../logic/Equation.hpp"
#include <vector>
#include <string>
#include <array>
#include <cmath>
#include <complex>
#include <stdexcept>

namespace autodiscover {
namespace domain {

using core::Term;
using core::TermFactory;
using core::Sort;
using core::CDLevel;
using logic::Equation;

// =============================================================================
// MATHEMATICAL CONSTANTS  (canonical source: core/Constants.hpp)
// =============================================================================

// Re-export from core namespace for convenience within domain code
using constants::PHI;
using constants::PHI_INV;
using constants::SQRT5;
using constants::PI;
using constants::E;
using constants::SCOUT_RHO_MAX;

// =============================================================================
// CAYLEY-DICKSON NUMERIC TOWER
// =============================================================================

/**
 * @brief Generic Cayley-Dickson element template
 * 
 * A_n has dimension 2^n, represented as a pair of A_{n-1} elements.
 * 
 * @tparam N The Cayley-Dickson level (0=Real, 1=Complex, 2=Quaternion, 3=Octonion, ...)
 */
template<unsigned N>
class CayleyDickson {
public:
    using Lower = CayleyDickson<N-1>;
    static constexpr unsigned dimension = 1u << N;
    
    Lower first;   // First component
    Lower second;  // Second component (imaginary part)
    
    // Default constructor: zero element
    CayleyDickson() : first(), second() {}
    
    // Construct from two lower-level elements
    CayleyDickson(const Lower& a, const Lower& b) : first(a), second(b) {}
    
    // Construct from scalar (embed real)
    explicit CayleyDickson(double r) : first(r), second() {}
    
    // Conjugation: (a,b)* = (a*, -b)
    [[nodiscard]] CayleyDickson conj() const {
        return CayleyDickson(first.conj(), -second);
    }
    
    // Negation
    [[nodiscard]] CayleyDickson operator-() const {
        return CayleyDickson(-first, -second);
    }
    
    // Addition: (a,b) + (c,d) = (a+c, b+d)
    [[nodiscard]] CayleyDickson operator+(const CayleyDickson& o) const {
        return CayleyDickson(first + o.first, second + o.second);
    }
    
    // Subtraction
    [[nodiscard]] CayleyDickson operator-(const CayleyDickson& o) const {
        return CayleyDickson(first - o.first, second - o.second);
    }
    
    // Cayley-Dickson multiplication: (a,b)(c,d) = (ac - d*b, da + bc*)
    [[nodiscard]] CayleyDickson operator*(const CayleyDickson& o) const {
        // (a,b)(c,d) = (ac - d*b, da + bc*)
        return CayleyDickson(
            first * o.first - o.second.conj() * second,
            o.second * first + second * o.first.conj()
        );
    }
    
    // Scalar multiplication
    [[nodiscard]] CayleyDickson operator*(double s) const {
        return CayleyDickson(first * s, second * s);
    }
    
    // Squared norm: |x| = x  x* (always real)
    [[nodiscard]] double normSq() const {
        return first.normSq() + second.normSq();
    }
    
    // Norm: |x|
    [[nodiscard]] double norm() const {
        return std::sqrt(normSq());
    }
    
    // Scalar part: Scal(q) = (q + q*)
    [[nodiscard]] double scalarPart() const {
        return first.scalarPart();
    }
    
    // Vector part: Vec(q) = q - Scal(q)
    [[nodiscard]] CayleyDickson vectorPart() const {
        CayleyDickson result = *this;
        result.first = result.first.vectorPart();
        return result;
    }
    
    // Inverse: x^{-1} = x* / |x| (for division algebras, N  3)
    [[nodiscard]] CayleyDickson inv() const {
        double n2 = normSq();
        if (n2 < 1e-30) {
            throw std::domain_error("Cannot invert zero element");
        }
        return conj() * (1.0 / n2);
    }
    
    // Normalize to unit element
    [[nodiscard]] CayleyDickson normalized() const {
        double n = norm();
        if (n < 1e-30) return CayleyDickson(1.0);
        return *this * (1.0 / n);
    }
    
    // Check if approximately zero
    [[nodiscard]] bool isZero(double tol = 1e-12) const {
        return normSq() < tol * tol;
    }
    
    // Check if approximately unit
    [[nodiscard]] bool isUnit(double tol = 1e-10) const {
        return std::abs(normSq() - 1.0) < tol;
    }
    
    // Access as flat array of doubles
    [[nodiscard]] std::array<double, dimension> toArray() const {
        std::array<double, dimension> result;
        auto lowFirst = first.toArray();
        auto lowSecond = second.toArray();
        for (unsigned i = 0; i < dimension/2; ++i) {
            result[i] = lowFirst[i];
            result[i + dimension/2] = lowSecond[i];
        }
        return result;
    }
};

// Base case: A_0 = 
template<>
class CayleyDickson<0> {
public:
    static constexpr unsigned dimension = 1;
    double value;
    
    CayleyDickson() : value(0.0) {}
    explicit CayleyDickson(double r) : value(r) {}
    
    [[nodiscard]] CayleyDickson conj() const { return *this; }
    [[nodiscard]] CayleyDickson operator-() const { return CayleyDickson(-value); }
    [[nodiscard]] CayleyDickson operator+(const CayleyDickson& o) const { return CayleyDickson(value + o.value); }
    [[nodiscard]] CayleyDickson operator-(const CayleyDickson& o) const { return CayleyDickson(value - o.value); }
    [[nodiscard]] CayleyDickson operator*(const CayleyDickson& o) const { return CayleyDickson(value * o.value); }
    [[nodiscard]] CayleyDickson operator*(double s) const { return CayleyDickson(value * s); }
    [[nodiscard]] double normSq() const { return value * value; }
    [[nodiscard]] double norm() const { return std::abs(value); }
    [[nodiscard]] double scalarPart() const { return value; }
    [[nodiscard]] CayleyDickson vectorPart() const { return CayleyDickson(0.0); }
    [[nodiscard]] CayleyDickson inv() const {
        if (std::abs(value) < 1e-30) throw std::domain_error("Cannot invert zero");
        return CayleyDickson(1.0 / value);
    }
    [[nodiscard]] CayleyDickson normalized() const {
        if (std::abs(value) < 1e-30) return CayleyDickson(1.0);
        return CayleyDickson(value > 0 ? 1.0 : -1.0);
    }
    [[nodiscard]] bool isZero(double tol = 1e-12) const { return std::abs(value) < tol; }
    [[nodiscard]] bool isUnit(double tol = 1e-10) const { return std::abs(std::abs(value) - 1.0) < tol; }
    [[nodiscard]] std::array<double, 1> toArray() const { return {value}; }
};

// Type aliases for common algebras
using Real = CayleyDickson<0>;       //  (dimension 1)
using Complex = CayleyDickson<1>;    //  (dimension 2)
using Quaternion = CayleyDickson<2>; //  (dimension 4)
using Octonion = CayleyDickson<3>;   //  (dimension 8)
using Sedenion = CayleyDickson<4>;   //  (dimension 16)

// =============================================================================
// J-PLANE AND CAYLEY MAP
// =============================================================================

/**
 * @brief J-plane element: z = a + Jb where J = -1
 * 
 * This is the canonical 2D commutative subalgebra of any Cayley-Dickson algebra.
 * Isomorphic to , but distinguished as the "phase transport plane".
 */
struct JPlane {
    double re;  // Real part
    double im;  // J-imaginary part
    
    JPlane() : re(0.0), im(0.0) {}
    JPlane(double r) : re(r), im(0.0) {}
    JPlane(double r, double i) : re(r), im(i) {}
    
    // From complex (for convenience)
    explicit JPlane(const std::complex<double>& c) : re(c.real()), im(c.imag()) {}
    
    // Convert to complex
    [[nodiscard]] std::complex<double> toComplex() const { return {re, im}; }
    
    // Conjugation: (a + Jb)* = a - Jb
    [[nodiscard]] JPlane conj() const { return JPlane(re, -im); }
    
    // Negation
    [[nodiscard]] JPlane operator-() const { return JPlane(-re, -im); }
    
    // Addition
    [[nodiscard]] JPlane operator+(const JPlane& o) const { return JPlane(re + o.re, im + o.im); }
    
    // Subtraction
    [[nodiscard]] JPlane operator-(const JPlane& o) const { return JPlane(re - o.re, im - o.im); }
    
    // Multiplication: (a+Jb)(c+Jd) = (ac-bd) + J(ad+bc)
    [[nodiscard]] JPlane operator*(const JPlane& o) const {
        return JPlane(re * o.re - im * o.im, re * o.im + im * o.re);
    }
    
    // Scalar multiplication
    [[nodiscard]] JPlane operator*(double s) const { return JPlane(re * s, im * s); }
    
    // Division
    [[nodiscard]] JPlane operator/(const JPlane& o) const {
        double d = o.re * o.re + o.im * o.im;
        if (d < 1e-30) throw std::domain_error("Division by zero in J-plane");
        return JPlane((re * o.re + im * o.im) / d, (im * o.re - re * o.im) / d);
    }
    
    // Squared norm
    [[nodiscard]] double normSq() const { return re * re + im * im; }
    
    // Norm
    [[nodiscard]] double norm() const { return std::sqrt(normSq()); }
    
    // Argument (angle):  such that z = |z|exp(J)
    [[nodiscard]] double arg() const { return std::atan2(im, re); }
    
    // Inverse
    [[nodiscard]] JPlane inv() const {
        double n2 = normSq();
        if (n2 < 1e-30) throw std::domain_error("Cannot invert zero");
        return conj() * (1.0 / n2);
    }
    
    // Normalize to unit
    [[nodiscard]] JPlane normalized() const {
        double n = norm();
        if (n < 1e-30) return JPlane(1.0, 0.0);
        return *this * (1.0 / n);
    }
    
    // Check if unit
    [[nodiscard]] bool isUnit(double tol = 1e-10) const {
        return std::abs(normSq() - 1.0) < tol;
    }
    
    // The canonical imaginary unit J
    static JPlane J() { return JPlane(0.0, 1.0); }
    
    // Unit element
    static JPlane one() { return JPlane(1.0, 0.0); }
    
    // Zero element
    static JPlane zero() { return JPlane(0.0, 0.0); }
};

/**
 * @brief Exponential in J-plane: exp(J) = cos() + Jsin()
 */
[[nodiscard]] inline JPlane expJ(double theta) {
    return JPlane(std::cos(theta), std::sin(theta));
}

/**
 * @brief Natural logarithm in J-plane: log(z) = log|z| + Jarg(z)
 */
[[nodiscard]] inline JPlane logJ(const JPlane& z) {
    return JPlane(std::log(z.norm()), z.arg());
}

/**
 * @brief Power in J-plane: z^n = exp(nlog(z))
 */
[[nodiscard]] inline JPlane powJ(const JPlane& z, double n) {
    double r = z.norm();
    double theta = z.arg();
    double rn = std::pow(r, n);
    return JPlane(rn * std::cos(n * theta), rn * std::sin(n * theta));
}

/**
 * @brief Cayley Circle Map: _J(r) = (1 + Jr)(1 - Jr)^{-1}
 * 
 * Maps   {}  S (unit circle in J-plane)
 * 
 * Properties:
 *   - _J(r) = exp(2Jarctan(r))
 *   - _J(r)_J(1/r) = -1 (inversion identity)
 *   - _J(0) = 1, _J() = -1
 *   - _J(1) = J, _J(-1) = -J
 * 
 * @param r Real ratio (can be any real number including )
 * @return Unit element in J-plane
 */
[[nodiscard]] inline JPlane cayleyMap(double r) {
    if (!std::isfinite(r)) {
        // _J() = -1
        return JPlane(-1.0, 0.0);
    }
    
    double r2 = r * r;
    double denom = 1.0 + r2;
    
    // _J(r) = (1-r)/(1+r) + J(2r)/(1+r)
    return JPlane((1.0 - r2) / denom, (2.0 * r) / denom);
}

/**
 * @brief Cayley map via exponential form: _J(r) = exp(2Jarctan(r))
 * 
 * Mathematically identical to cayleyMap(), but computed via exponential.
 */
[[nodiscard]] inline JPlane cayleyMapExp(double r) {
    if (!std::isfinite(r)) {
        return JPlane(-1.0, 0.0);
    }
    double theta = 2.0 * std::atan(r);
    return expJ(theta);
}

/**
 * @brief Inverse Cayley map: given unit z, find r such that _J(r) = z
 * 
 * r = tan(arg(z) / 2)
 */
[[nodiscard]] inline double cayleyMapInverse(const JPlane& z) {
    double theta = z.arg();
    return std::tan(theta / 2.0);
}

/**
 * @brief Verify the inversion-pair identity: _J(r)_J(1/r) = -1
 */
[[nodiscard]] inline bool verifyCayleyInversion(double r, double tol = 1e-10) {
    if (std::abs(r) < 1e-15 || !std::isfinite(r)) return true; // Degenerate cases
    JPlane c1 = cayleyMap(r);
    JPlane c2 = cayleyMap(1.0 / r);
    JPlane product = c1 * c2;
    return std::abs(product.re + 1.0) < tol && std::abs(product.im) < tol;
}

// =============================================================================
// PHASE TRANSPORT
// =============================================================================

/**
 * @brief Phase transport accumulator
 * 
 * Computes _k = _{i=0}^{k-1} _J(r_i) = exp(2J  _{i=0}^{k-1} arctan(r_i))
 */
class PhaseTransport {
public:
    PhaseTransport() : phase_(JPlane::one()), accumulatedAngle_(0.0), stepCount_(0) {}
    
    /**
     * @brief Transport one step: _{k+1} = _k  _J(r_k)
     * 
     * @param r The ratio determining this step's phase rotation
     */
    void step(double r) {
        JPlane rotation = cayleyMap(r);
        phase_ = phase_ * rotation;
        accumulatedAngle_ += 2.0 * std::atan(r);
        stepCount_++;
        
        // Periodic renormalization to maintain unit constraint
        if (stepCount_ % 100 == 0) {
            phase_ = phase_.normalized();
        }
    }
    
    /**
     * @brief Current accumulated phase _k
     */
    [[nodiscard]] const JPlane& phase() const { return phase_; }
    
    /**
     * @brief Total accumulated angle (mod 2 for winding number)
     */
    [[nodiscard]] double accumulatedAngle() const { return accumulatedAngle_; }
    
    /**
     * @brief Winding number K = floor((accumulated angle) / 2)
     */
    [[nodiscard]] int windingNumber() const {
        return static_cast<int>(std::floor(accumulatedAngle_ / (2.0 * PI)));
    }
    
    /**
     * @brief Number of transport steps taken
     */
    [[nodiscard]] size_t stepCount() const { return stepCount_; }
    
    /**
     * @brief Reset to initial state
     */
    void reset() {
        phase_ = JPlane::one();
        accumulatedAngle_ = 0.0;
        stepCount_ = 0;
    }
    
    /**
     * @brief Alignment operator: Align_(q) = Scal(*  q)
     * 
     * For J-plane element q, returns Re(conj()  q)
     */
    [[nodiscard]] double align(const JPlane& q) const {
        JPlane aligned = phase_.conj() * q;
        return aligned.re; // Scalar part
    }
    
private:
    JPlane phase_;           // Current accumulated phase
    double accumulatedAngle_; // Total angle (unbounded, tracks winding)
    size_t stepCount_;       // Number of steps taken
};

// =============================================================================
// PHASE ARROW
// =============================================================================

/**
 * @brief Phase arrow: _J^[] = (-1)^K  exp(JZ_SCOUT)
 * 
 * This is the combined discrete/continuous phase tag that captures
 * both the topological winding (K) and the scalar invariant (Z_SCOUT).
 */
struct PhaseArrow {
    int K;           // Winding number / topological degree
    double Z_SCOUT;  // Scalar invariant from SCOUT
    double alpha0;   // Phase modulation constant
    
    PhaseArrow() : K(0), Z_SCOUT(0.0), alpha0(1.0) {}
    PhaseArrow(int k, double z, double a = 1.0) : K(k), Z_SCOUT(z), alpha0(a) {}
    
    /**
     * @brief Compute the phase arrow as J-plane unit
     * 
     * _J^ = (-1)^K  exp(JZ_SCOUT)
     */
    [[nodiscard]] JPlane compute() const {
        JPlane expPart = expJ(alpha0 * Z_SCOUT);
        if (K % 2 == 0) {
            return expPart;
        } else {
            return -expPart;
        }
    }
    
    /**
     * @brief Check if two phase arrows are equivalent (mod branch)
     * 
     * Equal when: (Z - Z) + (K - K)  2
     */
    [[nodiscard]] bool equivalent(const PhaseArrow& other, double tol = 1e-8) const {
        double angle1 = alpha0 * Z_SCOUT + PI * K;
        double angle2 = other.alpha0 * other.Z_SCOUT + PI * other.K;
        double diff = angle1 - angle2;
        // Check if diff is a multiple of 2
        double normalized = std::fmod(diff, 2.0 * PI);
        if (normalized < 0) normalized += 2.0 * PI;
        return normalized < tol || std::abs(normalized - 2.0 * PI) < tol;
    }
};

/**
 * @brief Cayley-Dickson algebra axiom generator
 */
class CayleyDicksonAxioms {
public:
    explicit CayleyDicksonAxioms(TermFactory& factory) : factory_(factory) {}
    
    /**
     * @brief Generate all axioms up to given CD level
     */
    [[nodiscard]] std::vector<std::unique_ptr<Equation>> generateAxioms(CDLevel maxLevel) {
        std::vector<std::unique_ptr<Equation>> axioms;
        
        // Universal axioms (all levels)
        generateUniversalAxioms(axioms);
        
        // Level-specific axioms
        for (uint8_t n = 0; n <= maxLevel.level; ++n) {
            generateLevelAxioms(CDLevel(n), axioms);
        }
        
        return axioms;
    }

private:
    TermFactory& factory_;
    
    // Fresh variable counter
    uint32_t varCounter_ = 0;
    
    const Term* freshVar(Sort sort = Sort::Generic) {
        return factory_.variable("x" + std::to_string(varCounter_++), sort);
    }
    
    void generateUniversalAxioms(std::vector<std::unique_ptr<Equation>>& axioms) {
        // Create variables
        auto x = freshVar();
        auto y = freshVar();
        auto z = freshVar();
        
        // === Additive Group ===
        
        // Commutativity: x + y = y + x
        axioms.push_back(std::make_unique<Equation>(
            factory_.add(x, y),
            factory_.add(y, x)
        ));
        
        // Associativity: (x + y) + z = x + (y + z)
        axioms.push_back(std::make_unique<Equation>(
            factory_.add(factory_.add(x, y), z),
            factory_.add(x, factory_.add(y, z))
        ));
        
        // Identity: x + 0 = x
        auto zero = factory_.scalar(0.0);
        axioms.push_back(std::make_unique<Equation>(
            factory_.add(x, zero),
            x
        ));
        
        // Inverse: x + (-x) = 0
        axioms.push_back(std::make_unique<Equation>(
            factory_.add(x, factory_.neg(x)),
            zero
        ));
        
        // === Multiplicative axioms ===
        
        // Identity: x * 1 = x
        auto one = factory_.scalar(1.0);
        axioms.push_back(std::make_unique<Equation>(
            factory_.mul(x, one),
            x
        ));
        axioms.push_back(std::make_unique<Equation>(
            factory_.mul(one, x),
            x
        ));
        
        // Zero: x * 0 = 0
        axioms.push_back(std::make_unique<Equation>(
            factory_.mul(x, zero),
            zero
        ));
        
        // Distributivity: x * (y + z) = x*y + x*z
        axioms.push_back(std::make_unique<Equation>(
            factory_.mul(x, factory_.add(y, z)),
            factory_.add(factory_.mul(x, y), factory_.mul(x, z))
        ));
        
        // === Conjugation ===
        
        // Involution: (x*)* = x
        axioms.push_back(std::make_unique<Equation>(
            factory_.conj(factory_.conj(x)),
            x
        ));
        
        // Anti-homomorphism: (xy)* = y* x*
        axioms.push_back(std::make_unique<Equation>(
            factory_.conj(factory_.mul(x, y)),
            factory_.mul(factory_.conj(y), factory_.conj(x))
        ));
        
        // Sum: (x + y)* = x* + y*
        axioms.push_back(std::make_unique<Equation>(
            factory_.conj(factory_.add(x, y)),
            factory_.add(factory_.conj(x), factory_.conj(y))
        ));
        
        // === Norm ===
        
        // |x| = x * x*
        axioms.push_back(std::make_unique<Equation>(
            factory_.norm(x),
            factory_.mul(x, factory_.conj(x))
        ));
        
        // === Negation ===
        
        // Double negation: --x = x
        axioms.push_back(std::make_unique<Equation>(
            factory_.neg(factory_.neg(x)),
            x
        ));
        
        // Negation distributes: -(x + y) = -x + -y
        axioms.push_back(std::make_unique<Equation>(
            factory_.neg(factory_.add(x, y)),
            factory_.add(factory_.neg(x), factory_.neg(y))
        ));
    }
    
    void generateLevelAxioms(CDLevel level, std::vector<std::unique_ptr<Equation>>& axioms) {
        auto x = freshVar();
        auto y = freshVar();
        auto z = freshVar();
        
        // === Level 0 (Reals): Commutativity ===
        if (level.level <= CDLevel::COMPLEX) {
            // xy = yx (commutative up to complex)
            axioms.push_back(std::make_unique<Equation>(
                factory_.mul(x, y),
                factory_.mul(y, x)
            ));
        }
        
        // === Up to Quaternions: Associativity ===
        if (level.level <= CDLevel::QUATERNION) {
            // (xy)z = x(yz)
            axioms.push_back(std::make_unique<Equation>(
                factory_.mul(factory_.mul(x, y), z),
                factory_.mul(x, factory_.mul(y, z))
            ));
        }
        
        // === Up to Octonions: Alternative property ===
        if (level.level == CDLevel::OCTONION) {
            // Left alternative: x(xy) = (xx)y
            axioms.push_back(std::make_unique<Equation>(
                factory_.mul(x, factory_.mul(x, y)),
                factory_.mul(factory_.mul(x, x), y)
            ));
            
            // Right alternative: (yx)x = y(xx)
            axioms.push_back(std::make_unique<Equation>(
                factory_.mul(factory_.mul(y, x), x),
                factory_.mul(y, factory_.mul(x, x))
            ));
            
            // Flexible: x(yx) = (xy)x
            axioms.push_back(std::make_unique<Equation>(
                factory_.mul(x, factory_.mul(y, x)),
                factory_.mul(factory_.mul(x, y), x)
            ));
        }
        
        // === Division algebra (up to Octonions) ===
        if (level.level <= CDLevel::OCTONION) {
            // For non-zero x: x * x = 1
            auto one = factory_.scalar(1.0);
            axioms.push_back(std::make_unique<Equation>(
                factory_.mul(x, factory_.inv(x)),
                one
            ));
        }
        
        // === Cayley-Dickson multiplication ===
        if (level.level >= CDLevel::COMPLEX) {
            // (a,b)(c,d) = (ac - d*b, da + bc*)
            auto a = freshVar();
            auto b = freshVar();
            auto c = freshVar();
            auto d = freshVar();
            
            auto lhs = factory_.mul(factory_.pair(a, b), factory_.pair(c, d));
            auto rhs_first = factory_.add(
                factory_.mul(a, c),
                factory_.neg(factory_.mul(factory_.conj(d), b))
            );
            auto rhs_second = factory_.add(
                factory_.mul(d, a),
                factory_.mul(b, factory_.conj(c))
            );
            auto rhs = factory_.pair(rhs_first, rhs_second);
            
            axioms.push_back(std::make_unique<Equation>(lhs, rhs));
        }
        
        // === J unit ===
        if (level.level >= CDLevel::COMPLEX) {
            // J = (0, 1)
            auto J = factory_.J();
            auto zero = factory_.scalar(0.0);
            auto one = factory_.scalar(1.0);
            
            axioms.push_back(std::make_unique<Equation>(
                J,
                factory_.pair(zero, one)
            ));
            
            // J=-1 is NOT seeded  it is a THEOREM derivable from
            // J=(0,1) + CD multiplication rule. The engine DISCOVERS it
            // by evaluating mul(J,J) and finding it matches neg(one).
        }
        
        // === Conjugation for pairs ===
        if (level.level >= CDLevel::COMPLEX) {
            auto a = freshVar();
            auto b = freshVar();
            
            // (a,b)* = (a*, -b)
            axioms.push_back(std::make_unique<Equation>(
                factory_.conj(factory_.pair(a, b)),
                factory_.pair(factory_.conj(a), factory_.neg(b))
            ));
        }
    }
};

/**
 * @brief Golden ratio () axioms for Z[] ring
 * 
 * Z[] = {a + b : a,b  } with  =  + 1
 * 
 * Canonical form: (a + b) where coefficients are minimal
 */
class GoldenRatioAxioms {
public:
    explicit GoldenRatioAxioms(TermFactory& factory) : factory_(factory) {}
    
    /**
     * @brief Generate axioms for  ring
     */
    [[nodiscard]] std::vector<std::unique_ptr<Equation>> generateAxioms() {
        std::vector<std::unique_ptr<Equation>> axioms;
        
        auto phi = factory_.phi();
        auto one = factory_.scalar(1.0);
        
        // =================================================================
        //  =  + 1  THE SOLE DEFINING AXIOM of the golden ratio ring.
        //
        // This is the ONLY preset equation for . ALL consequences 
        // 1/=-1, =2+1, =3+2, Fibonacci recurrence, Lucas numbers,
        // Galois norms, etc.  are DISCOVERED by the engine through
        // systematic evaluation and bucketing. ZERO BIAS.
        // =================================================================
        axioms.push_back(std::make_unique<Equation>(
            factory_.mul(phi, phi),
            factory_.add(phi, one)
        ));
        
        return axioms;
    }
    
    /**
     * @brief Generate Zeckendorf representation axioms
     * 
     * Every positive integer has a unique representation as a sum of
     * non-consecutive Fibonacci numbers: n = _{kS} F_k where k,k+1  S
     */
    [[nodiscard]] std::vector<std::unique_ptr<Equation>> generateZeckendorfAxioms() {
        std::vector<std::unique_ptr<Equation>> axioms;
        
        // Carry rule: F_k + F_{k+1} = F_{k+2}
        // This is used for normalization in CarryRewriter
        
        return axioms;
    }

private:
    TermFactory& factory_;
};

/**
 * @brief Cayley Map axioms
 * 
 * Symbolic representation of _J(r) = (1 + Jr)(1 - Jr)^{-1}
 */
class CayleyMapAxioms {
public:
    explicit CayleyMapAxioms(TermFactory& factory) : factory_(factory) {}
    
    /**
     * @brief Generate Cayley map axioms
     */
    [[nodiscard]] std::vector<std::unique_ptr<Equation>> generateAxioms() {
        std::vector<std::unique_ptr<Equation>> axioms;
        
        auto r = factory_.variable("r");
        auto J = factory_.J();
        auto one = factory_.scalar(1.0);
        auto zero = factory_.scalar(0.0);
        
        // === Cayley map definition ===
        // C_J(r) = (1 + Jr)(1 - Jr)^{-1}
        
        // Numerator: 1 + Jr
        auto cayleyNum = [&](const Term* ratio) {
            return factory_.add(one, factory_.mul(J, ratio));
        };
        
        // Denominator: 1 - Jr
        auto cayleyDen = [&](const Term* ratio) {
            return factory_.add(one, factory_.neg(factory_.mul(J, ratio)));
        };
        
        // === Key properties (symbolic) ===
        
        // C_J(0) = 1
        // cayleyMap(zero) reduces to (1 + 0)/(1 - 0) = 1
        axioms.push_back(std::make_unique<Equation>(
            factory_.mul(cayleyNum(zero), factory_.inv(cayleyDen(zero))),
            one
        ));
        
        // C_J(1) = J (rotates to imaginary unit)
        // (1 + J)(1 - J)^{-1} = (1 + J)(1 + J)/(1-J) = (1+J)/2 = J
        auto c1 = factory_.mul(cayleyNum(one), factory_.inv(cayleyDen(one)));
        axioms.push_back(std::make_unique<Equation>(
            c1,
            J
        ));
        
        // === Inversion pair identity ===
        // C_J(r)  C_J(1/r) = -1 for r  0, 
        
        // === Unit constraint ===
        // |C_J(r)| = 1 for all r (always on unit circle)
        axioms.push_back(std::make_unique<Equation>(
            factory_.norm(factory_.mul(cayleyNum(r), factory_.inv(cayleyDen(r)))),
            one
        ));
        
        // === Phase transport composition ===
        // C_J(r)  C_J(s) = C_J((r+s)/(1-rs)) (angle addition on circle)
        
        return axioms;
    }

private:
    TermFactory& factory_;
};

/**
 * @brief SCOUT alignment axioms
 */
class AlignmentAxioms {
public:
    explicit AlignmentAxioms(TermFactory& factory) : factory_(factory) {}
    
    /**
     * @brief Generate alignment axioms
     */
    [[nodiscard]] std::vector<std::unique_ptr<Equation>> generateAxioms() {
        std::vector<std::unique_ptr<Equation>> axioms;
        
        auto x = factory_.variable("x");
        auto U = factory_.variable("U"); // Unit frame
        auto one = factory_.scalar(1.0);
        auto two = factory_.scalar(2.0);
        
        // === Scalar part ===
        // Scal(q) = (q + q*)
        axioms.push_back(std::make_unique<Equation>(
            factory_.scalarPart(x),
            factory_.mul(factory_.add(x, factory_.conj(x)), factory_.inv(two))
        ));
        
        // Scal(r) = r for real r
        axioms.push_back(std::make_unique<Equation>(
            factory_.scalarPart(one),
            one
        ));
        
        // === Alignment operator ===
        // Align_U(q) = Scal(U*  q)
        axioms.push_back(std::make_unique<Equation>(
            factory_.align(U, x),
            factory_.scalarPart(factory_.mul(factory_.conj(U), x))
        ));
        
        // Align_1(q) = Scal(q) (unit frame = 1)
        axioms.push_back(std::make_unique<Equation>(
            factory_.align(one, x),
            factory_.scalarPart(x)
        ));
        
        // === Key property ===
        // If q = Ua where a is scalar, then Align_U(q) = a
        // Align_U(Ua) = Scal(U*Ua) = Scal(|U|a) = a (when |U|=1)
        
        // === Phase transport connection ===
        // Align_(a) = Scal(*a) = Scal(a) = a for  unit, a real
        
        return axioms;
    }

private:
    TermFactory& factory_;
};

/**
 * @brief Phase transport axioms
 */
class PhaseTransportAxioms {
public:
    explicit PhaseTransportAxioms(TermFactory& factory) : factory_(factory) {}
    
    /**
     * @brief Generate phase transport axioms
     */
    [[nodiscard]] std::vector<std::unique_ptr<Equation>> generateAxioms() {
        std::vector<std::unique_ptr<Equation>> axioms;
        
        auto phi  = factory_.variable("phi_transport");
        auto r    = factory_.variable("r_step");
        auto J    = factory_.J();
        auto one  = factory_.scalar(1.0);
        auto zero = factory_.scalar(0.0);

        // Axiom PT1: INITIAL CONDITION   = 1
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("PhaseInit", {}),
            one
        ));

        // Axiom PT2: TRANSPORT STEP  _{k+1} = _k  C_J(r_k)
        // The Cayley map C_J(r) = (1 + Jr)(1 - Jr)
        auto Jr = factory_.mul(J, r);
        auto cayleyNum = factory_.add(one, Jr);
        auto cayleyDen = factory_.add(one, factory_.neg(Jr));
        auto cayleyR   = factory_.mul(cayleyNum, factory_.inv(cayleyDen));

        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("PhaseStep", {phi, r}),
            factory_.mul(phi, cayleyR)
        ));

        // Axiom PT3: PHASE IS UNITARY  |_k| = 1 for all k
        // Since  = 1 and |C_J(r)| = 1, by induction |_k| = 1
        axioms.push_back(std::make_unique<Equation>(
            factory_.norm(factory_.apply("PhaseStep", {phi, r})),
            factory_.norm(phi)
        ));

        // Axiom PT4: ACCUMULATED ANGLE
        // The angle of _k = 2_{i=0}^{k-1} arctan(r_i)
        // Single step angle contribution: 2arctan(r)
        // This connects to the holonomy axioms in ScoutModule
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("PhaseAngle", {factory_.apply("PhaseStep", {phi, r})}),
            factory_.add(
                factory_.apply("PhaseAngle", {phi}),
                factory_.mul(factory_.scalar(2.0), factory_.apply("Arctan", {r}))
            )
        ));

        // Axiom PT5: INITIAL ANGLE  angle() = angle(1) = 0
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("PhaseAngle", {one}),
            zero
        ));

        // Axiom PT6: WINDING NUMBER  K = angle/(2)
        // WindingNumber() = Floor(PhaseAngle() / (2))
        axioms.push_back(std::make_unique<Equation>(
            factory_.apply("WindingNumber", {phi}),
            factory_.apply("Floor", {
                factory_.mul(
                    factory_.apply("PhaseAngle", {phi}),
                    factory_.inv(factory_.mul(
                        factory_.scalar(2.0),
                        factory_.apply("Pi", {})
                    ))
                )
            })
        ));
        
        return axioms;
    }

private:
    TermFactory& factory_;
};

/**
 * @brief Numeric evaluator for Cayley-Dickson algebra
 * 
 * Provides numeric evaluation of symbolic terms in A_n
 */
template<unsigned N>
class CayleyDicksonEvaluator {
public:
    using CD = CayleyDickson<N>;
    
    /**
     * @brief Evaluate term to numeric value
     */
    [[nodiscard]] CD evaluate(const Term* term) const {
        if (!term) return CD();
        
        switch (term->kind()) {
            case core::TermKind::Scalar:
                // Scalar embeds as (r, 0, 0, ...)
                return CD(term->scalarValue());
                
            case core::TermKind::Constant:
                // Named constants
                if (term->symbol() == "phi") {
                    return CD(PHI);
                } else if (term->symbol() == "phi_bar") {
                    return CD(1.0 - PHI);
                } else if (term->symbol() == "one") {
                    return CD(1.0);
                } else if (term->symbol() == "zero") {
                    return CD(0.0);
                }
                // Unknown constant - return zero
                return CD();
                
            case core::TermKind::Phi:
                return CD(PHI);
                
            case core::TermKind::PhiBar:
                return CD(1.0 - PHI);
                
            case core::TermKind::J:
                // J = (0, 1) at each level
                if constexpr (N >= 1) {
                    return CD(typename CD::Lower(), typename CD::Lower(1.0));
                } else {
                    // At level 0 (reals), J doesn't exist - return 0
                    return CD();
                }
                
            case core::TermKind::Application:
                return evaluateApplication(term);
                
            case core::TermKind::Pair:
                // (a, b) construction
                if constexpr (N >= 1) {
                    const auto& args = term->children();
                    if (args.size() >= 2) {
                        CayleyDicksonEvaluator<N-1> lowerEval;
                        auto a = lowerEval.evaluate(args[0]);
                        auto b = lowerEval.evaluate(args[1]);
                        return CD(a, b);
                    }
                }
                return CD();
                
            default:
                // Variable or unknown - return zero (cannot evaluate symbolically)
                return CD();
        }
    }
    
private:
    [[nodiscard]] CD evaluateApplication(const Term* term) const {
        const std::string& op = term->symbol();
        const auto& args = term->children();

        // === Binary operations ===
        if ((op == "add" || op == "+") && args.size() >= 2) {
            return evaluate(args[0]) + evaluate(args[1]);
        }
        if ((op == "mul" || op == "*") && args.size() >= 2) {
            return evaluate(args[0]) * evaluate(args[1]);
        }
        if ((op == "sub" || op == "-") && args.size() >= 2) {
            return evaluate(args[0]) - evaluate(args[1]);
        }

        // === Unary operations ===
        if (op == "neg" && args.size() >= 1) {
            return -evaluate(args[0]);
        }
        if (op == "conj" && args.size() >= 1) {
            return evaluate(args[0]).conj();
        }
        if (op == "inv" && args.size() >= 1) {
            return evaluate(args[0]).inv();
        }
        if (op == "norm" && args.size() >= 1) {
            // norm(x) = |x|^2 = x * conj(x), embedded as scalar in CD<N>
            auto val = evaluate(args[0]);
            return CD(val.normSq());
        }
        if ((op == "Scal" || op == "scalarPart") && args.size() >= 1) {
            auto val = evaluate(args[0]);
            return CD(val.scalarPart());
        }
        if ((op == "Vec" || op == "vectorPart") && args.size() >= 1) {
            auto val = evaluate(args[0]);
            return val - CD(val.scalarPart());
        }

        return CD(); // Unknown operation
    }
};

// Explicit instantiation for Quaternion evaluation
template class CayleyDicksonEvaluator<2>;

/**
 * @brief Combined algebra module
 */
class AlgebraModule {
public:
    explicit AlgebraModule(TermFactory& factory) 
        : factory_(factory)
        , cdAxioms_(factory)
        , phiAxioms_(factory)
        , cayleyAxioms_(factory)
        , alignAxioms_(factory)
        , phaseAxioms_(factory) {}
    
    /**
     * @brief Generate all algebra axioms
     */
    [[nodiscard]] std::vector<std::unique_ptr<Equation>> 
    generateAllAxioms(CDLevel maxLevel = CDLevel(CDLevel::OCTONION)) {
        std::vector<std::unique_ptr<Equation>> result;
        
        // Cayley-Dickson construction axioms (ring DEFINITIONS)
        auto cd = cdAxioms_.generateAxioms(maxLevel);
        for (auto& eq : cd) {
            result.push_back(std::move(eq));
        }
        
        // Golden ratio axiom: phi^2 = phi + 1 (ONLY)
        auto phi = phiAxioms_.generateAxioms();
        for (auto& eq : phi) {
            result.push_back(std::move(eq));
        }
        
        // NOTE: CayleyMapAxioms, AlignmentAxioms, PhaseTransportAxioms
        // are NOT included here. They contain DERIVED PROPERTIES
        // (C_J(0)=1, Align_q(q)=|q|^2, etc.) that should be DISCOVERED.
        // SCOUT module provides the DEFINITIONS only (via seedAxioms).
        
        return result;
    }
    
    /**
     * @brief Get the TermFactory
     */
    [[nodiscard]] TermFactory& factory() { return factory_; }
    [[nodiscard]] const TermFactory& factory() const { return factory_; }
    
    /**
     * @brief Create Cayley map term: C_J(r)
     */
    [[nodiscard]] const Term* cayleyMapTerm(const Term* r) {
        auto J = factory_.J();
        auto one = factory_.scalar(1.0);
        auto Jr = factory_.mul(J, r);
        auto num = factory_.add(one, Jr);
        auto den = factory_.add(one, factory_.neg(Jr));
        return factory_.mul(num, factory_.inv(den));
    }
    
    /**
     * @brief Create phase transport step term
     * 
     * _{n+1} = _n  C_J(r_n)
     */
    [[nodiscard]] const Term* phaseTransportStep(const Term* phi_n, const Term* r_n) {
        return factory_.mul(phi_n, cayleyMapTerm(r_n));
    }
    
    /**
     * @brief Numeric Cayley map evaluation
     */
    [[nodiscard]] static JPlane evaluateCayleyMap(double r) {
        return cayleyMap(r);
    }
    
    /**
     * @brief Verify Cayley map properties numerically
     */
    [[nodiscard]] static bool verifyCayleyMapProperties(double tol = 1e-10) {
        bool allOk = true;
        
        // C_J(0) = 1
        auto c0 = cayleyMap(0.0);
        allOk &= (std::abs(c0.re - 1.0) < tol && std::abs(c0.im) < tol);
        
        // C_J(1) = J
        auto c1 = cayleyMap(1.0);
        allOk &= (std::abs(c1.re) < tol && std::abs(c1.im - 1.0) < tol);
        
        // C_J(-1) = -J
        auto cm1 = cayleyMap(-1.0);
        allOk &= (std::abs(cm1.re) < tol && std::abs(cm1.im + 1.0) < tol);
        
        // Inversion identity for various r
        for (double r : {0.5, 2.0, 0.1, 10.0, PHI, PHI_INV}) {
            allOk &= verifyCayleyInversion(r, tol);
        }
        
        return allOk;
    }

private:
    TermFactory& factory_;
    CayleyDicksonAxioms cdAxioms_;
    GoldenRatioAxioms phiAxioms_;
    CayleyMapAxioms cayleyAxioms_;
    AlignmentAxioms alignAxioms_;
    PhaseTransportAxioms phaseAxioms_;
};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Compute Fibonacci number F_n (double-precision, small n)
 * 
 * O(n) iterative computation. Suitable for n < ~93 (uint64_t overflow).
 * 
 * @note For arbitrary-precision Fibonacci using fast-doubling O(log n),
 *       see ring::fibonacci(const BigInt&) in ring/ZPhi.hpp.
 *       For int64-based Fibonacci, see domain::Fibonacci in Closure.hpp.
 */
[[nodiscard]] inline uint64_t fibonacci(unsigned n) {
    if (n == 0) return 0;
    if (n == 1) return 1;
    uint64_t a = 0, b = 1;
    for (unsigned i = 2; i <= n; ++i) {
        uint64_t c = a + b;
        a = b;
        b = c;
    }
    return b;
}

/**
 * @brief Compute ^n using Fibonacci recurrence (floating-point)
 * 
 * ^n = F_n   + F_{n-1}
 * 
 * @note For exact Z[] power computation, see ring::phiNegPower()
 *       in ring/ZPhi.hpp. For GoldenRatio::phiPower() see Closure.hpp.
 */
[[nodiscard]] inline double phiPower(int n) {
    if (n == 0) return 1.0;
    if (n > 0) {
        uint64_t fn = fibonacci(static_cast<unsigned>(n));
        uint64_t fn1 = fibonacci(static_cast<unsigned>(n - 1));
        return fn * PHI + fn1;
    } else {
        // Exact: ^{-m} = (-1)^m (F_{m+1} - F_m  )  for m = -n > 0
        // This avoids std::pow(PHI_INV, ...) which accumulates float error.
        unsigned m = static_cast<unsigned>(-n);
        uint64_t fm  = fibonacci(m);
        uint64_t fm1 = fibonacci(m + 1);
        double val = static_cast<double>(fm1) - static_cast<double>(fm) * PHI;
        return (m % 2 == 0) ? val : -val;
    }
}

/**
 * @brief Check if a number is (close to) an integer
 */
[[nodiscard]] inline bool isInteger(double x, double tol = 1e-10) {
    return std::abs(x - std::round(x)) < tol;
}

/**
 * @brief Check if x  [] (expressible as a + b with integer a, b)
 */
[[nodiscard]] inline bool isInGoldenRing(double x, double tol = 1e-8) {
    // x = a + b => a + b(1+5)/2 = x
    // b = x - a => b = (x - a) / 
    // Try integer values of a near x
    for (int a = static_cast<int>(x) - 5; a <= static_cast<int>(x) + 5; ++a) {
        double bCandidate = (x - a) / PHI;
        if (isInteger(bCandidate, tol)) {
            return true;
        }
    }
    return false;
}

} // namespace domain
} // namespace autodiscover

#endif // AUTODISCOVER_DOMAIN_ALGEBRA_HPP
