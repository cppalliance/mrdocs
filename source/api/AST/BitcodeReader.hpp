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

#ifndef MRDOX_API_AST_BITCODEREADER_HPP
#define MRDOX_API_AST_BITCODEREADER_HPP

//
// This file implements a reader for parsing the
// mrdox internal representation from LLVM bitcode.
// The reader takes in a stream of bits and generates
// the set of infos that it represents.
//

#include <mrdox/Platform.hpp>
#include "AnyNodeList.hpp"
#include "BitcodeIDs.hpp"
#include "ParseJavadoc.hpp"
#include <mrdox/Error.hpp>
#include <mrdox/Metadata.hpp>
#include <mrdox/Reporter.hpp>
#include <clang/AST/AST.h>
#include <llvm/ADT/IndexedMap.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Bitstream/BitstreamReader.h>

namespace clang {
namespace mrdox {

using Record = llvm::SmallVector<uint64_t, 1024>;

//------------------------------------------------

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

private:
    enum class Cursor
    {
        BadBlock = 1,
        Record,
        BlockEnd,
        BlockBegin
    };

    llvm::Error validateStream();
    llvm::Error readBlockInfoBlock();

    /** Read the next Info

        Calls createInfo after casting.
    */
    llvm::Expected<std::unique_ptr<Info>>
    readBlockToInfo(unsigned ID);

    /** Return T from reading the stream.
    */
    template <typename T>
    llvm::Expected<std::unique_ptr<Info>>
    createInfo(unsigned ID);

    /** Read a single block.

        Calls readRecord on each record found.
    */
    template<typename T>
    llvm::Error
    readBlock(unsigned ID, T I);

    // Step through a block of records to find the next data field.
    template<typename T>
    llvm::Error
    readSubBlock(unsigned ID, T I);

    /** Read a record into a data field.

        This calls parseRecord after casting.
    */
    template<typename T>
    llvm::Error
    readRecord(unsigned ID, T I);

    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, const unsigned VersionNo);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, NamespaceInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, RecordInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, BaseRecordInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, FunctionInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, EnumInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, EnumValueInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, TypedefInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, TypeInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, FieldTypeInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, MemberTypeInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, Reference* I, FieldId& F);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, TemplateInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, TemplateSpecializationInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, TemplateParamInfo* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, llvm::Optional<Javadoc>* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, AnyNodeList* I);
    llvm::Error parseRecord(Record const& R, unsigned ID,
        llvm::StringRef Blob, AnyNodeList::Nodes* I);

    // Helper function to step through blocks to find and dispatch the next record
    // or block to be read.
    Cursor skipUntilRecordOrBlock(unsigned &BlockOrRecordID);

private:
    Reporter& R_;
    llvm::BitstreamCursor& Stream;
    llvm::Optional<llvm::BitstreamBlockInfo> BlockInfo;
    FieldId CurrentReferenceField;
    llvm::Optional<Javadoc>* javadoc_ = nullptr;
    AnyNodeList* nodes_ = nullptr;
};

//------------------------------------------------

} // mrdox
} // clang

#endif
