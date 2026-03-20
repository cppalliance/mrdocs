//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/DocComment/Block.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** A processed documentation comment attached to a declaration.

    A complete documentation comment document consists of a
    sequence of text blocks. Blocks are the top-level
    structural units that might contain other blocks
    or inline elements. These are analogous to markdown
    blocks.

    Inline elements (text, emphasis, links, code spans, etc.)
    live inside certain block types, like paragraphs or
    headings. Inlines can contain other inlines
    (e.g., emphasis contains text, link contains text), but
    they cannot directly contain blocks.

    Some blocks contain metadata about the symbol being
    documented, such as parameters and return values. These
    blocks are parsed as usual, but are stored separately in the
    DocComment structure.

    Each block in the document might contain:

    - No other blocks (leaf blocks)
    - Other blocks (container blocks: e.g. lists)

    When they contain no other blocks, they might be:

    - Inlines only (e.g. paragraphs)
    - No inlines (e.g. horizontal rule)

    Inline content elements contain other inlines
    but cannot contain blocks.
*/
struct MRDOCS_DECL DocComment {
    /// The list of text blocks.
    std::vector<Polymorphic<doc::Block>> Document;

    // ----------------------
    // Symbol Metadata

    /// A brief description of the symbol.
    Optional<doc::BriefBlock> brief;

    /** The list of return type descriptions.

        Multiple return descriptions are allowed.

        The results are concatenated in the order
        they appear in the source code.
    */
    std::vector<doc::ReturnsBlock> returns;

    /// The list of parameters.
    std::vector<doc::ParamBlock> params;

    /// The list of template parameters.
    std::vector<doc::TParamBlock> tparams;

    /// The list of exceptions.
    std::vector<doc::ThrowsBlock> exceptions;

    /// The list of "see also" references.
    std::vector<doc::SeeBlock> sees;

    /// The list of preconditions.
    std::vector<doc::PreconditionBlock> preconditions;

    /// The list of postconditions.
    std::vector<doc::PostconditionBlock> postconditions;

    /** The list of "relates" references.

        These references are created with the
        \\relates command.
    */
    std::vector<doc::ReferenceInline> relates;

    /** The list of "related" references.

        These references are the inverse of
        the \\relates command.

        They are calculated automatically by
        MrDocs and are rendered as
        Non-Member Functions.
    */
    std::vector<doc::ReferenceInline> related;

    /// True if the @functionobject (or @functor) command was used on this symbol.
    bool IsFunctionObject = false;

    /** Constructor.
    */
    MRDOCS_DECL
    DocComment() noexcept;

    /** Constructor
    */
    explicit DocComment(
        std::vector<Polymorphic<doc::Block>> blocks);

    /** Return true if this is empty
    */
    bool
    empty() const noexcept
    {
        return Document.empty() &&
            !brief &&
            returns.empty() &&
            params.empty() &&
            tparams.empty() &&
            exceptions.empty() &&
            sees.empty() &&
            related.empty() &&
            preconditions.empty() &&
            postconditions.empty();
    }

    /** Three-way comparison on the rendered block sequence.
    */
    auto operator<=>(DocComment const& other) const noexcept {
        if (auto const cmp = Document.size() <=> other.Document.size();
            !std::is_eq(cmp))
        {
            return cmp;
        }
        for (std::size_t i = 0; i < Document.size(); ++i)
        {
            if (auto cmp = CompareDerived(Document[i], other.Document[i]);
                !std::is_eq(cmp))
            {
                return cmp;
            }
        }
        return std::strong_ordering::equal;
    }

    /** Equality compares all stored blocks.
    */
    bool operator==(DocComment const&) const noexcept = default;

    /** Inequality is the negation of equality.
    */
    bool operator!=(DocComment const&) const noexcept;

    /** Append blocks from another DocComment to this.
    */
    void
    append(DocComment&& other);
};

MRDOCS_DESCRIBE_STRUCT(
    DocComment,
    (),
    (Document, brief, returns, params, tparams,
     exceptions, sees, preconditions, postconditions,
     relates, related)
)

/** Append blocks from `other` into `I`, preserving order.
*/
inline
void merge(DocComment& I, DocComment&& other)
{
    // Merge the function-object flag before comparing so
    // the flag alone doesn't cause spurious inequality.
    I.IsFunctionObject |= other.IsFunctionObject;

    // FIXME: this doesn't merge parameter information;
    // parameters with the same name but different directions
    // or descriptions end up being duplicated
    if(other != I)
    {
        // Unconditionally extend the blocks
        // since each decl may have a comment.
        I.append(std::move(other));
    }
}

/** Map the @ref DocComment to a @ref dom::Object.

    @param io The output object.
    @param I The DocComment to map.
    @param domCorpus The DOM corpus, or nullptr.
*/
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    DocComment const& I,
    DomCorpus const* domCorpus);

/** Return the @ref DocComment as a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    DocComment const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** Concept to check if a type represents a DocComment node.
*/
template <class T>
concept DocCommentNode =
    std::derived_from<T, doc::Block> || std::derived_from<T, doc::Inline>;

