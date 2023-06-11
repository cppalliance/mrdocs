// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TOOL_AST_ASTVISITORHELPERS_HPP
#define MRDOX_TOOL_AST_ASTVISITORHELPERS_HPP

#include "Support/Debug.hpp"
#include <mrdox/Platform.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Metadata/Record.hpp>
#include <mrdox/Metadata/Namespace.hpp>
#include <type_traits>

namespace clang {
namespace mrdox {

template<typename T, typename... Args>
void insertChild(NamespaceInfo& I, Args&&... args)
{
    if constexpr(std::is_constructible_v<SymbolID, Args...>)
    {
        if constexpr(T::isField())
            // invalid namespace member
            MRDOX_UNREACHABLE();
        else if constexpr(T::isSpecialization())
            I.Specializations.emplace_back(std::forward<Args>(args)...);
        else
            I.Members.emplace_back(std::forward<Args>(args)...);
    }
    else
    {
        // invalid arguments
        MRDOX_UNREACHABLE();
    }
}

template<typename T, typename... Args>
void insertChild(RecordInfo& I, Args&&... args)
{
    if constexpr(std::is_constructible_v<SymbolID, Args...>)
    {
        if constexpr(T::isSpecialization())
            I.Specializations.emplace_back(std::forward<Args>(args)...);
        else
            I.Members.emplace_back(std::forward<Args>(args)...);
    }
    else
    {
        // invalid arguments
        MRDOX_UNREACHABLE();
    }
}

} // mrdox
} // clang

#endif
