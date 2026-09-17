/**
 * @file CertificateExporter.cpp
 * @brief Implementation of proof certificate export
 */

#include "CertificateExporter.hpp"
#include "../core/Term.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>

namespace autodiscover {
namespace proof {

// Forward declaration for TSTP term conversion
static std::string termToTSTP(const core::Term* t);

//------------------------------------------------------------------------------
// Main export interface
//------------------------------------------------------------------------------

std::string CertificateExporter::exportProof(const Proof& proof, ExportFormat format) const {
    switch (format) {
        case ExportFormat::Text:
            return toText(proof);
        case ExportFormat::TSTP:
            return toTSTP(proof);
        case ExportFormat::JSON:
            return toJSON(proof);
        case ExportFormat::LaTeX:
            return toLaTeX(proof);
        default:
            return toText(proof);
    }
}

void CertificateExporter::exportToStream(const Proof& proof, ExportFormat format, 
                                          std::ostream& out) const {
    out << exportProof(proof, format);
}

//------------------------------------------------------------------------------
// Text format
//------------------------------------------------------------------------------

std::string CertificateExporter::toText(const Proof& proof) const {
    std::ostringstream oss;
    
    auto stats = ProofStatistics::compute(proof);
    
    oss << "========================================\n";
    oss << "PROOF CERTIFICATE\n";
    oss << "========================================\n\n";
    
    if (verbose_) {
        oss << "Statistics:\n";
        oss << stats.toString();
        oss << "\n";
    }
    
    oss << "Proof Steps:\n";
    oss << "------------\n\n";
    
    size_t stepNum = 1;
    for (auto stepId : proof.topologicalOrder()) {
        const ProofStep* step = proof.getStep(stepId);
        if (!step) continue;
        
        oss << stepNum++ << ". " << formatStepText(*step) << "\n";
    }
    
    oss << "\n========================================\n";
    oss << "END OF PROOF\n";
    oss << "========================================\n";
    
    return oss.str();
}

std::string CertificateExporter::formatStepText(const ProofStep& step) const {
    std::ostringstream oss;
    
    oss << "[" << step.id() << "] " << ruleName(step.rule());
    
    if (!step.premises().empty()) {
        oss << " (from: ";
        for (size_t i = 0; i < step.premises().size(); ++i) {
            if (i > 0) oss << ", ";
            oss << step.premises()[i];
        }
        oss << ")";
    }
    
    if (step.conclusion()) {
        oss << "\n" << indent_ << "=> " << step.conclusion()->toString();
    }
    
    if (includeAnnotations_ && !step.annotation().empty()) {
        oss << "\n" << indent_ << "// " << step.annotation();
    }
    
    return oss.str();
}

//------------------------------------------------------------------------------
// TSTP format (compatible with ATP systems)
//------------------------------------------------------------------------------

std::string CertificateExporter::toTSTP(const Proof& proof) const {
    std::ostringstream oss;
    
    auto stats = ProofStatistics::compute(proof);
    
    // TPTP-compliant header
    oss << "% File     : autodiscover_proof.p\n";
    oss << "% Domain   : Algebra (Cayley-Dickson)\n";
    oss << "% Problem  : Equational identity discovery\n";
    oss << "% Version  : AutoDiscoverProver 1.0\n";
    oss << "% Source   : AutoDiscoverProver\n";
    oss << "%\n";
    oss << "% Proof statistics:\n";
    oss << "%   Total steps     : " << stats.totalSteps << "\n";
    oss << "%   Axiom steps     : " << stats.axiomSteps << "\n";
    oss << "%   Superposition   : " << stats.superpositionSteps << "\n";
    oss << "%   Demodulation    : " << stats.demodulationSteps << "\n";
    oss << "%   GOD canonical   : " << stats.godSteps << "\n";
    oss << "%   Max depth       : " << stats.maxDepth << "\n";
    oss << "%\n";
    oss << "% SZS status Theorem\n";
    oss << "% SZS output start Proof for autodiscover\n\n";
    
    size_t index = 0;
    for (auto stepId : proof.topologicalOrder()) {
        const ProofStep* step = proof.getStep(stepId);
        if (!step) continue;
        
        oss << formatStepTSTP(*step, index++) << "\n";
    }
    
    oss << "\n% SZS output end Proof\n";
    
    return oss.str();
}

std::string CertificateExporter::formatStepTSTP(const ProofStep& step, size_t /*index*/) const {
    std::ostringstream oss;
    
    // fof(step_N, type, formula, source).
    oss << "fof(step_" << step.id() << ", ";
    
    // Step type
    if (step.rule() == InferenceRule::Axiom) {
        oss << "axiom";
    } else if (step.rule() == InferenceRule::Hypothesis) {
        oss << "negated_conjecture";
    } else if (step.rule() == InferenceRule::EqualityResolution && !step.conclusion()) {
        oss << "plain";  // contradiction step
    } else {
        oss << "plain";
    }
    
    oss << ", ";
    
    // Formula
    if (step.conclusion()) {
        oss << "equal(" << termToTSTP(step.conclusion()->lhs()) 
            << ", " << termToTSTP(step.conclusion()->rhs()) << ")";
    } else {
        // Contradiction (empty clause)
        oss << "$false";
    }
    
    // Source annotation
    if (step.rule() == InferenceRule::Axiom) {
        oss << ", introduced(axiom, [";
        if (!step.annotation().empty()) {
            oss << "'" << step.annotation() << "'";
        }
        oss << "])";
    } else if (step.rule() == InferenceRule::Hypothesis) {
        oss << ", introduced(assumption, [negated_conjecture])";
    } else {
        oss << ", inference(" << ruleTSTPName(step.rule()) << ", [status(thm)], [";
        for (size_t i = 0; i < step.premises().size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "step_" << step.premises()[i];
        }
        oss << "])";
    }
    
    oss << ").";
    
    return oss.str();
}

std::string CertificateExporter::ruleTSTPName(InferenceRule rule) const {
    switch (rule) {
        case InferenceRule::Axiom: return "axiom";
        case InferenceRule::Hypothesis: return "assumption";
        case InferenceRule::SuperpositionLeft: return "superposition_left";
        case InferenceRule::SuperpositionRight: return "superposition_right";
        case InferenceRule::Demodulation: return "demodulation";
        case InferenceRule::EqualityResolution: return "eq_resolution";
        case InferenceRule::GODNormalization: return "god_normalization";
        case InferenceRule::Reflexivity: return "reflexivity";
        case InferenceRule::Symmetry: return "symmetry";
        case InferenceRule::Transitivity: return "transitivity";
        case InferenceRule::Congruence: return "congruence";
        case InferenceRule::Substitution: return "substitution";
        default: return "unknown";
    }
}

//------------------------------------------------------------------------------
// JSON format
//------------------------------------------------------------------------------

std::string CertificateExporter::toJSON(const Proof& proof) const {
    std::ostringstream oss;
    
    auto stats = ProofStatistics::compute(proof);
    auto order = proof.topologicalOrder();
    
    oss << "{\n";
    oss << indent_ << "\"type\": \"proof_certificate\",\n";
    oss << indent_ << "\"version\": \"1.0\",\n";
    
    // Statistics
    oss << indent_ << "\"statistics\": {\n";
    oss << indent_ << indent_ << "\"totalSteps\": " << stats.totalSteps << ",\n";
    oss << indent_ << indent_ << "\"maxDepth\": " << stats.maxDepth << ",\n";
    oss << indent_ << indent_ << "\"axiomSteps\": " << stats.axiomSteps << ",\n";
    oss << indent_ << indent_ << "\"rewriteSteps\": " << stats.rewriteSteps << ",\n";
    oss << indent_ << indent_ << "\"superpositionSteps\": " << stats.superpositionSteps << "\n";
    oss << indent_ << "},\n";
    
    // Steps
    oss << indent_ << "\"steps\": [\n";
    
    for (size_t i = 0; i < order.size(); ++i) {
        const ProofStep* step = proof.getStep(order[i]);
        if (!step) continue;
        
        oss << formatStepJSON(*step, i == order.size() - 1);
    }
    
    oss << indent_ << "]\n";
    oss << "}\n";
    
    return oss.str();
}

std::string CertificateExporter::formatStepJSON(const ProofStep& step, bool isLast) const {
    std::ostringstream oss;
    
    oss << indent_ << indent_ << "{\n";
    oss << indent_ << indent_ << indent_ << "\"id\": " << step.id() << ",\n";
    oss << indent_ << indent_ << indent_ << "\"rule\": \"" << ruleName(step.rule()) << "\",\n";
    
    // Premises
    oss << indent_ << indent_ << indent_ << "\"premises\": [";
    for (size_t i = 0; i < step.premises().size(); ++i) {
        if (i > 0) oss << ", ";
        oss << step.premises()[i];
    }
    oss << "],\n";
    
    // Conclusion
    if (step.conclusion()) {
        oss << indent_ << indent_ << indent_ << "\"conclusion\": \"" 
            << escapeJSON(step.conclusion()->toString()) << "\"";
    } else {
        oss << indent_ << indent_ << indent_ << "\"conclusion\": null";
    }
    
    if (includeAnnotations_ && !step.annotation().empty()) {
        oss << ",\n" << indent_ << indent_ << indent_ 
            << "\"annotation\": \"" << escapeJSON(step.annotation()) << "\"";
    }
    
    oss << "\n" << indent_ << indent_ << "}" << (isLast ? "" : ",") << "\n";
    
    return oss.str();
}

//------------------------------------------------------------------------------
// LaTeX format
//------------------------------------------------------------------------------

std::string CertificateExporter::toLaTeX(const Proof& proof) const {
    std::ostringstream oss;
    
    oss << "\\documentclass{article}\n";
    oss << "\\usepackage{amsmath,amssymb}\n";
    oss << "\\usepackage{bussproofs}\n\n";
    oss << "\\begin{document}\n\n";
    oss << "\\section*{Proof Certificate}\n\n";
    
    auto stats = ProofStatistics::compute(proof);
    
    oss << "\\textbf{Statistics:} " << stats.totalSteps << " steps, ";
    oss << "depth " << stats.maxDepth << "\n\n";
    
    oss << "\\begin{enumerate}\n";
    
    for (auto stepId : proof.topologicalOrder()) {
        const ProofStep* step = proof.getStep(stepId);
        if (!step) continue;
        
        oss << formatStepLaTeX(*step);
    }
    
    oss << "\\end{enumerate}\n\n";
    oss << "\\end{document}\n";
    
    return oss.str();
}

std::string CertificateExporter::formatStepLaTeX(const ProofStep& step) const {
    std::ostringstream oss;
    
    oss << "\\item[" << step.id() << ".] ";
    oss << "\\textsc{" << ruleName(step.rule()) << "}";
    
    if (!step.premises().empty()) {
        oss << " (from ";
        for (size_t i = 0; i < step.premises().size(); ++i) {
            if (i > 0) oss << ", ";
            oss << step.premises()[i];
        }
        oss << ")";
    }
    
    if (step.conclusion()) {
        oss << "\n  \\[" << escapeLaTeX(step.conclusion()->toString()) << "\\]\n";
    } else {
        oss << "\n";
    }
    
    return oss.str();
}

//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

std::string CertificateExporter::escapeJSON(const std::string& s) const {
    std::ostringstream oss;
    for (char c : s) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default: oss << c;
        }
    }
    return oss.str();
}

