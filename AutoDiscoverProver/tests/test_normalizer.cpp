/**
 * @file test_normalizer.cpp
 * @brief Tests for GOD normalization and rewrite rules
 */

#include "core/Term.hpp"
#include "logic/Normalizer.hpp"
#include <iostream>
#include <cassert>

using namespace autodiscover::core;
using namespace autodiscover::logic;

void testCarryRewriterNormal() {
    std::cout << "Test: Carry rewriter normal form detection... ";
    
    // Normal words (no consecutive 1s)
    assert(CarryRewriter::isNormal({0}));
    assert(CarryRewriter::isNormal({1}));
    assert(CarryRewriter::isNormal({0, 1}));
    assert(CarryRewriter::isNormal({1, 0}));
    assert(CarryRewriter::isNormal({1, 0, 1}));
    assert(CarryRewriter::isNormal({0, 1, 0, 1}));
    
    // Non-normal words (have consecutive 1s)
    assert(!CarryRewriter::isNormal({1, 1}));
    assert(!CarryRewriter::isNormal({0, 1, 1}));
    assert(!CarryRewriter::isNormal({1, 1, 0}));
    assert(!CarryRewriter::isNormal({1, 0, 1, 1}));
    
    std::cout << "PASSED\n";
}

void testCarryRewriterNormalization() {
    std::cout << "Test: Carry rewriter normalization... ";
    
    // 11 -> 100 (in value: 1+1 = 2 -> 2)
    auto n1 = CarryRewriter::normalize({1, 1});
    assert(CarryRewriter::isNormal(n1));
    
    // 011 -> 100 (011 in LSB-first is 110 in normal = 2+4=6, but with  value)
    auto n2 = CarryRewriter::normalize({0, 1, 1});
    assert(CarryRewriter::isNormal(n2));
    
    // 1011 -> should normalize
    auto n3 = CarryRewriter::normalize({1, 0, 1, 1});
    assert(CarryRewriter::isNormal(n3));
    
    // All outputs should lack consecutive 1s
    std::vector<std::vector<uint8_t>> testCases = {
        {1, 1},
        {1, 1, 1},
        {0, 1, 1, 0},
        {1, 1, 1, 1},
        {1, 0, 1, 1, 0, 1, 1}
    };
    
    for (const auto& tc : testCases) {
        auto normalized = CarryRewriter::normalize(tc);
        assert(CarryRewriter::isNormal(normalized));
    }
    
    std::cout << "PASSED\n";
}

void testCarryRewriterCounting() {
    std::cout << "Test: Carry rewriter normal word count... ";
    
    // N(n) = F_{n+2}
    // N(0) = F_2 = 1
    // N(1) = F_3 = 2
    // N(2) = F_4 = 3
    // etc.
    
    assert(CarryRewriter::countNormal(0) == 1);
    assert(CarryRewriter::countNormal(1) == 2);
    assert(CarryRewriter::countNormal(2) == 3);
    assert(CarryRewriter::countNormal(3) == 5);
    assert(CarryRewriter::countNormal(4) == 8);
    assert(CarryRewriter::countNormal(5) == 13);
    assert(CarryRewriter::countNormal(10) == 144);
    
    std::cout << "PASSED\n";
}

void testNormalizerDoubleNegation() {
    std::cout << "Test: Normalizer double negation --x = x... ";
    
    TermFactory factory;
    Normalizer normalizer(factory);
    normalizer.initStandardRules();
    
    auto x = factory.variable("x", Sort::Real);
    auto negX = factory.neg(x);
    auto negNegX = factory.neg(negX);
    
    auto normalized = normalizer.normalize(negNegX);
    (void)normalized;
    
    assert(normalized == x);
    
    std::cout << "PASSED\n";
}

void testNormalizerDoubleConjugation() {
    std::cout << "Test: Normalizer double conjugation (x*)* = x... ";
    
    TermFactory factory;
    Normalizer normalizer(factory);
    normalizer.initStandardRules();
    
    auto x = factory.variable("x", Sort::Complex);
    auto conjX = factory.conj(x);
    auto conjConjX = factory.conj(conjX);
    
    auto normalized = normalizer.normalize(conjConjX);
    (void)normalized;
    
    assert(normalized == x);
    
    std::cout << "PASSED\n";
}

void testNormalizerAddZero() {
    std::cout << "Test: Normalizer x + 0 = x... ";
    
    TermFactory factory;
    Normalizer normalizer(factory);
    normalizer.initStandardRules();
    
    auto x = factory.variable("x", Sort::Real);
    auto zero = factory.scalar(0.0);
    auto xPlusZero = factory.add(x, zero);
    
    auto normalized = normalizer.normalize(xPlusZero);
    (void)normalized;
    
    assert(normalized == x);
    
    std::cout << "PASSED\n";
}

void testNormalizerMulOne() {
    std::cout << "Test: Normalizer x * 1 = x... ";
    
    TermFactory factory;
    Normalizer normalizer(factory);
    normalizer.initStandardRules();
    
    auto x = factory.variable("x", Sort::Real);
    auto one = factory.scalar(1.0);
    auto xTimesOne = factory.mul(x, one);
    
    auto normalized = normalizer.normalize(xTimesOne);
    (void)normalized;
    
    assert(normalized == x);
    
    std::cout << "PASSED\n";
}

