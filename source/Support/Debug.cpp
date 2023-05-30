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
#include <mrdox/Metadata/Reference.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <atomic>
#include <memory>

#if defined(_MSC_VER) && ! defined(NDEBUG)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace clang {
namespace mrdox {

namespace {

static bool const isDebuggerPresent = ::IsDebuggerPresent();

class debug_ostream
    : public llvm::raw_ostream
{
    static constexpr std::size_t BufferSize = 4096;

    llvm::raw_ostream& os_;
    std::string buf_;

    void write_impl(const char * Ptr, size_t Size) override
    {
        os_.write(Ptr, Size);

        if(isDebuggerPresent)
        {
            // Windows expects a null terminated string
            buf_[Size] = '\0';
            ::OutputDebugStringA(buf_.data());
        }
    }

public:
    debug_ostream(
        llvm::raw_ostream& os)
        : os_(os)
    {
        buf_.resize(BufferSize + 1);
        SetBuffer(buf_.data(), BufferSize);
        os_.tie(this);
    }

    ~debug_ostream()
    {
        os_.tie(nullptr);
        flush();
    }

    std::size_t preferred_buffer_size() const override
    {
        return BufferSize;
    }

    std::uint64_t current_pos() const override
    {
        return 0;
    }
};

} // (anon)

llvm::raw_ostream& debug_outs()
{
    static debug_ostream stream(llvm::outs());
    return stream;
}

llvm::raw_ostream& debug_errs()
{
    static debug_ostream stream(llvm::errs());
    return stream;
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

#else

//------------------------------------------------

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

void debugEnableHeapChecking()
{
}

} // mrdox
} // clang

#endif

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
fmt::formatter<clang::mrdox::InfoType>::
format(
    clang::mrdox::InfoType t,
    fmt::format_context& ctx) const
{
    const char* str = "<unknown InfoType>";
    switch(t)
    {
    case clang::mrdox::InfoType::IT_default:
        str = "default";
        break;
    case clang::mrdox::InfoType::IT_namespace:
        str = "namespace";
        break;
    case clang::mrdox::InfoType::IT_record:
        str = "record";
        break;
    case clang::mrdox::InfoType::IT_function:
        str = "function";
        break;
    case clang::mrdox::InfoType::IT_enum:
        str = "enum";
        break;
    case clang::mrdox::InfoType::IT_typedef:
        str = "typedef";
        break;
    case clang::mrdox::InfoType::IT_variable:
        str = "variable";
        break;
    case clang::mrdox::InfoType::IT_field:
        str = "field";
        break;
    default:
        break;
    }
    return fmt::formatter<std::string>::format(str, ctx);
}

fmt::format_context::iterator
fmt::formatter<clang::mrdox::Access>::
format(
    clang::mrdox::Access a,
    fmt::format_context& ctx) const
{
    const char* str = "<unknown Access>";
    switch(a)
    {
    case clang::mrdox::Access::Public:
        str = "public";
        break;
    case clang::mrdox::Access::Protected:
        str = "protected";
        break;
    case clang::mrdox::Access::Private:
        str = "private";
        break;
    default:
        break;
    }
    return fmt::formatter<std::string>::format(str, ctx);
}

fmt::format_context::iterator
fmt::formatter<clang::mrdox::Reference>::
format(
    const clang::mrdox::Reference& r,
    fmt::format_context& ctx) const
{
    std::string str = fmt::format("Reference: type = {}", r.RefType);
    if(! r.Name.empty())
        str += fmt::format(", name = '{}'", std::string(r.Name));
    str += fmt::format(", ID = {}", r.id);
    return fmt::formatter<std::string>::format(std::move(str), ctx);
}

fmt::format_context::iterator
fmt::formatter<clang::mrdox::MemberRef>::
format(
    const clang::mrdox::MemberRef& r,
    fmt::format_context& ctx) const
{
    std::string str = fmt::format("MemberRef: access = {}, ID = {}",
        r.access, r.id);
    return fmt::formatter<std::string>::format(std::move(str), ctx);
}

fmt::format_context::iterator
fmt::formatter<clang::mrdox::Info>::
format(
    const clang::mrdox::Info& i,
    fmt::format_context& ctx) const
{
    std::string str = fmt::format("Info: type = {}", i.IT);
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
