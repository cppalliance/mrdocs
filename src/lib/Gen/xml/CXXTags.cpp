//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "CXXTags.hpp"
#include <mrdocs/Metadata/Info/Typedef.hpp>
#include <mrdocs/Metadata/Info/Record.hpp>

namespace clang::mrdocs::xml {

llvm::StringRef
getDefaultTagName(Info const& I) noexcept
{
    switch(I.Kind)
    {
#define INFO(PascalName, LowerName) \
    case InfoKind::PascalName: \
        return LowerName##TagName;
#include <mrdocs/Metadata/InfoNodesPascalAndCamel.inc>
    default:
        break;
    }
    MRDOCS_UNREACHABLE();
}


llvm::StringRef
getTagName(Info const& I) noexcept
{
    switch(I.Kind)
    {
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
    case InfoKind::Typedef:
        if(static_cast<TypedefInfo const&>(I).IsUsing)
            return namespaceAliasTagName;
        else
            return typedefTagName;
    default:
        return getDefaultTagName(I);
    }
    MRDOCS_UNREACHABLE();
}

} // clang::mrdocs::xml
