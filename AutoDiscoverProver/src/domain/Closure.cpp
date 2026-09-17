/**
 * @file Closure.cpp
 * @brief Implementation of golden ratio closure and fixed-point detection
 */

#include "Closure.hpp"
#include "../core/Constants.hpp"
#include <cmath>
#include <algorithm>

namespace autodiscover {
namespace domain {

// Minimal local result type used by this implementation file.
struct ClosureResult {
    bool converged;
    double value;
    int iterations;
};

// Use canonical PI from Constants.hpp
static constexpr double PI = constants::PI;

//------------------------------------------------------------------------------
// Fixed-point solver
//------------------------------------------------------------------------------

ClosureResult solveFixedPoint(
    std::function<double(double)> f,
    double initial,
    int maxIterations,
    double tolerance) {

    double x = initial;

    for (int i = 0; i < maxIterations; ++i) {
        double next = f(x);

        if (std::abs(next - x) < tolerance) {
            return ClosureResult{true, next, i};
        }

        x = next;
    }

    return ClosureResult{false, x, maxIterations};
}

//------------------------------------------------------------------------------
// Polynomial root finding
//------------------------------------------------------------------------------

std::vector<double> solveQuadratic(double a, double b, double c) {
    std::vector<double> roots;
    
    double discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) {
        // No real roots
        return roots;
    }
    
    if (std::abs(discriminant) < 1e-15) {
        // One root
        roots.push_back(-b / (2 * a));
    } else {
        // Two roots
        double sqrtD = std::sqrt(discriminant);
        roots.push_back((-b + sqrtD) / (2 * a));
        roots.push_back((-b - sqrtD) / (2 * a));
    }
    
    return roots;
}

//------------------------------------------------------------------------------
// Golden ratio verification
//------------------------------------------------------------------------------

bool verifyGoldenRatioProperty(double x, double tolerance) {
    // Check: x = x + 1
    return std::abs(x * x - x - 1.0) < tolerance;
}

bool verifyGoldenReciprocal(double x, double tolerance) {
    // Check: 1/x = x - 1
    if (std::abs(x) < 1e-15) return false;
    return std::abs(1.0 / x - (x - 1.0)) < tolerance;
}

bool verifyGoldenContinuedFraction(int depth, double tolerance) {
    // Verify  = 1 + 1/(1 + 1/(1 + ...)) converges
    double x = 1.0;
    
    for (int i = 0; i < depth; ++i) {
        x = 1.0 + 1.0 / x;
    }
    
    return std::abs(x - GoldenRatio::PHI) < tolerance;
}

//------------------------------------------------------------------------------
// Zeckendorf arithmetic
//------------------------------------------------------------------------------

uint64_t zeckendorfToDecimal(const std::vector<uint32_t>& indices) {
    uint64_t result = 0;
    
    for (uint32_t idx : indices) {
        result += Fibonacci::fib(idx);
    }
    
    return result;
}

std::vector<uint32_t> addZeckendorf(
    const std::vector<uint32_t>& a,
    const std::vector<uint32_t>& b) {
    
    // Convert to decimal, add, convert back
    uint64_t sum = zeckendorfToDecimal(a) + zeckendorfToDecimal(b);
    return Fibonacci::toZeckendorf(sum);
}

//------------------------------------------------------------------------------
// Fibonacci matrix operations
//------------------------------------------------------------------------------

std::pair<uint64_t, uint64_t> fibMatrix(uint32_t n) {
    // [F(n+1), F(n)] using matrix exponentiation
    // [1 1]^n   [F(n+1) F(n)  ]
    // [1 0]   = [F(n)   F(n-1)]
    
    if (n == 0) return {1, 0};
    if (n == 1) return {1, 1};
    
    uint64_t a = 1, b = 0, c = 0, d = 1;  // [a b; c d] = I
    uint64_t x = 1, y = 1, z = 1, w = 0;  // [x y; z w] = [1 1; 1 0]
    
    uint32_t exp = n;
    while (exp > 0) {
        if (exp & 1) {
            uint64_t newA = a * x + b * z;
            uint64_t newB = a * y + b * w;
            uint64_t newC = c * x + d * z;
            uint64_t newD = c * y + d * w;
            a = newA; b = newB; c = newC; d = newD;
        }
        
        uint64_t newX = x * x + y * z;
        uint64_t newY = x * y + y * w;
        uint64_t newZ = z * x + w * z;
        uint64_t newW = z * y + w * w;
        x = newX; y = newY; z = newZ; w = newW;
        
        exp >>= 1;
    }
    
    return {a, b};  // [F(n+1), F(n)]
}

uint64_t fibFast(uint32_t n) {
    auto [fnp1, fn] = fibMatrix(n);
    return fn;
}

//------------------------------------------------------------------------------
// Golden angle and spiral
//------------------------------------------------------------------------------

double goldenAngle() {
    // Golden angle = 2 /   137.5
    return 2.0 * PI / (GoldenRatio::PHI * GoldenRatio::PHI);
}

std::pair<double, double> goldenSpiralPoint(double theta) {
    // r = a * ^( / /2)
    double r = std::pow(GoldenRatio::PHI, theta * 2.0 / PI);
    return {r * std::cos(theta), r * std::sin(theta)};
}

//------------------------------------------------------------------------------
// Closure detector term analysis
//------------------------------------------------------------------------------

// Helper: detect recursion in an arbitrary term (used by closure analysis)
static bool detectRecursion(const Term* term, std::vector<const Term*>& stack) {
    for (const Term* t : stack) {
        if (t->id() == term->id()) return true;
    }

    stack.push_back(term);
    for (const Term* child : term->children()) {
        if (detectRecursion(child, stack)) {
            stack.pop_back();
            return true;
        }
    }
    stack.pop_back();
    return false;
}

// NOTE: tryIntroduceClosure is currently unused but preserved for future
// general fixed-point detection. Commented out to suppress C4505.
#if 0
// Helper: propose a canonical closure term for a recursive fixed-point.
// Returns nullptr if the recursive term does not match the golden ratio
// fixed-point equation R(z) = 1 + 1/z  z = z + 1.
// Returning factory.phi() unconditionally was a soundness hole because
// it would claim any recursive term has  as its closure, which is false.
static const Term* tryIntroduceClosure(TermFactory& factory, const Term* recursiveTerm) {
    if (!recursiveTerm) return nullptr;
    
    // Only introduce  if the recursive term is actually a FibStep application
    // (i.e., R(z) = 1 + 1/z), which has  as its unique positive fixed point.
    if (recursiveTerm->isApplication() && recursiveTerm->symbol() == "FibStep") {
        return factory.phi();
    }
    
    // For any other recursive structure, we cannot blindly claim .
    // Future: implement general fixed-point detection.
    return nullptr;
}
#endif

} // namespace domain
} // namespace autodiscover
