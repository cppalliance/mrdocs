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

#ifndef MRDOCS_API_ADT_ARRAYVIEW_HPP
#define MRDOCS_API_ADT_ARRAYVIEW_HPP

#include <mrdocs/Platform.hpp>
#include <algorithm>
#include <cassert>
#include <compare>
#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>

namespace mrdocs {

/** A non-owning, read-only view over a contiguous array of T.

    Similar to std::string_view but for arbitrary element type T.
*/
template <class T>
class ArrayView {
    static_assert(!std::is_void_v<T>, "ArrayView<void> is ill-formed");

public:
    // types
    using value_type             = T;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using pointer                = const T*;
    using const_pointer          = const T*;
    using reference              = const T&;
    using const_reference        = const T&;
    using iterator               = const T*;
    using const_iterator         = const T*;
    using reverse_iterator       = std::reverse_iterator<const_iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    static constexpr size_type npos = static_cast<size_type>(-1);

    // ctors
    constexpr ArrayView() noexcept = default;

    constexpr ArrayView(const T* data, size_type count) noexcept
        : data_(data), size_(count) {}

    template <size_type N>
    constexpr ArrayView(const T (&arr)[N]) noexcept
        : data_(arr), size_(N) {}

    template <class It>
    requires (std::contiguous_iterator<It> &&
             std::same_as<std::remove_cv_t<std::remove_reference_t<std::iter_value_t<It>>>, T>)
    constexpr ArrayView(It first, size_type count) noexcept
        : data_(std::to_address(first)), size_(count) {}

    // iterators
    constexpr const_iterator begin()  const noexcept { return data_; }
    constexpr const_iterator end()    const noexcept { return data_ + size_; }
    constexpr const_iterator cbegin() const noexcept { return begin(); }
    constexpr const_iterator cend()   const noexcept { return end(); }
    constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    constexpr const_reverse_iterator rend()   const noexcept { return const_reverse_iterator(begin()); }

    // capacity
    constexpr size_type size()   const noexcept { return size_; }
    constexpr size_type length() const noexcept { return size_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

    // element access
    constexpr const_reference operator[](size_type i) const noexcept {
        return data_[i];
    }
    constexpr const_reference at(size_type i) const {
        assert(i < size_);
        return data_[i];
    }
    constexpr const_reference front() const {
        assert(!empty());
        return data_[0];
    }
    constexpr const_reference back() const {
        assert(!empty());
        return data_[size_ - 1];
    }
    constexpr const_pointer data() const noexcept { return data_; }

    // modifiers (adjust the view; do not touch underlying data)
    constexpr void remove_prefix(size_type n) noexcept {
        assert(n <= size_);
        data_ += n;
        size_ -= n;
    }
    constexpr void remove_suffix(size_type n) noexcept {
        assert(n <= size_);
        size_ -= n;
    }

    // slicing
    constexpr ArrayView slice(size_type pos, size_type count = npos) const noexcept {
        assert(pos <= size_);
        const size_type rcount = (count == npos || pos + count > size_) ? (size_ - pos) : count;
        return ArrayView(data_ + pos, rcount);
    }
    constexpr ArrayView take_front(size_type n) const noexcept {
        return slice(0, n <= size_ ? n : size_);
    }
    constexpr ArrayView take_back(size_type n) const noexcept {
        n = (n <= size_) ? n : size_;
        return slice(size_ - n, n);
    }
    constexpr ArrayView drop_front(size_type n) const noexcept {
        return (n >= size_) ? ArrayView() : ArrayView(data_ + n, size_ - n);
    }
    constexpr ArrayView drop_back(size_type n) const noexcept {
        return (n >= size_) ? ArrayView() : ArrayView(data_, size_ - n);
    }

    // comparisons
    friend constexpr bool operator==(ArrayView a, ArrayView b) noexcept
    requires requires (const T& x, const T& y) { { x == y } -> std::convertible_to<bool>; }
    {
        return a.size_ == b.size_ && std::equal(a.begin(), a.end(), b.begin());
    }

    friend constexpr auto operator<=>(ArrayView a, ArrayView b) noexcept
    requires requires (const T& x, const T& y) { x <=> y; }
    {
        return std::lexicographical_compare_three_way(
            a.begin(), a.end(), b.begin(), b.end(), std::compare_three_way{});
    }

private:
    const T* data_ = nullptr;
    size_type size_ = 0;
};

// deduction guides
template <class T>
ArrayView(const T*, std::size_t) -> ArrayView<T>;

template <class T, std::size_t N>
ArrayView(const T (&)[N]) -> ArrayView<T>;

// helpers
template <class T>
constexpr ArrayView<T> make_array_view(const T* data, std::size_t count) noexcept {
    return ArrayView<T>(data, count);
}

template <class T, std::size_t N>
constexpr ArrayView<T> make_array_view(const T (&arr)[N]) noexcept {
    return ArrayView<T>(arr);
}

} // mrdocs

#endif // MRDOCS_API_ADT_ARRAYVIEW_HPP
