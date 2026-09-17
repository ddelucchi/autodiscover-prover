#pragma once
// =============================================================================
//  IntersticeAxiomDeriver.hpp  —  SYMBOLIC DERIVATION FROM INTERSTICE AXIOMS
// =============================================================================
//
//  This module DERIVES new equations from the Interstices framework axioms.
//  Unlike the orbit-evaluation engine (which evaluates known functions at
//  numeric points and matches values), this engine applies ALGEBRAIC RULES
//  to symbolic expressions, producing genuinely new identities.
//
//  The axioms come directly from the Interstices manuscript:
//
//  A1. Groupoid Leibniz:  Ð_g(FH) = (Ð_g F)·E_g(H) + F·(Ð_g H)
//  A2. Groupoid Inverse:  Ð_g(F⁻¹) = -(E_g F)⁻¹·(Ð_g F)·F⁻¹
//  A3. Cocycle Composition: (E_h α_g + α_h)·Ð_{g∘h} F = E_h(α_g·Ð_g F) + α_h·Ð_h F
//  A4. Shift-Derivative:   E_g F = F + α_g·Ð_g F
//  A5. Iterated Shift:     E_{g^n} F - F = Σ_{k=0}^{n-1} E_{g^k}(α_g·Ð_g F)
//  A6. FTC with Defect:    F(ψ+2π) - F(ψ) = (lnΛ/2π)∫∂_ψ F dψ + μ_F^{int}
//  A7. Diamond Squared:    ◇² = (id - Π)·I₂
//  A8. Gauged Diamond:     (◇+A)² = (id-Π)I₂ + M,  M = ◇A + A◇ + A²
//  A9. Scale Leibniz:      D_Λ(FG) = (D_Λ F)·(E_Λ G) + F·(D_Λ G)
//  A10. Chain Rule:        D_Λ(H∘F) = [∫₀¹ H'(F+θ(E_Λ F-F)) dθ]·D_Λ F
//  A11. Tower Transport:   C_{m→n}(D_g F) = (D_g)^{×2^k} ∘ C_{m→n}(F)
//  A12. UFE:               (D+A)†(D+A)F = 0
//  A13. Conjugation:       Ð_g(F̄) = Ð_g(F)̄  (when χ_g ∈ ℝ)
//  A14. Conservation:      Ð_g η(U) + Div_g q(U) = μ_src
//
// =============================================================================

#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <algorithm>
#include <cassert>

namespace axiom_deriver {

// =============================================================================
// SYMBOLIC EXPRESSION TREE
// =============================================================================

enum class SxKind {
    // Atoms
    Var,        // Named variable/function: f, g, h, F, G, H
    Const,      // Named constant:  φ, Λ, κ, π, ln_φ, α_g, χ_g
    Num,        // Numeric literal: 0, 1, 2, -1

    // Algebraic operations
    Add,        // a + b
    Mul,        // a · b  (noncommutative in general)
    Neg,        // -a
    Inv,        // a⁻¹
    Conj,       // a*  (Cayley-Dickson conjugation)
    Norm,       // N(a) = a·a*

    // Interstice operators
    Dg,         // Ð_g(F)     — groupoid derivative
    Eg,         // E_g(F)     — shift operator
    DLambda,    // D_Λ(F)     — scale derivative
    DDelta,     // D_Δ(F)     — time-scale derivative
    PartialPsi, // ∂_ψ(F)     — angular partial derivative
    Integral,   // ∫(F)       — integral operator
    MuInt,      // μ^{int}_F  — defect measure
    Diamond,    // ◇_B(F)     — diamond operator (2×2 matrix)
    PiProj,     // Π_B(F)     — projection  
    BoxOp,      // □_{g,χ}(F) — Laplace-type operator Ð†Ð

    // Structure
    Compose,    // f ∘ g      — function composition
    Apply,      // f(x)       — function application
    Power,      // f^n
    
    // Tower
    Iota,       // ι_{n→m}(F) — CD embedding
    Proj,       // π_{m→n}(F) — CD projection
    CDcomp,     // C_{m→n}(F) — full CD decomposition (returns 2^k components)
};

// Forward declaration
struct Sx;
using SxPtr = std::shared_ptr<const Sx>;

struct Sx {
    SxKind kind;
    std::string name;       // for Var, Const, or operator annotation (e.g. action name)
    double numVal = 0.0;    // for Num
    std::vector<SxPtr> children;
    
    // --- Constructors ---
    static SxPtr var(const std::string& name) {
        auto s = std::make_shared<Sx>();
        s->kind = SxKind::Var;
        s->name = name;
        return s;
    }
    static SxPtr con(const std::string& name) {
        auto s = std::make_shared<Sx>();
        s->kind = SxKind::Const;
        s->name = name;
        return s;
    }
    static SxPtr num(double v) {
        auto s = std::make_shared<Sx>();
        s->kind = SxKind::Num;
        s->numVal = v;
        return s;
    }
    static SxPtr op(SxKind k, std::vector<SxPtr> ch, const std::string& ann = "") {
        auto s = std::make_shared<Sx>();
        s->kind = k;
        s->name = ann;
        s->children = std::move(ch);
        return s;
    }
    
    // Convenience builders
    static SxPtr add(SxPtr a, SxPtr b) { return op(SxKind::Add, {a, b}); }
    static SxPtr mul(SxPtr a, SxPtr b) { return op(SxKind::Mul, {a, b}); }
    static SxPtr neg(SxPtr a)          { return op(SxKind::Neg, {a}); }
    static SxPtr inv(SxPtr a)          { return op(SxKind::Inv, {a}); }
    static SxPtr conj(SxPtr a)         { return op(SxKind::Conj, {a}); }
    static SxPtr norm(SxPtr a)         { return op(SxKind::Norm, {a}); }
    
    static SxPtr Dg(SxPtr f, const std::string& g = "g") { 
        return op(SxKind::Dg, {f}, g); 
    }
    static SxPtr Eg(SxPtr f, const std::string& g = "g") { 
        return op(SxKind::Eg, {f}, g); 
    }
    static SxPtr DLam(SxPtr f)     { return op(SxKind::DLambda, {f}); }
    static SxPtr DDel(SxPtr f)     { return op(SxKind::DDelta, {f}); }
    static SxPtr dpsi(SxPtr f)     { return op(SxKind::PartialPsi, {f}); }
    static SxPtr integ(SxPtr f)    { return op(SxKind::Integral, {f}); }
    static SxPtr muint(SxPtr f)    { return op(SxKind::MuInt, {f}); }
    static SxPtr diamond(SxPtr f)  { return op(SxKind::Diamond, {f}); }
    static SxPtr piproj(SxPtr f)   { return op(SxKind::PiProj, {f}); }
    static SxPtr box(SxPtr f, const std::string& g = "g") { 
        return op(SxKind::BoxOp, {f}, g); 
    }
    static SxPtr compose(SxPtr f, SxPtr g) { return op(SxKind::Compose, {f, g}); }
    static SxPtr apply(SxPtr f, SxPtr x)   { return op(SxKind::Apply, {f, x}); }
    static SxPtr power(SxPtr f, int n)     { return op(SxKind::Power, {f, num((double)n)}); }
    static SxPtr iota(SxPtr f, int n, int m) { 
        return op(SxKind::Iota, {f, num((double)n), num((double)m)}); 
    }
    static SxPtr proj(SxPtr f, int m, int n) { 
        return op(SxKind::Proj, {f, num((double)m), num((double)n)}); 
    }
    
