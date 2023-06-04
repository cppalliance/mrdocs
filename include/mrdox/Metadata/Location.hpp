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

#ifndef MRDOX_API_METADATA_LOCATION_HPP
#define MRDOX_API_METADATA_LOCATION_HPP

#include <mrdox/Platform.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>

namespace clang {
namespace mrdox {

struct Location
{
    int LineNumber = 0;             // Line number of this Location.
    llvm::SmallString<32> Filename; // File for this Location.
    bool IsFileInRootDir = false;   // Indicates if file is inside root directory

    //--------------------------------------------

    Location(
        int LineNumber = 0,
        llvm::StringRef Filename = llvm::StringRef(),
        bool IsFileInRootDir = false)
        : LineNumber(LineNumber)
        , Filename(Filename)
        , IsFileInRootDir(IsFileInRootDir)
    {
    }
};

} // mrdox
} // clang

#endif
