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
    /** Element type referenced by the view.
    */
    using value_type             = T;
    /** Unsigned size type.
    */
    using size_type              = std::size_t;
    /** Signed iterator difference type.
    */
    using difference_type        = std::ptrdiff_t;
    /** Pointer to constant element.
    */
    using pointer                = const T*;
    /** Pointer to constant element.
    */
    using const_pointer          = const T*;
    /** Reference to constant element.
    */
    using reference              = const T&;
    /** Reference to constant element.
    */
    using const_reference        = const T&;
    /** Iterator over elements.
    */
    using iterator               = const T*;
    /** Iterator over elements.
    */
    using const_iterator         = const T*;
    /** Reverse iterator over the view.
    */
    using reverse_iterator       = std::reverse_iterator<const_iterator>;
    /** Const reverse iterator over the view.
    */
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    /** Sentinel used by slicing helpers to indicate “until the end”.
    */
    static constexpr size_type npos = static_cast<size_type>(-1);

    // ctors
    /** Construct an empty view.
    */
    constexpr ArrayView() noexcept = default;

    /** Construct a view from a pointer and element count.
        @param data Pointer to the first element.
        @param count Number of elements in the range.
    */
    constexpr ArrayView(const T* data, size_type count) noexcept
        : data_(data), size_(count) {}

    /** Construct a view from a C-style array.
        @param arr The array to view.
    */
    template <size_type N>
    constexpr ArrayView(const T (&arr)[N]) noexcept
        : data_(arr), size_(N) {}

    /** Construct a view from a contiguous iterator range of known size.
        @param first Iterator to the first element.
        @param count Number of elements starting at `first`.
    */
    template <class It>
    requires (std::contiguous_iterator<It> &&
             std::same_as<std::remove_cv_t<std::remove_reference_t<std::iter_value_t<It>>>, T>)
    constexpr ArrayView(It first, size_type count) noexcept
        : data_(std::to_address(first)), size_(count) {}

    // iterators
    /** Return an iterator to the first element.
        @return Iterator to the start of the view.
    */
    constexpr const_iterator begin()  const noexcept { return data_; }
    /** Return an iterator one past the last element.
        @return Iterator to the end sentinel.
    */
    constexpr const_iterator end()    const noexcept { return data_ + size_; }
    /** Return a const iterator to the first element.
        @return Const iterator to the start of the view.
    */
    constexpr const_iterator cbegin() const noexcept { return begin(); }
    /** Return a const iterator one past the last element.
        @return Const iterator to the end sentinel.
    */
    constexpr const_iterator cend()   const noexcept { return end(); }
    /** Return a reverse iterator to the last element.
        @return Reverse iterator to the last element.
    */
    constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    /** Return a reverse iterator one before the first element.
        @return Reverse iterator denoting the rend sentinel.
    */
    constexpr const_reverse_iterator rend()   const noexcept { return const_reverse_iterator(begin()); }

    // capacity
    /** Return the number of elements in the view.
        @return Element count.
    */
    constexpr size_type size()   const noexcept { return size_; }
    /** Return the number of elements in the view.
        @return Element count.
    */
    constexpr size_type length() const noexcept { return size_; }
    /** Return true if the view contains no elements.
        @return `true` when size is zero, otherwise `false`.
    */
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

    // element access
    /** Access the element at the specified index without bounds checking.
        @param i Zero-based index.
        @return Reference to the element.
    */
    constexpr const_reference operator[](size_type i) const noexcept {
        return data_[i];
    }
    /** Access the element at the specified index with bounds checking.
        @param i Zero-based index.
        @return Reference to the element.
    */
    constexpr const_reference at(size_type i) const {
        assert(i < size_);
        return data_[i];
    }
    /** Return a reference to the first element.
    */
    constexpr const_reference front() const {
        assert(!empty());
        return data_[0];
    }
    /** Return a reference to the last element.
    */
    constexpr const_reference back() const {
        assert(!empty());
        return data_[size_ - 1];
    }
    /** Return a pointer to the underlying data.
        @return Pointer to the first element, or nullptr when empty.
    */
    constexpr const_pointer data() const noexcept { return data_; }

    // modifiers (adjust the view; do not touch underlying data)
    /** Remove `n` elements from the front of the view.
        @param n Number of elements to drop.
    */
    constexpr void remove_prefix(size_type n) noexcept {
        assert(n <= size_);
        data_ += n;
        size_ -= n;
    }
    /** Remove `n` elements from the back of the view.
        @param n Number of elements to drop.
    */
    constexpr void remove_suffix(size_type n) noexcept {
        assert(n <= size_);
        size_ -= n;
    }

    // slicing
    /** Return a subview starting at `pos` with up to `count` elements.
        @param pos Starting index within the current view.
        @param count Maximum number of elements to include; use npos for the remainder.
        @return Subview representing the requested slice.
    */
    constexpr ArrayView slice(size_type pos, size_type count = npos) const noexcept {
        assert(pos <= size_);
        const size_type rcount = (count == npos || pos + count > size_) ? (size_ - pos) : count;
        return ArrayView(data_ + pos, rcount);
    }
    /** Return a view containing the first `n` elements.
        @param n Desired prefix length.
        @return Subview of the first `n` elements (clamped to size).
    */
    constexpr ArrayView take_front(size_type n) const noexcept {
        return slice(0, n <= size_ ? n : size_);
    }
    /** Return a view containing the last `n` elements.
        @param n Desired suffix length.
        @return Subview of the last `n` elements (clamped to size).
    */
    constexpr ArrayView take_back(size_type n) const noexcept {
        n = (n <= size_) ? n : size_;
        return slice(size_ - n, n);
    }
    /** Return a view with the first `n` elements removed.
        @param n Number of elements to drop from the front.
        @return Subview starting after the dropped elements.
    */
    constexpr ArrayView drop_front(size_type n) const noexcept {
        return (n >= size_) ? ArrayView() : ArrayView(data_ + n, size_ - n);
    }
    /** Return a view with the last `n` elements removed.
        @param n Number of elements to drop from the back.
        @return Subview excluding the dropped elements.
    */
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
/** Deduce ArrayView element type from pointer and count.
*/
template <class T>
ArrayView(const T*, std::size_t) -> ArrayView<T>;

/** Deduce ArrayView element type from C-style array.
*/
template <class T, std::size_t N>
ArrayView(const T (&)[N]) -> ArrayView<T>;

// helpers
/** Create an ArrayView from a pointer and count.
    @param data Pointer to the first element.
    @param count Number of elements.
    @return View over the provided range.
*/
template <class T>
constexpr ArrayView<T> make_array_view(const T* data, std::size_t count) noexcept {
    return ArrayView<T>(data, count);
}

/** Create an ArrayView from a C-style array.
    @param arr Array to view.
    @return View over the provided array.
*/
template <class T, std::size_t N>
constexpr ArrayView<T> make_array_view(const T (&arr)[N]) noexcept {
    return ArrayView<T>(arr);
}

} // mrdocs

#endif // MRDOCS_API_ADT_ARRAYVIEW_HPP
