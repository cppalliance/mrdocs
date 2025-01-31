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

#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Template.hpp>

namespace clang::mrdocs {

std::string_view
toString(TArgKind kind) noexcept
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
    TArg const& arg) noexcept
{
    return visit(arg, 
        []<typename T>(T const& t)
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

template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    TArg const& I,
    DomCorpus const*)
{
    io.map("kind", toString(I.Kind));
    io.map("is-pack", I.IsPackExpansion);
    visit(I, [&io]<typename T>(T const& t) {
        if constexpr(T::isType())
        {
            io.map("type", t.Type);
        }
        if constexpr(T::isNonType())
        {
            io.map("value", t.Value.Written);
        }
        if constexpr(T::isTemplate())
        {
            io.map("name", t.Name);
            io.map("template", t.Template);
        }
    });
}


void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TArg const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    TParam const& I,
    DomCorpus const* domCorpus)
{
    io.map("kind", toString(I.Kind));
    io.map("name", dom::stringOrNull(I.Name));
    io.map("is-pack", I.IsParameterPack);
    visit(I, [domCorpus, &io]<typename T>(T const& t) {
        if(t.Default)
        {
            io.map("default", t.Default);
        }
        if constexpr(T::isType())
        {
            io.map("key", t.KeyKind);
            if (t.Constraint)
            {
                io.map("constraint", t.Constraint);
            }
        }
        if constexpr(T::isNonType())
        {
            io.map("type", t.Type);
        }
        if constexpr(T::isTemplate())
        {
            io.map("params", dom::LazyArray(t.Params, domCorpus));
        }
    });
}

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TParam const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    TemplateInfo const& I,
    DomCorpus const* domCorpus)
{
    io.defer("kind", [&] {
        return toString(I.specializationKind());
    });
    io.map("primary", I.Primary);
    io.map("params", dom::LazyArray(I.Params, domCorpus));
    io.map("args", dom::LazyArray(I.Args, domCorpus));
    io.map("requires", dom::stringOrNull(I.Requires.Written));
}

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TemplateInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs
