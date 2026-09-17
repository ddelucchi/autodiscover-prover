/**
 * @file CertificateExporter.hpp
 * @brief Proof certificate export in multiple formats
 * 
 * Exports verified proofs to:
 * - Human-readable text format
 * - Machine-checkable format (TSTP-like)
 * - JSON for tooling integration
 * - LaTeX for papers
 * 
 * Method implementations are in CertificateExporter.cpp.
 */

#ifndef AUTODISCOVER_PROOF_CERTIFICATE_EXPORTER_HPP
#define AUTODISCOVER_PROOF_CERTIFICATE_EXPORTER_HPP

#include "Proof.hpp"
#include <string>
#include <ostream>
#include <memory>

namespace autodiscover {
namespace proof {

//------------------------------------------------------------------------------
// Export format enumeration
//------------------------------------------------------------------------------

enum class ExportFormat {
    Text,           // Human-readable text
    TSTP,           // TPTP/TSTP format for ATP systems
    JSON,           // JSON for tooling
    LaTeX,          // LaTeX for papers
    Coq,            // Coq proof terms (future)
    Lean            // Lean proof terms (future)
};

//------------------------------------------------------------------------------
// Certificate exporter
//------------------------------------------------------------------------------

class CertificateExporter {
public:
    CertificateExporter() = default;
    
    // Main export interface
    std::string exportProof(const Proof& proof, ExportFormat format) const;
    
    // Stream-based export
    void exportToStream(const Proof& proof, ExportFormat format, std::ostream& out) const;
    
    // Format-specific exports
    std::string toText(const Proof& proof) const;
    std::string toTSTP(const Proof& proof) const;
    std::string toJSON(const Proof& proof) const;
    std::string toLaTeX(const Proof& proof) const;
    
    // Configuration
    void setVerbose(bool verbose) { verbose_ = verbose; }
    void setIncludeAnnotations(bool include) { includeAnnotations_ = include; }
    void setIndent(const std::string& indent) { indent_ = indent; }
    
private:
    bool verbose_ = true;
    bool includeAnnotations_ = true;
    std::string indent_ = "  ";
    
    // Helper methods
    std::string formatStepText(const ProofStep& step) const;
    std::string formatStepTSTP(const ProofStep& step, size_t index) const;
    std::string formatStepJSON(const ProofStep& step, bool isLast) const;
    std::string formatStepLaTeX(const ProofStep& step) const;
    
    std::string escapeJSON(const std::string& s) const;
    std::string escapeLaTeX(const std::string& s) const;
    std::string ruleTSTPName(InferenceRule rule) const;
};

//------------------------------------------------------------------------------
// Proof summary statistics
//------------------------------------------------------------------------------

struct ProofStatistics {
    size_t totalSteps = 0;
    size_t axiomSteps = 0;
    size_t rewriteSteps = 0;
    size_t superpositionSteps = 0;
    size_t demodulationSteps = 0;
    size_t godSteps = 0;
    size_t otherSteps = 0;
    size_t maxDepth = 0;
    
    static ProofStatistics compute(const Proof& proof);
    std::string toString() const;
};

} // namespace proof
} // namespace autodiscover

#endif // AUTODISCOVER_PROOF_CERTIFICATE_EXPORTER_HPP
