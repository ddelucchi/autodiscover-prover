/**
 * @file Type.hpp
 * @brief Type system for the Cayley-Dickson algebra tower
 * 
 * Mathematical Foundation:
 * - A =  (reals)
 * - A =  (complex numbers)
 * - A =  (quaternions)  
 * - A =  (octonions)
 * - A =  (sedenions)
 * - A_{n+1} = A_n  A_n with (a,b)(c,d) = (ac - d*b, da + bc*)
 */

#ifndef AUTODISCOVER_CORE_TYPE_HPP
#define AUTODISCOVER_CORE_TYPE_HPP

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <cstdint>

namespace autodiscover {
namespace core {

/**
 * @brief Type kind in the algebra tower
 */
enum class TypeKind : uint8_t {
    BaseSort,       // A_n for some n  0
    FunctionType,   // T  T
    PairType,       // T  T (Cayley-Dickson product)
    PolymorphicVar, // Type variable for generics
};

/**
 * @brief Cayley-Dickson level (dimension = 2^level)
 */
struct CDLevel {
    uint8_t level;
    
    static constexpr uint8_t REAL = 0;       // 2^0 = 1-dim
    static constexpr uint8_t COMPLEX = 1;    // 2^1 = 2-dim
    static constexpr uint8_t QUATERNION = 2; // 2^2 = 4-dim
    static constexpr uint8_t OCTONION = 3;   // 2^3 = 8-dim
    static constexpr uint8_t SEDENION = 4;   // 2^4 = 16-dim
    
    constexpr CDLevel(uint8_t l = 0) : level(l) {}
    
    [[nodiscard]] constexpr uint32_t dimension() const { return 1u << level; }
    [[nodiscard]] constexpr bool isAssociative() const { return level <= QUATERNION; }
    [[nodiscard]] constexpr bool isAlternative() const { return level <= OCTONION; }
    [[nodiscard]] constexpr bool isDivision() const { return level <= OCTONION; }
    [[nodiscard]] constexpr bool isCommutative() const { return level <= COMPLEX; }
    
    [[nodiscard]] std::string name() const {
        switch (level) {
            case REAL: return "";
            case COMPLEX: return "";
            case QUATERNION: return "";
            case OCTONION: return "";
            case SEDENION: return "";
            default: return "A" + std::to_string(level);
        }
    }
    
    [[nodiscard]] constexpr CDLevel next() const { return CDLevel(level + 1); }
    [[nodiscard]] constexpr CDLevel prev() const { return level > 0 ? CDLevel(level - 1) : CDLevel(0); }
    
    constexpr bool operator==(const CDLevel& other) const { return level == other.level; }
    constexpr bool operator!=(const CDLevel& other) const { return level != other.level; }
    constexpr bool operator<(const CDLevel& other) const { return level < other.level; }
    constexpr bool operator<=(const CDLevel& other) const { return level <= other.level; }
};

class Type;
using TypePtr = std::shared_ptr<const Type>;

/**
 * @brief Type representation in the algebra system
 */
class Type {
public:
    virtual ~Type() = default;
    
    [[nodiscard]] virtual TypeKind kind() const = 0;
    [[nodiscard]] virtual std::string toString() const = 0;
    [[nodiscard]] virtual bool equals(const Type& other) const = 0;
    [[nodiscard]] virtual size_t hash() const = 0;
    
    // Convenience checks
    [[nodiscard]] bool isBaseSort() const { return kind() == TypeKind::BaseSort; }
    [[nodiscard]] bool isFunctionType() const { return kind() == TypeKind::FunctionType; }
    [[nodiscard]] bool isPairType() const { return kind() == TypeKind::PairType; }
    [[nodiscard]] bool isPolymorphic() const { return kind() == TypeKind::PolymorphicVar; }
};

/**
 * @brief Base sort type (A_n in the Cayley-Dickson tower)
 */
class BaseSort : public Type {
public:
    explicit BaseSort(CDLevel lvl) : level_(lvl) {}
    
    [[nodiscard]] TypeKind kind() const override { return TypeKind::BaseSort; }
    [[nodiscard]] CDLevel level() const { return level_; }
    
    [[nodiscard]] std::string toString() const override {
        return level_.name();
    }
    
    [[nodiscard]] bool equals(const Type& other) const override {
        if (other.kind() != TypeKind::BaseSort) return false;
        return level_ == static_cast<const BaseSort&>(other).level_;
    }
    
    [[nodiscard]] size_t hash() const override {
        return std::hash<uint8_t>{}(level_.level);
    }

private:
    CDLevel level_;
};

/**
 * @brief Function type T  T
 */
class FunctionType : public Type {
public:
    FunctionType(TypePtr domain, TypePtr codomain)
        : domain_(std::move(domain)), codomain_(std::move(codomain)) {}
    
    [[nodiscard]] TypeKind kind() const override { return TypeKind::FunctionType; }
    [[nodiscard]] TypePtr domain() const { return domain_; }
    [[nodiscard]] TypePtr codomain() const { return codomain_; }
    
