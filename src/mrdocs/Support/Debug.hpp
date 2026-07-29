//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_DEBUG_HPP
#define MRDOCS_LIB_SUPPORT_DEBUG_HPP

#include <mrdocs/Platform.hpp>
#if ! defined(NDEBUG)
#    include <llvm/Support/ErrorHandling.h>
#endif
#include "Radix.hpp"
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <llvm/Support/raw_ostream.h>
#include <format>
#include <string>

template <>
struct std::formatter<mrdocs::SymbolID> : std::formatter<std::string> {
  template <class FmtContext>
  auto format(mrdocs::SymbolID const &s,
                                       FmtContext &ctx) const {
    std::string str = s ? mrdocs::toBase64(std::string_view(
        reinterpret_cast<char const*>(s.data()), s.size())) : "<invalid SymbolID>";
    return std::formatter<std::string>::format(std::move(str), ctx);
  }
};

template <>
struct std::formatter<mrdocs::SymbolKind> : std::formatter<std::string> {
  template <class FmtContext>
  auto format(mrdocs::SymbolKind t,
                                       FmtContext &ctx) const {
    return std::formatter<std::string>::format(std::string(toString(t)), ctx);
  }
};

template <>
struct std::formatter<mrdocs::AccessKind> : std::formatter<std::string> {
  template <class FmtContext>
  auto format(mrdocs::AccessKind a,
                                       FmtContext &ctx) const {
    return std::formatter<std::string>::format(std::string(toString(a)), ctx);
  }
};

template <>
struct std::formatter<mrdocs::Symbol> : std::formatter<std::string> {
  template <class FmtContext>
  auto format(mrdocs::Symbol const &i,
                                       FmtContext &ctx) const {
    return std::formatter<std::string>::format(toString(i), ctx);
  }

private:
  static std::string toString(mrdocs::Symbol const &i);
};

// Some nice odds and ends such as leak checking
// and redirection to the Visual Studio output window.

namespace mrdocs {

/** Enable debug heap checking.
*/
MRDOCS_DECL void debugEnableHeapChecking();

#define static_error(msg, value) \
    static_assert(!std::is_same_v<decltype(value),decltype(value)>,msg)

} // mrdocs

#endif // MRDOCS_LIB_SUPPORT_DEBUG_HPP
