//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_DETAIL_TAGFILEINDEX_HPP
#define MRDOCS_API_DETAIL_TAGFILEINDEX_HPP

// The reading half of tagfile support.

#include <mrdocs/Platform.hpp>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace mrdocs::detail {

/** The symbols documented outside this corpus, and where to find them.

    A reference in a doc comment can name a symbol MrDocs never
    extracted. A tagfile says which symbols another documentation set
    covers and which page each one is on, so a reference to one of them
    can still become a link instead of plain text.
*/
class MRDOCS_DECL
    TagfileIndex
{
public:
    /** Record the URL documenting a symbol.

        The first URL recorded for a name is the one kept, so recording
        a name a second time leaves the index as it was.

        @param qualifiedName The fully qualified name of the symbol.
        @param url The URL documenting the symbol.
        @return `true` if the URL was recorded, `false` if the name was
            already known or either the name or the URL is empty.
    */
    bool
    insert(std::string_view qualifiedName, std::string url);

    /** Return whether the index holds a symbol.

        @param qualifiedName The fully qualified name to look for.
    */
    bool
    contains(std::string_view qualifiedName) const;

    /** Return the URL documenting a symbol, or nothing if it has none.

        @param qualifiedName The fully qualified name to look for.
    */
    std::optional<std::string>
    find(std::string_view qualifiedName) const;

    /** Return whether the index holds nothing.
    */
    bool
    empty() const noexcept;

    /** Return how many symbols the index holds.
    */
    std::size_t
    size() const noexcept;

private:
    std::map<std::string, std::string, std::less<>> urls_;
};

} // mrdocs::detail

#endif // MRDOCS_API_DETAIL_TAGFILEINDEX_HPP
