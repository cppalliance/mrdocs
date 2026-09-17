//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_TAGFILEINDEX_HPP
#define MRDOCS_API_SUPPORT_TAGFILEINDEX_HPP

// The reading half of tagfile support.

#include <mrdocs/Platform.hpp>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace mrdocs {

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
    /** Where one symbol is documented.

        The parts are what a tagfile offers, joined into a URL by
        @ref find.
    */
    struct Target
    {
        /** URL the documentation set is published under.
        */
        std::string baseUrl;
        /** Name of the page within that documentation set.
        */
        std::string page;
        /** Anchor on that page; empty for a whole-page entry.
        */
        std::string anchor;
    };

    /** Record where a symbol is documented.

        The first target recorded for a name is the one kept, so reading
        a name a second time leaves the index as it was.

        @return `true` if the target was recorded, `false` if the name
        was already known or either the name or the page is empty.

        @param qualifiedName The fully qualified name of the symbol.
        @param target Where the symbol is documented.
    */
    bool
    insert(std::string_view qualifiedName, Target target);

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

        Reported per tagfile as it is read, since a tagfile that
        contributes nothing is the first thing to suspect when a
        reference to it stays unresolved.
    */
    std::size_t
    size() const noexcept;

private:
    std::map<std::string, Target, std::less<>> targets_;
};

} // mrdocs

#endif // MRDOCS_API_SUPPORT_TAGFILEINDEX_HPP
