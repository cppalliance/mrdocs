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
#include <mrdocs/Metadata/Symbol/Record.hpp>
#include <mrdocs/Metadata/Symbol/Typedef.hpp>
#include <mrdocs/Support/String.hpp>

namespace mrdocs::xml {

std::string
getDefaultTagName(Symbol const& I) noexcept
{
    switch(I.Kind)
    {
#define INFO(Type) \
    case SymbolKind::Type: \
        return toKebabCase(#Type) + "TagName";
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>
    default:
        break;
    }
    MRDOCS_UNREACHABLE();
}


std::string
getTagName(Symbol const& I) noexcept
{
    switch(I.Kind)
    {
    case SymbolKind::Record:
        switch(static_cast<RecordSymbol const&>(I).KeyKind)
        {
        case RecordKeyKind::Class:     return classTagName;
        case RecordKeyKind::Struct:    return structTagName;
        case RecordKeyKind::Union:     return unionTagName;
        default:
            break;
        }
        break;
    case SymbolKind::Typedef:
        if (static_cast<TypedefSymbol const&>(I).IsUsing)
        {
            return "namespace";
        }
        else
        {
            return "typedef";
        }
    default:
        return getDefaultTagName(I);
    }
    MRDOCS_UNREACHABLE();
}

} // mrdocs::xml
