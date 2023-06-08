// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_AST_ASTVISITORHELPERS_HPP
#define MRDOX_LIB_AST_ASTVISITORHELPERS_HPP

#include "Support/Debug.hpp"
#include <mrdox/Platform.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Metadata/Record.hpp>
#include <mrdox/Metadata/Scope.hpp>
#include <type_traits>

namespace clang {
namespace mrdox {


template<typename T, typename... Args>
void insertChild(Scope& P, Args&&... args)
{
    if constexpr(std::is_constructible_v<Reference, Args...>)
    {
        using U = std::remove_cvref_t<T>;
        if constexpr(std::is_same_v<U, NamespaceInfo>)
            P.Namespaces.emplace_back(std::forward<Args>(args)...);
        else if constexpr(std::is_same_v<U, RecordInfo>)
            P.Records.emplace_back(std::forward<Args>(args)...);
        else if constexpr(std::is_same_v<U, FunctionInfo>)
            P.Functions.emplace_back(std::forward<Args>(args)...);
        else if constexpr(std::is_same_v<U, TypedefInfo>)
            P.Typedefs.emplace_back(std::forward<Args>(args)...);
        else if constexpr(std::is_same_v<U, EnumInfo>)
            P.Enums.emplace_back(std::forward<Args>(args)...);
        else if constexpr(std::is_same_v<U, VarInfo>)
            P.Vars.emplace_back(std::forward<Args>(args)...);
        else if constexpr(std::is_same_v<U, SpecializationInfo>)
            P.Specializations.emplace_back(std::forward<Args>(args)...);
        else
            // KRYSTIAN NOTE: Child should *never* be FieldInfo
            llvm_unreachable("invalid Scope child");
    }
    else
    {
        llvm_unreachable("invalid arguments");
    }
}

template<typename T, typename... Args>
void insertChild(RecordScope& P, Args&&... args)
{
    using U = std::remove_cvref_t<T>;
    if constexpr(std::is_constructible_v<MemberRef, Args...>)
    {
        if constexpr(std::is_same_v<U, RecordInfo>)
            P.Records.emplace_back(std::forward<Args>(args)...);
        else if constexpr(std::is_same_v<U, FunctionInfo>)
            P.Functions.emplace_back(std::forward<Args>(args)...);
        else if constexpr(std::is_same_v<U, TypedefInfo>)
            P.Types.emplace_back(std::forward<Args>(args)...);
        else if constexpr(std::is_same_v<U, EnumInfo>)
            P.Enums.emplace_back(std::forward<Args>(args)...);
        else if constexpr(std::is_same_v<U, FieldInfo>)
            P.Fields.emplace_back(std::forward<Args>(args)...);
        else if constexpr(std::is_same_v<U, VarInfo>)
            P.Vars.emplace_back(std::forward<Args>(args)...);
        else
            llvm_unreachable("invalid RecordScope member");
    }
    else if constexpr(std::is_constructible_v<SymbolID, Args...>)
    {
        if constexpr(std::is_same_v<U, SpecializationInfo>)
            P.Specializations.emplace_back(std::forward<Args>(args)...);
        else
            llvm_unreachable("invalid RecordScope member");
    }
    else
    {
        llvm_unreachable("invalid arguments");
    }
}

} // mrdox
} // clang

#endif
