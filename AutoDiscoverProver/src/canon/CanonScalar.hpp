#ifndef AUTODISCOVER_CANON_SCALAR_HPP
#define AUTODISCOVER_CANON_SCALAR_HPP

/**
 * @file CanonScalar.hpp
 * @brief Master API for canonical scalar mapping
 * 
 * THE GRAND UNIFICATION
 * =====================
 * 
 * This module implements the complete canonical scalar mapping:
 * 
 *   CanonScalar : E(,X)  C
 * 
 * Where:
 *   - E(,X) = equations over terms with signature  and variables X
 *   - C = canonical scalar space (integer codes + semantic signatures)
 * 
 * Core Invariant:
 * ---------------
 * 
 *   CanonScalar(e) := (Encode(NF(e)), SemSig(e))
 * 
 * Properties:
 *   - DETERMINISTIC: Same equation always produces same scalar
 *   - COMPLETE*: e  e  CanonScalar(e) = CanonScalar(e)
 *     (*depends on NF confluence  guaranteed only when the rewrite system
 *      is convergent, which requires a complete Knuth-Bendix completion.
 *      In practice, the GOD normalizer approximates this.)
 *   - SOUND*: CanonScalar(e) = CanonScalar(e)  e  e (via structural code)
 *     (*modulo injective encoding; the structural code is injective by
 *      construction, but semantic signatures use floating-point probes 
 *      which have false-positive risk ~2^{-53} per probe.)
 *   - REPRODUCIBLE: Independent of hash order, thread timing, etc.
 *
 * @author AutoDiscover Prover
 * @date 2024
 */

#include "../core/Term.hpp"
#include "Equivalence.hpp"
#include "NFEngine.hpp"
#include "../encoding/Structural.hpp"
#include "../fingerprint/Semantic.hpp"
#include "../fingerprint/Fingerprinter.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <sstream>

namespace autodiscover {
namespace canon {

// ===========================================================================
// CANON SCALAR RESULT
// ===========================================================================

/**
 * @brief Complete canonical scalar for a term or equation
 */
class CanonScalarResult {
public:
    encoding::StructuralCode structuralCode;    // Channel 1: Collision-free
    fingerprint::SemanticSignature semantic;    // Channel 2: Similarity
    fingerprint::Fingerprint fingerprint;       // Hash for quick lookup
    const core::Term* normalForm = nullptr;     // The normalized term (raw ptr)
    
    // Metadata
    size_t originalSize = 0;      // Size of input term
    size_t normalizedSize = 0;    // Size of normal form
    double normalizationTimeMs = 0.0;
    std::string profileUsed;      // Which equivalence profile
    
    CanonScalarResult() = default;
    
    // -----------------------------------------------------------------------
    // COMPARISON
    // -----------------------------------------------------------------------
    
    /**
     * @brief Structural equality (exact, collision-free)
     */
    [[nodiscard]] bool structurallyEquals(const CanonScalarResult& other) const {
        return structuralCode == other.structuralCode;
    }
    
    /**
     * @brief Semantic similarity (approximate)
     */
    [[nodiscard]] double semanticDistance(const CanonScalarResult& other) const {
        return semantic.distanceTo(other.semantic);
    }
    
    /**
     * @brief Quick equality via fingerprint
     */
    [[nodiscard]] bool fingerprintEquals(const CanonScalarResult& other) const {
        return fingerprint.hash256 == other.fingerprint.hash256;
    }
    
    // -----------------------------------------------------------------------
    // SERIALIZATION
    // -----------------------------------------------------------------------
    
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << "CanonScalar{\n";
        oss << "  structural: " << structuralCode.toString() << "\n";
        oss << "  semantic: " << semantic.dimension() << "-dim signature\n";
        oss << "  fingerprint: " << fingerprint.hash256.toHex().substr(0, 16) << "...\n";
        oss << "  profile: " << profileUsed << "\n";
        oss << "  normTime: " << normalizationTimeMs << "ms\n";
        oss << "}";
        return oss.str();
    }
    
    [[nodiscard]] size_t hash() const {
        return fingerprint.hash256.toSizeT();
    }
};

// ===========================================================================
// EQUATION CANON SCALAR
// ===========================================================================

/**
 * @brief Canonical scalar for an equation (lhs = rhs)
 */
class EquationCanonScalar {
public:
    CanonScalarResult lhs;
    CanonScalarResult rhs;
    encoding::StructuralCode combinedCode;  // Cantor-paired code
    fingerprint::Fingerprint combinedFingerprint;
    double residualEnergy = 0.0;  // |lhs - rhs| at probes
    
