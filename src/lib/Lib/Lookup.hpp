//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_LOOKUP_HPP
#define MRDOCS_LIB_LOOKUP_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata/Symbols.hpp>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <ranges>
#include <unordered_map>

namespace clang {
namespace mrdocs {

class LookupTable
{
    std::unordered_multimap<
        std::string_view, const Info*> lookups_;

public:
    LookupTable(
        const Info& info,
        const Corpus& corpus);

    auto lookup(std::string_view name) const
    {
        auto [first, last] = lookups_.equal_range(name);
        return std::ranges::subrange(
            first, last) | std::views::values;
    }

    void add(std::string_view name, const Info* info)
    {
        lookups_.emplace(name, info);
    }
};

class SymbolLookup
{
    const Corpus& corpus_;

    // maps symbol ID to its lookup table, if lookup is supported
    std::unordered_map<
        const Info*,
        LookupTable> lookup_tables_;

    const Info*
    adjustLookupContext(const Info* context);

    const Info*
    lookThroughTypedefs(const Info* I);

    const Info*
    getTypeAsTag(
        const std::unique_ptr<TypeInfo>& T);

    const Info*
    lookupInContext(
        const Info* context,
        std::string_view name,
        bool for_nns = false);

    const Info*
    lookupUnqualifiedImpl(
        const Info* context,
        std::string_view name,
        bool for_nns);
public:
    SymbolLookup(const Corpus& corpus);

    const Info*
    lookupUnqualified(
        const Info* context,
        std::string_view name);

    const Info*
    lookupQualified(
        const Info* context,
        std::span<const std::string_view> qualifier,
        std::string_view terminal);
};

} // mrdocs
} // clang

#endif
