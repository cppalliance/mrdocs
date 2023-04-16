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

//
// This file implements a writer for serializing the clang-doc internal
// representation to LLVM bitcode. The writer takes in a stream and emits the
// generated bitcode to that stream.
//

#ifndef MRDOX_SOURCE_AST_BITCODEWRITER_HPP
#define MRDOX_SOURCE_AST_BITCODEWRITER_HPP

#include <mrdox/MetadataFwd.hpp>
#include <mrdox/meta/Javadoc.hpp>
#include <clang/AST/AST.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Bitstream/BitstreamWriter.h>
#include <initializer_list>
#include <vector>

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
    static constexpr unsigned JavadocKindSize = 1U;
    static constexpr unsigned char Signature[4] = {'D', 'O', 'C', 'S'};
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
    BI_COMMENT_BLOCK_ID,
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
    JAVADOC_NODE_KIND,
    COMMENT_KIND,
    COMMENT_TEXT,
    COMMENT_NAME,
    COMMENT_DIRECTION,
    COMMENT_PARAMNAME,
    COMMENT_CLOSENAME,
    COMMENT_SELFCLOSING,
    COMMENT_EXPLICIT,
    COMMENT_ATTRKEY,
    COMMENT_ATTRVAL,
    COMMENT_ARG,
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

//------------------------------------------------

class BitcodeWriter
{
    // Emission of validation and overview blocks.
    void emitHeader();
    void emitVersionBlock();
    void emitRecordID(RecordId ID);
    void emitBlockID(BlockId ID);
    void emitBlockInfoBlock();
    void emitBlockInfo(BlockId BID, const std::vector<RecordId> &RIDs);

    // Emission of individual record types.
    void emitRecord(StringRef Str, RecordId ID);
    void emitRecord(SymbolID const& Str, RecordId ID);
    void emitRecord(Location const& Loc, RecordId ID);
    void emitRecord(Reference const& Ref, RecordId ID);
    void emitRecord(bool Value, RecordId ID);
    void emitRecord(int Value, RecordId ID);
    void emitRecord(unsigned Value, RecordId ID);
    void emitRecord(TemplateInfo const& Templ);

    void emitRecord(Javadoc::Text const& node);
    void emitRecord(Javadoc::StyledText const& node);
    void emitRecord(Javadoc::Paragraph const& node);
    void emitRecord(Javadoc::Brief const& node);
    void emitRecord(Javadoc::Admonition const& node);
    void emitRecord(Javadoc::Code const& node);
    void emitRecord(Javadoc::Returns const& node);
    void emitRecord(Javadoc::Param const& node);
    void emitRecord(Javadoc::TParam const& node);

    bool prepRecordData(RecordId ID, bool ShouldEmit = true);

    // Emission of appropriate abbreviation type.
    void emitAbbrev(RecordId ID, BlockId Block);

public:
    BitcodeWriter(
        llvm::BitstreamWriter &Stream)
        : Stream(Stream)
    {
        emitHeader();
        emitBlockInfoBlock();
        emitVersionBlock();
    }

    // Write a specific info to a bitcode stream.
    bool dispatchInfoForWrite(Info *I);

    // Block emission of different info types.

    void emitBlock(NamespaceInfo const& I);
    void emitBlock(RecordInfo const& I);
    void emitBlock(BaseRecordInfo const& I);
    void emitBlock(FunctionInfo const& I);
    void emitBlock(EnumInfo const& I);
    void emitBlock(EnumValueInfo const& I);
    void emitBlock(TypeInfo const& B);
    void emitBlock(TypedefInfo const& B);
    void emitBlock(FieldTypeInfo const& B);
    void emitBlock(MemberTypeInfo const& T);
    void emitBlock(Javadoc const& jd);
    template<class T>
    void emitBlock(List<T> const& list);
    void emitBlock(CommentInfo const& B);
    void emitBlock(TemplateInfo const& T);
    void emitBlock(TemplateSpecializationInfo const& T);
    void emitBlock(TemplateParamInfo const& T);
    void emitBlock(Reference const& B, FieldId F);

private:
    class AbbreviationMap
    {
        llvm::DenseMap<unsigned, unsigned> Abbrevs;

    public:
        AbbreviationMap()
            : Abbrevs(RecordIdCount)
        {
        }

        void add(RecordId RID, unsigned AbbrevID);
        unsigned get(RecordId RID) const;
    };

    class StreamSubBlockGuard
    {
        llvm::BitstreamWriter &Stream;

    public:
        ~StreamSubBlockGuard()
        {
            Stream.ExitBlock();
        }

        StreamSubBlockGuard(
            llvm::BitstreamWriter &Stream_,
            BlockId ID)
            : Stream(Stream_)
        {
            // NOTE: SubBlockIDSize could theoretically
            // be calculated on the fly, based on the
            // initialization list of records in each block.
            Stream.EnterSubblock(ID, BitCodeConstants::SubblockIDSize);
        }

        StreamSubBlockGuard(StreamSubBlockGuard &) = delete;
        StreamSubBlockGuard &operator=(StreamSubBlockGuard &) = delete;
    };

    // Static size is the maximum length of
    // the block/record names we're pushing
    // to this + 1. Longest is currently
    // `MemberTypeBlock` at 15 chars.
    //
    SmallVector<
        uint32_t,
        BitCodeConstants::RecordSize> Record;
    llvm::BitstreamWriter& Stream;
    AbbreviationMap Abbrevs;
};

} // mrdox
} // clang

#endif
