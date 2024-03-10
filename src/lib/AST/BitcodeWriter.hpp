//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_AST_BITCODEWRITER_HPP
#define MRDOCS_LIB_AST_BITCODEWRITER_HPP

//
// This file implements a writer for serializing the
// mrdocs internal representation to LLVM bitcode. The
// writer takes in a stream and emits the generated
// bitcode to that stream.
//
#include "BitcodeIDs.hpp"
#include <mrdocs/Metadata.hpp>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/IndexedMap.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Bitstream/BitstreamWriter.h>
#include <initializer_list>
#include <vector>

namespace clang {
namespace mrdocs {

/** A writer for serializing the mrdocs' `Info` representation to LLVM bitcode.

    This class takes in a llvm::BitstreamWriter stream and
    emits the generated bitcode to that stream.

    The function `dispatchInfoForWrite` is used to dispatch
    the appropriate write function for the given `Info` object.

    After calling `dispatchInfoForWrite`, the buffer used to
    create the initial llvm::BitstreamWriter will be populated with
    the serialized bitcode for the given `Info` object.

 */
class BitcodeWriter
{
public:
    // Static size is the maximum length of
    // the block/record names we're pushing
    // to this + 1.
    //
    using RecordValue = std::uint32_t;
    using RecordType = llvm::SmallVector<
        RecordValue, BitCodeConstants::RecordSize>;

    explicit
    BitcodeWriter(llvm::BitstreamWriter &Stream);

    /** Write a specific Info to a bitcode stream.

        This function takes in an `Info` object and
        emits the generated bitcode to the internal
        llvm::BitstreamWriter stream.

        The function will dispatch the appropriate
        write function for the given `Info` object.

        After calling `dispatchInfoForWrite`, the
        buffer used to create the initial
        llvm::BitstreamWriter will be populated with
        the serialized bitcode for the given
        `Info` object.

        @param I The info to write.
    */
    bool dispatchInfoForWrite(const Info& I);

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
    requires std::integral<Integer>
    void emitRecord(Integer Value, RecordID ID);

    template<class Enum>
    requires std::is_enum_v<Enum>
    void emitRecord(Enum Value, RecordID ID);

    void emitRecord(
        std::vector<SymbolID> const& Values,
        RecordID ID);

    void emitRecord(SymbolID const& Str, RecordID ID);
    void emitRecord(llvm::StringRef Str, RecordID ID);
    void emitRecord(Location const& Loc, RecordID ID);
    void emitRecord(NoexceptInfo const& I, RecordID ID);
    void emitRecord(bool Value, RecordID ID);
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

    template<typename T>
    void emitBlock(doc::List<T> const& list);

    void emitInfoPart(Info const& I);
    void emitSourceInfo(SourceInfo const& S);
    void emitScopeInfo(ScopeInfo const& S);
    void
    emitLookup(
        llvm::StringRef Name,
        std::vector<SymbolID> const& Members);

    void emitBlock(BaseInfo const& I);
    void emitBlock(EnumInfo const& I);
    void emitBlock(FunctionInfo const& I);
    void emitBlock(GuideInfo const& I);
    void emitBlock(Param const& I);
    void emitBlock(std::unique_ptr<Javadoc> const& jd);
    void emitBlock(doc::Node const& I);
    void emitBlock(NamespaceInfo const& I);
    void emitBlock(RecordInfo const& I);
    void emitBlock(SpecializationInfo const& T);
    void emitBlock(TemplateInfo const& T);
    void emitBlock(std::unique_ptr<TParam> const& T);
    void emitBlock(std::unique_ptr<TArg> const& T);
    void emitBlock(TypedefInfo const& I);
    void emitBlock(VariableInfo const& I);
    void emitBlock(FieldInfo const& I);
    void emitBlock(FriendInfo const& I);
    void emitBlock(EnumeratorInfo const& I);
    void emitBlock(NameInfo const& I);
    void emitBlock(NamespaceAliasInfo const& I);
    void emitBlock(UsingInfo const& I);

    void emitBlock(std::unique_ptr<TypeInfo> const& TI);
    void emitBlock(std::unique_ptr<TypeInfo> const& TI, BlockID ID);

    template<typename ExprInfoTy>
        requires std::derived_from<ExprInfoTy, ExprInfo>
    void emitBlock(ExprInfoTy const& E);
    template<typename ExprInfoTy>
        requires std::derived_from<ExprInfoTy, ExprInfo>
    void emitBlock(ExprInfoTy const& E, BlockID ID);

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

} // mrdocs
} // clang

#endif
