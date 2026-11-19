//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_TAGFILEREADER_HPP
#define MRDOCS_LIB_SUPPORT_TAGFILEREADER_HPP

#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <mrdocs/detail/TagfileIndex.hpp>
#include <string>
#include <string_view>

namespace mrdocs {

struct ParsedRef;

/** Return the name a tagfile entry for a referenced symbol is keyed by.

    It is the components of the reference, without template arguments
    or a parameter list. The reader keys each entry it records by
    parsing its name the same way, so a reference and an entry meet
    however each of them writes the name: `operator[](size_type)` and
    `operator[]` are one key.

    @param ref The parsed reference.
*/
std::string
tagfileKey(ParsedRef const& ref);

/** Parse a tagfile's contents into an index.

    A tagfile is written in a small, machine-generated subset of XML,
    and this reads that subset.

    @param index The index to record the symbols in.
    @param contents The whole content of the tagfile.
    @param baseUrl The location the documentation set is published at,
        a URL or a path, which every target from this tagfile is joined
        to.
    @return Nothing, or an error naming the line that stopped it.
*/
Expected<void>
readTagfile(
    detail::TagfileIndex& index,
    std::string_view contents,
    std::string_view baseUrl);

/** Read a tagfile from a file into an index.

    @param index The index to record the symbols in.
    @param path The file system path of the tagfile to read.
    @param baseUrl The location the documentation set is published at.
    @return Nothing, or an error naming the file that stopped it.
*/
Expected<void>
loadTagfile(
    detail::TagfileIndex& index,
    std::string_view path,
    std::string_view baseUrl);

} // mrdocs

#endif // MRDOCS_LIB_SUPPORT_TAGFILEREADER_HPP
