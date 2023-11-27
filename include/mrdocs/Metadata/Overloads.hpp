//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_OVERLOADS_HPP
#define MRDOCS_API_METADATA_OVERLOADS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Function.hpp>
#include <mrdocs/MetadataFwd.hpp>
#include <span>
#include <string_view>

namespace clang {
namespace mrdocs {

struct OverloadSet
{
    std::string_view Name;

    SymbolID Parent;

    std::span<const SymbolID> Namespace;

    std::span<const SymbolID> Members;

    OverloadSet(
        std::string_view name,
        const SymbolID& parent,
        std::span<const SymbolID> ns,
        std::span<const SymbolID> members)
        : Name(name)
        , Parent(parent)
        , Namespace(ns)
        , Members(members)
    {
    }
};

template<
    class Fn,
    class... Args>
decltype(auto)
visit(
    const OverloadSet& overloads,
    Fn&& fn,
    Args&&... args)
{
    return std::forward<Fn>(fn)(overloads,
        std::forward<Args>(args)...);
}

} // mrdocs
} // clang

#endif
