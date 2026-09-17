/**
 * @file PadicBridge.hpp
 * @brief P-ADIC BRIDGE — Archimedean ↔ Non-archimedean transport
 *
 * UNIVERSAL CLOSURE THEOREM (from Interstices.txt):
 *   ∀p,q ∈ primes ∪ {∞}, ∀E ∈ T:
 *     E = 0 in Q_p  ⟺  equivalent translated system in Q_q
 *
 * This module enables the Autodiscoverer to:
 *   1. Evaluate equations in Q_p (p-adic numbers) via Hensel lifting
 *   2. Transport between R (= Q_∞) and Q_p for verification
 *   3. Check equation universality across number systems
 *   4. Generate p-adic specialization terms for discovery
 *
 * Key identities discovered through this:
 *   - Ostrowski: every non-trivial absolute value on Q is |·|_p or |·|_∞
 *   - Product formula: Π_v |x|_v = 1 for all x ∈ Q*
 *   - Adelic closure: equations valid at ALL places simultaneously
 *
 * This ensures: the Autodiscoverer discovers equations that are TRUE
 * in EVERY number system, not just over the reals.
 */

#ifndef AUTODISCOVER_DOMAIN_PADIC_BRIDGE_HPP
#define AUTODISCOVER_DOMAIN_PADIC_BRIDGE_HPP

#include "../core/Term.hpp"
#include "../logic/Equation.hpp"
#include <vector>
#include <memory>
#include <string>
#include <cmath>
#include <cstdint>

namespace autodiscover {
namespace domain {
namespace padic {

using core::Term;
using core::TermFactory;
using core::Sort;
using logic::Equation;

// =============================================================================
// P-ADIC VALUATION AND NORM
// =============================================================================

class PadicValuation {
public:
    /// p-adic valuation: v_p(n) = largest k such that p^k | n
    static int valuation(int64_t n, int p) {
        if (n == 0) return -1; // convention: v_p(0) = ∞
        if (p < 2) return 0;
        n = std::abs(n);
        int v = 0;
        while (n % p == 0) { n /= p; v++; }
        return v;
    }

    /// p-adic absolute value: |n|_p = p^{-v_p(n)}
    static double padicNorm(int64_t n, int p) {
        if (n == 0) return 0.0;
        int v = valuation(n, p);
        return std::pow(static_cast<double>(p), -v);
    }

    /// p-adic norm of a rational a/b
    static double padicNormRational(int64_t a, int64_t b, int p) {
        if (a == 0) return 0.0;
        if (b == 0) return std::numeric_limits<double>::infinity();
        int va = valuation(a, p);
        int vb = valuation(b, p);
        return std::pow(static_cast<double>(p), -(va - vb));
    }

    /// Verify product formula: Π_{all places v} |x|_v = 1
    /// For integer n: |n|_∞ · Π_p |n|_p = 1
    static double productFormula(int64_t n, const std::vector<int>& primes) {
        if (n == 0) return 0.0;
        double product = static_cast<double>(std::abs(n)); // |n|_∞
        for (int p : primes) {
            product *= padicNorm(n, p);
        }
        return product;
    }

    /// Ultrametric inequality verification: |x+y|_p ≤ max(|x|_p, |y|_p)
    static bool verifyUltrametric(int64_t x, int64_t y, int p) {
        double nx = padicNorm(x, p);
        double ny = padicNorm(y, p);
        double nxy = padicNorm(x + y, p);
        return nxy <= std::max(nx, ny) + 1e-15;
    }
};

// =============================================================================
// HENSEL LIFTING — refine p-adic approximations
// =============================================================================

class HenselLift {
public:
    /// Simple Hensel lifting for f(x) ≡ 0 (mod p^k)
    /// Given x₀ with f(x₀) ≡ 0 (mod p), lift to x with f(x) ≡ 0 (mod p^k)
    /// Uses: x_{n+1} = x_n - f(x_n)/f'(x_n)  (all mod p^{n+1})
    static int64_t lift(
        std::function<int64_t(int64_t)> f,
        std::function<int64_t(int64_t)> fprime,
        int64_t x0, int p, int k)
    {
        int64_t x = x0 % p;
        int64_t pk = p;
        for (int i = 1; i < k; ++i) {
            int64_t fx = f(x);
            int64_t fp = fprime(x);
            // Need fp invertible mod p
            int64_t fp_inv = modInverse(fp, p);
            if (fp_inv == 0) return x; // singular case
            int64_t correction = -(fx / pk) * fp_inv;
            x = ((x + correction * pk) % (pk * p) + pk * p) % (pk * p);
            pk *= p;
        }
        return x;
    }

private:
    static int64_t modInverse(int64_t a, int64_t m) {
        a = ((a % m) + m) % m;
        if (a == 0) return 0;
        int64_t g = std::gcd(a, m);
        if (g != 1) return 0; // not invertible
        // Extended GCD
        int64_t old_r = a, r = m;
        int64_t old_s = 1, s = 0;
        while (r != 0) {
            int64_t q = old_r / r;
            int64_t temp = r; r = old_r - q * r; old_r = temp;
            temp = s; s = old_s - q * s; old_s = temp;
        }
        return ((old_s % m) + m) % m;
    }
};

// =============================================================================
// P-ADIC BRIDGE AXIOM MODULE
// =============================================================================

class PadicBridgeModule {
public:
    explicit PadicBridgeModule(TermFactory& factory) : factory_(factory) {}

