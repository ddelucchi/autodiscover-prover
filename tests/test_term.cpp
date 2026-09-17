/**
 * @file test_term.cpp
 * @brief Tests for term representation and hash-consing
 */

#include "core/Term.hpp"
#include <iostream>
#include <cassert>
#include <unordered_set>

using namespace autodiscover::core;

void testTermCreation() {
    std::cout << "Test: Term creation... ";
    
    TermFactory factory;
    
    // Create variables
    auto x = factory.variable("x", Sort::Complex);
    auto y = factory.variable("y", Sort::Complex);
    (void)x; (void)y;
    
    assert(x != nullptr);
    assert(y != nullptr);
    assert(x->kind() == TermKind::Variable);
    assert(x->symbol() == "x");
    assert(x->sort() == Sort::Complex);
    
    std::cout << "PASSED\n";
}

void testHashConsing() {
    std::cout << "Test: Hash consing (structural sharing)... ";
    
    TermFactory factory;
    
    // Same variable should return same pointer
    auto x1 = factory.variable("x", Sort::Real);
    auto x2 = factory.variable("x", Sort::Real);
    (void)x2;
    
    assert(x1 == x2);
    assert(x1->id() == x2->id());
    
    // Different variables should differ
    auto y = factory.variable("y", Sort::Real);
    assert(x1 != y);
    assert(x1->id() != y->id());
    
    // Same structure should share
    auto xy1 = factory.mul(x1, y);
    auto xy2 = factory.mul(x1, y);
    (void)xy1; (void)xy2;
    
    assert(xy1 == xy2);
    assert(xy1->id() == xy2->id());
    
    std::cout << "PASSED\n";
}

void testSpecialTerms() {
    std::cout << "Test: Special terms (, J)... ";
    
    TermFactory factory;
    
    // Golden ratio
    auto phi1 = factory.phi();
    auto phi2 = factory.phi();
    (void)phi1; (void)phi2;
    
    assert(phi1 == phi2);
    assert(phi1->kind() == TermKind::Phi);
    assert(phi1->toString().find("") != std::string::npos || 
           phi1->toString().find("phi") != std::string::npos);
    
    // Imaginary unit
    auto J1 = factory.J();
    auto J2 = factory.J();
    (void)J1; (void)J2;
    
    assert(J1 == J2);
    assert(J1->kind() == TermKind::J);
    
    std::cout << "PASSED\n";
}

void testPairTerms() {
    std::cout << "Test: Pair terms (Cayley-Dickson)... ";
    
    TermFactory factory;
    
    auto a = factory.variable("a", Sort::Real);
    auto b = factory.variable("b", Sort::Real);
    
    auto pair = factory.pair(a, b);
    (void)pair;
    
    assert(pair != nullptr);
    assert(pair->kind() == TermKind::Pair);
    assert(pair->children().size() == 2);
    assert(pair->children()[0] == a);
    assert(pair->children()[1] == b);
    
    std::cout << "PASSED\n";
}

void testOperations() {
    std::cout << "Test: Operations (add, mul, conj, neg)... ";
    
    TermFactory factory;
    
    auto x = factory.variable("x", Sort::Complex);
    auto y = factory.variable("y", Sort::Complex);
    
    // Addition
    auto sum = factory.add(x, y);
    (void)sum;
    assert(sum != nullptr);
    assert(sum->symbol() == "+");
    
    // Multiplication
    auto prod = factory.mul(x, y);
    (void)prod;
    assert(prod != nullptr);
    assert(prod->symbol() == "*");
    
    // Conjugation
    auto conj = factory.conj(x);
    (void)conj;
    assert(conj != nullptr);
    assert(conj->symbol() == "conj");
    
    // Negation
    auto neg = factory.neg(x);
    (void)neg;
    assert(neg != nullptr);
    assert(neg->symbol() == "neg");
    
    // Inverse
    auto inv = factory.inv(x);
    (void)inv;
    assert(inv != nullptr);
    assert(inv->symbol() == "inv");
    
    std::cout << "PASSED\n";
}

