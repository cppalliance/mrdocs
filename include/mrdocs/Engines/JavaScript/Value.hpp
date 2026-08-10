//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_ENGINES_JAVASCRIPT_VALUE_HPP
#define MRDOCS_API_ENGINES_JAVASCRIPT_VALUE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Engines/JavaScript/Context.hpp>
#include <mrdocs/Engines/JavaScript/Type.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace mrdocs {
namespace js {

/** An ECMAScript value.

    This class represents a value in the
    JavaScript interpreter.

    A value is a variable that is defined
    in a @ref Scope. It can be a primitive
    type or an object.

    A @ref Value not associated with a
    @ref Scope is undefined.

    The user is responsible for ensuring that
    the lifetime of a @ref Value does not
    exceed the lifetime of the @ref Scope
    that created it.

    A value can be converted to a DOM value
    using the @ref getDom function.

    @see Scope
    @see Type

*/
class MRDOCS_DECL Value
{
protected:
    /// Shared lifetime owner for the underlying JavaScript runtime.
    std::shared_ptr<Context::Impl> impl_;

    /// Opaque engine value handle stored as an integer (engine-specific inside the implementation).
    std::uint32_t val_;

    friend class Scope;

    /** Wrap an existing engine value without transferring ownership.
        @param val JerryScript value handle that will be acquired.
        @param impl Shared runtime state that keeps the context alive.
    */
    Value(std::uint32_t val, std::shared_ptr<Context::Impl> impl) noexcept;

public:
    /** Destructor

        Releases the underlying engine handle; lifetime is tied to the shared
        @ref Context::Impl, not to a stack frame.
    */
    ~Value();

    /** Constructor

        Construct a value that is not associated
        with a @ref Scope.

        The value is undefined.

    */
    Value() noexcept;

    /** Constructor

        Duplicates the underlying engine handle held by `value` and shares the
        same runtime state.
    */
    Value(Value const&);

    /** Constructor

        The function associates the
        existing value with this object.
    */
    Value(Value&&) noexcept;

    /** Copy assignment.

        @copydetails Value(Value const&)

    */
    Value& operator=(Value const&);

    /** Move assignment.

        @copydetails Value(Value&&)

    */
    Value& operator=(Value&&) noexcept;

    /** Return the type of the value.

        This function returns the JavaScript
        type of the value.

        The type can represent a primitive
        type (such as boolean, number,
        and string) or an object.

        When the type is an object, the
        return type also classifies the
        object as an array or function.

        An array is an object with the
        internal ECMAScript class `Array`
        or a Proxy wrapping an `Array`.

        A function is an object with the
        internal ECMAScript class `Function`.

    */
    Type type() const noexcept;

    /** Check if the value is undefined.

        @return `true` if the value is undefined, `false` otherwise
    */
    bool
    isUndefined() const noexcept;

    /** Check if the value is null.

        @return `true` if the value is null, `false` otherwise
    */
    bool
    isNull() const noexcept;

    /** Check if the value is a boolean.

        @return `true` if the value is a boolean, `false` otherwise
    */
    bool
    isBoolean() const noexcept;

    /** Check if the value is a number.

        In ECMA, the number type is an IEEE double,
        including +/- Infinity and NaN values.

        Zero sign is also preserved.

        An IEEE double can represent all integers
        up to 53 bits accurately.

        The user should not rely on NaNs preserving
        their exact non-normalized form.

        @return `true` if the value is a number, `false` otherwise
    */
    bool
    isNumber() const noexcept;

    /** Check if the value is an integer number.

        All numbers are internally represented by
        IEEE doubles, which are capable of
        representing all integers up to
        53 bits accurately.

        This function returns `true` if the
        value is a number with no precision
        loss when representing an integer.

        When `isNumber()` is `true`, the
        function behaves as if evaluating
        the condition
        `d == static_cast<double>(static_cast<int>(d))`
        where `d` is the result of `toDouble()`.

        @return `true` if the value is a number with no fractional part, `false` otherwise
    */
    bool
    isInteger() const noexcept;

