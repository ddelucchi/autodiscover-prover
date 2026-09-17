#ifndef AUTODISCOVER_CORE_DIMENSION_HPP
#define AUTODISCOVER_CORE_DIMENSION_HPP

/**
 * @file Dimension.hpp
 * @brief Physical dimension vectors for typed term algebra
 * 
 * @note STATUS: CURRENTLY UNUSED  This module is not #included by any other
 * source file in the project. It is retained as a designed-but-not-yet-integrated
 * extension point for dimensioned type checking (EquivalenceLayer::DIM).
 * No DimTermPass implementation exists in NFEngine.hpp yet.
 * 
 * Mathematical Foundation:
 * ========================
 * 
 * Dimensions form a free abelian group ^d where d = number of base dimensions.
 * Standard SI base dimensions: Length, Mass, Time, Current, Temperature, Amount, Luminosity
 * 
 * A dimension vector D = (d, d, ..., d_k) represents:
 *   [D] = L^{d}  M^{d}  T^{d}  I^{d}  ^{d}  N^{d}  J^{d}
 * 
 * Algebra of Dimensions:
 * ----------------------
 * - [ab] = [a] + [b]  (multiplication adds dimension vectors)
 * - [a/b] = [a] - [b]  (division subtracts dimension vectors)
 * - [a^n] = n  [a]    (exponentiation scales dimension vector)
 * - [a+b] requires [a] = [b] (addition only defined for same dimensions)
 * 
 * Units:
 * ------
 * A Unit U = (scale, dimension_vector) where:
 *   - scale   is the conversion factor to base units
 *   - dimension_vector  ^d specifies the physical dimension
 * 
 * Example: 
 *   meter = (1.0, [1,0,0,0,0,0,0])
 *   kilometer = (1000.0, [1,0,0,0,0,0,0])
 *   second = (1.0, [0,0,1,0,0,0,0])
 *   velocity = [1,0,-1,0,0,0,0]  (m/s = LT)
 * 
 * Type System Integration:
 * ------------------------
 * Every term t has:
 *   - Algebraic sort: Sort::Real, Sort::Complex, etc.
 *   - Physical dimension: DimensionVector
 *   - Optional unit annotation: Unit
 * 
 * Type checking enforces:
 *   - add(a,b): [a] = [b]
 *   - mul(a,b): [result] = [a] + [b]
 *   - pow(a,n): [result] = n  [a]
 *   - functions f:  require dimensionless argument
 * 
 * @author AutoDiscover Prover
 * @date 2024
 */

#include <array>
#include <cstdint>
#include <string>
#include <sstream>
#include <cmath>
#include <stdexcept>
#include <functional>
#include <unordered_map>
#include <optional>

namespace autodiscover {
namespace core {

// ===========================================================================
// CONSTANTS
// ===========================================================================

/// Number of base dimensions (SI base units + extension slots)
constexpr size_t NUM_BASE_DIMENSIONS = 10;

/// Base dimension indices (SI + extensions)
enum class BaseDimension : size_t {
    LENGTH      = 0,   // L - meter
    MASS        = 1,   // M - kilogram
    TIME        = 2,   // T - second  
    CURRENT     = 3,   // I - ampere
    TEMPERATURE = 4,   //  - kelvin
    AMOUNT      = 5,   // N - mole
    LUMINOSITY  = 6,   // J - candela
    // Extensions for abstract algebra
    ANGLE       = 7,   // rad - for dimensionful angles
    INFO        = 8,   // bit - information dimension
    CURRENCY    = 9    // $ - economic dimension
};

// ===========================================================================
// DIMENSION VECTOR
// ===========================================================================

/**
 * @brief Dimension vector in ^d
 * 
 * Represents a physical dimension as a vector of integer exponents.
 * Uses int8_t for compact storage (exponents rarely exceed 10).
 */
class DimensionVector {
public:
    using Exponent = int8_t;
    using Storage = std::array<Exponent, NUM_BASE_DIMENSIONS>;
    
private:
    Storage exponents_{};
    
public:
    // -----------------------------------------------------------------------
    // CONSTRUCTORS
    // -----------------------------------------------------------------------
    
