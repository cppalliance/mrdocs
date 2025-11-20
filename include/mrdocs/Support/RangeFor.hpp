//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_RANGEFOR_HPP
#define MRDOCS_API_SUPPORT_RANGEFOR_HPP

#include <mrdocs/Platform.hpp>

namespace mrdocs {

/** Range adapter to expose first/last flags inside range-for loops.
*/
template<class Container>
class RangeFor
{
    Container const& C_;

public:
    /** Proxy describing an element plus first/last flags.
    */
    struct value_type;
    /** Unsigned size type.
    */
    using size_type = std::size_t;
    /** Signed distance type.
    */
    using difference_type = std::ptrdiff_t;
    /** Pointer to proxy.
    */
    using pointer = value_type*;
    /** Reference to proxy.
    */
    using reference = value_type&;
    /** Pointer to const proxy.
    */
    using const_pointer = value_type const*;
    /** Reference to const proxy.
    */
    using const_reference = value_type const&;

    class iterator;

    /** Construct a range wrapper over a container.
        @param C Container to iterate.
    */
    explicit
    RangeFor(Container const& C) noexcept
        : C_(C)
    {
    }

    /** Iterator to first element.
    */
    iterator begin() const noexcept;
    /** Iterator past the last element.
    */
    iterator end() const noexcept;
};

//------------------------------------------------

/** Reference to current element.
*/
template<class Container>
struct RangeFor<Container>::value_type
{
    /// The contained value.
    typename Container::value_type const& value;

    /** True if this element is the first in the range.
    */
    bool const first;

    /** True if this element is the last in the range.
    */
    bool const last;

    /** Access members through pointer syntax.
        @return Pointer to this proxy.
    */
    value_type const* operator->() const noexcept
    {
        return this;
    }
};

//------------------------------------------------

/** Iterator yielding RangeFor::value_type proxies with first/last flags.
*/
template<class Container>
class RangeFor<Container>::iterator
{
    using const_iterator =
        typename Container::const_iterator;

    const_iterator it_{};
    const_iterator begin_;
    const_iterator last_;
    const_iterator end_;

    friend class RangeFor;
    friend struct value_type;

    iterator(
        Container const& C,
        const_iterator it) noexcept
        : it_(it)
        , begin_(C.begin())
        , last_(C.begin() != C.end() ?
            std::prev(C.end()) : C.end())
        , end_(C.end())
    {
    }

public:
    /** Proxy value type.
    */
    using value_type = typename RangeFor<Container>::value_type;
    /** Pointer type (unused).
    */
    using pointer    = void;
    /** Reference type.
    */
    using reference  = value_type;
    /** Size type alias.
    */
    using size_type  = std::size_t;
    /** Iterator category forwarded from container.
    */
    using iterator_category = typename
        std::iterator_traits<typename Container::iterator>::iterator_category;

    /** Default constructor.
    */
    iterator() = default;
    /** Copy constructor.
    */
    iterator(iterator const&) = default;
    /** Copy assignment.
    */
    iterator& operator=(
        iterator const&) = default;

    /** Pre-increment.
        @return *this advanced to next element.
    */
    iterator& operator++() noexcept
    {
        ++it_;
        return *this;
    }

    /** Post-increment.
        @param unused Dummy parameter for postfix form.
        @return Iterator prior to increment.
    */
    iterator operator++(int unused) noexcept
    {
        (void)unused;
        auto temp = *this;
        ++temp;
        return temp;
    }

    /** Return proxy for current element.
    */
    reference operator->() const noexcept
    {
        return value_type{
            *it_,
            it_ == begin_,
            it_ == last_ };
    }

    /** Dereference to proxy value.
    */
    reference operator*() const noexcept
    {
        return value_type{
            *it_,
            it_ == begin_,
            it_ == last_ };
    }

    /** Equality comparison.
    */
    bool operator==(iterator const& it) const noexcept
    {
        return it_ == it.it_;
    }

    /** Inequality comparison.
    */
    bool operator!=(iterator const& it) const noexcept
    {
        return it_ != it.it_;
    }
};

//------------------------------------------------

/** Deduction guide for RangeFor.
*/
/** Deduction guide for RangeFor.
*/
template<class Container>
RangeFor(Container const&) -> RangeFor<Container>;

template<class Container>
auto RangeFor<Container>::begin() const noexcept ->
    iterator
{
    return iterator(C_, C_.begin());
}

template<class Container>
auto RangeFor<Container>::end() const noexcept ->
    iterator
{
    return iterator(C_, C_.end());
}

} // mrdocs


#endif
