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

#ifndef MRDOX_AST_BITCODEREADER_HPP
#define MRDOX_AST_BITCODEREADER_HPP

//
// This file implements a reader for parsing the
// mrdox internal representation from LLVM bitcode.
// The reader takes in a stream of bits and generates
// the set of infos that it represents.
//

#include <mrdox/Platform.hpp>
#include "BitcodeIDs.hpp"
#include <mrdox/Error.hpp>
#include <mrdox/Metadata.hpp>
#include <mrdox/Reporter.hpp>
#include <clang/AST/AST.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Bitstream/BitstreamReader.h>
#include <optional>

namespace clang {
namespace mrdox {

using Record = llvm::SmallVector<uint64_t, 1024>;

// Class to read bitstream into an InfoSet collection
class BitcodeReader
{
public:
    BitcodeReader(
        llvm::BitstreamCursor& Stream,
        Reporter& R)
        : R_(R)
        , Stream(Stream)
    {
    }

    // Main entry point, calls readBlock to read each block in the given stream.
    llvm::Expected<
        std::vector<std::unique_ptr<Info>>>
    getInfos();

public:
    struct AnyBlock;

    enum class Cursor
    {
        BadBlock = 1,
        Record,
        BlockEnd,
        BlockBegin
    };

    llvm::Error validateStream();
    llvm::Error readBlockInfoBlock();

    /** Return the next decoded Info from the stream.
    */
    template<class T>
    llvm::Expected<std::unique_ptr<Info>>
    readInfo(unsigned ID);

    /** Read a single block.

        Calls readRecord on each record found.
    */
    llvm::Error
    readBlock(AnyBlock& B, unsigned ID);

    /** Read a record into a data field.

        This calls parseRecord after casting.
    */
    llvm::Error
    readRecord(unsigned ID);

    // Helper function to step through blocks to find and dispatch the next record
    // or block to be read.
    Cursor skipUntilRecordOrBlock(unsigned &BlockOrRecordID);

public:
    Reporter& R_;
    llvm::BitstreamCursor& Stream;
    std::optional<llvm::BitstreamBlockInfo> BlockInfo;
    std::vector<AnyBlock*> blockStack_;
};

} // mrdox
} // clang

#endif
