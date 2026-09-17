# AutoDiscoverProver — Comprehensive READ-ONLY Audit Report

**Scope**: Every source file under `src/` and `tests/`, plus `CMakeLists.txt`  
**Files inspected**: 46 files (31 headers, 11 `.cpp` source, 6 test files, 1 CMake)  
**Date**: 2025  

---

## Table of Contents

1. [Issue Legend](#issue-legend)  
2. [File-by-File Findings](#file-by-file-findings)  
3. [Cross-Cutting Summaries](#cross-cutting-summaries)  

---

## Issue Legend

| Tag | Category |
|-----|----------|
| **[MOJIBAKE]** | Corrupted / mojibake Unicode |
| **[SOUNDNESS]** | Soundness / correctness bug |
| **[NAMING]** | Inconsistent operator naming |
| **[RAW-NEW]** | Raw `new` instead of `make_unique` / `make_shared` |
| **[SINGLETON]** | Global singleton (static local, free function, `std::once_flag`) |
| **[EPSILON]** | Epsilon / floating-point comparison used for semantic identity |
| **[STUB]** | Stub `.cpp` file < 20 lines (header-only with empty translation unit) |
| **[PTR-EQ]** | Pointer / id equality where structural equality may be needed |
| **[PLACEHOLDER]** | Unbound RHS variable, dummy merge reason, or placeholder return |
| **[OVERCLAIM]** | Documentation / comment claims a guarantee the code cannot uphold |

---

## File-by-File Findings

---

### `src/core/Constants.hpp` (57 lines)

No issues found. Clean constant definitions.

---

### `src/core/Context.hpp` (205 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~192 | **[SINGLETON]** | `globalContext()` returns `static Context` — process-wide singleton. |
| ~50–80 | **[NAMING]** | `SymbolTable` class duplicated here conflicts with `src/core/SymbolTable.hpp`; `"*"` / `"mul"` mapped to multiplication, but in `SymbolTable.hpp` `"*"` maps to `BuiltinOp::Conj` (conjugate). |

---

### `src/core/Dimension.hpp` (786 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~761 | **[SINGLETON]** | `UnitRegistry::global()` static-local singleton. |
| ~500–550 | **[SINGLETON]** | Multiple `Unit::meters()`, `Unit::seconds()` etc. — each uses a `static` local singleton pattern. |
| Header comment | **[OVERCLAIM]** | File marked "UNUSED / retained as extension point" but still compiled into `autodiscover_core`. Dead code in the build. |

---

### `src/core/SymbolTable.hpp` (286 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~216–221 | **[NAMING]** | **CRITICAL**: `BuiltinOp::Mul` → `"·"` (Unicode middle dot U+00B7), `BuiltinOp::Conj` → `"*"` (ASCII asterisk). Every other subsystem uses `"*"` or `"mul"` for multiplication. This is the root of the naming inconsistency across the codebase. |

---

### `src/core/SymbolTable.cpp` (13 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| entire file | **[STUB]** | Empty translation unit: includes header + `namespace {}` only. |

---

### `src/core/Term.hpp` (303 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~70 | **[PTR-EQ]** | `operator==(const Term& o) const { return this == &o; }` — intentional under hash-consing, but fragile if terms come from different `TermFactory` instances. No cross-factory guard. |

---

### `src/core/Term.cpp` (391 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~109 | **[EPSILON]** | `scalarEq(a, b)` uses `std::abs(a - b) < 1e-10` for hash-consing scalar terms. Two scalars within 1e-10 are silently aliased to the same term, altering structural identity. |
| ~258–291 | **[RAW-NEW]** | `TermFactory::variable()`, `constant()`, `application()`, `scalar()`, `pair()`, `phi()`, `J()` all use `new Term(...)` and store raw pointers in `std::vector<Term*> allTerms_`. Should use `std::unique_ptr`. |

---

### `src/core/Type.hpp` (295 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~240–260 | **[SINGLETON]** | `TypeFactory::builtinSort()`, `builtinArrow()` etc. use `static` local singletons for each sort kind. |

---

### `src/core/Type.cpp` (13 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| entire file | **[STUB]** | Empty translation unit. |

---

### `src/canon/Canonicalizer.hpp` (711 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~80 | **[SOUNDNESS]** | Creates its own `TermFactory factory_` — violates the single-factory invariant required for pointer-equality semantics. Terms created here will **never** be `==` to terms from any other factory. |
| ~660 | **[SINGLETON]** | `globalEngine()` returns `static CanonEngine`. |
| ~670 | **[SINGLETON]** | `canon()` free function wrapping `globalEngine()`. |
| ~680 | **[SINGLETON]** | `fp()` free function wrapping Fingerprinter singleton. |
| ~400 | **[PLACEHOLDER]** | `reconstructFromENode()` returns `nullptr` unconditionally — anything calling it gets a null term. |

---

### `src/canon/CanonScalar.hpp` (573 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~210 | **[EPSILON]** | `semanticallyHolds()` uses `1e-10` threshold for equation truth. |
| ~250 | **[EPSILON]** | `areSimilar()` uses `1e-10` for fingerprint comparison. |
| ~560 | **[SINGLETON]** | `globalCanonScalarEngine()` static-local singleton. |

---

### `src/canon/Equivalence.hpp` (825 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~780 | **[SINGLETON]** | `ProfileRegistry::global()` static-local singleton. |
| ~650 | **[OVERCLAIM]** | `NormalizationPipeline::buildFromProfile()` declared in class body but never defined — linker error if ever called. |
| ~500 | **[PTR-EQ]** | `areEquivalent()` compares `canonical1->id() == canonical2->id()` — same fragility across factories. |

---

### `src/canon/GODfinal.hpp` (113 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~60 | **[PTR-EQ]** | Fixed-point check `next == current` uses pointer equality. Sound within one factory; breaks silently across factories. |

---

### `src/canon/NFEngine.hpp` (1085 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~50 | **[SOUNDNESS]** | Creates its own `TermFactory factory_` — second independent factory in the canonicalization pipeline. |
| ~1020 | **[SINGLETON]** | `globalNFEngine()` static-local singleton. |
| ~1040 | **[SINGLETON]** | `NF()` free function. |
| ~1060 | **[SINGLETON]** | `equiv()` free function. |
| ~380 | **[PLACEHOLDER]** | `GODTermPass::reconstructFromENode()` returns `nullptr`. |
| ~420 | **[PLACEHOLDER]** | `GODTermPass::applyGaloisConjugation()` returns `nullptr` for some node kinds. |
| ~350 | **[OVERCLAIM]** | `GODTermPass::isNormal()` always returns `true` — makes the normality predicate meaningless. Any term is "normal" regardless of whether GOD has actually processed it. |

---

### `src/domain/Algebra.hpp` (1372 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~110 | **[EPSILON]** | `1e-30` threshold in `isApproxZero()`. |
| ~185 | **[EPSILON]** | `1e-12` threshold in `goldenRatioConvergence()`. |
| ~220 | **[EPSILON]** | `1e-10` general tolerance. |
| ~900 | **[EPSILON]** | `1e-8` tolerance in Cayley-Dickson norm checks. |
| ~750 | **[SOUNDNESS]** | `phiPower()` for negative exponents uses `std::pow(PHI, n)` — abandons exact `Z[φ]` arithmetic for a floating-point approximation. |
| ~1100 | **[PLACEHOLDER]** | `CayleyDicksonEvaluator` returns `CD()` (zero) for unknown operations and all variables — silent failure, no error propagation. |

---

### `src/domain/Algebra.cpp` (15 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| entire file | **[STUB]** | Empty translation unit. |

---

### `src/domain/Closure.hpp` (930 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~250 | **[SOUNDNESS]** | `GoldenArithmetic::divide()` converts to `double`, divides, then rounds back to `Z[φ]` — not exact division; silently truncates. |
| ~600 | **[OVERCLAIM]** | `enumerateNormalWords()` brute-forces all $2^n$ binary strings — exponential growth. Documentation claims it finds "all normal words" but the `maxLength` parameter limits exploration, silently under-enumerating. |

---

### `src/domain/Closure.cpp` (210 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~81, ~91 | **[EPSILON]** | `1e-15` threshold for convergence checks. |
| ~198 | **[PTR-EQ]** | `id()` comparison for term equality. |

---

### `src/domain/MultiRingEval.hpp` (728 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~90–130 | **[NAMING]** | Operator name variants: `"add"` / `"+"`, `"sub"` / `"-"`, `"mul"` / `"*"` — handled via fallthrough, but adds to the inconsistency burden. |
| ~350 | **[SOUNDNESS]** | `Rational::canonicalize()` uses `int64_t` GCD. Products of large numerators/denominators can overflow `int64_t` before canonicalization, producing wrong rationals. No overflow detection. |

---

### `src/domain/Ontology.hpp` (500 lines)

No issues found. Clean ontology framework.

---

### `src/domain/Scout.hpp` (907 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~120 | **[EPSILON]** | `1e-30` in phase alignment checks. |
| ~280 | **[EPSILON]** | `1e-10` in scalar product comparison. |
| ~450 | **[EPSILON]** | `1e-12` in convergence tests. |
| ~500 | **[OVERCLAIM]** | Fano-plane structure table for octonion multiplication is hardcoded — the comment says "verified" but there is no compile-time or runtime validation that the table is correct. |

---

### `src/domain/Scout.cpp` (15 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| entire file | **[STUB]** | Empty translation unit. |

---

### `src/egraph/EGraph.hpp` (1305 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~300 | **[SOUNDNESS]** | `std::any userData` on e-nodes — type-unsafe; `std::any_cast` failures throw at runtime with no compile-time protection. |
| ~629 | **[PLACEHOLDER]** | `MergeReason::fromCongruence(ENode{}, ENode{})` — constructs merge reason with **dummy empty ENodes**. The "reason" carries no meaningful information about which congruence triggered the merge. |
| ~900 | **[SOUNDNESS]** | Pattern matching cross-product: when a pattern variable binds to multiple e-nodes in a class, all combinations are enumerated — exponential blowup with no bound. |
| ~1123 | **[PLACEHOLDER]** | `instantiatePattern()` creates a fresh leaf for an **unbound RHS variable** instead of reporting an error. This silently introduces free variables into the instantiated term. |

---

### `src/egraph/RuleMiner.hpp` (~230 lines)

No issues found. Clean rule mining interface.

---

### `src/encoding/Encoding.hpp` (718 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~160 | **[SOUNDNESS]** | Type-punning via `union { double d; uint64_t u; }` is **undefined behavior** in C++ (only legal in C). Should use `std::memcpy` or `std::bit_cast` (C++20). |
| ~500 | **[SOUNDNESS]** | `EquationEncoder` stores a **reference** to an external `Encoder`; if the referent is destroyed first, dangling reference. |

---

### `src/encoding/Structural.hpp` (637 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~620 | **[SINGLETON]** | `globalStructuralEncoder()` static-local singleton. |

---

### `src/fingerprint/Fingerprinter.hpp` (650 lines)

No issues found. Well-documented φ-weighted fingerprinting.

---

### `src/fingerprint/Semantic.hpp` (819 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~310 | **[EPSILON]** | `1e-10` in `areSemanticallyEqual()`. |
| ~350 | **[EPSILON]** | `1e-15` in high-precision semantic comparison. |
| ~600 | **[OVERCLAIM]** | `areSemanticallySimilar()` is marked `[[deprecated]]` but still present and callable — dead API surface. |
| ~797 | **[SINGLETON]** | `globalSemanticScalarizer()` static-local singleton. |
| ~700 | **[SOUNDNESS]** | `SemanticSignature::toFingerprint()` creates a `Fingerprint` with exact value `ring::ZPhi()` (zero) — discards the exact algebraic content, keeping only the floating-point semantic hash. |

---

### `src/fingerprint/DeltaEngine.hpp` (~260 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~80 | **[EPSILON]** | `1e-15` threshold for delta significance. |
| ~120 | **[SOUNDNESS]** | `approximateMagnitude()` converts a `BigInt` to `double` for magnitude estimation — loses precision for large integers beyond 2^53. |

---

### `src/fingerprint/WeightSchedule.hpp` (~100 lines)

No issues found.

---

### `src/logic/Equation.hpp` (~220 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~80 | **[PTR-EQ]** | `isTrivial()` checks `lhs_->id() == rhs_->id()` — fragile across factories. |

---

### `src/logic/Equation.cpp` (13 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| entire file | **[STUB]** | Empty translation unit. |

---

### `src/logic/InferenceEngine.hpp` (1018 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~580–640 | **[SOUNDNESS]** | **CRITICAL BUG in `superpositionPositions()`**: When superposing into a subterm at position $p$ in term $s$, the code sets `newLhs = rSigma` (just $r\sigma$) instead of $s[r\sigma]_p$ (the full term $s$ with the subterm at position $p$ replaced by $r\sigma$). During recursive descent into children, the original term `t` is passed unchanged but the new equation's LHS should be the **reconstructed** parent with the rewritten child. This produces **wrong inferences** for any non-root superposition position. |
| ~700 | **[SOUNDNESS]** | `phaseSuperpositionPositions()` calls `signaturesCompatible()` which has no visible definition in the codebase. |
| ~850 | **[EPSILON]** | `FALSIFICATION_TOLERANCE = 1e-8` — semantic falsification uses floating-point threshold. |
| ~900 | **[SOUNDNESS]** | `DiscoveryEngine` dedup uses `hash(canonBytecodeA) XOR hash(canonBytecodeB)` — XOR is commutative and not injective; distinct equation pairs can collide. |
| ~multiple | **[PTR-EQ]** | `newLhs->id() != newRhs->id()` used throughout for trivial-equation checks. |

---

### `src/logic/InferenceEngine.cpp` (13 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| entire file | **[STUB]** | Empty translation unit. |

---

### `src/logic/KnowledgeBase.hpp` (727 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~200 | **[SOUNDNESS]** | `mutable TermFactory subsumptionFactory_` — a const method creates terms in a separate factory, breaking the single-factory invariant. |
| ~400 | **[SOUNDNESS]** | `DiscriminationTree::retrieve()` variable handling may produce false positive matches: a query with a variable at a position may match terms with different structures at that position. |
| ~600 | **[OVERCLAIM]** | `rewriteToFixpoint()` bounded by `maxIters = 100`. If the rewrite system needs > 100 steps, the fixpoint is silently abandoned and the intermediate (non-normal) form is returned as if it were the normal form. |

---

### `src/logic/KnowledgeBase.cpp` (13 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| entire file | **[STUB]** | Empty translation unit. |

---

### `src/logic/MatcherUnifier.hpp` (~500 lines)

No issues found. Clean matching/unification with occurs check.

---

### `src/logic/MatcherUnifier.cpp` (13 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| entire file | **[STUB]** | Empty translation unit. |

---

### `src/logic/Normalizer.hpp` (1269 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~230 | **[NAMING]** | `goto done` label in `TermLFPEngine` — minor style violation, not a naming issue per se, but non-idiomatic C++. |
| ~800 | **[OVERCLAIM]** | GODNormalizer bounds orbit exploration at `maxOrbitSize_ = 10000`. If the orbit is larger, the BFS is truncated and the minimum-id representative found so far is returned. This means the "canonical form" depends on exploration order and may not be the true minimum, **violating the idempotence guarantee** documented in comments. |
| ~950 | **[EPSILON]** | `isScalarZero()` uses exact `== 0.0` comparison (`scalar == 0.0`). Correct for literal constants but will fail for scalars that are the result of floating-point computation (e.g., `1.0 - 1.0` is fine, but `0.1 + 0.1 + ... - 1.0` may not be exactly zero). |
| ~960 | **[EPSILON]** | `isScalarOne()` similarly uses `== 1.0`. |

---

### `src/logic/Normalizer.cpp` (13 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| entire file | **[STUB]** | Empty translation unit. |

---

### `src/ring/ZPhi.hpp` (1086 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~680–710 | **[SOUNDNESS]** | **BigInt::toString() truncation bug**: Multi-limb division by 10 uses `UINT128_T val = ...` then `static_cast<Limb>(val) / 10` and `static_cast<Limb>(val) % 10`. The `static_cast<Limb>` discards the high 64 bits of the 128-bit intermediate value. For numbers ≥ 2^64, decimal output will be **incorrect**. |
| ~400 | **[SOUNDNESS]** | `BigInt::subMagnitude()` borrow logic: when `b` is `UINT64_MAX` (0xFFFF…FFFF), the expression `b + borrow` wraps around to 0 when `borrow = 1`. The comparison `a < b + borrow` then becomes `a < 0` which is always false, so the borrow is lost. |
| ~1040 | **[SINGLETON]** | `globalFibCache()` returns `static FibPow2Cache` via `std::call_once`. |
| ~300 | **[SOUNDNESS]** | `phiNegPower(int n)` unsigned-conversion: when called with `n = 0`, `posN = -n = 0`, then `fibDoubling(posN - 1)` computes `fibDoubling(-1)` which underflows the unsigned argument. |

---

### `src/ring/ZeckendorfBigInt.hpp` (~230 lines)

No issues found. Clean implementation; `normalizeDigits()` carry rule is correct.

---

### `src/store/ProofStore.hpp` (689 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~400 | **[SOUNDNESS]** | `evictOne()` — **TOCTOU race condition**: scans all buckets under `shared_lock` to find the LRU entry (recording `targetBucket` and `targetIdx`), releases the shared lock, then takes a `unique_lock` on that single bucket to erase. Between the two locks, another thread may have already evicted or modified that bucket, making `targetIdx` stale. Can result in erasing the wrong entry or out-of-bounds access. |
| ~500 | **[SOUNDNESS]** | `findByFingerprint()` throws `std::logic_error("fingerprint collision")` on exact fingerprint match with different canonical bytes. In production, this crashes the process. Should be a fallback search or a soft error. |

---

### `src/proof/Proof.hpp` (~300 lines)

No issues found. Clean DAG-structured proof; `topologicalOrder()` uses min-heap for determinism.

---

### `src/proof/Proof.cpp` (11 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| entire file | **[STUB]** | Empty translation unit. |

---

### `src/proof/ProofChecker.hpp` (931 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~350–400 | **[SOUNDNESS]** | **SOUNDNESS GAP in `verifySuperposition()`**: At `Full` verification level, the checker only confirms "conclusion differs from at least one premise" — it does **not** verify that the superposition position exists, that the subterm at that position actually unifies with the LHS of the rewrite rule, or that the conclusion equals $s[r\sigma]_p$. This means an invalid rewrite (e.g., rewriting a subterm that doesn't match) would still be accepted as valid. |
| ~430 | **[SOUNDNESS]** | `verifyDemodulation()` similarly only checks "result differs from input" — does not verify that the demodulator's LHS actually matches a subterm. |
| ~480 | **[OVERCLAIM]** | `verifyGODNormalization()` at `Standard` level performs **no verification at all** — it only checks at `Scout` level. A `Standard`-level proof certificate could contain invalid GOD steps and pass verification. |
| ~500 | **[OVERCLAIM]** | Class documentation claims "de Bruijn criterion" but the Full-level checks don't actually reconstruct the rewrite to verify structural correctness. The kernel in `CertificateKernel.hpp` is the true LCF core; this checker is a weaker heuristic layer. |

---

### `src/proof/ProofChecker.cpp` (12 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| entire file | **[STUB]** | Empty translation unit. |

---

### `src/proof/CertificateKernel.hpp` (~470 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~200 | **[PTR-EQ]** | `transitivity()` checks `thm1.rhs() != thm2.lhs()` via pointer inequality. Correct under hash-consing within a single factory. **Fails silently** if theorems come from different factories (the transitivity would be refused even when the terms are structurally identical). |
| ~280 | **[SOUNDNESS]** | `rewrite()` substitution: no check that the substitution maps **all** free variables. A partial substitution silently leaves unbound variables in the result theorem's terms. |

LCF design (private `Theorem` ctor, `Kernel` as `friend`) is correctly implemented — **commendation**.

---

### `src/proof/DerivationTrace.hpp` (~200 lines)

No issues found. Clean derivation trace with chain validation.

---

### `src/proof/CertificateExporter.hpp` (~100 lines)

No issues found. Clean export format interface.

---

### `src/proof/CertificateExporter.cpp` (498 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~430–498 | (none) | `termToTSTP()` static helper is correctly defined at the bottom of the file. TPTP variable convention (uppercase) is implemented. No issues in the export logic. |

No issues found.

---

### `src/main.cpp` (~370 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~330 | **[OVERCLAIM]** | `check` mode hardcodes `a * b = b * a` (commutativity) as the demonstration axiom. This is correct for commutative rings but misleading for the project's actual scope (octonions, sedenions) where multiplication is **non-commutative** and **non-associative**. |

---

### `tests/test_term.cpp` (~230 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| all | (note) | Uses raw `assert()` throughout instead of a test framework (Google Test, Catch2). Not a category issue, but limits test diagnostics. |

No category issues found.

---

### `tests/test_golden.cpp` (~160 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~80 | **[EPSILON]** | `1e-10` tolerance in golden ratio convergence assertion. |
| ~120 | **[EPSILON]** | `1e-14` tolerance in Binet's formula assertion. |
| ~150 | **[EPSILON]** | `1e-6` tolerance in topological entropy assertion. |

These are **test assertions**, not production semantic-identity checks. Noted for completeness but low severity.

---

### `tests/test_normalizer.cpp` (~290 lines)

No issues found. Good coverage of CarryRewriter, Normalizer rules, GOD invariance.

---

### `tests/test_canon_scalar.cpp` (~320 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~180–320 | **[OVERCLAIM]** | **Five integration tests commented out**: `testCanonScalarEngine`, `testCanonScalarEquivalence`, `testEquationCanonScalar`, `testFullPipeline`, `testDeduplication` — annotated "disabled pending API stabilization". This leaves the `CanonScalar` integration path **untested**. The file claims to test the canon-scalar pipeline, but the critical end-to-end paths are disabled. |

---

### `tests/test_properties.cpp` (720 lines)

No category issues found. Exercises ZPhi ring axioms, normalizer idempotence, encoding injectivity, phase mod-4 algebra, discrimination tree, KnowledgeBase, TSTP export, EGraph explain, NFEngine fixpoint, proof deterministic order. Good property-based coverage.

---

### `tests/test_deep_modules.cpp` (~290 lines)

No category issues found. Covers K=12 weight dominance, equality saturation, proof certificate kernel replay, multi-ring evaluation, dominant-index delta engine, Zeckendorf BigInt roundtrip, rule mining.

---

### `CMakeLists.txt` (312 lines)

| Line(s) | Tag | Detail |
|---------|-----|--------|
| ~94 | (note) | `Dimension.hpp` listed as `CORE_HEADERS` and compiled into `autodiscover_core`, despite being documented as "UNUSED / retained as extension point." |
| (note) | (note) | Missing from `CANON_HEADERS`: `GODfinal.hpp` is not listed but exists in `src/canon/`. Also `Context.hpp` exists in `src/core/` but is not listed in `CORE_HEADERS`. Build works because `target_include_directories` adds `src/` for header resolution, but these files are invisible to IDE-based build-file navigation. |
| (note) | (note) | Missing headers from lists: `src/domain/MultiRingEval.hpp`, `src/domain/Ontology.hpp`, `src/ring/ZeckendorfBigInt.hpp`, `src/fingerprint/DeltaEngine.hpp`, `src/fingerprint/WeightSchedule.hpp`, `src/egraph/RuleMiner.hpp`, `src/proof/CertificateKernel.hpp`, `src/proof/DerivationTrace.hpp`. All are used via `#include` but not tracked in `CMakeLists.txt` variables (cosmetic; does not affect build). |

---

## Cross-Cutting Summaries

### 1. **[MOJIBAKE]** — Corrupted Unicode

**No mojibake found.** The only Unicode in source is the intentional middle-dot `"·"` (U+00B7) in `SymbolTable.hpp` line ~216 and mathematical symbols in comments (ℝ, ℂ, ℍ, 𝕆, φ). All files are valid UTF-8.  The `/utf-8` MSVC flag in `CMakeLists.txt` ensures correct handling.

---

### 2. **[SOUNDNESS]** — Soundness Issues (14 findings)

| Severity | Location | Issue |
|----------|----------|-------|
| **CRITICAL** | `InferenceEngine.hpp` ~580–640 | `superpositionPositions()` does not reconstruct $s[r\sigma]_p$; uses $r\sigma$ alone. Produces wrong inferences for non-root positions. |
| **CRITICAL** | `ProofChecker.hpp` ~350–400 | `verifySuperposition()` at Full level doesn't verify rewrite position — accepts invalid rewrites. |
| **HIGH** | `ZPhi.hpp` ~680–710 | `BigInt::toString()` truncates 128-bit intermediate to 64 bits during division by 10. Wrong decimal output for large numbers. |
| **HIGH** | `ZPhi.hpp` ~400 | `BigInt::subMagnitude()` borrow lost when `b = UINT64_MAX`. |
| **HIGH** | `Encoding.hpp` ~160 | Type-punning via `union` — undefined behavior in C++. |
| **HIGH** | `MultiRingEval.hpp` ~350 | `Rational::canonicalize()` `int64_t` overflow. |
| **MEDIUM** | `ProofChecker.hpp` ~430 | `verifyDemodulation()` only checks "differs" — doesn't verify subterm match. |
| **MEDIUM** | `Canonicalizer.hpp` ~80, `NFEngine.hpp` ~50 | Own `TermFactory` instances — pointer equality breaks for cross-factory terms. |
| **MEDIUM** | `KnowledgeBase.hpp` ~200 | Mutable `subsumptionFactory_` in const method. |
| **MEDIUM** | `Algebra.hpp` ~750 | `phiPower()` uses `std::pow()` for negative exponents — loses exact Z[φ]. |
| **MEDIUM** | `EGraph.hpp` ~900 | Unbounded exponential cross-product in pattern matching. |
| **MEDIUM** | `ProofStore.hpp` ~400 | `evictOne()` TOCTOU race condition. |
| **MEDIUM** | `ProofStore.hpp` ~500 | `findByFingerprint()` throws on collision — crashes in production. |
| **LOW** | `ZPhi.hpp` ~300 | `phiNegPower(0)` underflow. |
| **LOW** | `CertificateKernel.hpp` ~280 | `rewrite()` allows partial substitution silently. |
| **LOW** | `DeltaEngine.hpp` ~120 | `approximateMagnitude()` loses precision for large BigInt. |
| **LOW** | `InferenceEngine.hpp` ~700 | `signaturesCompatible()` undefined. |
| **LOW** | `InferenceEngine.hpp` ~900 | XOR hash dedup — collision prone. |

---

### 3. **[NAMING]** — Inconsistent Operator Naming (3 clusters)

| Cluster | Files | Detail |
|---------|-------|--------|
| `"*"` = conjugate vs multiplication | `SymbolTable.hpp` vs `Context.hpp`, `MultiRingEval.hpp`, `main.cpp` | `BuiltinOp::Mul` = `"·"`, `BuiltinOp::Conj` = `"*"` in SymbolTable; everywhere else `"*"` or `"mul"` = multiplication. |
| `"add"` / `"+"` | `MultiRingEval.hpp` | Both accepted via fallthrough. |
| `"sub"` / `"-"` / `"mul"` / `"*"` | `MultiRingEval.hpp` | Multiple name variants for same operation. |

---

### 4. **[RAW-NEW]** — Raw `new` Instead of Smart Pointers

| Location | Count |
|----------|-------|
| `Term.cpp` ~258–291 | At least 7 allocation sites (`variable()`, `constant()`, `application()`, `scalar()`, `pair()`, `phi()`, `J()`). All use `new Term(...)` stored in `std::vector<Term*> allTerms_`. |

---

### 5. **[SINGLETON]** — Global Singletons (16 instances)

| Location | Singleton |
|----------|-----------|
| `Context.hpp` ~192 | `globalContext()` |
| `Dimension.hpp` ~761 | `UnitRegistry::global()` |
| `Dimension.hpp` ~500+ | `Unit::meters()`, `Unit::seconds()`, etc. (6+) |
| `Type.hpp` ~240–260 | `TypeFactory` static sorts |
| `Canonicalizer.hpp` ~660 | `globalEngine()` |
| `Canonicalizer.hpp` ~670 | `canon()` |
| `Canonicalizer.hpp` ~680 | `fp()` |
| `CanonScalar.hpp` ~560 | `globalCanonScalarEngine()` |
| `NFEngine.hpp` ~1020 | `globalNFEngine()` |
| `NFEngine.hpp` ~1040 | `NF()` |
| `NFEngine.hpp` ~1060 | `equiv()` |
| `Equivalence.hpp` ~780 | `ProfileRegistry::global()` |
| `Structural.hpp` ~620 | `globalStructuralEncoder()` |
| `Semantic.hpp` ~797 | `globalSemanticScalarizer()` |
| `ZPhi.hpp` ~1040 | `globalFibCache()` |

---

### 6. **[EPSILON]** — Floating-Point Comparisons for Semantic Identity

| Location | Value | Purpose |
|----------|-------|---------|
| `Term.cpp` ~109 | `1e-10` | Hash-consing scalar equality |
| `CanonScalar.hpp` ~210 | `1e-10` | Equation semantic truth |
| `CanonScalar.hpp` ~250 | `1e-10` | Fingerprint similarity |
| `Algebra.hpp` ~110 | `1e-30` | Approximate zero |
| `Algebra.hpp` ~185 | `1e-12` | Golden ratio convergence |
| `Algebra.hpp` ~220 | `1e-10` | General tolerance |
| `Algebra.hpp` ~900 | `1e-8` | CD norm check |
| `Closure.cpp` ~81, ~91 | `1e-15` | Convergence |
| `Scout.hpp` ~120 | `1e-30` | Phase alignment |
| `Scout.hpp` ~280 | `1e-10` | Scalar product |
| `Scout.hpp` ~450 | `1e-12` | Convergence |
| `Semantic.hpp` ~310 | `1e-10` | Semantic equality |
| `Semantic.hpp` ~350 | `1e-15` | High-precision comparison |
| `DeltaEngine.hpp` ~80 | `1e-15` | Delta significance |
| `InferenceEngine.hpp` ~850 | `1e-8` | Falsification tolerance |
| `Normalizer.hpp` ~950, ~960 | `== 0.0`, `== 1.0` | Exact scalar tests (edge case) |

**Most critical**: `Term.cpp` ~109 — epsilon in hash-consing means two distinct scalars within `1e-10` become the **same term**. This is a semantic-identity issue because all downstream processing (fingerprinting, proofs, canonicalization) treats them as identical.

---

### 7. **[STUB]** — Stub `.cpp` Files < 20 Lines (11 files)

| File | Lines | Content |
|------|-------|---------|
| `src/core/SymbolTable.cpp` | 13 | Include + empty namespace |
| `src/core/Type.cpp` | 13 | Include + empty namespace |
| `src/domain/Algebra.cpp` | 15 | Include + empty namespace |
| `src/domain/Scout.cpp` | 15 | Include + empty namespace |
| `src/logic/Equation.cpp` | 13 | Include + empty namespace |
| `src/logic/InferenceEngine.cpp` | 13 | Include + empty namespace |
| `src/logic/KnowledgeBase.cpp` | 13 | Include + empty namespace |
| `src/logic/MatcherUnifier.cpp` | 13 | Include + empty namespace |
| `src/logic/Normalizer.cpp` | 13 | Include + empty namespace |
| `src/proof/Proof.cpp` | 11 | Include + empty namespace |
| `src/proof/ProofChecker.cpp` | 12 | Include + empty namespace |

**Pattern**: These are header-only modules that declare an empty `.cpp` file solely to satisfy CMake's `add_library(STATIC ...)` requirement for at least one translation unit. The real implementation lives entirely in the `.hpp` file. `Closure.cpp` (210 lines) and `CertificateExporter.cpp` (498 lines) are the only non-stub `.cpp` files besides `Term.cpp`.

---

### 8. **[PTR-EQ]** — Pointer/ID Equality Where Structural Equality May Be Needed

| Location | Check | Risk |
|----------|-------|------|
| `Term.hpp` ~70 | `this == &o` | Cross-factory terms never equal |
| `GODfinal.hpp` ~60 | `next == current` | Fixed-point missed across factories |
| `Equation.hpp` ~80 | `lhs_->id() == rhs_->id()` | Triviality missed across factories |
| `Closure.cpp` ~198 | `id()` comparison | Same |
| `CertificateKernel.hpp` ~200 | `thm1.rhs() != thm2.lhs()` | Transitivity wrongly refused |
| `InferenceEngine.hpp` (multiple) | `->id() != ->id()` | Trivial equation check |
| `Equivalence.hpp` ~500 | `canonical1->id() == canonical2->id()` | Equivalence missed |

**Systemic root cause**: The codebase assumes **exactly one `TermFactory`** exists, but `Canonicalizer.hpp` and `NFEngine.hpp` each create their own. Any term created in those private factories is invisible to `id()`-based comparisons in the rest of the system.

---

### 9. **[PLACEHOLDER]** — Unbound RHS Variables / Dummy Merge Reasons / Placeholder Returns

| Location | Kind | Detail |
|----------|------|--------|
| `Canonicalizer.hpp` ~400 | Placeholder return | `reconstructFromENode()` → `nullptr` |
| `NFEngine.hpp` ~380 | Placeholder return | `GODTermPass::reconstructFromENode()` → `nullptr` |
| `NFEngine.hpp` ~420 | Placeholder return | `applyGaloisConjugation()` → `nullptr` for some node kinds |
| `EGraph.hpp` ~629 | Dummy merge reason | `MergeReason::fromCongruence(ENode{}, ENode{})` — empty dummy nodes |
| `EGraph.hpp` ~1123 | Unbound RHS variable | `instantiatePattern()` creates fresh leaf for unbound variable instead of erroring |
| `Algebra.hpp` ~1100 | Silent zero return | `CayleyDicksonEvaluator` returns `CD()` (zero) for unknown ops/variables |

---

### 10. **[OVERCLAIM]** — Overclaimed Guarantees

| Location | Claim | Reality |
|----------|-------|---------|
| `NFEngine.hpp` ~350 | `isNormal()` returns whether a term is in normal form | Always returns `true` — every term is "normal" |
| `Normalizer.hpp` ~800 | GODNormalizer produces **the** canonical (minimum-id) orbit representative | Orbit exploration truncated at 10,000 nodes; result depends on BFS order; idempotence can fail for large orbits |
| `KnowledgeBase.hpp` ~600 | `rewriteToFixpoint()` returns the normal form | Bounded at 100 iterations; may return a non-normal intermediate |
| `ProofChecker.hpp` ~500 | Claims "de Bruijn criterion" verification | Full-level doesn't verify actual rewrite positions; `CertificateKernel` is the real LCF kernel |
| `ProofChecker.hpp` ~480 | GOD verification at Standard level | Performs no verification — only checks at Scout level |
| `Closure.hpp` ~600 | `enumerateNormalWords()` enumerates all normal words | Exponential brute force truncated by `maxLength`; under-enumerates for large alphabets |
| `Dimension.hpp` header | Retained as "extension point" | Compiled into production binary as dead code |
| `test_canon_scalar.cpp` ~180–320 | File tests the canon-scalar pipeline | 5 integration tests disabled "pending API stabilization" |
| `Scout.hpp` ~500 | Fano-plane table "verified" | No compile-time or runtime validation |
| `main.cpp` ~330 | `check` mode demonstrates proof checking | Hardcodes commutativity, inapplicable to non-commutative algebras |

---

## Summary Statistics

| Category | Count |
|----------|-------|
| **[MOJIBAKE]** | 0 |
| **[SOUNDNESS]** | 18 |
| **[NAMING]** | 3 clusters |
| **[RAW-NEW]** | 7 sites in 1 file |
| **[SINGLETON]** | 16+ |
| **[EPSILON]** | 16 sites |
| **[STUB]** | 11 files |
| **[PTR-EQ]** | 7 sites |
| **[PLACEHOLDER]** | 6 sites |
| **[OVERCLAIM]** | 10 |

**Top 3 most impactful issues:**

1. **`superpositionPositions()` does not reconstruct $s[r\sigma]_p$** (`InferenceEngine.hpp` ~580) — any non-root superposition inference is wrong. This is the core inference engine of the prover.

2. **Multiple `TermFactory` instances** (`Canonicalizer.hpp`, `NFEngine.hpp`, `KnowledgeBase.hpp`) — breaks the pointer-equality invariant that the entire codebase depends on.

3. **`BigInt::toString()` 128-bit truncation** (`ZPhi.hpp` ~680) — incorrect decimal output for any number requiring more than one 64-bit limb, which affects proof certificates, logging, and any human-readable output of exact arithmetic results.