    /// Dimensionless (all zeros)
    constexpr DimensionVector() noexcept : exponents_{} {}
    
    /// From initializer list
    constexpr DimensionVector(std::initializer_list<Exponent> init) : exponents_{} {
        size_t i = 0;
        for (auto e : init) {
            if (i < NUM_BASE_DIMENSIONS) {
                exponents_[i++] = e;
            }
        }
    }
    
    /// From array
    constexpr explicit DimensionVector(const Storage& arr) : exponents_(arr) {}
    
    /// Single base dimension with exponent
    static constexpr DimensionVector base(BaseDimension d, Exponent exp = 1) {
        DimensionVector v;
        v.exponents_[static_cast<size_t>(d)] = exp;
        return v;
    }
    
    // -----------------------------------------------------------------------
    // NAMED CONSTRUCTORS FOR COMMON DIMENSIONS
    // -----------------------------------------------------------------------
    
    static constexpr DimensionVector length()      { return base(BaseDimension::LENGTH); }
    static constexpr DimensionVector mass()        { return base(BaseDimension::MASS); }
    static constexpr DimensionVector time()        { return base(BaseDimension::TIME); }
    static constexpr DimensionVector current()     { return base(BaseDimension::CURRENT); }
    static constexpr DimensionVector temperature() { return base(BaseDimension::TEMPERATURE); }
    static constexpr DimensionVector amount()      { return base(BaseDimension::AMOUNT); }
    static constexpr DimensionVector luminosity()  { return base(BaseDimension::LUMINOSITY); }
    static constexpr DimensionVector angle()       { return base(BaseDimension::ANGLE); }
    static constexpr DimensionVector info()        { return base(BaseDimension::INFO); }
    static constexpr DimensionVector currency()    { return base(BaseDimension::CURRENCY); }
    
    // Derived dimensions
    static constexpr DimensionVector velocity()    { return length() - time(); }
    static constexpr DimensionVector acceleration(){ return velocity() - time(); }
    static constexpr DimensionVector force()       { return mass() + acceleration(); }
    static constexpr DimensionVector energy()      { return force() + length(); }
    static constexpr DimensionVector power()       { return energy() - time(); }
    static constexpr DimensionVector area()        { return length().scale(2); }
    static constexpr DimensionVector volume()      { return length().scale(3); }
    static constexpr DimensionVector density()     { return mass() - volume(); }
    static constexpr DimensionVector frequency()   { return DimensionVector() - time(); }
    
    // -----------------------------------------------------------------------
    // ACCESSORS
    // -----------------------------------------------------------------------
    
    constexpr Exponent operator[](size_t i) const { return exponents_[i]; }
    constexpr Exponent& operator[](size_t i) { return exponents_[i]; }
    
    constexpr Exponent operator[](BaseDimension d) const { 
        return exponents_[static_cast<size_t>(d)]; 
    }
    constexpr Exponent& operator[](BaseDimension d) { 
        return exponents_[static_cast<size_t>(d)]; 
    }
    
    constexpr const Storage& data() const { return exponents_; }
    
    /// Is this the dimensionless vector (all zeros)?
    [[nodiscard]] constexpr bool isDimensionless() const {
        for (auto e : exponents_) {
            if (e != 0) return false;
        }
        return true;
    }
    
    // -----------------------------------------------------------------------
    // ARITHMETIC (GROUP OPERATIONS)
    // -----------------------------------------------------------------------
    
    /// Addition (for multiplication of quantities)
    constexpr DimensionVector operator+(const DimensionVector& other) const {
        DimensionVector result;
        for (size_t i = 0; i < NUM_BASE_DIMENSIONS; ++i) {
            result.exponents_[i] = exponents_[i] + other.exponents_[i];
        }
        return result;
    }
    
