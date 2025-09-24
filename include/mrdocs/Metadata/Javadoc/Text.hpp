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

#ifndef MRDOCS_API_METADATA_JAVADOC_TEXT_HPP
#define MRDOCS_API_METADATA_JAVADOC_TEXT_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Javadoc/Text/CopyDetails.hpp>
#include <mrdocs/Metadata/Javadoc/Text/Link.hpp>
#include <mrdocs/Metadata/Javadoc/Text/Reference.hpp>
#include <mrdocs/Metadata/Javadoc/Text/Style.hpp>
#include <mrdocs/Metadata/Javadoc/Text/Styled.hpp>
#include <mrdocs/Metadata/Javadoc/Text/TextBase.hpp>
#include <mrdocs/Support/Concepts.hpp>
#include <mrdocs/Support/Visitor.hpp>
#include <compare>

namespace clang::mrdocs::doc {

/** Visit a text.

    @param text The text to visit.
    @param fn The function to call for each text.
    @param args Additional arguments to pass to the function.
    @return The result of calling the function.
 */
template<
    class TextTy,
    class Fn,
    class... Args>
    requires std::derived_from<TextTy, Text>
decltype(auto)
visit(
    TextTy& text,
    Fn&& fn,
    Args&&... args)
{
    auto visitor = makeVisitor<Text>(
        text, std::forward<Fn>(fn),
        std::forward<Args>(args)...);
    switch(text.Kind)
    {
    case NodeKind::link:
        return visitor.template visit<Link>();
    case NodeKind::reference:
        return visitor.template visit<Reference>();
    case NodeKind::copy_details:
        return visitor.template visit<CopyDetails>();
    case NodeKind::styled:
        return visitor.template visit<Styled>();
    case NodeKind::text:
        return visitor.template visit<Text>();
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Traverse a list of texts.

    @param list The list of texts to traverse.
    @param f The function to call for each text.
    @param args Additional arguments to pass to the function.
 */
template<class F, class T, class... Args>
requires std::derived_from<T, Text>
void traverse(
    std::vector<std::unique_ptr<T>> const& list,
    F&& f, Args&&... args)
{
    for(auto const& text : list)
        visit(*text,
            std::forward<F>(f),
            std::forward<Args>(args)...);
}

/** Map the Polymorphic Text as a @ref dom::Value object.

    @param io The output parameter to receive the dom::Object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO, polymorphic_storage_for<Text> TextTy>
void
tag_invoke(
    dom::ValueFromTag,
    IO& io,
    TextTy const& I,
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
operator<=>(Polymorphic<Text> const& lhs, Polymorphic<Text> const& rhs);

inline
bool
operator==(Polymorphic<Text> const& lhs, Polymorphic<Text> const& rhs) {
    return std::is_eq(lhs <=> rhs);
}

} // clang::mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_TEXT_HPP
