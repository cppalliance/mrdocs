//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_STRINGLIST_HPP
#define MRDOCS_API_SUPPORT_STRINGLIST_HPP

#include <mrdocs/Platform.hpp>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace mrdocs {

/** A list of strings that accepts several input forms.

    A configuration option of this type can be written as a single
    scalar (`generator: xml`), a YAML sequence (`generator: [xml, adoc]`),
    or a comma-separated scalar (`generator: "xml,adoc"`). All three forms
    normalize to the same list of strings.

    The type models a range of `std::string`, so generic configuration
    code (normalization, DOM exposure) treats it like any other list of
    strings.
*/
struct StringList
{
    /// The individual strings, in order.
    std::vector<std::string> values;

    /// Construct an empty list.
    StringList() = default;

    /** Construct from a braced list of strings.

        @param il The strings to store.
    */
    StringList(std::initializer_list<std::string> il)
        : values(il)
    {
    }

    /** Construct from a vector of strings.

        @param v The strings to store.
    */
    StringList(std::vector<std::string> v)
        : values(std::move(v))
    {
    }

    /// Return an iterator to the first string.
    auto begin() { return values.begin(); }

    /// Return an iterator past the last string.
    auto end() { return values.end(); }

    /// Return a const iterator to the first string.
    auto begin() const { return values.begin(); }

    /// Return a const iterator past the last string.
    auto end() const { return values.end(); }

    /// Return `true` if the list has no strings.
    bool empty() const noexcept { return values.empty(); }

    /// Return the number of strings.
    std::size_t size() const noexcept { return values.size(); }

    /** Expand comma-separated entries in place.

        Each element is split on commas, surrounding whitespace is
        trimmed, and empty tokens are dropped. This lets a single
        element such as `"xml,adoc"` become two entries.
    */
    void
    splitCommaSeparated()
    {
        std::vector<std::string> out;
        for (auto const& v : values)
        {
            std::size_t start = 0;
            while (start <= v.size())
            {
                std::size_t const pos = v.find(',', start);
                std::size_t const len =
                    pos == std::string::npos ? std::string::npos : pos - start;
                std::string token = v.substr(start, len);
                std::size_t const b = token.find_first_not_of(" \t");
                if (b != std::string::npos)
                {
                    std::size_t const e = token.find_last_not_of(" \t");
                    out.push_back(token.substr(b, e - b + 1));
                }
                if (pos == std::string::npos)
                {
                    break;
                }
                start = pos + 1;
            }
        }
        values = std::move(out);
    }
};

} // mrdocs

#endif // MRDOCS_API_SUPPORT_STRINGLIST_HPP
