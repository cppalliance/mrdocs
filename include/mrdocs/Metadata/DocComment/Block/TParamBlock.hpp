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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TPARAMBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TPARAMBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** Documentation for a template parameter

    Syntax:

    @code
    @tparam T description
    @endcode
*/
struct TParamBlock final
    : BlockCommonBase<BlockKind::TParam>
    , InlineContainer
{
    /** Template parameter name.
    */
    std::string name;

    /** Inherit inline container constructors.
    */
    using InlineContainer::InlineContainer;
};

MRDOCS_DESCRIBE_STRUCT(
    TParamBlock,
    (BlockCommonBase<BlockKind::TParam>, InlineContainer),
    (name)
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TPARAMBLOCK_HPP
