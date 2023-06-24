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

/** Reference-counting base class for dynamic types.
*/
class MRDOX_DECL
    Any
{
    std::atomic<std::size_t> mutable refs_ = 1;

protected:
    /** Destructor.
    */
    virtual ~Any() = 0;

    /** Constructor.
    */
    Any() noexcept;

    /** Constructor.

        The caller will have exclusive ownership
        immediately after construction is complete.
    */
    Any(Any const&) noexcept;

public:
    /** Return a pointer with shared ownership.
    */
    Any* addref() noexcept
    {
        ++refs_;
        return this;
    }

    /** Return a pointer with shared ownership.
    */
    Any const* addref() const noexcept
    {
        ++refs_;
        return this;
    }

    /** Release shared ownership.

        If this is the last remaining owner,
        then this will be deleted.
    */
    void release() const noexcept
    {
        if(--refs_ > 0)
            return;
        delete this;
    }
};

template<class U, class... Args>
requires std::derived_from<U, Any>
auto create(Args&&... args);

//------------------------------------------------
//
// Pointer
//
//------------------------------------------------

/** A pointer container for dynamic objects.
*/
template<class T>
class Pointer
{
    Any* any_;

    static_assert(std::derived_from<T, Any>);

    template<class U>
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
    requires std::derived_from<U, Any>
    friend auto create(Args&&... args);
};

template<class U, class... Args>
requires std::derived_from<U, Any>
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
protected:
    /** Constructor.

        The newly constructed object will retain
        a copy of the list of values in `other`.
    */
    Object(Object const& other);

public:
    /** The type of an element in this container.
    */
    using value_type = std::pair<std::string, Value>;

    /** The underlying, iterable range used by this container.
    */
    using list_type = std::vector<value_type>;

    /** Constructor.

        Default-constructed objects are empty.
    */
    Object() noexcept;

    /** Constructor.

        Upon construction, the object will retain
        ownership of a shallow copy of the specified
        list. In particular, dynamic objects will
        be acquired with shared ownership.

        @param list The initial list of values.
    */
    explicit Object(list_type list);

    /** Return an iterable range with the contents.
    */
    list_type const& list() const noexcept;

    /** Add elements to the container.

        No checking is performed to prevent
        the insertion of duplicate keys.
    */
    void append(std::string_view key, Value value);

    /** Add elements to the container.

        No checking is performed to prevent
        the insertion of duplicate keys.
    */
    void append(std::initializer_list<value_type>);

    /** Return a deep copy of the container.

        In particular, dynamic objects will be
        deeply copied; changes to the copy are
        not reflected in the original.
    */
    virtual Value copy() const;

    /** Return true if the container is empty.
    */
    virtual bool empty() const noexcept;

    /** Return the value for a given key.

        If the key does not exist, a null value
        is returned.

        @return The value, or null.

        @param key The key.
    */
    virtual Value get(std::string_view key) const;

    /** Set or replace the value for a given key.

        This function inserts a new key or changes
        the value for the existing key if it is
        already present.

        @param key The key.

        @param value The value to set.
    */
    virtual void set(std::string_view key, Value value);

    // VFALCO DEPRECATED (for duktape)
    virtual std::vector<std::string_view> props() const;

private:
    list_type list_;
};

/** Alias for a pointer to an Object.
*/
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

    /** Return the object.
    */
    virtual ObjectPtr construct() const = 0;

public:
    /** Destructor.
    */
    ~LazyObject() noexcept;

    /** Constructor.
    */
    LazyObject() noexcept;

    /** Return the object.
    */
    ObjectPtr get() const;
};

/** Alias for a pointer to a LazyObject.
*/
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

    /** Return a copy.
    */
    Value copy() const;

    /** Return the type of value contained.|
    */
    dom::Kind kind() const noexcept;

    /** Return true if this is null.
    */
    bool isNull() const noexcept { return kind_ == Kind::Null; }

    /** Return true if this is a boolean.
    */
    bool isBool() const noexcept { return kind_ == Kind::Boolean; }

    /** Return true if this is an integer.
    */
    bool isInteger() const noexcept { return kind_ == Kind::Integer; }

    /** Return true if this is a string.
    */
    bool isString() const noexcept { return kind_ == Kind::String; }

    /** Return true if this is an array.
    */
    bool isArray() const noexcept { return kind_ == Kind::Array; }

    /** Return true if this is an object.
    */
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

    /** Return the object if this is an object.

        @throw Error `! isObject()`
    */
    ObjectPtr
    getObject() const;

    /** Swap two values.
    */
    void
    swap(Value& other) noexcept;

    /** Swap two values.
    */
    friend
    void
    swap(Value& v0, Value& v1) noexcept
    {
        v0.swap(v1);
    }
};

/** Return a non-empty string, or a null.
*/
inline
Value
stringOrNull(
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
