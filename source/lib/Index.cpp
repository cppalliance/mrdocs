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

#include "Index.hpp"
#include "Reduce.h"
#include "Representation.h"
#include "Namespace.hpp"
//#include <mrdox/Config.hpp>
//#include <llvm/Support/Error.h>
//#include <llvm/Support/Path.h>

namespace clang {
namespace mrdox {

// Dispatch function.
llvm::Expected<std::unique_ptr<Info>>
mergeInfos(std::vector<std::unique_ptr<Info>>& Values)
{
    if (Values.empty() || !Values[0])
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "no info values to merge");

    switch (Values[0]->IT) {
    case InfoType::IT_namespace:
        return reduce<NamespaceInfo>(Values);
    case InfoType::IT_record:
        return reduce<RecordInfo>(Values);
    case InfoType::IT_enum:
        return reduce<EnumInfo>(Values);
    case InfoType::IT_function:
        return reduce<FunctionInfo>(Values);
    case InfoType::IT_typedef:
        return reduce<TypedefInfo>(Values);
    default:
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "unexpected info type");
    }
}

// Order is based on the Name attribute:
// case insensitive order
bool
Index::
operator<(
    Index const& Other) const
{
    // Loop through each character of both strings
    for (unsigned I = 0; I < Name.size() && I < Other.Name.size(); ++I) {
        // Compare them after converting both to lower case
        int D = tolower(Name[I]) - tolower(Other.Name[I]);
        if (D == 0)
            continue;
        return D < 0;
    }
    // If both strings have the size it means they would be equal if changed to
    // lower case. In here, lower case will be smaller than upper case
    // Example: string < stRing = true
    // This is the opposite of how operator < handles strings
    if (Name.size() == Other.Name.size())
        return Name > Other.Name;
    // If they are not the same size; the shorter string is smaller
    return Name.size() < Other.Name.size();
}

void Index::sort() {
    llvm::sort(Children);
    for (auto& C : Children)
        C.sort();
}

} // mrdox
} // clang
