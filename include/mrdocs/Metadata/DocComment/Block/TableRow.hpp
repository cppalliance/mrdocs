//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TABLEROW_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TABLEROW_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/DocComment/Block/TableCell.hpp>
#include <string>

namespace mrdocs::doc {

/** An item in a list
*/
struct TableRow final
{
    bool is_header = false;
    std::vector<TableCell> Cells;

    auto operator<=>(TableRow const&) const = default;
    bool
    operator==(TableRow const&) const noexcept = default;
};

/** Map the @ref TableRow to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    TableRow const& I,
    DomCorpus const* domCorpus)
{
    io.map("is_header", I.is_header);
    io.defer("cells", [&] {
        return dom::LazyArray(I.Cells, domCorpus);
    });
}

/** Return the @ref TableRow as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TableRow const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_TABLEROW_HPP
