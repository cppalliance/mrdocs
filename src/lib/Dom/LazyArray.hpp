//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_DOM_LAZY_ARRAY_HPP
#define MRDOCS_LIB_DOM_LAZY_ARRAY_HPP

#include "mrdocs/Dom.hpp"
#include "mrdocs/Platform.hpp"
#include "mrdocs/Support/Error.hpp"
#include <string_view>
#include <ranges>

namespace clang {
namespace mrdocs {
namespace dom {

namespace detail {
    struct noop {
        template <class T>
        auto operator()(T&& t) const
        {
            return t;
        }
    };

    struct no_size_tag {};
}

/** Lazy array implementation

    This array type is used to define a dom::Array
    whose members are evaluated on demand
    as they are accessed.

    Each member can goes through a transform
    function before being returned as a Value so
    that all types can be converted to dom::Value.

    The underlying representation of the array is
    a range from where the elements are extracted.
    Elements in this range should be convertible
    to dom::Value.

    This class is typically useful for
    implementing arrays that are expensive
    and have recursive dependencies, as these
    recursive dependencies can also be deferred.

    Unlike a LazyObjectImpl, which contains an
    overlay object, this implementation is
    read-only. The `set` and `emplace_back`
    methods are not implemented.

*/
template <std::ranges::random_access_range R, class F = detail::noop>
requires
    std::invocable<F, std::ranges::range_value_t<R>> &&
    std::constructible_from<Value, std::invoke_result_t<F, std::ranges::range_value_t<R>>>
class LazyArrayImpl : public ArrayImpl
{
    using const_iterator_t = decltype(std::ranges::cbegin(std::declval<R&>()));
    using const_sentinel_t = decltype(std::ranges::cend(std::declval<R&>()));
    using size_type = std::conditional_t<
        std::ranges::sized_range<R>,
        std::ranges::range_size_t<R>,
        detail::no_size_tag>;

    const_iterator_t begin_;
    const_sentinel_t end_;
    [[no_unique_address]] size_type size_;
    [[no_unique_address]] F transform_;

public:
    explicit
    LazyArrayImpl(R const& arr)
        : begin_(std::ranges::begin(arr))
        , end_(std::ranges::end(arr))
    {
        if constexpr (std::ranges::sized_range<R>)
        {
            size_ = std::ranges::size(arr);
        }
    }

    explicit
    LazyArrayImpl(R const& arr, F transform)
        : begin_(std::ranges::begin(arr))
        , end_(std::ranges::end(arr))
        , transform_(std::move(transform))
    {
        if constexpr (std::ranges::sized_range<R>)
        {
            size_ = std::ranges::size(arr);
        }
    }

    ~LazyArrayImpl() override = default;

    /// @copydoc ObjectImpl::type_key
    char const*
    type_key() const noexcept override
    {
        return "LazyArray";
    }

    std::size_t size() const noexcept override
    {
        if constexpr (std::ranges::sized_range<R>)
        {
            return size_;
        }
        else
        {
            return std::ranges::distance(begin_, end_);
        }
    }

    dom::Value get(std::size_t i) const override
    {
        if (i >= size())
        {
            return {};
        }
        auto it = begin_;
        std::ranges::advance(it, i);
        if constexpr (std::is_same_v<F, detail::noop>)
        {
            return Value(*it);
        }
        else
        {
            return Value(transform_(*it));
        }
    }
};

/** Return a new dom::Array based on a lazy array implementation.
*/
template <std::ranges::random_access_range T>
requires std::constructible_from<Value, std::ranges::range_value_t<T>>
Array
LazyArray(T const& arr)
{
    return newArray<LazyArrayImpl<T>>(arr);
}

/** Return a new dom::Array based on a transformed lazy array implementation.
*/
template <std::ranges::random_access_range T, class F>
requires
    std::invocable<F, std::ranges::range_value_t<T>> &&
    std::constructible_from<Value, std::invoke_result_t<F, std::ranges::range_value_t<T>>>
Array
LazyArray(T const& arr, F transform)
{
    return newArray<LazyArrayImpl<T, F>>(arr, std::move(transform));
}

} // dom
} // mrdocs
} // clang

#endif
