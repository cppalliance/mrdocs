//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_SUPPORT_DOM_HPP
#define MRDOX_API_SUPPORT_DOM_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Support/SharedPtr.hpp>
#include <fmt/format.h>
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
// Array
//
//------------------------------------------------

class MRDOX_DECL
    Array
{
public:
    using value_type = Value;

    using list_type = std::vector<value_type>;

    /** Destructor.
    */
    virtual ~Array();

    /** Constructor.

        Default-constructed arrays are empty.
    */
    Array() noexcept;

    /** Constructor.

        The newly constructed array will retain
        a copy of the list of values in `other`.
    */
    Array(Array const& other);

    /** Constructor.

        Upon construction, the object will retain
        ownership of a shallow copy of the specified
        list. In particular, dynamic objects will
        be acquired with shared ownership.

        @param list The initial list of values.
    */
    explicit Array(list_type list);

    virtual bool empty() const noexcept;
    virtual std::size_t size() const noexcept;
    virtual Value get(std::size_t) const;

    /** Return a deep copy of the container.

        In particular, dynamic objects will be
        deeply copied; changes to the copy are
        not reflected in the original.
    */
    virtual Value copy() const;

    list_type const& values() const noexcept;

    /** Return a diagnostic string.
    */
    std::string
    displayString() const;

private:
    std::vector<Value> list_;
};

using ArrayPtr = SharedPtr<Array>;

//------------------------------------------------
//
// LazyArray
//
//------------------------------------------------

/** An Array whose construction is deferred.
*/
class MRDOX_DECL
    LazyArray
{
    AtomicSharedPtr<Array> sp_;

    /** Return the array.
    */
    virtual ArrayPtr construct() const = 0;

public:
    /** Destructor.
    */
    virtual ~LazyArray() = 0;

    /** Constructor.
    */
    LazyArray() noexcept;

    /** Return the object.
    */
    ArrayPtr get() const;
};

/** Alias for a pointer to a LazyArray.
*/
using LazyArrayPtr = SharedPtr<LazyArray>;

//------------------------------------------------
//
// Object
//
//------------------------------------------------

/** A container of key and value pairs.
*/
class MRDOX_DECL
    Object
{
public:
    /** The type of an element in this container.
    */
    using value_type = std::pair<std::string, Value>;

    /** The underlying, iterable range used by this container.
    */
    using list_type = std::vector<value_type>;

    /** Destructor.
    */
    virtual ~Object();

    /** Constructor.

        Default-constructed objects are empty.
    */
    Object() noexcept;

    /** Constructor.

        The newly constructed object will retain
        a copy of the list of values in `other`.
    */
    Object(Object const& other);

    /** Constructor.

        Upon construction, the object will retain
        ownership of a shallow copy of the specified
        list. In particular, dynamic objects will
        be acquired with shared ownership.

        @param list The initial list of values.
    */
    explicit Object(list_type list);

    /** Return true if the container is empty.
    */
    bool empty() const noexcept;

    /** Return the number of elements.
    */
    std::size_t size() const noexcept;

    /** Return the range of contained values.
    */
    list_type const& values() const noexcept;

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

    /** Return a diagnostic string.
    */
    std::string
    displayString() const;

    // VFALCO DEPRECATED (for duktape)
    virtual std::vector<std::string_view> props() const;

private:
    list_type list_;
};

/** Alias for a pointer to an Object.
*/
using ObjectPtr = SharedPtr<Object>;

//------------------------------------------------
//
// LazyObject
//
//------------------------------------------------

/** An Object whose construction is deferred.
*/
class MRDOX_DECL
    LazyObject
{
    AtomicSharedPtr<Object> sp_;

    /** Return the object.
    */
    virtual ObjectPtr construct() const = 0;

public:
    /** Destructor.
    */
    virtual ~LazyObject() = 0;

    /** Constructor.
    */
    LazyObject() noexcept;

    /** Return the object.
    */
    ObjectPtr get() const;
};

/** Alias for a pointer to a LazyObject.
*/
using LazyObjectPtr = SharedPtr<LazyObject>;

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
        LazyArray,
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
        LazyArrayPtr  lazy_arr_;
        LazyObjectPtr lazy_obj_;
    };

    friend class Array;
    friend class Object;

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
    Value(LazyArrayPtr lazy_arr) noexcept;
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
    Value(SharedPtr<T> const& ptr) noexcept
        : Value(ArrayPtr(ptr))
    {
    }

    template<class T>
    requires std::derived_from<T, Object>
    Value(SharedPtr<T> const& ptr) noexcept
        : Value(ObjectPtr(ptr))
    {
    }

    template<class T>
    requires std::derived_from<T, LazyArray>
    Value(SharedPtr<T> const& ptr) noexcept
        : Value(LazyArrayPtr(ptr))
    {
    }

    template<class T>
    requires std::derived_from<T, LazyObject>
    Value(SharedPtr<T> const& ptr) noexcept
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
    bool isArray() const noexcept
    {
        return
            kind_ == Kind::Array ||
            kind_ == Kind::LazyArray;
    }

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

    /** Return the array if this is an object.

        @throw Error `! isArray()`
    */
    ArrayPtr
    getArray() const;

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

    /** Return a diagnostic string.
    */
    std::string
    displayString() const;

private:
    /** Return a diagnostic string.

        This function will not traverse children.
    */
    std::string
    displayString1() const;
};

//------------------------------------------------

/** Create an object from an initializer list.
*/
inline
ObjectPtr
createObject(
    std::initializer_list<Object::value_type> init)
{
    return makeShared<Object>(Object::list_type(init));
}

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

//------------------------------------------------

template<>
struct fmt::formatter<clang::mrdox::dom::Object>
    : fmt::formatter<std::string>
{
    auto format(
        clang::mrdox::dom::Object const& value,
        fmt::format_context& ctx) const
    {
        return fmt::formatter<std::string>::format(
            value.displayString(), ctx);
    }
};

template<>
struct fmt::formatter<clang::mrdox::dom::Value>
    : fmt::formatter<std::string>
{
    auto format(
        clang::mrdox::dom::Value const& value,
        fmt::format_context& ctx) const
    {
        return fmt::formatter<std::string>::format(
            value.displayString(), ctx);
    }
};

#endif
