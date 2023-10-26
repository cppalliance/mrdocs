//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/Template.hpp>

namespace clang {
namespace mrdocs {

std::string_view
toString(
    TArgKind kind) noexcept
{
    switch(kind)
    {
    case TArgKind::Type:
        return "type";
    case TArgKind::NonType:
        return "non-type";
    case TArgKind::Template:
        return "template";
    default:
        MRDOCS_UNREACHABLE();
    }
}

std::string_view
toString(
    TParamKind kind) noexcept
{
    switch(kind)
    {
    case TParamKind::Type:
        return "type";
    case TParamKind::NonType:
        return "non-type";
    case TParamKind::Template:
        return "template";
    default:
        MRDOCS_UNREACHABLE();
    }
}

std::string_view
toString(
    TParamKeyKind kind) noexcept
{
    switch(kind)
    {
    case TParamKeyKind::Class:
        return "class";
    case TParamKeyKind::Typename:
        return "typename";
    default:
        MRDOCS_UNREACHABLE();
    }
}

std::string_view
toString(
    TemplateSpecKind kind)
{
    switch(kind)
    {
    case TemplateSpecKind::Primary:
        return "primary";
    case TemplateSpecKind::Explicit:
        return "explicit";
    case TemplateSpecKind::Partial:
        return "partial";
    default:
        MRDOCS_UNREACHABLE();
    }
}

std::string
toString(
    const TArg& arg) noexcept
{
    return visit(arg, 
        []<typename T>(const T& t)
    {
        std::string result;
        if constexpr(T::isType())
        {
            if(t.Type)
                result += toString(*t.Type);
        }
        if constexpr(T::isNonType())
        {
            result += t.Value.Written;
        }
        if constexpr(T::isTemplate())
        {
            result += t.Name;
        }

        if(t.IsPackExpansion)
            result += "...";
        return result;
    });
}

} // mrdocs
} // clang
