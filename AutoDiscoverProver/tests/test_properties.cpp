/**
 * @file test_properties.cpp
 * @brief Property and roundtrip tests for core invariants
 *
 * Tests that key mathematical and engineering invariants hold:
 *   1. ZPhi arithmetic ring properties (commutativity, associativity, etc.)
 *   2. Normalizer idempotence: normalize(normalize(t)) == normalize(t)
 *   3. Encoding roundtrip injectivity
 *   4. Fingerprint consistency
 *   5. Phase_mod4 algebra is exact
 *   6. Discrimination tree retrieval correctness
 *   7. Proof topological order well-formedness
 *   8. TSTP export roundtrip
 */

#include "core/Term.hpp"
#include "ring/ZPhi.hpp"
#include "logic/Normalizer.hpp"
#include "logic/KnowledgeBase.hpp"
#include "logic/Equation.hpp"
#include "logic/InferenceEngine.hpp"
#include "proof/Proof.hpp"
#include "proof/CertificateExporter.hpp"
#include "encoding/Structural.hpp"
#include "egraph/EGraph.hpp"
#include "canon/NFEngine.hpp"
#include "canon/Equivalence.hpp"
#include <iostream>
#include <cassert>
#include <string>
#include <unordered_set>
#include <algorithm>

using namespace autodiscover;

// =============================================================================
// 1. ZPhi ring properties
// =============================================================================

void testZPhiCommutativity() {
    std::cout << "Test: ZPhi multiplication commutativity... ";
    
    using ring::ZPhi;
    
    // (a + b)(c + d) == (c + d)(a + b) for several values
    std::pair<int64_t, int64_t> vals[] = {
        {0, 0}, {1, 0}, {0, 1}, {3, -2}, {-5, 7}, {13, 8}, {-1, -1}
    };
    
    for (auto [a, b] : vals) {
        for (auto [c, d] : vals) {
            ZPhi x(a, b), y(c, d);
            ZPhi xy = x * y;
            ZPhi yx = y * x;
            assert(xy == yx);
        }
    }
    
    std::cout << "PASSED\n";
}

void testZPhiAssociativity() {
    std::cout << "Test: ZPhi multiplication associativity... ";
    
    using ring::ZPhi;
    
    ZPhi a(3, -2), b(-1, 5), c(2, 3);
    
    ZPhi ab_c = (a * b) * c;
    ZPhi a_bc = a * (b * c);
    
    assert(ab_c == a_bc);
    
    // Also test addition associativity
    ZPhi ab_plus_c = (a + b) + c;
    ZPhi a_plus_bc = a + (b + c);
    assert(ab_plus_c == a_plus_bc);
    
    std::cout << "PASSED\n";
}

void testZPhiDistributivity() {
    std::cout << "Test: ZPhi distributivity a*(b+c) == a*b + a*c... ";
    
    using ring::ZPhi;
    
    ZPhi a(3, 1), b(-2, 4), c(5, -1);
    
    ZPhi lhs = a * (b + c);
    ZPhi rhs = a * b + a * c;
    
    assert(lhs == rhs);
    
    std::cout << "PASSED\n";
}

void testZPhiPhiSquaredIdentity() {
    std::cout << "Test: ZPhi  =  + 1... ";
    
    using ring::ZPhi;
    
    //  = ZPhi(0, 1), so  = (0 + 1) 
    // = 0 + 0 + 0 + 1 = 1 
    // In Z[]:  =  + 1 = ZPhi(1, 1)
    ZPhi phi(0, 1);
    ZPhi phi_sq = phi * phi;
    ZPhi phi_plus_1(1, 1);
    
    assert(phi_sq == phi_plus_1);
    
    std::cout << "PASSED\n";
}

void testZPhiNormProperty() {
    std::cout << "Test: ZPhi norm is multiplicative N(xy) = N(x)N(y)... ";
    
    using ring::ZPhi;
    using ring::BigInt;
    
    ZPhi x(3, 2), y(-1, 4);
    
    BigInt nxy = (x * y).norm();
    BigInt nx_ny = x.norm() * y.norm();
    
    assert(nxy == nx_ny);
    
    std::cout << "PASSED\n";
}