std::string CertificateExporter::escapeLaTeX(const std::string& s) const {
    std::ostringstream oss;
    for (char c : s) {
        switch (c) {
            case '_': oss << "\\_"; break;
            case '%': oss << "\\%"; break;
            case '&': oss << "\\&"; break;
            case '#': oss << "\\#"; break;
            case '{': oss << "\\{"; break;
            case '}': oss << "\\}"; break;
            case '$': oss << "\\$"; break;
            default: oss << c;
        }
    }
    return oss.str();
}

//------------------------------------------------------------------------------
// ProofStatistics
//------------------------------------------------------------------------------

ProofStatistics ProofStatistics::compute(const Proof& proof) {
    ProofStatistics stats;
    stats.totalSteps = proof.size();
    stats.maxDepth = proof.depth();
    
    for (const auto* step : proof.allSteps()) {
        switch (step->rule()) {
            case InferenceRule::Axiom:
                stats.axiomSteps++;
                break;
            case InferenceRule::SuperpositionLeft:
            case InferenceRule::SuperpositionRight:
                stats.superpositionSteps++;
                break;
            case InferenceRule::Demodulation:
                stats.demodulationSteps++;
                break;
            case InferenceRule::GODNormalization:
                stats.godSteps++;
                break;
            default:
                stats.otherSteps++;
                break;
        }
    }
    
    return stats;
}

