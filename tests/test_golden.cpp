/**
 * @file test_golden.cpp
 * @brief Tests for golden ratio, Fibonacci, and Zeckendorf representation
 */

#include "domain/Closure.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace autodiscover::domain;

void testGoldenRatioValue() {
    std::cout << "Test: Golden ratio value... ";
    
    // Test constant value
    assert(std::abs(GoldenRatio::PHI - 1.618033988749895) < 1e-10);
    assert(std::abs(GoldenRatio::PHI_INV - 0.618033988749895) < 1e-10);
    
    // Test identity:  * (1/) = 1
    assert(std::abs(GoldenRatio::PHI * GoldenRatio::PHI_INV - 1.0) < 1e-14);
    
    // Test identity:  - 1 = 1/
    assert(std::abs((GoldenRatio::PHI - 1.0) - GoldenRatio::PHI_INV) < 1e-14);
    
    std::cout << "PASSED\n";
}

void testGoldenRatioIdentity() {
    std::cout << "Test: Golden ratio identity  =  + 1... ";
    
    double phi = GoldenRatio::PHI;
    double lhs = phi * phi;
    double rhs = phi + 1.0;
    (void)lhs; (void)rhs;
    
    assert(std::abs(lhs - rhs) < 1e-14);
    assert(GoldenRatio::verifyIdentity(phi));
    
    std::cout << "PASSED\n";
}

void testFibStepConvergence() {
    std::cout << "Test: Fibonacci step R(z) = 1 + 1/z converges to ... ";
    
    // Start from various initial values
    double starts[] = {1.0, 2.0, 0.5, 10.0, 0.1};
    
    for (double start : starts) {
        double result = GoldenRatio::convergeToGolden(start, 100);
        (void)result;
        assert(GoldenRatio::isGolden(result, 1e-10));
    }
    
    std::cout << "PASSED\n";
}

void testFibonacciSequence() {
    std::cout << "Test: Fibonacci sequence... ";
    
    uint64_t expected[] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144};
    
    for (int i = 0; i <= 12; ++i) {
        assert(Fibonacci::fib(i) == expected[i]);
    }
    
    // Test larger values
    assert(Fibonacci::fib(20) == 6765);
    assert(Fibonacci::fib(30) == 832040);
    
    std::cout << "PASSED\n";
}

void testBinetFormula() {
    std::cout << "Test: Binet's formula... ";
    
    for (int i = 1; i <= 20; ++i) {
        double binet = Fibonacci::binetFormula(i);
        uint64_t exact = Fibonacci::fib(i);
        (void)binet; (void)exact;
        assert(std::abs(binet - static_cast<double>(exact)) < 0.5);
    }
    
    std::cout << "PASSED\n";
}

void testNormalWordCount() {
    std::cout << "Test: Normal word count N(n) = F_{n+2}... ";
    
    // N(0) = F_2 = 1 (empty word)
    // N(1) = F_3 = 2 (0, 1)
    // N(2) = F_4 = 3 (00, 01, 10)
    // N(3) = F_5 = 5 (000, 001, 010, 100, 101)
    
    assert(Fibonacci::normalWordCount(0) == 1);
    assert(Fibonacci::normalWordCount(1) == 2);
    assert(Fibonacci::normalWordCount(2) == 3);
    assert(Fibonacci::normalWordCount(3) == 5);
    assert(Fibonacci::normalWordCount(4) == 8);
    assert(Fibonacci::normalWordCount(5) == 13);
    
    std::cout << "PASSED\n";
}

void testZeckendorfRepresentation() {
    std::cout << "Test: Zeckendorf representation... ";
    
    // 1 = F_1
    auto z1 = Fibonacci::toZeckendorf(1);
    assert(!z1.empty());
    assert(Fibonacci::isValidZeckendorf(z1));
    
    // 10 = F_5 + F_2 = 8 + 2
    auto z10 = Fibonacci::toZeckendorf(10);
    assert(Fibonacci::isValidZeckendorf(z10));
    
    // 42 = F_9 + F_6 + F_2 = 34 + 8 + 2 = 44... wait, let me recalculate
    // F_1=1, F_2=1, F_3=2, F_4=3, F_5=5, F_6=8, F_7=13, F_8=21, F_9=34
    // 42 = 34 + 8 = F_9 + F_6... but that's 34+8=42. Check!
    auto z42 = Fibonacci::toZeckendorf(42);
    assert(Fibonacci::isValidZeckendorf(z42));
    
    // Verify reconstruction
    auto verify = [](uint64_t n) {
        auto zeck = Fibonacci::toZeckendorf(n);
        uint64_t sum = 0;
        for (auto idx : zeck) {
            sum += Fibonacci::fib(idx);
        }
        return sum == n;
    };
    
    for (uint64_t n = 1; n <= 100; ++n) {
        assert(verify(n));
    }
    
    std::cout << "PASSED\n";
}

void testTopologicalEntropy() {
    std::cout << "Test: Topological entropy h_top = log()... ";
    
    double h = GoldenMeanShift::topologicalEntropy();
    double expected = std::log(GoldenRatio::PHI);
    (void)h; (void)expected;
    
    assert(std::abs(h - expected) < 1e-14);
    assert(std::abs(h - 0.4812118) < 1e-6);
    
    // Growth rate should converge to log()
    assert(GoldenMeanShift::verifyEntropy(20, 0.01));
    assert(GoldenMeanShift::verifyEntropy(50, 0.001));
    
    std::cout << "PASSED\n";
}

int main() {
    std::cout << "=== Golden Ratio Tests ===\n";
    
    testGoldenRatioValue();
    testGoldenRatioIdentity();
    testFibStepConvergence();
    testFibonacciSequence();
    testBinetFormula();
    testNormalWordCount();
    testZeckendorfRepresentation();
    testTopologicalEntropy();
    
    std::cout << "\nAll tests PASSED!\n";
    return 0;
}