    /// Subtraction (for division of quantities)
    constexpr DimensionVector operator-(const DimensionVector& other) const {
        DimensionVector result;
        for (size_t i = 0; i < NUM_BASE_DIMENSIONS; ++i) {
            result.exponents_[i] = exponents_[i] - other.exponents_[i];
        }
        return result;
    }
    
    /// Negation (for reciprocal)
    constexpr DimensionVector operator-() const {
        DimensionVector result;
        for (size_t i = 0; i < NUM_BASE_DIMENSIONS; ++i) {
            result.exponents_[i] = -exponents_[i];
        }
        return result;
    }
    
    /// Scalar multiplication (for power)
    constexpr DimensionVector scale(int n) const {
        DimensionVector result;
        for (size_t i = 0; i < NUM_BASE_DIMENSIONS; ++i) {
            result.exponents_[i] = static_cast<Exponent>(exponents_[i] * n);
        }
        return result;
    }
    
    constexpr DimensionVector operator*(int n) const { return scale(n); }
    
    // -----------------------------------------------------------------------
    // COMPARISON
    // -----------------------------------------------------------------------
    
    constexpr bool operator==(const DimensionVector& other) const {
        return exponents_ == other.exponents_;
    }
    
    constexpr bool operator!=(const DimensionVector& other) const {
        return !(*this == other);
    }
    
    /// Lexicographic ordering for canonical form
    constexpr bool operator<(const DimensionVector& other) const {
        return exponents_ < other.exponents_;
    }
    
    // -----------------------------------------------------------------------
    // SERIALIZATION
    // -----------------------------------------------------------------------
    
    /// Convert to string representation
    [[nodiscard]] std::string toString() const {
        static const char* names[] = {"L", "M", "T", "I", "", "N", "J", "", "bit", "$"};
        
        if (isDimensionless()) {
            return "1";
        }
        
        std::ostringstream oss;
        bool first = true;
        for (size_t i = 0; i < NUM_BASE_DIMENSIONS; ++i) {
            if (exponents_[i] != 0) {
                if (!first) oss << "";
                oss << names[i];
                if (exponents_[i] != 1) {
                    oss << "^" << static_cast<int>(exponents_[i]);
                }
                first = false;
            }
        }
        return oss.str();
    }
    
    /// Hash for use in containers
    [[nodiscard]] size_t hash() const {
        size_t h = 0;
        for (size_t i = 0; i < NUM_BASE_DIMENSIONS; ++i) {
            h ^= std::hash<Exponent>{}(exponents_[i]) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};

/// Free function for scalar mult
constexpr DimensionVector operator*(int n, const DimensionVector& d) {
    return d.scale(n);
}

// Hash specialization
} // namespace core
} // namespace autodiscover

namespace std {
    template<>
    struct hash<autodiscover::core::DimensionVector> {
        size_t operator()(const autodiscover::core::DimensionVector& d) const {
            return d.hash();
        }
    };
}

namespace autodiscover {
namespace core {

// ===========================================================================
// UNIT
// ===========================================================================

/**
 * @brief A physical unit with scale factor and dimension
 * 
 * Unit U = (scale, dimension) where scale converts to base SI units.
 */
class Unit {
private:
    double scale_;
    DimensionVector dimension_;
    std::string name_;
    std::string symbol_;
    
public:
    // -----------------------------------------------------------------------
    // CONSTRUCTORS
    // -----------------------------------------------------------------------
    
    /// Default dimensionless unit
    Unit() : scale_(1.0), dimension_(), name_("1"), symbol_("1") {}
    
    /// Full constructor
    Unit(double scale, DimensionVector dim, std::string name = "", std::string symbol = "")
        : scale_(scale), dimension_(std::move(dim)), 
          name_(std::move(name)), symbol_(std::move(symbol)) {}
    
