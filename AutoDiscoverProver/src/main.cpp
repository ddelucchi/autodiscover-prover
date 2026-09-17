/**
 * @file main.cpp
 * @brief Main entry point for the Autodiscovery Equational Prover
 * 
 * The Autodiscovery Equational Prover with SCOUT Integration
 * 
 * This prover implements:
 * - Cayley-Dickson algebra tower (          ...)
 * - GOD (Groupoid-Orbit Determinizer) canonicalization
 * - Golden ratio () fixed-point detection
 * - SCOUT phase transport operations
 * - Superposition-based equational reasoning
 * - DAG-structured proof generation with verification
 * 
 * Based on: "The Field Equations of Ontologies"
 *           (The Geometrization of Logic, all of Mathematics, and Thus their Physics)
 */

#include "core/Term.hpp"
#include "core/Type.hpp"
#include "core/Context.hpp"
#include "core/SymbolTable.hpp"
#include "logic/Equation.hpp"
#include "logic/KnowledgeBase.hpp"
#include "logic/MatcherUnifier.hpp"
#include "logic/Normalizer.hpp"
#include "logic/InferenceEngine.hpp"
#include "domain/Algebra.hpp"
#include "domain/Scout.hpp"
#include "domain/Closure.hpp"
#include "domain/MultiRingEval.hpp"
#include "domain/CayleyLambda.hpp"
#include "domain/SigmaDerivation.hpp"
#include "domain/EquivariantCohomology.hpp"
#include "domain/HybridCalculus.hpp"
#include "proof/Proof.hpp"
#include "proof/ProofChecker.hpp"
#include "proof/CertificateKernel.hpp"
#include "egraph/EGraph.hpp"
#include "egraph/RuleMiner.hpp"
#include "fingerprint/DeltaEngine.hpp"
#include "ring/ZeckendorfBigInt.hpp"
#include "discovery/ScalarDiscovery.hpp"
#include "discovery/UniversalDiscovery.hpp"

#include <iostream>
#include <string>
#include <memory>

namespace ad = autodiscover;

/**
 * @brief Print banner
 */
void printBanner() {
    std::cout << "\n"
        "================================================================\n"
        "    AUTODISCOVERY EQUATIONAL PROVER v14.0\n"
        "    INTERSTICE ORBIT + UNIVERSAL DISCOVERY ENGINE\n"
        "----------------------------------------------------------------\n"
        "    Cayley-Dickson Tower:  R -> C -> H -> O -> S\n"
        "    Interstice Orbit:      Orbit-evaluated D_{g,chi} on X\n"
        "                           Domain: X = (Sigma x S^1) x T\n"
        "                           UFE: Box Phi = J (wave equation)\n"
        "                           Tower Transport: A_p -> A_q auto\n"
        "    Extended Algebra:      Torus, Sigma, Cohomology, Hybrid\n"
        "    Interstice Engine:     Time-Scale, q-Calculus, BV, Scale,\n"
        "                           Xi-Phi, Gauge, UDQ, FTC, Transport\n"
        "    P-adic Bridge:         Valuations, Norms, Product Formula\n"
        "    Universal Context:     Cocycles, Meadow, Naturality\n"
        "    Constants:             Lambda=phi^4, q=i*phi, N=4\n"
        "                           chi_phi=ln(phi)+i*pi/2\n"
        "    Operations:            add, mul, neg, conj, inv, norm,\n"
        "                           Scal, Vec, comm = [a,b] = ab-ba\n"
        "    Method:                Systematic depth-bounded generation\n"
        "                           + evaluation bucketing + proof\n"
        "                           + ORBIT structural matching (NEW)\n"
        "    Bootstrap:             Proven terms -> new atoms -> repeat\n"
        "    Bias:                  NONE. Pure combinatorial exploration.\n"
        "================================================================\n"
        << std::endl;
}

/**
 * @brief Demonstrate golden ratio computations
 */
