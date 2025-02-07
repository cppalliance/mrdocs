//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_DEBUG_HPP
#define MRDOCS_LIB_SUPPORT_DEBUG_HPP

#include <mrdocs/Platform.hpp>
#if ! defined(NDEBUG)
#include <llvm/Support/ErrorHandling.h>
#endif
#include <llvm/Support/raw_ostream.h>
#include <mrdocs/Metadata/Symbols.hpp>
#include <mrdocs/MetadataFwd.hpp>
#include <fmt/format.h>
#include <string>

template<>
struct fmt::formatter<clang::mrdocs::SymbolID>
    : fmt::formatter<std::string>
{
    fmt::format_context::iterator format(
        clang::mrdocs::SymbolID const& s,
        fmt::format_context& ctx) const;
};

template<>
struct fmt::formatter<clang::mrdocs::InfoKind>
    : fmt::formatter<std::string>
{
    fmt::format_context::iterator format(
        clang::mrdocs::InfoKind t,
        fmt::format_context& ctx) const;
};

template<>
struct fmt::formatter<clang::mrdocs::AccessKind>
    : fmt::formatter<std::string>
{
    fmt::format_context::iterator format(
        clang::mrdocs::AccessKind a,
        fmt::format_context& ctx) const;
};

template<>
struct fmt::formatter<clang::mrdocs::Info>
    : fmt::formatter<std::string>
{
    fmt::format_context::iterator format(
        clang::mrdocs::Info const& i,
        fmt::format_context& ctx) const;
};

// Some nice odds and ends such as leak checking
// and redirection to the Visual Studio output window.

namespace clang::mrdocs {

/** Enable debug heap checking.
*/
MRDOCS_DECL void debugEnableHeapChecking();

#define static_error(msg, value) \
    static_assert(!std::is_same_v<decltype(value),decltype(value)>,msg)

} // clang::mrdocs

#endif
