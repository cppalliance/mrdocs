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
    BlockKind Kind = BlockKind::Paragraph;

    virtual ~Block() = default;

    auto
    operator<=>(Block const& other) const = default;

    bool
    operator==(Block const& other) const noexcept = default;

    constexpr Block const& asBlock() const noexcept
    {
        return *this;
    }

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
    constexpr
    Block() = default;

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

    ~BlockCommonBase() override = default;

    #define INFO(Kind) \
    static constexpr bool is##Kind() noexcept { return K == BlockKind::Kind; }
#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>

    auto operator<=>(BlockCommonBase const&) const = default;

protected:
    constexpr explicit BlockCommonBase()
        : Block(K)
    {}
};

/** Map the @ref Block to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
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


struct MRDOCS_DECL BlockContainer
{
    std::vector<Polymorphic<Block>> blocks;

    BlockContainer&
    asBlockContainer()
    {
        return *this;
    }

    BlockContainer const&
    asBlockContainer() const
    {
        return *this;
    }

    std::strong_ordering
    operator<=>(BlockContainer const&) const;

    bool
    operator==(BlockContainer const&) const = default;
};

template <class IO, polymorphic_storage_for<Block> BlockTy>
void
tag_invoke(
    dom::ValueFromTag,
    IO& io,
    BlockTy const& I,
    DomCorpus const* domCorpus);

template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    BlockContainer const& I,
    DomCorpus const* domCorpus)
{
    io.defer("blocks", [&I, domCorpus] {
        return dom::LazyArray(I.blocks, domCorpus);
    });
}

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
