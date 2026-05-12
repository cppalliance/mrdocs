//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_DOCCOMMENT_BLOCK_BLOCKKINDS_HPP
#define MRDOCS_LIB_METADATA_DOCCOMMENT_BLOCK_BLOCKKINDS_HPP

#include <mrdocs/Metadata/DocComment/Block/AdmonitionBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/BriefBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/CodeBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/DefinitionListBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/FootnoteDefinitionBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/HeadingBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ListBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/MathBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParamBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/PostconditionBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/PreconditionBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/QuoteBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ReturnsBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/SeeBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/TParamBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/TableBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ThematicBreakBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ThrowsBlock.hpp>
#include <mrdocs/Support/DescribeKinds.hpp>

namespace mrdocs::doc {

#define INFO(Name) MRDOCS_KIND_ENTRY(Block, Name##Block)
MRDOCS_DESCRIBE_KINDS_BEGIN(Block)
#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>
MRDOCS_DESCRIBE_KINDS_END(Block)
#undef INFO

} // namespace mrdocs::doc

#endif
