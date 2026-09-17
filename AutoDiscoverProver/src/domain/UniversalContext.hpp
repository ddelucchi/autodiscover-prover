/**
 * @file UniversalContext.hpp
 * @brief UNIVERSAL CONTEXT — The transport/functor framework
 *
 * Context C = (A_n, X, G, E, Ω, μ)
 *
 * Transport theorem: T_h : C → C' is functorial
 *   T_{k∘h} = T_k ∘ T_h
 *
 * Every derivation D_{g,χ} lives in a context.
 * Every equation E=0 in context C can be transported to context C'.
 *
 * Universal closure: ∀p,q ∈ primes ∪ {∞},
 *   E=0 in Q_p ⟺ equivalent translated system in Q_q
 * 
 * This module enables the AUTODISCOVERER to discover equations that
 * hold universally across different algebraic contexts (real, complex,
 * quaternion, octonion, p-adic) and different time/space structures.
 */

#ifndef AUTODISCOVER_DOMAIN_UNIVERSAL_CONTEXT_HPP
#define AUTODISCOVER_DOMAIN_UNIVERSAL_CONTEXT_HPP

#include "../core/Term.hpp"
#include "../logic/Equation.hpp"
#include <vector>
#include <memory>
#include <string>
#include <cmath>

namespace autodiscover {
namespace domain {
namespace context {

using core::Term;
using core::TermFactory;
using core::Sort;
using logic::Equation;

// =============================================================================
// CONTEXT STRUCTURE
// =============================================================================
//
// A context C = (A_n, X, G, E, Ω, μ) consists of:
//   A_n : Cayley-Dickson algebra at level n
//   X   : space on which functions are defined
//   G   : group acting on X
//   E   : evaluation point(s)
//   Ω   : cocycle Ω_{gh}(x) = Ω_g(h·x) + Ω_h(x)
//   μ   : defect measure (BV decomposition)
//
// Transport T_h : C → C' preserves equation structure.
// =============================================================================

struct ContextSpec {
    uint8_t cdLevel = 0;       // n in A_n
    std::string spaceName;      // X identifier
    std::string groupName;      // G identifier
    bool hasDefect = false;     // whether μ ≠ 0
    bool isContinuous = true;   // T = R vs discrete
};

// =============================================================================
// UNIVERSAL CONTEXT AXIOM MODULE
// =============================================================================

class UniversalContextModule {
public:
    explicit UniversalContextModule(TermFactory& factory) : factory_(factory) {}

    [[nodiscard]] std::vector<std::unique_ptr<Equation>> generateAxioms() {
        std::vector<std::unique_ptr<Equation>> axioms;
        generateTransportAxioms(axioms);
        generateCocycleAxioms(axioms);
        generateNaturalityAxioms(axioms);
        generateCDCompatibilityAxioms(axioms);
        generateMeadowAxioms(axioms);
        // === New groupoid / categorical axioms (Interstices integration) ===
        generateGroupoidAxioms(axioms);
        generateUniversalIdentityAxioms(axioms);
        generateRepresentationAxioms(axioms);
        generateFToILiftAxioms(axioms);
        generateStructureTupleAxioms(axioms);
        generateChainRuleAxioms(axioms);
        generateDiamondAxioms(axioms);
        return axioms;
    }

private:
    TermFactory& factory_;
    uint32_t vc_ = 0;

    const Term* fv(Sort s = Sort::Generic) {
        return factory_.variable("ctx" + std::to_string(vc_++), s);
    }

    // --- Transport functor axioms ---
    void generateTransportAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto f = fv(); auto ctx1 = fv(); auto ctx2 = fv(); auto ctx3 = fv();

        // TR1: T_id = id (identity transport)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Transport", {factory_.apply("IdCtx", {}), f}),
            f
        ));

