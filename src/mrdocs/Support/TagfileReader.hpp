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
#include <mrdocs/Support/TagfileIndex.hpp>
#include <string_view>

namespace mrdocs {

/** Parses a tagfile's contents into an index.

    A tagfile is written in a small, machine-generated subset of XML,
    and this reads that subset.

    @return Nothing, or an error naming the line that stopped it.

    @param index The index to record the symbols in.
    @param contents The whole content of the tagfile.
    @param baseUrl The URL the documentation set is published under,
        which every target from this tagfile is resolved against.
*/
Expected<void>
readTagfile(
    TagfileIndex& index,
    std::string_view contents,
    std::string_view baseUrl);

/** Read a tagfile from a file into an index.

    Reads a tagfile from disk, naming the file in whatever error occurs,
    since a run may read several.

    @return Nothing, or an error naming the file that stopped it.

    @param index The index to record the symbols in.
    @param path The path of the tagfile to read.
    @param baseUrl The URL the documentation set is published under.
*/
Expected<void>
loadTagfile(
    TagfileIndex& index,
    std::string_view path,
    std::string_view baseUrl);

} // mrdocs

#endif // MRDOCS_LIB_SUPPORT_TAGFILEREADER_HPP
