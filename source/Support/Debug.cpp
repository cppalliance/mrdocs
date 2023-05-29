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
#include <atomic>
#include <memory>

#ifdef MRDOX_HAS_CXX20_FORMAT
    #include "Support/Radix.hpp"
    #include <mrdox/Metadata/Access.hpp>
    #include <mrdox/Metadata/Reference.hpp>
    #include <mrdox/Metadata/Symbols.hpp>
    #include <mrdox/Metadata/Info.hpp>
#endif

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

#ifdef MRDOX_HAS_CXX20_FORMAT
    std::format_context::iterator
    std::formatter<clang::mrdox::SymbolID>::
    format(
        const clang::mrdox::SymbolID& s,
        std::format_context& ctx) const
    {
        return s == clang::mrdox::SymbolID::zero ?
            std::format_to(ctx.out(), "<empty SymbolID>") :
            std::format_to(ctx.out(), "{}", clang::mrdox::toBase64(s));;
    }

    std::format_context::iterator
    std::formatter<clang::mrdox::OptionalSymbolID>::
    format(
        const clang::mrdox::OptionalSymbolID& s,
        std::format_context& ctx) const
    {
        // what if it's null?
        return std::formatter<clang::mrdox::SymbolID>::format(*s, ctx);
    }

    std::format_context::iterator
    std::formatter<clang::mrdox::InfoType>::
    format(
        clang::mrdox::InfoType t,
        std::format_context& ctx) const
    {
        return std::format_to(ctx.out(), "{}", to_string(t));
    }

    std::format_context::iterator
    std::formatter<clang::mrdox::Access>::
    format(
        clang::mrdox::Access a,
        std::format_context& ctx) const
    {
        switch(a)
        {
        case clang::mrdox::Access::Public:
            return std::format_to(ctx.out(), "public");
        case clang::mrdox::Access::Protected:
            return std::format_to(ctx.out(), "protected");
        case clang::mrdox::Access::Private:
            return std::format_to(ctx.out(), "private");
        default:
            return std::format_to(ctx.out(), "<unknown Access>");
        }
    }

    std::format_context::iterator
    std::formatter<clang::mrdox::Reference>::
    format(
        const clang::mrdox::Reference& r,
        std::format_context& ctx) const
    {
        auto itr = std::format_to(ctx.out(), "Reference: type = {}", to_string(r.RefType));
        if(! r.Name.empty())
            itr = std::format_to(itr, ", name = '{}'", std::string_view(r.Name.data(), r.Name.size()));
        itr = std::format_to(itr, ", ID = ");
        return std::formatter<clang::mrdox::SymbolID>().format(r.id, ctx);
    }

    std::format_context::iterator
    std::formatter<clang::mrdox::MemberRef>::
    format(
        const clang::mrdox::MemberRef& r,
        std::format_context& ctx) const
    {
        auto itr = std::format_to(ctx.out(), "MemberRef: access = ");
        itr = std::formatter<clang::mrdox::Access>().format(r.access, ctx);
        std::format_to(itr, ", ID = ");
        return std::formatter<clang::mrdox::SymbolID>().format(r.id, ctx);
    }

    std::format_context::iterator
    std::formatter<clang::mrdox::Info>::
    format(
        const clang::mrdox::Info& i,
        std::format_context& ctx) const
    {
        auto itr = std::format_to(ctx.out(), "Info: type = {}", to_string(i.IT));
        if(! i.Name.empty())
            itr = std::format_to(itr, ", name = '{}'", std::string_view(i.Name.data(), i.Name.size()));
        itr = std::format_to(itr, ", ID = ");
        itr = std::formatter<clang::mrdox::SymbolID>().format(i.id, ctx);
        if(! i.Namespace.empty())
        {
            itr = std::format_to(itr, ", namespace = {}",
                                 std::string_view(i.Namespace[0].Name.data(),
                                                  i.Namespace[0].Name.size()));
            std::format_to(itr, ", namespace = ");


            for(std::size_t idx = 1; idx < i.Namespace.size(); ++idx)
                itr = std::format_to(itr, "::{}",
                                     std::string_view(i.Namespace[idx].Name.data(),
                                                      i.Namespace[idx].Name.size()));
        }
        return itr;
    }
#endif