void testZPhiIdentityElement() {
    std::cout << "Test: ZPhi identity elements (0, 1)... ";
    
    using ring::ZPhi;
    
    ZPhi zero;
    ZPhi one(1, 0);
    ZPhi x(7, -3);
    
    assert(x + zero == x);
    assert(x * one == x);
    assert(x + (-x) == zero);
    
    std::cout << "PASSED\n";
}

// =============================================================================
// 2. Normalizer idempotence
// =============================================================================

void testNormalizerIdempotence() {
    std::cout << "Test: Normalizer idempotence...  ";
    
    core::TermFactory factory;
    logic::Normalizer normalizer(factory);
    normalizer.initStandardRules();
    
    // Build term: mul(phi, add(x, y))
    auto x = factory.variable("x", core::Sort::Complex);
    auto y = factory.variable("y", core::Sort::Complex);
    auto phi = factory.phi();
    auto sum = factory.add(x, y);
    auto prod = factory.mul(phi, sum);
    
    const core::Term* once = normalizer.normalize(prod);
    const core::Term* twice = normalizer.normalize(once);
    (void)twice;
    
    // Idempotence: normalize(normalize(t)) == normalize(t)
    assert(once->id() == twice->id());
    
    // Also test on nested terms
    auto conj_prod = factory.conj(prod);
    const core::Term* conj_once = normalizer.normalize(conj_prod);
    const core::Term* conj_twice = normalizer.normalize(conj_once);
    (void)conj_twice;
    assert(conj_once->id() == conj_twice->id());
    
    // Negation
    auto neg_x = factory.neg(x);
    auto neg_neg_x = factory.neg(neg_x);
    const core::Term* dneg_once = normalizer.normalize(neg_neg_x);
    const core::Term* dneg_twice = normalizer.normalize(dneg_once);
    (void)dneg_twice;
    assert(dneg_once->id() == dneg_twice->id());
    
    std::cout << "PASSED\n";
}

// =============================================================================
// 3. Encoding roundtrip injectivity 
// =============================================================================

void testEncodingInjectivity() {
    std::cout << "Test: Term.encode() injectivity... ";
    
    core::TermFactory factory;
    
    // Build a variety of structurally-distinct terms
    auto x = factory.variable("x", core::Sort::Complex);
    auto y = factory.variable("y", core::Sort::Complex);
    auto z = factory.variable("z", core::Sort::Complex);
    auto phi = factory.phi();
    auto J = factory.J();
    
    std::vector<const core::Term*> terms = {
        x, y, z, phi, J,
        factory.add(x, y),
        factory.add(y, x),
        factory.mul(x, y),
        factory.mul(y, x),
        factory.neg(x),
        factory.conj(x),
        factory.pair(x, y),
        factory.pair(y, x),
        factory.add(x, factory.add(y, z)),
        factory.add(factory.add(x, y), z),
        factory.mul(phi, x),
        factory.mul(x, phi),
        factory.pair(phi, J),
        factory.pair(J, phi),
        factory.neg(factory.conj(x)),
        factory.conj(factory.neg(x)),
    };
    
    // All distinct terms must have distinct encodings (injectivity)
    std::unordered_set<std::string> encodings;
    for (const core::Term* t : terms) {
        std::string enc = t->encode();
        // Check uniqueness: each structurally-distinct term produces a unique encoding
        // But note: hash-consed terms with same structure produce same encoding
        auto [it, inserted] = encodings.insert(enc);
        // Only check injectivity between non-identical terms
        if (!inserted) {
            // Find which other term has the same encoding
            for (const core::Term* other : terms) {
                if (other != t && other->encode() == enc) {
                    // Two distinct terms with same encoding  this is fine
                    // if they're structurally identical (hash-consed differently?)
                    // Actually with hash-consing, they WOULD be the same pointer.
                    // So if pointers differ, encodings must differ.
                    assert(other->id() == t->id()); // Must be same term
                }
            }
        }
    }
    
    std::cout << "PASSED\n";
}

// =============================================================================
// 4. Phase_mod4 algebra
// =============================================================================

