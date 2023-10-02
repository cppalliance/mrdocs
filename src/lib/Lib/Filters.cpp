//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Filters.hpp"

namespace clang {
namespace mrdox {

FilterPattern::
FilterPattern()
{
#ifndef NDEBUG
    // pattern for VS debugger visualizers
    pattern_.push_back('*');
#endif
}

FilterPattern::
FilterPattern(
    std::string_view pattern)
{
    while(! pattern.empty())
    {
        bool wildcard = pattern.front() == '*';
        std::size_t part_size = std::min(
            pattern.size(), wildcard ?
                pattern.find_first_not_of('*') :
                pattern.find('*'));

        if(! wildcard)
            raw_.append(pattern, 0, part_size);

        #ifndef NDEBUG
            // pattern for VS debugger visualizers
            pattern_.append(wildcard ?
                "*" : pattern.substr(0, part_size));
        #endif

        pattern.remove_prefix(part_size);

        // don't store the parts for patterns without
        // wildcards, as well as wildcard only patterns
        if(pattern.empty() && parts_.empty())
            break;

        parts_.push_back(wildcard ? 0 : part_size);
    }
}

bool
FilterPattern::
matchesSlow(
    std::string_view str,
    std::string_view pattern,
    std::span<const std::size_t> parts) const
{
    MRDOX_ASSERT(! parts.empty());
    const auto& part = pattern.substr(0, parts[0]);
    pattern.remove_prefix(parts[0]);
    parts = parts.subspan(1);
    // current part is not a wild card
    if(! part.empty())
    {
        // every character in the pattern must
        // match the beginning of the string
        if(! str.starts_with(part))
            return false;
        str.remove_prefix(part.size());
        // if there are no other parts, then we only
        // have a match if there are no characters left
        if(parts.empty())
            return str.empty();
        // if we had a match and the pattern ends in
        // a wildcard, the entire string matches
        if(parts.size() == 1)
            return true;
        return matchesSlow(str, pattern, parts);
    }
    // current part is a wildcard.
    // check all possible lengths for this wildcard until we
    // either find a match, or we run out of characters.
    for(; ! str.empty(); str.remove_prefix(1))
    {
        if(matchesSlow(str, pattern, parts))
            return true;
    }
    return false;
}

bool
FilterPattern::
matches(std::string_view str) const
{
    if(parts_.empty())
    {
        // if the raw pattern is empty, the pattern is '*'
        // and matches everything
        if(raw_.empty())
            return true;
        // the pattern contains no wildcards, compare with
        // the raw pattern
        return str == raw_;
    }
    // no match when the string is shorter than
    // the shortest possible match size
    if(str.size() < raw_.size())
        return false;
    // otherwise, use the wildcard matching algorithm
    return matchesSlow(str, raw_, parts_);
}

bool
FilterPattern::
subsumes(const FilterPattern& other) const
{
    return matches(other.raw_);
}

const FilterNode*
FilterNode::
findChild(
    std::string_view name) const
{
    const FilterNode* best = nullptr;
    for(auto& child : Children)
    {
        if(! child.Pattern.matches(name))
            continue;
        if(! best || best->Pattern.subsumes(child.Pattern))
            best = &child;
    }
    return best;
}

void
FilterNode::
mergePattern(
    std::span<FilterPattern> parts,
    bool excluded)
{
    if(parts.empty())
        return;

    const auto& pattern = parts[0];
    parts = parts.subspan(1);

    std::vector<FilterNode> subsumed;
    FilterNode* matching_node = nullptr;
    for(FilterNode& child : Children)
    {
        // if the new pattern would match everything
        // that the child node would, merge the subsequent
        // patterns into the child node
        if(pattern.subsumes(child.Pattern))
            child.mergePattern(parts, excluded);

        // found_exact |= child.Pattern == pattern;
        if(child.Pattern == pattern)
            matching_node = &child;
        // if an exact match has not been found, collect the
        // existing children which would match this pattern
        if(! matching_node && child.Pattern.subsumes(pattern))
        {
            subsumed.insert(subsumed.end(),
                child.Children.begin(),
                child.Children.end());
        }
    }
    // if we didn't find an exact match, add a new node
    if(! matching_node)
    {
        matching_node = &Children.emplace_back(
            pattern, std::move(subsumed), excluded);
        matching_node->mergePattern(parts, excluded);
    }

    if(parts.empty())
    {
        // mark terminal nodes are explicitly specified
        matching_node->Explicit = true;
        // whitelist overrides blacklist
        matching_node->Excluded &= excluded;
    }
}

void
FilterNode::
finalize(
    bool any_parent_explicit,
    bool any_parent_excluded,
    bool any_parent_included)
{
    any_parent_explicit |= Explicit;
    any_parent_excluded |= (Excluded && Explicit);
    any_parent_included |= (! Excluded && Explicit);

    for(FilterNode& child : Children)
        child.finalize(
            any_parent_explicit,
            any_parent_excluded,
            any_parent_included);

    if(! any_parent_explicit)
        return;

    std::erase_if(Children, [&](const auto& child) -> bool
        {
            // do not prune child nodes which are non-terminal
            if(! child.Children.empty())
                return false;
            if(! (any_parent_excluded || child.Excluded))
                return true;
            if(! (any_parent_included || ! child.Excluded))
                return true;
            return false;
        });
}

} // mrdox
} // clang
