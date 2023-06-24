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
#include <initializer_list>
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
    Null,
    Boolean,
    Integer,
    String,
    Array,
    Object
};

//------------------------------------------------
//
// Any
//
//------------------------------------------------

/** A type-erased object or array implementation.
*/
class MRDOX_DECL
    Any
{
    std::atomic<std::size_t> mutable refs_ = 1;

public:
    virtual ~Any() = 0;

    Any* addref() noexcept
    {
        ++refs_;
        return this;
    }

    Any const* addref() const noexcept
    {
        ++refs_;
        return this;
    }

    void release() const noexcept
    {
        if(--refs_ > 0)
            return;
        delete this;
    }
};

template<class U, class... Args>
auto create(Args&&... args);

//------------------------------------------------
//
// Pointer
//
//------------------------------------------------

/** A pointer container for object or array.
*/
template<class T = Any>
requires std::convertible_to<T*, Any*>
class Pointer
{
    Any* any_;

    template<class U>
    requires std::convertible_to<U*, Any*>
    friend class Pointer;

    explicit
    Pointer(Any* any) noexcept
        : any_(any)
    {
    }

public:
    Pointer() = delete;

    ~Pointer()
    {
        any_->release();
    }

    Pointer(
        Pointer const& other) noexcept
        : any_(other.any_->addref())
    {
    }

    template<class U>
    requires std::convertible_to<U*, T*>
    Pointer(U* u) noexcept
        : any_(u->addref())
    {
    }

    template<class U>
    requires std::convertible_to<U*, T*>
    Pointer(Pointer<U> const& other) noexcept
        : any_(other.any_->addref())
    {
    }

    template<class U>
    requires std::convertible_to<U*, T*>
    Pointer& operator=(
        Pointer<U> const& other) noexcept
    {
        Pointer temp(other);
        temp.swap(*this);
        return *this;
    }

    T* get() const noexcept
    {
        return static_cast<T*>(any_);
    }

    T* operator->() const noexcept
    {
        return get();
    }

    T& operator*() const noexcept
    {
        return *get();
    }

    void swap(Pointer& other) noexcept
    {
        auto temp = any_;
        any_ = other.any_;
        other.any_ = temp;
    }

    friend
    void
    swap(Pointer& p0, Pointer& p1) noexcept
    {
        p0.swap(p1);
    }

    template<class U, class... Args>
    friend auto create(Args&&... args);
};

template<class U, class... Args>
auto create(Args&&... args)
{
    return Pointer<U>(new U(
        std::forward<Args>(args)...));
}

//------------------------------------------------
//
// Array
//
//------------------------------------------------

class MRDOX_DECL
    Array : public Any
{
public:
    virtual std::size_t length() const noexcept;
    virtual Value get(std::size_t) const;
};

using ArrayPtr = Pointer<Array>;

//------------------------------------------------
//
// Object
//
//------------------------------------------------

/** A container of key and value pairs.
*/
class MRDOX_DECL
    Object : public Any
{
public:
    using value_type = std::pair<std::string, Value>;
    using list_type = std::vector<value_type>;

    Object() noexcept;
    explicit Object(list_type list);
    list_type const& list() const noexcept;
    void append(std::string_view key, Value value);
    void append(std::initializer_list<value_type>);
    virtual bool empty() const noexcept;
    virtual Value get(std::string_view key) const;
    virtual void set(std::string_view key, Value value);
    virtual std::vector<std::string_view> props() const;

private:
    list_type list_;
};

using ObjectPtr = Pointer<Object>;

/** Return a new object with a given list of properties.
*/
MRDOX_DECL
ObjectPtr
createObject(
    Object::list_type list);

//------------------------------------------------
//
// LazyObject
//
//------------------------------------------------

/** An Object whose construction is deferred.
*/
class MRDOX_DECL
    LazyObject : public Any
{
    std::atomic<Object*> mutable p_ = nullptr;

    virtual ObjectPtr construct() const = 0;

public:
    LazyObject() noexcept;
    ~LazyObject() noexcept;
    ObjectPtr get() const;
};

using LazyObjectPtr = Pointer<LazyObject>;

//------------------------------------------------

/** A variant container for any kind of Dom value.
*/
class MRDOX_DECL
    Value
{
    enum class Kind
    {
        Null,
        Boolean,
        Integer,
        String,
        Array,
        Object,
        LazyObject
    };

    Kind kind_;

    union
    {
        bool          b_;
        std::int64_t  i_;
        std::string   str_;
        ArrayPtr      arr_;
        ObjectPtr     obj_;
        LazyObjectPtr lazy_obj_;
    };

public:
    ~Value();
    Value() noexcept;
    Value(Value const&);
    Value(Value&&) noexcept;
    Value(std::nullptr_t) noexcept;
    Value(std::int64_t) noexcept;
    Value(std::string s) noexcept;
    Value(ArrayPtr arr) noexcept;
    Value(ObjectPtr obj) noexcept;
    Value(LazyObjectPtr lazy_obj) noexcept;
    Value& operator=(Value&&) noexcept;
    Value& operator=(Value const&);

    template<class Boolean>
    requires std::is_same_v<Boolean, bool>
    Value(Boolean const& b) noexcept
        : kind_(Kind::Boolean)
        , b_(b)
    {
    }

    Value(char const* s)
        : Value(std::string(s))
    {
    }

    template<class String>
    requires std::is_convertible_v<
        String, std::string_view>
    Value(String const& s)
        : Value(std::string(s))
    {
    }

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
    requires std::derived_from<T, LazyObject>
    Value(Pointer<T> const& ptr) noexcept
        : Value(LazyObjectPtr(ptr))
    {
    }

    template<class Enum>
    requires std::is_enum_v<Enum>
    Value(Enum v) noexcept
        : Value(static_cast<
            std::underlying_type_t<Enum>>(v))
    {
    }

    dom::Kind kind() const noexcept;
    bool isNull() const noexcept { return kind_ == Kind::Null; }
    bool isBool() const noexcept { return kind_ == Kind::Boolean; }
    bool isInteger() const noexcept { return kind_ == Kind::Integer; }
    bool isString() const noexcept { return kind_ == Kind::String; }
    bool isArray() const noexcept { return kind_ == Kind::Array; }
    bool isObject() const noexcept
    {
        return
            kind_ == Kind::Object ||
            kind_ == Kind::LazyObject;
    }

    bool isTruthy() const noexcept;

    bool getBool() const noexcept
    {
        MRDOX_ASSERT(kind_ == Kind::Boolean);
        return b_ != 0;
    }

    std::int64_t getInteger() const noexcept
    {
        MRDOX_ASSERT(kind_ == Kind::Integer);
        return i_;
    }

    std::string_view getString() const noexcept
    {
        MRDOX_ASSERT(kind_ == Kind::String);
        return str_;
    }

    ArrayPtr const&
    getArray() const noexcept
    {
        MRDOX_ASSERT(kind_ == Kind::Array);
        return arr_;
    }

    ObjectPtr getObject() const noexcept;

    void swap(Value& other) noexcept;

    friend void swap(Value& v0, Value& v1) noexcept
    {
        v0.swap(v1);
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
