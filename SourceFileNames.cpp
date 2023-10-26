//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

/*
    This file goes in the root of a repository. We
    use the C++ source_location information in order
    to the full path to the repository, so that we
    can strip this prefix later and generate pretty
    source filenames for diagnostic output.
*/

namespace SourceFileNames {

char const*
getFileName(char const* fileName) noexcept
{
    auto it  = fileName;
    auto it0 = fileName;
    auto it1 = __FILE__;
    while(*it0 && *it1)
    {
        if(*it0 != *it1)
            break;
    #ifdef _WIN32
        if(*it0++ == '\\')
            it = it0;
    #else
        if(*it0++ == '/')
            it = it0;
    #endif
        ++it1;
    }
    return it;
}

} // SourceFileNames

