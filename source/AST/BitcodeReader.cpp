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

#include "BitcodeReader.hpp"
#include "AnyBlock.hpp"
#include "DecodeRecord.hpp"
#include "Support/Debug.hpp"

namespace clang {
namespace mrdox {

// Entry point
llvm::Expected<
    std::vector<std::unique_ptr<Info>>>
BitcodeReader::
getInfos()
{
    std::vector<std::unique_ptr<Info>> Infos;
    if (auto Err = validateStream())
        return std::move(Err);

    // Read the top level blocks.
    while (!Stream.AtEndOfStream())
    {
        Expected<unsigned> MaybeCode = Stream.ReadCode();
        if (!MaybeCode)
            return MaybeCode.takeError();
        if (MaybeCode.get() != llvm::bitc::ENTER_SUBBLOCK)
            return makeError("no blocks in input");
        Expected<unsigned> MaybeID = Stream.ReadSubBlockID();
        if (!MaybeID)
            return MaybeID.takeError();
        unsigned ID = MaybeID.get();
        switch (ID)
        {
        // Top level Version is first
        case BI_VERSION_BLOCK_ID:
        {
            VersionBlock B;
            if (auto Err = readBlock(B, ID))
                return std::move(Err);
            continue;
        }

        // Top level blocks
        case BI_NAMESPACE_BLOCK_ID:
        {
            auto I = readInfo<NamespaceBlock>(ID);
            if(! I)
                return I.takeError();
            Infos.emplace_back(std::move(I.get()));
                continue;
        }
        case BI_RECORD_BLOCK_ID:
        {
            auto I = readInfo<RecordBlock>(ID);
            if(! I)
                return I.takeError();
            Infos.emplace_back(std::move(I.get()));
                continue;
        }
        case BI_FUNCTION_BLOCK_ID:
        {
            auto I = readInfo<FunctionBlock>(ID);
            if(! I)
                return I.takeError();
            Infos.emplace_back(std::move(I.get()));
                continue;
        }
        case BI_TYPEDEF_BLOCK_ID:
        {
            auto I = readInfo<TypedefBlock>(ID);
            if(! I)
                return I.takeError();
            Infos.emplace_back(std::move(I.get()));
                continue;
        }
        case BI_ENUM_BLOCK_ID:
        {
            auto I = readInfo<EnumBlock>(ID);
            if(! I)
                return I.takeError();
            Infos.emplace_back(std::move(I.get()));
                continue;
        }
        case BI_VARIABLE_BLOCK_ID:
        {
            auto I = readInfo<VarBlock>(ID);
            if(! I)
                return I.takeError();
            Infos.emplace_back(std::move(I.get()));
                continue;
        }

        // NamedType and Comment blocks should
        // not appear at the top level
        case BI_TYPE_BLOCK_ID:
        case BI_FIELD_TYPE_BLOCK_ID:
        case BI_MEMBER_TYPE_BLOCK_ID:
        case BI_JAVADOC_BLOCK_ID:
        case BI_JAVADOC_LIST_BLOCK_ID:
        case BI_JAVADOC_NODE_BLOCK_ID:
        case BI_REFERENCE_BLOCK_ID:
            return makeError("invalid top level block");
        case llvm::bitc::BLOCKINFO_BLOCK_ID:
            if (auto Err = readBlockInfoBlock())
                return std::move(Err);
            continue;
        default:
            if (llvm::Error Err = Stream.SkipBlock()) {
                // FIXME this drops the error on the floor.
                consumeError(std::move(Err));
            }
            continue;
        }
    }
    return std::move(Infos);
}

//------------------------------------------------

llvm::Error
BitcodeReader::
validateStream()
{
    if (Stream.AtEndOfStream())
        return makeError("premature end of stream");

    // Sniff for the signature.
    for (int i = 0; i != 4; ++i)
    {
        Expected<llvm::SimpleBitstreamCursor::word_t> MaybeRead = Stream.Read(8);
        if (!MaybeRead)
            return MaybeRead.takeError();
        else if (MaybeRead.get() != BitCodeConstants::Signature[i])
            return makeError("invalid bitcode signature");
    }
    return llvm::Error::success();
}

llvm::Error
BitcodeReader::
readBlockInfoBlock()
{
    Expected<llvm::Optional<llvm::BitstreamBlockInfo>> MaybeBlockInfo =
        Stream.ReadBlockInfoBlock();
    if (!MaybeBlockInfo)
        return MaybeBlockInfo.takeError();
    BlockInfo = MaybeBlockInfo.get();
    if (!BlockInfo)
        return makeError("unable to parse BlockInfoBlock");
    Stream.setBlockInfo(&*BlockInfo);
    return llvm::Error::success();
}

//------------------------------------------------

template<class T>
llvm::Expected<std::unique_ptr<Info>>
BitcodeReader::
readInfo(
    unsigned ID)
{
    T B(*this);
    if(auto Err = readBlock(B, ID))
        return Err;
    return std::move(B.I);
}

llvm::Error
BitcodeReader::
readBlock(
    AnyBlock& B, unsigned ID)
{
    blockStack_.push_back(&B);
    if (auto err = Stream.EnterSubBlock(ID))
        return err;

    for(;;)
    {
        unsigned BlockOrCode = 0;
        Cursor Res = skipUntilRecordOrBlock(BlockOrCode);

        switch (Res)
        {
        case Cursor::BadBlock:
            return makeError("bad block found");
        case Cursor::BlockEnd:
            blockStack_.pop_back();
            return llvm::Error::success();
        case Cursor::BlockBegin:
            if (auto Err = blockStack_.back()->readSubBlock(BlockOrCode))
            {
                if (llvm::Error Skipped = Stream.SkipBlock())
                    return joinErrors(std::move(Err), std::move(Skipped));
                return Err;
            }
            continue;
        case Cursor::Record:
            break;
        }
        if (auto Err = readRecord(BlockOrCode))
            return Err;
    }
}

//------------------------------------------------

// Read records from bitcode into AnyBlock
llvm::Error
BitcodeReader::
readRecord(unsigned ID)
{
    Record R;
    llvm::StringRef Blob;
    llvm::Expected<unsigned> MaybeRecID =
        Stream.readRecord(ID, R, &Blob);
    if (!MaybeRecID)
        return MaybeRecID.takeError();
    return blockStack_.back()->parseRecord(R, MaybeRecID.get(), Blob);
}

//------------------------------------------------

auto
BitcodeReader::
skipUntilRecordOrBlock(
    unsigned& BlockOrRecordID) ->
        Cursor
{
    BlockOrRecordID = 0;

    while (!Stream.AtEndOfStream())
    {
        Expected<unsigned> MaybeCode = Stream.ReadCode();
        if (!MaybeCode)
        {
            // FIXME this drops the error on the floor.
            consumeError(MaybeCode.takeError());
            return Cursor::BadBlock;
        }

        unsigned Code = MaybeCode.get();
        if (Code >= static_cast<unsigned>(llvm::bitc::FIRST_APPLICATION_ABBREV))
        {
            BlockOrRecordID = Code;
            return Cursor::Record;
        }
        switch (static_cast<llvm::bitc::FixedAbbrevIDs>(Code))
        {
        case llvm::bitc::ENTER_SUBBLOCK:
            if (Expected<unsigned> MaybeID = Stream.ReadSubBlockID())
                BlockOrRecordID = MaybeID.get();
            else {
                // FIXME this drops the error on the floor.
                consumeError(MaybeID.takeError());
            }
            return Cursor::BlockBegin;
        case llvm::bitc::END_BLOCK:
            if (Stream.ReadBlockEnd())
                return Cursor::BadBlock;
            return Cursor::BlockEnd;
        case llvm::bitc::DEFINE_ABBREV:
            if (llvm::Error Err = Stream.ReadAbbrevRecord()) {
                // FIXME this drops the error on the floor.
                consumeError(std::move(Err));
            }
            continue;
        case llvm::bitc::UNABBREV_RECORD:
            return Cursor::BadBlock;
        case llvm::bitc::FIRST_APPLICATION_ABBREV:
            llvm_unreachable("Unexpected abbrev id.");
        }
    }
    llvm_unreachable("Premature stream end.");
}

//------------------------------------------------

// Calls readBlock to read each block in the given bitcode.
llvm::Expected<
    std::vector<std::unique_ptr<Info>>>
readBitcode(
    llvm::StringRef bitcode,
    Reporter& R)
{
    llvm::BitstreamCursor Stream(bitcode);
    BitcodeReader reader(Stream, R);
    return reader.getInfos();
}

} // mrdox
} // clang
