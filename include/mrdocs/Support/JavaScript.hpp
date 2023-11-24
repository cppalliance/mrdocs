//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_JAVASCRIPT_HPP
#define MRDOCS_API_SUPPORT_JAVASCRIPT_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Error.hpp>
#include <cstdlib>
#include <string_view>
#include <span>

namespace clang {
namespace mrdocs {
namespace js {

struct Access;
class Context;
class Scope;

class Array;
class Boolean;
class Object;
class String;
class Value;

//------------------------------------------------

/** Types of values.
*/
enum class Type
{
    undefined = 1,
    null,
    boolean,
    number,
    string,
    object,
    function,
    array
};

//------------------------------------------------

class Prop
{
    unsigned int index_;
    std::string_view name_;

public:
    constexpr Prop(std::string_view name) noexcept
        : index_(0)
        , name_(name)
    {
    }

    constexpr Prop(unsigned int index) noexcept
        : index_(index)
    {
    }

    constexpr bool
    isIndex() const noexcept
    {
        return name_.empty();
    }
};

//------------------------------------------------

/** An instance of a JavaScript interpreter.

    This class represents a JavaScript interpreter
    context under which we can create @ref Scope
    objects to define variables and execute scripts.

    A context represents a JavaScript heap where
    variables can be allocated and will be later
    garbage collected.

    Each context is associated with a single
    heap allocated with default memory
    management.

    Once the context is created, a @ref Scope
    in this context can be created to define
    variables and execute scripts.

    @see Scope
*/
class MRDOCS_DECL
    Context
{
    struct Impl;

    Impl* impl_;

    friend struct Access;

public:
    /** Destructor.
    */
    ~Context();

    /** Constructor.

        Create a javascript execution context
        associated with its own garbage-collected
        heap.
    */
    Context();

    /** Constructor.

        Create a javascript execution context
        associated with the heap of another
        context.

        Both contexts will share the same
        garbage-collected heap, which is
        destroyed when the last context
        is destroyed.

        While they share the heap, their
        scripts can include references
        to the same variables.

        There are multi-threading
        restrictions, however: only one
        native thread can execute any
        code within a single heap at any
        time.

    */
    Context(Context const&) noexcept;

    /** Copy assignment.

        @copydetails Context(Context const&)

     */
    Context& operator=(Context const&) = delete;
};

//------------------------------------------------

/** A JavaScript scope

    This class represents a JavaScript scope
    under which we can define variables and
    execute scripts.

    Each scope is a section of the context heap
    in the JavaScript interpreter. A javascript
    variable is defined by creating a
    @ref Value that is associated with this
    Scope, i.e., subsection of the context heap.

    When a scope is destroyed, the heap section
    is popped and all variables defined in
    that scope are invalidated.

    For this reason, two scopes of the
    same context heap cannot be manipulated
    at the same time.

 */
class Scope
{
    Context ctx_;
    std::size_t refs_;
    int top_;

    friend struct Access;

    void reset();

public:
    /** Constructor.

        Construct a scope for the given context.

        Variables defined in this scope will be
        allocated on top of the specified
        context heap.

        When the Scope is destroyed, the
        variables defined in this scope will
        be popped from the heap.

        @param ctx The context to use.
    */
    MRDOCS_DECL
    Scope(Context const& ctx) noexcept;

    /** Destructor.

        All variables defined in this scope
        are popped from the internal context
        heap.

        There should be no @ref Value objects
        associated with this scope when it
        is destroyed.
     */
    MRDOCS_DECL
    ~Scope();

    /** Compile and run a script.

        This function compiles and executes
        the specified JavaScript code. The script
        can be used to execute commands or define
        global variables in the parent context.

        It evaluates the ECMAScript source code and
        converts any internal errors to @ref Error.

        @param jsCode The JavaScript code to execute.

     */
    MRDOCS_DECL
    Expected<void>
    script(std::string_view jsCode);

    /** Compile and run a expression.

        This function compiles and executes
        the specified JavaScript code. The script
        can be used to execute commands or define
        global variables in the parent context.

        It evaluates the ECMAScript source code and
        converts any internal errors to @ref Error.

        @param jsCode The JavaScript code to execute.

     */
    MRDOCS_DECL
    Expected<Value>
    eval(std::string_view jsCode);

    /** Compile a script and push results to stack.

        Compile ECMAScript source code and return it
        as a compiled function object that executes it.

        Unlike the `script()` function, the code is not
        executed. A compiled function that can be executed
        is returned.

        The returned function has zero arguments and
        executes as if we called `script()`.

        The script returns an implicit return value
        equivalent to the last non-empty statement value
        in the code.
    */
    MRDOCS_DECL
    Expected<Value>
    compile_script(std::string_view jsCode);

    /** Compile a script and push results to stack.

        Compile ECMAScript source code that defines a
        function and return the compiled function object.

        Unlike the `script()` function, the code is not
        executed. A compiled function with the specified
        number of arguments that can be executed is returned.

        If the function code contains more than one function, the
        return value is the first function compiled.

    */
    MRDOCS_DECL
    Expected<Value>
    compile_function(std::string_view jsCode);