    /// Named base unit
    static Unit baseUnit(BaseDimension d, std::string name, std::string symbol) {
        return Unit(1.0, DimensionVector::base(d), std::move(name), std::move(symbol));
    }
    
    // -----------------------------------------------------------------------
    // SI BASE UNITS
    // -----------------------------------------------------------------------
    
    static const Unit& meter()    { static Unit u = baseUnit(BaseDimension::LENGTH, "meter", "m"); return u; }
    static const Unit& kilogram() { static Unit u = baseUnit(BaseDimension::MASS, "kilogram", "kg"); return u; }
    static const Unit& second()   { static Unit u = baseUnit(BaseDimension::TIME, "second", "s"); return u; }
    static const Unit& ampere()   { static Unit u = baseUnit(BaseDimension::CURRENT, "ampere", "A"); return u; }
    static const Unit& kelvin()   { static Unit u = baseUnit(BaseDimension::TEMPERATURE, "kelvin", "K"); return u; }
    static const Unit& mole()     { static Unit u = baseUnit(BaseDimension::AMOUNT, "mole", "mol"); return u; }
    static const Unit& candela()  { static Unit u = baseUnit(BaseDimension::LUMINOSITY, "candela", "cd"); return u; }
    
    // -----------------------------------------------------------------------
    // DERIVED/SCALED UNITS
    // -----------------------------------------------------------------------
    
    static const Unit& kilometer() { 
        static Unit u(1000.0, DimensionVector::length(), "kilometer", "km"); 
        return u; 
    }
    static const Unit& gram() { 
        static Unit u(0.001, DimensionVector::mass(), "gram", "g"); 
        return u; 
    }
    static const Unit& minute() { 
        static Unit u(60.0, DimensionVector::time(), "minute", "min"); 
        return u; 
    }
    static const Unit& hour() { 
        static Unit u(3600.0, DimensionVector::time(), "hour", "h"); 
        return u; 
    }
    static const Unit& newton() {
        static Unit u(1.0, DimensionVector::force(), "newton", "N");
        return u;
    }
    static const Unit& joule() {
        static Unit u(1.0, DimensionVector::energy(), "joule", "J");
        return u;
    }
    static const Unit& watt() {
        static Unit u(1.0, DimensionVector::power(), "watt", "W");
        return u;
    }
    static const Unit& hertz() {
        static Unit u(1.0, DimensionVector::frequency(), "hertz", "Hz");
        return u;
    }
    
    // -----------------------------------------------------------------------
    // ACCESSORS
    // -----------------------------------------------------------------------
    
    [[nodiscard]] double scale() const { return scale_; }
    [[nodiscard]] const DimensionVector& dimension() const { return dimension_; }
    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] const std::string& symbol() const { return symbol_; }
    
    [[nodiscard]] bool isDimensionless() const { return dimension_.isDimensionless(); }
    
    // -----------------------------------------------------------------------
    // ARITHMETIC
    // -----------------------------------------------------------------------
    
    /// Unit multiplication
    [[nodiscard]] Unit operator*(const Unit& other) const {
        return Unit(
            scale_ * other.scale_,
            dimension_ + other.dimension_,
            name_ + "" + other.name_,
            symbol_ + "" + other.symbol_
        );
    }
    
    /// Unit division
    [[nodiscard]] Unit operator/(const Unit& other) const {
        return Unit(
            scale_ / other.scale_,
            dimension_ - other.dimension_,
            name_ + "/" + other.name_,
            symbol_ + "/" + other.symbol_
        );
    }
    
    /// Unit power
    [[nodiscard]] Unit pow(int n) const {
        return Unit(
            std::pow(scale_, n),
            dimension_.scale(n),
            name_ + "^" + std::to_string(n),
            symbol_ + "^" + std::to_string(n)
        );
    }
    