namespace detail {
// forward declaration to handle derived nodes
template <bool bottomUp, DocCommentNode T, class F>
void
traverseImpl(Polymorphic<T>& node, F&& func);

// forward declaration to handle base class nodes
template <bool bottomUp, class T, class F>
requires std::same_as<T, doc::Block> || std::same_as<T, doc::Inline>
void
traverseImpl(T& baseNode, F&& func);

template <bool bottomUp, class F>
void
traverseImpl(doc::ListItem& item, F&& func)
{
    for (auto& block : item.blocks)
    {
        traverseImpl<bottomUp>(block, func);
    }
}

// Traverse a derived node
template <bool bottomUp, DocCommentNode NodeTy, class F>
requires(!std::same_as<NodeTy, doc::Block> && !std::same_as<NodeTy, doc::Inline>)
void
traverseImpl(NodeTy& N, F&& func)
{
    if constexpr (!bottomUp && std::invocable<F, NodeTy&>)
    {
        func(N);
    }
    if constexpr (requires { N.children; })
    {
        for (auto& child: N.children)
        {
            traverseImpl<bottomUp>(child, func);
        }
    }
    if constexpr (requires { N.blocks; })
    {
        for (auto& block: N.blocks)
        {
            traverseImpl<bottomUp>(block, func);
        }
    }
    if constexpr (std::same_as<NodeTy, doc::ThrowsBlock>)
    {
        traverseImpl<bottomUp>(N.exception, func);
    }

    if constexpr (std::same_as<NodeTy, doc::ListBlock>)
    {
        for (auto& item: N.items)
        {
            traverseImpl<bottomUp>(item, func);
        }
    }

    if constexpr (bottomUp && std::invocable<F, NodeTy&>)
    {
        func(N);
    }
}

template <bool bottomUp, class F>
void
traverseImpl(DocComment& doc, F&& func)
{
    if constexpr (!bottomUp && std::invocable<F, DocComment&>)
    {
        func(doc);
    }
    for (auto& block: doc.Document)
    {
        traverseImpl<bottomUp>(block, std::forward<F>(func));
    }
    if (doc.brief)
    {
        traverseImpl<bottomUp>(*doc.brief, std::forward<F>(func));
    }
    for (auto& returnsEl: doc.returns)
    {
        traverseImpl<bottomUp>(returnsEl, std::forward<F>(func));
    }
    for (auto& paramsEl: doc.params)
    {
        traverseImpl<bottomUp>(paramsEl, std::forward<F>(func));
    }
    for (auto& tparamsEl: doc.tparams)
    {
        traverseImpl<bottomUp>(tparamsEl, std::forward<F>(func));
    }
    for (auto& exceptionsEl: doc.exceptions)
    {
        traverseImpl<bottomUp>(exceptionsEl, std::forward<F>(func));
    }
    for (auto& seesEl: doc.sees)
    {
        traverseImpl<bottomUp>(seesEl, std::forward<F>(func));
    }
    for (auto& preconditionsEl: doc.preconditions)
    {
        traverseImpl<bottomUp>(preconditionsEl, std::forward<F>(func));
    }
    for (auto& postconditionsEl: doc.postconditions)
    {
        traverseImpl<bottomUp>(postconditionsEl, std::forward<F>(func));
    }
    if constexpr (bottomUp && std::invocable<F, DocComment&>)
    {
        func(doc);
    }
}

// Traverse a reference to a base class node
template <bool bottomUp, class T, class F>
requires std::same_as<T, doc::Block> || std::same_as<T, doc::Inline>
void
traverseImpl(T& baseNode, F&& func)
{
    visit(baseNode, [&]<typename NodeTy>(NodeTy& N) {
        traverseImpl<bottomUp>(N, std::forward<F>(func));
    });
}

// Traverse a polymorphic node
template <bool bottomUp, DocCommentNode T, class F>
void
traverseImpl(Polymorphic<T>& node, F&& func)
{
    traverseImpl<bottomUp>(*node, std::forward<F>(func));
}
}

/** Types that can be traversed by DocComment helpers.
*/
template <class T>
concept DocCommentNodeTraversable =
    DocCommentNode<T> ||
    std::same_as<DocComment, T> ||
    (detail::IsPolymorphic<T> && DocCommentNode<typename T::value_type>);

/** Traverse a DocComment tree bottom-up (post-order).
    @param node Root node to traverse.
    @param func Visitor invoked after children are visited.
*/
template <DocCommentNodeTraversable T, class F>
void
bottomUpTraverse(T& node, F&& func)
{
    detail::traverseImpl<true>(node, std::forward<F>(func));
}

/** Traverse a DocComment tree top-down (pre-order).
    @param node Root node to traverse.
    @param func Visitor invoked before children are visited.
*/
template <DocCommentNodeTraversable T, class F>
void
topDownTraverse(T& node, F&& func)
{
    detail::traverseImpl<false>(node, std::forward<F>(func));
}
} // mrdocs

#endif // MRDOCS_API_METADATA_DOCCOMMENT_HPP