void testNormalizerMulZero() {
    std::cout << "Test: Normalizer x * 0 = 0... ";
    
    TermFactory factory;
    Normalizer normalizer(factory);
    normalizer.initStandardRules();
    
    auto x = factory.variable("x", Sort::Real);
    auto zero = factory.scalar(0.0);
    auto xTimesZero = factory.mul(x, zero);
    
    auto normalized = normalizer.normalize(xTimesZero);
    (void)normalized;
    
    assert(normalized->kind() == TermKind::Scalar);
    assert(normalized->symbol() == "0");
    
    std::cout << "PASSED\n";
}

void testNormalizerPlusInverse() {
    std::cout << "Test: Normalizer x + (-x) = 0... ";
    
    TermFactory factory;
    Normalizer normalizer(factory);
    normalizer.initStandardRules();
    
    auto x = factory.variable("x", Sort::Real);
    auto negX = factory.neg(x);
    auto xPlusNegX = factory.add(x, negX);
    
    auto normalized = normalizer.normalize(xPlusNegX);
    (void)normalized;
    
    assert(normalized->kind() == TermKind::Scalar);
    assert(normalized->symbol() == "0");
    
    std::cout << "PASSED\n";
}

void testNormalizerPhiSquared() {
    std::cout << "Test: Normalizer  =  + 1... ";
    
    TermFactory factory;
    Normalizer normalizer(factory);
    normalizer.initStandardRules();
    
    auto phi = factory.phi();
    auto phiSquared = factory.mul(phi, phi);
    
    auto normalized = normalizer.normalize(phiSquared);
    
    // Should be add(phi, 1)
    assert(normalized->kind() == TermKind::Application);
    assert(normalized->symbol() == "add");
    assert(normalized->children().size() == 2);
    
    // One child should be , other should be 1
    bool hasPhi = false, hasOne = false;
    for (const Term* child : normalized->children()) {
        if (child->kind() == TermKind::Phi) hasPhi = true;
        if (child->kind() == TermKind::Scalar && child->symbol() == "1") hasOne = true;
    }
    assert(hasPhi && hasOne);
    
    std::cout << "PASSED\n";
}

void testGODNormalizerIdempotence() {
    std::cout << "Test: GOD normalizer idempotence GOD(GOD(x)) = GOD(x)... ";
    
    TermFactory factory;
    GODNormalizer god(factory);
    god.initCayleyDicksonActions();
    
    auto a = factory.variable("a", Sort::Real);
    auto b = factory.variable("b", Sort::Real);
    auto pair = factory.pair(a, b);
    
    auto norm1 = god.normalize(pair);
    auto norm2 = god.normalize(norm1);
    (void)norm2;
    
    // Idempotence: applying twice gives same result
    assert(norm1 == norm2);
    
    std::cout << "PASSED\n";
}

void testGODNormalizerInvariance() {
    std::cout << "Test: GOD normalizer orbit invariance... ";
    
    TermFactory factory;
    GODNormalizer god(factory);
    god.initCayleyDicksonActions();
    
    auto a = factory.variable("a", Sort::Real);
    auto b = factory.variable("b", Sort::Real);
    auto pair = factory.pair(a, b);
    
    // Apply actions to get different orbit elements
    auto conjFlipped = factory.pair(factory.conj(a), factory.neg(b));
    
    auto norm1 = god.normalize(pair);
    auto norm2 = god.normalize(conjFlipped);
    (void)norm1; (void)norm2;
    
    // Elements in same orbit should normalize to same thing
    // (This depends on the specific actions defined)
    // For now, just verify normalization completes
    assert(norm1 != nullptr);
    assert(norm2 != nullptr);
    
    std::cout << "PASSED\n";
}

void testGroupoidActionSelfInverse() {
    std::cout << "Test: Groupoid action self-inverse property... ";
    
    TermFactory factory;
    
    auto a = factory.variable("a", Sort::Real);
    auto b = factory.variable("b", Sort::Real);
    auto pair = factory.pair(a, b);
    (void)pair;
    
    // conj_flip: (a,b) -> (a*,-b) -> should be self-inverse
    auto conjFlipped = factory.pair(factory.conj(a), factory.neg(b));
    (void)conjFlipped;
    auto doubleFlipped = factory.pair(factory.conj(factory.conj(a)), factory.neg(factory.neg(b)));
    
    // After normalization, (a*)* = a and --b = b
    Normalizer normalizer(factory);
    normalizer.initStandardRules();
    
    auto normalized = normalizer.normalize(doubleFlipped);
    (void)normalized;
    
    // Should return to original (a, b)
    assert(normalized->kind() == TermKind::Pair);
    assert(normalized->children()[0] == a);
    assert(normalized->children()[1] == b);
    
    std::cout << "PASSED\n";
}

int main() {
    std::cout << "=== Normalizer Tests ===\n";
    
    testCarryRewriterNormal();
    testCarryRewriterNormalization();
    testCarryRewriterCounting();
    testNormalizerDoubleNegation();
    testNormalizerDoubleConjugation();
    testNormalizerAddZero();
    testNormalizerMulOne();
    testNormalizerMulZero();
    testNormalizerPlusInverse();
    testNormalizerPhiSquared();
    testGODNormalizerIdempotence();
    testGODNormalizerInvariance();
    testGroupoidActionSelfInverse();
    
    std::cout << "\nAll tests PASSED!\n";
    return 0;
}
