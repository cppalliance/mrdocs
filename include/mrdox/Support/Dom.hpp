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
#include <atomic>
#include <cstdint>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

#include <vector>

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

/** A type-erased object or array implementation.
*/
class MRDOX_DECL
    Any
{
    void* ptr_;
    std::atomic<std::size_t> refs_ = 1;

protected:
    explicit Any(void* ptr) noexcept
        : ptr_(ptr)
    {
    }

public:
    virtual ~Any() = 0;

    void* get() const noexcept
    {
        return ptr_;
    }

    Any* addref() noexcept
    {
        ++refs_;
        return this;
    }

    void release() noexcept
    {
        if(--refs_ > 0)
            return;
        delete this;
    }
};

/** A pointer container for object or array.
*/
template<class T>
requires std::derived_from<T, Any>
class Pointer
{
public: // VFALCO FIXME!
    Any* any_;

public: // VFALCO FIXME!
    explicit
    Pointer(Any* any) noexcept
        : any_(any)
    {
    }

public:
    Pointer() = delete;
    Pointer& operator=(
        Pointer const&) = delete;

    ~Pointer()
    {
        any_->release();
    }

    Pointer(Pointer const& other) noexcept
        : any_(other.any_->addref())
    {
    }

    template<class U>
    requires std::derived_from<U, T>
    Pointer(Pointer<U> const& other) noexcept
        : any_(other.any_->addref())
    {
    }

    T* get() const noexcept
    {
        return reinterpret_cast<T*>(any_->get());
    }

    T* operator->() const noexcept
    {
        return get();
    }

    T& operator*() const noexcept
    {
        return *get();
    }

    Any* any() const noexcept
    {
        return any_;
    }
};

template<
    class T, class... Args>
requires std::derived_from<T, Any>
Pointer<T>
makePointer(Args&&... args)
{
    return Pointer<T>(new T(
        std::forward<Args>(args)...));
}

//------------------------------------------------

class MRDOX_DECL
    Array : public Any
{
public:
    Array();
    virtual std::size_t length() const noexcept = 0;
    virtual Value get(std::size_t) const = 0;
};

using ArrayPtr = Pointer<Array>;

//------------------------------------------------

class MRDOX_DECL
    Object : public Any
{
public:
    Object();
    virtual bool empty() const noexcept;
    virtual Value get(std::string_view) const = 0;
    virtual std::vector<std::string_view> props() const = 0;
};

using ObjectPtr = Pointer<Object>;

//------------------------------------------------

class MRDOX_DECL
    Value
{
    Kind kind_;

    union
    {
        std::int64_t number_;
        std::string string_;
        ArrayPtr array_;
        ObjectPtr object_;
    };

public:
    ~Value();
    Value() noexcept;
    Value(bool b) noexcept;
    Value(ArrayPtr const& arr) noexcept;
    Value(ObjectPtr const& arr) noexcept;
    Value(std::nullptr_t) noexcept;
    Value(char const* string) noexcept;
    Value(std::string_view s) noexcept;
    Value(std::string string) noexcept;

    template<class T>
    requires std::derived_from<T, Array>
    Value(Pointer<T> const& ptr) noexcept
        : Value(ArrayPtr(ptr))
    {
    }

    template<class T>
    requires std::derived_from<T, Object>
    Value(Pointer<T> const& ptr) noexcept
        : Value(ObjectPtr(ptr))
    {
    }

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

    Kind kind() const noexcept { return kind_; }
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

    ArrayPtr const&
    getArray() const noexcept
    {
        MRDOX_ASSERT(kind_ == Kind::Array);
        return array_;
    }

    ObjectPtr const&
    getObject() const noexcept
    {
        MRDOX_ASSERT(kind_ == Kind::Object);
        return object_;
    }
};

inline
Value
nonEmptyString(
    std::string_view s)
{
    if(! s.empty())
        return s;
    return nullptr;
}

} // dom
} // mrdox
} // clang

#endif
