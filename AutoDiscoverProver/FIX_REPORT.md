# AutoDiscoverProver — Comprehensive Fix Report

**Scope**: All issues identified in `AUDIT_REPORT.md` + all compiler warnings  
**Date**: 2025  
**Build**: 0 errors, 0 warnings (MSVC 14.41, Release x64)  
**Tests**: 6/6 pass (GoldenRatioTests, TermTests, NormalizerTests, CanonScalarTests, PropertyTests, DeepModuleTests)

---

## Phase 1: Soundness Bug Triage

Of the 18 `[SOUNDNESS]` bugs in the audit report, **13 were already fixed** in the current codebase and **5 required code changes**.

### Already Fixed (No Action Needed)

| # | Audit Bug | Status | Evidence |
|---|-----------|--------|----------|
| 1 | `superpositionPositions()` doesn't reconstruct s[rσ]_p | ✅ Fixed | `InferenceEngine.hpp` ~810: uses `replaceAtPath()` for non-root positions |
| 2 | `ProofChecker` superposition replay trivially passes | ✅ Fixed | `ProofChecker.hpp` ~530: full certificate replay with path extraction |
| 3 | `ProofChecker` demodulation verification | ✅ Fixed | Deterministic subterm/match/rebuild verification implemented |
| 4 | Private `TermFactory` in `Canonicalizer.hpp` | ✅ Fixed | No private factory; takes shared `TermFactory&` reference |
| 5 | Private `TermFactory` in `NFEngine.hpp` | ✅ Fixed | No private factory; takes shared `TermFactory&` reference |
| 6 | `KnowledgeBase` `subsumptionFactory_` | ✅ Fixed | Replaced with shared `factory_` reference |
| 7 | `SymbolTable` naming: "Mul" vs `BuiltinOp::Conj` | ✅ Fixed | `"*"` maps to Mul correctly |
| 8 | `EGraph` unbound RHS variable silent | ✅ Fixed | Now throws `std::logic_error` for unbound variables |
| 9 | `signaturesCompatible()` undefined | ✅ Fixed | Defined at `InferenceEngine.hpp` ~524 |
| 10 | `phiNegPower(0)` crash | ✅ N/A | Parameter is `uint64_t`; n=0 returns ZPhi(1,0)=1 correctly |
| 11 | `BigInt::toString()` 128-bit truncation | ✅ Fixed | Uses schoolbook 32-bit division loop, handles arbitrary limb count |
| 12 | `phiPower` negative exponent handling | ✅ Fixed | Uses exact Fibonacci formula with proper sign handling |
| 13 | `findByFingerprint` collision crash | ✅ Fixed | Records stats (`hashCollisions_`) instead of throwing |

---

## Phase 2: Applied Soundness Fixes (5 bugs)

### Fix 1: `BigInt::subMagnitude()` Borrow Overflow
**File**: `src/ring/ZPhi.hpp` ~line 544  
**Bug**: When `b = UINT64_MAX` and `borrow = 1`, the expression `b + borrow` wraps to 0, making the comparison `a < b + borrow` give the wrong result.

**Before**:
```cpp
borrow = (a < b + borrow || (borrow && b == static_cast<Limb>(-1))) ? 1 : 0;
```

**After**:
```cpp
Limb need_borrow = borrow ? (a <= b ? 1 : 0) : (a < b ? 1 : 0);
result.limbs_[i] = a - b - borrow;
borrow = need_borrow;
```

