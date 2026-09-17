/**
 * @file test_deep_modules.cpp
 * @brief Comprehensive tests for all deep-dive modules:
 * 
 *   1. K=12 weight dominance verification
 *   2. Equality saturation (RewriteRule + Saturator)
 *   3. Proof certificate replay kernel
 *   4. Multi-ring semantic evaluation (F_p, Z/2^k, Q)
 *   5. Dominant-index delta engine
 *   6. Zeckendorf BigInt normal form
 *   7. Rule mining from e-graph
 */

#include "../src/ring/ZPhi.hpp"
#include "../src/ring/ZeckendorfBigInt.hpp"
#include "../src/core/Term.hpp"
#include "../src/egraph/EGraph.hpp"
#include "../src/egraph/RuleMiner.hpp"
#include "../src/proof/CertificateKernel.hpp"
#include "../src/domain/MultiRingEval.hpp"
#include "../src/fingerprint/DeltaEngine.hpp"
#include "../src/fingerprint/Fingerprinter.hpp"

#include <cassert>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>

namespace ad = autodiscover;

// =========================================================================
// 1. K=12 WEIGHT DOMINANCE
// =========================================================================

void test_k12_dominance() {
    std::cout << "  [1] K=12 weight dominance... ";
    
    ad::ring::FibPow2Cache cache;
    cache.build(10);
    
    // Compute weights w_0, w_1, w_2
    auto w0 = ad::ring::ZPhi::weight(0, cache);
    auto w1 = ad::ring::ZPhi::weight(1, cache);
    auto w2 = ad::ring::ZPhi::weight(2, cache);
    
    // Verify weights are non-zero
    assert(!w0.isZero() && "w_0 must be non-zero");
    assert(!w1.isZero() && "w_1 must be non-zero");
    assert(!w2.isZero() && "w_2 must be non-zero");
    
    // Verify w_0  w_1 (different weights)
    assert(w0 != w1 && "w_0 must differ from w_1");
    
    // Verify the old K=1 collision no longer exists:
    // streams (1,3,1) and (2,1,2) must now give DIFFERENT fingerprints
    // FP(stream) =  (byte+1)  w_i
    auto fp1 = w0 * ad::ring::ZPhi(2) + w1 * ad::ring::ZPhi(4) + w2 * ad::ring::ZPhi(2);
    auto fp2 = w0 * ad::ring::ZPhi(3) + w1 * ad::ring::ZPhi(2) + w2 * ad::ring::ZPhi(3);
    assert(fp1 != fp2 && "K=12 must prevent the (1,3,1)/(2,1,2) collision");
    
    std::cout << "PASSED\n";
}

// =========================================================================
// 2. EQUALITY SATURATION
// =========================================================================

void test_equality_saturation() {
    std::cout << "  [2] Equality saturation... ";
    
    ad::egraph::EGraph graph;
    
    // Build terms: x, 0, x+0, 0+x
    auto x = graph.addLeaf("x");
    auto zero = graph.addLeaf("0");
    auto x_plus_0 = graph.addApp("add", {x, zero});
    auto zero_plus_x = graph.addApp("add", {zero, x});
    
    // They start in different classes
    assert(!graph.equivalent(x, x_plus_0) && "x and x+0 should start separate");
    
    // Assert x+0 = x (additive identity)
    graph.merge(x_plus_0, x, ad::egraph::MergeReason::fromAxiom(0, "add_identity_right"));
    graph.merge(zero_plus_x, x, ad::egraph::MergeReason::fromAxiom(1, "add_identity_left"));
    graph.rebuild();
    
    // Now x, x+0, and 0+x should all be equivalent
    assert(graph.equivalent(x, x_plus_0) && "x = x+0 after merge");
    assert(graph.equivalent(x, zero_plus_x) && "x = 0+x after merge");
    assert(graph.equivalent(x_plus_0, zero_plus_x) && "x+0 = 0+x transitively");
    
    // Test Saturator with a simple rule
    ad::egraph::RewriteRule rule;
    rule.name = "add_zero";
    rule.lhs = ad::egraph::Pattern::app("add", {
        ad::egraph::Pattern::var("$a"),
        ad::egraph::Pattern::var("$b")
    });
    rule.rhs = ad::egraph::Pattern::app("add", {
        ad::egraph::Pattern::var("$b"),
        ad::egraph::Pattern::var("$a")
    });
    rule.bidirectional = false;
    
    std::vector<ad::egraph::RewriteRule> rules = {rule};
    
    ad::egraph::Saturator::Config config;
    config.maxIterations = 5;
    config.maxNodes = 1000;
    
    ad::egraph::Saturator saturator(config);
    auto stats = saturator.saturate(graph, rules);
    
    // Should terminate (either fixpoint or iteration limit)
    assert(stats.iterations <= 5 && "Saturator should respect iteration limit");
    
    std::cout << "PASSED (iters=" << stats.iterations 
              << ", merges=" << stats.totalMerges << ")\n";
}

