//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_ALGORITHM_HPP
#define MRDOCS_API_SUPPORT_ALGORITHM_HPP

#include <cstdint>
#include <mrdocs/Support/Concepts.hpp>

namespace clang::mrdocs {

/** Determine if a range contains a specific element.
    @param range The range to search.
    @param el The element to search for.
    @return True if the element is found, false otherwise.
 */
template <std::ranges::range Range, class El>
requires std::equality_comparable_with<El, std::ranges::range_value_t<Range>>
bool
contains(Range const& range, El const& el)
{
    return std::find(range.begin(), range.end(), el) != range.end();
}

// A second overload where the range is an initializer list

/** Determine if a range contains a specific element.
    @param range The range to search.
    @param el The element to search for.
    @return True if the element is found, false otherwise.
 */
template <class T, class U>
requires std::equality_comparable_with<T, U>
bool
contains(std::initializer_list<T> const& range, U const& el)
{
    return std::find(range.begin(), range.end(), el) != range.end();
}

/** Determine if a range contains any of the specified elements.
    @param range The range to search.
    @param els The elements to search for.
    @return True if any of the elements are found, false otherwise.
 */
template <std::ranges::range Range, std::ranges::range Els>
requires std::equality_comparable_with<std::ranges::range_value_t<Els>, std::ranges::range_value_t<Range>>
bool
contains_any(Range const& range, Els const& els)
{
    return std::ranges::find_first_of(range, els) != range.end();
}

template <std::ranges::range Range, class El>
requires std::equality_comparable_with<El, std::ranges::range_value_t<Range>>
bool
contains_any(Range const& range, std::initializer_list<El> const& els)
{
    return std::ranges::find_first_of(range, els) != range.end();
}

/** Determine if a range contains at least N instances of the specified element.
    @param range The range to search.
    @param el The element to search for.
    @param n The number of instances to search for.
    @return True if the element is found, false otherwise.
 */
template <std::ranges::range Range, class El>
requires std::equality_comparable_with<El, std::ranges::range_value_t<Range>>
bool
contains_n(Range const& range, El const& el, std::size_t n)
{
    for (auto const& item : range)
    {
        if (item == el)
        {
            --n;
            if (n == 0)
            {
                return true;
            }
        }
    }
    return false;
}

} // clang::mrdocs

#endif