    [[nodiscard]] std::string toString() const override {
        return "(" + domain_->toString() + "  " + codomain_->toString() + ")";
    }
    
    [[nodiscard]] bool equals(const Type& other) const override {
        if (other.kind() != TypeKind::FunctionType) return false;
        const auto& ft = static_cast<const FunctionType&>(other);
        return domain_->equals(*ft.domain_) && codomain_->equals(*ft.codomain_);
    }
    
    [[nodiscard]] size_t hash() const override {
        size_t h = domain_->hash();
        h ^= codomain_->hash() + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }

private:
    TypePtr domain_;
    TypePtr codomain_;
};

/**
 * @brief Pair type T  T (for Cayley-Dickson construction)
 */
class PairType : public Type {
public:
    PairType(TypePtr first, TypePtr second)
        : first_(std::move(first)), second_(std::move(second)) {}
    
    [[nodiscard]] TypeKind kind() const override { return TypeKind::PairType; }
    [[nodiscard]] TypePtr first() const { return first_; }
    [[nodiscard]] TypePtr second() const { return second_; }
    
    /**
     * @brief Get the Cayley-Dickson result type for this pair
     * If both components are A_n, the pair represents A_{n+1}
     */
    [[nodiscard]] std::optional<CDLevel> cdLevel() const {
        if (first_->kind() == TypeKind::BaseSort && 
            second_->kind() == TypeKind::BaseSort) {
            const auto& f = static_cast<const BaseSort&>(*first_);
            const auto& s = static_cast<const BaseSort&>(*second_);
            if (f.level() == s.level()) {
                return f.level().next();
            }
        }
        return std::nullopt;
    }
    
    [[nodiscard]] std::string toString() const override {
        return "(" + first_->toString() + "  " + second_->toString() + ")";
    }
    
    [[nodiscard]] bool equals(const Type& other) const override {
        if (other.kind() != TypeKind::PairType) return false;
        const auto& pt = static_cast<const PairType&>(other);
        return first_->equals(*pt.first_) && second_->equals(*pt.second_);
    }
    
    [[nodiscard]] size_t hash() const override {
        size_t h = first_->hash();
        h ^= second_->hash() + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= 0xDEADBEEF; // Distinguish from FunctionType
        return h;
    }

private:
    TypePtr first_;
    TypePtr second_;
};

/**
 * @brief Polymorphic type variable
 */
class PolymorphicVar : public Type {
public:
    explicit PolymorphicVar(uint32_t id) : id_(id) {}
    
    [[nodiscard]] TypeKind kind() const override { return TypeKind::PolymorphicVar; }
    [[nodiscard]] uint32_t id() const { return id_; }
    
    [[nodiscard]] std::string toString() const override {
        return "" + std::to_string(id_);
    }
    
    [[nodiscard]] bool equals(const Type& other) const override {
        if (other.kind() != TypeKind::PolymorphicVar) return false;
        return id_ == static_cast<const PolymorphicVar&>(other).id_;
    }
    
    [[nodiscard]] size_t hash() const override {
        return std::hash<uint32_t>{}(id_) ^ 0xCAFEBABE;
    }

private:
    uint32_t id_;
};

/**
 * @brief Factory for creating common types
 */
class TypeFactory {
public:
    // Singleton base sorts
    static TypePtr Real() {
        static auto t = std::make_shared<BaseSort>(CDLevel::REAL);
        return t;
    }
    
    static TypePtr Complex() {
        static auto t = std::make_shared<BaseSort>(CDLevel::COMPLEX);
        return t;
    }
    
    static TypePtr Quaternion() {
        static auto t = std::make_shared<BaseSort>(CDLevel::QUATERNION);
        return t;
    }
    
    static TypePtr Octonion() {
        static auto t = std::make_shared<BaseSort>(CDLevel::OCTONION);
        return t;
    }
    
    static TypePtr Sedenion() {
        static auto t = std::make_shared<BaseSort>(CDLevel::SEDENION);
        return t;
    }
    
    static TypePtr AtLevel(CDLevel level) {
        return std::make_shared<BaseSort>(level);
    }
    
    static TypePtr Function(TypePtr domain, TypePtr codomain) {
        return std::make_shared<FunctionType>(std::move(domain), std::move(codomain));
    }
    
    static TypePtr Pair(TypePtr first, TypePtr second) {
        return std::make_shared<PairType>(std::move(first), std::move(second));
    }
    
    static TypePtr Var(uint32_t id) {
        return std::make_shared<PolymorphicVar>(id);
    }
    
    /**
     * @brief Get the Cayley-Dickson pair type A_n  A_n representing A_{n+1}
     */
    static TypePtr CDPair(CDLevel level) {
        auto base = std::make_shared<BaseSort>(level);
        return std::make_shared<PairType>(base, base);
    }
};

} // namespace core
} // namespace autodiscover

#endif // AUTODISCOVER_CORE_TYPE_HPP
