//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_DEMANGLE_HPP
#define MRDOCS_API_SUPPORT_DEMANGLE_HPP

#include <mrdocs/Platform.hpp>
#include <string>
#include <string_view>
#include <typeinfo>

// The Itanium ABI demangler lives in <cxxabi.h>. Gate on its actual
// presence rather than on the compiler: clang-cl on Windows defines
// __clang__ but ships no <cxxabi.h>, and MrDocs parses this header with its
// own clang while documenting itself, so a compiler-only guard breaks that
// self-extraction on Windows.
#if defined(__has_include)
    #if __has_include(<cxxabi.h>)
        #define MRDOCS_HAS_CXXABI 1
    #endif
#endif

#if defined(MRDOCS_HAS_CXXABI)
    #include <cstdlib>
    #include <cxxabi.h>
#endif

// Type-name rendering, both compile-time (from the compiler's
// function-signature macros, usable in `constexpr` contexts) and runtime
// (demangling a `type_info` name). Each comes in a qualified variant
// (namespaces kept) and an unqualified one (only the innermost name), so
// callers in both the application and the tests have a single place to
// get a readable type name in whatever form they need.

namespace mrdocs {

namespace detail {

/** Drop the namespace qualifiers (and any template arguments) from a type name.

    E.g. `"mrdocs::FunctionSymbol"` becomes `"FunctionSymbol"`, and
    `"mrdocs::SymbolCommonBase<mrdocs::SymbolKind::EnumConstant>"` becomes
    `"SymbolCommonBase"`.
*/
constexpr std::string_view
removeNamespaceQualifiers(std::string_view name)
{
    std::string_view const head = name.substr(0, name.find('<'));
    constexpr std::string_view scopeDelimiter = "::";
    std::string_view::size_type const pos = head.rfind(scopeDelimiter);
    return pos != std::string_view::npos
        ? head.substr(pos + scopeDelimiter.size())
        : head;
}

} // namespace detail

/** The fully-qualified name of a type `T`, at compile time.

    Reads the type out of the compiler's function-signature macro
    (`__PRETTY_FUNCTION__` on Clang/GCC, `__FUNCSIG__` on MSVC), keeping
    any namespace qualifiers.

    @tparam T The type whose name to render.
    @return A view over static storage holding the qualified name, e.g.
        `qualifiedTypeName<mrdocs::FunctionSymbol>()` ->
        `"mrdocs::FunctionSymbol"`.
*/
template <typename T>
constexpr std::string_view
qualifiedTypeName()
{
#if defined(__clang__) || defined(__GNUC__)
    constexpr std::string_view typePrefix = "T = ";
    constexpr std::string_view fn = __PRETTY_FUNCTION__;
    constexpr std::string_view::size_type start = fn.find(typePrefix);
    static_assert(start != std::string_view::npos,
        "__PRETTY_FUNCTION__ does not have the expected format");
    std::string_view::size_type const pos = start + typePrefix.size();
    std::string_view::size_type const end = fn.find_first_of(";]", pos);
    return fn.substr(pos, end - pos);

#elif defined(_MSC_VER)
    constexpr std::string_view funcPrefix = "qualifiedTypeName<";
    constexpr std::string_view fn = __FUNCSIG__;
    constexpr std::string_view::size_type start = fn.find(funcPrefix);
    static_assert(start != std::string_view::npos,
        "__FUNCSIG__ does not have the expected format");
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
    return fn.substr(pos, end - pos);

#else
    return "unknown";
#endif
}

/** The unqualified name of a type `T`, at compile time.

    As @ref qualifiedTypeName, with the namespace qualifiers removed so
    only the innermost name remains.

    @tparam T The type whose name to render.
    @return A view over static storage holding the unqualified name, e.g.
        `unqualifiedTypeName<mrdocs::FunctionSymbol>()` ->
        `"FunctionSymbol"`, `unqualifiedTypeName<int>()` -> `"int"`.
*/
template <typename T>
constexpr std::string_view
unqualifiedTypeName()
{
    return detail::removeNamespaceQualifiers(qualifiedTypeName<T>());
}

/** Demangle a mangled type name at runtime.

    Uses the Itanium ABI demangler on Clang/GCC; on other toolchains the
    input is returned unchanged (MSVC's `type_info::name` is already
    human-readable).

    @param mangled A mangled name, e.g. from `type_info::name()`.
    @return The demangled, fully-qualified name, or `mangled` on failure.
*/
inline std::string
demangle(char const* mangled)
{
#if defined(MRDOCS_HAS_CXXABI)
    int status = 0;
    char* const demangled =
        abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
    std::string result = (status == 0 && demangled) ? demangled : mangled;
    std::free(demangled);
    return result;
#else
    return mangled;
#endif
}

/** Demangle a `type_info`'s name.

    @param ti The type to name.
    @return The demangled, fully-qualified name.
*/
inline std::string
demangle(std::type_info const& ti)
{
    return demangle(ti.name());
}

/** The fully-qualified runtime name of a type `T`.

    @return The demangled name of `T`, with namespaces.
*/
template <typename T>
std::string
qualifiedRuntimeTypeName()
{
    return demangle(typeid(T));
}

/** The unqualified runtime name of a type `T`.

    @return The demangled name of `T`, innermost component only.
*/
template <typename T>
std::string
unqualifiedRuntimeTypeName()
{
    return std::string(
        detail::removeNamespaceQualifiers(demangle(typeid(T))));
}

} // namespace mrdocs

#endif // MRDOCS_API_SUPPORT_DEMANGLE_HPP
