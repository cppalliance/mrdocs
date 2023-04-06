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

#include <mrdox/ErrorCode.hpp>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Path.h>
#include <string_view>

namespace clang {
namespace mrdox {

static
llvm::StringRef
prettyFilePath(
    llvm::StringRef fullPath,
    llvm::SmallVectorImpl<char>& temp)
{
    namespace path = llvm::sys::path;

    auto it = path::rbegin(fullPath);
    auto const end = path::rend(fullPath);
    if(it == end)
    {
        temp.clear();
        return {};
    }
    for(;;)
    {
        if( *it == "source" ||
            *it == "include")
        {
            temp.assign(
                it->data(),
                fullPath.end());
            break;
        }
        ++it;
        if(it == end)
        {
            temp.assign(
                fullPath.begin(),
                fullPath.end());
            break;
        }
    }
    path::remove_dots(temp, true);
#ifdef _WIN32
    for(auto& c : temp)
        if( c == '\\')
            c = '/';
#endif
    return llvm::StringRef(
        temp.begin(), temp.size());
}


void
ErrorCode::
write(
    llvm::raw_ostream& os) const
{
    llvm::SmallString<256> temp;
    os <<
        prettyFilePath(loc_.file_name(), temp) << "(" <<
        loc_.line() << "): " <<
        message() << "\n";
}

} // mrdox
} // clang
