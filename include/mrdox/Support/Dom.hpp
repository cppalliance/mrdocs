//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_SUPPORT_DOM_HPP
#define MRDOX_SUPPORT_DOM_HPP

#include <mrdox/Platform.hpp>
#include <cstdint>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

namespace clang {
namespace mrdox {
namespace dom {

class Array;
class Object;
class Value;

/** The type of data in a Value.
*/
enum class Kind
{
    Object,
    Array,
    String,
    Integer,
    Boolean,
    Null
};

//------------------------------------------------

class MRDOX_DECL
    Array
{
public:
    struct Impl
    {
        virtual ~Impl() = 0;
        virtual std::size_t length() const noexcept = 0;
        virtual Value get(std::size_t) const = 0;
    };

    explicit
    Array(std::shared_ptr<Impl> impl) noexcept
        : impl_(std::move(impl))
    {
    }

    std::size_t length() const noexcept
    {
        return impl_->length();
    }

    Value get(std::size_t index) const;

private:
    std::shared_ptr<Impl> impl_;
};

//------------------------------------------------

class MRDOX_DECL
    Object
{
public:
    struct MRDOX_DECL
        Impl
    {
        virtual ~Impl() = 0;
        virtual bool empty() const noexcept;
        virtual Value get(std::string_view) const = 0;
    };

    explicit
    Object(std::shared_ptr<Impl> impl) noexcept
        : impl_(std::move(impl))
    {
    }

    bool empty() const noexcept
    {
        return impl_->empty();
    }

    Value get(std::string_view key) const;

private:
    std::shared_ptr<Impl> impl_;
};

//------------------------------------------------

class MRDOX_DECL
    Value
{
    Kind kind_;

    union
    {
        std::int64_t number_;
        std::string string_;
        Object object_;
        Array array_;
    };

public:
    ~Value();
    Value() noexcept;
    Value(bool b) noexcept;
    Value(Array arr) noexcept;
    Value(Object arr) noexcept;
    Value(std::nullptr_t) noexcept;
    Value(char const* string) noexcept;
    Value(std::string_view s) noexcept;
    Value(std::string string) noexcept;

    template<class T>
    requires std::is_integral_v<T>
    Value(T number) noexcept
    {
        if constexpr(std::is_same_v<T, bool>)
            kind_ = Kind::Boolean;
        else
            kind_ = Kind::Integer;
        number_ = static_cast<std::int64_t>(number);
    }

    bool isBool() const noexcept { return kind_ == Kind::Boolean; }
    bool isArray() const noexcept { return kind_ == Kind::Array; }
    bool isObject() const noexcept { return kind_ == Kind::Object; }
    bool isNull() const noexcept { return kind_ == Kind::Null; }
    bool isInteger() const noexcept { return kind_ == Kind::Integer; }
    bool isString() const noexcept { return kind_ == Kind::String; }

    bool isTruthy() const noexcept;

    bool getBool() const noexcept
    {
        MRDOX_ASSERT(kind_ == Kind::Boolean);
        return number_ == 0;
    }

    Array const& getArray() const noexcept
    {
        MRDOX_ASSERT(kind_ == Kind::Array);
        return array_;
    }

    Object const& getObject() const noexcept
    {
        MRDOX_ASSERT(kind_ == Kind::Object);
        return object_;
    }

    std::int64_t getInteger() const noexcept
    {
        MRDOX_ASSERT(kind_ == Kind::Integer);
        return number_;
    }

    std::string_view getString() const noexcept
    {
        MRDOX_ASSERT(kind_ == Kind::String);
        return string_;
    }
};

//------------------------------------------------

inline
Value
Array::
get(
    std::size_t index) const
{
    return impl_->get(index);
}

inline
Value
Object::
get(
    std::string_view key) const
{
    return impl_->get(key);
}

//------------------------------------------------

template<class T, class... Args>
requires std::derived_from<T, Array::Impl>
Array
makeArray(Args&&... args)
{
    return Array(std::make_shared<T>(
        std::forward<Args>(args)...));
}

template<class T, class... Args>
requires std::derived_from<T, Object::Impl>
Object
makeObject(Args&&... args)
{
    return Object(std::make_shared<T>(
        std::forward<Args>(args)...));
}

} // dom
} // mrdox
} // clang

#endif