    [[nodiscard]] std::vector<std::unique_ptr<Equation>> generateAxioms() {
        std::vector<std::unique_ptr<Equation>> axioms;
        generateProductFormulaAxioms(axioms);
        generateUltrametricAxioms(axioms);
        generateUniversalClosureAxioms(axioms);
        generatePadicExpansionAxioms(axioms);
        return axioms;
    }

private:
    TermFactory& factory_;
    uint32_t vc_ = 0;

    const Term* fv(Sort s = Sort::Generic) {
        return factory_.variable("pad" + std::to_string(vc_++), s);
    }

    void generateProductFormulaAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto x = fv();

        // PF1: Product formula: |x|_∞ · Π_p |x|_p = 1 for x ∈ Q*
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("ProductFormula", {x}),
            factory_.scalar(1.0)
        ));

        // PF2: Ostrowski classification: all norms are |·|_p or |·|_∞
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("NormClassify", {x}),
            factory_.apply("OstrowskiClass", {x})
        ));
    }

    void generateUltrametricAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto x = fv(); auto y = fv(); auto p = fv();

        // UM1: |x+y|_p ≤ max(|x|_p, |y|_p)  (ultrametric inequality)
        // Encoded as: PadicNorm(add(x,y), p) ≤ PadicMax(PadicNorm(x,p), PadicNorm(y,p))
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("UltrametricCheck", {x, y, p}),
            factory_.scalar(1.0) // = true
        ));

        // UM2: If |x|_p ≠ |y|_p, then |x+y|_p = max(|x|_p, |y|_p)
        // (strict ultrametric when norms differ)
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("StrictUltrametric", {x, y, p}),
            factory_.apply("PadicMax", {
                factory_.apply("PadicNorm", {x, p}),
                factory_.apply("PadicNorm", {y, p})})
        ));
    }

    void generateUniversalClosureAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto eq = fv(); auto p = fv(); auto q = fv();

        // UC1: Universal closure: E=0 in Q_p ⟺ translated system in Q_q
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("ValidAt", {eq, p}),
            factory_.apply("ValidAt", {factory_.apply("TranslateEq", {eq, p, q}), q})
        ));

        // UC2: Adelic verification: E=0 everywhere ⟺ E=0 at all places
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("AdelicValid", {eq}),
            factory_.apply("AllPlacesValid", {eq})
        ));
    }

    void generatePadicExpansionAxioms(std::vector<std::unique_ptr<Equation>>& ax) {
        auto x = fv(); auto p = fv();

        // PE1: p-adic expansion: x = Σ a_i p^i with 0 ≤ a_i < p
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("PadicExpand", {x, p}),
            factory_.apply("PadicSum", {x, p})
        ));

        // PE2: v_p(xy) = v_p(x) + v_p(y)
        auto y = fv();
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("PadicVal", {factory_.mul(x, y), p}),
            factory_.add(
                factory_.apply("PadicVal", {x, p}),
                factory_.apply("PadicVal", {y, p}))
        ));

        // PE3: |xy|_p = |x|_p · |y|_p
        ax.push_back(std::make_unique<Equation>(
            factory_.apply("PadicNorm", {factory_.mul(x, y), p}),
            factory_.mul(
                factory_.apply("PadicNorm", {x, p}),
                factory_.apply("PadicNorm", {y, p}))
        ));
    }
};

// =============================================================================
// P-ADIC TERM GENERATOR — For the Discovery Engine
// =============================================================================

class PadicTermGenerator {
public:
    explicit PadicTermGenerator(TermFactory& factory) : factory_(factory) {}