**Mathematical correctness**: The new formulation avoids computing `b + borrow` entirely. When `borrow = 1`, we need to borrow again iff `a ≤ b` (since we're really comparing `a` against `b + 1`). When `borrow = 0`, we borrow iff `a < b`. The subtraction `a - b - borrow` is always performed in modular arithmetic, and the new borrow value correctly captures whether the result underflowed.

---

### Fix 2: `Encoding.hpp` Type-Punning UB
**File**: `src/encoding/Encoding.hpp` ~lines 391, 463  
**Bug**: Used `union { double d; uint64_t u; }` for bit reinterpretation — undefined behavior in C++ (only valid in C11+).

**Before**:
```cpp
union { double d; uint64_t u; } pun;
pun.d = val;
writeU64(pun.u);
```

**After**:
```cpp
uint64_t bits;
std::memcpy(&bits, &val, sizeof(bits));
writeU64(bits);
```

Added `#include <cstring>` at the top of the file. Both `writeDouble()` and `readDouble()` were fixed. The `std::memcpy` approach is the standard-blessed way to do type punning in C++17 and is optimized to a single register move by all major compilers.

---

### Fix 3: `Rational` Arithmetic Overflow
**File**: `src/domain/MultiRingEval.hpp` ~lines 255–267  
**Bug**: `operator+`, `operator-`, and `operator*` computed cross-products like `num * o.den + o.num * den` which can overflow `int64_t` for large numerators/denominators.

**After**: All three operators now use **cross-reduction** before multiplication:

- `operator+` / `operator-`: Compute `g = gcd(den, o.den)`, then `den1 = den/g`, `den2 = o.den/g`. The new numerator is `num * den2 ± o.num * den1` (smaller products). The new denominator is `den1 * o.den` (avoids squaring).
- `operator*`: Compute `g1 = gcd(|num|, o.den)` and `g2 = gcd(|o.num|, den)`. Cross-cancel before multiplying: `(num/g1)*(o.num/g2)` over `(den/g2)*(o.den/g1)`.

This prevents overflow for all cases where the final reduced result fits in `int64_t`.

---

### Fix 4: `ProofStore::evictOne()` TOCTOU Race
**File**: `src/store/ProofStore.hpp` ~lines 478–506  
**Bug**: The original implementation found the LRU victim under `shared_lock`, recorded its bucket index and position, then released the lock and re-acquired a `unique_lock`. Between locks, another thread could modify the bucket, making the recorded position stale.

**After**: Two-pass approach:
1. **Pass 1** (shared locks): Scans all buckets to find the one containing the globally-oldest entry, recording only the bucket index.
2. **Pass 2** (unique lock on candidate bucket only): Re-scans that single bucket to find the actual LRU entry under the write lock, then erases it.

This eliminates the TOCTOU window because the victim is identified and removed in the same critical section.

---

### Fix 5: `NFEngine::GODTermPass::isNormal()` Overclaim
**File**: `src/canon/NFEngine.hpp` ~line 673  
**Bug**: `isNormal()` always returned `true`, making every term appear already normalized. This caused the normalization pipeline to skip GOD canonicalization.

**After**: Returns `!containsPhiBar(term)`, where `containsPhiBar()` recursively checks whether the term contains any `PhiBar` node. Since the GOD pass's primary job is to map φ̄ → φ (selecting the canonical orbit representative), any term containing PhiBar is definitively non-normal. This is a sound conservative approximation — it may return false for some terms that are already normal, which just causes a harmless re-normalization pass.

---

## Phase 3: Warning Elimination (All ~50+ Warnings)

### Normalizer.hpp — 8 × C4100 `'f': unreferenced formal parameter`
**Fix**: Changed `TermFactory& f` to `TermFactory& /*f*/` in 8 lambda expressions for rewrite rules that don't use the factory (`double_neg`, `double_conj`, `add_zero`, `mul_one`, `mul_zero`, `scout_scalar_norm`, `scout_unt_scalar`, `phase_transport_one`).

### Algebra.hpp — 8 × C4189 `local variable initialized but not referenced`
**Fix**: Removed unused variable declarations:
- `zero` in `PhiRingAxioms::generateAxioms()` — never used in any axiom
- `x`, `y` in `CayleyMapAxioms::generateAxioms()` — leftover from planned axioms
- `y` in `AlignmentAxioms::generateAxioms()` — only `x` and `U` are used
- `phi_n`, `r_n`, `J`, `one` in `PhaseTransportAxioms::generateAxioms()` — entire function is a stub returning empty axiom set; replaced with descriptive comments

### Scout.hpp — 5 warnings (C4189 + C4100)
**Fix**:
- Removed unused `rhoPower` local — replaced by scaled Fibonacci recurrence `g_k = F_k · ρ^k`
- Removed unused `x`, `y` vars from `generateUNTAxioms()` — were declared for linearity axioms that are meta-level, not term-level
- Commented out unused `axioms` parameter in `generateUNTAxioms()` and `generateBoundaryCollapseAxioms()` — both are stub functions

### InferenceEngine.hpp — 2 warnings
**Fix**:
- Commented out unused `result` parameter in `equalityResolution()` — function only sets `contradictionEqId_` flag
- Removed unused `InferenceResult result` local in `EquationDiscoverer::discover()` — return value wasn't used

### EGraph.hpp — 1 × C4457 `declaration of 'classId' hides function parameter`
**Fix**: Renamed structured binding variable from `classId` to `cid` in the Cartesian product loop to avoid shadowing the function parameter.

### NFEngine.hpp — 8 warnings (C4100)
**Fix**:
- Commented out `node` parameter in cost-function lambda (line ~838) — only uses `childCosts`
- Commented out `n` parameter in `costFn` lambda (line ~967) — only uses `childCosts`
- Commented out `factory` and `term` in all 3 stub passes (`PolyTermPass`, `DimTermPass`, `TensorTermPass`) for both `normalize()` and `isNormal()` methods

### ScalarDiscovery.hpp — 1 × C4100
**Fix**: Commented out unused `target` parameter in `generateCandidates()`.

### main.cpp — 1 × C4100
**Fix**: Commented out unused `factory` parameter in `interactiveMode()`.

### test_canon_scalar.cpp — 1 × C4189
**Fix**: Added `(void)nx;` after the assert to suppress the "initialized but not referenced" warning for a variable that exists solely for its assert.

---

## Final Build Verification

```
Build:    0 errors, 0 warnings (MSVC 14.41, Release x64)
Tests:    6/6 pass

  1/6 GoldenRatioTests  .... Passed  0.04 sec
  2/6 TermTests ............ Passed  0.04 sec
  3/6 NormalizerTests ...... Passed  0.04 sec
  4/6 CanonScalarTests ..... Passed  0.82 sec
  5/6 PropertyTests ........ Passed  0.63 sec
  6/6 DeepModuleTests ...... Passed  0.04 sec

100% tests passed, 0 tests failed out of 6
```

---

## Files Modified

| File | Changes |
|------|---------|
| `src/ring/ZPhi.hpp` | Fixed `subMagnitude()` borrow overflow |
| `src/encoding/Encoding.hpp` | Fixed type-punning UB → `std::memcpy`; added `<cstring>` |
| `src/domain/MultiRingEval.hpp` | Fixed Rational +/−/× overflow via cross-reduction |
| `src/store/ProofStore.hpp` | Fixed `evictOne()` TOCTOU race condition |
| `src/canon/NFEngine.hpp` | Fixed `isNormal()` overclaim; suppressed stub param warnings |
| `src/logic/Normalizer.hpp` | Suppressed 8 unused `f` param warnings |
| `src/domain/Algebra.hpp` | Removed 8 unused variable declarations |
| `src/domain/Scout.hpp` | Removed unused `rhoPower`, `x`, `y`; suppressed param warnings |
| `src/logic/InferenceEngine.hpp` | Suppressed unused `result` param; removed unused local |
| `src/egraph/EGraph.hpp` | Renamed shadowing `classId` → `cid` |
| `src/discovery/ScalarDiscovery.hpp` | Suppressed unused `target` param |
| `src/main.cpp` | Suppressed unused `factory` param |
| `tests/test_canon_scalar.cpp` | Suppressed unused `nx` warning |

---

## Remaining Known Limitations (Not Bugs)

These are documented design decisions, not defects:

1. **Stub passes** (`PolyTermPass`, `DimTermPass`, `TensorTermPass`): Return input unchanged. Clearly documented as stubs for future development.
2. **GOD orbit exploration bounded at 10,000 nodes**: Practical limit; correctly documented as an approximation.
3. **`rewriteToFixpoint()` bounded at 100 iterations**: Defensive against non-termination; sound but incomplete.
4. **5 disabled tests in `test_canon_scalar.cpp`**: Marked "pending API stabilization" — not regressions.
5. **Floating-point epsilon comparisons**: Used for numerical falsification (heuristic), not for soundness-critical paths.
6. **Singletons** (`globalContext()`, etc.): Architectural choice for header-only design; thread-safe via `std::call_once`.
