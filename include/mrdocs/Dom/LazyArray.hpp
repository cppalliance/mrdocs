//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_DOM_LAZYARRAY_HPP
#define MRDOCS_API_DOM_LAZYARRAY_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Error.hpp>
#include <ranges>
#include <string_view>

namespace clang {
namespace mrdocs {
namespace dom {

namespace detail {
    struct no_context_tag {};
    struct no_size_tag {};

    template <class R, class Context>
    concept IsCallableContext =
        std::invocable<Context, std::ranges::range_value_t<R>> &&
        HasStandaloneValueFrom<std::invoke_result_t<Context, std::ranges::range_value_t<R>>>;
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
template <std::ranges::random_access_range R, class Context = detail::no_context_tag>
requires
    HasValueFrom<std::ranges::range_value_t<R>, Context> ||
       (std::invocable<Context, std::ranges::range_value_t<R>> &&
        HasStandaloneValueFrom<std::invoke_result_t<Context, std::ranges::range_value_t<R>>>)
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
    MRDOCS_NO_UNIQUE_ADDRESS size_type size_;
    MRDOCS_NO_UNIQUE_ADDRESS Context context_;

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
    LazyArrayImpl(R const& arr, Context const& ctx)
        : begin_(std::ranges::begin(arr))
        , end_(std::ranges::end(arr))
        , context_(std::move(ctx))
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

    std::size_t
    size() const noexcept override
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

    dom::Value
    get(std::size_t i) const override
    {
        if (i >= size())
        {
            return {};
        }
        auto it = begin_;
        std::ranges::advance(it, i);
        if constexpr (std::is_same_v<Context, detail::no_context_tag>)
        {
            return ValueFrom(*it);
        }
        else if constexpr (detail::IsCallableContext<R, Context>)
        {
            return ValueFrom(context_(*it));
        }
        else
        {
            return ValueFrom(*it, context_);
        }
    }
};

/** Return a new @ref dom::Array based on a lazy array implementation.

    @param arr The underlying range of elements.
    @return A new dom::Array whose elements are the result of converting each element in the underlying range to a dom::Value.
 */
template <std::ranges::random_access_range T>
requires HasStandaloneValueFrom<std::ranges::range_value_t<T>>
Array
LazyArray(T const& arr)
{
    return newArray<LazyArrayImpl<T>>(arr);
}

/** Return a new dom::Array based on a FromValue context

    @param arr The underlying range of elements.
    @param ctx The context used to convert each element to a dom::Value.
    @return A new dom::Array whose elements are the result of converting each element in the underlying range using the specified context.
*/
template <std::ranges::random_access_range T, class Context>
requires HasValueFrom<std::ranges::range_value_t<T>, Context>
Array
LazyArray(T const& arr, Context const& ctx)
{
    return newArray<LazyArrayImpl<T, Context>>(arr, ctx);
}

/** Return a new dom::Array based on a transformed lazy array implementation.

    @param arr The underlying range of elements.
    @param f The transform function to apply to each element before converting it to a dom::Value.
    @return A new dom::Array whose elements are the result of applying the transform function to each element in the underlying range.
*/
template <std::ranges::random_access_range T, class F>
requires
    std::invocable<F, std::ranges::range_value_t<T>> &&
    HasStandaloneValueFrom<std::invoke_result_t<F, std::ranges::range_value_t<T>>>
Array
TransformArray(T const& arr, F const& f)
{
    return newArray<LazyArrayImpl<T, F>>(arr, f);
}

} // dom
} // mrdocs
} // clang

#endif // MRDOCS_API_DOM_LAZYARRAY_HPP
