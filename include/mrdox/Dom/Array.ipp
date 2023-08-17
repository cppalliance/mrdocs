//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_ARRAY_IPP
#define MRDOX_API_DOM_ARRAY_IPP

namespace clang {
namespace mrdox {
namespace dom {

//------------------------------------------------
//
// Array::iterator
//
//------------------------------------------------

class MRDOX_DECL
    Array::iterator
{
    ArrayImpl const* obj_ = nullptr;
    std::size_t i_ = 0;

    friend class Array;

    iterator(
        ArrayImpl const& obj,
        std::size_t i) noexcept
        : obj_(&obj)
        , i_(i)
    {
    }

public:
    using value_type = Array::value_type;
    using reference = Array::value_type;
    using difference_type = std::ptrdiff_t;
    using size_type  = std::size_t;
    using pointer = reference;
    using iterator_category =
        std::random_access_iterator_tag;

    iterator() = default;

    reference operator*() const noexcept
    {
        return obj_->get(i_);
    }

    pointer operator->() const noexcept
    {
        return obj_->get(i_);
    }

    reference operator[](difference_type n) const noexcept
    {
        return obj_->get(i_ + n);
    }

    iterator& operator--() noexcept
    {
        --i_;
        return *this;
    }

    iterator operator--(int) noexcept
    {
        auto temp = *this;
        --*this;
        return temp;
    }

    iterator& operator++() noexcept
    {
        ++i_;
        return *this;
    }

    iterator operator++(int) noexcept
    {
        auto temp = *this;
        ++*this;
        return temp;
    }

    auto operator<=>(iterator const& other) const noexcept
    {
        MRDOX_ASSERT(obj_ == other.obj_);
        return i_ <=> other.i_;
    }

#if 1
    // VFALCO Why does ranges need these? Isn't <=> enough?
    bool operator==(iterator const& other) const noexcept
    {
        MRDOX_ASSERT(obj_ == other.obj_);
        return i_ == other.i_;
    }

    bool operator!=(iterator const& other) const noexcept
    {
        MRDOX_ASSERT(obj_ == other.obj_);
        return i_ != other.i_;
    }
#endif

    iterator& operator-=(difference_type n) noexcept
    {
        i_ -= n;
        return *this;
    }

    iterator& operator+=(difference_type n) noexcept
    {
        i_ += n;
        return *this;
    }

    iterator operator-(difference_type n) const noexcept
    {
        return {*obj_, i_ - n};
    }

    iterator operator+(difference_type n) const noexcept
    {
        return {*obj_, i_ + n};
    }

    difference_type operator-(iterator other) const noexcept
    {
        MRDOX_ASSERT(obj_ == other.obj_);
        return static_cast<difference_type>(i_) - static_cast<difference_type>(other.i_);
    }

    friend iterator operator+(difference_type n, iterator it) noexcept
    {
        return it + n;
    }
};

//------------------------------------------------
//
// Array
//
//------------------------------------------------

inline char const* Array::type_key() const noexcept
{
    return impl_->type_key();
}

inline bool Array::empty() const noexcept
{
    return impl_->size() == 0;
}

inline auto Array::size() const noexcept -> size_type
{
    return impl_->size();
}

inline auto Array::get(std::size_t i) const -> value_type
{
    return impl_->get(i);
}

inline auto Array::operator[](std::size_t i) const -> value_type
{
    return get(i);
}

inline auto Array::at(std::size_t i) const -> value_type
{
    if(i < size())
        return get(i);
    Error("out of range").Throw();
}

inline auto Array::begin() const -> iterator
{
    return {*impl_, 0};
}

inline auto Array::end() const -> iterator
{
    return {*impl_, impl_->size()};
}

inline void Array::emplace_back(value_type value)
{
    impl_->emplace_back(std::move(value));
}

} // dom
} // mrdox
} // clang

#endif
