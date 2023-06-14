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

#include "Support/Debug.hpp"
#include "Support/Radix.hpp"
#include <mrdox/Metadata/Info.hpp>
#include <mrdox/Metadata/Record.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <memory>

namespace clang {
namespace mrdox {

llvm::raw_ostream& debug_outs()
{
    return llvm::outs();
}

llvm::raw_ostream& debug_errs()
{
    return llvm::errs();
}

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

} // mrdox
} // clang

fmt::format_context::iterator
fmt::formatter<clang::mrdox::SymbolID>::
format(
    const clang::mrdox::SymbolID& s,
    fmt::format_context& ctx) const
{
    std::string str = s == clang::mrdox::SymbolID::zero ?
        "<empty SymbolID>" : clang::mrdox::toBase64(s);
    return fmt::formatter<std::string>::format(std::move(str), ctx);
}

fmt::format_context::iterator
fmt::formatter<clang::mrdox::OptionalSymbolID>::
format(
    const clang::mrdox::OptionalSymbolID& s,
    fmt::format_context& ctx) const
{
    return fmt::formatter<clang::mrdox::SymbolID>::format(*s, ctx);
}

fmt::format_context::iterator
fmt::formatter<clang::mrdox::InfoKind>::
format(
    clang::mrdox::InfoKind t,
    fmt::format_context& ctx) const
{
    const char* str = "<unknown InfoKind>";
    switch(t)
    {
    case clang::mrdox::InfoKind::Namespace:
        str = "namespace";
        break;
    case clang::mrdox::InfoKind::Record:
        str = "record";
        break;
    case clang::mrdox::InfoKind::Function:
        str = "function";
        break;
    case clang::mrdox::InfoKind::Enum:
        str = "enum";
        break;
    case clang::mrdox::InfoKind::Typedef:
        str = "typedef";
        break;
    case clang::mrdox::InfoKind::Variable:
        str = "variable";
        break;
    case clang::mrdox::InfoKind::Field:
        str = "field";
        break;
    case clang::mrdox::InfoKind::Specialization:
        str = "specialization";
        break;
    default:
        break;
    }
    return fmt::formatter<std::string>::format(str, ctx);
}

fmt::format_context::iterator
fmt::formatter<clang::mrdox::AccessKind>::
format(
    clang::mrdox::AccessKind a,
    fmt::format_context& ctx) const
{
    const char* str = "<unknown Access>";
    switch(a)
    {
    case clang::mrdox::AccessKind::Public:
        str = "public";
        break;
    case clang::mrdox::AccessKind::Protected:
        str = "protected";
        break;
    case clang::mrdox::AccessKind::Private:
        str = "private";
        break;
    case clang::mrdox::AccessKind::None:
        str = "none";
        break;
    default:
        break;
    }
    return fmt::formatter<std::string>::format(str, ctx);
}

fmt::format_context::iterator
fmt::formatter<clang::mrdox::Info>::
format(
    const clang::mrdox::Info& i,
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
