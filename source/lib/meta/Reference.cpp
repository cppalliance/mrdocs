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

#include <mrdox/meta/Info.hpp>
#include <mrdox/meta/Reference.hpp>

namespace clang {
namespace mrdox {

llvm::SmallString<64>
Reference::
getRelativeFilePath(
    llvm::StringRef const& CurrentPath) const
{
    return calculateRelativeFilePath(
        RefType, Path, Name, CurrentPath);
}

/*
llvm::SmallString<16>
Reference::
getFileBaseName() const
{
    if (RefType == InfoType::IT_namespace)
        return llvm::SmallString<16>("index");
    return Name;
}
*/

bool
Reference::
canMerge(
    Reference const& Other)
{
    return
        RefType == Other.RefType &&
        USR == Other.USR;
}

void
Reference::
merge(
    Reference&& Other)
{
    assert(canMerge(Other));
    if (Name.empty())
        Name = Other.Name;
    if (Path.empty())
        Path = Other.Path;
}

} // mrdox
} // clang
