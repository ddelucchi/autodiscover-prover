# AutoDiscoverProver

AutoDiscoverProver is an experimental C++17 equational-reasoning and symbolic-discovery engine. It combines hash-consed term construction, rewriting, e-graph-style equivalence machinery, exact algebraic evaluation, proof objects, certificate checking, and bounded candidate discovery.

The project is intentionally public with its audit history visible.

## What "discovery" means here

In this repository, discovery is a software pipeline:

1. generate bounded candidate terms from an explicit grammar
2. evaluate eligible ground terms in exact algebraic domains when possible
3. group candidates that evaluate to the same exact value
4. attempt to lift candidate equalities into proof steps
5. check the resulting certificate against the registered rules
6. retain only equations that satisfy the configured verification path

That process is not equivalent to a claim that the software autonomously proves arbitrary mathematics or establishes external novelty.

## Core components

- term/type/symbol infrastructure
- equation and knowledge-base representation
- matching, unification, normalization, and inference
- proof objects, explicit trusted-axiom registration, and proof checking
- exact `Z[phi]` arithmetic with arbitrary-precision support
- prefix-free structural encoding
- canonicalization and semantic fingerprints
- content-addressable proof storage
- e-graph and rule-mining machinery
- scalar and multi-algebra candidate discovery
- Cayley-Dickson, SCOUT, and related experimental domain modules

## Audit trail

The repository keeps the uncomfortable material on purpose:

- [AUDIT_REPORT.md](AUDIT_REPORT.md) records earlier soundness, identity, and implementation problems.
- [EXHAUSTIVE_AUDIT_REPORT.md](EXHAUSTIVE_AUDIT_REPORT.md) specifically checks whether derivable mathematical identities were being pre-seeded.
- [FIX_REPORT.md](FIX_REPORT.md) records the remediation pass and build/test state at that time.

Current source should be judged against current code, not against the earlier bug list. For example, the phi-ring normalizer now explicitly removes several derivable identities from the preset rewrite table, and the scalar-discovery path documents which relations are definitions versus candidates for discovery.

## Build and test

```bash
cmake -S . -B build \
  -DBUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The CMake suite registers tests for the golden-ratio domain, terms, normalization, canonical scalar behavior, property/round-trip behavior, deep modules, scalar discovery, and universal multi-algebra discovery.

## Trust boundary and limitations

This is experimental theorem/discovery software.

Important limits include:

- `ProofChecker` rejects `Axiom` leaves by default unless their canonical equation has been explicitly registered in the checker trust base; a caller can opt out only for legacy/debug proof objects whose assumptions are tracked externally
- candidate generation is bounded
- some normalization passes remain explicit extension stubs
- domain axioms determine what can be derived
- implementation tests are self-authored and are not independent mathematical validation
- numerical or structural checks outside exact domains do not become theorems merely because a residual is small
- the presence of a proof object is meaningful only relative to the explicitly registered axiom trust base, checker implementation, and registered inference rules

The audit files are therefore part of the project, not historical clutter.

## Repository layout

The active CMake project now lives at repository root. Historical equation dumps, exploratory outputs, and one-off scripts are retained under `archive/legacy/` for provenance and are not part of the build.

## License

Source is publicly viewable for portfolio and technical evaluation. See [LICENSE](LICENSE).


## Verification status

The repository contains self-authored regression/property suites for exact ring arithmetic, normalization, term encoding, proof/certificate replay, discovery, e-graph behavior, and multi-domain modules. The public source has also hardened the proof-checker leaf boundary so an arbitrary equation cannot become trusted merely by being labelled `Axiom`.

GitHub Actions is configured for build, test, and sanitizer jobs, but the account currently reports workflow startup failures before job creation. Treat the clone-local CMake/CTest commands above as the executable verification path until hosted runner execution is restored.
