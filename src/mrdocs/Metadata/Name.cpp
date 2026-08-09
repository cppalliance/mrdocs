//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <span>

namespace mrdocs {

static
void
writeTo(
    std::string& result,
    auto&&... args)
{
    (result += ... += args);
}

std::strong_ordering
Name::
operator<=>(Name const& other) const
{
    if (this == &other)
    {
        return std::strong_ordering::equal;
    }
    if (Kind != other.Kind)
    {
        return Kind <=> other.Kind;
    }
    if (Identifier != other.Identifier)
    {
        return Identifier <=> other.Identifier;
    }
    if (bool(Prefix) != bool(other.Prefix))
    {
        return bool(Prefix) <=> bool(other.Prefix);
    }
    if (Prefix && other.Prefix)
    {
        return *Prefix <=> *other.Prefix;
    }
    return std::strong_ordering::equal;
}

std::strong_ordering
operator<=>(Polymorphic<Name> const& lhs, Polymorphic<Name> const& rhs)
{
    MRDOCS_ASSERT(!lhs.valueless_after_move());
    MRDOCS_ASSERT(!rhs.valueless_after_move());
    if (lhs->Kind == rhs->Kind)
    {
        if (lhs->isIdentifier())
        {
            return *lhs <=> *rhs;
        }
        return visit(*lhs, detail::VisitCompareFn<Name>(*rhs));
    }
    return lhs->Kind <=> rhs->Kind;
}

// Defined out of line, not inline in a header: the body resolves `lhs <=>
// rhs`, which drives the visitor comparison and `has_describe_kinds<Name>`.
// This translation unit includes Name.hpp, so the kinds are already
// registered here; a header-inline body could be parsed before the
// registration and cache the trait as false (Clang <= 19).
bool
operator==(Polymorphic<Name> const& lhs, Polymorphic<Name> const& rhs)
{
    return std::is_eq(lhs <=> rhs);
}

static
void
toStringImpl(
    std::string& result,
    Name const& N)
{
    if (N.Prefix)
    {
        toStringImpl(result, **N.Prefix);
        writeTo(result, "::");
    }

    writeTo(result, N.Identifier);

    if (!N.isSpecialization())
    {
        return;
    }
    auto const& NN = N.asSpecialization();
    std::span const targs = NN.TemplateArgs;
    writeTo(result, '<');
    if(! targs.empty())
    {
        auto targ_writer =
            [&]<typename U>(U const& u)
            {
                if constexpr(U::isType())
                {
                    MRDOCS_ASSERT(!u.Type.valueless_after_move());
                    writeTo(result, toString(*u.Type));
                }
                if constexpr(U::isConstant())
                {
                    writeTo(result, u.Value.Written);
                }
                if constexpr(U::isTemplate())
                {
                    writeTo(result, u.Name);
                }
                if(u.IsPackExpansion)
                    writeTo(result, "...");
            };
        visit(*targs.front(), targ_writer);
        for(auto const& arg : targs.subspan(1))
        {
            writeTo(result, ", ");
            visit(*arg, targ_writer);
        }
    }
    writeTo(result, '>');
}

std::string
toString(Name const& N)
{
    std::string result;
    toStringImpl(result, N);
    return result;
}


} // mrdocs
