//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_LEGIBLENAMES_HPP
#define MRDOCS_LIB_SUPPORT_LEGIBLENAMES_HPP

#include <mrdocs/MetadataFwd.hpp>
#include <mrdocs/Metadata/Symbols.hpp>
#include <memory>
#include <string>

namespace clang::mrdocs {

/** A table mapping Info objects to legible names.

    A legible name for a symbol is:

    @li case-insensitive
    @li unique
    @li only characters valid for both filenames and URL paths.

    For filenames this includes only the subset of
    characters valid for Windows, OSX, and Linux
    type filesystems.
*/
class LegibleNames
{
    class Impl;
    std::unique_ptr<Impl> impl_;

public:
    /** Constructor.

        Upon construction, the entire table of
        legible names is built from the corpus.
    */
    LegibleNames(
        Corpus const& corpus,
        bool enabled);

    /** Destructor.
     */
    ~LegibleNames() noexcept;

    /** Return the legible name for a symbol name.
     */
    std::string
    getUnqualified(SymbolID const& id) const;

    std::string
    getQualified(
        SymbolID const& id,
        char delim = '-') const;
};

} // clang::mrdocs

#endif