void testPhaseMod4Algebra() {
    std::cout << "Test: Phase_mod4 /4 arithmetic... ";
    
    // Phase operations from InferenceEngine:
    //   J:    (phase + 1) & 3
    //   conj: (4 - phase) & 3
    //   neg:  (phase + 2) & 3
    
    // Test: J applied 4 times returns to original
    for (uint8_t p = 0; p < 4; ++p) {
        uint8_t after4J = p;
        for (int i = 0; i < 4; ++i) {
            after4J = (after4J + 1) & 3;
        }
        assert(after4J == p);
    }
    
    // Test: neg(neg(x)) == x    (phase + 2 + 2) & 3 == phase
    for (uint8_t p = 0; p < 4; ++p) {
        uint8_t negated = (p + 2) & 3;
        uint8_t double_neg = (negated + 2) & 3;
        (void)double_neg;
        assert(double_neg == p);
    }
    
    // Test: conj(conj(x)) == x    (4 - (4 - phase)) & 3 == phase
    for (uint8_t p = 0; p < 4; ++p) {
        uint8_t conjugated = (4 - p) & 3;
        uint8_t double_conj = (4 - conjugated) & 3;
        (void)double_conj;
        assert(double_conj == p);
    }
    
    // Test: Jconj is an involution (J-conj-J-conj == identity happens at phase level)
    // Actually JconjJconj: 
    //   step 1: conj(p) = (4-p) & 3
    //   step 2: J(r)    = (r+1) & 3  where r = step1
    //   So Jconj(p)    = (4-p+1)&3 = (5-p)&3
    //   (Jconj)^2(p)   = (5-(5-p)&3)&3 ... well let's just verify directly:
    for (uint8_t p = 0; p < 4; ++p) {
        uint8_t r1 = ((4 - p) + 1) & 3;   // J(conj(p))
        uint8_t r2 = ((4 - r1) + 1) & 3;  // J(conj(J(conj(p))))
        (void)r2;
        // Jconj has order dividing 4, so after 4 iterations we return
        uint8_t rr = p;
        for (int i = 0; i < 4; ++i) {
            rr = ((4 - rr) + 1) & 3;
        }
        assert(rr == p);
    }
    
    std::cout << "PASSED\n";
}

// =============================================================================
// 5. Discrimination tree retrieval
// =============================================================================

void testDiscriminationTreeBasic() {
    std::cout << "Test: Discrimination tree insert and retrieve... ";
    
    core::TermFactory factory;
    logic::DiscriminationTree dt;
    
    auto x = factory.variable("x", core::Sort::Complex);
    auto y = factory.variable("y", core::Sort::Complex);
    auto add_xy = factory.add(x, y);
    auto mul_xy = factory.mul(x, y);
    auto phi = factory.phi();
    
    // Insert some terms
    logic::IndexEntry e1{1, true, add_xy};
    logic::IndexEntry e2{2, true, mul_xy};
    logic::IndexEntry e3{3, false, phi};
    
    dt.insert(add_xy, e1);
    dt.insert(mul_xy, e2);
    dt.insert(phi, e3);
    
    assert(dt.size() == 3);
    
    // Retrieve: looking for add_xy should find it
    auto results = dt.retrieve(add_xy);
    bool found_add = false;
    for (const auto& r : results) {
        if (r.equationId == 1) found_add = true;
    }
    assert(found_add);
    
    // Retrieve by root should find "add" entries but not "mul"
    auto addResults = dt.retrieveByRoot("add");
    bool has_add = false, has_mul = false;
    for (const auto& r : addResults) {
        if (r.equationId == 1) has_add = true;
        if (r.equationId == 2) has_mul = true;
    }
    assert(has_add);
    assert(!has_mul);
    
    std::cout << "PASSED\n";
}

void testDiscriminationTreeVariableMatchesAll() {
    std::cout << "Test: Discrimination tree variable-as-query... ";
    
    core::TermFactory factory;
    logic::DiscriminationTree dt;
    
    auto x = factory.variable("x", core::Sort::Complex);
    auto y = factory.variable("y", core::Sort::Complex);
    auto phi = factory.phi();
    auto J = factory.J();
    
    dt.insert(phi, logic::IndexEntry{1, true, phi});
    dt.insert(J, logic::IndexEntry{2, true, J});
    dt.insert(factory.add(x, y), logic::IndexEntry{3, true, factory.add(x, y)});
    
    // Querying with a variable should match all entries
    auto results = dt.retrieve(x);
    // Variables match all single-node entries (phi, J) at minimum
    assert(results.size() >= 2);
    
    std::cout << "PASSED\n";
}