void demoGoldenRatio() {
    std::cout << "=== Golden Ratio () Demo ===\n";
    
    // Verify  via iteration
    double phi_computed = ad::domain::GoldenRatio::convergeToGolden(1.0, 50);
    std::cout << " computed via R(z) = 1 + 1/z: " << phi_computed << "\n";
    std::cout << " exact value:                  " << ad::domain::GoldenRatio::PHI << "\n";
    
    // Verify identity
    bool satisfies = ad::domain::GoldenRatio::verifyIdentity(phi_computed);
    std::cout << "Satisfies  =  + 1: " << (satisfies ? "YES" : "NO") << "\n";
    
    // Fibonacci and Zeckendorf
    std::cout << "\nFibonacci sequence: ";
    for (int i = 1; i <= 12; ++i) {
        std::cout << ad::domain::Fibonacci::fib(i) << " ";
    }
    std::cout << "\n";
    
    // Normal word counts
    std::cout << "Normal word counts N(n) = F_{n+2}: ";
    for (int i = 0; i <= 8; ++i) {
        std::cout << ad::domain::Fibonacci::normalWordCount(i) << " ";
    }
    std::cout << "\n";
    
    // Zeckendorf representation
    std::cout << "Zeckendorf(42) = { ";
    auto zeck = ad::domain::Fibonacci::toZeckendorf(42);
    for (auto idx : zeck) {
        std::cout << "F_" << idx << " ";
    }
    std::cout << "}\n";
    
    // Entropy
    double entropy = ad::domain::GoldenMeanShift::topologicalEntropy();
    std::cout << "Topological entropy h_top = log() = " << entropy << "\n";
    
    std::cout << std::endl;
}

/**
 * @brief Demonstrate SCOUT operations
 */
void demoScout() {
    std::cout << "=== SCOUT Operations Demo ===\n";
    
    // Complex alignment
    std::complex<double> U(1.0 / std::sqrt(2.0), 1.0 / std::sqrt(2.0)); // Unit
    std::complex<double> q(3.0, 4.0);
    
    double aligned = ad::domain::ScoutEvaluator::alignComplex(U, q);
    std::cout << "Align_U(q) for U = (1+i)/2, q = 3+4i: " << aligned << "\n";
    
    // Quaternion alignment
    ad::domain::ScoutEvaluator::Quaternion Uq(0.5, 0.5, 0.5, 0.5); // Unit quaternion
    ad::domain::ScoutEvaluator::Quaternion qq(1.0, 2.0, 3.0, 4.0);
    
    double alignedQ = ad::domain::ScoutEvaluator::alignQuaternion(Uq, qq);
    std::cout << "Align_U(q) for quaternions: " << alignedQ << "\n";
    
    // Phase transport
    auto transported = ad::domain::ScoutEvaluator::phaseTransportQuaternion(
        ad::domain::ScoutEvaluator::Quaternion(1, 0, 0, 0), Uq);
    std::cout << "Phase transport  = 1U: (" << transported.w << ", " 
              << transported.x << ", " << transported.y << ", " << transported.z << ")\n";
    
    std::cout << std::endl;
}

/**
 * @brief Demonstrate term representation and GOD normalization
 */
void demoTermsAndGOD(ad::core::TermFactory& factory) {
    std::cout << "=== Term Representation and GOD ===\n";
    
    // Create some terms
    auto x = factory.variable("x", ad::core::Sort::Complex);
    auto y = factory.variable("y", ad::core::Sort::Complex);
    auto phi = factory.phi();
    auto J = factory.J();
    
    std::cout << "Variable x: " << x->toString() << " (id=" << x->id() << ")\n";
    std::cout << "Variable y: " << y->toString() << " (id=" << y->id() << ")\n";
    std::cout << "Golden ratio : " << phi->toString() << " (id=" << phi->id() << ")\n";
    std::cout << "Imaginary unit J: " << J->toString() << " (id=" << J->id() << ")\n";
    
    // Create complex terms
    auto xy = factory.mul(x, y);
    auto yx = factory.mul(y, x);
    
    std::cout << "xy: " << xy->toString() << " (id=" << xy->id() << ")\n";
    std::cout << "yx: " << yx->toString() << " (id=" << yx->id() << ")\n";
    
    // Cayley-Dickson pair
    auto pair = factory.pair(x, y);
    std::cout << "Pair (x,y): " << pair->toString() << "\n";
    
    // Conjugation
    auto conj = factory.conj(pair);
    std::cout << "(x,y)*: " << conj->toString() << "\n";
    
    // GOD normalization demo
    ad::logic::Normalizer normalizer(factory);
    normalizer.initStandardRules();
    
    // Create  and normalize to  + 1
    auto phiSq = factory.mul(phi, phi);
    std::cout << "\nBefore normalization:  = " << phiSq->toString() << "\n";
    auto normalized = normalizer.normalize(phiSq);
    std::cout << "After normalization: " << normalized->toString() << "\n";
    
    std::cout << std::endl;
}

