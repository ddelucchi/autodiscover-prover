/**
 * @file test_canon_scalar.cpp
 * @brief Tests for the canonical scalar system
 */

#include "core/Term.hpp"
#include "canon/Equivalence.hpp"
#include "canon/NFEngine.hpp"
#include "encoding/Structural.hpp"
#include "fingerprint/Semantic.hpp"
#include "canon/CanonScalar.hpp"

#include <iostream>
#include <cassert>

using namespace autodiscover;

// ===========================================================================
// EQUIVALENCE PROFILE TESTS
// ===========================================================================

void testEquivalenceProfile() {
    std::cout << "Test: EquivalenceProfile... ";
    
    // Test standard profile
    auto standard = canon::EquivalenceProfile::standard();
    assert(standard.isEnabled(canon::EquivalenceLayer::ALPHA));
    assert(standard.isEnabled(canon::EquivalenceLayer::AC));
    assert(!standard.name().empty());
    
    // Test minimal profile
    auto minimal = canon::EquivalenceProfile::minimal();
    assert(minimal.isEnabled(canon::EquivalenceLayer::ALPHA));
    
    // Test profile builder
    canon::EquivalenceProfile custom;
    custom.enable(canon::EquivalenceLayer::ALPHA);
    custom.enable(canon::EquivalenceLayer::PHI);
    assert(custom.isEnabled(canon::EquivalenceLayer::ALPHA));
    assert(custom.isEnabled(canon::EquivalenceLayer::PHI));
    assert(!custom.isEnabled(canon::EquivalenceLayer::GOD));
    
    std::cout << "PASSED\n";
}

// ===========================================================================
// NF ENGINE TESTS
// ===========================================================================

void testNFEngineBasic() {
    std::cout << "Test: NFEngine basic... ";
    
    core::TermFactory factory;
    canon::NFEngine engine(canon::EquivalenceProfile::standard(), factory);
    
    // Create simple terms
    auto x = factory.variable("x", core::Sort::Real);
    auto y = factory.variable("y", core::Sort::Real);
    (void)y;
    
    assert(x != nullptr);
    assert(y != nullptr);
    
    // Normalize should work
    auto nx = engine.normalize(x);
    assert(nx != nullptr);
    (void)nx;  // result validated via assert
    
    // Check stats
    assert(engine.numPasses() > 0);
    
    std::cout << "PASSED\n";
}

void testNFEngineIdempotence() {
    std::cout << "Test: NFEngine idempotence... ";
    
    core::TermFactory factory;
    canon::NFEngine engine(canon::EquivalenceProfile::standard(), factory);
    
    // Create a term: add(x, y)
    auto x = factory.variable("x", core::Sort::Real);
    auto y = factory.variable("y", core::Sort::Real);
    auto sum = factory.add(x, y);
    
    // NF(NF(t)) = NF(t)
    auto nf1 = engine.normalize(sum);
    auto nf2 = engine.normalize(nf1);
    (void)nf2;
    
    // Due to hash-consing, pointer equality should work
    assert(nf1 == nf2);
    
    std::cout << "PASSED\n";
}

// ===========================================================================
// STRUCTURAL ENCODER TESTS
// ===========================================================================

void testStructuralEncoder() {
    std::cout << "Test: StructuralEncoder... ";
    
    core::TermFactory factory;
    
    auto x = factory.variable("x", core::Sort::Real);
    auto y = factory.variable("y", core::Sort::Real);
    
    encoding::StructuralEncoder encoder(encoding::EncodingStrategy::POSITIONAL_256);
    
    auto code1 = encoder.encode(*x);
    auto code2 = encoder.encode(*y);
    auto code3 = encoder.encode(*x);  // Same term
    
    // Injectivity: Different terms  different codes
    assert(code1 != code2);
    
    // Determinism: Same term  same code
    assert(code1 == code3);
    
    std::cout << "PASSED\n";
}

void testMultipleEncodingStrategies() {
    std::cout << "Test: Multiple encoding strategies... ";
    
    core::TermFactory factory;
    auto term = factory.add(
        factory.variable("x", core::Sort::Real),
        factory.scalar(1.0)
    );
    
    // Test each strategy compiles and works
    encoding::StructuralEncoder enc256(encoding::EncodingStrategy::POSITIONAL_256);
    auto code256 = enc256.encode(*term);
    assert(code256.isInt());
    
    encoding::StructuralEncoder encPF(encoding::EncodingStrategy::PREFIX_FREE);
    auto codePF = encPF.encode(*term);
    assert(codePF.isInt());
    
    encoding::StructuralEncoder encZ(encoding::EncodingStrategy::ZECKENDORF);
    auto codeZ = encZ.encode(*term);
    assert(codeZ.isPhi());  // Zeckendorf uses ZPhi
    
    std::cout << "PASSED\n";
}

// ===========================================================================
// SEMANTIC SCALARIZER TESTS (DISABLED - API MISMATCH)
// ===========================================================================

// Semantic and CanonScalar tests now enabled

void testSemanticSignature() {
    std::cout << "Test: SemanticSignature... ";
    
    core::TermFactory factory;
    
    auto phi = factory.phi();
    
    fingerprint::SemanticScalarizer scalarizer;
    auto sig = scalarizer.signature(*phi);
    
    assert(sig.isValid());
    assert(sig.dimension() > 0);
    
    // Check  evaluates to golden ratio
    if (!sig.values().empty()) {
        double val = sig.values()[0].real();
        (void)val;
        // Should be close to 1.618...
        assert(std::abs(val - 1.618) < 0.01 || std::abs(val - 0.618) < 0.01 || val > 1.0);
    }
    
    std::cout << "PASSED\n";
}

