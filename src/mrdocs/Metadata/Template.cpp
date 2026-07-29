//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Template.hpp>

namespace mrdocs {

std::strong_ordering
operator<=>(Polymorphic<TParam> const& lhs, Polymorphic<TParam> const& rhs)
{
    MRDOCS_ASSERT(!lhs.valueless_after_move());
    MRDOCS_ASSERT(!rhs.valueless_after_move());
    if (lhs->Kind == rhs->Kind)
    {
        return visit(*lhs, detail::VisitCompareFn<TParam>(*rhs));
    }
    return lhs->Kind <=> rhs->Kind;
}


std::strong_ordering
operator<=>(Polymorphic<TArg> const& lhs, Polymorphic<TArg> const& rhs)
{
    MRDOCS_ASSERT(!lhs.valueless_after_move());
    MRDOCS_ASSERT(!rhs.valueless_after_move());
    if (lhs->Kind == rhs->Kind)
    {
        return visit(*lhs, detail::VisitCompareFn<TArg>(*rhs));
    }
    return lhs->Kind <=> rhs->Kind;
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

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TArg const& I,
    DomCorpus const* domCorpus)
{
    visit(I, [&]<typename T>(T const& t) {
        v = dom::LazyObject(t, domCorpus);
    });
}

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TParam const& I,
    DomCorpus const* domCorpus)
{
    visit(I, [&]<typename T>(T const& t) {
        v = dom::LazyObject(t, domCorpus);
    });
}

void
merge(TemplateInfo& I, TemplateInfo&& Other)
{
    std::size_t const pn = std::min(I.Params.size(), Other.Params.size());
    for (std::size_t i = 0; i < pn; ++i)
    {
        MRDOCS_ASSERT(!I.Params[i].valueless_after_move());
        if (I.Params[i]->Kind != Other.Params[i]->Kind)
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
        MRDOCS_ASSERT(!I.Args[i].valueless_after_move());
        if (I.Args[i]->Kind != Other.Args[i]->Kind)
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

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TemplateInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs
