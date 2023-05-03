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

#include "CXXTags.hpp"
#include <mrdox/Metadata/Typedef.hpp>

namespace clang {
namespace mrdox {
namespace xml {

llvm::StringRef
getTagName(Info const& I) noexcept
{
    switch(I.IT)
    {
    case InfoType::IT_namespace:
        return namespaceTagName;
    case InfoType::IT_record:
        switch(static_cast<RecordInfo const&>(I).TagType)
        {
        case TagTypeKind::TTK_Class:  return classTagName;
        case TagTypeKind::TTK_Struct: return structTagName;
        case TagTypeKind::TTK_Union:  return unionTagName;
        default:
            break;
        }
        break;
    case InfoType::IT_function:
        return functionTagName;
    case InfoType::IT_typedef:
        if(static_cast<TypedefInfo const&>(I).IsUsing)
            return aliasTagName;
        else
            return typedefTagName;
        break;
    case InfoType::IT_enum:
        return enumTagName;
    case InfoType::IT_variable: 
        return variableTagName;
    default:
        break;
    }
    Assert(false);
    return "(unknown)";
}

} // xml
} // mrdox
} // clang