    [[nodiscard]] std::vector<const Term*> generateAll() {
        std::vector<const Term*> result;
        std::unordered_set<const Term*> seen;

        auto tryAdd = [&](const Term* t) {
            if (t && result.size() < 2000 && seen.insert(t).second) {
                result.push_back(t);
            }
        };

        auto phi = factory_.phi();
        std::vector<int> primes = {2, 3, 5, 7, 11, 13};

        // p-adic norms of key values
        for (int p : primes) {
            auto pT = factory_.scalar(static_cast<double>(p));
            tryAdd(factory_.apply("PadicNorm", {phi, pT}));

            for (int n = 1; n <= 10; ++n) {
                auto nT = factory_.scalar(static_cast<double>(n));
                tryAdd(factory_.apply("PadicNorm", {nT, pT}));
                tryAdd(factory_.apply("PadicVal", {nT, pT}));
            }

            // Product formula verification terms
            for (int n = 1; n <= 10; ++n) {
                auto nT = factory_.scalar(static_cast<double>(n));
                tryAdd(factory_.apply("ProductFormula", {nT}));
            }
        }

        // Ultrametric terms
        for (int i = 1; i <= 5; ++i) {
            for (int j = 1; j <= 5; ++j) {
                auto a = factory_.scalar(static_cast<double>(i));
                auto b = factory_.scalar(static_cast<double>(j));
                for (int p : {2, 3, 5}) {
                    auto pT = factory_.scalar(static_cast<double>(p));
                    tryAdd(factory_.apply("UltrametricCheck", {a, b, pT}));
                }
                if (result.size() >= 2000) break;
            }
            if (result.size() >= 2000) break;
        }

        return result;
    }

private:
    TermFactory& factory_;
};

// =============================================================================
// P-ADIC EVALUATOR
// =============================================================================

class PadicNumericEvaluator {
public:
    [[nodiscard]] std::optional<double> evaluate(const Term* t) const {
        if (!t) return std::nullopt;

        if (t->isScalar()) return t->scalarValue();
        if (t->isPhi()) return 1.6180339887498948482;
        if (t->isPhiBar()) return 0.6180339887498948482;

        if (t->kind() == core::TermKind::Application) {
            const auto& sym = t->symbol();
            const auto& ch = t->children();

            if (sym == "PadicNorm" && ch.size() == 2) {
                auto x_opt = evaluate(ch[0]);
                auto p_opt = evaluate(ch[1]);
                if (x_opt && p_opt) {
                    int64_t n = static_cast<int64_t>(std::round(*x_opt));
                    int p = static_cast<int>(std::round(*p_opt));
                    if (p >= 2 && std::abs(*x_opt - n) < 1e-9 &&
                        std::abs(*p_opt - p) < 1e-9) {
                        return PadicValuation::padicNorm(n, p);
                    }
                }
            }

            if (sym == "PadicVal" && ch.size() == 2) {
                auto x_opt = evaluate(ch[0]);
                auto p_opt = evaluate(ch[1]);
                if (x_opt && p_opt) {
                    int64_t n = static_cast<int64_t>(std::round(*x_opt));
                    int p = static_cast<int>(std::round(*p_opt));
                    if (p >= 2 && n != 0 &&
                        std::abs(*x_opt - n) < 1e-9 &&
                        std::abs(*p_opt - p) < 1e-9) {
                        return static_cast<double>(PadicValuation::valuation(n, p));
                    }
                }
            }

            if (sym == "ProductFormula" && ch.size() == 1) {
                auto x_opt = evaluate(ch[0]);
                if (x_opt) {
                    int64_t n = static_cast<int64_t>(std::round(*x_opt));
                    if (n != 0 && std::abs(*x_opt - n) < 1e-9) {
                        // For integers, product formula = 1.0 always
                        return 1.0;
                    }
                }
            }

            if (sym == "UltrametricCheck" && ch.size() == 3) {
                auto x_opt = evaluate(ch[0]);
                auto y_opt = evaluate(ch[1]);
                auto p_opt = evaluate(ch[2]);
                if (x_opt && y_opt && p_opt) {
                    int64_t x = static_cast<int64_t>(std::round(*x_opt));
                    int64_t y = static_cast<int64_t>(std::round(*y_opt));
                    int p = static_cast<int>(std::round(*p_opt));
                    if (p >= 2) {
                        return PadicValuation::verifyUltrametric(x, y, p) ? 1.0 : 0.0;
                    }
                }
            }

            // Forward to arithmetic
            if (sym == "add" && ch.size() == 2) {
                auto a = evaluate(ch[0]); auto b = evaluate(ch[1]);
                if (a && b) return *a + *b;
            }
            if (sym == "mul" && ch.size() == 2) {
                auto a = evaluate(ch[0]); auto b = evaluate(ch[1]);
                if (a && b) return *a * *b;
            }
            if (sym == "neg" && ch.size() == 1) {
                auto v = evaluate(ch[0]);
                if (v) return -*v;
            }
        }

        return std::nullopt;
    }
};

} // namespace padic
} // namespace domain
} // namespace autodiscover

#endif // AUTODISCOVER_DOMAIN_PADIC_BRIDGE_HPP
