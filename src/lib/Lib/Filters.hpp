//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_FILTERS_HPP
#define MRDOCS_LIB_FILTERS_HPP

#include <mrdocs/Platform.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace clang {
namespace mrdocs {

class FilterPattern
{
    // pattern without any wildcards
    std::string raw_;
    // pattern part lengths, where zero represents a wildcard
    std::vector<std::size_t> parts_;

#ifndef NDEBUG
    // pattern string for VS debugger visualizers
    std::string pattern_;
#endif

    /** Returns whether a string matches the multi-component pattern.

        This function implements matching for patterns which contain
        multiple components (i.e. both literal strings and wildcards).
    */
    bool
    matchesSlow(
        std::string_view str,
        std::string_view pattern,
        std::span<const std::size_t> parts) const;

public:
    FilterPattern();
    FilterPattern(std::string_view pattern);

    bool
    operator==(const FilterPattern&) const noexcept = default;

    /** Returns whether a string matches the pattern.
    */
    bool
    matches(std::string_view str) const;

    /** Returns whether this pattern subsumes the other.

        A pattern `a` subsumes a pattern `b` if `a` would
        at least match every string that `b` does.
    */
    bool
    subsumes(const FilterPattern& other) const;
};

struct FilterNode
{
    /** The filter pattern.

        The pattern defines which symbols match this node.
    */
    FilterPattern Pattern;

    /** Filter nodes for members of matching symbols.

        Members of symbols which match this node will be
        matched against the child nodes.
    */
    std::vector<FilterNode> Children;

    /** Whether the node is excluded (i.e. blacklisted).
    */
    bool Excluded : 1 = false;

    /** Whether the node is explicit.

        A node is explicit if it represents the last component
        of a filter config string, e.g. `B` in `A::B`.
    */
    bool Explicit : 1 = false;

    FilterNode() = default;

    FilterNode(
        const FilterPattern& pattern,
        std::vector<FilterNode>&& children,
        bool excluded)
        : Pattern(pattern)
        , Children(std::move(children))
        , Excluded(excluded)
    {
    }

    bool isTerminal() const noexcept
    {
        return Children.empty();
    }

    /** Find a matching child node.

        Finds the most constrained child node which
        matches `name`.
    */
    const FilterNode*
    findChild(std::string_view name) const;

    /** Add child node for the specified pattern.

        Creates a child node from `parts` and merges
        it into the children of this node.
    */
    void
    mergePattern(
        std::span<FilterPattern> parts,
        bool excluded);

    /** Prune child nodes.

        Removes any children which specify
        meaningless or redundant filters.
    */
    void
    finalize(
        bool any_parent_explicit,
        bool any_parent_excluded,
        bool any_parent_included);
};

} // mrdocs
} // clang

#endif
