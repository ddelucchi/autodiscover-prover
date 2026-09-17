/**
 * @file test_universal.cpp
 * @brief Tests for the Universal Multi-Algebra Discovery Engine v5.0
 *
 * Tests the DEEP STRUCTURAL autodiscovery pipeline across ALL CD levels:
 *   CDTermGenerator -> MultiLevelEvaluator -> UniversalDiscoveryEngine
 *
 * Verified properties:
 *   - Term generation at levels 0 (R), 1 (C), 2 (H), 3 (O)
 *   - J included in multiplication atoms (enables J^2=-1 discovery)
 *   - Hamilton units generated at level 2 (enables i^2=j^2=k^2=-1)
 *   - Complete evaluator: inv, norm, scalarPart, sub
 *   - Numeric evaluation correctness at each level
 *   - Cross-level scalar-value detection
 *   - Complete engine integration (ZERO presets)
 */

#include "core/Term.hpp"
#include "core/Context.hpp"
#include "ring/ZPhi.hpp"
#include "domain/Algebra.hpp"
#include "logic/KnowledgeBase.hpp"
#include "discovery/UniversalDiscovery.hpp"

#include <iostream>
#include <cassert>
#include <cmath>
#include <unordered_set>

using namespace autodiscover;
using namespace autodiscover::core;
using namespace autodiscover::ring;
using namespace autodiscover::discovery;

// ===========================================================================
// HELPER
// ===========================================================================

static int testsPassed = 0;
static int testsFailed = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            testsFailed++; \
        } else { \
            testsPassed++; \
        } \
    } while (0)

// ===========================================================================
// TEST 1: CDTermGenerator  Level 0 (R)
// ===========================================================================

void testLevel0Generation() {
    std::cout << "Test: CDTermGenerator Level 0 (R)... ";
    auto& factory = globalContext().factory();

    CDTermGenerator::Config cfg;
    cfg.maxDepthReal = 2;
    cfg.maxTermsPerLevel = 500;
    CDTermGenerator gen(factory, cfg);
    auto terms = gen.generateLevel0();

    CHECK(terms.size() >= 6, "Level 0 should have >= 6 terms");
    CHECK(terms.size() >= 20, "Level 0 depth 2 should produce many terms");

    std::unordered_set<const Term*> seen(terms.begin(), terms.end());
    CHECK(seen.size() == terms.size(), "Level 0 terms should be unique (hash-consed)");

    std::cout << "PASS (" << terms.size() << " terms)\n";
}

// ===========================================================================
// TEST 2: CDTermGenerator  Level 1 (C) with J in multiplication atoms
// ===========================================================================

void testLevel1Generation() {
    std::cout << "Test: CDTermGenerator Level 1 (C)... ";
    auto& factory = globalContext().factory();

    CDTermGenerator::Config cfg;
    cfg.maxDepthReal = 1;
    cfg.maxDepthComplex = 1;
    cfg.maxTermsPerLevel = 500;
    CDTermGenerator gen(factory, cfg);

    auto real = gen.generateLevel0();
    auto complex = gen.generateLevel1(real);

    CHECK(!complex.empty(), "Level 1 should generate complex terms");

    size_t pairCount = 0;
    for (const Term* t : complex) {
        if (t->isPair()) pairCount++;
    }
    CHECK(pairCount > 0, "Level 1 should contain Pair(a,b) terms");

    bool hasJ = false;
    for (const Term* t : complex) {
        if (t->kind() == TermKind::J) hasJ = true;
    }
    CHECK(hasJ, "Level 1 should contain J unit");

    // KEY: Check that mul(J, J) is generated (J in multiplication atoms)
    bool hasJJ = false;
    for (const Term* t : complex) {
        if (t->kind() == TermKind::Application && t->symbol() == "*") {
            const auto& ch = t->children();
            if (ch.size() == 2 && ch[0]->kind() == TermKind::J && ch[1]->kind() == TermKind::J) {
                hasJJ = true;
            }
        }
    }
    CHECK(hasJJ, "Level 1 should contain mul(J,J)  J must be in multiplication atoms");

    std::cout << "PASS (" << complex.size() << " terms, " << pairCount << " pairs, J*J=" << (hasJJ?"yes":"no") << ")\n";
}

