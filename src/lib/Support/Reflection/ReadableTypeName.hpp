//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_REFLECTION_READABLETYPENAME_HPP
#define MRDOCS_LIB_SUPPORT_REFLECTION_READABLETYPENAME_HPP

#include <string_view>

namespace mrdocs {

namespace detail {

/** Remove namespace qualifiers from a type name.

    E.g.: "mrdocs::FunctionSymbol" -> "FunctionSymbol".
*/
constexpr std::string_view
removeNamespaceQualifiers(std::string_view name)
{
    constexpr std::string_view scopeDelimiter = "::";
    std::string_view::size_type const pos = name.rfind(scopeDelimiter);
    return pos != std::string_view::npos
        ? name.substr(pos + scopeDelimiter.size())
        : name;
}

}

/** Get the unqualified name of a type.

    Extracts the name from __PRETTY_FUNCTION__ (Clang/GCC) or __FUNCSIG__ (MSVC).

    E.g.: readableTypeName<mrdocs::FunctionSymbol>() -> "FunctionSymbol".
*/
template <typename T>
constexpr std::string_view
readableTypeName()
{
#if defined(__clang__) || defined(__GNUC__)
    // Expected format of __PRETTY_FUNCTION__:
    // Clang: "std::string_view mrdocs::detail::readableTypeName() [T = mrdocs::FunctionSymbol]"
    // GCC:   "constexpr std::string_view mrdocs::detail::readableTypeName() [with T = mrdocs::FunctionSymbol; ...]"
    constexpr std::string_view typePrefix = "T = ";
    constexpr std::string_view fn = __PRETTY_FUNCTION__;
    constexpr std::string_view::size_type start = fn.find(typePrefix);
    static_assert(start != std::string_view::npos, "__PRETTY_FUNCTION__ does not have the expected format");

    std::string_view::size_type pos = start + typePrefix.size();
    std::string_view::size_type const end = fn.find_first_of(";]", pos);
    std::string_view const name = fn.substr(pos, end - pos);
    return detail::removeNamespaceQualifiers(name);

#elif defined(_MSC_VER)
    // Expected format of __FUNCSIG__:
    // MSVC: "... __cdecl mrdocs::detail::readableTypeName<struct mrdocs::FunctionSymbol>(void)"
    constexpr std::string_view funcPrefix = "readableTypeName<";
    constexpr std::string_view fn = __FUNCSIG__;
    constexpr std::string_view::size_type start = fn.find(funcPrefix);
    static_assert(start != std::string_view::npos, "__FUNCSIG__ does not have the expected format");
    std::string_view::size_type pos = start + funcPrefix.size();

    // Skip "struct ", "class ", "enum ".
    for (std::string_view const prefix : { "struct ", "class ", "enum " })
    {
        if (fn.substr(pos, prefix.size()) == prefix)
        {
            pos += prefix.size();
        }
    }

    std::string_view::size_type const end = fn.find('>', pos);
    std::string_view const name = fn.substr(pos, end - pos);
    return detail::removeNamespaceQualifiers(name);

#else
    return "unknown";
#endif
}

}

#endif
