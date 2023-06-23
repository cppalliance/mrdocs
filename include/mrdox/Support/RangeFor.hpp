//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_SUPPORT_RANGEFOR_HPP
#define MRDOX_API_SUPPORT_RANGEFOR_HPP

#include <mrdox/Platform.hpp>

namespace clang {
namespace mrdox {

/** Range to help range-for loops identify first and last.
*/
template<class Container>
class RangeFor
{
    Container const& C_;

public:
    struct value_type;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;
    using const_pointer = value_type const*;
    using const_reference = value_type const&;

    class iterator;

    explicit
    RangeFor(Container const& C) noexcept
        : C_(C)
    {
    }

    iterator begin() const noexcept;
    iterator end() const noexcept;
};

//------------------------------------------------

template<class Container>
struct RangeFor<Container>::value_type
{
    typename Container::value_type const& value;
    bool const first;
    bool const last;

    value_type const* operator->() const noexcept
    {
        return this;
    }
};

//------------------------------------------------

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
    using value_type = typename RangeFor<Container>::value_type;
    using pointer    = void;
    using reference  = value_type;
    using size_type  = std::size_t;
    using iterator_category = typename
        std::iterator_traits<typename Container::iterator>::iterator_category;

    iterator() = default;
    iterator(iterator const&) = default;
    iterator& operator=(
        iterator const&) = default;

    iterator& operator++() noexcept
    {
        ++it_;
        return *this;
    }

    iterator operator++(int) noexcept
    {
        auto temp = *this;
        ++temp;
        return temp;
    }

    reference operator->() const noexcept
    {
        return value_type{
            *it_,
            it_ == begin_,
            it_ == last_ };
    }

    reference operator*() const noexcept
    {
        return value_type{
            *it_,
            it_ == begin_,
            it_ == last_ };
    }

    bool operator==(iterator const& it) const noexcept
    {
        return it_ == it.it_;
    }

    bool operator!=(iterator const& it) const noexcept
    {
        return it_ != it.it_;
    }
};

//------------------------------------------------

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

} // mrdox
} // clang

#endif