    EquationCanonScalar() = default;
    
    /**
     * @brief Check if this is a tautology (lhs = rhs structurally)
     */
    [[nodiscard]] bool isTautology() const {
        return lhs.structurallyEquals(rhs);
    }
    
    /**
     * @brief Check if semantically holds (zero residual)
     */
    [[nodiscard]] bool semanticallyHolds(double tol = 1e-10) const {
        return residualEnergy < tol * tol;
    }
    
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << "EquationCanonScalar{\n";
        oss << "  lhs: " << lhs.structuralCode.toString() << "\n";
        oss << "  rhs: " << rhs.structuralCode.toString() << "\n";
        oss << "  combined: " << combinedCode.toString() << "\n";
        oss << "  residual: " << residualEnergy << "\n";
        oss << "  tautology: " << (isTautology() ? "yes" : "no") << "\n";
        oss << "}";
        return oss.str();
    }
    
    [[nodiscard]] size_t hash() const {
        return combinedFingerprint.hash256.toSizeT();
    }
};

// ===========================================================================
// CANON SCALAR ENGINE
// ===========================================================================

/**
 * @brief Main engine for computing canonical scalars
 * 
 * This is the master API that combines all components:
 *   - NFEngine for normalization
 *   - StructuralEncoder for integer codes
 *   - SemanticScalarizer for signatures
 *   - Fingerprinter for hashing
 */
class CanonScalarEngine {
public:
    struct Stats {
        size_t totalTerms = 0;
        size_t totalEquations = 0;
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
        std::chrono::nanoseconds totalNormTime{0};
        std::chrono::nanoseconds totalEncodeTime{0};
        std::chrono::nanoseconds totalSemanticTime{0};
        
        [[nodiscard]] double avgNormTimeMs() const {
            size_t total = totalTerms + totalEquations;
            return total == 0 ? 0.0 : 
                static_cast<double>(totalNormTime.count()) / total / 1e6;
        }
        
        [[nodiscard]] double cacheHitRate() const {
            size_t total = cacheHits + cacheMisses;
            return total == 0 ? 0.0 : static_cast<double>(cacheHits) / total;
        }
    };
    
private:
    NFEngine nfEngine_;
    encoding::StructuralEncoder structEncoder_;
    encoding::TermEncoder termEncoder_;
    fingerprint::SemanticScalarizer semScalarizer_;
    fingerprint::Fingerprinter fingerprinter_;
    
    // Cache: term id  result
    mutable std::unordered_map<core::TermId, CanonScalarResult> termCache_;
    mutable std::mutex cacheMutex_;
    mutable Stats stats_;
    
    static constexpr size_t MAX_CACHE_SIZE = 50000;
    
public:
    // -----------------------------------------------------------------------
    // CONSTRUCTORS
    // -----------------------------------------------------------------------
    
    explicit CanonScalarEngine(core::TermFactory& factory) 
        : nfEngine_(EquivalenceProfile::standard(), factory),
          structEncoder_(encoding::EncodingStrategy::POSITIONAL_256) {}
    
    CanonScalarEngine(EquivalenceProfile profile, core::TermFactory& factory)
        : nfEngine_(std::move(profile), factory),
          structEncoder_(encoding::EncodingStrategy::POSITIONAL_256) {}
    
    CanonScalarEngine(
        EquivalenceProfile profile,
        encoding::EncodingStrategy encStrategy,
        core::TermFactory& factory
    ) : nfEngine_(std::move(profile), factory),
        structEncoder_(encStrategy) {}
    
    // -----------------------------------------------------------------------
    // MAIN API: TERMS
    // -----------------------------------------------------------------------
    