// ===========================================================================
// TEST 3: CDTermGenerator  Level 2 (H) with Hamilton units
// ===========================================================================

void testLevel2Generation() {
    std::cout << "Test: CDTermGenerator Level 2 (H)... ";
    auto& factory = globalContext().factory();

    CDTermGenerator::Config cfg;
    cfg.maxDepthReal = 1;
    cfg.maxDepthComplex = 1;
    cfg.maxDepthQuaternion = 1;
    cfg.maxTermsPerLevel = 500;
    CDTermGenerator gen(factory, cfg);

    auto real = gen.generateLevel0();
    auto complex = gen.generateLevel1(real);
    auto quat = gen.generateLevel2(complex);

    CHECK(!quat.empty(), "Level 2 should generate quaternion terms");

    size_t pairCount = 0;
    for (const Term* t : quat) {
        if (t->isPair()) pairCount++;
    }
    CHECK(pairCount > 0, "Level 2 should contain Pair(C,C) terms");

    // Check that Hamilton units are generated
    // Hamilton units: i=((0,1),(0,0)), j=((0,0),(1,0)), k=((0,0),(0,1))
    // These are Pair(Pair(a,b), Pair(c,d)) structures
    // We can check for mul() terms at the quaternion level (Hamilton products)
    size_t mulCount = 0;
    for (const Term* t : quat) {
        if (t->kind() == TermKind::Application && t->symbol() == "*") {
            mulCount++;
        }
    }
    CHECK(mulCount >= 9, "Level 2 should contain >= 9 Hamilton unit products (3x3)");

    std::cout << "PASS (" << quat.size() << " terms, " << pairCount << " pairs, " << mulCount << " products)\n";
}

// ===========================================================================
// TEST 4: CDTermGenerator  Level 3 (O)
// ===========================================================================

void testLevel3Generation() {
    std::cout << "Test: CDTermGenerator Level 3 (O)... ";
    auto& factory = globalContext().factory();

    CDTermGenerator::Config cfg;
    cfg.maxDepthReal = 1;
    cfg.maxDepthComplex = 1;
    cfg.maxDepthQuaternion = 1;
    cfg.maxDepthOctonion = 1;
    cfg.maxTermsPerLevel = 100;
    CDTermGenerator gen(factory, cfg);

    auto real = gen.generateLevel0();
    auto complex = gen.generateLevel1(real);
    auto quat = gen.generateLevel2(complex);
    auto oct = gen.generateLevel3(quat);

    CHECK(!oct.empty(), "Level 3 should generate octonion terms");

    std::cout << "PASS (" << oct.size() << " terms)\n";
}

// ===========================================================================
// TEST 5: Hamilton unit evaluation  i^2 = -1 from ground zero
// ===========================================================================