    /// Reciprocal
    [[nodiscard]] Unit inverse() const {
        return Unit(
            1.0 / scale_,
            -dimension_,
            "1/" + name_,
            "1/" + symbol_
        );
    }
    
    // -----------------------------------------------------------------------
    // CONVERSION
    // -----------------------------------------------------------------------
    
    /// Convert a value from this unit to base SI units
    [[nodiscard]] double toBase(double value) const {
        return value * scale_;
    }
    
    /// Convert a value from base SI units to this unit  
    [[nodiscard]] double fromBase(double value) const {
        return value / scale_;
    }
    
    /// Convert between units (must have same dimension)
    [[nodiscard]] double convertTo(double value, const Unit& target) const {
        if (dimension_ != target.dimension_) {
            throw std::invalid_argument("Cannot convert between incompatible dimensions: " 
                + dimension_.toString() + " vs " + target.dimension_.toString());
        }
        return target.fromBase(toBase(value));
    }
    
    // -----------------------------------------------------------------------
    // COMPARISON
    // -----------------------------------------------------------------------
    
    [[nodiscard]] bool compatibleWith(const Unit& other) const {
        return dimension_ == other.dimension_;
    }
    
    [[nodiscard]] bool operator==(const Unit& other) const {
        return scale_ == other.scale_ && dimension_ == other.dimension_;
    }
    
    [[nodiscard]] std::string toString() const {
        if (scale_ == 1.0) {
            return symbol_.empty() ? dimension_.toString() : symbol_;
        }
        return std::to_string(scale_) + "" + dimension_.toString();
    }
};

// ===========================================================================
// DIMENSIONED TYPE
// ===========================================================================

/**
 * @brief A type with physical dimension
 * 
 * Extends the base Sort with dimension information for dimensional analysis.
 */
class DimensionedType {
public:
    enum class BaseType {
        REAL,
        COMPLEX,
        QUATERNION,
        OCTONION,
        SEDENION,
        INTEGER,
        RATIONAL,
        BOOLEAN,
        UNKNOWN
    };
    
private:
    BaseType baseType_;
    DimensionVector dimension_;
    std::optional<Unit> unit_;
    
public:
    // -----------------------------------------------------------------------
    // CONSTRUCTORS
    // -----------------------------------------------------------------------
    
    DimensionedType() 
        : baseType_(BaseType::REAL), dimension_(), unit_(std::nullopt) {}
    
    explicit DimensionedType(BaseType bt)
        : baseType_(bt), dimension_(), unit_(std::nullopt) {}
    
    DimensionedType(BaseType bt, DimensionVector dim)
        : baseType_(bt), dimension_(std::move(dim)), unit_(std::nullopt) {}
    
    DimensionedType(BaseType bt, Unit u)
        : baseType_(bt), dimension_(u.dimension()), unit_(std::move(u)) {}
    
    // Named constructors for common types
    static DimensionedType real() { return DimensionedType(BaseType::REAL); }
    static DimensionedType complex() { return DimensionedType(BaseType::COMPLEX); }
    static DimensionedType quaternion() { return DimensionedType(BaseType::QUATERNION); }
    static DimensionedType octonion() { return DimensionedType(BaseType::OCTONION); }
    static DimensionedType boolean() { return DimensionedType(BaseType::BOOLEAN); }
    static DimensionedType integer() { return DimensionedType(BaseType::INTEGER); }
    
    static DimensionedType length() { 
        return DimensionedType(BaseType::REAL, DimensionVector::length()); 
    }
    static DimensionedType mass() { 
        return DimensionedType(BaseType::REAL, DimensionVector::mass()); 
    }
    static DimensionedType time() { 
        return DimensionedType(BaseType::REAL, DimensionVector::time()); 
    }
    static DimensionedType velocity() { 
        return DimensionedType(BaseType::REAL, DimensionVector::velocity()); 
    }
    static DimensionedType energy() { 
        return DimensionedType(BaseType::REAL, DimensionVector::energy()); 
    }
    
