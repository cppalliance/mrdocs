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
#    include <llvm/Support/ErrorHandling.h>
#endif
#include <lib/Support/Radix.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Info/SymbolID.hpp>
#include <llvm/Support/raw_ostream.h>
#include <format>
#include <string>

template <>
struct std::formatter<clang::mrdocs::SymbolID> : std::formatter<std::string> {
  template <class FmtContext>
  std::format_context::iterator format(clang::mrdocs::SymbolID const &s,
                                       FmtContext &ctx) const {
    std::string str = s ? clang::mrdocs::toBase64(s) : "<invalid SymbolID>";
    return std::formatter<std::string>::format(std::move(str), ctx);
  }
};

template <>
struct std::formatter<clang::mrdocs::InfoKind> : std::formatter<std::string> {
  template <class FmtContext>
  std::format_context::iterator format(clang::mrdocs::InfoKind t,
                                       FmtContext &ctx) const {
    return std::formatter<std::string>::format(toString(t).str(), ctx);
  }
};

template <>
struct std::formatter<clang::mrdocs::AccessKind> : std::formatter<std::string> {
  template <class FmtContext>
  std::format_context::iterator format(clang::mrdocs::AccessKind a,
                                       FmtContext &ctx) const {
    return std::formatter<std::string>::format(toString(a).str(), ctx);
  }
};

template <>
struct std::formatter<clang::mrdocs::Info> : std::formatter<std::string> {
  template <class FmtContext>
  std::format_context::iterator format(clang::mrdocs::Info const &i,
                                       FmtContext &ctx) const {
    return std::formatter<std::string>::format(toString(i), ctx);
  }

private:
  static std::string toString(clang::mrdocs::Info const &i);
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

#endif // MRDOCS_LIB_SUPPORT_DEBUG_HPP