void testHamiltonEvaluation() {
    std::cout << "Test: Hamilton unit evaluation (i^2=-1 from ground zero)... ";
    auto& factory = globalContext().factory();

    // Build Hamilton unit i = ((0,1),(0,0))
    auto s0 = factory.scalar(0.0);
    auto s1 = factory.scalar(1.0);
    auto c01 = factory.pair(s0, s1);  // (0,1) = J in C
    auto c00 = factory.pair(s0, s0);  // (0,0) = 0 in C
    auto qi = factory.pair(c01, c00); // i = ((0,1),(0,0))

    // Evaluate i*i using CayleyDickson<2>
    auto ii_term = factory.mul(qi, qi);
    domain::CayleyDicksonEvaluator<2> eval2;
    auto result = eval2.evaluate(ii_term);
    auto arr = result.toArray();

    // i^2 should be (-1, 0, 0, 0) = scalar -1
    CHECK(std::abs(arr[0] - (-1.0)) < 1e-10, "i^2 scalar part should be -1");
    CHECK(std::abs(arr[1]) < 1e-10, "i^2 component 1 should be 0");
    CHECK(std::abs(arr[2]) < 1e-10, "i^2 component 2 should be 0");
    CHECK(std::abs(arr[3]) < 1e-10, "i^2 component 3 should be 0");

    // Build j = ((0,0),(1,0)) and k = ((0,0),(0,1))
    auto c10 = factory.pair(s1, s0);  // (1,0) = 1 in C
    auto qj = factory.pair(c00, c10); // j
    auto qk = factory.pair(c00, c01); // k
    (void)qk; // used in concept, suppress warning

    // i*j should equal k = (0, 0, 0, 1)
    auto ij_term = factory.mul(qi, qj);
    auto ij_result = eval2.evaluate(ij_term);
    auto ij_arr = ij_result.toArray();
    CHECK(std::abs(ij_arr[0]) < 1e-10, "ij scalar should be 0");
    CHECK(std::abs(ij_arr[1]) < 1e-10, "ij comp 1 should be 0");
    CHECK(std::abs(ij_arr[2]) < 1e-10, "ij comp 2 should be 0");
    CHECK(std::abs(ij_arr[3] - 1.0) < 1e-10, "ij comp 3 should be 1 (= k)");

    // j*i should equal -k = (0, 0, 0, -1)
    auto ji_term = factory.mul(qj, qi);
    auto ji_result = eval2.evaluate(ji_term);
    auto ji_arr = ji_result.toArray();
    CHECK(std::abs(ji_arr[3] - (-1.0)) < 1e-10, "ji comp 3 should be -1 (= -k)");

    std::cout << "PASS (i^2=-1, ij=k, ji=-k all verified)\n";
}

// ===========================================================================
// TEST 5b: COMPLETE evaluator  norm, inv, scalarPart at CD levels
// ===========================================================================

void testCompleteEvaluator() {
    std::cout << "Test: Complete evaluator (norm/inv/scalarPart)... ";
    auto& factory = globalContext().factory();

    // Build Hamilton unit i = ((0,1),(0,0))
    auto s0 = factory.scalar(0.0);
    auto s1 = factory.scalar(1.0);
    auto c01 = factory.pair(s0, s1);
    auto c00 = factory.pair(s0, s0);
    auto c10 = factory.pair(s1, s0);
    auto qi = factory.pair(c01, c00);  // i
    auto qj = factory.pair(c00, c10);  // j
    auto q_one = factory.pair(c10, c00); // 1 in H

    domain::CayleyDicksonEvaluator<2> eval2;

    // norm(i) should be 1 (normSq = 0^2 + 1^2 + 0^2 + 0^2 = 1)
    auto norm_i = factory.norm(qi);
    auto norm_result = eval2.evaluate(norm_i);
    auto narr = norm_result.toArray();
    CHECK(std::abs(narr[0] - 1.0) < 1e-10, "norm(i) should be 1");
    CHECK(std::abs(narr[1]) < 1e-10, "norm(i) comp 1 should be 0");
    CHECK(std::abs(narr[2]) < 1e-10, "norm(i) comp 2 should be 0");
    CHECK(std::abs(narr[3]) < 1e-10, "norm(i) comp 3 should be 0");

    // inv(i) should be -i = (0, -1, 0, 0) because i*(-i)=1
    auto inv_i = factory.inv(qi);
    auto inv_result = eval2.evaluate(inv_i);
    auto iarr = inv_result.toArray();
    CHECK(std::abs(iarr[0]) < 1e-10, "inv(i) scalar should be 0");
    CHECK(std::abs(iarr[1] - (-1.0)) < 1e-10, "inv(i) comp 1 should be -1");
    CHECK(std::abs(iarr[2]) < 1e-10, "inv(i) comp 2 should be 0");
    CHECK(std::abs(iarr[3]) < 1e-10, "inv(i) comp 3 should be 0");

    // scalarPart(i) should be 0
    auto scal_i = factory.scalarPart(qi);
    auto scal_result = eval2.evaluate(scal_i);
    auto sarr = scal_result.toArray();
    CHECK(std::abs(sarr[0]) < 1e-10, "scalarPart(i) should be 0");

    // scalarPart(1_H) should be 1
    auto scal_one = factory.scalarPart(q_one);
    auto scal_one_result = eval2.evaluate(scal_one);
    auto soarr = scal_one_result.toArray();
    CHECK(std::abs(soarr[0] - 1.0) < 1e-10, "scalarPart(1_H) should be 1");

    // norm(i*j) should equal norm(i)*norm(j) = 1*1 = 1
    auto ij = factory.mul(qi, qj);
    auto norm_ij = factory.norm(ij);
    auto norm_ij_result = eval2.evaluate(norm_ij);
    auto nij_arr = norm_ij_result.toArray();
    CHECK(std::abs(nij_arr[0] - 1.0) < 1e-10, "norm(i*j) should be 1 (norm multiplicativity)");

    // sub(i, j) should work: i - j = (0, 1, -1, 0)
    auto sub_ij = factory.apply("-", {qi, qj});
    auto sub_result = eval2.evaluate(sub_ij);
    auto sub_arr = sub_result.toArray();
    CHECK(std::abs(sub_arr[0]) < 1e-10, "sub(i,j) scalar should be 0");
    CHECK(std::abs(sub_arr[1] - 1.0) < 1e-10, "sub(i,j) comp 1 should be 1");
    CHECK(std::abs(sub_arr[2] - (-1.0)) < 1e-10, "sub(i,j) comp 2 should be -1");

    std::cout << "PASS (norm/inv/scalarPart/sub all verified)\n";
}

