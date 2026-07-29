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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_SEEBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_SEEBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** A @see paragraph listing related symbols or references.

    Syntax:

    @code
    @see other_symbol
    @endcode

    or

    @code
    @see AnotherFunction()
    @see MyClass::calculateSomething()
    @see MyFile.h
    @see "The Doxygen Manual"
    @endcode


*/
struct SeeBlock final
    : BlockCommonBase<BlockKind::See>
    , InlineContainer
{
    /** Inherit inline container constructors.
    */
    using InlineContainer::InlineContainer;
};

MRDOCS_DESCRIBE_STRUCT(
    SeeBlock,
    (BlockCommonBase<BlockKind::See>, InlineContainer),
    ()
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_SEEBLOCK_HPP