    /** Check if the value is a floating point number.

       @return `true` if the value is a number but not an integer, `false` otherwise
    */
    bool
    isDouble() const noexcept;

    /** Check if the value is a string.

        @return `true` if the value is a string, `false` otherwise
    */
    bool
    isString() const noexcept;

    /** Check if the value is an array.

        @return `true` if the value is an array, `false` otherwise
    */
    bool
    isArray() const noexcept;

    /** Check if the value is an object.

        Check if the value is an object but not
        an array or function.

        While in ECMA anything with properties
        is an object, this function returns
        `false` for arrays and functions.

        Properties are key-value pairs with
        a string key and an arbitrary value,
        including undefined.

        @return `true` if the value is an object, `false` otherwise

    */
    bool
    isObject() const noexcept;

    /** Check if the value is a function.

        @return `true` if the value is a function, `false` otherwise
    */
    bool
    isFunction() const noexcept;

    /** Determine if a value is truthy

        A value is truthy if it is a boolean and is true, a number and not
        zero, or an non-empty string, array or object.

        @return `true` if the value is truthy, `false` otherwise
    */
    bool
    isTruthy() const noexcept;

    /** Return the underlying string

        This function returns the value
        as a string.

        This function performs no coercions.
        If the value is not a string, it is not
        converted to a string.

        JerryScript allocates a new buffer for string extraction, so the
        returned value is an owning `std::string` rather than a view.

        @note Behaviour is undefined if `!isString()`

    */
    std::string
    getString() const;

    /** Return the underlying boolean value.

        @note Behaviour is undefined if `!isBoolean()`

    */
    bool
    getBool() const noexcept;

    /** Return the underlying integer value.

        @note Behaviour is undefined if `!isNumber()`
    */
    std::int64_t
    getInteger() const noexcept;

    /** Return the underlying double value.

        @note Behaviour is undefined if `!isNumber()`
    */
    double
    getDouble() const noexcept;

    /** Return the underlying object.

        @note Behaviour is undefined if `!isObject()`
    */
    dom::Object
    getObject() const noexcept;

    /** Return the underlying array.

        @note Behaviour is undefined if `!isArray()`
    */
    dom::Array
    getArray() const noexcept;

    /** Return the underlying array.

        @note Behaviour is undefined if `!isFunction()`
    */
    dom::Function
    getFunction() const noexcept;

    /** Return the value as a dom::Value

        This function returns the value as a
        @ref dom::Value.

        If the value is a primitive type,
        it is converted to a DOM primitive.

        If the value is an object, a type with
        reference semantics to access the
        underlying DOM object is returned.

    */
    dom::Value getDom() const;

    /** Return the element for a given key.

        If the Value is not an object, or the key
        is not found, a Value of type Kind::Undefined
        is returned.

        @param key The key to look up.
        @return The element for the given key, or
        a Value of type Kind::Undefined if
        the key is not found.
    */
    Value
    get(std::string_view key) const;

    /** @copydoc get(std::string_view)
    */
    template <std::convertible_to<std::string_view> S>
    Value
    get(S const& key) const
    {
        return get(std::string_view(key));
    }

    /** Return the element at a given index.

        @param i The index of the element to return.
        @return The element at the given index, or
        a Value of type `Kind::Undefined` if
        the index is out of range.
    */
    Value
    get(std::size_t i) const;

    /** Return the element at a given index or key.
    */
    Value
    get(dom::Value const& i) const;

    /** Lookup a sequence of keys.

        This function is equivalent to calling `get`
        multiple times, once for each key in the sequence
        of dot-separated keys.

        @param keys A sequence of keys separated by dots.

        @return The value at the end of the sequence, or
        a Value of type Kind::Undefined if any key
        is not found.
    */
    Value
    lookup(std::string_view keys) const;

