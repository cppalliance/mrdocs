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

#include "Path.hpp"
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdox {

//------------------------------------------------

std::string
makeFullPath(
    std::string_view pathName,
    std::string_view workingDir)
{
    namespace path = llvm::sys::path;

    llvm::SmallString<0> result;

    if(! path::is_absolute(pathName))
    {
        result = workingDir;
        path::append(result, path::Style::posix, pathName);
        path::remove_dots(result, true, path::Style::posix);
        return std::string(result);
    }

    result = pathName;
    path::remove_dots(result, true);
    convert_to_slash(result);
    return std::string(result);
}


//------------------------------------------------

llvm::StringRef
convert_to_slash(
    llvm::SmallVectorImpl<char> &path,
    llvm::sys::path::Style style)
{
    if (! llvm::sys::path::is_style_posix(style))
        std::replace(path.begin(), path.end(), '\\', '/');
    return llvm::StringRef(path.data(), path.size());
}

void
makeDirsy(
    llvm::SmallVectorImpl<char>& s,
    llvm::sys::path::Style style)
{
    namespace path = llvm::sys::path;

    if( ! s.empty() &&
        ! path::is_separator(s.back(), style))
    {
        auto const sep = path::get_separator(style);
        s.insert(s.end(), sep.begin(), sep.end());
    }
}

} // mrdox
} // clang
