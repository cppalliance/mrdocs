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

#include <lib/Support/Debug.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata/DocComment.hpp>
#include <mrdocs/Metadata/DocComment/Inline/Parts.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <llvm/Support/Error.h>
#include <llvm/Support/Path.h>
#include <format>


namespace mrdocs {
namespace doc {

dom::String
toString(AdmonitionKind kind) noexcept
{
    switch(kind)
    {
    case AdmonitionKind::none:
        return "";
    case AdmonitionKind::note:
        return "note";
    case AdmonitionKind::tip:
        return "tip";
    case AdmonitionKind::important:
        return "important";
    case AdmonitionKind::caution:
        return "caution";
    case AdmonitionKind::warning:
        return "warning";
    default:
        MRDOCS_UNREACHABLE();
    }
}

dom::String
toString(ParamDirection kind) noexcept
{
    switch(kind)
    {
    case ParamDirection::none:
        return "";
    case ParamDirection::in:
        return "in";
    case ParamDirection::out:
        return "out";
    case ParamDirection::inout:
        return "inout";
    default:
        MRDOCS_UNREACHABLE();
    }
}

dom::String
toString(Parts kind) noexcept
{
    switch(kind)
    {
    case Parts::all:
        return "all";
    case Parts::brief:
        return "brief";
    case Parts::description:
        return "description";
    default:
        MRDOCS_UNREACHABLE();
    }
}

std::strong_ordering
operator<=>(Polymorphic<TextInline> const& lhs, Polymorphic<TextInline> const& rhs)
{
    MRDOCS_ASSERT(!lhs.valueless_after_move());
    MRDOCS_ASSERT(!rhs.valueless_after_move());
    if (lhs->Kind == rhs->Kind)
    {
        return visit(*lhs, detail::VisitCompareFn<TextInline>(*rhs));
    }
    return lhs->Kind <=> rhs->Kind;
}

} // doc

//------------------------------------------------

DocComment::DocComment() noexcept = default;

DocComment::DocComment(
    std::vector<Polymorphic<doc::Block>> blocks)
    : Document(std::move(blocks))
{
}

bool
DocComment::
operator!=(DocComment const& other) const noexcept
{
    return !(*this == other);
}

void
DocComment::
append(DocComment&& other)
{
    using std::ranges::find;
    using std::ranges::copy_if;
    using std::views::transform;

    // blocks
    for (auto&& block: other.Document)
    {
        Document.emplace_back(std::move(block));
    }
    // returns
    copy_if(other.returns, std::back_inserter(returns),
        [&](auto const& r)
        {
            return find(returns, r) == returns.end();
        });
    // params
    copy_if(other.params, std::back_inserter(params),
        [&](auto const& p)
        {
            auto namesAndDirection = transform(params, [](auto const& q)
            {
                return std::make_pair(std::string_view(q.name), q.direction);
            });
            auto el = std::make_pair(std::string_view(p.name), p.direction);
            return find(namesAndDirection, el) == namesAndDirection.end();
        });
    // tparams
    copy_if(other.tparams, std::back_inserter(tparams),
        [&](auto const& p)
        {
            auto names = transform(tparams, &doc::TParamBlock::name);
            return find(names, p.name) == names.end();
        });
    // exceptions
    copy_if(other.exceptions, std::back_inserter(exceptions),
        [&](auto const& e)
        {
            // e.exception.string
            auto exceptionRefs = transform(exceptions, &doc::ThrowsBlock::exception);
            static_assert(range_of<decltype(exceptionRefs), doc::ReferenceInline>);
            auto exceptionStrs = transform(exceptionRefs, &doc::ReferenceInline::literal);
            static_assert(range_of<decltype(exceptionStrs), std::string>);
            return find(exceptionStrs, e.exception.literal) == exceptionStrs.end();
        });
    // sees
    copy_if(other.sees, std::back_inserter(sees),
        [&](auto const& s)
        {
            return find(sees, s) == sees.end();
        });
    // preconditions
    copy_if(other.preconditions, std::back_inserter(preconditions),
        [&](auto const& p)
        {
            return find(preconditions, p) == preconditions.end();
        });
    // postconditions
    copy_if(other.postconditions, std::back_inserter(postconditions),
        [&](auto const& p)
        {
            return find(postconditions, p) == postconditions.end();
        });
}

} // mrdocs

