//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Reporter.hpp>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Path.h>
#include <string>
#include <string_view>

namespace clang {
namespace mrdox {

//------------------------------------------------
//
// Reporter
//
//------------------------------------------------

int
Reporter::
getExitCode() const noexcept
{
    if(errorCount_ > 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

//------------------------------------------------

void
Reporter::
threadSafePrint(
    llvm::raw_fd_ostream& os,
    llvm::StringRef s,
    std::size_t* n)

{
    std::lock_guard<llvm::sys::Mutex> lock(m_);
    os << s << '\n';
    if(n)
        (*n)++;
}

std::string&
Reporter::
temp_string()
{
    static thread_local std::string s;
    return s;
}

//------------------------------------------------

void
Reporter::
reportError()
{
    std::lock_guard<llvm::sys::Mutex> lock(m_);
    ++errorCount_;
}

} // mrdox
} // clang
