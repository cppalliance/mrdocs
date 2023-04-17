//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Debug.hpp>

#if defined(_MSC_VER) && ! defined(NDEBUG)

#include <iostream>
#include <sstream>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace clang {
namespace mrdox {

namespace {

class debug_streambuf
    : public std::stringbuf
{
    std::ostream& os_;
    bool dbg_;

    void
    write(char const* s)
    {
        if(dbg_)
            ::OutputDebugStringA(s);
        os_ << s;
    }

public:
    explicit
    debug_streambuf(
        std::ostream& os)
        : os_(os)
        , dbg_(::IsDebuggerPresent() != 0)
    {
    }

    ~debug_streambuf()
    {
        sync();
    }

    int sync() override
    {
        write(this->str().c_str());
        this->str("");
        return 0;
    }
};

} // (anon)

void
debugEnableRedirecton()
{
    static debug_streambuf out(std::cout);
    static debug_streambuf err(std::cerr);

    std::cout.rdbuf(&out);
    std::cerr.rdbuf(&err);
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

void debugEnableRedirecton()
{
}

void debugEnableHeapChecking()
{
}

} // mrdox
} // clang

#endif
