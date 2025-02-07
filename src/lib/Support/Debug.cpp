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

#include "lib/Support/Debug.hpp"
#include "lib/Support/Radix.hpp"
#include <memory>
#include <mrdocs/Metadata/Info/Record.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Symbols.hpp>

namespace clang {
namespace mrdocs {

void
debugEnableHeapChecking()
{
#if defined(_MSC_VER) && ! defined(NDEBUG)
    int flags = _CrtSetDbgFlag(
        _CRTDBG_REPORT_FLAG);
    flags |= _CRTDBG_LEAK_CHECK_DF;
    _CrtSetDbgFlag(flags);
#endif
}

} // mrdocs
} // clang

fmt::format_context::iterator
fmt::formatter<clang::mrdocs::SymbolID>::
format(
    clang::mrdocs::SymbolID const& s,
    fmt::format_context& ctx) const
{
    std::string str = s ?
        "<invalid SymbolID>" :
        clang::mrdocs::toBase64(s);
    return fmt::formatter<std::string>::format(std::move(str), ctx);
}

fmt::format_context::iterator
fmt::formatter<clang::mrdocs::InfoKind>::
format(
    clang::mrdocs::InfoKind t,
    fmt::format_context& ctx) const
{
    return fmt::formatter<std::string>::format(toString(t).str(), ctx);
}

fmt::format_context::iterator
fmt::formatter<clang::mrdocs::AccessKind>::
format(
    clang::mrdocs::AccessKind a,
    fmt::format_context& ctx) const
{
    return fmt::formatter<std::string>::format(toString(a).str(), ctx);
}

fmt::format_context::iterator
fmt::formatter<clang::mrdocs::Info>::
format(
    clang::mrdocs::Info const& i,
    fmt::format_context& ctx) const
{
    std::string str = fmt::format("Info: kind = {}", i.Kind);
    if (!i.Name.empty())
    {
        str += fmt::format(", name = '{}'", i.Name);
    }
    str += fmt::format(", ID = {}", i.id);
    clang::mrdocs::SymbolID curParent = i.Parent;
    std::string namespaces;
    while (curParent)
    {
        namespaces += fmt::format("{}", curParent);
        curParent = i.Parent;
        if (curParent)
        {
            namespaces += "::";
        }
    }
    if (!namespaces.empty())
    {
        str += fmt::format(", namespace = {}", namespaces);
    }
    return fmt::formatter<std::string>::format(std::move(str), ctx);
}