    // -----------------------------------------------------------------------
    // ACCESSORS
    // -----------------------------------------------------------------------
    
    [[nodiscard]] BaseType baseType() const { return baseType_; }
    [[nodiscard]] const DimensionVector& dimension() const { return dimension_; }
    [[nodiscard]] const std::optional<Unit>& unit() const { return unit_; }
    
    [[nodiscard]] bool isDimensionless() const { return dimension_.isDimensionless(); }
    [[nodiscard]] bool hasUnit() const { return unit_.has_value(); }
    
    // -----------------------------------------------------------------------
    // TYPE CHECKING
    // -----------------------------------------------------------------------
    
    /// Can these types be added? (requires same dimension)
    [[nodiscard]] bool canAdd(const DimensionedType& other) const {
        return dimension_ == other.dimension_;
    }
    
    /// Result type of multiplication
    [[nodiscard]] DimensionedType multiply(const DimensionedType& other) const {
        // For hypercomplex, use the higher level
        BaseType resultBase = static_cast<int>(baseType_) >= static_cast<int>(other.baseType_) 
            ? baseType_ : other.baseType_;
        return DimensionedType(resultBase, dimension_ + other.dimension_);
    }
    
    /// Result type of division
    [[nodiscard]] DimensionedType divide(const DimensionedType& other) const {
        BaseType resultBase = static_cast<int>(baseType_) >= static_cast<int>(other.baseType_) 
            ? baseType_ : other.baseType_;
        return DimensionedType(resultBase, dimension_ - other.dimension_);
    }
    
    /// Result type of power (requires integer exponent)
    [[nodiscard]] DimensionedType power(int n) const {
        return DimensionedType(baseType_, dimension_.scale(n));
    }
    
    /// Result type of sqrt (requires even exponents)
    [[nodiscard]] std::optional<DimensionedType> sqrt() const {
        // Check all exponents are even
        for (size_t i = 0; i < NUM_BASE_DIMENSIONS; ++i) {
            if (dimension_[i] % 2 != 0) {
                return std::nullopt; // Cannot take sqrt of odd-power dimension
            }
        }
        DimensionVector halfDim;
        for (size_t i = 0; i < NUM_BASE_DIMENSIONS; ++i) {
            halfDim[i] = dimension_[i] / 2;
        }
        return DimensionedType(baseType_, halfDim);
    }
    
    // -----------------------------------------------------------------------
    // COMPARISON
    // -----------------------------------------------------------------------
    
    [[nodiscard]] bool operator==(const DimensionedType& other) const {
        return baseType_ == other.baseType_ && dimension_ == other.dimension_;
    }
    
    [[nodiscard]] bool operator!=(const DimensionedType& other) const {
        return !(*this == other);
    }
    
    [[nodiscard]] bool dimensionCompatible(const DimensionedType& other) const {
        return dimension_ == other.dimension_;
    }
    
    // -----------------------------------------------------------------------
    // SERIALIZATION
    // -----------------------------------------------------------------------
    
    [[nodiscard]] std::string toString() const {
        static const char* baseNames[] = {
            "", "", "", "", "", "", "", "", "?"
        };
        std::ostringstream oss;
        oss << baseNames[static_cast<int>(baseType_)];
        if (!dimension_.isDimensionless()) {
            oss << "[" << dimension_.toString() << "]";
        }
        return oss.str();
    }
    
