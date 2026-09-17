/**
 * @file Term.cpp
 * @brief Implementation of DAG-based term representation
 */

#include "Term.hpp"
#include "Constants.hpp"
#include <sstream>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <iomanip>

namespace autodiscover {
namespace core {

// Use canonical constants from Constants.hpp
using constants::PHI;
using constants::PHI_INV;

//------------------------------------------------------------------------------
// Term Implementation
//------------------------------------------------------------------------------

Term::Term(TermId id, TermKind kind, Sort sort,
           const std::string& symbol,
           std::vector<const Term*> children,
           double scalarValue)
    : id_(id)
    , kind_(kind)
    , sort_(sort)
    , symbol_(symbol)
    , children_(std::move(children))
    , scalarValue_(scalarValue)
    , depth_(0)
    , size_(1)
    , hash_(0)
    , hashComputed_(false)
{
    computeMetrics();
}

void Term::computeMetrics() {
    size_ = 1;
    depth_ = 0;
    
    for (const Term* child : children_) {
        if (child) {
            size_ += child->size_;
            depth_ = std::max(depth_, child->depth_);
        }
    }
    
    if (!children_.empty()) {
        depth_ += 1;
    }
}

size_t Term::computeHash() const noexcept {
    if (hashComputed_) return hash_;
    
    size_t h = std::hash<uint8_t>{}(static_cast<uint8_t>(kind_));
    h ^= std::hash<std::string>{}(symbol_) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<uint8_t>{}(static_cast<uint8_t>(sort_)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    
    if (kind_ == TermKind::Scalar) {
        // Use bitwise comparison for consistency with encode() which
        // uses a bit-pun hex representation. This ensures +0.0 != -0.0
        // and NaN == NaN (same bit pattern) just like the encoding does.
        uint64_t u;
        std::memcpy(&u, &scalarValue_, sizeof(double));
        h ^= std::hash<uint64_t>{}(u) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    
    for (const Term* child : children_) {
        if (child) {
            h ^= child->id() + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
    }
    
    hash_ = h;
    hashComputed_ = true;
    return h;
}

const Term* Term::first() const noexcept {
    if (kind_ == TermKind::Pair && children_.size() >= 1) {
        return children_[0];
    }
    return nullptr;
}

const Term* Term::second() const noexcept {
    if (kind_ == TermKind::Pair && children_.size() >= 2) {
        return children_[1];
    }
    return nullptr;
}

std::string Term::toString() const {
    std::ostringstream oss;
    
    switch (kind_) {
        case TermKind::Variable:
            oss << symbol_;
            break;
            
        case TermKind::Constant:
            oss << symbol_;
            break;
            
        case TermKind::Scalar:
            // Print scalars by exact value  never infer named constants
            // from approximate floats.  phi has its own TermKind::Phi.
            oss << std::setprecision(17) << scalarValue_;
            break;
            
        case TermKind::Phi:
            oss << "";
            break;
            
        case TermKind::PhiBar:
            oss << "";
            break;
            
        case TermKind::J:
            oss << "J";
            break;
            
        case TermKind::Pair:
            oss << "(";
            if (children_.size() > 0 && children_[0]) {
                oss << children_[0]->toString();
            }
            oss << ", ";
            if (children_.size() > 1 && children_[1]) {
                oss << children_[1]->toString();
            }
            oss << ")";
            break;
            
        case TermKind::Application:
            oss << symbol_ << "(";
            for (size_t i = 0; i < children_.size(); ++i) {
                if (i > 0) oss << ", ";
                if (children_[i]) {
                    oss << children_[i]->toString();
                }
            }
            oss << ")";
            break;
    }
    
    return oss.str();
}

std::string Term::encode() const {
    // Injective encoding for shortlex comparison.
    // Format: <kind_char>.<sort_char>.<symbol_length>:<symbol>.<scalar_hex>.<child_count>:<child_encodings>
    // Delimiters ('.', ':') ensure no ambiguity between numeric fields.
    std::ostringstream oss;
    
    // Encode kind (single char)
    oss << static_cast<char>('A' + static_cast<int>(kind_));
    oss << '.';
    
    // Encode sort (single char)
    oss << static_cast<char>('0' + static_cast<int>(sort_));
    oss << '.';
    
    // Encode symbol with explicit length prefix separated by ':'
    oss << symbol_.size() << ':' << symbol_;
    oss << '.';
    
    // Encode scalar value if applicable
    if (kind_ == TermKind::Scalar) {
        // Use memcpy for type-safe reinterpretation (no union punning UB)
        uint64_t bits;
        static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits");
        std::memcpy(&bits, &scalarValue_, sizeof(bits));
        oss << std::hex << std::setw(16) << std::setfill('0') << bits;
    }
    oss << '.';
    
    // Recursively encode children with count separated by ':'
    oss << children_.size() << ':';
    for (const Term* child : children_) {
        if (child) {
            oss << child->encode();
        } else {
            oss << "NULL.";
        }
    }
    
    return oss.str();
}

//------------------------------------------------------------------------------
// Hash and Equality for Interning
//------------------------------------------------------------------------------

size_t TermHash::operator()(const Term* t) const noexcept {
    if (!t) return 0;
    return t->computeHash();
}

bool TermEqual::operator()(const Term* a, const Term* b) const noexcept {
    if (a == b) return true;
    if (!a || !b) return false;
    
    if (a->kind() != b->kind()) return false;
    if (a->sort() != b->sort()) return false;
    if (a->symbol() != b->symbol()) return false;
    
    if (a->kind() == TermKind::Scalar) {
        // Bitwise comparison: consistent with computeHash() and encode().
        // Distinguishes +0.0 from -0.0; treats identical NaN bit-patterns as equal.
        uint64_t ua, ub;
        std::memcpy(&ua, &a->scalarValue_, sizeof(double));
        std::memcpy(&ub, &b->scalarValue_, sizeof(double));
        if (ua != ub) return false;
    }
    
    const auto& ac = a->children();
    const auto& bc = b->children();
    if (ac.size() != bc.size()) return false;
    
    for (size_t i = 0; i < ac.size(); ++i) {
        // Children should already be interned, so pointer comparison suffices
        if (ac[i] != bc[i]) return false;
    }
    
    return true;
}

//------------------------------------------------------------------------------
// Shortlex Comparator
//------------------------------------------------------------------------------

bool ShortlexComparator::operator()(const Term* a, const Term* b) const noexcept {
    if (!a && !b) return false;
    if (!a) return true; // nullptr < anything
    if (!b) return false;
    
    // First compare by size
    if (a->size() != b->size()) {
        return a->size() < b->size();
    }
    
    // Then by encoding (lexicographic)
    return a->encode() < b->encode();
}

//------------------------------------------------------------------------------
// TermFactory Implementation
//------------------------------------------------------------------------------

TermFactory::TermFactory() = default;
TermFactory::~TermFactory() = default;

TermId TermFactory::nextId() {
    return nextId_++;
}

const Term* TermFactory::intern(std::unique_ptr<Term> term) {
    // Check if an equivalent term already exists
    auto it = internMap_.find(term.get());
    if (it != internMap_.end()) {
        return it->second;
    }
    
    // Store the new term
    const Term* ptr = term.get();
    terms_.push_back(std::move(term));
    internMap_[ptr] = ptr;
    return ptr;
}

const Term* TermFactory::variable(const std::string& name, Sort sort) {
    return intern(std::unique_ptr<Term>(new Term(
        nextId(), TermKind::Variable, sort, name, std::vector<const Term*>{}
    )));
}

const Term* TermFactory::constant(const std::string& name, Sort sort) {
    return intern(std::unique_ptr<Term>(new Term(
        nextId(), TermKind::Constant, sort, name, std::vector<const Term*>{}
    )));
}

const Term* TermFactory::scalar(double value) {
    return intern(std::unique_ptr<Term>(new Term(
        nextId(), TermKind::Scalar, Sort::Real, "", std::vector<const Term*>{}, value
    )));
}

const Term* TermFactory::phi() {
    if (phiTerm_) return phiTerm_;
    
    phiTerm_ = intern(std::unique_ptr<Term>(new Term(
        nextId(), TermKind::Phi, Sort::Real, "phi", std::vector<const Term*>{}, PHI
    )));
    return phiTerm_;
}

const Term* TermFactory::phiBar() {
    if (phiBarTerm_) return phiBarTerm_;
    
    //  = 1    0.6180339887498949
    constexpr double PHI_BAR = 1.0 - 1.6180339887498949;
    phiBarTerm_ = intern(std::unique_ptr<Term>(new Term(
        nextId(), TermKind::PhiBar, Sort::Real, "phi_bar", std::vector<const Term*>{}, PHI_BAR
    )));
    return phiBarTerm_;
}

const Term* TermFactory::J(Sort level) {
    // J = (0, 1) in the Cayley-Dickson construction
    // J = -1
    return intern(std::unique_ptr<Term>(new Term(
        nextId(), TermKind::J, level, "J", std::vector<const Term*>{}
    )));
}

const Term* TermFactory::pair(const Term* first, const Term* second) {
    if (!first || !second) return nullptr;
    
    // Determine the sort of the pair (one level up in CD tower)
    Sort pairSort = Sort::Generic;
    if (first->sort() == Sort::Real && second->sort() == Sort::Real) {
        pairSort = Sort::Complex;
    } else if (first->sort() == Sort::Complex && second->sort() == Sort::Complex) {
        pairSort = Sort::Quaternion;
    } else if (first->sort() == Sort::Quaternion && second->sort() == Sort::Quaternion) {
        pairSort = Sort::Octonion;
    } else if (first->sort() == Sort::Octonion && second->sort() == Sort::Octonion) {
        pairSort = Sort::Sedenion;
    }
    
    return intern(std::unique_ptr<Term>(new Term(
        nextId(), TermKind::Pair, pairSort, "", 
        std::vector<const Term*>{first, second}
    )));
}

const Term* TermFactory::apply(const std::string& funcName,
                               std::vector<const Term*> args,
                               Sort resultSort) {
    return intern(std::unique_ptr<Term>(new Term(
        nextId(), TermKind::Application, resultSort, funcName, std::move(args)
    )));
}

const Term* TermFactory::add(const Term* a, const Term* b) {
    if (!a || !b) return nullptr;
    
    Sort resultSort = (a->sort() == b->sort()) ? a->sort() : Sort::Generic;
    return apply("+", {a, b}, resultSort);
}

const Term* TermFactory::mul(const Term* a, const Term* b) {
    if (!a || !b) return nullptr;
    
    Sort resultSort = (a->sort() == b->sort()) ? a->sort() : Sort::Generic;
    return apply("*", {a, b}, resultSort);
}

const Term* TermFactory::conj(const Term* a) {
    if (!a) return nullptr;
    return apply("conj", {a}, a->sort());
}

const Term* TermFactory::neg(const Term* a) {
    if (!a) return nullptr;
    return apply("neg", {a}, a->sort());
}

const Term* TermFactory::inv(const Term* a) {
    if (!a) return nullptr;
    return apply("inv", {a}, a->sort());
}

const Term* TermFactory::norm(const Term* a) {
    if (!a) return nullptr;
    return apply("norm", {a}, Sort::Real);
}

const Term* TermFactory::scalarPart(const Term* a) {
    if (!a) return nullptr;
    return apply("Scal", {a}, Sort::Real);
}

const Term* TermFactory::align(const Term* U, const Term* q) {
    if (!U || !q) return nullptr;
    // Align_U(q) = Scal(U*  q)
    return apply("Align", {U, q}, Sort::Real);
}

const Term* TermFactory::phaseTransport(const Term* phi, const Term* U) {
    if (!phi || !U) return nullptr;
    // _{j+1} = _j  U_j
    return apply("PhaseTransport", {phi, U}, phi->sort());
}

const Term* TermFactory::fibStep(const Term* z) {
    if (!z) return nullptr;
    // R(z) = 1 + 1/z - the Fibonacci recursion step
    // Fixed point is 
    return apply("FibStep", {z}, z->sort());
}

} // namespace core
} // namespace autodiscover