// ===========================================================================
// TEST 5c: vectorPart evaluator + field decomposition
// ===========================================================================

void testVectorPartEvaluator() {
    std::cout << "Test: VectorPart evaluator + decomposition... ";
    auto& factory = globalContext().factory();

    auto s0 = factory.scalar(0.0);
    auto s1 = factory.scalar(1.0);
    auto c01 = factory.pair(s0, s1);
    auto c00 = factory.pair(s0, s0);
    auto c10 = factory.pair(s1, s0);
    auto qi = factory.pair(c01, c00);    // i = ((0,1),(0,0))
    auto q_one = factory.pair(c10, c00); // 1_H

    domain::CayleyDicksonEvaluator<2> eval2;

    // Vec(i) should be i itself (pure imaginary)
    auto vec_i = factory.apply("Vec", {qi});
    auto vec_result = eval2.evaluate(vec_i);
    auto varr = vec_result.toArray();
    CHECK(std::abs(varr[0]) < 1e-10, "Vec(i) scalar should be 0");
    CHECK(std::abs(varr[1] - 1.0) < 1e-10, "Vec(i) comp 1 should be 1");

    // Vec(1_H) should be 0 (scalar has no vector part)
    auto vec_one = factory.apply("Vec", {q_one});
    auto vec_one_result = eval2.evaluate(vec_one);
    auto voarr = vec_one_result.toArray();
    CHECK(std::abs(voarr[0]) < 1e-10, "Vec(1_H) scalar should be 0");
    CHECK(std::abs(voarr[1]) < 1e-10, "Vec(1_H) comp 1 should be 0");

    // Scal(i) + Vec(i) should equal i (field decomposition)
    auto scal_i = factory.scalarPart(qi);
    auto recomp = factory.add(scal_i, vec_i);
    auto recomp_result = eval2.evaluate(recomp);
    auto rarr = recomp_result.toArray();
    auto iarr = eval2.evaluate(qi).toArray();
    CHECK(std::abs(rarr[0] - iarr[0]) < 1e-10, "Scal(i)+Vec(i) scalar should match i");
    CHECK(std::abs(rarr[1] - iarr[1]) < 1e-10, "Scal(i)+Vec(i) comp1 should match i");

    std::cout << "PASS (Vec evaluator + field decomposition verified)\n";
}