    /** Set or replace the value for a given key.

        @param key The key to set.
        @param value The value to set.
    */
    void
    set(
        std::string_view key,
        Value const& value) const;

    /** Set or replace the value for a given key.

        @param key The key to set.
        @param value The value to set.
    */
    void
    set(
        std::string_view key,
        dom::Value const& value) const;

    /** Remove a property from an object if it exists.
        @param key Property name to erase from the current object.
    */
    void
    erase(std::string_view key) const;

    /** Return true if a key exists.

        @param key The key to check for.
        @return `true` if the key exists, `false` otherwise.
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

    /** Return the element for a property name.

        @param key Property name to fetch from the current object.
        @return The element for the given key, or undefined if missing / not an object.
    */
    Value
    operator[](std::string_view key) const;

    /** Return the element for an array index.

        @param index Zero-based array index to fetch when the value is an array.
        @return The element for the given index, or undefined if out of bounds / not an array.
    */
    Value
    operator[](std::size_t index) const;

    /** Invoke a function.

        @param args Zero or more arguments to pass to the method.
        @return The return value of the method.
    */
    template<std::convertible_to<dom::Value>... Args>
    Expected<Value>
    call(Args&&... args) const
    {
        return apply({ dom::Value(std::forward<Args>(args))... });
    }

    /** Invoke a function with a span of arguments.

        @param args Arguments to pass to the JavaScript function.
        @return The return value of the function.
    */
    Expected<Value>
    apply(std::span<const dom::Value> args) const;

    /** Invoke a function with an initializer_list of arguments.

        @param args Arguments to pass to the JavaScript function.
        @return The return value of the function.
    */
    Expected<Value>
    apply(std::initializer_list<dom::Value> args) const
    {
        return apply(std::span<const dom::Value>(args.begin(), args.size()));
    }

    /** Invoke a function.

        @param args Zero or more arguments to pass to the method.
        @return The return value of the method.
    */
    template<class... Args>
    Value
    operator()(Args&&... args) const
    {
        return call(std::forward<Args>(args)...).value();
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

    friend
    bool
    operator!=(
        Value const& lhs,
        Value const& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    /// @overload
    template <std::convertible_to<Value> S>
    friend auto operator!=(
        S const& lhs, Value const& rhs) noexcept
    {
        return Value(lhs) != rhs;
    }

    /// @overload
    template <std::convertible_to<Value> S>
    friend auto operator!=(
        Value const& lhs, S const& rhs) noexcept
    {
        return lhs != Value(rhs);
    }

    /** Compare two values for inequality.
    */
    friend
    std::strong_ordering
    operator<=>(
        Value const& lhs,
        Value const& rhs) noexcept;

    /** Return the first Value that is truthy, or the last one.

        This function is equivalent to the JavaScript `||` operator.
    */
    friend
    Value
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

    /** Return the first Value that is not truthy, or the last one.

        This function is equivalent to the JavaScript `&&` operator.
    */
    friend
    Value
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

        This function coerces any value to a string.
    */
    friend
    std::string
    toString(Value const& value);
};

inline
bool
Value::
isUndefined() const noexcept
{
    return type() == Type::undefined;
}

inline
bool
Value::
isNull() const noexcept
{
    return type() == Type::null;
}

inline
bool
Value::
isBoolean() const noexcept
{
    return type() == Type::boolean;
}

inline
bool
Value::
isNumber() const noexcept
{
    return type() == Type::number;
}

inline
bool
Value::
isString() const noexcept
{
    return type() == Type::string;
}

inline
bool
Value::
isObject() const noexcept
{
    return type() == Type::object;
}

inline
bool
Value::
isArray() const noexcept
{
    return type() == Type::array;
}

inline
bool
Value::
isFunction() const noexcept
{
    return type() == Type::function;
}

} // js
} // mrdocs

#endif // MRDOCS_API_ENGINES_JAVASCRIPT_VALUE_HPP