std::string ProofStatistics::toString() const {
    std::ostringstream oss;
    oss << "  Total steps: " << totalSteps << "\n";
    oss << "  Max depth: " << maxDepth << "\n";
    oss << "  Axioms: " << axiomSteps << "\n";
    oss << "  Rewrites: " << rewriteSteps << "\n";
    oss << "  Superposition: " << superpositionSteps << "\n";
    oss << "  Demodulation: " << demodulationSteps << "\n";
    oss << "  GOD canonical: " << godSteps << "\n";
    oss << "  Other: " << otherSteps << "\n";
    return oss.str();
}

// Helper for TSTP term conversion.
// TPTP conventions: variables are UPPERCASE, functors are lowercase.
static std::string termToTSTP(const core::Term* t) {
    if (!t) return "$true";
    
    switch (t->kind()) {
        case core::TermKind::Variable: {
            // TPTP variables must start with uppercase
            std::string var = t->symbol();
            if (!var.empty() && var[0] >= 'a' && var[0] <= 'z') {
                var[0] = static_cast<char>(var[0] - 'a' + 'A');
            }
            return var;
        }
            
        case core::TermKind::Constant: {
            // TPTP functors must start with lowercase or be single-quoted
            std::string sym = t->symbol();
            if (!sym.empty() && sym[0] >= 'A' && sym[0] <= 'Z') {
                return "'" + sym + "'";
            }
            return sym;
        }
            
        case core::TermKind::Application: {
            std::ostringstream oss;
            std::string sym = t->symbol();
            // Ensure functor starts lowercase or is quoted
            if (!sym.empty() && sym[0] >= 'A' && sym[0] <= 'Z') {
                oss << "'" << sym << "'";
            } else {
                oss << sym;
            }
            if (!t->children().empty()) {
                oss << "(";
                for (size_t i = 0; i < t->children().size(); ++i) {
                    if (i > 0) oss << ", ";
                    oss << termToTSTP(t->children()[i]);
                }
                oss << ")";
            }
            return oss.str();
        }
        
        case core::TermKind::Pair: {
            std::ostringstream oss;
            oss << "cd_pair(";
            if (t->children().size() >= 2) {
                oss << termToTSTP(t->children()[0]) << ", ";
                oss << termToTSTP(t->children()[1]);
            }
            oss << ")";
            return oss.str();
        }
            
        case core::TermKind::Phi:
            return "golden_ratio";
            
        case core::TermKind::PhiBar:
            return "golden_conjugate";
            
        case core::TermKind::J:
            return "j_unit";
        
        case core::TermKind::Scalar: {
            std::ostringstream oss;
            oss << "scalar(" << t->scalarValue() << ")";
            return oss.str();
        }
            
        default:
            return "unknown";
    }
}

} // namespace proof
} // namespace autodiscover
