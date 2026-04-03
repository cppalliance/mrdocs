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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_ADMONITIONBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_ADMONITIONBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/AdmonitionKind.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** A block for side-notes like tips, warnings, notes

    This paragraph represents an admonition, such as
    a note, tip, important, caution, or warning.


    Syntax:

    @code
    @note text
    @endcode
*/
struct AdmonitionBlock final
    : BlockCommonBase<BlockKind::Admonition>
    , BlockContainer
{
    /** The kind of admonition

        This is typically a string in other implementations.
    */
    AdmonitionKind admonish;

    /// Optional title for the admonition
    Optional<Polymorphic<Inline>> Title;

    /** Construct an admonition with the given kind.
    */
    explicit
    AdmonitionBlock(
        AdmonitionKind const admonish_ = AdmonitionKind::none) noexcept
        : admonish(admonish_)
    {}

    /** Compare admonitions by kind, title, and contents.
    */
    auto operator<=>(AdmonitionBlock const&) const = default;

    /** Equality compares the admonition contents.
    */
    bool operator==(AdmonitionBlock const&) const noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    AdmonitionBlock,
    (BlockCommonBase<BlockKind::Admonition>, BlockContainer),
    (admonish)
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_ADMONITIONBLOCK_HPP
