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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_BLOCKBASE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_BLOCKBASE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/ArrayView.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockKind.hpp>
#include <mrdocs/Metadata/DocComment/Inline.hpp>
#include <algorithm>
#include <string>

namespace mrdocs::doc {

/* Forward declarations
 */
#define INFO(Type) struct Type##Block;
#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>

/** A piece of block content

    The top level is a list of blocks.

    There are two types of blocks: headings and paragraphs.
*/
struct MRDOCS_DECL Block
{
    /** Discriminator identifying which concrete block this instance holds.
    */
    BlockKind Kind = BlockKind::Paragraph;

    /** Virtual to allow deleting through a base pointer.
    */
    virtual ~Block() = default;

    /** Three-way comparison on the block contents.
    */
    auto
    operator<=>(Block const& other) const = default;

    /** Equality compares the block contents.
    */
    bool
    operator==(Block const& other) const noexcept = default;

    /** View this object as a `Block` reference.
    */
    constexpr Block const& asBlock() const noexcept
    {
        return *this;
    }

    /** View this object as a mutable `Block` reference.
    */
    constexpr Block& asBlock() noexcept
    {
        return *this;
    }

    #define INFO(Type) constexpr bool is##Type() const noexcept { \
        return Kind == BlockKind::Type; \
    }
#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>

#define INFO(Type) \
    constexpr Type##Block const& as##Type() const noexcept { \
        if (Kind == BlockKind::Type) \
            return reinterpret_cast<Type##Block const&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>

#define INFO(Type) \
    constexpr Type##Block & as##Type() noexcept { \
        if (Kind == BlockKind::Type) \
            return reinterpret_cast<Type##Block&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>

#define INFO(Type) \
    constexpr Type##Block const* as##Type##Ptr() const noexcept { \
        if (Kind == BlockKind::Type) { return reinterpret_cast<Type##Block const*>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>

#define INFO(Type) \
    constexpr Type##Block * as##Type##Ptr() noexcept { \
        if (Kind == BlockKind::Type) { return reinterpret_cast<Type##Block *>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>

protected:
    /** Default-construct a paragraph block.
    */
    constexpr
    Block() = default;

    /** Construct a block with the specified discriminant.
    */
    explicit
    Block(BlockKind const kind_) noexcept
        : Kind(kind_)
    {}
};

/** Base class for providing variant discriminator functions.

    This offers functions that return a boolean at
    compile-time, indicating if the most-derived
    class is a certain type.
*/
template <BlockKind K>
struct BlockCommonBase : Block
{
    /** The variant discriminator constant of the most-derived class.

        It only distinguishes from `Block::kind` in that it is a constant.

    */
    static constexpr BlockKind kind_id = K;

    /** Virtual to keep dynamic dispatch working for block hierarchies.
    */
    ~BlockCommonBase() override = default;

    #define INFO(Kind) \
    static constexpr bool is##Kind() noexcept { return K == BlockKind::Kind; }
#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>

    /** Compare two blocks that share the same `kind_id`.
    */
    auto operator<=>(BlockCommonBase const&) const = default;

protected:
    /** Construct with the fixed block kind.
    */
    constexpr explicit BlockCommonBase()
        : Block(K)
    {}
};

/** Map the @ref Block to a @ref dom::Object.

    @param io The output object.
    @param I The input object.
*/
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    Block const& I,
    DomCorpus const*)
{
    io.map("kind", doc::toString(I.Kind));
}

/** Return the @ref Block as a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Block const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** Removes leading whitespace from the block.

    @param el The Block to trim.
    @return void
*/
MRDOCS_DECL
void
ltrim(Block& el);

/** Removes trailing whitespace from the block.

    @param el The Block to trim.
    @return void
*/
MRDOCS_DECL
void
rtrim(Block& el);

/** Removes leading and trailing whitespace from the block.

    @param el The Block to trim.
    @return void
*/
inline
void
trim(Block& el)
{
    ltrim(el);
    rtrim(el);
}

/** Determine if the inline is empty
*/
MRDOCS_DECL
bool
isEmpty(Block const& el);


/** A composite block that stores a sequence of child blocks.
*/
struct MRDOCS_DECL BlockContainer
{
    /** Child blocks contained within this composite block.
    */
    std::vector<Polymorphic<Block>> blocks;

    /** Access the container as a mutable view.
    */
    BlockContainer&
    asBlockContainer()
    {
        return *this;
    }

    /** Access the container as a const view.
    */
    BlockContainer const&
    asBlockContainer() const
    {
        return *this;
    }

    /** Order containers lexicographically by their children.
    */
    std::strong_ordering
    operator<=>(BlockContainer const&) const;

    /** Equality compares the stored child blocks.
    */
    bool
    operator==(BlockContainer const&) const = default;
};

/** Convert a polymorphic block storage into a DOM value.
    @param io Destination value to fill.
    @param I Block to convert.
    @param domCorpus Corpus context for lazy references.
*/
template <class IO, polymorphic_storage_for<Block> BlockTy>
void
tag_invoke(
    dom::ValueFromTag,
    IO& io,
    BlockTy const& I,
    DomCorpus const* domCorpus);

/** Map a block container into a lazily-evaluated DOM object.
    @param io Destination object.
    @param I Block container to convert.
    @param domCorpus Corpus context for lazy references.
*/
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    BlockContainer const& I,
    DomCorpus const* domCorpus)
{
    io.defer("blocks", [&I, domCorpus] {
        return dom::LazyArray(I.blocks, domCorpus);
    });
}

/** Return the block container as a DOM value.
    @param v Destination value.
    @param I Block container to convert.
    @param domCorpus Corpus context for lazy references.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    BlockContainer const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** Removes leading whitespace from the first text elements

    @param blocks The BlockContainer to trim.
    @return void
*/
MRDOCS_DECL
void
ltrim(BlockContainer& blocks);

/** Removes trailing whitespace from the last text elements

    @param blocks The BlockContainer to trim.
    @return void
*/
MRDOCS_DECL
void
rtrim(BlockContainer& blocks);

/** Removes leading and trailing whitespace from the text elements

    @param blocks The BlockContainer to trim.
    @return void
*/
inline
void
trim(BlockContainer& blocks)
{
    ltrim(blocks);
    rtrim(blocks);
}


} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_BLOCKBASE_HPP
