//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_AST_BITCODEIDS_HPP
#define MRDOX_AST_BITCODEIDS_HPP

#include <mrdox/Platform.hpp>
#include <llvm/Bitstream/BitCodeEnums.h>

namespace clang {
namespace mrdox {

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
enum BlockID
{
    BI_VERSION_BLOCK_ID = llvm::bitc::FIRST_APPLICATION_BLOCKID,

    BI_INFO_PART_ID,
    BI_SYMBOL_PART_ID,

    BI_BASE_BLOCK_ID,
    BI_ENUM_BLOCK_ID,
    BI_ENUM_VALUE_BLOCK_ID,
    BI_FIELD_BLOCK_ID,
    BI_FUNCTION_BLOCK_ID,
    BI_FUNCTION_PARAM_BLOCK_ID,
    BI_JAVADOC_BLOCK_ID,
    BI_JAVADOC_LIST_BLOCK_ID,
    BI_JAVADOC_NODE_BLOCK_ID,
    BI_NAMESPACE_BLOCK_ID,
    BI_RECORD_BLOCK_ID,
    BI_REFERENCE_BLOCK_ID,
    BI_TEMPLATE_ARG_BLOCK_ID,
    BI_TEMPLATE_BLOCK_ID,
    BI_TEMPLATE_PARAM_BLOCK_ID,
    BI_TYPE_BLOCK_ID,
    BI_TYPEDEF_BLOCK_ID,
    BI_VARIABLE_BLOCK_ID,
    BI_LAST,
    BI_FIRST = BI_VERSION_BLOCK_ID
};

// New IDs need to be added to the enum here, and to the relevant IdNameMap and
// initialization list in the implementation file.
enum RecordID
{
    VERSION = 1,

    INFO_PART_ID,
    INFO_PART_NAME,
    INFO_PART_PARENTS,
    SYMBOL_PART_DEFLOC,
    SYMBOL_PART_LOC,
    BASE_ACCESS,
    BASE_ID,
    BASE_IS_VIRTUAL,
    BASE_NAME,
    FIELD_ATTRIBUTES,
    FIELD_DEFAULT,
    FIELD_NAME,
    FUNCTION_BITS,
    FUNCTION_PARAM_NAME,
    FUNCTION_PARAM_DEFAULT,
    JAVADOC_LIST_KIND,
    JAVADOC_NODE_ADMONISH,
    JAVADOC_NODE_KIND,
    JAVADOC_NODE_STRING,
    JAVADOC_NODE_STYLE,
    JAVADOC_PARAM_DIRECTION,
    ENUM_SCOPED,
    ENUM_VALUE_NAME,
    ENUM_VALUE_VALUE,
    ENUM_VALUE_EXPR,
    RECORD_BITS,
    RECORD_FRIENDS,
    RECORD_IS_TYPE_DEF,
    RECORD_KEY_KIND,
    REFERENCE_FIELD,
    REFERENCE_NAME,
    REFERENCE_TYPE,
    REFERENCE_USR,
    RECORD_ENUMS,
    RECORD_FUNCTIONS,
    RECORD_RECORDS,
    RECORD_TYPES,
    RECORD_VARS,
    RECORD_FIELDS,
    TEMPLATE_ARG_VALUE,
    TEMPLATE_PARAM_DEFAULT,
    TEMPLATE_PARAM_IS_PACK,
    TEMPLATE_PARAM_KIND,
    TEMPLATE_PARAM_NAME,
    TEMPLATE_PRIMARY_USR,
    TYPEDEF_IS_USING,
    VARIABLE_BITS,
    RI_LAST,
    RI_FIRST = VERSION
};

static constexpr unsigned BlockIdCount = BI_LAST - BI_FIRST;
static constexpr unsigned RecordIDCount = RI_LAST - RI_FIRST;

// Identifiers for differentiating between subblocks
enum class FieldId
{
    F_default,
    F_namespace,
    F_vparent,
    F_type,
    F_child_namespace,
    F_child_record,
    F_child_function,
    F_child_typedef,
    F_child_enum,
    F_child_variable
};

} // mrdox
} // clang

#endif
