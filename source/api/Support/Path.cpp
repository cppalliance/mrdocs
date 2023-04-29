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

namespace clang {
namespace mrdox {

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
