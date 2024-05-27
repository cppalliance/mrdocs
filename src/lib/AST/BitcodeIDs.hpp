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

#ifndef MRDOCS_LIB_AST_BITCODEIDS_HPP
#define MRDOCS_LIB_AST_BITCODEIDS_HPP

#include <mrdocs/Platform.hpp>
#include <llvm/Bitstream/BitCodeEnums.h>

namespace clang {
namespace mrdocs {

// Current version number of clang-doc bitcode.
// Should be bumped when removing or changing BlockIds, RecordIDs, or
// BitCodeConstants, though they can be added without breaking it.
static const unsigned BitcodeVersion = 3;

struct BitCodeConstants
{
    static constexpr unsigned RecordSize = 32U;
    static constexpr unsigned SignatureBitSize = 8U;
    static constexpr unsigned SubblockIDSize = 4U;
    static constexpr unsigned BoolSize = 1U;
    static constexpr unsigned IntSize = 16U;
    static constexpr unsigned StringLengthSize = 16U;  // up to 32767 chars
    static constexpr unsigned FilenameLengthSize = 16U;
    static constexpr unsigned LineNumberSize = 32U;
    static constexpr unsigned USRLengthSize = 6U;
    static constexpr unsigned USRBitLengthSize = 8U;
    static constexpr unsigned USRHashSize = 20;
    static constexpr unsigned char Signature[4] = {'M', 'R', 'D', 'X'};
};

/** List of block identifiers.

    New IDs need to be added to both the enum
    here and the relevant IdNameMap in the
    implementation file.
*/
enum BlockID
{
    BI_VERSION_BLOCK_ID = llvm::bitc::FIRST_APPLICATION_BLOCKID,

    BI_INFO_PART_ID,
    BI_SOURCE_INFO_ID,
    BI_SCOPE_INFO_ID,

    BI_NAME_INFO_ID,
    BI_LOOKUP_INFO_ID,
    BI_BASE_BLOCK_ID,
    BI_ENUM_BLOCK_ID,
    BI_EXPR_BLOCK_ID,
    BI_BITFIELD_WIDTH_BLOCK_ID,
    BI_FIELD_BLOCK_ID,
    BI_FUNCTION_BLOCK_ID,
    BI_FUNCTION_PARAM_BLOCK_ID,
    BI_GUIDE_BLOCK_ID,
    BI_JAVADOC_BLOCK_ID,
    BI_JAVADOC_LIST_BLOCK_ID,
    BI_JAVADOC_NODE_BLOCK_ID,
    BI_NAMESPACE_BLOCK_ID,
    BI_RECORD_BLOCK_ID,
    BI_TEMPLATE_ARG_BLOCK_ID,
    BI_TEMPLATE_BLOCK_ID,
    BI_TEMPLATE_PARAM_BLOCK_ID,
    BI_SPECIALIZATION_BLOCK_ID,
    BI_FRIEND_BLOCK_ID,
    BI_ENUMERATOR_BLOCK_ID,
    BI_TYPEINFO_BLOCK_ID,
    BI_TYPEINFO_PARENT_BLOCK_ID,
    BI_TYPEINFO_CHILD_BLOCK_ID,
    BI_TYPEINFO_PARAM_BLOCK_ID,
    BI_TYPEDEF_BLOCK_ID,
    BI_VARIABLE_BLOCK_ID,
    BI_ALIAS_BLOCK_ID,
    BI_USING_BLOCK_ID,
    BI_LAST,
    BI_FIRST = BI_VERSION_BLOCK_ID
};

// New IDs need to be added to the enum here, and to the relevant IdNameMap and
// initialization list in the implementation file.
enum RecordID
{
    VERSION = 1,

    INFO_PART_ID,
    INFO_PART_ACCESS,
    INFO_PART_IMPLICIT,
    INFO_PART_NAME,
    INFO_PART_PARENTS,
    SOURCE_INFO_DEFLOC,
    SOURCE_INFO_LOC,
    SCOPE_INFO_MEMBERS,
    LOOKUP_NAME,
    LOOKUP_MEMBERS,

    NAME_INFO_KIND,
    NAME_INFO_ID,
    NAME_INFO_NAME,

    NAMESPACE_BITS,
    TYPEINFO_KIND,
    TYPEINFO_IS_PACK,
    TYPEINFO_CVQUAL,
    TYPEINFO_NOEXCEPT,
    TYPEINFO_REFQUAL,
    TYPEINFO_IS_VARIADIC,
    BASE_ACCESS,
    BASE_IS_VIRTUAL,
    FIELD_ATTRIBUTES,
    FIELD_DEFAULT,
    FIELD_IS_MUTABLE,
    FIELD_IS_BITFIELD,
    FRIEND_SYMBOL,
    FUNCTION_BITS,
    FUNCTION_CLASS,
    FUNCTION_NOEXCEPT,
    FUNCTION_EXPLICIT,
    FUNCTION_PARAM_NAME,
    FUNCTION_PARAM_DEFAULT,
    GUIDE_EXPLICIT,
    JAVADOC_NODE_ADMONISH,
    JAVADOC_NODE_HREF,
    JAVADOC_NODE_KIND,
    JAVADOC_NODE_STRING,
    JAVADOC_NODE_STYLE,
    JAVADOC_NODE_PART,
    JAVADOC_NODE_SYMBOLREF,
    JAVADOC_PARAM_DIRECTION,
    ENUM_SCOPED,
    EXPR_WRITTEN,
    EXPR_VALUE,
    RECORD_BITS,
    RECORD_IS_TYPE_DEF,
    RECORD_KEY_KIND,
    TEMPLATE_ARG_KIND,
    TEMPLATE_ARG_IS_PACK,
    TEMPLATE_ARG_TEMPLATE,
    TEMPLATE_ARG_NAME,
    TEMPLATE_PARAM_IS_PACK,
    TEMPLATE_PARAM_KIND,
    TEMPLATE_PARAM_NAME,
    TEMPLATE_PARAM_KEY_KIND,
    TEMPLATE_PRIMARY_USR,
    SPECIALIZATION_PRIMARY,
    TYPEDEF_IS_USING,
    VARIABLE_BITS,
    USING_SYMBOLS,
    USING_CLASS,
    RI_LAST,
    RI_FIRST = VERSION
};

static constexpr unsigned BlockIdCount = BI_LAST - BI_FIRST;
static constexpr unsigned RecordIDCount = RI_LAST - RI_FIRST;

} // mrdocs
} // clang

#endif