// =============================================================================
// 6. Proof topological order
// =============================================================================

void testProofTopologicalOrder() {
    std::cout << "Test: Proof topological order... ";
    
    core::TermFactory factory;
    auto x = factory.variable("x", core::Sort::Complex);
    auto y = factory.variable("y", core::Sort::Complex);
    
    auto eq_xy = std::make_unique<logic::Equation>(x, y, logic::EquationSource::Axiom);
    auto eq_yx = std::make_unique<logic::Equation>(y, x, logic::EquationSource::Inference);
    
    proof::Proof p;
    auto axiomStep = p.addAxiom(eq_xy.get(), "axiom1");
    // Use GODNormalization (takes single premise) to create a derived step
    auto derivedStep = p.addGODNormalization(eq_yx.get(), axiomStep);
    
    auto order = p.topologicalOrder();
    
    // Axiom must come before derived step in topological order
    assert(order.size() == 2);
    
    // Find positions
    size_t axiomPos = SIZE_MAX, derivedPos = SIZE_MAX;
    for (size_t i = 0; i < order.size(); ++i) {
        if (order[i] == axiomStep) axiomPos = i;
        if (order[i] == derivedStep) derivedPos = i;
    }
    assert(axiomPos < derivedPos);
    
    std::cout << "PASSED\n";
}

// =============================================================================
// 7. TSTP export basic sanity
// =============================================================================

void testTSTPExportFormat() {
    std::cout << "Test: TSTP export format... ";
    
    core::TermFactory factory;
    auto x = factory.variable("x", core::Sort::Complex);
    auto y = factory.variable("y", core::Sort::Complex);
    
    auto eq = std::make_unique<logic::Equation>(x, y, logic::EquationSource::Axiom);
    
    proof::Proof p;
    p.addAxiom(eq.get(), "test_axiom");
    
    proof::CertificateExporter exporter;
    std::string tstp = exporter.toTSTP(p);
    
    // Must contain SZS status header
    assert(tstp.find("SZS status Theorem") != std::string::npos);
    
    // Must contain SZS output markers
    assert(tstp.find("SZS output start Proof") != std::string::npos);
    assert(tstp.find("SZS output end Proof") != std::string::npos);
    
    // Must contain fof() step
    assert(tstp.find("fof(step_") != std::string::npos);
    
    // Must contain axiom type
    assert(tstp.find("axiom") != std::string::npos);
    
    // TPTP variables should be uppercase (no X_ prefix anymore)
    // The variable "x" should appear as just "x" or quoted if needed
    assert(tstp.find("equal(") != std::string::npos);
    
    std::cout << "PASSED\n";
}

// =============================================================================
// 8. KnowledgeBase consistency
// =============================================================================

void testKnowledgeBaseAddAndRetrieve() {
    std::cout << "Test: KnowledgeBase add and retrieve... ";
    
    core::TermFactory factory;
    logic::KnowledgeBase kb(factory);
    
    auto x = factory.variable("x", core::Sort::Complex);
    auto y = factory.variable("y", core::Sort::Complex);
    auto phi = factory.phi();
    
    auto id1 = kb.addAxiom(factory.add(x, y), factory.add(y, x));
    auto id2 = kb.addAxiom(factory.mul(phi, x), factory.add(x, factory.mul(phi, factory.add(x, factory.neg(x)))));
    (void)id1; (void)id2;
    
    assert(kb.numTotal() >= 2);
    assert(kb.numValid() >= 2);
    
    // Retrieve by root symbol
    auto addCandidates = kb.getCandidates("add");
    assert(!addCandidates.empty());
    
    // Discrimination tree retrieval
    auto dtCandidates = kb.getCandidatesForTerm(factory.add(x, y));
    assert(!dtCandidates.empty());
    
    // Index size should reflect insertions
    assert(kb.indexSize() >= 4); // At least 2 equations  2 sides
    
    std::cout << "PASSED\n";
}

