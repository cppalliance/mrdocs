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

#include "BitcodeIDs.hpp"
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

class BitcodeWriter
{
public:
    explicit
    BitcodeWriter(llvm::BitstreamWriter &Stream);

    // Write a specific info to a bitcode stream.
    bool dispatchInfoForWrite(Info const* I);

    void emitHeader();
    void emitBlockInfoBlock();
    void emitVersionBlock();

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
    void emitBlock(Javadoc::Node const& I);
    void emitBlock(TemplateInfo const& T);
    void emitBlock(TemplateSpecializationInfo const& T);
    void emitBlock(TemplateParamInfo const& T);
    void emitBlock(Reference const& B, FieldId F);

    // Emission of validation and overview blocks.
    void emitRecordID(RecordId ID);
    void emitBlockID(BlockId ID);
    void emitBlockInfo(BlockId BID, const std::vector<RecordId> &RIDs);

    //--------------------------------------------
    //
    // Records
    //
    //--------------------------------------------

    template<class Integer>
    requires
        std::is_integral_v<Integer> &&
        (sizeof(Integer) > 4)
    void emitRecord(Integer Value, RecordId ID) = delete;

    template<class Integer>
    requires std::is_integral_v<Integer>
    void emitRecord(Integer Value, RecordId ID);

    template<class Enum>
    requires std::is_enum_v<Enum>
    void emitRecord(Enum Value, RecordId ID);

    void emitRecord(llvm::SmallVectorImpl<SymbolID> const& Values, RecordId ID);

    void emitRecord(SymbolID const& Str, RecordId ID);
    void emitRecord(StringRef Str, RecordId ID);
    void emitRecord(Location const& Loc, RecordId ID);
    void emitRecord(bool Value, RecordId ID);
    void emitRecord(TemplateInfo const& Templ);
    //void emitRecord(Reference const& Ref, RecordId ID);

    bool prepRecordData(RecordId ID, bool ShouldEmit = true);

    //--------------------------------------------

    // Emission of appropriate abbreviation type.
    void emitAbbrev(RecordId ID, BlockId Block);

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
    using RecordValue = std::uint32_t;
    SmallVector<
        RecordValue,
        BitCodeConstants::RecordSize> Record;
    llvm::BitstreamWriter& Stream;
    AbbreviationMap Abbrevs;
};

} // mrdox
} // clang

#endif
