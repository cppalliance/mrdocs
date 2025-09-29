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

#include <mrdocs/Metadata/DocComment/Inline.hpp>
#include <mrdocs/Support/Algorithm.hpp>

namespace mrdocs::doc {

std::strong_ordering
operator<=>(Polymorphic<Inline> const& lhs, Polymorphic<Inline> const& rhs)
{
    return CompareDerived(lhs, rhs);
}

void
ltrim(Polymorphic<Inline>& el)
{
    // An inline element can contain:
    // 1. nothing: the ones that represent whitespace should be removed
    // 2. literal text: the initial whitespace should be removed
    // 3. other inlines: we should remove all whitespace-only inlines
    visit(*el, []<class InlineTy>(InlineTy& N)
    {
        if constexpr (std::derived_from<InlineTy, InlineContainer>)
        {
            ltrim(N.asInlineContainer());
        }
        else if constexpr (requires { { N.literal } -> std::same_as<std::string>; })
        {
            std::string& text = N.literal;
            text = mrdocs::ltrim(text);
        }
    });
}

void
rtrim(Polymorphic<Inline>& el)
{
    // Same as ltrim but for the end of the element
    visit(*el, []<class InlineTy>(InlineTy& N)
    {
        if constexpr (std::derived_from<InlineTy, InlineContainer>)
        {
            rtrim(N.asInlineContainer());
        }
        else if constexpr (requires { { N.literal } -> std::same_as<std::string>; })
        {
            std::string& text = N.literal;
            text = mrdocs::rtrim(text);
        }
    });
}

MRDOCS_DECL
bool
isEmpty(Polymorphic<Inline> const& el)
{
    return visit(*el, []<class InlineTy>(InlineTy const& N) -> bool
    {
        if constexpr (std::derived_from<InlineTy, InlineContainer>)
        {
            return N.children.empty();
        }
        else if constexpr (requires { { N.literal } -> std::same_as<std::string const&>; })
        {
            return N.literal.empty();
        }
        else
        {
            return is_one_of(
                N.Kind,
                { InlineKind::LineBreak, InlineKind::SoftBreak });
        }
    });
}


} // mrdocs::doc