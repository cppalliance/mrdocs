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

void
TParam::
destroy() const noexcept
{
    switch(Kind)
    {
    case TParamKind::Type:
        return Variant_.Type.~TypeTParam();
    case TParamKind::NonType:
        return Variant_.NonType.~NonTypeTParam();
    case TParamKind::Template:
        return Variant_.Template.~TemplateTParam();
    default:
        return;
    }
}

TParam::
TParam(
    TParam&& other) noexcept
    : Name(std::move(other.Name))
    , IsParameterPack(other.IsParameterPack)
{
    construct(std::move(other));
}

TParam::
TParam(
    std::string&& name,
    bool is_pack)
    : Name(std::move(name))
    , IsParameterPack(is_pack)
{
}

TParam::
~TParam()
{
    destroy();
}

TParam&
TParam::
operator=(TParam&& other) noexcept
{
    destroy();
    construct(std::move(other));
    Name = std::move(other.Name);
    IsParameterPack = other.IsParameterPack;
    return *this;
}

TArg::
TArg(
    std::string&& value)
    : Value(std::move(value))
{
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
        // kind should never be None
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