    /**
     * @brief Compute canonical scalar for a term
     */
    [[nodiscard]] CanonScalarResult computeTerm(const core::Term* term) {
        if (!term) {
            CanonScalarResult empty;
            empty.profileUsed = nfEngine_.profile().name();
            return empty;
        }
        
        // Check cache
        {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            auto it = termCache_.find(term->id());
            if (it != termCache_.end()) {
                ++stats_.cacheHits;
                return it->second;
            }
            ++stats_.cacheMisses;
        }
        
        CanonScalarResult result;
        result.originalSize = term->size();
        result.profileUsed = nfEngine_.profile().name();
        
        // Step 1: Normalize
        auto normStart = std::chrono::high_resolution_clock::now();
        result.normalForm = nfEngine_.normalize(term);
        auto normEnd = std::chrono::high_resolution_clock::now();
        
        if (result.normalForm) {
            result.normalizedSize = result.normalForm->size();
        }
        auto normDuration = normEnd - normStart;
        result.normalizationTimeMs = 
            std::chrono::duration<double, std::milli>(normDuration).count();
        
        // Step 2: Structural encoding
        auto encStart = std::chrono::high_resolution_clock::now();
        if (result.normalForm) {
            result.structuralCode = structEncoder_.encode(*result.normalForm);
        }
        auto encEnd = std::chrono::high_resolution_clock::now();
        
        // Step 3: Semantic signature
        auto semStart = std::chrono::high_resolution_clock::now();
        if (result.normalForm) {
            result.semantic = semScalarizer_.signature(*result.normalForm);
        }
        auto semEnd = std::chrono::high_resolution_clock::now();
        
        // Step 4: Fingerprint
        if (result.normalForm) {
            auto termBytes = termEncoder_.encode(result.normalForm);
            result.fingerprint = fingerprinter_.fp_term(termBytes);
        }
        
        // Update stats
        ++stats_.totalTerms;
        stats_.totalNormTime += normDuration;
        stats_.totalEncodeTime += encEnd - encStart;
        stats_.totalSemanticTime += semEnd - semStart;
        
        // Cache result
        {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            if (termCache_.size() < MAX_CACHE_SIZE) {
                termCache_[term->id()] = result;
            }
        }
        
        return result;
    }
    
    // -----------------------------------------------------------------------
    // MAIN API: EQUATIONS
    // -----------------------------------------------------------------------
    
    /**
     * @brief Compute canonical scalar for an equation
     */
    [[nodiscard]] EquationCanonScalar computeEquation(
        const core::Term* lhs,
        const core::Term* rhs
    ) {
        EquationCanonScalar result;
        
        // Compute each side
        result.lhs = computeTerm(lhs);
        result.rhs = computeTerm(rhs);
        
        // Combined structural code (Cantor pairing)
        encoding::StructuralEquationEncoder eqEncoder(structEncoder_.strategy());
        if (result.lhs.normalForm && result.rhs.normalForm) {
            result.combinedCode = eqEncoder.encode(
                *result.lhs.normalForm, 
                *result.rhs.normalForm
            );
        }
        
        // Combined fingerprint
        if (result.lhs.normalForm && result.rhs.normalForm) {
            // Encode both terms
            auto lhsBytes = termEncoder_.encode(result.lhs.normalForm);
            auto rhsBytes = termEncoder_.encode(result.rhs.normalForm);
            // Combine: [lhsLen as varint][lhsBytes][rhsBytes]
            std::vector<uint8_t> combinedBytes;
            combinedBytes.reserve(lhsBytes.size() + rhsBytes.size() + 4);
            // Add lhs length prefix
            uint64_t lhsLen = lhsBytes.size();
            while (lhsLen >= 0x80) {
                combinedBytes.push_back(static_cast<uint8_t>(lhsLen | 0x80));
                lhsLen >>= 7;
            }
            combinedBytes.push_back(static_cast<uint8_t>(lhsLen));
            combinedBytes.insert(combinedBytes.end(), lhsBytes.begin(), lhsBytes.end());
            combinedBytes.insert(combinedBytes.end(), rhsBytes.begin(), rhsBytes.end());
            result.combinedFingerprint = fingerprinter_.fp_equation(combinedBytes);
        }
        
        // Residual energy
        if (result.lhs.normalForm && result.rhs.normalForm) {
            result.residualEnergy = semScalarizer_.residualEnergy(
                *result.lhs.normalForm,
                *result.rhs.normalForm
            );
        }
        
        ++stats_.totalEquations;
        
        return result;
    }
    
    // -----------------------------------------------------------------------
    // EQUIVALENCE CHECKING
    // -----------------------------------------------------------------------
    
    /**
     * @brief Check if two terms are equivalent under the current profile
     */
    [[nodiscard]] bool areEquivalent(
        const core::Term* t1,
        const core::Term* t2
    ) {
        auto cs1 = computeTerm(t1);
        auto cs2 = computeTerm(t2);
        return cs1.structurallyEquals(cs2);
    }
    
    /**
     * @brief Check if two terms are semantically similar
     */
    [[nodiscard]] bool areSimilar(
        const core::Term* t1,
        const core::Term* t2,
        double tol = 1e-10
    ) {
        auto cs1 = computeTerm(t1);
        auto cs2 = computeTerm(t2);
        return cs1.semanticDistance(cs2) < tol;
    }
    
    // -----------------------------------------------------------------------
    // BATCH OPERATIONS
    // -----------------------------------------------------------------------
    
