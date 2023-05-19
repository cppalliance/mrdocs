//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Support/Debug.hpp"
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
