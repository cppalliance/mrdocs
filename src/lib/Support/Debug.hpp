//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_SUPPORT_DEBUG_HPP
#define MRDOX_LIB_SUPPORT_DEBUG_HPP

#include <mrdox/Platform.hpp>
#if ! defined(NDEBUG)
#include <llvm/Support/ErrorHandling.h>
#endif
#include <llvm/Support/raw_ostream.h>

#include <fmt/format.h>
#include <string>
#include <mrdox/MetadataFwd.hpp>

template<>
struct fmt::formatter<clang::mrdox::SymbolID>
    : fmt::formatter<std::string>
{
    fmt::format_context::iterator format(
        const clang::mrdox::SymbolID& s,
        fmt::format_context& ctx) const;
};

template<>
struct fmt::formatter<clang::mrdox::OptionalSymbolID>
    : fmt::formatter<clang::mrdox::SymbolID>
{
    fmt::format_context::iterator format(
        const clang::mrdox::OptionalSymbolID& s,
        fmt::format_context& ctx) const;
};

template<>
struct fmt::formatter<clang::mrdox::InfoKind>
    : fmt::formatter<std::string>
{
    fmt::format_context::iterator format(
        clang::mrdox::InfoKind t,
        fmt::format_context& ctx) const;
};

template<>
struct fmt::formatter<clang::mrdox::AccessKind>
    : fmt::formatter<std::string>
{
    fmt::format_context::iterator format(
        clang::mrdox::AccessKind a,
        fmt::format_context& ctx) const;
};

template<>
struct fmt::formatter<clang::mrdox::Info>
    : fmt::formatter<std::string>
{
    fmt::format_context::iterator format(
        const clang::mrdox::Info& i,
        fmt::format_context& ctx) const;
};

// Some nice odds and ends such as leak checking
// and redirection to the Visual Studio output window.

namespace clang {
namespace mrdox {

/** Return a stream which writes output to the debugger.
*/
/** @{ */
MRDOX_DECL llvm::raw_ostream& debug_outs();

/** @} */

/** Enable debug heap checking.
*/
MRDOX_DECL void debugEnableHeapChecking();

#define static_error(msg, value) \
    static_assert(!std::is_same_v<decltype(value),decltype(value)>,msg)

#ifndef NDEBUG
    // effectively the same as c++23 <print>,
    // except using debug_outs() as the output stream
    template<
        typename Format,
        typename... Args>
    void print_debug(
        Format fmt,
        Args&&... args)
    {
        debug_outs() << fmt::vformat(fmt,
            fmt::make_format_args(std::forward<Args>(args)...));
    }
#else
    template<typename... Args>
    void print_debug(
        Args&&...)
    {
        // if <format> isn't supported, ignore the call
    }
#endif

} // mrdox
} // clang

#endif
