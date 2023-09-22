//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_OBJECT_IPP
#define MRDOX_API_DOM_OBJECT_IPP

namespace clang {
namespace mrdox {
namespace dom {

//------------------------------------------------
//
// Object::value_type
//
//------------------------------------------------

struct Object::value_type
{
    String key;
    Value value;

    value_type() = default;

    value_type(
        String key_,
        Value value_) noexcept
        : key(std::move(key_))
        , value(std::move(value_))
    {
    }

    template<class U>
    requires std::constructible_from<
        std::pair<String, Value>, U>
    value_type(
        U&& u)
        : value_type(
            [&u]
            {
                std::pair<String, Value> p(
                    std::forward<U>(u));
                return value_type(
                    std::move(p).first,
                    std::move(p).second);
            })
    {
    }

    value_type* operator->() noexcept
    {
        return this;
    }

    value_type const* operator->() const noexcept
    {
        return this;
    }
};

//------------------------------------------------
//
// Object::reference
//
//------------------------------------------------

struct MRDOX_DECL
    Object::reference
{
    String const& key;
    Value const& value;

    reference(reference const&) = default;

    reference(
        String const& key_,
        Value const& value_) noexcept
        : key(key_)
        , value(value_)
    {
    }

    reference(
        value_type const& kv) noexcept
        : key(kv.key)
        , value(kv.value)
    {
    }

    operator value_type() const
    {
        return value_type(key, value);
    }

    reference const* operator->() noexcept
    {
        return this;
    }

    reference const* operator->() const noexcept
    {
        return this;
    }
};

//------------------------------------------------
//
// Object::iterator
//
//------------------------------------------------

class MRDOX_DECL
    Object::iterator
{
    ObjectImpl const* obj_ = nullptr;
    std::size_t i_ = 0;

    friend class Object;

    iterator(
        ObjectImpl const& obj,
        std::size_t i) noexcept
        : obj_(&obj)
        , i_(i)
    {
    }

public:
    using value_type = Object::value_type;
    using reference = Object::reference;
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
        return iterator(*obj_, i_ - n);
    }

    iterator operator+(difference_type n) const noexcept
    {
        return iterator(*obj_, i_ + n);
    }

    difference_type operator-(iterator other) const noexcept
    {
        MRDOX_ASSERT(obj_ == other.obj_);
        return static_cast<difference_type>(i_) - other.i_;
    }

    friend iterator operator+(difference_type n, iterator it) noexcept
    {
        return it + n;
    }
};

//------------------------------------------------
//
// Object
//
//------------------------------------------------

inline char const* Object::type_key() const noexcept
{
    return impl_->type_key();
}

inline bool Object::empty() const
{
    return size() == 0;
}

inline std::size_t Object::size() const
{
    return impl_->size();
}

inline auto Object::get(std::size_t i) const -> reference
{
    return impl_->get(i);
}

inline auto Object::operator[](size_type i) const -> reference
{
    return get(i);
}

inline auto Object::operator[](std::string_view key) const -> dom::Value
{
    return this->find(key);
}

inline auto Object::at(size_type i) const -> reference
{
    if(i < size())
        return get(i);
    Error("out of range").Throw();
}

inline Value Object::find(std::string_view key) const
{
    return impl_->find(key);
}

inline void Object::set(String key, Value value) const
{
    impl_->set(std::move(key), std::move(value));
}

inline auto Object::begin() const -> iterator
{
    return iterator(*impl_, 0);
}

inline auto Object::end() const -> iterator
{
    return iterator(*impl_, impl_->size());
}

} // dom
} // mrdox
} // clang

//------------------------------------------------

template<
    template <class> class TQual, 
    template <class> class UQual>
struct std::basic_common_reference<
    ::clang::mrdox::dom::Object::value_type,
    ::clang::mrdox::dom::Object::reference,
    TQual, UQual>
{
    using type = ::clang::mrdox::dom::Object::reference;
};

template<
    template <class> class TQual, 
    template <class> class UQual>
struct std::basic_common_reference<
    ::clang::mrdox::dom::Object::reference,
    ::clang::mrdox::dom::Object::value_type,
    TQual, UQual>
{
    using type = ::clang::mrdox::dom::Object::reference;
};

template<>
struct fmt::formatter<clang::mrdox::dom::Object>
    : fmt::formatter<std::string>
{
    auto format(
        clang::mrdox::dom::Object const& value,
        fmt::format_context& ctx) const
    {
        return fmt::formatter<std::string>::format(
            toString(value), ctx);
    }
};

#endif
