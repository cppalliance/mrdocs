//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_DOCCOMMENT_INLINE_INLINEKINDS_HPP
#define MRDOCS_LIB_METADATA_DOCCOMMENT_INLINE_INLINEKINDS_HPP

#include <mrdocs/Metadata/DocComment/Inline/CodeInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/CopyDetailsInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/EmphInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/FootnoteReferenceInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/HighlightInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/ImageInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/InlineBase.hpp>
#include <mrdocs/Metadata/DocComment/Inline/LineBreakInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/LinkInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/MathInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/ReferenceInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/SoftBreakInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/StrikethroughInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/StrongInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/SubscriptInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/SuperscriptInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/TextInline.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>

namespace mrdocs::doc {

#define INFO(Name) MRDOCS_KIND_ENTRY(Inline, Name##Inline)
MRDOCS_DESCRIBE_KINDS_BEGIN(Inline)
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>
MRDOCS_DESCRIBE_KINDS_END(Inline)
#undef INFO

} // namespace mrdocs::doc

#endif