// =========================================================================
// 3. PROOF CERTIFICATE KERNEL
// =========================================================================

void test_proof_kernel() {
    std::cout << "  [3] Proof certificate kernel... ";
    
    ad::core::TermFactory factory;
    ad::proof::Kernel kernel;
    
    // Create terms
    auto a = factory.variable("a");
    auto b = factory.variable("b");
    
    // Test reflexivity
    auto r1 = kernel.reflexivity(a);
    assert(r1.success && "Reflexivity should succeed");
    assert(r1.theorem->lhs() == a && "Refl: lhs = a");
    assert(r1.theorem->rhs() == a && "Refl: rhs = a");
    
    // Register an axiom: a*b = b*a (commutativity)
    auto ab = factory.mul(a, b);
    auto ba = factory.mul(b, a);
    size_t commIdx = kernel.addAxiom(ab, ba, "comm", "ring");
    
    // Introduce axiom as theorem
    auto r2 = kernel.introduceAxiom(commIdx);
    assert(r2.success && "Axiom introduction should succeed");
    assert(r2.theorem->lhs() == ab && "Axiom lhs = a*b");
    assert(r2.theorem->rhs() == ba && "Axiom rhs = b*a");
    
    // Symmetry: from a*b = b*a, derive b*a = a*b
    auto r3 = kernel.symmetry(*r2.theorem);
    assert(r3.success && "Symmetry should succeed");
    assert(r3.theorem->lhs() == ba && "Sym: lhs = b*a");
    assert(r3.theorem->rhs() == ab && "Sym: rhs = a*b");
    
    // Transitivity: from a*b = b*a and b*a = a*b, derive a*b = a*b
    auto r4 = kernel.transitivity(*r2.theorem, *r3.theorem);
    assert(r4.success && "Transitivity should succeed");
    assert(r4.theorem->lhs() == ab && "Trans: lhs = a*b");
    assert(r4.theorem->rhs() == ab && "Trans: rhs = a*b");
    
    // Transitivity failure: mismatched intermediate terms
    auto r5 = kernel.transitivity(*r2.theorem, *r2.theorem);
    assert(!r5.success && "Transitivity should fail on mismatched intermediates");
    
    // Certificate replay
    std::vector<ad::proof::CertificateStep> cert;
    cert.push_back(ad::proof::CertificateStep::axiom(commIdx));
    
    auto replay = kernel.replayCertificate(cert, factory);
    assert(replay.success && "Certificate replay should succeed");
    
    // Invalid axiom index
    auto r6 = kernel.introduceAxiom(999);
    assert(!r6.success && "Invalid axiom index should fail");
    
    std::cout << "PASSED (theorems=" << kernel.totalTheorems() << ")\n";
}

// =========================================================================
// 4. MULTI-RING SEMANTIC EVALUATION
// =========================================================================

