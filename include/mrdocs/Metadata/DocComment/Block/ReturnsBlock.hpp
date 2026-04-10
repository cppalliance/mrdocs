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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_RETURNSBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_RETURNSBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** Documentation for a function return type

    Syntax:

    @code
    @return description
    @endcode
*/
struct ReturnsBlock final
    : BlockCommonBase<BlockKind::Returns>
    , InlineContainer
{
    /** Inherit inline container constructors.
    */
    using InlineContainer::InlineContainer;

    /** Copy constructor.
    */
    ReturnsBlock(ReturnsBlock const&) = default;
    /** Move constructor.
    */
    ReturnsBlock(ReturnsBlock&&) noexcept = default;
    /** Construct from inline content (copy).
    */
    ReturnsBlock(InlineContainer const& other) : InlineContainer(other) {}
    /** Construct from inline content (move).
    */
    ReturnsBlock(InlineContainer&& other) noexcept : InlineContainer(other) {}
    /** Virtual destructor for polymorphic base.
    */
    ~ReturnsBlock() override = default;
    /** Copy assignment.
    */
    ReturnsBlock& operator=(ReturnsBlock const&) = default;
    /** Move assignment.
    */
    ReturnsBlock& operator=(ReturnsBlock&&) noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    ReturnsBlock,
    (BlockCommonBase<BlockKind::Returns>, InlineContainer),
    ()
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_RETURNSBLOCK_HPP