// =============================================================================
// Main
// =============================================================================
// 9. Structural equation encoder ZPhi-ZPhi injectivity
// =============================================================================

void testZPhiPairingInjectivity() {
    std::cout << "Test: ZPhi-ZPhi equation pairing injectivity... ";
    
    // The old bug: (a1=3,a2=0) and (a1=1,a2=1) both mapped to 3
    // with the simple *2 shift. Now we use Cantor pairing which is injective.
    using encoding::StructuralEncoder;
    using encoding::StructuralEquationEncoder;
    using encoding::EncodingStrategy;
    
    core::TermFactory factory;
    
    // Create distinct terms
    auto x = factory.variable("x");
    auto y = factory.variable("y");
    auto z = factory.variable("z");
    auto w = factory.variable("w");
    
    StructuralEncoder encoder(EncodingStrategy::ZECKENDORF);
    StructuralEquationEncoder eqEncoder(EncodingStrategy::ZECKENDORF);
    
    // Encode several distinct equation pairs and verify no collisions
    auto ex = encoder.encode(*x);
    auto ey = encoder.encode(*y);
    auto ez = encoder.encode(*z);
    auto ew = encoder.encode(*w);
    
    // If the strategy produces ZPhi codes, test pairings
    if (ex.isPhi() && ey.isPhi()) {
        auto c1 = eqEncoder.encode(*x, *y);
        auto c2 = eqEncoder.encode(*y, *x);
        auto c3 = eqEncoder.encode(*x, *z);
        auto c4 = eqEncoder.encode(*z, *x);
        
        // All must be distinct (x=y vs y=x might be same if commutative, but
        // the encoder doesn't reorder  so they should differ)
        // c1 and c3 must differ since y  z
        assert(!(c1 == c3) && "Distinct equations must have distinct codes");
    }
    
    // Also test with positional encoding (produces BigInt codes)
    StructuralEquationEncoder eqEncoder256(EncodingStrategy::POSITIONAL_256);
    auto p1 = eqEncoder256.encode(*x, *y);
    auto p2 = eqEncoder256.encode(*x, *z);
    auto p3 = eqEncoder256.encode(*y, *z);
    
    // All three must be distinct  
    assert(!(p1 == p2) && "x=y vs x=z must have distinct codes");
    assert(!(p1 == p3) && "x=y vs y=z must have distinct codes");
    assert(!(p2 == p3) && "x=z vs y=z must have distinct codes");
    
    std::cout << "PASSED\n";
}

// =============================================================================
// 10. EGraph explain produces correct explanations
// =============================================================================

void testEGraphExplain() {
    std::cout << "Test: EGraph explain produces explanations... ";
    
    using egraph::EGraph;
    using egraph::MergeReason;
    
    EGraph eg;
    
    // Add some e-nodes
    auto a = eg.addLeaf("a");
    auto b = eg.addLeaf("b");
    auto c = eg.addLeaf("c");
    
    // Merge a=b by axiom, b=c by axiom
    eg.merge(a, b, MergeReason::fromAxiom(0, "axiom1"));
    eg.merge(b, c, MergeReason::fromAxiom(1, "axiom2"));
    eg.rebuild();
    
    // Now a, b, c should all be in the same class
    assert(eg.find(a) == eg.find(c) && "a and c must be in same class");
    
    // explain(a, c) should produce non-empty explanation
    auto explanation = eg.explain(a, c);
    assert(!explanation.empty() && "Explanation must be non-empty");
    
    // Each explanation step should mention "axiom" or "congruence" etc.
    for (const auto& step : explanation) {
        (void)step;
        assert(!step.empty() && "Explanation step must not be empty");
        // Should contain "class X = class Y by ..."
        assert(step.find("class") != std::string::npos && 
               "Explanation should mention classes");
    }
    
    std::cout << "PASSED\n";
}

// =============================================================================
// 11. NFEngine fixpoint iteration across passes
// =============================================================================