void testScalarTerms() {
    std::cout << "Test: Scalar terms... ";
    
    TermFactory factory;
    
    auto zero = factory.scalar(0.0);
    auto one = factory.scalar(1.0);
    auto pi = factory.scalar(3.14159);
    (void)zero; (void)one; (void)pi;
    
    assert(zero->kind() == TermKind::Scalar);
    assert(one->kind() == TermKind::Scalar);
    
    // Same scalar value should share
    auto one2 = factory.scalar(1.0);
    (void)one2;
    assert(one == one2);
    
    std::cout << "PASSED\n";
}

void testTermMetrics() {
    std::cout << "Test: Term metrics (depth, size)... ";
    
    TermFactory factory;
    
    auto x = factory.variable("x", Sort::Real);
    
    // Variable has depth 0, size 1
    assert(x->depth() == 0);
    assert(x->size() == 1);
    
    // f(x) has depth 1, size 2
    auto negX = factory.neg(x);
    assert(negX->depth() == 1);
    assert(negX->size() == 2);
    
    // f(g(x)) has depth 2
    auto negNegX = factory.neg(negX);
    (void)negNegX;
    assert(negNegX->depth() == 2);
    assert(negNegX->size() == 3);
    
    std::cout << "PASSED\n";
}

void testShortlexEncoding() {
    std::cout << "Test: Shortlex encoding... ";
    
    TermFactory factory;
    
    auto x = factory.variable("x", Sort::Real);
    auto y = factory.variable("y", Sort::Real);
    
    // Smaller terms should have smaller encodings
    auto small = x;
    auto large = factory.mul(x, y);
    
    std::string encSmall = small->encode();
    std::string encLarge = large->encode();
    
    // Shortlex: shorter strings come first
    assert(encSmall.length() <= encLarge.length());
    
    std::cout << "PASSED\n";
}

void testShortlexComparator() {
    std::cout << "Test: Shortlex comparator... ";
    
    TermFactory factory;
    
    ShortlexComparator cmp;
    (void)cmp;
    
    auto x = factory.variable("x", Sort::Real);
    auto y = factory.variable("y", Sort::Real);
    auto xy = factory.mul(x, y);
    (void)xy;
    
    // x < xy (smaller size)
    assert(cmp(x, xy));
    assert(!cmp(xy, x));
    
    // Same term is not < itself
    assert(!cmp(x, x));
    
    std::cout << "PASSED\n";
}

void testSCOUTOperations() {
    std::cout << "Test: SCOUT operations (align, phaseTransport)... ";
    
    TermFactory factory;
    
    auto U = factory.variable("U", Sort::Quaternion);
    auto q = factory.variable("q", Sort::Quaternion);
    auto phi = factory.variable("phi", Sort::Quaternion);
    
    // Alignment
    auto aligned = factory.align(U, q);
    (void)aligned;
    assert(aligned != nullptr);
    assert(aligned->symbol() == "Align");
    
    // Phase transport
    auto transported = factory.phaseTransport(phi, U);
    (void)transported;
    assert(transported != nullptr);
    assert(transported->symbol() == "PhaseTransport");
    
    std::cout << "PASSED\n";
}

void testFibStep() {
    std::cout << "Test: Fibonacci step R(z) = 1 + 1/z... ";
    
    TermFactory factory;
    
    auto z = factory.variable("z", Sort::Real);
    auto Rz = factory.fibStep(z);
    (void)Rz;
    
    assert(Rz != nullptr);
    assert(Rz->symbol() == "FibStep");
    assert(Rz->children().size() == 1);
    assert(Rz->children()[0] == z);
    
    std::cout << "PASSED\n";
}

int main() {
    std::cout << "=== Term Representation Tests ===\n";
    
    testTermCreation();
    testHashConsing();
    testSpecialTerms();
    testPairTerms();
    testOperations();
    testScalarTerms();
    testTermMetrics();
    testShortlexEncoding();
    testShortlexComparator();
    testSCOUTOperations();
    testFibStep();
    
    std::cout << "\nAll tests PASSED!\n";
    return 0;
}
