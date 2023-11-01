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
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Record.hpp>
#include <mrdocs/Metadata/Symbols.hpp>
#include <memory>

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
    const clang::mrdocs::SymbolID& s,
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
    const char* str = "<unknown InfoKind>";
    switch(t)
    {
    case clang::mrdocs::InfoKind::Namespace:
        str = "namespace";
        break;
    case clang::mrdocs::InfoKind::Record:
        str = "record";
        break;
    case clang::mrdocs::InfoKind::Function:
        str = "function";
        break;
    case clang::mrdocs::InfoKind::Enum:
        str = "enum";
        break;
    case clang::mrdocs::InfoKind::Typedef:
        str = "typedef";
        break;
    case clang::mrdocs::InfoKind::Variable:
        str = "variable";
        break;
    case clang::mrdocs::InfoKind::Field:
        str = "field";
        break;
    case clang::mrdocs::InfoKind::Specialization:
        str = "specialization";
        break;
    default:
        break;
    }
    return fmt::formatter<std::string>::format(str, ctx);
}

fmt::format_context::iterator
fmt::formatter<clang::mrdocs::AccessKind>::
format(
    clang::mrdocs::AccessKind a,
    fmt::format_context& ctx) const
{
    const char* str = "<unknown Access>";
    switch(a)
    {
    case clang::mrdocs::AccessKind::Public:
        str = "public";
        break;
    case clang::mrdocs::AccessKind::Protected:
        str = "protected";
        break;
    case clang::mrdocs::AccessKind::Private:
        str = "private";
        break;
    case clang::mrdocs::AccessKind::None:
        str = "none";
        break;
    default:
        break;
    }
    return fmt::formatter<std::string>::format(str, ctx);
}

fmt::format_context::iterator
fmt::formatter<clang::mrdocs::Info>::
format(
    const clang::mrdocs::Info& i,
    fmt::format_context& ctx) const
{
    std::string str = fmt::format("Info: kind = {}", i.Kind);
    if(! i.Name.empty())
        str += fmt::format(", name = '{}'", i.Name);
    str += fmt::format(", ID = {}", i.id);
    if(! i.Namespace.empty())
    {
        std::string namespaces;
        namespaces += fmt::format("{}", i.Namespace[0]);
        for(std::size_t idx = 1; idx < i.Namespace.size(); ++idx)
        {
            namespaces += "::";
            namespaces += fmt::format("{}", i.Namespace[idx]);
        }
        str += fmt::format(", namespace = {}", namespaces);
    }
    return fmt::formatter<std::string>::format(std::move(str), ctx);
}