void test_multi_ring() {
    std::cout << "  [4] Multi-ring evaluation... ";
    
    ad::core::TermFactory factory;
    
    // Test F_p arithmetic
    ad::domain::FieldFp f7(7);
    assert(f7.add(3, 5) == 1 && "3+5  1 (mod 7)");
    assert(f7.mul(3, 5) == 1 && "35  15  1 (mod 7)");
    auto inv3 = f7.inv(3);
    assert(inv3.has_value() && *inv3 == 5 && "3^{-1}  5 (mod 7)");
    auto inv0 = f7.inv(0);
    assert(!inv0.has_value() && "0 has no inverse");
    
    // Test Z/2^k arithmetic
    ad::domain::RingMod2k r8(8);
    assert(r8.add(200, 100) == 44 && "200+100  44 (mod 256)");
    assert(r8.mul(16, 16) == 0 && "1616  0 (mod 256)");
    auto inv3_mod = r8.inv(3);
    assert(inv3_mod.has_value() && "3 is invertible in Z/2^8");
    assert(r8.mul(3, *inv3_mod) == 1 && "3  3^{-1}  1 (mod 256)");
    auto inv4_mod = r8.inv(4);
    assert(!inv4_mod.has_value() && "4 is NOT invertible in Z/2^8");
    
    // Test rational arithmetic
    ad::domain::Rational r1(1, 3);
    ad::domain::Rational r2(1, 6);
    auto sum = r1 + r2;
    assert(sum == ad::domain::Rational(1, 2) && "1/3 + 1/6 = 1/2");
    auto prod = r1 * r2;
    assert(prod == ad::domain::Rational(1, 18) && "1/3  1/6 = 1/18");
    
    // Test multi-ring gate on a TRUE identity: x+0 = x
    auto x = factory.variable("x");
    auto z = factory.constant("0");
    auto x_plus_0 = factory.add(x, z);
    
    ad::domain::MultiRingGate gate;
    gate.addStandardTestPoints({"x"});
    auto result = gate.test(x_plus_0, x);
    assert(result.passed && "x+0 = x should pass all rings");
    
    // Test on a FALSE identity: x+1 = x
    auto one = factory.constant("1");
    auto x_plus_1 = factory.add(x, one);
    auto result2 = gate.test(x_plus_1, x);
    assert(!result2.passed && "x+1 = x should be falsified");
    
    std::cout << "PASSED (gate tests: " << result.ringsTestedTotal << " ring evaluations)\n";
}

// =========================================================================
// 5. DOMINANT-INDEX DELTA ENGINE
// =========================================================================

void test_dominant_index() {
    std::cout << "  [5] Dominant-index delta... ";
    
    // Zero delta  maxIndex
    ad::ring::ZPhi zero_delta;
    size_t idx = ad::fingerprint::dominantIndex(zero_delta);
    (void)idx;
    assert(idx == 64 && "Zero delta should return maxIndex");
    
    // Small delta  small index
    ad::ring::ZPhi small_delta(1, 0);  // Just 1
    size_t idx2 = ad::fingerprint::dominantIndex(small_delta, 64);
    (void)idx2;
    // A value of 1 should have a very small dominant index (0 or near 0)
    assert(idx2 <= 2 && "Small delta should have small dominant index");
    
    // Large delta  larger index
    ad::ring::ZPhi large_delta(1000000, 0);
    size_t idx3 = ad::fingerprint::dominantIndex(large_delta, 64);
    (void)idx3;
    assert(idx3 > idx2 && "Larger delta should have larger dominant index");
    
    // Test DeltaPlan creation
    ad::fingerprint::DeltaPlanner planner;
    ad::ring::FibPow2Cache cache;
    cache.build(10);
    
    std::vector<uint8_t> srcStream = {10, 20, 30, 40, 50};
    ad::ring::ZPhi target_delta;  // Zero delta
    auto plan = planner.plan(srcStream, target_delta, cache);
    assert(plan.exact && "Zero delta should produce exact empty plan");
    assert(plan.isEmpty() && "Zero delta should need no edits");
    
    std::cout << "PASSED\n";
}

// =========================================================================
// 6. ZECKENDORF BIGINT
// =========================================================================