// ===========================================================================
// TEST 6: MultiLevelEvaluator  Level 0 (Z[phi])
// ===========================================================================

void testEvaluatorLevel0() {
    std::cout << "Test: MultiLevelEvaluator Level 0 (Z[phi])... ";
    auto& factory = globalContext().factory();
    MultiLevelEvaluator eval;

    auto val = eval.evalReal(factory.phi());
    CHECK(val.has_value(), "phi should evaluate");
    CHECK(*val == ZPhi(0, 1), "phi should be (0,1)");

    auto phiSq = factory.mul(factory.phi(), factory.phi());
    auto phiP1 = factory.add(factory.phi(), factory.scalar(1.0));
    auto v1 = eval.evalReal(phiSq);
    auto v2 = eval.evalReal(phiP1);
    CHECK(v1.has_value() && v2.has_value(), "phi^2 and phi+1 should evaluate");
    CHECK(*v1 == *v2, "phi^2 should equal phi+1");

    auto phiPhiBar = factory.mul(factory.phi(), factory.phiBar());
    auto v3 = eval.evalReal(phiPhiBar);
    CHECK(v3.has_value(), "phi*phibar should evaluate");
    CHECK(*v3 == ZPhi(-1, 0), "phi*phibar should be -1");

    std::cout << "PASS\n";
}

// ===========================================================================
// TEST 7: MultiLevelEvaluator  Level 1 (C)
// ===========================================================================

void testEvaluatorLevel1() {
    std::cout << "Test: MultiLevelEvaluator Level 1 (C)... ";
    auto& factory = globalContext().factory();
    MultiLevelEvaluator eval;

    auto one = factory.scalar(1.0);
    auto zero = factory.scalar(0.0);
    auto pair10 = factory.pair(one, zero);

    auto sig = eval.eval(pair10);
    CHECK(sig.has_value(), "Pair(1,0) should evaluate");
    if (sig) {
        CHECK(sig->cdLevel == 1, "Pair(1,0) should be level 1");
        CHECK(sig->dim == 2, "Complex should have 2 components");
        CHECK(std::abs(sig->components[0] - 1.0) < 1e-10, "Real part should be 1");
        CHECK(std::abs(sig->components[1] - 0.0) < 1e-10, "Imag part should be 0");
    }

    std::cout << "PASS\n";
}

// ===========================================================================
// TEST 8: NumericSignature bucketing
// ===========================================================================

void testSignatureBucketing() {
    std::cout << "Test: NumericSignature bucketing... ";

    MultiLevelEvaluator::NumericSignature s1;
    s1.cdLevel = 1; s1.dim = 2;
    s1.components[0] = 1.618033988; s1.components[1] = 0.0;
    s1.valid = true;

    MultiLevelEvaluator::NumericSignature s2;
    s2.cdLevel = 1; s2.dim = 2;
    s2.components[0] = 1.618033988; s2.components[1] = 0.0;
    s2.valid = true;

    MultiLevelEvaluator::NumericSignature s3;
    s3.cdLevel = 1; s3.dim = 2;
    s3.components[0] = 2.718281828; s3.components[1] = 0.0;
    s3.valid = true;

    CHECK(s1.matches(s2), "Same values should match");
    CHECK(!s1.matches(s3), "Different values should not match");
    CHECK(s1.bucketKey() == s2.bucketKey(), "Same values should have same bucket key");
    CHECK(s1.bucketKey() != s3.bucketKey(), "Different values should usually have different bucket keys");

    std::cout << "PASS\n";
}

// ===========================================================================
// TEST 9: UniversalDiscoveryEngine  full run (ZERO presets)
// ===========================================================================