        // TR2: T_{k∘h} = T_k ∘ T_h (functoriality)
        auto h = fv(); auto k = fv();
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Transport", {factory_.apply("ComposeCtx", {k, h}), f}),
            factory_.apply("Transport", {k, factory_.apply("Transport", {h, f})})
        ));

        // TR3: Transport preserves equations:
        //   if lhs = rhs in C, then T_h(lhs) = T_h(rhs) in C'
        auto lhs = fv(); auto rhs = fv();
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Transport", {h, factory_.apply("Eq", {lhs, rhs})}),
            factory_.apply("Eq", {
                factory_.apply("Transport", {h, lhs}),
                factory_.apply("Transport", {h, rhs})})
        ));
    }

    // --- Cocycle axioms: Ω_{gh}(x) = Ω_g(h·x) + Ω_h(x) ---
    void generateCocycleAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto g = fv(); auto h = fv(); auto x = fv();

        // CC1: Cocycle condition
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Omega", {factory_.apply("ComposeG", {g, h}), x}),
            factory_.add(
                factory_.apply("Omega", {g, factory_.apply("Act", {h, x})}),
                factory_.apply("Omega", {h, x}))
        ));

        // CC2: Omega_e = 0 (identity has trivial cocycle)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Omega", {factory_.apply("IdG", {}), x}),
            factory_.scalar(0.0)
        ));

        // CC3: Master equation: Δ_g F = Ω_g · D_g F + μ_{F,g}
        auto f = fv();
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Delta_g", {g, f, x}),
            factory_.add(
                factory_.mul(
                    factory_.apply("Omega", {g, x}),
                    factory_.apply("UnivDQ", {g, factory_.apply("Chi", {g}), f, x})),
                factory_.apply("Defect_g", {f, g, x}))
        ));
    }

    // --- Naturality: D commutes with embeddings ---
    void generateNaturalityAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto f = fv(); auto x = fv(); auto g = fv();

        // NAT1: ι_{n→m} ∘ D = D ∘ ι_{n→m} (CD embedding commutes with all operators)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("iota", {factory_.apply("UnivDQ", {g, factory_.apply("Chi", {g}), f, x})}),
            factory_.apply("UnivDQ", {g, factory_.apply("Chi", {g}), factory_.apply("iota", {f}), x})
        ));

        // NAT2: π_n ∘ D = D ∘ π_n (CD projection commutes with all operators)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("pi_n", {factory_.apply("UnivDQ", {g, factory_.apply("Chi", {g}), f, x})}),
            factory_.apply("UnivDQ", {g, factory_.apply("Chi", {g}), factory_.apply("pi_n", {f}), x})
        ));
    }

    // --- Cayley-Dickson compatibility axioms ---
    void generateCDCompatibilityAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto a = fv(); auto b = fv();

        // CDC1: ι(a·b) = ι(a)·ι(b) (embedding is multiplicative)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("iota", {factory_.mul(a, b)}),
            factory_.mul(factory_.apply("iota", {a}), factory_.apply("iota", {b}))
        ));

        // CDC2: ι(a+b) = ι(a)+ι(b) (embedding is additive)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("iota", {factory_.add(a, b)}),
            factory_.add(factory_.apply("iota", {a}), factory_.apply("iota", {b}))
        ));

        // CDC3: π(ι(a)) = a (projection ∘ embedding = identity)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("pi_n", {factory_.apply("iota", {a})}),
            a
        ));

        // CDC4: N(ι(a)) = N(a) (norm preserved under embedding)
        ax.push_back(std::make_unique<Equation>(
            factory_.norm(factory_.apply("iota", {a})),
            factory_.norm(a)
        ));
    }

    // --- Meadow axioms: 0^{-1} = 0 (total algebra, no division-by-zero errors) ---
    void generateMeadowAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        // M1: 0^{-1} = 0 (meadow zero-inverse)
        ax.push_back(std::make_unique<Equation>(
            factory_.inv(factory_.scalar(0.0)),
            factory_.scalar(0.0)
        ));

        auto a = fv();

        // M2: a · a^{-1} · a = a (meadow regularity)
        ax.push_back(std::make_unique<Equation>(
            factory_.mul(a, factory_.mul(factory_.inv(a), a)),
            a
        ));

        // M3: a^{-1} · a · a^{-1} = a^{-1} (Von Neumann regularity of inverse)
        ax.push_back(std::make_unique<Equation>(
            factory_.mul(factory_.inv(a), factory_.mul(a, factory_.inv(a))),
            factory_.inv(a)
        ));
    }

    // =========================================================================
    // GROUPOID Ð_g OPERATOR (Interstices §CXIII final groupoid framework)
    // =========================================================================
    //
    // Given a groupoid G with objects (A_a)_{a ∈ G_0}, morphisms g ∈ G_1:
    //   λ_g ∈ A_{s(g)}^×           (invertible weight)
    //   α_g := ln(λ_g)             (cocycle value)
    //   E_g: actions on functions   (shift operator)
    //   Δ_g := E_g − Id            (difference operator)
    //   Ð_g := α_g^{-1} · Δ_g     (THE universal interstice derivative)
    //
    // Fundamental properties:
    //   E_g F = F + α_g · Ð_g F
    //   Ð_g(FH) = (Ð_g F)·(E_g H) + F·(Ð_g H)     (Leibniz)
    //   Ð_g(F^{-1}) = −(E_g F)^{-1} (Ð_g F) F^{-1} (inverse rule)
    //   E_{g^n} F − F = Σ_{k=0}^{n-1} E_{g^k}(α_g · Ð_g F)   (FTC)
    //   α_{g∘h} · Ð_{g∘h} F = E_h(α_g · Ð_g F) + α_h · Ð_h F (composition)
    //   F(g,h) := (E_h α_g + α_h)·Ð_{g∘h} − E_h(α_g·Ð_g) − α_h·Ð_h = 0
    //
    // Newton limit: λ_g = e^{εω_g}, lim_{ε→0} Ð_g F = ω_g^{-1} X_g F
    // =========================================================================
    void generateGroupoidAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto f = fv(); auto h_term = fv();
        auto g = fv(); auto gp = fv(); auto x = fv();

        // GÐ1: E_g F = F + α_g · Ð_g F (shift decomposition)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("E_g", {g, f, x}),
            factory_.add(
                factory_.apply("eval", {f, x}),
                factory_.mul(
                    factory_.apply("Alpha_g", {g}),
                    factory_.apply("Eth_g", {g, f, x})))
        ));

        // GÐ2: Ð_g(FH) = (Ð_g F)(E_g H) + F(Ð_g H) (Leibniz rule)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Eth_g", {g, factory_.mul(f, h_term), x}),
            factory_.add(
                factory_.mul(
                    factory_.apply("Eth_g", {g, f, x}),
                    factory_.apply("E_g", {g, h_term, x})),
                factory_.mul(
                    factory_.apply("eval", {f, x}),
                    factory_.apply("Eth_g", {g, h_term, x})))
        ));

        // GÐ3: Ð_g(F^{-1}) = −(E_g F)^{-1} (Ð_g F) F^{-1} (inverse rule)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Eth_g", {g, factory_.inv(f), x}),
            factory_.neg(factory_.mul(
                factory_.inv(factory_.apply("E_g", {g, f, x})),
                factory_.mul(
                    factory_.apply("Eth_g", {g, f, x}),
                    factory_.inv(factory_.apply("eval", {f, x})))))
        ));

        // GÐ4: Cocycle composition α_{g∘h} = E_h α_g + α_h
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Alpha_g", {factory_.apply("ComposeG", {g, gp})}),
            factory_.add(
                factory_.apply("E_g", {gp, factory_.apply("Alpha_g", {g}), x}),
                factory_.apply("Alpha_g", {gp}))
        ));

        // GÐ5: Curvature flatness F(g,h) = 0
        //       (E_h α_g + α_h)·Ð_{g∘h} = E_h(α_g Ð_g) + α_h Ð_h
        ax.push_back(std::make_unique<Equation>(
            factory_.mul(
                factory_.add(
                    factory_.apply("E_g", {gp, factory_.apply("Alpha_g", {g}), x}),
                    factory_.apply("Alpha_g", {gp})),
                factory_.apply("Eth_g", {factory_.apply("ComposeG", {g, gp}), f, x})),
            factory_.add(
                factory_.apply("E_g", {gp,
                    factory_.mul(
                        factory_.apply("Alpha_g", {g}),
                        factory_.apply("Eth_g", {g, f, x})), x}),
                factory_.mul(
                    factory_.apply("Alpha_g", {gp}),
                    factory_.apply("Eth_g", {gp, f, x})))
        ));

        // GÐ6: Identity morphism: α_e = 0, Ð_e = 0
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Alpha_g", {factory_.apply("IdG", {})}),
            factory_.scalar(0.0)
        ));
    }

    // =========================================================================
    // UNIVERSAL IDENTITY UNI_p (Section X boxed equation)
    // =========================================================================
    //
    // UNI_p[f; g,χ,A,F] :=
    //   J^(N)_{g,χ}(∇^A_{g,χ} f) − (f(g^N·) − f) − ⟨F, [·→g^N·]⟩ = 0
    //
    // Transport: T_{p→q}(UNI_p[...]) = 0  ⟺  UNI_q[T f; g,χ, T A, T F] = 0
    // =========================================================================
    void generateUniversalIdentityAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto f = fv(); auto g = fv(); auto x = fv();
        auto A_conn = fv(); auto F_curv = fv();

        // UNI1: UNI_p[f; g,χ,A,F] = 0 (universal identity)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("UNI_p", {f, g, A_conn, F_curv, x}),
            factory_.scalar(0.0)
        ));

        // UNI2: Transport preserves universal identity
        //   T_{p→q}(UNI_p[...]) = UNI_q[T f; ...]
        auto h_transport = fv();
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Transport", {h_transport,
                factory_.apply("UNI_p", {f, g, A_conn, F_curv, x})}),
            factory_.apply("UNI_p", {
                factory_.apply("Transport", {h_transport, f}),
                g,
                factory_.apply("Transport", {h_transport, A_conn}),
                factory_.apply("Transport", {h_transport, F_curv}),
                x})
        ));

        // UNI3: ∇^A_{g,χ} = D_{g,χ} + A (covariant derivative)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("CovD", {g, A_conn, f, x}),
            factory_.add(
                factory_.apply("UnivDQ", {g, factory_.apply("Chi", {g}), f, x}),
                factory_.mul(A_conn, factory_.apply("eval", {f, x})))
        ));

        // UNI4: Curvature F = Δ_g A + A ∧ A (field strength)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("FieldStrength", {g, A_conn, x}),
            factory_.add(
                factory_.apply("Delta_g", {g, A_conn, x}),
                factory_.mul(A_conn, A_conn))
        ));

        // UNI5: T_{p→q}(F) = F (curvature is transport-invariant)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Transport", {h_transport,
                factory_.apply("FieldStrength", {g, A_conn, x})}),
            factory_.apply("FieldStrength", {g,
                factory_.apply("Transport", {h_transport, A_conn}), x})
        ));
    }

    // =========================================================================
    // REPRESENTATION CATEGORY Rep(E) (Section CXII)
    // =========================================================================
    //
    // Rep(E) := {(B, h, A_B, M_B) : E_B = 0}
    // (B,h) ~ (C,k)  ⟺  ∃ ℓ:B→C : Φ_ℓ(E_B) = E_C
    // [E] = {E_B : B any} / ~
    //
    // E_B := (◊_B + A_B)² − (id − Π_B)I₂ − M_B = 0  (master equation)
    // h:B→C  ⟹  Φ_h(E_B) = E_C
    // E_B = 0 ⟹ E_C = 0  (equations transport through morphisms)
    // =========================================================================
    void generateRepresentationAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto f = fv(); auto A_conn = fv(); auto M_curv = fv();

        // REP1: E_B := (◊ + A)² − (id − Π) − M = 0 (master field equation)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("FieldEq_B", {A_conn, M_curv, f}),
            factory_.scalar(0.0)
        ));

        // REP2: Transport preserves field equation
        //   Φ_h(E_B) = E_C  where A_C = Φ_h(A_B), M_C = Φ_h(M_B)
        auto h_morph = fv();
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Transport", {h_morph,
                factory_.apply("FieldEq_B", {A_conn, M_curv, f})}),
            factory_.apply("FieldEq_B", {
                factory_.apply("Transport", {h_morph, A_conn}),
                factory_.apply("Transport", {h_morph, M_curv}),
                factory_.apply("Transport", {h_morph, f})})
        ));

        // REP3: E_B = 0 ⟹ E_C = 0 (consistency under transport)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("FieldEq_B", {A_conn, M_curv, f}),
            factory_.apply("FieldEq_B", {
                factory_.apply("Transport", {h_morph, A_conn}),
                factory_.apply("Transport", {h_morph, M_curv}),
                factory_.apply("Transport", {h_morph, f})})
        ));
    }

    // =========================================================================
    // FULL FToI LIFT L_{n→n+1} (Sections XXII–XXV)
    // =========================================================================
    //
    // L_{n→n+1}(f_n)(x) := ι_n(f_n(π_n x))     (lift operator)
    //
    // Commutativity diagrams:
    //   U_g^{(n+1)} ∘ L_{n→n+1} = L_{n→n+1} ∘ U_g^{(n)}
    //   𝔇_g^{(n+1)} ∘ L_{n→n+1} = L_{n→n+1} ∘ 𝔇_g^{(n)}
    //   J_{g^m}^{(n+1)} ∘ L_{n→n+1} = L_{n→n+1} ∘ J_{g^m}^{(n)}
    //
    // FToI_{n→n+1}: f_n(y) − f_n(x) = π_n · J^{(n+1)}(D^{(n+1)}(L f_n))
    //
    // Equation transport:
    //   E_{n+1}[L f_n] = L(E_n[f_n])
    //   E_n[f_n] = 0 ⟹ E_{n+1}[L f_n] = 0 ⟹ π_n E_{n+1}[L f_n] = E_n[f_n] = 0
    // =========================================================================
    void generateFToILiftAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto f = fv(); auto x = fv(); auto g = fv();

        // LIFT1: L_{n→n+1}(f_n) := ι_n(f_n(π_n(·)))
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Lift", {f, x}),
            factory_.apply("iota", {factory_.apply("eval", {f, factory_.apply("pi_n", {x})})})
        ));

        // LIFT2: U_g ∘ L = L ∘ U_g (shift commutes with lift)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("E_g", {g, factory_.apply("Lift", {f, x}), x}),
            factory_.apply("Lift", {factory_.apply("E_g", {g, f, x}), x})
        ));

        // LIFT3: D_g ∘ L = L ∘ D_g (derivative commutes with lift)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Eth_g", {g, factory_.apply("Lift", {f, x}), x}),
            factory_.apply("Lift", {factory_.apply("Eth_g", {g, f, x}), x})
        ));

        // LIFT4: J_g ∘ L = L ∘ J_g (integral commutes with lift)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("IntersticeJ", {g, factory_.apply("Lift", {f, x}), x}),
            factory_.apply("Lift", {factory_.apply("IntersticeJ", {g, f, x}), x})
        ));

        // LIFT5: π ∘ L = id (projection recovers original)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("pi_n", {factory_.apply("Lift", {f, x})}),
            factory_.apply("eval", {f, x})
        ));

        // LIFT6: E_{n+1}[L(f_n)] = L(E_n[f_n]) (equation lift)
        auto eq = fv();
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("eval", {eq, factory_.apply("Lift", {f, x})}),
            factory_.apply("Lift", {factory_.apply("eval", {eq, f}), x})
        ));
    }

    // =========================================================================
    // STRUCTURE TUPLE U (Section XXIX)
    // =========================================================================
    //
    // U := ({A_n}, {ι_n, π_n}) × (Γ, χ, {U_g}) × (D, J) × (U, Q_n)
    //
    // Master identities for all n, all g ≠ e, all m ∈ ℕ, all f_n:
    //   (U_g J_{g^m} D_g − Id + U_{g^m}) f_n = 0
    //   (D_g^{(n+1)} ∘ L − L ∘ D_g^{(n)}) = 0
    //   (J_{g^m}^{(n+1)} ∘ L − L ∘ J_{g^m}^{(n)}) = 0
    //   E_{n+1}[L f_n] = L(E_n[f_n])
    // =========================================================================
    void generateStructureTupleAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto f = fv(); auto x = fv(); auto g = fv();

        // STU1: (U_g J D_g − Id + U_{g^m}) f = 0  (master FTC identity)
        ax.push_back(std::make_unique<Equation>(
            factory_.add(
                factory_.apply("E_g", {g,
                    factory_.apply("IntersticeJ", {g,
                        factory_.apply("Eth_g", {g, f, x}), x}), x}),
                factory_.add(
                    factory_.neg(factory_.apply("eval", {f, x})),
                    factory_.apply("E_gN", {g, f, x}))),
            factory_.scalar(0.0)
        ));

        // STU2: All operators commute with CD tower embeddings
        //       ι(D_g f) = D_g(ι f)  (already in NAT1, restated for structure)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("iota", {factory_.apply("Eth_g", {g, f, x})}),
            factory_.apply("Eth_g", {g, factory_.apply("iota", {f}), x})
        ));

        // STU3: Scale covariance S_Λ^* μ = Λ^σ μ  ⟹  μ = ρ^{-(σ+Q)} C_{Λ,A}^* μ̂
        auto sigma = fv();
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("ScalePull", {factory_.apply("Lambda", {}), f}),
            factory_.mul(
                factory_.apply("LambdaPow", {sigma}),
                f)
        ));

        // STU4: κ = 1/3 critical exponent forcing
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("kappa", {}),
            factory_.apply("div", {factory_.scalar(1.0), factory_.scalar(3.0)})
        ));
    }

    // =========================================================================
    // CHAIN RULE AXIOM (Section XXVIII)
    // =========================================================================
    //
    // Ð_g(Φ ∘ f) = [Φ(U_g f) − Φ(f)] / [U_g f − f] · Ð_g f
    // =========================================================================
    void generateChainRuleAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto phi = fv(); auto f = fv(); auto g = fv(); auto x = fv();

        // CHAIN1: D_g(Φ∘f) = difference_quotient(Φ, f, g) · D_g(f)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Eth_g", {g,
                factory_.apply("compose", {phi, f}), x}),
            factory_.mul(
                factory_.apply("DiffQuot", {phi, f,
                    factory_.apply("E_g", {g, f, x}),
                    factory_.apply("eval", {f, x})}),
                factory_.apply("Eth_g", {g, f, x}))
        ));

        // CHAIN2: Newton limit recovering standard chain rule d/dx(φ∘f) = φ'(f)·f'
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("NewtonLimit", {
                factory_.apply("Eth_g", {g,
                    factory_.apply("compose", {phi, f}), x})}),
            factory_.mul(
                factory_.apply("deriv", {phi, factory_.apply("eval", {f, x})}),
                factory_.apply("NewtonLimit", {
                    factory_.apply("Eth_g", {g, f, x})}))
        ));
    }

    // =========================================================================
    // DIAMOND OPERATOR AXIOMS (Sections CIV–CXI)
    // =========================================================================
    //
    // ◊_B = [[0, D], [S, 0]]
    // ◊² = (id − Π)·I₂
    // (◊ + A)² = (id − Π)I₂ + M
    // h:B→C ⟹ h_* ◊_B = ◊_C h_*
    // =========================================================================
    void generateDiamondAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto f = fv(); auto g_upper = fv(); auto g_lower = fv();

        // DIA1: ◊²(F,F) = ((id−Π)F, (id−Π)F)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("DiamondSq", {f}),
            factory_.apply("IdMinusPi", {f})
        ));

        // DIA2: (◊+A)² = (id−Π)I₂ + M  where M = ◊A + A◊ + A²
        auto A_conn = fv(); auto M_curv = fv();
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("DiamondConnSq", {A_conn, f}),
            factory_.add(
                factory_.apply("IdMinusPi", {f}),
                factory_.apply("CurvatureM", {A_conn, f}))
        ));

        // DIA3: Π · ◊² = 0  (projection kills derivatives)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Pi_B", {factory_.apply("DiamondSq", {f})}),
            factory_.scalar(0.0)
        ));

        // DIA4: Transport h_* ◊_B = ◊_C h_* (naturality of diamond)
        auto h = fv();
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("Transport", {h, factory_.apply("Diamond", {f})}),
            factory_.apply("Diamond", {factory_.apply("Transport", {h, f})})
        ));
    }
};

} // namespace context
} // namespace domain
} // namespace autodiscover

#endif // AUTODISCOVER_DOMAIN_UNIVERSAL_CONTEXT_HPP
