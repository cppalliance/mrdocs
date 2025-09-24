//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_DOM_VALUE_HPP
#define MRDOCS_API_DOM_VALUE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/Dom/Array.hpp>
#include <mrdocs/Dom/Function.hpp>
#include <mrdocs/Dom/Kind.hpp>
#include <mrdocs/Dom/Object.hpp>
#include <mrdocs/Dom/String.hpp>
#include <mrdocs/Support/Error.hpp>
#include <charconv>
#include <compare>
#include <format>
#include <optional> // BAD
#include <string>

namespace clang {
namespace mrdocs {

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

/** Objects representing JSON-like values.

    This class is a variant-like container for holding
    any kind of value that can be represented in JSON,
    with extensions for functions and "safe strings".

    The class supports the following types:

    - Undefined
    - Null
    - Boolean
    - Integer
    - String
    - SafeString
    - Array
    - Object
    - Function

    The class provides type-safe accessors for each type,
    as well as methods to check the type of the contained value.

    Example:

    @code{.cpp}
        dom::Value v1 = 42; // Integer
        dom::Value v2 = "Hello, World!"; // String
        dom::Value v3 = dom::Array{v1, v2}; // Array

        if (v1.isInteger())
        {
            std::cout << "v1 is an integer: " << v1.getInteger() << "\n";
        }

        if (v2.isString())
        {
            std::cout << "v2 is a string: " << v2.getString() << "\n";
        }

        if (v3.isArray())
        {
            std::cout << "v3 is an array with " << v3.getArray().size() << " elements.\n";
        }
    @endcode

*/
namespace dom {

/** A variant container for any kind of Dom value.
*/
class MRDOCS_DECL
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
    friend Value clang::mrdocs::safeString(std::string_view str);

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

    template<std::same_as<bool> Boolean>
    Value(Boolean const& b) noexcept
        : kind_(Kind::Boolean)
        , b_(b)
    {
    }

    template <std::integral T>
    requires
        (!std::same_as<T, bool>) &&
        (!std::same_as<T, char>)
    Value(T v) noexcept : Value(std::int64_t(v)) {}

    template <std::floating_point T>
    Value(T v) noexcept : Value(std::int64_t(v)) {}

    Value(char c) noexcept : Value(std::string_view(&c, 1)) {}

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

    /** Return the underlying boolean value.

        @note Behaviour is undefined if `!isBoolean()`

     */
    bool getBool() const noexcept
    {
        MRDOCS_ASSERT(isBoolean());
        return b_ != 0;
    }

    /** Return the underlying integer value.

        @note Behaviour is undefined if `!isInteger()`
    */
    std::int64_t
    getInteger() const noexcept
    {
        MRDOCS_ASSERT(isInteger());
        return i_;
    }

