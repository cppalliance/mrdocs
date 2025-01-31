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

namespace clang::mrdocs {

class LookupTable
{
    // maps unquaified names to symbols with that name.
    // names from member symbols which are "transparent"
    // (e.g. unscoped enums and inline namespaces) will
    // have their members added to the table as well
    std::unordered_multimap<
        std::string_view, Info const*> lookups_;

public:
    LookupTable(
        Info const& info,
        Corpus const& corpus);

    auto lookup(std::string_view name) const
    {
        auto [first, last] = lookups_.equal_range(name);
        return std::ranges::subrange(
            first, last) | std::views::values;
    }

    void add(std::string_view name, Info const* info)
    {
        lookups_.emplace(name, info);
    }
};

/*  A tool for looking up symbols by name

    This class provides a way to look up symbols by name.

    It is mainly used to resolve references in the
    documentation.
 */
class SymbolLookup
{
    Corpus const& corpus_;

    // maps symbol ID to its lookup table, if lookup is supported
    std::unordered_map<
        Info const*,
        LookupTable> lookup_tables_;

    struct LookupCallback
    {
        virtual ~LookupCallback() = default;
        virtual bool operator()(Info const&) = 0;
    };

    template<typename Fn>
    auto makeHandler(Fn& fn);

    Info const*
    adjustLookupContext(Info const* context);

    Info const*
    lookThroughTypedefs(Info const* I);

    Info const*
    lookupInContext(
        Info const* context,
        std::string_view name,
        bool for_nns,
        LookupCallback& callback);

    Info const*
    lookupUnqualifiedImpl(
        const Info* context,
        std::string_view name,
        bool for_nns,
        LookupCallback& callback);

    const Info*
    lookupQualifiedImpl(
        const Info* context,
        std::span<const std::string_view> qualifier,
        std::string_view terminal,
        LookupCallback& callback);

public:
    SymbolLookup(const Corpus& corpus);

    template<typename Fn>
    const Info*
    lookupUnqualified(
        const Info* context,
        std::string_view name,
        Fn&& callback)
    {
        auto handler = makeHandler(callback);
        return lookupUnqualifiedImpl(
            context, name, false, handler);
    }

    template<typename Fn>
    const Info*
    lookupQualified(
        const Info* context,
        std::span<const std::string_view> qualifier,
        std::string_view terminal,
        Fn&& callback)
    {
        auto handler = makeHandler(callback);
        return lookupQualifiedImpl(
            context, qualifier, terminal, handler);
    }
};

template<typename Fn>
auto
SymbolLookup::
makeHandler(Fn& fn)
{
    class LookupCallbackImpl
        : public LookupCallback
    {
        Fn& fn_;

    public:
        ~LookupCallbackImpl() override = default;

        LookupCallbackImpl(Fn& fn)
            : fn_(fn)
        {
        }

        bool operator()(const Info& I) override
        {
            return fn_(I);
        }
    };
    return LookupCallbackImpl(fn);
}

} // clang::mrdocs

#endif
