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
#include <charconv>
#include <compare>

namespace clang {
namespace mrdox {

/** Create a wrapper for a safe string.

    This string wrapper prevents the string from being escaped
    when the template is rendered.

    When a helper returns a safe string, it will be marked
    as safe and will not be escaped when rendered. The
    string will be rendered as if converted to a `dom::Value`
    and rendered as-is.

    When constructing the string that will be marked as safe, any
    external content should be properly escaped using the
    `escapeExpression` function to avoid potential security concerns.

    @param str The string to mark as safe
    @return The safe string wrapper

    @see https://handlebarsjs.com/api-reference/utilities.html#handlebars-safestring-string
 */
dom::Value
safeString(std::string_view str);

dom::Value
safeString(dom::Value const& str);

namespace dom {

/** A variant container for any kind of Dom value.
*/
class MRDOX_DECL
    Value
{
    Kind kind_{Kind::Undefined};

    union
    {
        bool                b_{false};
        std::int64_t        i_;
        String              str_;
        Array               arr_;
        Object              obj_;
        Function            fn_;
    };

    friend class Array;
    friend class Object;
    friend Value clang::mrdox::safeString(std::string_view str);

public:
    ~Value();
    Value() noexcept;
    Value(Value const& other);
    Value(Value&& other) noexcept;
    Value(dom::Kind kind) noexcept;
    Value(std::nullptr_t v) noexcept;
    Value(std::int64_t v) noexcept;
    Value(String str) noexcept;
    Value(Array arr) noexcept;
    Value(Object obj) noexcept;
    Value(Function fn) noexcept;

    template<class F>
    requires
        function_traits_convertible_to_value<F>
    Value(F const& f)
        : Value(Function(f))
    {}

    template<class Boolean>
    requires std::is_same_v<Boolean, bool>
    Value(Boolean const& b) noexcept
        : kind_(Kind::Boolean)
        , b_(b)
    {
    }

    template <std::integral T>
    requires (!std::same_as<T, bool> && !std::same_as<T, char>)
    Value(T v) noexcept : Value(std::int64_t(v)) {}

    template <std::floating_point T>
    Value(T v) noexcept : Value(std::int64_t(v)) {}

    Value(char c) noexcept : Value(std::string_view(&c, 1)) {}

    template<class Enum>
    requires std::is_enum_v<Enum> && (!std::same_as<Enum, dom::Kind>)
    Value(Enum v) noexcept
        : Value(static_cast<std::underlying_type_t<Enum>>(v))
    {}

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
        : Value(opt.value_or(Value(Kind::Undefined)))
    {
    }

    template<class T>
    requires std::constructible_from<Value, T>
    Value(Optional<T> const& opt)
        : Value(opt ? Value(*opt) : Value(Kind::Undefined))
    {
    }

    Value(Array::storage_type elements)
        : Value(Array(std::move(elements)))
    {
    }

    Value& operator=(Value const& other);
    Value& operator=(Value&& other) noexcept;

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

    /** Determine if a value is truthy

        A value is truthy if it is a boolean and is true, a number and not
        zero, or an non-empty string, array or object.

        @return `true` if the value is truthy, `false` otherwise
    */
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

    /** @copydoc getArray()
    */
    Array&
    getArray();

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

    /** Return the element for a given key.

        If the Value is not an object, or the key
        is not found, a Value of type @ref Kind::Undefined
        is returned.
    */
    dom::Value
    get(std::string_view key) const;

    template <std::convertible_to<std::string_view> S>
    dom::Value
    get(S const& key) const
    {
        return get(std::string_view(key));
    }

    /** Return the element at a given index.
    */
    dom::Value
    get(std::size_t i) const;

    /** Return the element at a given index or key.
    */
    dom::Value
    get(dom::Value const& i) const;

    /** Lookup a sequence of keys.

        This function is equivalent to calling `get`
        multiple times, once for each key in the sequence
        of dot-separated keys.

        @return The value at the end of the sequence, or
        a Value of type @ref Kind::Undefined if any key
        is not found.
    */
    dom::Value
    lookup(std::string_view keys) const;

    /** Set or replace the value for a given key.
     */
    void
    set(String const& key, Value const& value);

    /** Return true if a key exists.
    */
    bool
    exists(std::string_view key) const;

    /** Invoke the function.

        If the Value is not an object, or the key
        is not found, a Value of type @ref Kind::Undefined
        is returned.
     */
    template<class... Args>
    Value operator()(Args&&... args) const
    {
        if (!isFunction())
        {
            return Kind::Undefined;
        }
        return getFunction()(std::forward<Args>(args)...);
    }

    /** Return if an Array or Object is empty.
    */
    bool
    empty() const;

    /** Return if an Array or Object is empty.
    */
    std::size_t
    size() const;

    /// @copydoc isTruthy()
    explicit
    operator bool() const noexcept
    {
        return isTruthy();
    }

    /** Return the string.
     */
    explicit
    operator std::string() const noexcept
    {
        return toString(*this);
    }

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

    /** Compare two values for equality.

        This operator uses strict equality, meaning that
        the types must match exactly, and for objects and
        arrays the children must match exactly.

        The `==` operator behaves differently for objects
        compared to primitive data types like numbers and strings.
        When comparing objects using `==`, it checks for
        reference equality, not structural equality.

        This means that two objects are considered equal with
        `===` only if they reference the exact same object in
        memory.

        @note In JavaScript, this is equivalent to the `===`
        operator, which does not perform type conversions.
     */
    friend
    bool
    operator==(
        Value const& lhs,
        Value const& rhs) noexcept;

    /// @overload
    template <std::convertible_to<Value> S>
    friend auto operator==(
        S const& lhs, Value const& rhs) noexcept
    {
        return Value(lhs) == rhs;
    }

    /// @overload
    template <std::convertible_to<Value> S>
    friend auto operator==(
        Value const& lhs, S const& rhs) noexcept
    {
        return lhs == Value(rhs);
    }

    /** Compare two values for inequality.
     */
    friend
    std::strong_ordering
    operator<=>(
        Value const& lhs,
        Value const& rhs) noexcept;

    /** Add or concatenate two values.
     */
    friend
    dom::Value
    operator+(Value const& lhs, Value const& rhs);

    /// @overload
    template <std::convertible_to<Value> S>
    friend auto operator+(
        S const& lhs, Value const& rhs) noexcept
    {
        return Value(lhs) + rhs;
    }

    /// @overload
    template <std::convertible_to<Value> S>
    friend auto operator+(
        Value const& lhs, S const& rhs) noexcept
    {
        return lhs + Value(rhs);
    }

    /** Return the first dom::Value that is truthy, or the last one.

        This function is equivalent to the JavaScript `||` operator.
     */
    friend
    dom::Value
    operator||(Value const& lhs, Value const& rhs);

    /// @overload
    template <std::convertible_to<Value> S>
    friend auto operator||(
        S const& lhs, Value const& rhs) noexcept
    {
        return Value(lhs) || rhs;
    }

    /// @overload
    template <std::convertible_to<Value> S>
    friend auto operator||(
        Value const& lhs, S const& rhs) noexcept
    {
        return lhs || Value(rhs);
    }

    /** Return the first dom::Value that is not truthy, or the last one.

        This function is equivalent to the JavaScript `&&` operator.
     */
    friend
    dom::Value
    operator&&(Value const& lhs, Value const& rhs);

    /// @overload
    template <std::convertible_to<Value> S>
    friend auto operator&&(
        S const& lhs, Value const& rhs) noexcept
    {
        return Value(lhs) && rhs;
    }

    /// @overload
    template <std::convertible_to<Value> S>
    friend auto operator&&(
        Value const& lhs, S const& rhs) noexcept
    {
        return lhs && Value(rhs);
    }

    /** Return a diagnostic string.
    */
    friend
    std::string
    toString(Value const& value);
};

//------------------------------------------------

namespace JSON
{
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
stringify(dom::Value const& value);
}

/** Return a non-empty string, or a null.
*/
inline
Value
stringOrNull(
    std::string_view s)
{
    if(!s.empty())
    {
        return s;
    }
    return nullptr;
}

//------------------------------------------------

} // dom

template <std::convertible_to<std::string_view> SV>
dom::Value
safeString(SV const& str) {
    return safeString(std::string_view(str));
}

} // mrdox
} // clang

// These are here because of circular references
#include <mrdox/Dom/Array.ipp>
#include <mrdox/Dom/Function.ipp>
#include <mrdox/Dom/Object.ipp>

//------------------------------------------------

template<>
struct fmt::formatter<clang::mrdox::dom::Value>
    : public fmt::formatter<std::string>
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
