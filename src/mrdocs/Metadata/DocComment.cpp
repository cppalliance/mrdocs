//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/Debug.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata/DocComment.hpp>
#include <mrdocs/Metadata/DocComment/Inline/Parts.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Support/Reflection/MergeReflectedType.hpp>
#include <llvm/Support/Error.h>
#include <llvm/Support/Path.h>
#include <format>


namespace mrdocs {
namespace doc {

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
    IsFunctionObject |= other.IsFunctionObject;

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

void
merge(DocComment& I, DocComment&& other)
{
    // Sticky flags: set once either declaration sets them. These are not part
    // of the reflected member set visited below.
    I.IsFunctionObject |= other.IsFunctionObject;
    I.IsSeeBelow |= other.IsSeeBelow;
    I.IsImplementationDefined |= other.IsImplementationDefined;

    // Two references name the same thing when they resolve to the same symbol,
    // or, while unresolved, name the same text.
    auto const sameRef =
        [](doc::ReferenceInline const& a, doc::ReferenceInline const& b)
        {
            if (a.id || b.id)
            {
                return a.id == b.id;
            }
            return a.literal == b.literal;
        };

    // Only keyed sections are unioned, so different declarations can each
    // contribute distinct entries and the key makes deduplication reliable:
    // params/tparams by name, throws/relates/related by the symbol they
    // reference. Everything else is free-form prose (the description, returns,
    // see-also, pre/postconditions, footnotes). There a value comparison is
    // meaningless (a small wording difference defeats it) and appending would
    // just accumulate near-duplicate sections, so the first declaration's is
    // kept. Unrecognized (e.g. newly added) list fields default to keep-first.
    describe::for_each(
        describe::describe_members<DocComment>{},
        [&](auto const& descriptor)
        {
            using Descriptor = std::decay_t<decltype(descriptor)>;
            auto& dst = I.*Descriptor::pointer;
            auto& src = other.*Descriptor::pointer;
            using Member = std::decay_t<decltype(dst)>;
            if constexpr (is_specialization_of_v<Member, Optional>)
            {
                // Singular field (the brief): keep the first one written.
                if (!dst)
                {
                    dst = std::move(src);
                }
            }
            else if constexpr (is_specialization_of_v<Member, std::vector>)
            {
                using Elem = typename Member::value_type;
                auto keyedUnion = [&](auto same)
                {
                    for (auto&& e : src)
                    {
                        if (std::ranges::none_of(dst,
                                [&](auto const& q) { return same(q, e); }))
                        {
                            dst.push_back(std::move(e));
                        }
                    }
                };
                if constexpr (
                    std::is_same_v<Elem, doc::ParamBlock> ||
                    std::is_same_v<Elem, doc::TParamBlock>)
                {
                    keyedUnion([](auto const& q, auto const& e)
                        { return q.name == e.name; });
                }
                else if constexpr (std::is_same_v<Elem, doc::ThrowsBlock>)
                {
                    keyedUnion([&](auto const& q, auto const& e)
                        { return sameRef(q.exception, e.exception); });
                }
                else if constexpr (std::is_same_v<Elem, doc::ReferenceInline>)
                {
                    keyedUnion([&](auto const& q, auto const& e)
                        { return sameRef(q, e); });
                }
                else if (dst.empty())
                {
                    // Prose section: keep the first declaration's.
                    dst = std::move(src);
                }
            }
        });
}

} // mrdocs
