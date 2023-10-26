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

#include "AnyBlock.hpp"
#include "BitcodeReader.hpp"
#include "lib/Support/Debug.hpp"
#include "lib/Support/Error.hpp"
#include <mrdocs/Support/Error.hpp>

namespace clang {
namespace mrdocs {

// Entry point
mrdocs::Expected<std::vector<std::unique_ptr<Info>>>
BitcodeReader::
getInfos()
{
    std::vector<std::unique_ptr<Info>> Infos;
    if (auto err = validateStream())
    {
        return Unexpected(err);
    }

    // Read the top level blocks.
    while (!Stream.AtEndOfStream())
    {
        llvm::Expected<unsigned> MaybeCode = Stream.ReadCode();
        if (!MaybeCode)
            return Unexpected(toError(MaybeCode.takeError()));
        if (MaybeCode.get() != llvm::bitc::ENTER_SUBBLOCK)
            return Unexpected(formatError("no blocks in input"));
        llvm::Expected<unsigned> MaybeID = Stream.ReadSubBlockID();
        if (!MaybeID)
            return Unexpected(toError(MaybeID.takeError()));
        unsigned ID = MaybeID.get();
        switch (ID)
        {
        // Top level Version is first
        case BI_VERSION_BLOCK_ID:
        {
            VersionBlock B;
            if (auto err = readBlock(B, ID))
                return Unexpected(std::move(err));
            continue;
        }
        case llvm::bitc::BLOCKINFO_BLOCK_ID:
        {
            if (auto err = readBlockInfoBlock())
                return Unexpected(std::move(err));
            continue;
        }
        // Top level Info blocks
        case BI_NAMESPACE_BLOCK_ID:
        {
            MRDOCS_TRY(auto I, readInfo<NamespaceBlock>(ID));
            Infos.emplace_back(std::move(I));
            continue;
        }
        case BI_RECORD_BLOCK_ID:
        {
            MRDOCS_TRY(auto I, readInfo<RecordBlock>(ID));
            Infos.emplace_back(std::move(I));
            continue;
        }
        case BI_FUNCTION_BLOCK_ID:
        {
            MRDOCS_TRY(auto I, readInfo<FunctionBlock>(ID));
            Infos.emplace_back(std::move(I));
            continue;
        }
        case BI_TYPEDEF_BLOCK_ID:
        {
            MRDOCS_TRY(auto I, readInfo<TypedefBlock>(ID));
            Infos.emplace_back(std::move(I));
            continue;
        }
        case BI_ENUM_BLOCK_ID:
        {
            MRDOCS_TRY(auto I, readInfo<EnumBlock>(ID));
            Infos.emplace_back(std::move(I));
            continue;
        }
        case BI_VARIABLE_BLOCK_ID:
        {
            MRDOCS_TRY(auto I, readInfo<VarBlock>(ID));
            Infos.emplace_back(std::move(I));
            continue;
        }
        // although fields can only be members of records,
        // they are emitted as top-level blocks anyway
        case BI_FIELD_BLOCK_ID:
        {
            MRDOCS_TRY(auto I, readInfo<FieldBlock>(ID));
            Infos.emplace_back(std::move(I));
            continue;
        }

        case BI_SPECIALIZATION_BLOCK_ID:
        {
            MRDOCS_TRY(auto I, readInfo<SpecializationBlock>(ID));
            Infos.emplace_back(std::move(I));
            continue;
        }
        default:
            // return formatError("invalid top level block");
            if (llvm::Error err = Stream.SkipBlock())
            {
                // FIXME this drops the error on the floor.
                consumeError(std::move(err));
            }
            continue;
        }
    }
    return std::move(Infos);
}

//------------------------------------------------

Error
BitcodeReader::
validateStream()
{
    if (Stream.AtEndOfStream())
        return formatError("premature end of stream");

    // Sniff for the signature.
    for (int i = 0; i != 4; ++i)
    {
        llvm::Expected<llvm::SimpleBitstreamCursor::word_t> MaybeRead = Stream.Read(8);
        if (!MaybeRead)
            return toError(MaybeRead.takeError());
        else if (MaybeRead.get() != BitCodeConstants::Signature[i])
            return formatError("invalid bitcode signature");
    }
    return Error::success();
}

Error
BitcodeReader::
readBlockInfoBlock()
{
    llvm::Expected<std::optional<llvm::BitstreamBlockInfo>> MaybeBlockInfo =
        Stream.ReadBlockInfoBlock();
    if (!MaybeBlockInfo)
        return toError(MaybeBlockInfo.takeError());
    BlockInfo = MaybeBlockInfo.get();
    if (!BlockInfo)
        return formatError("unable to parse BlockInfoBlock");
    Stream.setBlockInfo(&*BlockInfo);
    return Error::success();
}

//------------------------------------------------

template<class T>
mrdocs::Expected<std::unique_ptr<Info>>
BitcodeReader::
readInfo(
    unsigned ID)
{
    T B(*this);
    if(auto err = readBlock(B, ID))
        return Unexpected(err);
    return std::move(B.I);
}

Error
BitcodeReader::
readBlock(
    AnyBlock& B, unsigned ID)
{
    blockStack_.push_back(&B);
    if (auto err = Stream.EnterSubBlock(ID))
        return toError(std::move(err));

    for(;;)
    {
        unsigned BlockOrCode = 0;
        Cursor Res = skipUntilRecordOrBlock(BlockOrCode);

        switch (Res)
        {
        case Cursor::BadBlock:
            return formatError("bad block found");
        case Cursor::BlockEnd:
            blockStack_.pop_back();
            return Error::success();
        case Cursor::BlockBegin:
            if (auto err = blockStack_.back()->readSubBlock(BlockOrCode))
            {
                if (llvm::Error Skipped = Stream.SkipBlock())
                {
                    return toError(std::move(Skipped));
                }
                return err;
            }
            continue;
        case Cursor::Record:
            break;
        }
        if (auto err = readRecord(BlockOrCode))
            return err;
    }
}

//------------------------------------------------

// Read records from bitcode into AnyBlock
Error
BitcodeReader::
readRecord(unsigned ID)
{
    Record R;
    llvm::StringRef Blob;
    llvm::Expected<unsigned> MaybeRecID =
        Stream.readRecord(ID, R, &Blob);
    if (!MaybeRecID)
        return toError(MaybeRecID.takeError());
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
        llvm::Expected<unsigned> MaybeCode = Stream.ReadCode();
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
            if (llvm::Expected<unsigned> MaybeID = Stream.ReadSubBlockID())
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
            if (llvm::Error err = Stream.ReadAbbrevRecord())
            {
                // FIXME this drops the error on the floor.
                consumeError(std::move(err));
            }
            continue;
        case llvm::bitc::UNABBREV_RECORD:
            return Cursor::BadBlock;
        case llvm::bitc::FIRST_APPLICATION_ABBREV:
            // Unexpected abbrev id
            MRDOCS_UNREACHABLE();
        }
    }
    // Premature stream end
    MRDOCS_UNREACHABLE();
}

//------------------------------------------------

// Calls readBlock to read each block in the given bitcode.
mrdocs::Expected<std::vector<std::unique_ptr<Info>>>
readBitcode(llvm::StringRef bitcode)
{
    llvm::BitstreamCursor Stream(bitcode);
    BitcodeReader reader(Stream);
    return reader.getInfos();
}

} // mrdocs
} // clang
