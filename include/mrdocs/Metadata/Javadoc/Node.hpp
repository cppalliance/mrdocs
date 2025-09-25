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

#ifndef MRDOCS_API_METADATA_JAVADOC_NODE_HPP
#define MRDOCS_API_METADATA_JAVADOC_NODE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Block.hpp>
#include <mrdocs/Metadata/Javadoc/Inline.hpp>
#include <mrdocs/Metadata/Javadoc/Node/NodeBase.hpp>
#include <mrdocs/Support/Concepts.hpp>
#include <mrdocs/Support/Visitor.hpp>

/** Javadoc related types and functions.

    Javadoc is a documentation generator originally
    created for the Java language from source code.

    The Javadoc documentation generator tool can interpret
    text in the "doc comments" format included
    directly in the source code.

    The same "doc comments" format has been replicated
    and extended by documentation systems for other
    languages, including the cross-language Doxygen
    and the JSDoc system for JavaScript.

    Because Clang can already parse and extract
    blocks of Javadoc-style comments from source
    code, these classes are used to represent the
    parsed documentation in a structured form.

    @see https://en.wikipedia.org/wiki/Javadoc
    @see https://www.doxygen.nl

 */
namespace clang::mrdocs::doc {

/** Visit a node.

    @param node The node to visit.
    @param fn The function to call for each node.
    @param args Additional arguments to pass to the function.
    @return The result of calling the function.
 */
template<
    class NodeTy,
    class Fn,
    class... Args>
requires
    std::same_as<std::remove_cvref_t<NodeTy>, Node>
decltype(auto)
visit(
    NodeTy& node,
    Fn&& fn,
    Args&&... args)
{
    auto visitor = makeVisitor<Node>(
        node, std::forward<Fn>(fn),
        std::forward<Args>(args)...);
    switch(node.Kind)
    {
    case NodeKind::admonition:
        return visitor.template visit<Admonition>();
    case NodeKind::brief:
        return visitor.template visit<Brief>();
    case NodeKind::code:
        return visitor.template visit<Code>();
    case NodeKind::heading:
        return visitor.template visit<Heading>();
    case NodeKind::paragraph:
        return visitor.template visit<Paragraph>();
    case NodeKind::link:
        return visitor.template visit<Link>();
    case NodeKind::reference:
        return visitor.template visit<Reference>();
    case NodeKind::copy_details:
        return visitor.template visit<CopyDetails>();
    case NodeKind::list_item:
        return visitor.template visit<ListItem>();
    case NodeKind::unordered_list:
        return visitor.template visit<UnorderedList>();
    case NodeKind::param:
        return visitor.template visit<Param>();
    case NodeKind::returns:
        return visitor.template visit<Returns>();
    case NodeKind::styled:
        return visitor.template visit<Styled>();
    case NodeKind::text:
        return visitor.template visit<Text>();
    case NodeKind::tparam:
        return visitor.template visit<TParam>();
    case NodeKind::throws:
        return visitor.template visit<Throws>();
    case NodeKind::see:
        return visitor.template visit<See>();
    case NodeKind::precondition:
        return visitor.template visit<Precondition>();
    case NodeKind::postcondition:
        return visitor.template visit<Postcondition>();
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Traverse a list of nodes.

    @param list The list of nodes to traverse.
    @param f The function to call for each node.
    @param args Additional arguments to pass to the function.
 */
template<class F, class T, class... Args>
requires std::derived_from<T, Node>
void traverse(
    std::vector<std::unique_ptr<T>> const& list,
    F&& f, Args&&... args)
{
    for(auto const& node : list)
        visit(*node,
            std::forward<F>(f),
            std::forward<Args>(args)...);
}

/** Map the Polymorphic Node as a @ref dom::Value object.

    @param io The output parameter to receive the dom::Object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::ValueFromTag,
    IO& io,
    Polymorphic<Node> const& I,
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


} // clang::mrdocs::doc

#endif