    /** Return a global object if it exists.

        This function returns a @ref Value that
        represents a global variable in the
        parent context.

        If the variable does not exist, an
        error is returned.

        @param name The name of the global variable.

    */
    MRDOCS_DECL
    Expected<Value>
    getGlobal(std::string_view name);

    /** Set a global object.
    */
    MRDOCS_DECL
    void
    setGlobal(std::string_view name, dom::Value const& value);

    /** Return the global object.

        This function returns a @ref Value that
        represents the global object in the
        parent context.

        The global object is the root of the
        ECMAScript object hierarchy and is
        the value returned by the global
        `this` expression.

        If the global object does not exist, an
        error is returned.

     */
    MRDOCS_DECL
    Value
    getGlobalObject();
};

//------------------------------------------------

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
    Scope* scope_;
    int idx_;

    friend struct Access;

    Value(int, Scope&) noexcept;

public:
    /** Destructor

        If the value is associated with a
        @ref Scope and it is on top of the
        stack, it is popped. Also, if
        there are no other Value references
        to the @ref Scope, all variables
        defined in that scope are popped
        via @ref Scope::reset.

     */
    MRDOCS_DECL ~Value();

    /** Constructor

        Construct a value that is not associated
        with a @ref Scope.

        The value is undefined.

     */
    MRDOCS_DECL Value() noexcept;

    /** Constructor

        The function pushes a duplicate of
        value to the stack and associates
        the new value the top of the stack.
     */
    MRDOCS_DECL Value(Value const&);

    /** Constructor

        The function associates the
        existing value with this object.
     */
    MRDOCS_DECL Value(Value&&) noexcept;

    /** Copy assignment.

        @copydetails Value(Value const&)

     */
    MRDOCS_DECL Value& operator=(Value const&);

    /** Move assignment.

        @copydetails Value(Value&&)

     */
    MRDOCS_DECL Value& operator=(Value&&) noexcept;

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
    MRDOCS_DECL Type type() const noexcept;

    /// Check if the value is undefined.
    bool
    isUndefined() const noexcept;

    /// Check if the value is null.
    bool
    isNull() const noexcept;

    /// Check if the value is a boolean.
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

     */
    bool
    isInteger() const noexcept;

    /// Check if the value is a floating point number.
    bool
    isDouble() const noexcept;

    /// Check if the value is a string.
    bool
    isString() const noexcept;

    /// Check if the value is an array.
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

     */
    bool
    isObject() const noexcept;

    /// Check if the value is a function.
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

        @note Behaviour is undefined if `!isString()`

     */
    std::string_view
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

    /** Set "log" property

        This function sets the "log" property
        in the object.

        The "log" property is populated with
        a function that takes two javascript
        arguments `(level, message)` where
        `level` is an unsigned integer and
        `message` is a string.

        The mrdocs library function
        `clang::mrdocs::report::print`
        is then called with these
        two arguments to report a
        message to the console.

     */
    void setlog();


    /** Return the element for a given key.

        If the Value is not an object, or the key
        is not found, a Value of type @ref Kind::Undefined
        is returned.
    */
    Value
    get(std::string_view key) const;

    template <std::convertible_to<std::string_view> S>
    Value
    get(S const& key) const
    {
        return get(std::string_view(key));
    }

    /** Return the element at a given index.
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

        @return The value at the end of the sequence, or
        a Value of type @ref Kind::Undefined if any key
        is not found.
    */
    Value
    lookup(std::string_view keys) const;

    /** Set or replace the value for a given key.
     */
    MRDOCS_DECL
    void
    set(
        std::string_view key,
        Value const& value) const;

    /** Set or replace the value for a given key.
     */
    MRDOCS_DECL
    void
    set(
        std::string_view key,
        dom::Value const& value) const;

    /** Return true if a key exists.
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

    /** Invoke a function.
    */
    template<std::convertible_to<dom::Value>... Args>
    Expected<Value>
    call(Args&&... args) const
    {
        return callImpl({ dom::Value(std::forward<Args>(args))... });
    }

    /** Invoke a function with variadic arguments.
    */
    Expected<Value>
    apply(std::span<dom::Value> args) const
    {
        return callImpl(args);
    }

    /** Invoke a function.
    */
    template<class... Args>
    Value
    operator()(Args&&... args) const
    {
        return call(std::forward<Args>(args)...).value();
    }

    /** Invoke a method.
    */
    template<class... Args>
    Expected<Value>
    callProp(
        std::string_view prop,
        Args&&... args) const
    {
        return callPropImpl(prop,
            { dom::Value(std::forward<Args>(args))... });
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

private:
    MRDOCS_DECL
    Expected<Value>
    callImpl(
        std::initializer_list<dom::Value> args) const;

    MRDOCS_DECL
    Expected<Value>
    callImpl(std::span<dom::Value> args) const;

    MRDOCS_DECL
    Expected<Value>
    callPropImpl(
        std::string_view prop,
        std::initializer_list<dom::Value> args) const;
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
} // clang

#endif
