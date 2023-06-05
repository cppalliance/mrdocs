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

#ifndef MRDOX_AST_BITCODE_HPP
#define MRDOX_AST_BITCODE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Support/Expected.hpp>
#include <clang/Tooling/Execution.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <memory>
#include <vector>

namespace llvm {
class BitstreamWriter;
class BitstreamCursor;
} // llvm

namespace clang {
namespace mrdox {

/** Contains metadata for one symbol, serialized to bitcode.

    Because multiple translation units can include
    the same header files, it is generally the case
    that there will be multiple bitcodes for each
    unique symbol. These get merged later.
*/
struct Bitcode
{
    /** The symbol ID for this bitcode.
    */
    SymbolID id;

    /** The serialized bitcode.

        We use small string here to avoid
        making needless copies.
    */
    llvm::SmallString<0> data;

    Bitcode() noexcept = default;

    Bitcode(
        SymbolID const& id_,
        llvm::SmallString<0>&& data_) noexcept
        : id(id_)
        , data(std::move(data_))
    {
    }

    bool empty() const noexcept
    {
        return data.empty();
    }
};

/** A collection of bitcodes, keyed by ID.
*/
using Bitcodes = llvm::StringMap<std::vector<StringRef>>;

/** Return the serialized bitcode for a metadata node.
*/
Bitcode
writeBitcode(
    Info const& I);

/** Return an array of Info read from a bitstream.
*/
mrdox::Expected<std::vector<std::unique_ptr<Info>>>
readBitcode(llvm::StringRef bitcode);

/** Store a key/value pair in the tool results.

    This function inserts the bitcode for the
    specified symbol ID into the tool results
    of the execution context.

    Each symbol ID can have multiple bitcodes.
*/
void
insertBitcode(
    tooling::ExecutionContext& ex,
    Bitcode&& bitcode);

/** Return the bitcodes grouped by matching ID.

    Each ID may have one or more associated
    bitcodes, with duplicate bitcodes possible.
*/
Bitcodes
collectBitcodes(
    tooling::ToolExecutor& ex);

} // mrdox
} // clang

#endif