/**
 * @brief Demonstrate the inference engine
 */
void demoInference(ad::core::TermFactory& factory) {
    std::cout << "=== Inference Engine Demo ===\n";
    
    // Create knowledge base
    ad::logic::KnowledgeBase kb(factory);
    
    // Create algebra axioms
    ad::domain::AlgebraModule algebra(factory);
    auto axioms = algebra.generateAllAxioms(ad::core::CDLevel(ad::core::CDLevel::QUATERNION));
    
    std::cout << "Generated " << axioms.size() << " algebra axioms\n";
    
    // Add axioms to knowledge base
    for (auto& eq : axioms) {
        kb.addAxiom(eq->lhs(), eq->rhs());
    }
    
    // Add SCOUT axioms
    ad::domain::ScoutModule scout(factory);
    auto scoutAxioms = scout.generateAxioms();
    std::cout << "Generated " << scoutAxioms.size() << " SCOUT axioms\n";
    
    for (auto& eq : scoutAxioms) {
        kb.addAxiom(eq->lhs(), eq->rhs());
    }
    
    std::cout << "Knowledge base: " << kb.numValid() << " equations\n";
    
    // Create inference engine
    ad::logic::InferenceConfig config;
    config.maxSteps = 100;
    config.useGODNormalization = true;
    config.verbose = false;
    
    ad::logic::InferenceEngine engine(factory, config);
    
    // Run a few saturation steps
    auto result = engine.saturate(kb);
    
    std::cout << "Inference result: ";
    switch (result) {
        case ad::logic::InferenceResult::Saturated:
            std::cout << "SATURATED\n"; break;
        case ad::logic::InferenceResult::ProofFound:
            std::cout << "PROOF FOUND\n"; break;
        case ad::logic::InferenceResult::StepLimit:
            std::cout << "STEP LIMIT\n"; break;
        default:
            std::cout << "OTHER\n"; break;
    }
    
    const auto& stats = engine.stats();
    std::cout << "Statistics:\n";
    std::cout << "  Steps: " << stats.steps << "\n";
    std::cout << "  Generated clauses: " << stats.generatedClauses << "\n";
    std::cout << "  Kept clauses: " << stats.keptClauses << "\n";
    std::cout << "  Superpositions: " << stats.superpositions << "\n";
    
    std::cout << std::endl;
}

/**
 * @brief Demonstrate carry rewriting (Zeckendorf normalization)
 */
void demoCarryRewrite() {
    std::cout << "=== Carry Rewrite (011  100) Demo ===\n";
    
    // Test word: 11011 (should normalize to 100100)
    std::vector<uint8_t> word = {1, 1, 0, 1, 1}; // LSB first
    
    std::cout << "Input word:  ";
    for (auto b : word) std::cout << (int)b;
    std::cout << " (has consecutive 1s)\n";
    
    bool isNormal = ad::logic::CarryRewriter::isNormal(word);
    std::cout << "Is normal: " << (isNormal ? "YES" : "NO") << "\n";
    
    auto normalized = ad::logic::CarryRewriter::normalize(word);
    
    std::cout << "Output word: ";
    for (auto it = normalized.rbegin(); it != normalized.rend(); ++it) {
        std::cout << (int)*it;
    }
    std::cout << " (no consecutive 1s)\n";
    
    isNormal = ad::logic::CarryRewriter::isNormal(normalized);
    std::cout << "Is normal: " << (isNormal ? "YES" : "NO") << "\n";
    
    std::cout << std::endl;
}

/**
 * @brief Interactive mode
 */
void interactiveMode(ad::core::TermFactory& /*factory*/) {
    std::cout << "=== Interactive Mode ===\n";
    std::cout << "Commands: quit, phi, fib <n>, zeck <n>\n\n";
    
    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        if (line == "quit" || line == "exit" || line == "q") break;
        
        if (line == "phi") {
            std::cout << " = " << ad::domain::GoldenRatio::PHI << "\n";
        } else if (line.substr(0, 4) == "fib ") {
            int n = std::stoi(line.substr(4));
            std::cout << "F_" << n << " = " << ad::domain::Fibonacci::fib(n) << "\n";
        } else if (line.substr(0, 5) == "zeck ") {
            int n = std::stoi(line.substr(5));
            auto zeck = ad::domain::Fibonacci::toZeckendorf(n);
            std::cout << n << " = ";
            for (size_t i = 0; i < zeck.size(); ++i) {
                if (i > 0) std::cout << " + ";
                std::cout << "F_" << zeck[i];
            }
            std::cout << "\n";
        } else {
            std::cout << "Unknown command. Try: phi, fib <n>, zeck <n>, quit\n";
        }
    }
}

