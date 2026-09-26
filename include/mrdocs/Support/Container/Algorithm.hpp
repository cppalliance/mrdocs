//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_CONTAINER_ALGORITHM_HPP
#define MRDOCS_API_SUPPORT_CONTAINER_ALGORITHM_HPP

#include <mrdocs/Support/TypeTraits/Concepts.hpp>
#include <algorithm>
#include <cstdint>

namespace mrdocs {

/** Determine if a range contains a specific element.
    @param range The range to search.
    @param el The element to search for.
    @return True if the element is found, false otherwise.
*/
template <std::ranges::range Range, class El>
requires std::equality_comparable_with<El, std::ranges::range_value_t<Range>>
bool
contains(Range&& range, El const& el)
{
    return std::find(
        std::ranges::begin(range),
        std::ranges::end(range), el) != std::ranges::end(range);
}

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
    return std::find(
        std::ranges::begin(range),
        std::ranges::end(range), el) != std::ranges::end(range);
}

/** Determine if an element is equal to any of the elements in the specified range.

    @param el The element to search for.
    @param range The range to search.
    @return True if the element is found, false otherwise.
*/
template <class El, std::ranges::range Range>
requires std::equality_comparable_with<std::ranges::range_value_t<Range>, El>
bool
is_one_of(El const& el, Range const& range)
{
    return contains(range, el);
}

/// @copydoc is_one_of(El const&, Range const&)
template <class U, class T>
requires std::equality_comparable_with<U, T>
bool
is_one_of(U const& el, std::initializer_list<T> const& range)
{
    return contains(range, el);
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
    return std::ranges::find_first_of(range, els) != std::ranges::end(range);
}

/// @copydoc contains_any(Range const&, Els const&)
template <std::ranges::range Range, class El>
requires std::equality_comparable_with<El, std::ranges::range_value_t<Range>>
bool
contains_any(Range const& range, std::initializer_list<El> const& els)
{
    return std::ranges::find_first_of(range, els) != std::ranges::end(range);
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

/** Determine if a range contains at least N instances of any of the specified elements.
    @param range The range to search.
    @param els The elements to search for.
    @param n The number of instances to search for.
    @return True if the element is found, false otherwise.
*/
template <std::ranges::range Range, std::ranges::range Els>
requires std::equality_comparable_with<std::ranges::range_value_t<Els>, std::ranges::range_value_t<Range>>
bool
contains_n_any(Range const& range, Els const& els, std::size_t n)
{
    for (auto const& item : range)
    {
        if (contains(els, item))
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

/// @copydoc contains_n_any(Range const&, Els const&, std::size_t)
template <std::ranges::range Range, class El>
requires std::equality_comparable_with<El, std::ranges::range_value_t<Range>>
bool
contains_n_any(Range const& range, std::initializer_list<El> const& els, std::size_t n)
{
    for (auto const& item : range)
    {
        if (contains(els, item))
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

/** Find the last element in a range that matches an element in the specified range.

    @param range The range to search.
    @param els The elements to search for.
    @return An iterator to the last element found, or std::ranges::end(range) if not found.
*/
template <std::ranges::range Range, std::ranges::range Els>
requires std::equality_comparable_with<std::ranges::range_value_t<Els>, std::ranges::range_value_t<Range>>
auto
find_last_of(Range&& range, Els&& els)
{
    if (std::ranges::empty(range))
    {
        return std::ranges::end(range);
    }
    auto it = std::ranges::end(range);
    do {
        --it;
        if (contains(els, *it))
        {
            return it;
        }
    } while (it != std::ranges::begin(range));
    return std::ranges::end(range);
}

/** Insert a value into a range sorted by a comparator, searching only
    a subrange of it.

    @param container The container to insert into.
    @param first The beginning of the sorted subrange to search.
    @param last The end of the sorted subrange to search.
    @param value The value to insert.
    @param comp The comparator the subrange is sorted by.
    @return An iterator to the inserted element.
*/
template <class Container, std::forward_iterator It, class T, class Comp>
auto
insert_sorted(Container& container, It first, It last, T&& value, Comp comp)
{
    auto const it = std::ranges::lower_bound(first, last, value, comp);
    return container.insert(it, std::forward<T>(value));
}

/** Insert a value into a range sorted by a comparator.

    @param container The sorted container to insert into.
    @param value The value to insert.
    @param comp The comparator the container is sorted by.
    @return An iterator to the inserted element.
*/
template <std::ranges::range Container, class T, class Comp>
auto
insert_sorted(Container& container, T&& value, Comp comp)
{
    return insert_sorted(
        container,
        std::ranges::begin(container),
        std::ranges::end(container),
        std::forward<T>(value),
        comp);
}

} // mrdocs

#endif