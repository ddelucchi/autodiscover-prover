# ============================================================================
# Autodiscovery Equational Prover with SCOUT Integration
# ============================================================================
# 
# Build System: CMake 3.16+
# Language: C++17
#
# Based on: "The Field Equations of Ontologies"
#           (The Geometrization of Logic, all of Mathematics, and Thus their Physics)
#
# Mathematical Foundation:
#   - Cayley-Dickson algebra tower: ℝ → ℂ → ℍ → 𝕆 → 𝕊 → ...
#   - GOD (Groupoid-Orbit Determinizer) canonicalization
#   - Golden ratio φ fixed-point: φ² = φ + 1
#   - SCOUT phase transport: Align_U(q) = Scal(U* · q)
#
# ============================================================================

cmake_minimum_required(VERSION 3.16)

project(AutoDiscoverProver
    VERSION 0.1.0
    DESCRIPTION "Autodiscovery Equational Prover with SCOUT Integration"
    LANGUAGES CXX
)

# ============================================================================
# C++ Standard
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Build Options
# ============================================================================
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_DOCS "Build documentation" OFF)
option(ENABLE_SANITIZERS "Enable address/undefined sanitizers" OFF)
option(ENABLE_LTO "Enable Link-Time Optimization" OFF)

# ============================================================================
# Compiler Warnings
# ============================================================================
if(MSVC)
    add_compile_options(/W4 /permissive-)
    # Enable UTF-8 source and execution character sets
    add_compile_options(/utf-8)
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wno-unused-parameter
    )
endif()

# ============================================================================
# Sanitizers (Debug mode)
# ============================================================================
if(ENABLE_SANITIZERS AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# ============================================================================
# Link-Time Optimization
# ============================================================================
if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT lto_supported OUTPUT lto_error)
    if(lto_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "LTO not supported: ${lto_error}")
    endif()
endif()

# ============================================================================
# Source Files
# ============================================================================
set(CORE_SOURCES
    src/core/Term.cpp
    src/core/Type.cpp
    src/core/SymbolTable.cpp
)

set(CORE_HEADERS
    src/core/Term.hpp
    src/core/Type.hpp
    src/core/SymbolTable.hpp
    src/core/Constants.hpp
    src/core/Dimension.hpp      # Currently unused — retained as extension point
    src/core/Budget.hpp         # Shared budget/gas system for all loops
)

set(LOGIC_SOURCES
    src/logic/Equation.cpp
    src/logic/KnowledgeBase.cpp
    src/logic/MatcherUnifier.cpp
    src/logic/Normalizer.cpp
    src/logic/InferenceEngine.cpp
)

set(LOGIC_HEADERS
    src/logic/Equation.hpp
    src/logic/KnowledgeBase.hpp
    src/logic/MatcherUnifier.hpp
    src/logic/Normalizer.hpp
    src/logic/InferenceEngine.hpp
)

set(DOMAIN_SOURCES
    src/domain/Algebra.cpp
    src/domain/Scout.cpp
    src/domain/Closure.cpp
)

set(DOMAIN_HEADERS
    src/domain/Algebra.hpp
    src/domain/Scout.hpp
    src/domain/Closure.hpp
    src/domain/IntersticeEngine.hpp
    src/domain/UniversalContext.hpp
    src/domain/PadicBridge.hpp
    src/domain/DifferentialCalculus.hpp
    src/domain/TensorSpinor.hpp
    src/domain/PhysicsUniverse.hpp
    src/domain/IntersticeDeep.hpp
    src/domain/IntersticeOrbit.hpp
    src/domain/FieldEquation.hpp
)

set(PROOF_SOURCES
    src/proof/Proof.cpp
    src/proof/ProofChecker.cpp
    src/proof/CertificateExporter.cpp
)

set(PROOF_HEADERS
    src/proof/Proof.hpp
    src/proof/ProofChecker.hpp
    src/proof/CertificateExporter.hpp
)

# ============================================================================
# Canonical Fingerprinting Module (Header-Only)
# ============================================================================
set(RING_HEADERS
    src/ring/ZPhi.hpp
)

set(ENCODING_HEADERS
    src/encoding/Encoding.hpp
    src/encoding/Structural.hpp
)

set(FINGERPRINT_HEADERS
    src/fingerprint/Fingerprinter.hpp
    src/fingerprint/Semantic.hpp
)

set(STORE_HEADERS
    src/store/ProofStore.hpp
)

set(EGRAPH_HEADERS
    src/egraph/EGraph.hpp
)

set(CANON_HEADERS
    src/canon/Canonicalizer.hpp
    src/canon/Equivalence.hpp
    src/canon/NFEngine.hpp
    src/canon/CanonScalar.hpp
)

set(DISCOVERY_HEADERS
    src/discovery/ScalarDiscovery.hpp
    src/discovery/ScalarOracle.hpp
    src/discovery/UniversalDiscovery.hpp
)

# ============================================================================
# Core Library
# ============================================================================
add_library(autodiscover_core STATIC
    ${CORE_SOURCES}
    ${CORE_HEADERS}
    ${LOGIC_SOURCES}
    ${LOGIC_HEADERS}
    ${DOMAIN_SOURCES}
    ${DOMAIN_HEADERS}
    ${PROOF_SOURCES}
    ${PROOF_HEADERS}
    ${RING_HEADERS}
    ${ENCODING_HEADERS}
    ${FINGERPRINT_HEADERS}
    ${STORE_HEADERS}
    ${EGRAPH_HEADERS}
    ${CANON_HEADERS}
    ${DISCOVERY_HEADERS}
)

