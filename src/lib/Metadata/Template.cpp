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
    case TArgKind::Constant:
        return "constant";
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
    case TParamKind::Constant:
        return "constant";
    case TParamKind::Template:
        return "template";
    default:
        MRDOCS_UNREACHABLE();
    }
}

std::strong_ordering
TParam::
operator<=>(TParam const&) const = default;

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

std::strong_ordering
operator<=>(Polymorphic<TParam> const& lhs, Polymorphic<TParam> const& rhs)
{
    if (!lhs.valueless_after_move() && !rhs.valueless_after_move())
    {
        if (lhs->Kind == rhs->Kind)
        {
            return visit(*lhs, detail::VisitCompareFn<TParam>(*rhs));
        }
        return lhs->Kind <=> rhs->Kind;
    }
    return lhs.valueless_after_move() ?
               std::strong_ordering::less :
               std::strong_ordering::greater;
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

std::strong_ordering
operator<=>(Polymorphic<TArg> const& lhs, Polymorphic<TArg> const& rhs)
{
    if (!lhs.valueless_after_move() && !rhs.valueless_after_move())
    {
        if (lhs->Kind == rhs->Kind)
        {
            return visit(*lhs, detail::VisitCompareFn<TArg>(*rhs));
        }
        return lhs->Kind <=> rhs->Kind;
    }
    return lhs.valueless_after_move() ?
               std::strong_ordering::less :
               std::strong_ordering::greater;
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
            MRDOCS_ASSERT(!t.Type.valueless_after_move());
            result += toString(*t.Type);
        }
        if constexpr(T::isConstant())
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
        if constexpr(T::isConstant())
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

std::strong_ordering
TypeTParam::
operator<=>(TypeTParam const&) const = default;

std::strong_ordering
ConstantTParam::
operator<=>(ConstantTParam const&) const = default;

std::strong_ordering
TemplateTParam::
operator<=>(TemplateTParam const& other) const
{
    if (auto const r = Params.size() <=> other.Params.size();
        !std::is_eq(r))
    {
        return r;
    }
    for (std::size_t i = 0; i < Params.size(); ++i)
    {
        if (auto const r = Params[i] <=> other.Params[i];
            !std::is_eq(r))
        {
            return r;
        }
    }
    return std::strong_ordering::equal;
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
            io.map("default", **t.Default);
        }
        if constexpr(T::isType())
        {
            io.map("key", t.KeyKind);
            if (t.Constraint)
            {
                io.map("constraint", t.Constraint);
            }
        }
        if constexpr(T::isConstant())
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

std::strong_ordering
TemplateInfo::
operator<=>(TemplateInfo const& other) const {
    if (auto const r = Args.size() <=> other.Args.size();
        !std::is_eq(r))
    {
        return r;
    }
    if (auto const r = Params.size() <=> other.Params.size();
        !std::is_eq(r))
    {
        return r;
    }
    return std::tie(Args, Params, Requires, Primary) <=>
        std::tie(Args, Params, other.Requires, other.Primary);
}

void
merge(TemplateInfo& I, TemplateInfo&& Other)
{
    std::size_t const pn = std::min(I.Params.size(), Other.Params.size());
    for (std::size_t i = 0; i < pn; ++i)
    {
        if (I.Params[i].valueless_after_move() ||
            I.Params[i]->Kind != Other.Params[i]->Kind)
        {
            I.Params[i] = std::move(Other.Params[i]);
        }
        else
        {
            if (I.Params[i]->Name.empty())
            {
                I.Params[i]->Name = std::move(Other.Params[i]->Name);
            }
            if (!I.Params[i]->Default)
            {
                I.Params[i]->Default = std::move(Other.Params[i]->Default);
            }
        }
    }
    if (Other.Params.size() > pn)
    {
        I.Params.insert(
            I.Params.end(),
            std::make_move_iterator(Other.Params.begin() + pn),
            std::make_move_iterator(Other.Params.end()));
    }

    std::size_t const an = std::min(I.Args.size(), Other.Args.size());
    for (std::size_t i = 0; i < an; ++i)
    {
        if (I.Args[i].valueless_after_move() ||
            I.Args[i]->Kind != Other.Args[i]->Kind)
        {
            I.Args[i] = std::move(Other.Args[i]);
        }
    }
    if (Other.Args.size() > an)
    {
        I.Args.insert(
            I.Args.end(),
            std::make_move_iterator(Other.Args.begin() + an),
            std::make_move_iterator(Other.Args.end()));
    }

    if (I.Requires.Written.empty())
    {
        I.Requires = std::move(Other.Requires);
    }

    if (I.Primary == SymbolID::invalid)
    {
        I.Primary = Other.Primary;
    }
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
    if (I.Primary != SymbolID::invalid)
    {
        io.map("primary", I.Primary);
    }
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