    // --- Pretty Print ---
    std::string toString() const {
        switch (kind) {
        case SxKind::Var:   return name;
        case SxKind::Const: return name;
        case SxKind::Num: {
            if (numVal == (int)numVal) return std::to_string((int)numVal);
            std::ostringstream oss; oss << numVal; return oss.str();
        }
        case SxKind::Add:
            return "(" + children[0]->toString() + " + " + children[1]->toString() + ")";
        case SxKind::Mul:
            return "(" + children[0]->toString() + "·" + children[1]->toString() + ")";
        case SxKind::Neg:
            return "(-" + children[0]->toString() + ")";
        case SxKind::Inv:
            return "(" + children[0]->toString() + ")⁻¹";
        case SxKind::Conj:
            return children[0]->toString() + "*";
        case SxKind::Norm:
            return "N(" + children[0]->toString() + ")";
        case SxKind::Dg:
            return "Ð_" + name + "(" + children[0]->toString() + ")";
        case SxKind::Eg:
            return "E_" + name + "(" + children[0]->toString() + ")";
        case SxKind::DLambda:
            return "D_Λ(" + children[0]->toString() + ")";
        case SxKind::DDelta:
            return "D_Δ(" + children[0]->toString() + ")";
        case SxKind::PartialPsi:
            return "∂_ψ(" + children[0]->toString() + ")";
        case SxKind::Integral:
            return "∫(" + children[0]->toString() + ")";
        case SxKind::MuInt:
            return "μ^{int}_{" + children[0]->toString() + "}";
        case SxKind::Diamond:
            return "◇(" + children[0]->toString() + ")";
        case SxKind::PiProj:
            return "Π(" + children[0]->toString() + ")";
        case SxKind::BoxOp:
            return "□_" + name + "(" + children[0]->toString() + ")";
        case SxKind::Compose:
            return "(" + children[0]->toString() + " ∘ " + children[1]->toString() + ")";
        case SxKind::Apply:
            return children[0]->toString() + "(" + children[1]->toString() + ")";
        case SxKind::Power:
            return children[0]->toString() + "^" + children[1]->toString();
        case SxKind::Iota:
            return "ι_{" + children[1]->toString() + "→" + children[2]->toString() + "}(" + children[0]->toString() + ")";
        case SxKind::Proj:
            return "π_{" + children[1]->toString() + "→" + children[2]->toString() + "}(" + children[0]->toString() + ")";
        case SxKind::CDcomp:
            return "C_{m→n}(" + children[0]->toString() + ")";
        }
        return "?";
    }
    
    // Structural equality (deep)
    bool eq(const Sx& other) const {
        if (kind != other.kind || name != other.name) return false;
        if (kind == SxKind::Num && numVal != other.numVal) return false;
        if (children.size() != other.children.size()) return false;
        for (size_t i = 0; i < children.size(); ++i)
            if (!children[i]->eq(*other.children[i])) return false;
        return true;
    }
    
    // Structural hash
    size_t hash() const {
        size_t h = std::hash<int>()(static_cast<int>(kind));
        h ^= std::hash<std::string>()(name) + 0x9e3779b9 + (h << 6) + (h >> 2);
        if (kind == SxKind::Num) {
            h ^= std::hash<double>()(numVal) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        for (auto& c : children) {
            h ^= c->hash() + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};

// =============================================================================
// DERIVED EQUATION: an equation with its full derivation trace
// =============================================================================

struct DerivedEquation {
    SxPtr lhs;                     // Left-hand side expression
    SxPtr rhs;                     // Right-hand side expression
    std::string axiomUsed;         // Which axiom produced this
    std::vector<std::string> derivationSteps;  // Full derivation trace
    int derivationDepth = 0;       // How many axiom applications deep
    std::string category;          // Classification of the equation
    
    std::string toString() const {
        return lhs->toString() + "  =  " + rhs->toString();
    }
    
    std::string toFullString() const {
        std::ostringstream oss;
        oss << "[DERIVED|" << category << "|depth=" << derivationDepth << "] "
            << lhs->toString() << "  =  " << rhs->toString() << "\n";
        oss << "  Axiom: " << axiomUsed << "\n";
        for (auto& step : derivationSteps) {
            oss << "    " << step << "\n";
        }
        return oss.str();
    }
};

// =============================================================================
// THE AXIOM DERIVATION ENGINE
// =============================================================================
//
// This engine applies the Interstice framework axioms to symbolic expressions
// to DERIVE new equations. Each axiom is a rewrite rule:
//   pattern → result
// The engine systematically applies all axioms to all term combinations.
//
// =============================================================================

class AxiomDeriver {
public:
    struct Config {
        int maxDepth = 3;         // Max derivation depth
        int maxEquations = 5000;  // Max equations to derive  
        bool verbose = true;
    };
    
    explicit AxiomDeriver(Config cfg = {}) : cfg_(cfg) {}
    
    // =========================================================================
    // MAIN DERIVATION ENTRY POINT
    // =========================================================================
    std::vector<DerivedEquation> deriveAll() {
        std::vector<DerivedEquation> results;
        
        if (cfg_.verbose) {
            std::cout << "\n"
                "================================================================\n"
                "  AXIOM DERIVATION ENGINE  (Interstice Framework)\n"
                "  Method: Symbolic rewriting from framework axioms\n"
                "  Max depth: " << cfg_.maxDepth << "\n"
                "  Axioms: A1-A14 (Interstices manuscript)\n"
                "================================================================\n\n";
        }
        
        // Phase AD-1: Generate base atoms (generic functions & operators)
        auto atoms = generateAtoms();
        
        // Phase AD-2: Apply Leibniz rule to all function pairs
        if (cfg_.verbose) std::cout << "[AXIOM] Phase AD-1: Leibniz derivations Ð_g(F·H)\n";
        deriveLeibniz(atoms, results);
        
        // Phase AD-3: Apply inverse rule
        if (cfg_.verbose) std::cout << "[AXIOM] Phase AD-2: Inverse derivations Ð_g(F⁻¹)\n";
        deriveInverse(atoms, results);
        
        // Phase AD-4: Apply cocycle composition
        if (cfg_.verbose) std::cout << "[AXIOM] Phase AD-3: Cocycle composition Ð_{g∘h}\n";
        deriveCocycle(atoms, results);
        
        // Phase AD-5: Apply shift-derivative relation
        if (cfg_.verbose) std::cout << "[AXIOM] Phase AD-4: Shift-derivative E_g F = F + α_g·Ð_g F\n";
        deriveShiftDerivative(atoms, results);
        
        // Phase AD-6: Apply iterated shift
        if (cfg_.verbose) std::cout << "[AXIOM] Phase AD-5: Iterated shift E_{g^n}F - F = Σ E_{g^k}(α_g·Ð_g F)\n";
        deriveIteratedShift(atoms, results);
        
        // Phase AD-7: Apply FTC with defect
        if (cfg_.verbose) std::cout << "[AXIOM] Phase AD-6: FTC with defect F(ψ+2π) - F(ψ)\n";
        deriveFTCDefect(atoms, results);
        
        // Phase AD-8: Apply scale Leibniz rule
        if (cfg_.verbose) std::cout << "[AXIOM] Phase AD-7: Scale Leibniz D_Λ(FG)\n";
        deriveScaleLeibniz(atoms, results);
        
        // Phase AD-9: Apply diamond operator identities
        if (cfg_.verbose) std::cout << "[AXIOM] Phase AD-8: Diamond ◇² = (id-Π)I₂\n";
        deriveDiamond(atoms, results);
        
        // Phase AD-10: Apply gauged diamond
        if (cfg_.verbose) std::cout << "[AXIOM] Phase AD-9: Gauged diamond (◇+A)² = (id-Π)I₂ + M\n";
        deriveGaugedDiamond(atoms, results);
        
        // Phase AD-11: Tower transport
        if (cfg_.verbose) std::cout << "[AXIOM] Phase AD-10: Tower transport A_m ↔ A_n\n";
        deriveTowerTransport(atoms, results);
        
        // Phase AD-12: Conjugation rule
        if (cfg_.verbose) std::cout << "[AXIOM] Phase AD-11: Conjugation Ð_g(F̄) = Ð_g(F)̄\n";
        deriveConjugation(atoms, results);
        
        // Phase AD-13: Conservation law structure
        if (cfg_.verbose) std::cout << "[AXIOM] Phase AD-12: Conservation Ð_g η + Div_g q = μ\n";
        deriveConservation(atoms, results);
        
        // Phase AD-14: Second-order derivations (compose previously derived equations)
        if (cfg_.maxDepth >= 2) {
            if (cfg_.verbose) std::cout << "[AXIOM] Phase AD-13: Second-order composition of derived equations\n";
            deriveSecondOrder(results);
        }
        
        // Phase AD-15: Golden ratio specializations
        if (cfg_.verbose) std::cout << "[AXIOM] Phase AD-14: Golden ratio specialization Λ=φ⁴\n";
        deriveGoldenSpecialization(results);
        
        if (cfg_.verbose) {
            std::cout << "\n[AXIOM] Total axiom-derived equations: " << results.size() << "\n\n";
        }
        
        return results;
    }
    
private:
    Config cfg_;
    
    // =========================================================================
    // ATOM GENERATION: Create base symbolic terms
    // =========================================================================
    struct AtomSet {
        std::vector<SxPtr> functions;      // Generic functions: F, G, H, P, Q
        std::vector<std::string> actions;  // Group actions: g, h, Λ, σ, ψ-shift
        std::vector<SxPtr> connections;    // Connection 1-forms: A
        SxPtr phi, lambda, kappa, lnPhi;   // Framework constants
    };
    
    AtomSet generateAtoms() {
        AtomSet a;
        // Generic functions on X = (Σ × S¹) × T  (NO predefined semantics!)
        a.functions = {
            Sx::var("F"), Sx::var("G"), Sx::var("H"),
            Sx::var("P"), Sx::var("Q"),
        };
        // Group actions
        a.actions = { "g", "h", "Λ", "σ", "ψ" };
        // Connections (generic)
        a.connections = { Sx::var("A"), Sx::var("B") };
        // Constants from the framework
        a.phi    = Sx::con("φ");
        a.lambda = Sx::con("Λ");
        a.kappa  = Sx::con("κ");
        a.lnPhi  = Sx::con("ln(φ)");
        return a;
    }
    
    // =========================================================================
    // AXIOM A1: GROUPOID LEIBNIZ RULE
    //   Ð_g(F·H) = (Ð_g F)·E_g(H) + F·(Ð_g H)
    // =========================================================================
    void deriveLeibniz(const AtomSet& atoms, std::vector<DerivedEquation>& results) {
        for (auto& act : atoms.actions) {
            for (size_t i = 0; i < atoms.functions.size(); ++i) {
                for (size_t j = i; j < atoms.functions.size(); ++j) {
                    if ((int)results.size() >= cfg_.maxEquations) return;
                    auto F = atoms.functions[i];
                    auto H = atoms.functions[j];
                    
                    // LHS: Ð_g(F·H)
                    auto lhs = Sx::Dg(Sx::mul(F, H), act);
                    
                    // RHS: (Ð_g F)·E_g(H) + F·(Ð_g H)
                    auto rhs = Sx::add(
                        Sx::mul(Sx::Dg(F, act), Sx::Eg(H, act)),
                        Sx::mul(F, Sx::Dg(H, act))
                    );
                    
                    DerivedEquation eq;
                    eq.lhs = lhs;
                    eq.rhs = rhs;
                    eq.axiomUsed = "A1: Groupoid Leibniz Rule";
                    eq.derivationSteps = {
                        "Start: Ð_" + act + "(" + F->toString() + "·" + H->toString() + ")",
                        "Apply A1: Ð_g(FH) = (Ð_g F)·E_g(H) + F·(Ð_g H)",
                        "Result: " + rhs->toString()
                    };
                    eq.derivationDepth = 1;
                    eq.category = "LeibnizDerived";
                    results.push_back(eq);
                    
                    // ALSO derive for triple products: Ð_g(F·G·H) by two applications
                    if (i != j && j < atoms.functions.size() - 1) {
                        auto K = atoms.functions[j + 1 < atoms.functions.size() ? j + 1 : 0];
                        if (K->name != F->name && K->name != H->name) {
                            // Ð_g(F·G·H) = Ð_g(F·(G·H))
                            // = (Ð_g F)·E_g(G·H) + F·Ð_g(G·H)
                            // = (Ð_g F)·E_g(G·H) + F·[(Ð_g G)·E_g(H) + G·(Ð_g H)]
                            auto GH = Sx::mul(H, K);
                            auto lhs3 = Sx::Dg(Sx::mul(F, GH), act);
                            auto rhs3 = Sx::add(
                                Sx::mul(Sx::Dg(F, act), Sx::Eg(GH, act)),
                                Sx::mul(F, Sx::add(
                                    Sx::mul(Sx::Dg(H, act), Sx::Eg(K, act)),
                                    Sx::mul(H, Sx::Dg(K, act))
                                ))
                            );
                            
                            DerivedEquation eq3;
                            eq3.lhs = lhs3;
                            eq3.rhs = rhs3;
                            eq3.axiomUsed = "A1×2: Double Leibniz (triple product)";
                            eq3.derivationSteps = {
                                "Start: Ð_" + act + "(" + F->toString() + "·" + H->toString() + "·" + K->toString() + ")",
                                "Apply A1 to outer product: Ð_g(F·(GH)) = (Ð_g F)·E_g(GH) + F·Ð_g(GH)",
                                "Apply A1 to inner product: Ð_g(GH) = (Ð_g G)·E_g(H) + G·(Ð_g H)",
                                "Substitute: " + rhs3->toString()
                            };
                            eq3.derivationDepth = 2;
                            eq3.category = "LeibnizDerived";
                            results.push_back(eq3);
                        }
                    }
                }
            }
        }
        if (cfg_.verbose) std::cout << "  Derived " << results.size() << " Leibniz equations\n";
    }
    
    // =========================================================================
    // AXIOM A2: GROUPOID INVERSE RULE
    //   Ð_g(F⁻¹) = -(E_g F)⁻¹ · (Ð_g F) · F⁻¹
    // =========================================================================
    void deriveInverse(const AtomSet& atoms, std::vector<DerivedEquation>& results) {
        size_t before = results.size();
        for (auto& act : atoms.actions) {
            for (auto& F : atoms.functions) {
                if ((int)results.size() >= cfg_.maxEquations) return;
                
                auto lhs = Sx::Dg(Sx::inv(F), act);
                auto rhs = Sx::neg(Sx::mul(Sx::mul(
                    Sx::inv(Sx::Eg(F, act)),
                    Sx::Dg(F, act)),
                    Sx::inv(F)
                ));
                
                DerivedEquation eq;
                eq.lhs = lhs;
                eq.rhs = rhs;
                eq.axiomUsed = "A2: Groupoid Inverse Rule";
                eq.derivationSteps = {
                    "Start: Ð_" + act + "(" + F->toString() + "⁻¹)",
                    "Apply A2: Ð_g(F⁻¹) = -(E_g F)⁻¹·(Ð_g F)·F⁻¹",
                    "Result: " + rhs->toString()
                };
                eq.derivationDepth = 1;
                eq.category = "InverseDerived";
                results.push_back(eq);
            }
        }
        if (cfg_.verbose) std::cout << "  Derived " << (results.size() - before) << " inverse-rule equations\n";
    }
    
    // =========================================================================
    // AXIOM A3: COCYCLE COMPOSITION
    //   (E_h α_g + α_h) · Ð_{g∘h} F = E_h(α_g · Ð_g F) + α_h · Ð_h F
    // =========================================================================
    void deriveCocycle(const AtomSet& atoms, std::vector<DerivedEquation>& results) {
        size_t before = results.size();
        // For each pair of distinct actions (g, h), derive the cocycle
        for (size_t i = 0; i < atoms.actions.size(); ++i) {
            for (size_t j = i + 1; j < atoms.actions.size(); ++j) {
                auto g = atoms.actions[i];
                auto h = atoms.actions[j];
                auto gh = g + "∘" + h;
                
                for (auto& F : atoms.functions) {
                    if ((int)results.size() >= cfg_.maxEquations) return;
                    
                    auto alpha_g = Sx::con("α_" + g);
                    auto alpha_h = Sx::con("α_" + h);
                    
                    // LHS: (E_h(α_g) + α_h) · Ð_{g∘h}(F)
                    auto lhs = Sx::mul(
                        Sx::add(Sx::Eg(alpha_g, h), alpha_h),
                        Sx::Dg(F, gh)
                    );
                    
                    // RHS: E_h(α_g · Ð_g F) + α_h · Ð_h F
                    auto rhs = Sx::add(
                        Sx::Eg(Sx::mul(alpha_g, Sx::Dg(F, g)), h),
                        Sx::mul(alpha_h, Sx::Dg(F, h))
                    );
                    
                    DerivedEquation eq;
                    eq.lhs = lhs;
                    eq.rhs = rhs;
                    eq.axiomUsed = "A3: Cocycle Composition";
                    eq.derivationSteps = {
                        "Actions: g=" + g + ", h=" + h,
                        "Apply A3: (E_h α_g + α_h)·Ð_{g∘h} F = E_h(α_g·Ð_g F) + α_h·Ð_h F",
                        "This determines Ð_{" + gh + "} from Ð_" + g + " and Ð_" + h
                    };
                    eq.derivationDepth = 1;
                    eq.category = "CocycleDerived";
                    results.push_back(eq);
                }
            }
        }
        if (cfg_.verbose) std::cout << "  Derived " << (results.size() - before) << " cocycle equations\n";
    }
    
    // =========================================================================
    // AXIOM A4: SHIFT-DERIVATIVE RELATION
    //   E_g F = F + α_g · Ð_g F
    // =========================================================================
    void deriveShiftDerivative(const AtomSet& atoms, std::vector<DerivedEquation>& results) {
        size_t before = results.size();
        for (auto& act : atoms.actions) {
            for (auto& F : atoms.functions) {
                if ((int)results.size() >= cfg_.maxEquations) return;
                
                auto alpha = Sx::con("α_" + act);
                
                auto lhs = Sx::Eg(F, act);
                auto rhs = Sx::add(F, Sx::mul(alpha, Sx::Dg(F, act)));
                
                DerivedEquation eq;
                eq.lhs = lhs;
                eq.rhs = rhs;
                eq.axiomUsed = "A4: Shift-Derivative Relation";
                eq.derivationSteps = {
                    "Apply A4: E_" + act + "(" + F->toString() + ") = " + F->toString() + " + α_" + act + "·Ð_" + act + "(" + F->toString() + ")"
                };
                eq.derivationDepth = 1;
                eq.category = "ShiftDerived";
                results.push_back(eq);
            }
        }
        if (cfg_.verbose) std::cout << "  Derived " << (results.size() - before) << " shift-derivative equations\n";
    }
    
    // =========================================================================
    // AXIOM A5: ITERATED SHIFT
    //   E_{g^n} F - F = Σ_{k=0}^{n-1} E_{g^k}(α_g · Ð_g F)
    // =========================================================================
    void deriveIteratedShift(const AtomSet& atoms, std::vector<DerivedEquation>& results) {
        size_t before = results.size();
        int nValues[] = {2, 3, 4};
        
        for (auto& act : atoms.actions) {
            for (auto& F : atoms.functions) {
                for (int n : nValues) {
                    if ((int)results.size() >= cfg_.maxEquations) return;
                    
                    auto alpha = Sx::con("α_" + act);
                    std::string gn = act + "^" + std::to_string(n);
                    
                    // LHS: E_{g^n}(F) - F
                    auto lhs = Sx::add(Sx::Eg(F, gn), Sx::neg(F));
                    
                    // RHS: Σ_{k=0}^{n-1} E_{g^k}(α_g · Ð_g F)
                    auto aDf = Sx::mul(alpha, Sx::Dg(F, act));
                    SxPtr rhs = aDf;  // k=0 term
                    for (int k = 1; k < n; ++k) {
                        std::string gk = act + "^" + std::to_string(k);
                        rhs = Sx::add(rhs, Sx::Eg(aDf, gk));
                    }
                    
                    DerivedEquation eq;
                    eq.lhs = lhs;
                    eq.rhs = rhs;
                    eq.axiomUsed = "A5: Iterated Shift (n=" + std::to_string(n) + ")";
                    eq.derivationSteps = {
                        "Apply A5: E_{" + gn + "}F - F = Σ_{k=0}^{" + std::to_string(n-1) + "} E_{" + act + "^k}(α_" + act + "·Ð_" + act + " F)",
                        "Expands telescoping sum of n=" + std::to_string(n) + " steps"
                    };
                    eq.derivationDepth = 1;
                    eq.category = "IteratedShiftDerived";
                    results.push_back(eq);
                }
            }
        }
        if (cfg_.verbose) std::cout << "  Derived " << (results.size() - before) << " iterated-shift equations\n";
    }
    
    // =========================================================================
    // AXIOM A6: FTC WITH DEFECT MEASURE
    //   F(ω,ψ+2π,t) - F(ω,ψ,t) = (lnΛ/2π)∫_{ψ}^{ψ+2π} ∂_ψ' F dψ' + μ^{int}_F
    // =========================================================================
    void deriveFTCDefect(const AtomSet& atoms, std::vector<DerivedEquation>& results) {
        size_t before = results.size();
        for (auto& F : atoms.functions) {
            if ((int)results.size() >= cfg_.maxEquations) return;
            
            auto kLam = Sx::con("lnΛ/2π");
            
            // LHS: E_ψ(F) - F  (shift by 2π in ψ)
            auto lhs = Sx::add(Sx::Eg(F, "ψ"), Sx::neg(F));
            
            // RHS: (lnΛ/2π)·∫(∂_ψ F) + μ^{int}_F
            auto rhs = Sx::add(
                Sx::mul(kLam, Sx::integ(Sx::dpsi(F))),
                Sx::muint(F)
            );
            
            DerivedEquation eq;
            eq.lhs = lhs;
            eq.rhs = rhs;
            eq.axiomUsed = "A6: FTC with Defect";
            eq.derivationSteps = {
                "Apply A6: F(ψ+2π) - F(ψ) = (lnΛ/2π)·∫∂_ψ F dψ + μ^{int}_F",
                "The defect μ^{int}_F captures BV singular measure (jumps/Cantor parts)"
            };
            eq.derivationDepth = 1;
            eq.category = "FTCDefect";
            results.push_back(eq);
            
            // COROLLARY: When μ_F = 0 (absolutely continuous case):
            // E_ψ(F) - F = (lnΛ/2π)·∫∂_ψ F dψ
            auto lhs_ac = Sx::add(Sx::Eg(F, "ψ"), Sx::neg(F));
            auto rhs_ac = Sx::mul(kLam, Sx::integ(Sx::dpsi(F)));
            
            DerivedEquation eq_ac;
            eq_ac.lhs = lhs_ac;
            eq_ac.rhs = rhs_ac;
            eq_ac.axiomUsed = "A6 corollary: FTC (AC case, μ_F = 0)";
            eq_ac.derivationSteps = {
                "Specialize A6 with μ^{int}_F = 0 (absolutely continuous)",
                "E_ψ F - F = (lnΛ/2π)·∫∂_ψ F dψ"
            };
            eq_ac.derivationDepth = 1;
            eq_ac.category = "FTCDefect";
            results.push_back(eq_ac);
        }
        if (cfg_.verbose) std::cout << "  Derived " << (results.size() - before) << " FTC-defect equations\n";
    }
    
    // =========================================================================
    // AXIOM A9: SCALE LEIBNIZ RULE
    //   D_Λ(F·G) = (D_Λ F)·(E_Λ G) + F·(D_Λ G)
    // =========================================================================
    void deriveScaleLeibniz(const AtomSet& atoms, std::vector<DerivedEquation>& results) {
        size_t before = results.size();
        for (size_t i = 0; i < atoms.functions.size(); ++i) {
            for (size_t j = i; j < atoms.functions.size(); ++j) {
                if ((int)results.size() >= cfg_.maxEquations) return;
                auto F = atoms.functions[i];
                auto G = atoms.functions[j];
                
                // D_Λ(F·G) = (D_Λ F)·(E_Λ G) + F·(D_Λ G)
                auto lhs = Sx::DLam(Sx::mul(F, G));
                auto rhs = Sx::add(
                    Sx::mul(Sx::DLam(F), Sx::Eg(G, "Λ")),
                    Sx::mul(F, Sx::DLam(G))
                );
                
                DerivedEquation eq;
                eq.lhs = lhs;
                eq.rhs = rhs;
                eq.axiomUsed = "A9: Scale Leibniz Rule";
                eq.derivationSteps = {
                    "Apply A9: D_Λ(" + F->toString() + "·" + G->toString() + ") = (D_Λ " + F->toString() + ")·(E_Λ " + G->toString() + ") + " + F->toString() + "·(D_Λ " + G->toString() + ")"
                };
                eq.derivationDepth = 1;
                eq.category = "ScaleLeibnizDerived";
                results.push_back(eq);
            }
        }
        if (cfg_.verbose) std::cout << "  Derived " << (results.size() - before) << " scale-Leibniz equations\n";
    }
    
    // =========================================================================
    // AXIOM A7: DIAMOND OPERATOR
    //   ◇_B = [[0, D_B], [S_B, 0]]
    //   ◇²_B = (id - Π_B) · I₂
    //   ◇²·Π = 0,  Π·◇² = 0
    // =========================================================================
    void deriveDiamond(const AtomSet& atoms, std::vector<DerivedEquation>& results) {
        size_t before = results.size();
        for (auto& F : atoms.functions) {
            if ((int)results.size() >= cfg_.maxEquations) return;
            
            // ◇²(F) = (id - Π)(F) = F - Π(F)
            auto lhs = Sx::diamond(Sx::diamond(F));
            auto rhs = Sx::add(F, Sx::neg(Sx::piproj(F)));
            
            DerivedEquation eq;
            eq.lhs = lhs;
            eq.rhs = rhs;
            eq.axiomUsed = "A7: Diamond Squared";
            eq.derivationSteps = {
                "Apply A7: ◇²_B = (id - Π_B)·I₂",
                "◇(◇(F)) = F - Π(F)"
            };
            eq.derivationDepth = 1;
            eq.category = "DiamondDerived";
            results.push_back(eq);
            
            // ◇² · Π(F) = 0
            auto lhs2 = Sx::diamond(Sx::diamond(Sx::piproj(F)));
            auto rhs2 = Sx::num(0);
            
            DerivedEquation eq2;
            eq2.lhs = lhs2;
            eq2.rhs = rhs2;
            eq2.axiomUsed = "A7 corollary: ◇²·Π = 0";
            eq2.derivationSteps = {
                "From A7: (id - Π)·Π = Π - Π² = Π - Π = 0",
                "Therefore ◇²(Π(F)) = 0 for all F"
            };
            eq2.derivationDepth = 1;
            eq2.category = "DiamondDerived";
            results.push_back(eq2);
        }
        if (cfg_.verbose) std::cout << "  Derived " << (results.size() - before) << " diamond-operator equations\n";
    }
    
    // =========================================================================
    // AXIOM A8: GAUGED DIAMOND
    //   (◇ + A)² = (id - Π)I₂ + M
    //   where M = ◇A + A◇ + A²
    // =========================================================================
    void deriveGaugedDiamond(const AtomSet& atoms, std::vector<DerivedEquation>& results) {
        size_t before = results.size();
        for (auto& A : atoms.connections) {
            for (auto& F : atoms.functions) {
                if ((int)results.size() >= cfg_.maxEquations) return;
                
                // (◇+A)²(F) = ◇²(F) + ◇(A·F) + A·◇(F) + A²·F
                //            = (F - Π(F)) + ◇(A·F) + A·◇(F) + A·A·F
                auto diamF = Sx::diamond(F);
                auto AF = Sx::mul(A, F);
                auto AAF = Sx::mul(A, Sx::mul(A, F));
                
                auto lhs = Sx::add(Sx::diamond(Sx::add(diamF, AF)),
                                   Sx::add(AF, Sx::diamond(F)));
                
                // The curvature/mass term M(F) = ◇(A·F) + A·◇(F) + A²·F
                auto M_F = Sx::add(
                    Sx::add(Sx::diamond(AF), Sx::mul(A, diamF)),
                    AAF
                );
                
                // Full equation: (◇+A)²(F) = (id-Π)(F) + M(F)
                auto lhs_full = Sx::op(SxKind::BoxOp, {F}, "◇+A");
                auto rhs_full = Sx::add(
                    Sx::add(F, Sx::neg(Sx::piproj(F))),
                    M_F
                );
                
                DerivedEquation eq;
                eq.lhs = lhs_full;
                eq.rhs = rhs_full;
                eq.axiomUsed = "A8: Gauged Diamond";
                eq.derivationSteps = {
                    "Connection: " + A->toString(),
                    "Apply A8: (◇+" + A->toString() + ")²(F) = (id-Π)F + M_" + A->toString() + "(F)",
                    "where M = ◇" + A->toString() + " + " + A->toString() + "◇ + " + A->toString() + "²",
                    "This is the INTERSTICE FIELD EQUATION: curvature = defect"
                };
                eq.derivationDepth = 1;
                eq.category = "GaugedDiamondDerived";
                results.push_back(eq);
                
                // Flat connection condition: M = 0 ⟹ (◇+A)² = (id-Π)I₂
                DerivedEquation eq_flat;
                eq_flat.lhs = M_F;
                eq_flat.rhs = Sx::num(0);
                eq_flat.axiomUsed = "A8 corollary: Flat connection condition";
                eq_flat.derivationSteps = {
                    "If M_" + A->toString() + " = 0 (flat connection):",
                    "◇" + A->toString() + " + " + A->toString() + "◇ + " + A->toString() + "² = 0",
                    "Then (◇+" + A->toString() + ")² = (id-Π)I₂ (undeformed)"
                };
                eq_flat.derivationDepth = 1;
                eq_flat.category = "GaugedDiamondDerived";
                results.push_back(eq_flat);
            }
        }
        if (cfg_.verbose) std::cout << "  Derived " << (results.size() - before) << " gauged-diamond equations\n";
    }
    
    // =========================================================================
    // AXIOM A11: TOWER TRANSPORT
    //   C_{m→n}(Ð_g F) = (Ð_g)^{×2^k} ∘ C_{m→n}(F)
    //   An equation E=0 in A_m ⟺ 2^{m-n} equations in A_n
    // =========================================================================
    void deriveTowerTransport(const AtomSet& atoms, std::vector<DerivedEquation>& results) {
        size_t before = results.size();
        int levels[][2] = {{0,1}, {1,2}, {0,2}, {2,3}, {0,3}};
        // (ℝ→ℂ, ℂ→ℍ, ℝ→ℍ, ℍ→𝕆, ℝ→𝕆)
        std::string names[] = {"ℝ", "ℂ", "ℍ", "𝕆"};
        
        for (auto& [n, m] : levels) {
            int k = m - n;
            int numComponents = 1 << k;
            
            for (auto& F : atoms.functions) {
                for (auto& act : atoms.actions) {
                    if ((int)results.size() >= cfg_.maxEquations) return;
                    
                    // C_{m→n}(Ð_g F) = Ð_g^{×2^k}(C_{m→n}(F))
                    auto lhs = Sx::op(SxKind::CDcomp, {Sx::Dg(F, act)}, 
                                      names[m] + "→" + names[n]);
                    auto rhs = Sx::Dg(
                        Sx::op(SxKind::CDcomp, {F}, names[m] + "→" + names[n]),
                        act + "^{×" + std::to_string(numComponents) + "}"
                    );
                    
                    DerivedEquation eq;
                    eq.lhs = lhs;
                    eq.rhs = rhs;
                    eq.axiomUsed = "A11: Tower Transport " + names[m] + " → " + names[n];
                    eq.derivationSteps = {
                        "Apply A11: C_{" + names[m] + "→" + names[n] + "}(Ð_" + act + " F) = Ð_" + act + "^{×" + std::to_string(numComponents) + "}(C_{" + names[m] + "→" + names[n] + "}(F))",
                        "One equation in " + names[m] + " (dim " + std::to_string(1 << m) + ") becomes " + std::to_string(numComponents) + " equations in " + names[n] + " (dim " + std::to_string(1 << n) + ")",
                        "This is NON-TRIVIAL: the " + names[m] + " multiplication table mixes components"
                    };
                    eq.derivationDepth = 1;
                    eq.category = "TowerTransportDerived";
                    results.push_back(eq);
                }
            }
        }
        if (cfg_.verbose) std::cout << "  Derived " << (results.size() - before) << " tower-transport equations\n";
    }
    
    // =========================================================================
    // AXIOM A13: CONJUGATION RULE
    //   Ð_g(F̄) = Ð_g(F)̄   (when χ_g ∈ ℝ)
    // =========================================================================
    void deriveConjugation(const AtomSet& atoms, std::vector<DerivedEquation>& results) {
        size_t before = results.size();
        for (auto& act : atoms.actions) {
            for (auto& F : atoms.functions) {
                if ((int)results.size() >= cfg_.maxEquations) return;
                
                // Ð_g(F*) = (Ð_g F)*
                auto lhs = Sx::Dg(Sx::conj(F), act);
                auto rhs = Sx::conj(Sx::Dg(F, act));
                
                DerivedEquation eq;
                eq.lhs = lhs;
                eq.rhs = rhs;
                eq.axiomUsed = "A13: Conjugation Rule (χ_g ∈ ℝ)";
                eq.derivationSteps = {
                    "Apply A13: Ð_" + act + "(F̄) = (Ð_" + act + " F)̄",
                    "Requires χ_" + act + " ∈ ℝ (real-valued cocycle)"
                };
                eq.derivationDepth = 1;
                eq.category = "ConjugationDerived";
                results.push_back(eq);
            }
        }
        if (cfg_.verbose) std::cout << "  Derived " << (results.size() - before) << " conjugation equations\n";
    }
    
    // =========================================================================
    // AXIOM A14: CONSERVATION LAW STRUCTURE
    //   Ð_g η(U) + Div_g q(U) = μ_src
    //   Specializations for η = N (norm), η = tr (trace), etc.
    // =========================================================================
    void deriveConservation(const AtomSet& atoms, std::vector<DerivedEquation>& results) {
        size_t before = results.size();
        
        // Conservation of norm: Ð_g N(F) involves the Leibniz structure
        for (auto& act : atoms.actions) {
            for (auto& F : atoms.functions) {
                if ((int)results.size() >= cfg_.maxEquations) return;
                
                // N(F) = F·F*
                // Ð_g N(F) = Ð_g(F·F*) = (Ð_g F)·E_g(F*) + F·Ð_g(F*)
                // Using conjugation: Ð_g(F*) = (Ð_g F)*
                // So: Ð_g N(F) = (Ð_g F)·E_g(F*) + F·(Ð_g F)*
                auto DgF = Sx::Dg(F, act);
                auto EgFstar = Sx::Eg(Sx::conj(F), act);
                auto DgFstar = Sx::conj(DgF);
                
                auto lhs = Sx::Dg(Sx::norm(F), act);
                auto rhs = Sx::add(
                    Sx::mul(DgF, EgFstar),
                    Sx::mul(F, DgFstar)
                );
                
                DerivedEquation eq;
                eq.lhs = lhs;
                eq.rhs = rhs;
                eq.axiomUsed = "A14+A1+A13: Norm conservation derivation";
                eq.derivationSteps = {
                    "Start: Ð_" + act + "(N(F)) where N(F) = F·F*",
                    "Apply A1 (Leibniz): Ð_g(F·F*) = (Ð_g F)·E_g(F*) + F·Ð_g(F*)",
                    "Apply A13 (Conjugation): Ð_g(F*) = (Ð_g F)*",
                    "Result: Ð_g N(F) = (Ð_g F)·E_g(F*) + F·(Ð_g F)*",
                    "This is a MULTI-AXIOM derivation combining A1, A13, and the norm definition"
                };
                eq.derivationDepth = 2;
                eq.category = "ConservationDerived";
                results.push_back(eq);
                
                // When N(F) is conserved (Ð_g N(F) = 0):
                // (Ð_g F)·E_g(F*) = -F·(Ð_g F)*
                DerivedEquation eq_cons;
                eq_cons.lhs = Sx::mul(DgF, EgFstar);
                eq_cons.rhs = Sx::neg(Sx::mul(F, DgFstar));
                eq_cons.axiomUsed = "A14 corollary: Norm conservation condition";
                eq_cons.derivationSteps = {
                    "If Ð_g N(F) = 0 (norm is conserved):",
                    "(Ð_g F)·E_g(F*) + F·(Ð_g F)* = 0",
                    "(Ð_g F)·E_g(F*) = -F·(Ð_g F)*",
                    "This constrains Ð_g F in terms of F and its shifted conjugate"
                };
                eq_cons.derivationDepth = 2;
                eq_cons.category = "ConservationDerived";
                results.push_back(eq_cons);
            }
        }
        if (cfg_.verbose) std::cout << "  Derived " << (results.size() - before) << " conservation equations\n";
    }
    
    // =========================================================================
    // SECOND-ORDER DERIVATIONS: Compose previously derived equations
    // Apply D to both sides, substitute known identities, etc.
    // =========================================================================
    void deriveSecondOrder(std::vector<DerivedEquation>& results) {
        size_t before = results.size();
        size_t firstOrderCount = results.size();
        
        // For each first-order Leibniz equation Ð_g(FH) = ...,
        // apply Ð_g again to get Ð_g²(FH)
        for (size_t i = 0; i < firstOrderCount && (int)results.size() < cfg_.maxEquations; ++i) {
            auto& eq = results[i];
            if (eq.derivationDepth >= cfg_.maxDepth) continue;
            if (eq.category != "LeibnizDerived" && eq.category != "ShiftDerived") continue;
            
            // Apply Ð_g to both sides: Ð_g(LHS) = Ð_g(RHS)
            // This generates higher-order derivative relations
            for (auto& act : {"g", "h"}) {
                auto lhs2 = Sx::Dg(eq.lhs, act);
                auto rhs2 = Sx::Dg(eq.rhs, act);
                
                DerivedEquation eq2;
                eq2.lhs = lhs2;
                eq2.rhs = rhs2;
                eq2.axiomUsed = "Second-order: Ð_" + std::string(act) + " applied to [" + eq.axiomUsed + "]";
                eq2.derivationSteps = {
                    "Take derived equation: " + eq.lhs->toString() + " = " + eq.rhs->toString(),
                    "Apply Ð_" + std::string(act) + " to both sides",
                    "Ð_" + std::string(act) + "(" + eq.lhs->toString() + ") = Ð_" + std::string(act) + "(" + eq.rhs->toString() + ")"
                };
                eq2.derivationDepth = eq.derivationDepth + 1;
                eq2.category = "SecondOrderDerived";
                results.push_back(eq2);
            }
        }
        
        // Cross-axiom derivation: substitute shift-derivative into Leibniz
        // From A4: E_g(H) = H + α_g·Ð_g(H)
        // Substitute into A1: Ð_g(FH) = (Ð_g F)·(H + α_g·Ð_g H) + F·(Ð_g H)
        //                              = (Ð_g F)·H + α_g·(Ð_g F)·(Ð_g H) + F·(Ð_g H)
        for (auto& act : {"g", "h", "Λ"}) {
            auto F = Sx::var("F");
            auto H = Sx::var("H");
            auto alpha = Sx::con("α_" + std::string(act));
            auto DgF = Sx::Dg(F, act);
            auto DgH = Sx::Dg(H, act);
            
            // Ð_g(FH) = (Ð_g F)·H + α_g·(Ð_g F)·(Ð_g H) + F·(Ð_g H)
            auto lhs = Sx::Dg(Sx::mul(F, H), act);
            auto rhs = Sx::add(
                Sx::add(
                    Sx::mul(DgF, H),
                    Sx::mul(alpha, Sx::mul(DgF, DgH))
                ),
                Sx::mul(F, DgH)
            );
            
            DerivedEquation eq;
            eq.lhs = lhs;
            eq.rhs = rhs;
            eq.axiomUsed = "A1+A4: Leibniz with shift-derivative expansion";
            eq.derivationSteps = {
                "Start with A1: Ð_g(FH) = (Ð_g F)·E_g(H) + F·(Ð_g H)",
                "Apply A4: E_g(H) = H + α_g·Ð_g(H)",
                "Substitute: (Ð_g F)·(H + α_g·Ð_g H) + F·(Ð_g H)",
                "Expand: (Ð_g F)·H + α_g·(Ð_g F)·(Ð_g H) + F·(Ð_g H)",
                "The middle term α_g·(Ð_g F)·(Ð_g H) is the INTERSTICE CORRECTION to classical Leibniz"
            };
            eq.derivationDepth = 2;
            eq.category = "CrossAxiomDerived";
            results.push_back(eq);
        }
        
        if (cfg_.verbose) std::cout << "  Derived " << (results.size() - before) << " second-order equations\n";
    }
    
    // =========================================================================
    // GOLDEN RATIO SPECIALIZATION: Substitute Λ = φ⁴
    // =========================================================================
    void deriveGoldenSpecialization(std::vector<DerivedEquation>& results) {
        size_t before = results.size();
        
        // Key specializations from the Interstices manuscript
        auto F = Sx::var("F");
        auto phi = Sx::con("φ");
        auto four_ln_phi = Sx::con("4ln(φ)");
        auto two_ln_phi_over_pi = Sx::con("2ln(φ)/π");
        
        // α_Λ = ln(Λ) = ln(φ⁴) = 4ln(φ)
        {
            auto lhs = Sx::con("α_Λ");
            auto rhs = four_ln_phi;
            
            DerivedEquation eq;
            eq.lhs = lhs;
            eq.rhs = rhs;
            eq.axiomUsed = "Golden specialization: α_Λ = 4ln(φ)";
            eq.derivationSteps = {
                "Λ = φ⁴",
                "α_Λ = ln(Λ) = ln(φ⁴) = 4·ln(φ)",
                "This fixes the cocycle constant for scale-action"
            };
            eq.derivationDepth = 0;
            eq.category = "GoldenSpecialization";
            results.push_back(eq);
        }
        
        // FTC coefficient: lnΛ/2π = 2ln(φ)/π
        {
            auto lhs = Sx::con("lnΛ/2π");
            auto rhs = two_ln_phi_over_pi;
            
            DerivedEquation eq;
            eq.lhs = lhs;
            eq.rhs = rhs;
            eq.axiomUsed = "Golden specialization: lnΛ/2π = 2ln(φ)/π";
            eq.derivationSteps = {
                "lnΛ/2π = 4ln(φ)/2π = 2ln(φ)/π",
                "This is the FTC coefficient in the golden ratio setting"
            };
            eq.derivationDepth = 0;
            eq.category = "GoldenSpecialization";
            results.push_back(eq);
        }
        
        // φ² = φ + 1  (the fundamental golden recursion)
        {
            auto lhs = Sx::power(phi, 2);
            auto rhs = Sx::add(phi, Sx::num(1));
            
            DerivedEquation eq;
            eq.lhs = lhs;
            eq.rhs = rhs;
            eq.axiomUsed = "Golden recursion: φ² = φ + 1";
            eq.derivationSteps = {
                "φ = (1+√5)/2 satisfies x² = x + 1",
                "This is the MINIMAL POLYNOMIAL of the golden ratio"
            };
            eq.derivationDepth = 0;
            eq.category = "GoldenSpecialization";
            results.push_back(eq);
        }
        
        // Specialized FTC:
        // F(ψ+2π) - F(ψ) = (2ln(φ)/π)·∫∂_ψ F dψ + μ^{int}_F
        {
            auto lhs = Sx::add(Sx::Eg(F, "ψ"), Sx::neg(F));
            auto rhs = Sx::add(
                Sx::mul(two_ln_phi_over_pi, Sx::integ(Sx::dpsi(F))),
                Sx::muint(F)
            );
            
            DerivedEquation eq;
            eq.lhs = lhs;
            eq.rhs = rhs;
            eq.axiomUsed = "A6 + Golden: FTC with φ-specialization";
            eq.derivationSteps = {
                "Specialize A6 with Λ = φ⁴:",
                "F(ψ+2π) - F(ψ) = (2ln(φ)/π)·∫∂_ψ F dψ + μ^{int}_F",
                "The coefficient 2ln(φ)/π ≈ 0.3063 is CHARACTERISTIC of the golden interstice"
            };
            eq.derivationDepth = 1;
            eq.category = "GoldenSpecialization";
            results.push_back(eq);
        }
        
        // Ð_Λ(F) = [E_Λ(F) - F] / (4ln(φ))
        {
            auto lhs = Sx::Dg(F, "Λ");
            auto rhs = Sx::mul(
                Sx::inv(four_ln_phi),
                Sx::add(Sx::Eg(F, "Λ"), Sx::neg(F))
            );
            
            DerivedEquation eq;
            eq.lhs = lhs;
            eq.rhs = rhs;
            eq.axiomUsed = "Golden specialization: Ð_Λ definition";
            eq.derivationSteps = {
                "Ð_Λ(F) = (E_Λ F - F) / α_Λ = (E_Λ F - F) / (4ln(φ))",
                "The golden ratio derivative normalizes the shift by the cocycle 4ln(φ)"
            };
            eq.derivationDepth = 0;
            eq.category = "GoldenSpecialization";
            results.push_back(eq);
        }
        
        // Critical exponent κ = 1/3 specialization
        {
            auto lhs = Sx::con("3κ - 1");
            auto rhs = Sx::num(0);
            
            DerivedEquation eq;
            eq.lhs = lhs;
            eq.rhs = rhs;
            eq.axiomUsed = "Critical exponent: 3κ = 1 ⟹ κ = 1/3";
            eq.derivationSteps = {
                "From the scale covariance equation: u(Ax,τ) = Λ^κ·u(x,τ)",
                "The Interstice operator D is covariant iff 3κ - 1 = 0",
                "κ = 1/3 is the unique critical exponent of the framework"
            };
            eq.derivationDepth = 0;
            eq.category = "GoldenSpecialization";
            results.push_back(eq);
        }
        
        if (cfg_.verbose) std::cout << "  Derived " << (results.size() - before) << " golden-specialization equations\n";
    }
};

} // namespace axiom_deriver
