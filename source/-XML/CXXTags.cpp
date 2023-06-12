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
    switch(I.Kind)
    {
    case InfoKind::Namespace:
        return namespaceTagName;
    case InfoKind::Record:
        switch(static_cast<RecordInfo const&>(I).KeyKind)
        {
        case RecordKeyKind::Class:     return classTagName;
        case RecordKeyKind::Struct:    return structTagName;
        case RecordKeyKind::Union:     return unionTagName;
        default:
            break;
        }
        break;
    case InfoKind::Function:
        return functionTagName;
    case InfoKind::Typedef:
        if(static_cast<TypedefInfo const&>(I).IsUsing)
            return aliasTagName;
        else
            return typedefTagName;
        break;
    case InfoKind::Enum:
        return enumTagName;
    case InfoKind::Variable:
        return varTagName;
    default:
        break;
    }
    MRDOX_ASSERT(false);
    return "(unknown)";
}

} // xml
} // mrdox
} // clang
