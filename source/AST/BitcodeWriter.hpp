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

#ifndef MRDOX_LIB_AST_BITCODEWRITER_HPP
#define MRDOX_LIB_AST_BITCODEWRITER_HPP

//
// This file implements a writer for serializing the
// mrdox internal representation to LLVM bitcode. The
// writer takes in a stream and emits the generated
// bitcode to that stream.
//

#include <mrdox/Platform.hpp>
#include "BitcodeIDs.hpp"
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/ADT/BitField.hpp>
#include <mrdox/Metadata/Javadoc.hpp>
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
    // Static size is the maximum length of
    // the block/record names we're pushing
    // to this + 1.
    //
    using RecordValue = std::uint32_t;
    using RecordType = SmallVector<
        RecordValue, BitCodeConstants::RecordSize>;

    explicit
    BitcodeWriter(llvm::BitstreamWriter &Stream);

    // Write a specific info to a bitcode stream.
    bool dispatchInfoForWrite(Info const* I);

    void emitHeader();
    void emitBlockInfoBlock();
    void emitVersionBlock();


    // Emission of validation and overview blocks.
    void emitRecordID(RecordID ID);
    void emitBlockID(BlockID ID);
    void emitBlockInfo(BlockID BID, const std::vector<RecordID> &RIDs);

    //--------------------------------------------
    //
    // Records
    //
    //--------------------------------------------

    template<class Integer>
    requires
        std::is_integral_v<Integer> &&
        (sizeof(Integer) > 4)
    void emitRecord(Integer Value, RecordID ID) = delete;

    template<class Integer>
    requires std::is_integral_v<Integer>
    void emitRecord(Integer Value, RecordID ID);

    template<class Enum>
    requires std::is_enum_v<Enum>
    void emitRecord(Enum Value, RecordID ID);

    void emitRecord(std::vector<SymbolID> const& Values, RecordID ID);
    void emitRecord(std::vector<MemberRef> const& list, RecordID ID);

    void emitRecord(SymbolID const& Str, RecordID ID);
    void emitRecord(StringRef Str, RecordID ID);
    void emitRecord(Location const& Loc, RecordID ID);
    void emitRecord(bool Value, RecordID ID);
    void emitRecord(TemplateInfo const& Templ);
    void emitRecord(std::initializer_list<BitFieldFullValue> values, RecordID ID);

    bool prepRecordData(RecordID ID, bool ShouldEmit = true);

    //--------------------------------------------

    // Emission of appropriate abbreviation type.
    void emitAbbrev(RecordID ID, BlockID Block);

    //--------------------------------------------
    //
    // emitRecord
    //
    //--------------------------------------------

    //--------------------------------------------
    //
    // Block emission
    //
    //--------------------------------------------

    template<class T>
    void emitBlock(AnyList<T> const& list);

    void emitInfoPart(Info const& I);
    void emitSymbolPart(SymbolInfo const& I);

    void emitBlock(BaseInfo const& I);
    void emitBlock(EnumInfo const& I);
    void emitBlock(EnumValueInfo const& I);
    void emitBlock(FunctionInfo const& I);
    void emitBlock(Param const& I);
    void emitBlock(std::unique_ptr<Javadoc> const& jd);
    void emitBlock(Javadoc::Node const& I);
    void emitBlock(NamespaceInfo const& I);
    void emitBlock(RecordInfo const& I);
    void emitBlock(Reference const& B, FieldId F);
    void emitBlock(TemplateInfo const& T);
    void emitBlock(TParam const& T);
    void emitBlock(TArg const& T);
    void emitBlock(TypedefInfo const& I);
    void emitBlock(TypeInfo const& I);
    void emitBlock(VarInfo const& I);
    void emitBlock(FieldInfo const& I);

    //--------------------------------------------

private:
    class AbbreviationMap
    {
        llvm::DenseMap<unsigned, unsigned> Abbrevs;

    public:
        AbbreviationMap()
            : Abbrevs(RecordIDCount)
        {
        }

        void add(RecordID RID, unsigned AbbrevID);
        unsigned get(RecordID RID) const;
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
            BlockID ID)
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

    RecordType Record;
    llvm::BitstreamWriter& Stream;
    AbbreviationMap Abbrevs;
};

} // mrdox
} // clang

#endif