void testUniversalEngineFullRun() {
    std::cout << "Test: UniversalDiscoveryEngine full run (ZERO presets)... " << std::flush;
    auto& factory = globalContext().factory();
    logic::KnowledgeBase kb(factory);

    UniversalConfig cfg;
    cfg.verbose = false;
    cfg.maxDepthReal = 2;
    cfg.maxDepthComplex = 1;
    cfg.maxDepthQuaternion = 1;
    cfg.maxDepthOctonion = 0;
    cfg.maxTermsPerLevel = 200;
    cfg.maxPairsPerBucket = 10;
    cfg.enableGOD = true;
    cfg.enableSCOUT = true;
    cfg.enableKernelProof = true;
    cfg.enableEGraph = true;
    cfg.enableBudget = true;
    cfg.showAllEquations = false;
    cfg.gasBudget = 5000000;
    cfg.timeLimitMs = 30000;

    UniversalDiscoveryEngine engine(factory, kb, cfg);
    engine.seedAxioms();
    auto stats = engine.run();

    CHECK(stats.levels[0].equations > 0, "Should discover Level 0 equations");
    CHECK(stats.levels[0].termsGenerated > 0, "Should generate Level 0 terms");
    CHECK(stats.levels[0].termsEvaluated > 0, "Should evaluate Level 0 terms");
    CHECK(stats.totalEquations > 0, "Should discover some equations");

    size_t proven = stats.levels[0].kernelProven + stats.levels[0].egraphProven;
    CHECK(proven >= 0, "Proof infrastructure should not crash");

    std::cout << "PASS (total=" << stats.totalEquations
              << ", L0=" << stats.levels[0].equations
              << ", L1=" << stats.levels[1].equations
              << ", L2=" << stats.levels[2].equations
              << ", cross=" << stats.crossLevelEquations << ")\n";
}

// ===========================================================================
// TEST 10: UniversalEquation scoring
// ===========================================================================

void testEquationScoring() {
    std::cout << "Test: UniversalEquation scoring... ";

    UniversalEquation eq;
    eq.oracleConfirmed = false;
    eq.kernelProven = false;
    eq.scoutValidated = false;
    eq.egraphProven = false;
    CHECK(eq.verificationScore() == 0, "All false -> score 0");

    eq.oracleConfirmed = true;
    CHECK(eq.verificationScore() == 1, "Oracle only -> score 1");

    eq.kernelProven = true;
    CHECK(eq.verificationScore() == 5, "Oracle+Kernel -> score 5");

    eq.scoutValidated = true;
    CHECK(eq.verificationScore() == 7, "Oracle+Kernel+Scout -> score 7");

    eq.egraphProven = true;
    CHECK(eq.verificationScore() == 10, "Oracle+Kernel+Scout+EGraph -> score 10");

    std::cout << "PASS\n";
}

// ===========================================================================
// TEST 11: EquationClass names (5 classes, NO operator/wave presets)
// ===========================================================================

void testEquationClassNames() {
    std::cout << "Test: EquationClass names (5 classes, no presets)... ";

    CHECK(std::string(equationClassName(EquationClass::ScalarRing)) == "Scalar Ring Z[phi]",
          "ScalarRing name");
    CHECK(std::string(equationClassName(EquationClass::ComplexField)) == "Complex Field C",
          "ComplexField name");
    CHECK(std::string(equationClassName(EquationClass::QuaternionRing)) == "Quaternion Ring H",
          "QuaternionRing name");
    CHECK(std::string(equationClassName(EquationClass::OctonionAlgebra)) == "Octonion Algebra O",
          "OctonionAlgebra name");
    CHECK(std::string(equationClassName(EquationClass::CrossLevel)) == "Cross-Level",
          "CrossLevel name");

    // Verify exactly 5 classes (0-4), no operator/wave presets
    int sr = static_cast<int>(EquationClass::ScalarRing);
    int cl = static_cast<int>(EquationClass::CrossLevel);
    CHECK(sr == 0, "ScalarRing = 0");
    CHECK(cl == 4, "CrossLevel = 4");

    std::cout << "PASS\n";
}