    /** Return the underlying string value.

        @note Behaviour is undefined if `!isString()`
    */
    String const&
    getString() const noexcept
    {
        MRDOCS_ASSERT(isString() || isSafeString());
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

        @param key The key.
        @return The value for the specified key, or a Value of type
            @ref Kind::Undefined if the key does not exist.
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

        @param i The index.
        @return The value at the specified index, or a Value of type
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

        @param keys The dot-separated sequence of keys.
        @return The value at the end of the sequence, or
        a Value of type @ref Kind::Undefined if any key
        is not found.
    */
    dom::Value
    lookup(std::string_view keys) const;

    /** Set or replace the value for a given key.

        @param key The key.
        @param value The value to set.
     */
    void
    set(String const& key, Value const& value);

    /** Return true if a key exists.

        @param key The key to check for existence.
        @return `true` if the key exists, otherwise `false`.
    */
    bool
    exists(std::string_view key) const;

    /** Return if an Array or Object is empty.
    */
    bool
    empty() const;

    /** Return if an Array or Object is empty.
    */
    std::size_t
    size() const;

    /** Invoke the function.

        If the Value is not an object, or the key
        is not found, a Value of type @ref Kind::Undefined
        is returned.

        @param args The arguments to the function.
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

    /** Compare two values for inequality.
     */
    friend
    std::strong_ordering
    operator<=>(
        Value const& lhs,
        Value const& rhs) noexcept;

    /// @overload
    template <std::convertible_to<Value> S>
    friend
    auto
    operator<=>(
        S const& lhs,
        Value const& rhs) noexcept
    {
        return Value(lhs) <=> rhs;
    }

    /// @overload
    template <std::convertible_to<Value> S>
    friend
    auto
    operator<=>(
        Value const& lhs,
        S const& rhs) noexcept
    {
        return lhs <=> Value(rhs);
    }

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

    /** Return value as a string.
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
MRDOCS_DECL
std::string
stringify(dom::Value const& value);
}

/** Return a non-empty string, or a null.

    @param s The string to check.
*/
inline
Value
stringOrNull(
    std::string_view s)
{
    if(!s.empty())
    {
        return {s};
    }
    return nullptr;
}

/** Return a non-empty string, or a null.

    @param s The string to check.
*/
inline
Value
stringOrNull(
    Optional<std::string> s)
{
    if(s.has_value())
    {
        return {*s};
    }
    return nullptr;
}

//------------------------------------------------

/** Customization point tag.

    This tag type is used by the function
    @ref dom::ValueFrom to select overloads
    of `tag_invoke`.

    @note This type is empty; it has no members.

    @see @ref dom::ValueFrom
    <a href="http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1895r0.pdf">
        tag_invoke: A general pattern for supporting customisable functions</a>
*/
struct ValueFromTag { };

/** Concept to determine if a type can be converted to a @ref dom::Value
    with a user-provided conversion.

    This concept determines if the user-provided conversion is
    defined as:

    @code
    void tag_invoke( ValueFromTag, dom::Value&, T );
    @endcode
 */
template<class T>
concept HasValueFromWithoutContext = requires(
    Value& v,
    T const& t)
{
    tag_invoke(ValueFromTag{}, v, t);
};

/** Concept to determine if a type can be converted to a @ref dom::Value
    with a user-provided conversion.

    This concept determines if the user-provided conversion is
    defined as:

    @code
    void tag_invoke( ValueFromTag, dom::Value&, T,  Context const& );
    @endcode
 */
template<class T, class Context>
concept HasValueFromWithContext = requires(
    Value& v,
    T const& t,
    Context const& ctx)
{
    tag_invoke(ValueFromTag{}, v, t, ctx);
};

/** Determine if `T` can be converted to @ref dom::Value.

    If `T` can be converted to @ref dom::Value via a
    call to @ref dom::ValueFrom, the static data member `value`
    is defined as `true`. Otherwise, `value` is
    defined as `false`.

    @see @ref dom::ValueFrom
*/
template <class T, class Context>
concept HasValueFrom =
    HasValueFromWithContext<T, Context> ||
    HasValueFromWithoutContext<T> ||
    std::constructible_from<Value, T>;

/** Determine if ` T`  can be converted to @ref dom::Value
    without a context.

    This concept determines if there is a user-provided
    conversion to @ref dom::Value that does not require
    a context or if @ref dom::Value has a constructor
    that can be used to convert `T` to a @ref dom::Value.
 */
template <class T>
concept HasStandaloneValueFrom =
    HasValueFromWithoutContext<T> ||
    std::constructible_from<Value, T>;

/** Convert an object of type `T` to @ref dom::Value.

    This function attempts to convert an object
    of type `T` to @ref dom::Value using

    @li a user-provided overload of `tag_invoke`.

    @li one of @ref dom::Value's constructors,

    Conversion of user-provided types is done by calling an overload of
    `tag_invoke` found by argument-dependent lookup. Its signature should
    be similar to:

    @code
    void tag_invoke( ValueFromTag, dom::Value&, T,  Context const& );
    @endcode

    or

    @code
    void tag_invoke( ValueFromTag, dom::Value&, T );
    @endcode

    The overloads are checked for existence in that order and the first that
    matches will be selected.

    The `ctx` argument can be used either as a tag type to provide conversions
    for third-party types, or to pass extra data to the conversion function.

    @par Exception Safety
    Strong guarantee.

    @tparam T The type of the object to convert.

    @tparam Context The type of context passed to the conversion function.

    @param t The object to convert.

    @param ctx Context passed to the conversion function.

    @param v @ref dom::Value out parameter.

    @see @ref dom::ValueFromTag
    <a href="http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1895r0.pdf">
        tag_invoke: A general pattern for supporting customisable functions</a>
*/
template <class Context, HasValueFrom<Context> T>
void
ValueFrom(
    T&& t,
    Context const& ctx,
    Value& v)
{
    using BT = std::remove_cvref_t<T>;
    if constexpr (HasValueFromWithContext<BT, Context>)
    {
        tag_invoke(ValueFromTag{}, v, static_cast<T&&>(t), ctx);
    }
    else {
        ValueFrom(static_cast<T&&>(t), v);
    }
}

/** Convert an object of type `T` to @ref dom::Value.

    This function attempts to convert an object
    of type `T` to @ref dom::Value using

    @li a user-provided overload of `tag_invoke`.

    @li one of @ref dom::Value's constructors,

    Conversion of other types is done by calling an overload of `tag_invoke`
    found by argument-dependent lookup. Its signature should be similar to:

    @code
    void tag_invoke( ValueFromTag, dom::Value&, T );
    @endcode

    @par Exception Safety
    Strong guarantee.

    @tparam T The type of the object to convert.

    @param t The object to convert.

    @param v @ref dom::Value out parameter.

    @see @ref dom::ValueFromTag
    <a href="http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1895r0.pdf">
        tag_invoke: A general pattern for supporting customisable functions</a>
*/
template<class T>
requires HasStandaloneValueFrom<T>
void
ValueFrom(
    T&& t,
    Value& v)
{
    using BT = std::remove_cvref_t<T>;
    if constexpr (HasValueFromWithoutContext<BT>)
    {
        tag_invoke(ValueFromTag{}, v, static_cast<T&&>(t));
    }
    else /* if constexpr (std::constructible_from<Value, T>) */
    {
        v = Value(static_cast<T&&>(t));
    }
}

/** Convert an object of type `T` to @ref dom::Value.

    This function attempts to convert an object
    of type `T` to @ref dom::Value using

    @li a user-provided overload of `tag_invoke`.

    @li one of @ref dom::Value's constructors,

    Conversion of other types is done by calling an overload of `tag_invoke`
    found by argument-dependent lookup. Its signature should be similar to:

    @code
    void tag_invoke( ValueFromTag, dom::Value&, T );
    @endcode

    @par Exception Safety
    Strong guarantee.

    @tparam T The type of the object to convert.

    @param t The object to convert.

    @return @ref dom::Value out parameter.

    @see @ref dom::ValueFromTag,
    <a href="http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1895r0.pdf">
        tag_invoke: A general pattern for supporting customisable functions</a>
*/
template<class T>
requires HasStandaloneValueFrom<T>
Value
ValueFrom(T&& t)
{
    dom::Value v;
    ValueFrom(static_cast<T&&>(t), v);
    return v;
}

/** Convert an object of type `T` to @ref dom::Value with a context

    This function attempts to convert an object
    of type `T` to @ref dom::Value using

    @li a user-provided overload of `tag_invoke`.

    @li one of @ref dom::Value's constructors,

    Conversion of other types is done by calling an overload of `tag_invoke`
    found by argument-dependent lookup. Its signature should be similar to:

    @code
    void tag_invoke( ValueFromTag, dom::Value&, T );
    @endcode

    @par Exception Safety
    Strong guarantee.

    @tparam T The type of the object to convert.

    @param t The object to convert.

    @param ctx Context passed to the conversion function.

    @return @ref dom::Value out parameter.

    @see @ref dom::ValueFromTag,
    <a href="http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1895r0.pdf">
        tag_invoke: A general pattern for supporting customisable functions</a>
*/
template <class T, class Context>
requires HasValueFrom<T, Context>
Value
ValueFrom(T&& t, Context const& ctx)
{
    dom::Value v;
    ValueFrom(static_cast<T&&>(t), ctx, v);
    return v;
}

} // dom

template <std::convertible_to<std::string_view> SV>
dom::Value
safeString(SV const& str) {
    return safeString(std::string_view(str));
}

} // mrdocs
} // clang

// These are here because of circular references
#include <mrdocs/Dom/Array.ipp>
#include <mrdocs/Dom/Function.ipp>
#include <mrdocs/Dom/Object.ipp>

//------------------------------------------------

template <>
struct std::formatter<clang::mrdocs::dom::Value>
    : public std::formatter<std::string> {
  template <class FmtContext>
  auto format(clang::mrdocs::dom::Value const &value, FmtContext &ctx) const {
    return std::formatter<std::string>::format(toString(value), ctx);
  }
};

#endif
