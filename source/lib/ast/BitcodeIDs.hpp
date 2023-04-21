//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_SOURCE_AST_BITCODEIDS_HPP
#define MRDOX_SOURCE_AST_BITCODEIDS_HPP

#include <llvm/Bitstream/BitCodeEnums.h>

namespace clang {
namespace mrdox {

// Current version number of clang-doc bitcode.
// Should be bumped when removing or changing BlockIds, RecordIds, or
// BitCodeConstants, though they can be added without breaking it.
static const unsigned VersionNumber = 3;

struct BitCodeConstants
{
    static constexpr unsigned RecordSize = 32U;
    static constexpr unsigned SignatureBitSize = 8U;
    static constexpr unsigned SubblockIDSize = 4U;
    static constexpr unsigned BoolSize = 1U;
    static constexpr unsigned IntSize = 16U;
    static constexpr unsigned StringLengthSize = 16U;
    static constexpr unsigned FilenameLengthSize = 16U;
    static constexpr unsigned LineNumberSize = 32U;
    static constexpr unsigned ReferenceTypeSize = 8U;
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
enum BlockId
{
    BI_VERSION_BLOCK_ID = llvm::bitc::FIRST_APPLICATION_BLOCKID,
    BI_NAMESPACE_BLOCK_ID,
    BI_ENUM_BLOCK_ID,
    BI_ENUM_VALUE_BLOCK_ID,
    BI_TYPE_BLOCK_ID,
    BI_FIELD_TYPE_BLOCK_ID,
    BI_MEMBER_TYPE_BLOCK_ID,
    BI_RECORD_BLOCK_ID,
    BI_BASE_RECORD_BLOCK_ID,
    BI_FUNCTION_BLOCK_ID,
    BI_JAVADOC_BLOCK_ID,
    BI_JAVADOC_LIST_BLOCK_ID,
    BI_JAVADOC_NODE_BLOCK_ID,
    BI_REFERENCE_BLOCK_ID,
    BI_TEMPLATE_BLOCK_ID,
    BI_TEMPLATE_SPECIALIZATION_BLOCK_ID,
    BI_TEMPLATE_PARAM_BLOCK_ID,
    BI_TYPEDEF_BLOCK_ID,
    BI_LAST,
    BI_FIRST = BI_VERSION_BLOCK_ID
};

// New Ids need to be added to the enum here, and to the relevant IdNameMap and
// initialization list in the implementation file.
enum RecordId
{
    VERSION = 1,
    FUNCTION_USR,
    FUNCTION_NAME,
    FUNCTION_DEFLOCATION,
    FUNCTION_LOCATION,
    FUNCTION_ACCESS,
    FUNCTION_IS_METHOD,
    JAVADOC_LIST_KIND,
    JAVADOC_NODE_KIND,
    JAVADOC_NODE_STRING,
    JAVADOC_NODE_STYLE,
    JAVADOC_NODE_ADMONISH,
    FIELD_TYPE_NAME,
    FIELD_DEFAULT_VALUE,
    MEMBER_TYPE_NAME,
    MEMBER_TYPE_ACCESS,
    NAMESPACE_USR,
    NAMESPACE_NAME,
    NAMESPACE_PATH,
    ENUM_USR,
    ENUM_NAME,
    ENUM_DEFLOCATION,
    ENUM_LOCATION,
    ENUM_SCOPED,
    ENUM_VALUE_NAME,
    ENUM_VALUE_VALUE,
    ENUM_VALUE_EXPR,
    RECORD_USR,
    RECORD_NAME,
    RECORD_PATH,
    RECORD_DEFLOCATION,
    RECORD_LOCATION,
    RECORD_TAG_TYPE,
    RECORD_IS_TYPE_DEF,
    BASE_RECORD_USR,
    BASE_RECORD_NAME,
    BASE_RECORD_PATH,
    BASE_RECORD_TAG_TYPE,
    BASE_RECORD_IS_VIRTUAL,
    BASE_RECORD_ACCESS,
    BASE_RECORD_IS_PARENT,
    REFERENCE_USR,
    REFERENCE_NAME,
    REFERENCE_TYPE,
    REFERENCE_PATH,
    REFERENCE_FIELD,
    TEMPLATE_PARAM_CONTENTS,
    TEMPLATE_SPECIALIZATION_OF,
    TYPEDEF_USR,
    TYPEDEF_NAME,
    TYPEDEF_DEFLOCATION,
    TYPEDEF_IS_USING,
    RI_LAST,
    RI_FIRST = VERSION
};

static constexpr unsigned BlockIdCount = BI_LAST - BI_FIRST;
static constexpr unsigned RecordIdCount = RI_LAST - RI_FIRST;

// Identifiers for differentiating between subblocks
enum class FieldId
{
    F_default,
    F_namespace,
    F_parent,
    F_vparent,
    F_type,
    F_child_namespace,
    F_child_record,
    F_child_function
};

} // mrdox
} // clang

#endif
