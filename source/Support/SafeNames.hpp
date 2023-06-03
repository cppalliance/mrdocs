//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_SUPPORT_SAFENAMES_HPP
#define MRDOX_LIB_SUPPORT_SAFENAMES_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/Support/raw_ostream.h>
#include <string>

namespace clang {
namespace mrdox {

/** A table mapping symbolIDs to safe names.

    A safe name for a symbol is case-insensitive
    unique and only contains characters which are
    valid for both filenames and URL paths. For
    filenames this includes only the subset of
    characters valid for Windows, OSX, and Linux
    type filesystems.
*/
class SafeNames
{
    Corpus const& corpus_;
    llvm::StringMap<std::string> map_;

    SafeNames(
        llvm::raw_ostream& os,
        Corpus const&);

public:
    /** Constructor.

        Upon construction, the entire table of
        safe names is built from the corpus.
    */
    explicit
    SafeNames(
        Corpus const& corpus);

    llvm::StringRef
    get(SymbolID const& id) const noexcept;

    std::vector<llvm::StringRef>&
    getPath(
        std::vector<llvm::StringRef>& dest,
        SymbolID id) const;

    std::vector<llvm::StringRef>
    getPath(SymbolID const& id) const
    {
        std::vector<llvm::StringRef> v;
        getPath(v, id);
        return v;
    }
};

} // mrdox
} // clang

#endif
