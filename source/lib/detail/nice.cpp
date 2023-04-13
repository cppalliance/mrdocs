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

#include <mrdox/detail/nice.hpp>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdox {
namespace detail {

llvm::StringRef
nice(
    std::source_location loc)
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

} // detail
} // mrdox
} // clang