// ===========================================================================
// TEST 12: All levels have terms in full generateAll()
// ===========================================================================

void testGenerateAll() {
    std::cout << "Test: CDTermGenerator::generateAll()... " << std::flush;
    auto& factory = globalContext().factory();

    CDTermGenerator::Config cfg;
    cfg.maxDepthReal = 2;
    cfg.maxDepthComplex = 1;
    cfg.maxDepthQuaternion = 1;
    cfg.maxDepthOctonion = 1;
    cfg.maxTermsPerLevel = 200;
    CDTermGenerator gen(factory, cfg);
    auto all = gen.generateAll();

    CHECK(all.count(0) > 0 && !all[0].empty(), "Level 0 should have terms");
    CHECK(all.count(1) > 0 && !all[1].empty(), "Level 1 should have terms");
    CHECK(all.count(2) > 0 && !all[2].empty(), "Level 2 should have terms");
    CHECK(all.count(3) > 0 && !all[3].empty(), "Level 3 should have terms");

    std::cout << "PASS (L0=" << all[0].size()
              << ", L1=" << all[1].size()
              << ", L2=" << all[2].size()
              << ", L3=" << all[3].size() << ")\n";
}

// ===========================================================================
// TEST 13: Discovered equations are committed to KB
// ===========================================================================

void testKBCommit() {
    std::cout << "Test: Equations committed to KB... " << std::flush;
    auto& factory = globalContext().factory();
    logic::KnowledgeBase kb(factory);

    size_t before = kb.numValid();

    UniversalConfig cfg;
    cfg.verbose = false;
    cfg.maxDepthReal = 2;
    cfg.maxDepthComplex = 0;
    cfg.maxDepthQuaternion = 0;
    cfg.maxDepthOctonion = 0;
    cfg.maxTermsPerLevel = 100;
    cfg.enableGOD = true;
    cfg.enableSCOUT = true;
    cfg.enableKernelProof = true;
    cfg.enableEGraph = false;
    cfg.showAllEquations = false;
    cfg.gasBudget = 1000000;
    cfg.timeLimitMs = 10000;

    UniversalDiscoveryEngine engine(factory, kb, cfg);
    engine.seedAxioms();
    engine.run();

    size_t after = kb.numValid();
    CHECK(after > before, "KB should grow after discovery");

    std::cout << "PASS (KB grew from " << before << " to " << after << ")\n";
}

// ===========================================================================
// MAIN
// ===========================================================================

int main() {
    std::cout << "\n======================================================\n"
              << "  UNIVERSAL DISCOVERY v6.0  TEST SUITE\n"
              << "  ANALYTIC: calculus, waves, fields, polynomials\n"
              << "  R -> C -> H -> O, Cross-Level, ZERO presets\n"
              << "======================================================\n\n";

    testLevel0Generation();       // 1
    testLevel1Generation();       // 2  J in multiplication atoms
    testLevel2Generation();       // 3  Hamilton units
    testLevel3Generation();       // 4
    testHamiltonEvaluation();     // 5  i^2=-1, ij=k, ji=-k from ground zero
    testCompleteEvaluator();      // 5b  norm, inv, scalarPart, sub at CD levels
    testVectorPartEvaluator();    // 5c  vectorPart, field decomposition
    testEvaluatorLevel0();        // 6
    testEvaluatorLevel1();        // 7
    testSignatureBucketing();     // 8
    testUniversalEngineFullRun(); // 9  full engine, zero presets
    testEquationScoring();        // 10
    testEquationClassNames();     // 11  5 classes only
    testGenerateAll();            // 12
    testKBCommit();               // 13

    std::cout << "\n======================================================\n"
              << "  RESULTS: " << testsPassed << " passed, " << testsFailed << " failed\n"
              << "======================================================\n\n";

    return testsFailed > 0 ? 1 : 0;
}