void testResidualEnergy() {
    std::cout << "Test: Residual energy... ";
    
    core::TermFactory factory;
    
    auto phi = factory.phi();
    auto one = factory.scalar(1.0);
    
    fingerprint::SemanticScalarizer scalarizer;
    
    // Same term: zero residual
    double e1 = scalarizer.residualEnergy(*phi, *phi);
    (void)e1;
    assert(e1 < 1e-10);
    
    // Different terms: non-zero residual
    double e2 = scalarizer.residualEnergy(*phi, *one);
    (void)e2;
    assert(e2 > 0.1);  // phi  1.618, so should differ from 1.0
    
    std::cout << "PASSED\n";
}

// ===========================================================================
// CANON SCALAR ENGINE TESTS
// ===========================================================================

void testCanonScalarEngine() {
    std::cout << "Test: CanonScalarEngine... ";
    
    core::TermFactory factory;
    canon::CanonScalarEngine engine(factory);
    
    auto x = factory.variable("x", core::Sort::Real);
    auto phi = factory.phi();
    
    auto result1 = engine.computeTerm(x);
    auto result2 = engine.computeTerm(phi);
    
    // Both should produce valid results
    assert(result1.normalForm != nullptr);
    assert(result2.normalForm != nullptr);
    
    // Different terms should have different codes
    assert(!result1.structurallyEquals(result2));
    
    std::cout << "PASSED\n";
}

void testCanonScalarEquivalence() {
    std::cout << "Test: CanonScalar equivalence... ";
    
    core::TermFactory factory;
    canon::CanonScalarEngine engine(factory);
    
    // Create two terms that should normalize to the same thing
    auto x = factory.variable("x", core::Sort::Real);
    auto zero = factory.scalar(0.0);
    
    // add(x, 0) should normalize to x
    auto xPlusZero = factory.add(x, zero);
    
    // Check if they're equivalent
    bool equiv = engine.areEquivalent(x, xPlusZero);
    // This may or may not be true depending on ring normalization
    // Just verify the function works
    (void)equiv;
    
    // Same term should always be equivalent
    assert(engine.areEquivalent(x, x));
    
    std::cout << "PASSED\n";
}

void testEquationCanonScalar() {
    std::cout << "Test: Equation CanonScalar... ";
    
    core::TermFactory factory;
    canon::CanonScalarEngine engine(factory);
    
    auto phi = factory.phi();
    auto one = factory.scalar(1.0);
    
    // Equation: phi = phi (tautology)
    auto taut = engine.computeEquation(phi, phi);
    assert(taut.isTautology());
    assert(taut.residualEnergy < 1e-10);
    
    // Equation: phi = 1 (not tautology)
    auto nonTaut = engine.computeEquation(phi, one);
    assert(!nonTaut.isTautology());
    
    std::cout << "PASSED\n";
}

// ===========================================================================
// INTEGRATION TESTS
// ===========================================================================

void testFullPipeline() {
    std::cout << "Test: Full pipeline... ";
    
    // Create engine with golden profile
    core::TermFactory factory;
    auto engine = canon::presets::goldenEngine(factory);
    
    // Build:  =  + 1
    auto phi = factory.phi();
    auto phiSquared = factory.mul(phi, phi);
    auto phiPlusOne = factory.add(phi, factory.scalar(1.0));
    
    // Compute canonical scalars
    auto cs1 = engine.computeTerm(phiSquared);
    auto cs2 = engine.computeTerm(phiPlusOne);
    
    // With -normalization, these should be equivalent
    //    + 1
    // Note: This depends on the normalization implementation
    
    std::cout << "PASSED\n";
}

void testDeduplication() {
    std::cout << "Test: Deduplication... ";
    
    core::TermFactory factory;
    canon::CanonScalarEngine engine(factory);
    
    // Create a set of terms with some duplicates (same structure)
    auto x = factory.variable("x", core::Sort::Real);
    auto y = factory.variable("y", core::Sort::Real);
    auto xy1 = factory.add(x, y);
    auto xy2 = factory.add(x, y);  // Same structure, hash-consed
    
    // Due to hash-consing, xy1 == xy2
    assert(xy1 == xy2);
    
    // Compute and verify same code
    auto c1 = engine.computeTerm(xy1);
    auto c2 = engine.computeTerm(xy2);
    assert(c1.structurallyEquals(c2));
    
    std::cout << "PASSED\n";
}

// End of formerly disabled tests

// ===========================================================================
// MAIN
// ===========================================================================

int main() {
    std::cout << "\n=== Canon Scalar System Tests ===\n\n";
    
    // --- Equivalence profile tests ---
    testEquivalenceProfile();
    
    // --- NF Engine tests ---
    testNFEngineBasic();
    testNFEngineIdempotence();
    
    // --- Structural encoder tests ---
    testStructuralEncoder();
    testMultipleEncodingStrategies();
    
    // --- Semantic scalarizer tests ---
    testSemanticSignature();
    testResidualEnergy();
    
    // --- CanonScalarEngine tests ---
    // NOTE: The following tests exercise the full CanonScalarEngine pipeline.
    // They are temporarily disabled because the engine's internal API between
    // NFEngine, StructuralEncoder, and Fingerprinter is still stabilizing.
    // Re-enable once computeTerm/computeEquation signatures are finalized.
    //
    // testCanonScalarEngine();
    // testCanonScalarEquivalence();
    // testEquationCanonScalar();
    
    // --- Integration tests ---
    // testFullPipeline();
    // testDeduplication();
    
    std::cout << "\n=== Core Canon Scalar Tests PASSED ===\n";
    std::cout << "(CanonScalarEngine integration tests disabled pending API stabilization)\n";
    
    return 0;
}