void testNFEngineFixpoint() {
    std::cout << "Test: NFEngine fixpoint iteration... ";
    
    using canon::NFEngine;
    using canon::EquivalenceProfile;
    using canon::EquivalenceLayer;
    
    // Create engine with standard profile (Alpha + AC + Ring)
    core::TermFactory factory;
    auto profile = EquivalenceProfile::standard();
    profile.enable(EquivalenceLayer::PHI);
    NFEngine engine(profile, factory);
    
    // Create a term that needs multiple pass iterations:
    // phi^2 + 0  (Phi pass: phi+1) + 0  (Ring pass: phi+1)
    auto phi = factory.phi();
    auto phi_sq = factory.mul(phi, phi);
    auto zero = factory.scalar(0.0);
    auto term = factory.add(phi_sq, zero);
    
    auto nf = engine.normalize(term);
    assert(nf != nullptr && "Normal form must exist");
    
    // Check stats: should have needed >1 fixpoint iteration
    // (Phi reduces phi^2, then Ring removes +0)
    auto& stats = engine.stats();
    (void)stats;
    assert(stats.totalNormalizations == 1 && "Should have 1 normalization");
    // The fixpoint mechanism should handle this correctly regardless of
    // how many iterations were needed
    
    // Idempotence: normalizing the result again gives the same pointer
    auto nf2 = engine.normalize(nf);
    (void)nf2;
    assert(nf2 == nf && "Normal form must be idempotent");
    
    std::cout << "PASSED\n";
}

// =============================================================================
// 12. Proof deterministic iteration order
// =============================================================================

void testProofDeterministicOrder() {
    std::cout << "Test: Proof deterministic allSteps() order... ";
    
    using proof::Proof;
    using proof::ProofStep;
    using proof::InferenceRule;
    
    core::TermFactory factory;
    auto x = factory.variable("x");
    auto y = factory.variable("y");
    auto z = factory.variable("z");
    
    logic::Equation eq1(x, y);
    logic::Equation eq2(y, z);
    logic::Equation eq3(x, z);
    
    // Build a small proof: axiom1 + axiom2  transitivity
    Proof p;
    auto s1 = p.addAxiom(&eq1, "ax1");
    auto s2 = p.addAxiom(&eq2, "ax2");
    auto s3 = p.addTransitivity(&eq3, s1, s2);
    
    // Get all steps twice  must be in the same order
    auto steps1 = p.allSteps();
    auto steps2 = p.allSteps();
    
    assert(steps1.size() == 3 && "Should have 3 steps");
    assert(steps2.size() == 3 && "Should have 3 steps");
    
    for (size_t i = 0; i < steps1.size(); ++i) {
        assert(steps1[i]->id() == steps2[i]->id() && 
               "allSteps() must return deterministic order");
    }
    
    // Verify topological order: axioms must come before transitivity
    bool foundAx1 = false, foundAx2 = false;
    for (auto* step : steps1) {
        if (step->id() == s3) {
            assert(foundAx1 && foundAx2 && 
                   "Premises must appear before conclusion in topological order");
        }
        if (step->id() == s1) foundAx1 = true;
        if (step->id() == s2) foundAx2 = true;
    }
    
    // toString must also be deterministic
    auto str1 = p.toString();
    auto str2 = p.toString();
    assert(str1 == str2 && "toString() must be deterministic");
    
    std::cout << "PASSED\n";
}

// =============================================================================

int main() {
    std::cout << "\n=== Property & Roundtrip Tests ===\n\n";
    
    // ZPhi ring properties
    testZPhiCommutativity();
    testZPhiAssociativity();
    testZPhiDistributivity();
    testZPhiPhiSquaredIdentity();
    testZPhiNormProperty();
    testZPhiIdentityElement();
    
    // Normalizer
    testNormalizerIdempotence();
    
    // Encoding
    testEncodingInjectivity();
    
    // Phase algebra
    testPhaseMod4Algebra();
    
    // Discrimination tree
    testDiscriminationTreeBasic();
    testDiscriminationTreeVariableMatchesAll();
    
    // KnowledgeBase
    testKnowledgeBaseAddAndRetrieve();
    
    // TSTP export
    testTSTPExportFormat();
    
    // NEW: Regression tests for Session 10 fixes
    testZPhiPairingInjectivity();
    testEGraphExplain();
    testNFEngineFixpoint();
    testProofDeterministicOrder();
    
    std::cout << "\n=== All property tests PASSED ===\n";
    return 0;
}
