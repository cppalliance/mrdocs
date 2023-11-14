//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_SAFENAMES_HPP
#define MRDOCS_LIB_SUPPORT_SAFENAMES_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/MetadataFwd.hpp>
#include <memory>
#include <string>

namespace clang {
namespace mrdocs {

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
    class Impl;

    std::unique_ptr<Impl> impl_;

public:
    /** Constructor.

        Upon construction, the entire table of
        safe names is built from the corpus.
    */
    SafeNames(
        Corpus const& corpus,
        bool enabled);

    ~SafeNames() noexcept;

    std::string
    getUnqualified(
        SymbolID const& id) const;

    std::string
    getUnqualified(
        OverloadSet const& os) const;

    std::string
    getQualified(
        SymbolID const& id,
        char delim = '-') const;

    std::string
    getQualified(
        OverloadSet const& os,
        char delim = '-') const;
};

} // mrdocs
} // clang

#endif
