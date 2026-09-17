#ifndef AUTODISCOVER_CORE_TERMUTILS_HPP
#define AUTODISCOVER_CORE_TERMUTILS_HPP

/**
 * @file TermUtils.hpp
 * @brief Shared term-manipulation utilities
 *
 * Provides replaceAtPath and subtermAtPath  the canonical implementations
 * used by BOTH the inference engine and the proof checker, guaranteeing
 * identical path semantics everywhere.
 */

#include "Term.hpp"
#include <vector>
#include <cstddef>
#include <cstdint>

namespace autodiscover {
namespace core {

/**
 * @brief Extract the subterm at a given position path
 *
 * @tparam IndexT  The integer type used for path indices (uint32_t or size_t)
 * @param term     Root term to traverse
 * @param path     Sequence of child indices from root to target
 * @param depth    Current depth in the path (start at 0)
 * @return The subterm at the path, or nullptr if the path is invalid
 */
template<typename IndexT>
inline const Term* subtermAtPath(const Term* term,
                                 const std::vector<IndexT>& path,
                                 size_t depth) {
    if (!term) return nullptr;
    if (depth == path.size()) return term;
    
    auto idx = static_cast<size_t>(path[depth]);
    const auto& children = term->children();
    if (idx >= children.size()) return nullptr;
    
    return subtermAtPath(children[idx], path, depth + 1);
}

/**
 * @brief Replace the subterm at the given position path with a replacement term
 *
 * Reconstructs the term tree from the replacement position back up to the root,
 * interning each rebuilt node through the factory.
 *
 * @tparam IndexT      The integer type used for path indices
 * @param term         Root term to reconstruct
 * @param path         Sequence of child indices from root to target position
 * @param depth        Current depth in the path (start at 0)
 * @param replacement  The term to place at the target position
 * @param factory      TermFactory for interning rebuilt nodes
 * @return New interned term with the replacement at the given position
 */
template<typename IndexT>
inline const Term* replaceAtPath(const Term* term,
                                  const std::vector<IndexT>& path,
                                  size_t depth,
                                  const Term* replacement,
                                  TermFactory& factory) {
    if (!term) return replacement;
    if (depth == path.size()) return replacement;
    
    auto idx = static_cast<size_t>(path[depth]);
    std::vector<const Term*> newChildren = term->children();
    if (idx >= newChildren.size()) return term; // invalid path  return unchanged
    
    newChildren[idx] = replaceAtPath(newChildren[idx], path, depth + 1, replacement, factory);
    
    if (term->isPair() && newChildren.size() == 2) {
        return factory.pair(newChildren[0], newChildren[1]);
    }
    return factory.apply(term->symbol(), std::move(newChildren), term->sort());
}

} // namespace core
} // namespace autodiscover

#endif // AUTODISCOVER_CORE_TERMUTILS_HPP