    [[nodiscard]] size_t hash() const {
        size_t h = std::hash<int>{}(static_cast<int>(baseType_));
        h ^= dimension_.hash() + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

// ===========================================================================
// DIMENSION CHECKER
// ===========================================================================

/**
 * @brief Static dimension analysis and type checking
 */
class DimensionChecker {
public:
    enum class CheckResult {
        OK,
        DIMENSION_MISMATCH,
        REQUIRES_DIMENSIONLESS,
        INVALID_SQRT_DIMENSION,
        TYPE_ERROR
    };
    
    struct CheckError {
        CheckResult result;
        std::string message;
        DimensionVector expected;
        DimensionVector actual;
        
        operator bool() const { return result == CheckResult::OK; }
    };
    
    /// Check addition: a + b requires [a] = [b]
    static CheckError checkAdd(const DimensionedType& a, const DimensionedType& b) {
        if (a.dimension() != b.dimension()) {
            return {
                CheckResult::DIMENSION_MISMATCH,
                "Addition requires matching dimensions: " + 
                    a.dimension().toString() + " vs " + b.dimension().toString(),
                a.dimension(),
                b.dimension()
            };
        }
        return {CheckResult::OK, "", {}, {}};
    }
    
    /// Check functions like sin, cos, exp: require dimensionless argument
    static CheckError checkDimensionless(const DimensionedType& a, const std::string& funcName) {
        if (!a.isDimensionless()) {
            return {
                CheckResult::REQUIRES_DIMENSIONLESS,
                funcName + " requires dimensionless argument, got: " + a.dimension().toString(),
                DimensionVector(),
                a.dimension()
            };
        }
        return {CheckResult::OK, "", {}, {}};
    }
    
    /// Check sqrt: requires all exponents even
    static CheckError checkSqrt(const DimensionedType& a) {
        for (size_t i = 0; i < NUM_BASE_DIMENSIONS; ++i) {
            if (a.dimension()[i] % 2 != 0) {
                return {
                    CheckResult::INVALID_SQRT_DIMENSION,
                    "sqrt requires even dimension exponents: " + a.dimension().toString(),
                    DimensionVector(),
                    a.dimension()
                };
            }
        }
        return {CheckResult::OK, "", {}, {}};
    }
};

// ===========================================================================
// UNIT REGISTRY
// ===========================================================================

/**
 * @brief Registry of known units for parsing and lookup
 */
class UnitRegistry {
private:
    std::unordered_map<std::string, Unit> bySymbol_;
    std::unordered_map<std::string, Unit> byName_;
    
public:
    UnitRegistry() {
        // Register SI base units
        registerUnit(Unit::meter());
        registerUnit(Unit::kilogram());
        registerUnit(Unit::second());
        registerUnit(Unit::ampere());
        registerUnit(Unit::kelvin());
        registerUnit(Unit::mole());
        registerUnit(Unit::candela());
        
        // Register common derived/scaled units
        registerUnit(Unit::kilometer());
        registerUnit(Unit::gram());
        registerUnit(Unit::minute());
        registerUnit(Unit::hour());
        registerUnit(Unit::newton());
        registerUnit(Unit::joule());
        registerUnit(Unit::watt());
        registerUnit(Unit::hertz());
    }
    
    void registerUnit(const Unit& u) {
        if (!u.symbol().empty()) bySymbol_[u.symbol()] = u;
        if (!u.name().empty()) byName_[u.name()] = u;
    }
    
    [[nodiscard]] std::optional<Unit> findBySymbol(const std::string& symbol) const {
        auto it = bySymbol_.find(symbol);
        if (it != bySymbol_.end()) return it->second;
        return std::nullopt;
    }
    
    [[nodiscard]] std::optional<Unit> findByName(const std::string& name) const {
        auto it = byName_.find(name);
        if (it != byName_.end()) return it->second;
        return std::nullopt;
    }
    
    [[nodiscard]] std::optional<Unit> find(const std::string& key) const {
        auto u = findBySymbol(key);
        if (u) return u;
        return findByName(key);
    }
    
    static UnitRegistry& global() {
        static UnitRegistry instance;
        return instance;
    }
};

} // namespace core
} // namespace autodiscover

#endif // AUTODISCOVER_CORE_DIMENSION_HPP
