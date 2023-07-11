//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Metadata/Template.hpp>

namespace clang {
namespace mrdox {

#if 0
TArg::
TArg(
    std::string&& value)
    : Value(std::move(value))
{
}
#endif

dom::String
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
        MRDOX_UNREACHABLE();
    }
}

dom::String
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
        MRDOX_UNREACHABLE();
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
        MRDOX_UNREACHABLE();
    }
}

} // mrdox
} // clang