target_include_directories(autodiscover_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================================
# Main Executable
# ============================================================================
add_executable(autodiscover
    src/main.cpp
)

target_link_libraries(autodiscover PRIVATE
    autodiscover_core
)

# ============================================================================
# Tests
# ============================================================================
if(BUILD_TESTS)
    enable_testing()
    
    # Golden ratio tests
    add_executable(test_golden
        tests/test_golden.cpp
    )
    target_link_libraries(test_golden PRIVATE autodiscover_core)
    add_test(NAME GoldenRatioTests COMMAND test_golden)
    
    # Term tests
    add_executable(test_term
        tests/test_term.cpp
    )
    target_link_libraries(test_term PRIVATE autodiscover_core)
    add_test(NAME TermTests COMMAND test_term)
    
    # Normalizer tests
    add_executable(test_normalizer
        tests/test_normalizer.cpp
    )
    target_link_libraries(test_normalizer PRIVATE autodiscover_core)
    add_test(NAME NormalizerTests COMMAND test_normalizer)
    
    # CanonScalar tests
    add_executable(test_canon_scalar
        tests/test_canon_scalar.cpp
    )
    target_link_libraries(test_canon_scalar PRIVATE autodiscover_core)
    add_test(NAME CanonScalarTests COMMAND test_canon_scalar)
    
    # Property + roundtrip tests
    add_executable(test_properties
        tests/test_properties.cpp
    )
    target_link_libraries(test_properties PRIVATE autodiscover_core)
    add_test(NAME PropertyTests COMMAND test_properties)

    # Deep-dive module tests (certificate kernel, multi-ring, delta, zeckendorf, rule miner, saturator)
    add_executable(test_deep_modules
        tests/test_deep_modules.cpp
    )
    target_link_libraries(test_deep_modules PRIVATE autodiscover_core)
    add_test(NAME DeepModuleTests COMMAND test_deep_modules)

    # Scalar-upward discovery pipeline tests (ZPhiEvaluator, ProofLifter, ScoutValidator, DiscoveryOrchestrator)
    add_executable(test_discovery
        tests/test_discovery.cpp
    )
    target_link_libraries(test_discovery PRIVATE autodiscover_core)
    add_test(NAME DiscoveryTests COMMAND test_discovery)

    # Universal multi-algebra discovery tests (CDTermGenerator, MultiLevelEvaluator, UniversalDiscoveryEngine)
    add_executable(test_universal
        tests/test_universal.cpp
    )
    target_link_libraries(test_universal PRIVATE autodiscover_core)
    add_test(NAME UniversalTests COMMAND test_universal)
endif()

# ============================================================================
# Installation
# ============================================================================
install(TARGETS autodiscover
    RUNTIME DESTINATION bin
)

install(TARGETS autodiscover_core
    ARCHIVE DESTINATION lib
)

install(FILES ${CORE_HEADERS} DESTINATION include/autodiscover/core)
install(FILES ${LOGIC_HEADERS} DESTINATION include/autodiscover/logic)
install(FILES ${DOMAIN_HEADERS} DESTINATION include/autodiscover/domain)
install(FILES ${PROOF_HEADERS} DESTINATION include/autodiscover/proof)
install(FILES ${RING_HEADERS} DESTINATION include/autodiscover/ring)
install(FILES ${ENCODING_HEADERS} DESTINATION include/autodiscover/encoding)
install(FILES ${FINGERPRINT_HEADERS} DESTINATION include/autodiscover/fingerprint)
install(FILES ${STORE_HEADERS} DESTINATION include/autodiscover/store)
install(FILES ${EGRAPH_HEADERS} DESTINATION include/autodiscover/egraph)
install(FILES ${CANON_HEADERS} DESTINATION include/autodiscover/canon)
install(FILES ${DISCOVERY_HEADERS} DESTINATION include/autodiscover/discovery)

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "Autodiscovery Equational Prover v${PROJECT_VERSION}")
message(STATUS "================================================")
message(STATUS "  C++ Standard:     ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build Tests:      ${BUILD_TESTS}")
message(STATUS "  Sanitizers:       ${ENABLE_SANITIZERS}")
message(STATUS "  LTO:              ${ENABLE_LTO}")
message(STATUS "")
message(STATUS "Modules:")
message(STATUS "  - Core: Term, Type, SymbolTable")
message(STATUS "  - Logic: Equation, KnowledgeBase, MatcherUnifier, Normalizer, InferenceEngine")
message(STATUS "  - Domain: Algebra, Scout, Closure")
message(STATUS "  - Proof: Proof, ProofChecker, CertificateExporter")
message(STATUS "  - Ring: ZPhi (Z[phi] bigint arithmetic)")
message(STATUS "  - Encoding: Prefix-free encoding, canonical bytecode")
message(STATUS "  - Fingerprint: phi-weighted fingerprinting")
message(STATUS "  - Store: Content-addressable proof store")
message(STATUS "  - EGraph: E-graph quotient structure")
message(STATUS "  - Canon: Full canonicalization pipeline")
message(STATUS "  - Discovery: Scalar-driven forced discovery loop")
message(STATUS "")
