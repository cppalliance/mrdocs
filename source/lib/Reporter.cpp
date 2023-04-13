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
    if(failed_)
        return EXIT_FAILURE;
    if(errorCount_ > 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

llvm::StringRef
Reporter::
makeString(
    std::source_location const& loc)
{
    namespace path = llvm::sys::path;

    static thread_local llvm::SmallString<0> temp;

    llvm::StringRef fileName(loc.file_name());
    auto it = path::rbegin(fileName);
    auto const end = path::rend(fileName);
    if(it == end)
    {
        temp.clear();
        return {};
    }
    for(;;)
    {
        // VFALCO This assumes the directory
        //        layout of the source files.
        if( *it == "source" ||
            *it == "include")
        {
            temp.assign(
                it->data(),
                fileName.end());
            break;
        }
        ++it;
        if(it == end)
        {
            temp = fileName;
            break;
        }
    }
    path::remove_dots(temp, true);
    temp.push_back('(');
    temp.append(std::to_string(loc.line()));
    temp.push_back(')');
    return temp;
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
        *n++;
}

std::string&
Reporter::
temp_string()
{
    static thread_local std::string s;
    return s;
}

} // mrdox
} // clang