int main(int argc, char* argv[]) {
  try {
    // Parse CLI mode
    std::string mode = "demo";  // Default mode
    bool verbose = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--discover")    mode = "discover";
        else if (arg == "--discover-scalar") mode = "discover_scalar";
        else if (arg == "--discover-all")    mode = "discover_all";
        else if (arg == "--prove")  mode = "prove";
        else if (arg == "--check")  mode = "check";
        else if (arg == "--demo")   mode = "demo";
        else if (arg == "-i" || arg == "--interactive") mode = "interactive";
        else if (arg == "-v" || arg == "--verbose") verbose = true;
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: autodiscover [MODE] [OPTIONS]\n"
                      << "\nModes:\n"
                      << "  --demo              Run demos (default)\n"
                      << "  --discover          Run equality saturation + rule mining\n"
                      << "  --discover-scalar   Run scalar-driven forced discovery loop\n"
                      << "  --discover-all      Universal discovery: ALL numeric systems\n"
                      << "  --prove             Run superposition inference engine\n"
                      << "  --check             Verify a proof certificate\n"
                      << "  -i                  Interactive REPL\n"
                      << "\nOptions:\n"
                      << "  -v, --verbose Verbose output\n"
                      << "  -h, --help    Show this help\n";
            return 0;
        }
    }
    
    printBanner();
    
    // Use the global canonical context  ensures a single TermFactory
    // for the entire pipeline, preserving hash-consing pointer identity.
    auto& ctx = ad::core::globalContext();
    auto& factory = ctx.factory();
    
    if (mode == "demo") {
        demoGoldenRatio();
        demoScout();
        demoTermsAndGOD(factory);
        demoCarryRewrite();
        demoInference(factory);
    }
    else if (mode == "discover") {
        // ==============================================================
        // DISCOVER MODE: E-graph saturation + rule mining
        // ==============================================================
        std::cout << "=== DISCOVER MODE ===\n\n";
        
        // Build e-graph with ring axioms
        ad::egraph::EGraph egraph;
        
        // Add basic ring terms
        auto zero = egraph.addLeaf("0");
        auto one = egraph.addLeaf("1");
        auto x = egraph.addLeaf("x");
        auto y = egraph.addLeaf("y");
        auto z = egraph.addLeaf("z");
        
        auto xy = egraph.addApp("mul", {x, y});
        auto yz = egraph.addApp("mul", {y, z});
        auto xyz1 = egraph.addApp("mul", {xy, z});  // (x*y)*z
        auto xyz2 = egraph.addApp("mul", {x, yz});  // x*(y*z)
        
        // Assert associativity
        egraph.merge(xyz1, xyz2, ad::egraph::MergeReason::fromAxiom(0, "assoc"));
        
        // Assert x+0 = x
        auto x_plus_0 = egraph.addApp("add", {x, zero});
        egraph.merge(x_plus_0, x, ad::egraph::MergeReason::fromAxiom(1, "addId"));
        
        // Assert x*1 = x
        auto x_times_1 = egraph.addApp("mul", {x, one});
        egraph.merge(x_times_1, x, ad::egraph::MergeReason::fromAxiom(2, "mulId"));
        
        // Assert x*0 = 0
        auto x_times_0 = egraph.addApp("mul", {x, zero});
        egraph.merge(x_times_0, zero, ad::egraph::MergeReason::fromAxiom(3, "mulZero"));
        
        egraph.rebuild();
        
        // Build rewrite rules for saturation
        std::vector<ad::egraph::RewriteRule> rules;
        
        // Commutativity of addition: add(a,b)  add(b,a)
        ad::egraph::RewriteRule addComm;
        addComm.name = "add_comm";
        addComm.lhs = ad::egraph::Pattern::app("add", {
            ad::egraph::Pattern::var("$a"),
            ad::egraph::Pattern::var("$b")
        });
        addComm.rhs = ad::egraph::Pattern::app("add", {
            ad::egraph::Pattern::var("$b"),
            ad::egraph::Pattern::var("$a")
        });
        addComm.bidirectional = false;  // Already symmetric
        rules.push_back(addComm);
        
        // Saturate
        ad::egraph::Saturator saturator;
        ad::egraph::Saturator::Config satConfig;
        satConfig.maxIterations = 10;
        satConfig.maxNodes = 10000;
        saturator = ad::egraph::Saturator(satConfig);
        
        auto satStats = saturator.saturate(egraph, rules);
        
        std::cout << "Saturation: " << satStats.iterations << " iterations, "
                  << satStats.totalMatches << " matches, "
                  << satStats.totalMerges << " merges\n";
        std::cout << "Fixpoint: " << (satStats.reachedFixpoint ? "YES" : "NO") << "\n\n";
        
        // Mine rules
        ad::egraph::RuleMiner miner;
        auto minedRules = miner.mine(egraph);
        
        std::cout << "Mined " << minedRules.size() << " candidate rules:\n";
        for (const auto& rule : minedRules) {
            std::cout << "  " << rule.toString() << "\n";
        }
        
        // Multi-ring gate validation
        std::cout << "\n=== Multi-Ring Validation ===\n";
        ad::domain::MultiRingGate gate;
        gate.addStandardTestPoints({"x", "y", "z"});
        
        // Test known identity: x*(y+z) = x*y + x*z
        auto yPz = factory.add(factory.variable("y"), factory.variable("z"));
        auto xTimesYPz = factory.mul(factory.variable("x"), yPz);
        auto xTimesY = factory.mul(factory.variable("x"), factory.variable("y"));
        auto xTimesZ = factory.mul(factory.variable("x"), factory.variable("z"));
        auto xyPxz = factory.add(xTimesY, xTimesZ);
        
        auto gateResult = gate.test(xTimesYPz, xyPxz);
        std::cout << "Distributivity: " << gateResult.toString() << "\n";
    }
    else if (mode == "prove") {
        // ==============================================================
        // PROVE MODE: Superposition inference
        // ==============================================================
        std::cout << "=== PROVE MODE ===\n\n";
        demoInference(factory);
    }
    else if (mode == "check") {
        // ==============================================================
        // CHECK MODE: Verify a proof certificate
        // ==============================================================
        std::cout << "=== CHECK MODE ===\n\n";
        
        // Demo: verify a small proof certificate
        ad::proof::Kernel kernel;
        
        // Register ring axioms
        auto a = factory.variable("a");
        auto b = factory.variable("b");
        auto ab = factory.mul(a, b);
        auto ba = factory.mul(b, a);
        size_t commIdx = kernel.addAxiom(ab, ba, "commutativity", "ring");
        
        std::cout << "Registered " << kernel.numAxioms() << " axiom(s)\n";
        
        // Build a proof certificate: a*b = b*a (direct axiom application)
        std::vector<ad::proof::CertificateStep> cert;
        cert.push_back(ad::proof::CertificateStep::axiom(commIdx));
        
        auto result = kernel.replayCertificate(cert, factory);
        if (result.success) {
            std::cout << "Certificate VERIFIED: " << result.theorem->toString() << "\n";
        } else {
            std::cout << "Certificate REJECTED: " << result.error << "\n";
        }
        
        // More complex: prove a*b = b*a = a*b via symmetry + transitivity
        auto r1 = kernel.introduceAxiom(commIdx);  // a*b = b*a
        if (r1.success) {
            auto r2 = kernel.symmetry(*r1.theorem);  // b*a = a*b
            if (r2.success) {
                auto r3 = kernel.transitivity(*r1.theorem, *r2.theorem);  // a*b = a*b
                if (r3.success) {
                    std::cout << "Reflexivity proof: " << r3.theorem->toString() << "\n";
                }
            }
        }
        
        std::cout << "Total theorems produced: " << kernel.totalTheorems() << "\n";
    }
    else if (mode == "discover_scalar") {
        // ==============================================================
        // DISCOVER_SCALAR MODE: Scalar-Upward Discovery Engine v2.0
        //
        // Pipeline:
        //   1. Generate ALL ground terms over {, , 0, 1, -1, 2}
        //   2. GOD-normalize (  1- canonicalization)
        //   3. Evaluate each to EXACT Z[] value (a + b, a,b  Z)
        //   4. Bucket by exact ZPhi value (hash-based, zero tolerance)
        //   5. All pairs in same bucket  provably equal
        //   6. Generate kernel-verified proof certificates
        //   7. Validate via SCOUT boundary collapse
        //   8. Commit to KnowledgeBase
        // ==============================================================
        std::cout << "=== DISCOVER SCALAR MODE (v2.0  Scalar-Upward) ===\n\n";

        ad::logic::KnowledgeBase kb(factory);
        ad::discovery::DiscoveryConfig config;
        config.verbose         = true;
        config.maxTermDepth    = 3;       // depth 3  hundreds of terms
        config.maxCandidatesPerTarget = 500;
        config.maxPairsPerBucket = 30;
        config.enableGOD       = true;    // apply GOD normalization
        config.enableSCOUT     = true;    // validate via SCOUT
        config.enableProofLift = true;    // generate kernel proofs

        ad::discovery::DiscoveryOrchestrator orchestrator(factory, kb, config);
        orchestrator.seedGoldenRatio();
        auto stats = orchestrator.run();

        std::cout << "\n=== DISCOVERY COMPLETE ===\n";
        stats.print();

        // Report detailed results
        const auto& disc = orchestrator.discoveries();
        if (!disc.empty()) {
            size_t kernelCount = 0;
            size_t scoutCount = 0;
            for (const auto& eq : disc) {
                if (eq.kernelProven) kernelCount++;
                if (eq.scoutValidated) scoutCount++;
            }
            std::cout << "Kernel-proven:    " << kernelCount << "/" << disc.size() << "\n";
            std::cout << "SCOUT-validated:  " << scoutCount << "/" << disc.size() << "\n";
        }
    }
    else if (mode == "discover_all") {
        // ==============================================================
        // DISCOVER ALL MODE: Systematic Universal Discovery v10.0
        //
        // ZERO BIAS  ZERO PRESETS  PURE SYSTEMATIC GENERATION
        //
        // The engine discovers ALL equations in the Cayley-Dickson tower
        // using ONLY:
        //   1. Atoms (basis elements of each algebra level)
        //   2. Operations (add, mul, neg, conj, inv, norm, Scal, Vec, comm)
        //   3. Depth bounds (how many compositions to explore)
        //   4. Budget caps (maximum terms per level)
        //
        // NO hardcoded expressions. NO named theorems.
        // NO Fibonacci arrays. NO "generate Jacobi terms".
        // The engine explores ALL possible compositions systematically
        // and discovers WHATEVER identities exist by evaluation bucketing.
        //
        // The commutator [a,b] = ab-ba is a FIRST-CLASS operation,
        // enabling depth-2 generation of:
        //   [i,[j,k]]  Jacobi terms
        //   [a,[a,b]]  Laplacian terms (D)
        //   Scal([a,b])  structure constants
        //   comm(a,mul(b,c))  Leibniz terms
        //
        // Bootstrap: proven equation terms become new atoms  systematic
        // depth generation  discover more. Knowledge accumulates
        // without human guidance.
        //
        // v9.1: SYSTEMATIC UNIVERSAL DISCOVERY ENGINE  EXTENDED ALGEBRA
        // ==============================================================
        std::cout << "=== DISCOVER ALL MODE (v10.0  Universal Mathematics Discovery) ===\n";
        std::cout << "    ZERO BIAS | ZERO PRESETS | PURE SYSTEMATIC GENERATION\n\n";

        ad::logic::KnowledgeBase kb(factory);
        ad::discovery::UniversalConfig uconfig;
        uconfig.verbose            = true;
        uconfig.maxDepthReal       = 4;       // v9.0: deeper for ,  identities
        uconfig.maxDepthComplex    = 3;       // v9.0: deeper for wave structure
        uconfig.maxDepthQuaternion = 3;       // v9.0: deeper (comm at depth 23 gives D)
        uconfig.maxDepthOctonion   = 2;       // v9.0: deeper for Malcev, Moufang
        uconfig.maxTermsPerLevel   = 10000;   // v9.0: large for systematic coverage
        uconfig.maxPairsPerBucket  = 50;      // v9.0: more pairs for richer equation sets
        uconfig.maxTotalEquations  = 200000;  // v9.0: double capacity
        uconfig.enableGOD          = true;
        uconfig.enableSCOUT        = true;
        uconfig.enableKernelProof  = true;
        uconfig.enableEGraph       = true;
        uconfig.enableBudget       = true;
        uconfig.showProofTraces    = verbose;
        uconfig.showAllEquations   = true;
        uconfig.gasBudget          = 80000000;   // v9.0: larger budget for deeper exploration
        uconfig.timeLimitMs        = 600000;     // 10 minutes for systematic exploration
        uconfig.maxBootstrapRounds = 8;          // v9.0: more bootstrap feedback iterations
        uconfig.maxKBTermsPerLevel = 6000;       // v9.0: more KB-derived terms per round

        ad::discovery::UniversalDiscoveryEngine engine(factory, kb, uconfig);
        engine.seedAxioms();
        auto stats = engine.run();

        // Summary
        const auto& disc = engine.discoveries();
        size_t kernelN = 0, scoutN = 0, egraphN = 0;
        for (const auto& eq : disc) {
            if (eq.kernelProven)   kernelN++;
            if (eq.scoutValidated) scoutN++;
            if (eq.egraphProven)   egraphN++;
        }
        // Count by class for extended algebra
        std::unordered_map<int, size_t> classCounts;
        for (const auto& eq : disc) {
            classCounts[static_cast<int>(eq.eqClass)]++;
        }

        std::cout << "\n=== UNIVERSAL DISCOVERY COMPLETE (v14.0 — INTERSTICE ORBIT) ===\n";
        std::cout << "Total equations:   " << disc.size() << "\n";
        std::cout << "Kernel-proven:     " << kernelN << "/" << disc.size() << "\n";
        std::cout << "SCOUT-validated:   " << scoutN << "/" << disc.size() << "\n";
        std::cout << "E-graph-proven:    " << egraphN << "/" << disc.size() << "\n";
        std::cout << "Extended algebra:  " << stats.extendedEquations << "\n";
        std::cout << "  Torus Geometry:  " << classCounts[5] << "\n";
        std::cout << "  Sigma-Calculus:  " << classCounts[6] << "\n";
        std::cout << "  Cohomology:      " << classCounts[7] << "\n";
        std::cout << "  Hybrid Dynamics: " << classCounts[8] << "\n";
        std::cout << "  Analysis/Trans:  " << classCounts[9] << "\n";
        std::cout << "  Number Theory:   " << classCounts[10] << "\n";
        std::cout << "  Calculus:        " << classCounts[11] << "\n";
        std::cout << "  Interstice Calc: " << classCounts[12] << "\n";
        std::cout << "  Scale Covar:     " << classCounts[13] << "\n";
        std::cout << "  Q-Golden Calc:   " << classCounts[14] << "\n";
        std::cout << "  P-adic Bridge:   " << classCounts[15] << "\n";
        std::cout << "  Univ. Context:   " << classCounts[16] << "\n";
        std::cout << "  Diff. Identity:  " << classCounts[17] << "\n";
        std::cout << "  Tensor/Spinor:   " << classCounts[18] << "\n";
        std::cout << "  Physics Struct:  " << classCounts[19] << "\n";
        std::cout << "  Interstice Deep: " << classCounts[20] << "\n";
        std::cout << "  Field Equations: " << classCounts[21] << "\n";
        std::cout << "  ---- ORBIT DISCOVERY (v14.0) ----\n";
        std::cout << "  Orbit-Structural:" << classCounts[22] << "\n";
        std::cout << "    Derivatives:   " << stats.orbitDerivatives << "\n";
        std::cout << "    Leibniz:       " << stats.orbitLeibniz << "\n";
        std::cout << "    FToI:          " << stats.orbitFToI << "\n";
        std::cout << "    UFE:           " << stats.orbitUFE << "\n";
        std::cout << "    Scale Covar:   " << stats.orbitScaleCovar << "\n";
        std::cout << "    Cross-Action:  " << stats.orbitCrossAction << "\n";
        std::cout << "    Transported:   " << stats.orbitTransported << "\n";
        std::cout << "Bootstrap rounds:  " << stats.bootstrapRounds << "\n";
        if (!stats.roundDiscoveries.empty()) {
            std::cout << "Per-round:         ";
            for (size_t i = 0; i < stats.roundDiscoveries.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << "R" << (i+1) << "=" << stats.roundDiscoveries[i];
            }
            std::cout << "\n";
        }
    }
    else if (mode == "interactive") {
        interactiveMode(factory);
    }
    
    std::cout << "Done.\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "\n*** EXCEPTION: " << ex.what() << "\n";
    return 1;
  } catch (...) {
    std::cerr << "\n*** UNKNOWN EXCEPTION\n";
    return 1;
  }
}