    /**
     * @brief Compute canonical scalars for multiple terms
     */
    [[nodiscard]] std::vector<CanonScalarResult> computeTerms(
        const std::vector<const core::Term*>& terms
    ) {
        std::vector<CanonScalarResult> results;
        results.reserve(terms.size());
        for (const auto* t : terms) {
            results.push_back(computeTerm(t));
        }
        return results;
    }
    
    /**
     * @brief Find duplicate terms in a collection
     */
    [[nodiscard]] std::vector<std::vector<size_t>> findDuplicates(
        const std::vector<const core::Term*>& terms
    ) {
        auto results = computeTerms(terms);
        
        // Group by structural code
        std::unordered_map<size_t, std::vector<size_t>> groups;
        for (size_t i = 0; i < results.size(); ++i) {
            size_t h = results[i].hash();
            groups[h].push_back(i);
        }
        
        // Return groups with more than one member
        std::vector<std::vector<size_t>> duplicates;
        for (auto& [_, indices] : groups) {
            if (indices.size() > 1) {
                duplicates.push_back(std::move(indices));
            }
        }
        
        return duplicates;
    }
    
    // -----------------------------------------------------------------------
    // CONFIGURATION
    // -----------------------------------------------------------------------
    
    void setProfile(EquivalenceProfile profile) {
        nfEngine_.setProfile(std::move(profile));
        clearCache();
    }
    
    void setEncodingStrategy(encoding::EncodingStrategy strat) {
        structEncoder_.setStrategy(strat);
        clearCache();
    }
    
    void setProbeFamily(fingerprint::ProbeFamily family) {
        semScalarizer_.setDefaultFamily(std::move(family));
    }
    
    // -----------------------------------------------------------------------
    // CACHE AND STATS
    // -----------------------------------------------------------------------
    
    void clearCache() {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        termCache_.clear();
        stats_ = Stats{};
    }
    
    [[nodiscard]] size_t cacheSize() const {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        return termCache_.size();
    }
    
    [[nodiscard]] const Stats& stats() const { return stats_; }
    [[nodiscard]] const EquivalenceProfile& profile() const { return nfEngine_.profile(); }
    [[nodiscard]] NFEngine& nfEngine() { return nfEngine_; }
    [[nodiscard]] core::TermFactory& factory() { return nfEngine_.factory(); }
};

// ===========================================================================
// PRESET ENGINES
// ===========================================================================

namespace presets {

/**
 * @brief Minimal engine: just alpha + AC normalization
 */
inline CanonScalarEngine minimalEngine(core::TermFactory& factory) {
    return CanonScalarEngine(EquivalenceProfile::minimal(), factory);
}

/**
 * @brief Golden engine: includes  normalization
 */
inline CanonScalarEngine goldenEngine(core::TermFactory& factory) {
    return CanonScalarEngine(
        EquivalenceProfile::golden(),
        encoding::EncodingStrategy::ZECKENDORF,
        factory
    );
}

/**
 * @brief Full engine: all layers enabled
 */
inline CanonScalarEngine fullEngine(core::TermFactory& factory) {
    return CanonScalarEngine(EquivalenceProfile::full(), factory);
}

/**
 * @brief Zeckendorf engine: uses -based encoding
 */
inline CanonScalarEngine zeckendorfEngine(core::TermFactory& factory) {
    return CanonScalarEngine(
        EquivalenceProfile::standard(),
        encoding::EncodingStrategy::ZECKENDORF,
        factory
    );
}

} // namespace presets

// ===========================================================================
// GLOBAL ENGINE
// ===========================================================================

/**
 * @brief Global canon scalar engine  uses globalContext().factory()
 */
inline CanonScalarEngine& globalCanonScalarEngine() {
    static CanonScalarEngine engine(core::globalContext().factory());
    return engine;
}

/**
 * @brief Quick canonical scalar computation
 */
inline CanonScalarResult canonScalar(const core::Term* t) {
    return globalCanonScalarEngine().computeTerm(t);
}

/**
 * @brief Quick equivalence check via canonical scalar
 */
inline bool canonEquiv(const core::Term* t1, const core::Term* t2) {
    return globalCanonScalarEngine().areEquivalent(t1, t2);
}

/**
 * @brief Quick structural code
 */
inline encoding::StructuralCode structCode(const core::Term* t) {
    return canonScalar(t).structuralCode;
}

/**
 * @brief Quick semantic signature
 */
inline fingerprint::SemanticSignature semSig(const core::Term* t) {
    return canonScalar(t).semantic;
}

} // namespace canon
} // namespace autodiscover

#endif // AUTODISCOVER_CANON_SCALAR_HPP
