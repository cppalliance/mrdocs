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
        case BI_FRIEND_BLOCK_ID:
        {
            MRDOCS_TRY(auto I, readInfo<FriendBlock>(ID));
            Infos.emplace_back(std::move(I));
            continue;
        }
        case BI_NAMESPACE_ALIAS_BLOCK_ID:
        {
            MRDOCS_TRY(auto I, readInfo<NamespaceAliasBlock>(ID));
            Infos.emplace_back(std::move(I));
            continue;
        }
        case BI_USING_BLOCK_ID:
        {
            MRDOCS_TRY(auto I, readInfo<UsingBlock>(ID));
            Infos.emplace_back(std::move(I));
            continue;
        }

        case BI_ENUMERATOR_BLOCK_ID:
        {
            MRDOCS_TRY(auto I, readInfo<EnumeratorBlock>(ID));
            Infos.emplace_back(std::move(I));
            continue;
        }
        case BI_GUIDE_BLOCK_ID:
        {
            MRDOCS_TRY(auto I, readInfo<GuideBlock>(ID));
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
    return std::unique_ptr<Info>(B.I.release());
}

Error
BitcodeReader::
readBlock(
    AnyBlock& B, unsigned ID)
{
    if (auto err = Stream.EnterSubBlock(ID))
        return toError(std::move(err));
    blockStack_.push_back(&B);
    Record RecordData;
    for(;;)
    {
        llvm::Expected<llvm::BitstreamEntry> MaybeEntry = Stream.advance();
        if(! MaybeEntry)
            return toError(MaybeEntry.takeError());
        switch(llvm::BitstreamEntry Entry = MaybeEntry.get(); Entry.Kind)
        {
        case llvm::BitstreamEntry::Record:
        {
            llvm::StringRef Blob;
            llvm::Expected<unsigned> MaybeRecordID =
                Stream.readRecord(Entry.ID, RecordData, &Blob);
            if (! MaybeRecordID)
                return toError(MaybeRecordID.takeError());
            if(auto err = blockStack_.back()->parseRecord(
                RecordData, MaybeRecordID.get(), Blob))
                return err;
            RecordData.clear();
            continue;
        }
        case llvm::BitstreamEntry::SubBlock:
        {
            if(auto err = blockStack_.back()->readSubBlock(Entry.ID))
            {
                if(auto skip_err = Stream.SkipBlock())
                    return toError(std::move(skip_err));
                return err;
            }
            continue;
        }
        case llvm::BitstreamEntry::EndBlock:
            blockStack_.pop_back();
            return Error::success();
        case llvm::BitstreamEntry::Error:
            return formatError("bad block found");
        default:
            MRDOCS_UNREACHABLE();
        }
    }
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