void test_zeckendorf_bigint() {
    std::cout << "  [6] Zeckendorf BigInt... ";
    
    // Test BigFibonacci
    auto [f10, f11] = ad::ring::BigFibonacci::fibPair(10);
    assert(f10 == ad::ring::BigInt(55) && "F_10 = 55");
    assert(f11 == ad::ring::BigInt(89) && "F_11 = 89");
    
    auto f20 = ad::ring::BigFibonacci::fib(20);
    assert(f20 == ad::ring::BigInt(6765) && "F_20 = 6765");
    
    // Test encode + decode roundtrip
    ad::ring::BigInt val42(42);
    auto indices = ad::ring::ZeckendorfBigInt::encode(val42);
    assert(!indices.empty() && "42 should have Zeckendorf representation");
    assert(ad::ring::ZeckendorfBigInt::isValid(indices) && "Must satisfy Zeckendorf property");
    
    auto decoded = ad::ring::ZeckendorfBigInt::decode(indices);
    assert(decoded == val42 && "Roundtrip: decode(encode(42)) = 42");
    
    // Test roundtrip for several values
    for (int64_t n = 1; n <= 100; ++n) {
        ad::ring::BigInt val(n);
        auto zeck = ad::ring::ZeckendorfBigInt::encode(val);
        assert(ad::ring::ZeckendorfBigInt::isValid(zeck) && "Zeckendorf property must hold");
        auto back = ad::ring::ZeckendorfBigInt::decode(zeck);
        assert(back == val && "Roundtrip must be exact");
    }
    
    // Test digit normalization
    std::vector<int> digits = {0, 1, 1, 0};  // has adjacent 1s
    ad::ring::ZeckendorfBigInt::normalizeDigits(digits);
    // After normalization, no two adjacent positions should both be 1
    for (size_t i = 0; i + 1 < digits.size(); ++i) {
        assert(!(digits[i] >= 1 && digits[i + 1] >= 1) && "No adjacent 1s after normalization");
    }
    
    // Test alpha probe
    auto alpha = ad::ring::ZeckendorfBigInt::alphaProbe(indices);
    assert(!alpha.isZero() && "Alpha probe of 42 should be non-zero");
    
    std::cout << "PASSED (100 roundtrips verified)\n";
}

// =========================================================================
// 7. RULE MINING
// =========================================================================

void test_rule_mining() {
    std::cout << "  [7] Rule mining... ";
    
    ad::egraph::EGraph graph;
    
    // Build a small e-graph with some equivalences
    auto x = graph.addLeaf("x");
    auto zero = graph.addLeaf("0");
    auto one = graph.addLeaf("1");
    
    auto x_plus_0 = graph.addApp("add", {x, zero});
    auto zero_plus_x = graph.addApp("add", {zero, x});
    auto x_times_1 = graph.addApp("mul", {x, one});
    auto one_times_x = graph.addApp("mul", {one, x});
    
    // Merge identities
    graph.merge(x_plus_0, x, ad::egraph::MergeReason::fromAxiom(0, "add_id_right"));
    graph.merge(zero_plus_x, x, ad::egraph::MergeReason::fromAxiom(1, "add_id_left"));
    graph.merge(x_times_1, x, ad::egraph::MergeReason::fromAxiom(2, "mul_id_right"));
    graph.merge(one_times_x, x, ad::egraph::MergeReason::fromAxiom(3, "mul_id_left"));
    graph.rebuild();
    
    // Mine rules
    ad::egraph::RuleMiner miner;
    auto candidates = miner.mine(graph);
    
    // Should find some candidate rules (at least the identity simplifications)
    // The e-class containing x should have multiple nodes: x, add(x,0), mul(x,1), etc.
    assert(candidates.size() > 0 && "Should mine at least one candidate rule");
    
    // Check that mined rules have reasonable scores
    for (const auto& rule : candidates) {
        (void)rule;
        assert(rule.score >= 0.0 && "Scores should be non-negative");
        assert(!rule.lhsDescription.empty() && "LHS description must not be empty");
        assert(!rule.rhsDescription.empty() && "RHS description must not be empty");
    }
    
    std::cout << "PASSED (" << candidates.size() << " rules mined)\n";
}

// =========================================================================
// MAIN
// =========================================================================

int main() {
    std::cout << "=== Deep-Dive Module Tests ===\n\n";
    
    test_k12_dominance();
    test_equality_saturation();
    test_proof_kernel();
    test_multi_ring();
    test_dominant_index();
    test_zeckendorf_bigint();
    test_rule_mining();
    
    std::cout << "\n=== ALL DEEP-DIVE TESTS PASSED ===\n";
    return 0;
}
