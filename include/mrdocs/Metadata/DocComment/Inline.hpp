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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/DocComment/Inline/CodeInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/CopyDetailsInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/EmphInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/FootnoteReferenceInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/HighlightInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/ImageInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/LineBreakInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/LinkInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/MathInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/ReferenceInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/SoftBreakInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/StrikethroughInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/StrongInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/SubscriptInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/SuperscriptInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/TextInline.hpp>
#include <mrdocs/Support/Concepts.hpp>
#include <mrdocs/Support/Visitor.hpp>
#include <compare>


namespace mrdocs::doc {

/** Visit an inline.

    @param el The inline element to visit.
    @param fn The function to call for each inline.
    @param args Additional arguments to pass to the function.
    @return The result of calling the function.
 */
template<
    class InlineTy,
    class Fn,
    class... Args>
    requires std::derived_from<InlineTy, Inline>
decltype(auto)
visit(
    InlineTy& el,
    Fn&& fn,
    Args&&... args)
{
    auto visitor = makeVisitor<Inline>(
        el, std::forward<Fn>(fn),
        std::forward<Args>(args)...);
    switch(el.Kind)
    {
        #define INFO(Type) case InlineKind::Type: \
            return visitor.template visit<Type##Inline>();
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Traverse a list of inlines.

    @param list The list of texts to traverse.
    @param f The function to call for each text.
    @param args Additional arguments to pass to the function.
 */
template<class F, class T, class... Args>
requires std::derived_from<T, Inline>
void traverse(
    std::vector<std::unique_ptr<T>> const& list,
    F&& f, Args&&... args)
{
    for(auto const& el : list)
        visit(*el,
            std::forward<F>(f),
            std::forward<Args>(args)...);
}

/** Map the Polymorphic Inline as a @ref dom::Value object.

    @param io The output parameter to receive the dom::Object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO, polymorphic_storage_for<Inline> InlineTy>
void
tag_invoke(
    dom::ValueFromTag,
    IO& io,
    InlineTy const& I,
    DomCorpus const* domCorpus)
{
    visit(*I, [&](auto const& U)
    {
        tag_invoke(
            dom::ValueFromTag{},
            io,
            U,
            domCorpus);
    });
}

MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<Inline> const& lhs, Polymorphic<Inline> const& rhs);

inline
bool
operator==(Polymorphic<Inline> const& lhs, Polymorphic<Inline> const& rhs) {
    return std::is_eq(lhs <=> rhs);
}

/** Removes leading whitespace from the inline element.

    @param el The Polymorphic<Inline> to trim.
    @return void
 */
MRDOCS_DECL
void
ltrim(Polymorphic<Inline>& el);

/** Removes trailing whitespace from the inline element.

    @param el The Polymorphic<Inline> to trim.
    @return void
 */
MRDOCS_DECL
void
rtrim(Polymorphic<Inline>& el);

/** Removes leading and trailing whitespace from the inline element.

    @param el The Polymorphic<Inline> to trim.
    @return void
 */
inline
void
trim(Polymorphic<Inline>& el)
{
    ltrim(el);
    rtrim(el);
}

/** Determine if the inline is empty

    This determines if the inline is considered to
    have no content for the purposes of trimming.

 */
MRDOCS_DECL
bool
isEmpty(Polymorphic<Inline> const& el);

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_HPP
