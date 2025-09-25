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

#ifndef MRDOCS_API_METADATA_JAVADOC_HPP
#define MRDOCS_API_METADATA_JAVADOC_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Javadoc/Node.hpp>

namespace clang::mrdocs {

/** A processed documentation comment attached to a declaration.

    A complete documentation comment consists of a
    sequence of text blocks.

    Some blocks are used to contain regular text,
    such as paragraphs and lists. These are
    analogous to markdown blocks.

    Other blocks contain metadata about the declaration,
    such as parameters and return values. These
    blocks are stored separately in the
    @ref Javadoc structure.

    Each block in the document might contain:

    - No other blocks (leaf blocks)
    - Other blocks (container blocks: e.g. lists)

    When they contain no other blocks, they might be:

    - Inlines only (e.g. paragraphs)
    - No inlines (e.g. horizontal rule)

    Inline content elements contain other inlines
    but cannot contain blocks.
 */
struct MRDOCS_DECL
    Javadoc
{
    /// The list of text blocks.
    std::vector<Polymorphic<doc::Block>> blocks;

    // ----------------------
    // Symbol Metadata

    /// A brief description of the symbol.
    Optional<doc::Brief> brief;

    /** The list of return type descriptions.

        Multiple return descriptions are allowed.

        The results are concatenated in the order
        they appear in the source code.
     */
    std::vector<doc::Returns> returns;

    /// The list of parameters.
    std::vector<doc::Param> params;

    /// The list of template parameters.
    std::vector<doc::TParam> tparams;

    /// The list of exceptions.
    std::vector<doc::Throws> exceptions;

    /// The list of "see also" references.
    std::vector<doc::See> sees;

    /// The list of preconditions.
    std::vector<doc::Precondition> preconditions;

    /// The list of postconditions.
    std::vector<doc::Postcondition> postconditions;

    /** The list of "relates" references.

        These references are creates with the
        \\relates command.
     */
    std::vector<doc::Reference> relates;

    /** The list of "related" references.

        These references are the inverse of
        the \\relates command.
     */
    std::vector<doc::Reference> related;

    /** Constructor.
    */
    MRDOCS_DECL
    Javadoc() noexcept;

    /** Constructor
    */
    explicit
    Javadoc(
        std::vector<Polymorphic<doc::Block>> blocks);

    /** Return true if this is empty
    */
    bool
    empty() const noexcept
    {
        return
            blocks.empty() &&
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

    /** Return the list of top level blocks.
    */
    std::vector<Polymorphic<doc::Block>> const&
    getBlocks() const noexcept
    {
        return blocks;
    }

    // VFALCO This is unfortunately necessary for
    //        the deserialization from bitcode...
    std::vector<Polymorphic<doc::Block>>&
    getBlocks() noexcept
    {
        return blocks;
    }

    //--------------------------------------------

    /** Comparison

        These are used internally to impose a
        total ordering, and not visible in the
        output format.
    */
    /** @{ */
    auto operator<=>(Javadoc const& other) const noexcept {
        if (auto const cmp = blocks.size() <=> other.blocks.size();
            !std::is_eq(cmp))
        {
            return cmp;
        }
        for (std::size_t i = 0; i < blocks.size(); ++i)
        {
            if (auto cmp = CompareDerived(blocks[i], other.blocks[i]);
                !std::is_eq(cmp))
            {
                return cmp;
            }
        }
        return std::strong_ordering::equal;
    }
    bool operator==(Javadoc const&) const noexcept;
    bool operator!=(Javadoc const&) const noexcept;
    /* @} */

    //--------------------------------------------

    /** Attempt to append a block.

        @return An empty string on success, otherwise
        a string indicating the reason for the failure.

        @param block The block to append.
    */
    template<std::derived_from<doc::Block> T>
    std::string
    emplace_back(T&& block)
    {
        return emplace_back(Polymorphic<doc::Block>(std::in_place_type<T>,
                                                    std::forward<T>(block)));
    }

    /** Append blocks from another javadoc to this.
    */
    void append(Javadoc&& other);

    /** Append blocks from a list to this.

        @param blocks The blocks to append.
     */
    void
    append(std::vector<Polymorphic<doc::Node>>&& blocks);

private:
    std::string
    emplace_back(Polymorphic<doc::Block>);
};

inline
void merge(Javadoc& I, Javadoc&& other)
{
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

/** Map the @ref Javadoc to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The javadoc to map.
    @param domCorpus The DOM corpus, or nullptr.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    Javadoc const& I,
    DomCorpus const* domCorpus)
{
    io.defer("description", [&I, domCorpus] {
        return dom::LazyArray(I.blocks, domCorpus);
    });
    if (I.brief && !I.brief->children.empty())
    {
        io.map("brief", I.brief);
    }
    io.defer("returns", [&I, domCorpus] {
        return dom::LazyArray(I.returns, domCorpus);
    });
    io.defer("params", [&I, domCorpus] {
        return dom::LazyArray(I.params, domCorpus);
    });
    io.defer("tparams", [&I, domCorpus] {
        return dom::LazyArray(I.tparams, domCorpus);
    });
    io.defer("exceptions", [&I, domCorpus] {
        return dom::LazyArray(I.exceptions, domCorpus);
    });
    io.defer("sees", [&I, domCorpus] {
        return dom::LazyArray(I.sees, domCorpus);
    });
    io.defer("relates", [&I, domCorpus] {
        return dom::LazyArray(I.relates, domCorpus);
    });
    io.defer("related", [&I, domCorpus] {
        return dom::LazyArray(I.related, domCorpus);
    });
    io.defer("preconditions", [&I, domCorpus] {
        return dom::LazyArray(I.preconditions, domCorpus);
    });
    io.defer("postconditions", [&I, domCorpus] {
        return dom::LazyArray(I.postconditions, domCorpus);
    });
}

/** Return the @ref Javadoc as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Javadoc const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs

#endif
