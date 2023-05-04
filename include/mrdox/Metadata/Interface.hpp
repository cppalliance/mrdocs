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

#ifndef MRDOX_METADATA_INTERFACE_HPP
#define MRDOX_METADATA_INTERFACE_HPP

#include <mrdox/Platform.hpp>
#include <vector>

namespace clang {
namespace mrdox {

struct Interface
{
    struct Function
    {
        FunctionInfo const* I;
        AccessSpecifier Access;
    };

    std::vector<Function> Functions;

    MRDOX_DECL friend Interface
    makeInterface(
        RecordInfo const& I,
        Corpus const& corpus);
};

/** Return the composite interface for a record.

    @return The interface.

    @param I The record to use.

    @param corpus The complete metadata.
*/
MRDOX_DECL
Interface
makeInterface(
    RecordInfo const& I,
    Corpus const& corpus);

} // mrdox
} // clang

#endif
