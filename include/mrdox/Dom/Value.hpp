//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_VALUE_HPP
#define MRDOX_API_DOM_VALUE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Dom/Array.hpp>
#include <mrdox/Dom/Kind.hpp>
#include <mrdox/Dom/Function.hpp>
#include <mrdox/Dom/Object.hpp>
#include <mrdox/Dom/String.hpp>
#include <mrdox/ADT/Optional.hpp>
#include <mrdox/Support/Error.hpp>
#include <optional> // BAD
#include <string>

namespace clang {
namespace mrdox {

dom::Value
safeString(std::string_view str);

namespace dom {

/** A variant container for any kind of Dom value.
*/
class MRDOX_DECL
    Value
{
    Kind kind_;

    union
    {
        bool                b_;
        std::int64_t        i_;
        String              str_;
        Array               arr_;
        Object              obj_;
        Function            fn_;
    };

    friend class Array;
    friend class Object;
    friend Value clang::mrdox::safeString(std::string_view s);

public:
    ~Value();
    Value() noexcept;
    Value(Value const&);
    Value(Value&&) noexcept;
    Value& operator=(Value const&);
    Value& operator=(Value&&) noexcept;

    Value(dom::Kind kind) noexcept;
    Value(std::nullptr_t) noexcept;
    Value(std::int64_t) noexcept;
    Value(String str) noexcept;
    Value(Array arr) noexcept;
    Value(Object obj) noexcept;
    Value(Function fn) noexcept;


    template <std::integral T>
    requires (!std::same_as<T, bool>)
    Value(T v) noexcept : Value(std::int64_t(v)) {}

    template<class Boolean>
    requires std::is_same_v<Boolean, bool>
    Value(Boolean const& b) noexcept
        : kind_(Kind::Boolean)
        , b_(b)
    {
    }

    template<class Enum>
    requires std::is_enum_v<Enum> && (!std::same_as<Enum, dom::Kind>)
    Value(Enum v) noexcept
        : Value(static_cast<
            std::underlying_type_t<Enum>>(v))
    {
    }

    template<std::size_t N>
    Value(char const(&sz)[N])
        : Value(String(sz))
    {
    }

    // VFALCO Should this be a literal?
    Value(char const* s)
        : Value(String(s))
    {
    }

    template <std::convertible_to<String> StringLike>
    Value(StringLike const& s)
        : Value(String(s))
    {
    }

    template<class T>
    requires std::constructible_from<Value, T>
    Value(std::optional<T> const& opt)
        : Value(opt ? Value(*opt) : Value())
    {
    }

    template<class T>
    requires std::constructible_from<Value, T>
    Value(Optional<T> const& opt)
        : Value(opt ? Value(*opt) : Value())
    {
    }

    /** Return the type key of the value.
    */
    char const* type_key() const noexcept;

    /** Return the type of value contained.
    */
    dom::Kind kind() const noexcept;

    /** Return true if this is undefined.
    */
    bool isUndefined() const noexcept
    {
        return kind_ == Kind::Undefined;
    }

    /** Return true if this is null.
    */
    bool isNull() const noexcept
    {
        return kind_ == Kind::Null;
    }

    /** Return true if this is a boolean.
    */
    bool isBoolean() const noexcept
    {
        return kind_ == Kind::Boolean;
    }

    /** Return true if this is an integer.
    */
    bool isInteger() const noexcept
    {
        return kind_ == Kind::Integer;
    }

    /** Return true if this is a string.
    */
    bool isString() const noexcept
    {
        return kind_ == Kind::String;
    }

    /** Return true if this is a safe string.
    */
    bool isSafeString() const noexcept
    {
        return kind_ == Kind::SafeString;
    }

    /** Return true if this is an array.
    */
    bool isArray() const noexcept
    {
        return kind_ == Kind::Array;
    }

    /** Return true if this is an object.
    */
    bool isObject() const noexcept
    {
        return kind_ == Kind::Object;
    }

    /** Return true if this is a function.
    */
    bool isFunction() const noexcept
    {
        return kind_ == Kind::Function;
    }

    bool isTruthy() const noexcept;

    bool getBool() const noexcept
    {
        MRDOX_ASSERT(isBoolean());
        return b_ != 0;
    }

    std::int64_t
    getInteger() const noexcept
    {
        MRDOX_ASSERT(isInteger());
        return i_;
    }

    String const&
    getString() const noexcept
    {
        MRDOX_ASSERT(isString() || isSafeString());
        return str_;
    }

    /** Return the array.

        @throw Exception `! isArray()`
    */
    Array const&
    getArray() const;

    /** Return the object.

        @throw Exception `! isObject()`
    */
    Object const&
    getObject() const;

    /** Return the function.

        @throw Exception `! isFunction()`
    */
    Function const&
    getFunction() const;

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
    friend
    std::string
    toString(Value const&);

    /** Return a diagnostic string.

        This function will not traverse children.
    */
    friend
    std::string
    toStringChild(Value const&);
};

//------------------------------------------------

/** Stringify a value as JSON

    This function serialized a @ref Value to a
    string as if `JSON.stringify()` had been called
    on it.

    Recursive objects are identified.

    @return A string containing valid JSON.

    @param value The value to stringify.
 */
MRDOX_DECL
std::string
JSON_stringify(Value const& value);

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

//------------------------------------------------

} // dom
} // mrdox
} // clang

// These are here because of circular references
#include <mrdox/Dom/Array.ipp>
#include <mrdox/Dom/Function.ipp>
#include <mrdox/Dom/Object.ipp>

//------------------------------------------------

template<>
struct fmt::formatter<clang::mrdox::dom::Value>
    : fmt::formatter<std::string>
{
    auto format(
        clang::mrdox::dom::Value const& value,
        fmt::format_context& ctx) const
    {
        return fmt::formatter<std::string>::format(
            toString(value), ctx);
    }
};

#endif
