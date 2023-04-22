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

//
// This file defines the merging of different types of infos. The data in the
// calling Info is preserved during a merge unless that field is empty or
// default. In that case, the data from the parameter Info is used to replace
// the empty or default data.
//
// For most fields, the first decl seen provides the data. Exceptions to this
// include the location and description fields, which are collections of data on
// all decls related to a given definition. All other fields are ignored in new
// decls unless the first seen decl didn't, for whatever reason, incorporate
// data on that field (e.g. a forward declared class wouldn't have information
// on members on the forward declaration, but would have the class name).
//

#include <mrdox/Debug.hpp>
#include <mrdox/meta/Enum.hpp>

namespace clang {
namespace mrdox {

void
EnumInfo::
merge(
    EnumInfo&& Other)
{
    Assert(canMerge(Other));
    if (!Scoped)
        Scoped = Other.Scoped;
    if (Members.empty())
        Members = std::move(Other.Members);
    SymbolInfo::merge(std::move(Other));
}

} // mrdox
} // clang
