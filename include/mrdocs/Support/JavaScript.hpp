//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_JAVASCRIPT_HPP
#define MRDOCS_API_SUPPORT_JAVASCRIPT_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Error.hpp>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <span>
#include <string_view>

namespace mrdocs {

class Handlebars;

/** JavaScript interop helpers for the embedded runtime.

    These functions abstract over the embedded JavaScript engine so that
    scripts and helpers can be bound, invoked, and marshalled without leaking
    engine-specific types into the rest of the codebase.

    ## Implementation Notes (for Python/Lua integrations)

    The current implementation uses JerryScript with the following design choices:

    ### Context Isolation
    Each @ref Context owns an independent JerryScript interpreter with its own
    512KB heap. This allows multiple threads to execute JavaScript in parallel
    by giving each thread its own Context. The `JERRY_EXTERNAL_CONTEXT` build
    flag enables this multi-context support.

    ### Scope and Value Lifetime
    JerryScript uses heap-based reference counting (not a stack like Lua/Duktape).
    To provide deterministic cleanup similar to stack-based engines:

    - **Scope** tracks values created within it and releases one reference to
      each when the scope ends. Values that were copied elsewhere survive;
      values that remained local are freed.
    - **Value** holds its own reference via jerry_value_copy/jerry_value_free.
      Values can safely outlive their creating Scope if copied.

    ### Integer Limitations
    JerryScript only guarantees 32-bit integer precision. Values outside the
    int32 range (approximately +/-2 billion) are converted to strings when passed
    to JavaScript to avoid wraparound bugs. When reading values back, integers
    that fit in int64 are returned as integers; others remain as doubles.

    ### DOM to JS Conversion Strategy
    - **Objects**: Use lazy Proxy wrappers to avoid infinite recursion from
      circular references (e.g., Handlebars symbol contexts that reference
      parent symbols). Properties are converted on-demand when accessed.
    - **Arrays**: Converted eagerly (snapshot semantics) because they rarely
      contain circular references. This means JS mutations don't affect the
      original C++ array and vice versa.
    - **Functions**: Wrapped bidirectionally so JS can call C++ functions and
      C++ can invoke JS functions through dom::Function.

    ### Thread Safety
    Each Context has its own mutex serializing operations on that context.
    Different Contexts can execute in parallel on different threads. Values
    hold a shared_ptr to their Context, keeping it alive and using the mutex
    for all JerryScript operations.

    ### Port Functions
    JerryScript requires "port functions" for platform-specific operations.
    We use the default jerry-port library (JERRY_PORT=ON) for most functions
    (logging, time, fatal errors, etc.), but provide custom implementations
    of the context management functions:
    - `jerry_port_context_alloc`: Allocates context + heap memory
    - `jerry_port_context_free`: Frees context memory
    - `jerry_port_context_get`: Returns current thread's active context

    The default jerry-port context functions use a static global pointer,
    limiting all threads to a single shared context. When building with
    JERRY_EXTERNAL_CONTEXT=ON, these functions are excluded from jerry-port
    (see third-party/patches/jerryscript/CMakeLists.txt), and mrdocs provides
    TLS-based implementations that allow each thread to have its own active
    context, enabling parallel JavaScript execution.
*/
namespace js {

class Context;
class Scope;

/** Lightweight handle to a JavaScript array.
*/
class Array;
/** Boolean wrapper for JavaScript values.
*/
class Boolean;
/** Object wrapper for JavaScript values.
*/
class Object;
/** String wrapper for JavaScript values.
*/
class String;
/** Generic JavaScript value wrapper.
*/
class Value;

//------------------------------------------------

/** Types of values.
*/
enum class Type
{
    /// The value is undefined
    undefined = 1,
    /// The value is null
    null,
    /// The value is a boolean
    boolean,
    /// The value is a number
    number,
    /// The value is a string
    string,
    /// The value is an object
    object,
    /// The value is a function
    function,
    /// The value is an array
    array
};

//------------------------------------------------

/** An isolated JavaScript interpreter instance.

    Each Context owns an independent JerryScript interpreter with its own
    512KB heap. Multiple Contexts can exist simultaneously, allowing parallel
    JavaScript execution across threads (each thread should use its own Context).

    To execute scripts or create values, construct a @ref Scope from the Context.
    The Scope activates the Context on the current thread and provides methods
    for script evaluation and value creation.

    Contexts can be copied (via copy constructor); copies share the same
    underlying interpreter and heap. This is useful for passing context
    references without transferring ownership.

    @see Scope
*/
class MRDOCS_DECL
    Context
{
public:
    /** Shared runtime data for a JavaScript context. */
    struct Impl;

private:
    std::shared_ptr<Impl> impl_;

    friend class Value;
    friend class Scope;

public:
    /** Destructor.

        Releases this reference to the interpreter. The underlying JerryScript
        context is destroyed when the last Context (or Value) referencing it
        is destroyed.
    */
    ~Context();

    /** Constructor.

        Creates a new JavaScript interpreter with its own 512KB heap.
        The interpreter is initialized but inactive until a Scope is created.
    */
    Context();

    /** Copy constructor.

        Creates a new Context that shares the same underlying interpreter.
        Both Contexts reference the same heap and global object. This is
        useful for passing Context references without transferring ownership.

        @note Operations on the shared interpreter are serialized by a mutex,
        so only one thread can execute at a time per interpreter.
    */
    Context(Context const&) noexcept;

    /** Copy assignment (deleted).

        Copy assignment is deleted to prevent accidental interpreter sharing.
        Use the copy constructor explicitly if sharing is intended.
    */
    Context& operator=(Context const&) = delete;
};

//------------------------------------------------

/** A JavaScript scope for value lifetime management.

    Scope serves two purposes:

    1. **Value batch tracking**: Tracks JavaScript values created within
       the scope and releases one reference to each when the scope ends.
       Values that were copied elsewhere (e.g., returned from functions,
       stored in containers) survive because they hold their own references.
       Values that remained local to the scope are freed.

    2. **Thread safety**: Each Scope operation briefly locks the Context's
       mutex and activates the context (sets TLS). This allows multiple
       threads to share a Context while serializing access to the interpreter.
       Values obtained from a Scope can be used from other threads; they
       will acquire the lock as needed.

    This provides deterministic cleanup similar to stack-based engines
    (Lua, Duktape) while working with JerryScript's reference-counted heap.

    @note Multiple Scopes can exist for the same Context (even in different
    threads), but operations are serialized by the Context's mutex.
*/
class Scope
{
    std::shared_ptr<Context::Impl> impl_;

    // Values to release on destruction
    std::vector<std::uint32_t> tracked_;

public:
    /** Constructor.

        Records the context for this scope. The context's mutex is NOT held
        for the lifetime of the Scope; instead, each operation locks briefly.

        @param ctx The context to use.
    */
    MRDOCS_DECL
    Scope(Context const& ctx) noexcept;

    /** Destructor.

        Releases one reference to each value created within this scope.
        Values whose reference count drops to zero are freed; values
        that were copied elsewhere survive.
    */
    MRDOCS_DECL
    ~Scope();

    /** Push an integer to the stack

        @param value The integer value to push.
        @return A Value representing the integer.
    */
    MRDOCS_DECL
    Value
    pushInteger(std::int64_t value);

    /** Push a double to the stack

        @param value The double value to push.
        @return A Value representing the double.
    */
    MRDOCS_DECL
    Value
    pushDouble(double value);

    /** Push a boolean to the stack

        @param value The boolean value to push.
        @return A Value representing the boolean.
    */
    MRDOCS_DECL
    Value
    pushBoolean(bool value);

    /** Push a string to the stack

        @param value The string value to push.
        The string is copied to the internal
        heap.
        @return A Value representing the string.
    */
    MRDOCS_DECL
    Value
    pushString(std::string_view value);

    /** Push a new object to the stack
    */
    MRDOCS_DECL
    Value
    pushObject();

    /** Push a new array to the stack
    */
    MRDOCS_DECL
    Value
    pushArray();

    /** Compile and run a script.

        This function compiles and executes
        the specified JavaScript code. The script
        can be used to execute commands or define
        global variables in the parent context.

        ES module import/export is not enabled; scripts must be self-contained
        or rely on globals.

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

        Wraps arbitrary script text in an IIFE that calls `eval` when invoked,
        returning the last expression result. Function declarations are
        rejected to avoid silent re-declarations. Side effects in the script
        run at invocation time.

        @param jsCode The JavaScript code to compile.
        @return A function object that can be called.
        The function object has zero arguments.
    */
    MRDOCS_DECL
    Expected<Value>
    compile_script(std::string_view jsCode);

    /** Compile a script and push results to stack.

        Coerces provided source into a callable function. First parenthesizes
        the source to force expression parsing; if that fails, executes the
        script and returns the first declared function name. Ambiguous sources
        may run side effects twice (expression attempt + fallback) matching
        existing behavior.

        @param jsCode The JavaScript code to compile.
        @return A function object that can be called.
        The function object has the number of arguments
        defined in the code. If the code does not define
        a function, an error is returned.

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

        @param name The name of the global variable.
        @param value The value to set.
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
    MRDOCS_DECL ~Value();

    /** Constructor

        Construct a value that is not associated
        with a @ref Scope.

        The value is undefined.

    */
    MRDOCS_DECL Value() noexcept;

    /** Constructor

        Duplicates the underlying engine handle held by `value` and shares the
        same runtime state.
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
    MRDOCS_DECL
    void
    set(
        std::string_view key,
        Value const& value) const;

    /** Set or replace the value for a given key.

        @param key The key to set.
        @param value The value to set.
    */
    MRDOCS_DECL
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

/** Register a JavaScript helper function

    This function registers a JavaScript function
    as a helper function that can be called from
    Handlebars templates.

    The helper source is resolved in the following order:

    1. **Parenthesized eval** - wraps the script in parentheses and evaluates.
       Handles function declarations without side effects.
       Example: `"function add(a, b) { return a + b; }"`

    2. **Direct eval** - evaluates the script as-is.
       Handles IIFEs and expressions that return functions.
       Example: `"(function(){ return function(x){ return x*2; }; })()"`

    3. **Global lookup** - looks up the helper name on the global object.
       Handles scripts that define globals before returning.
       Example: `"var helper = function(x){ return x; }; helper;"`

    The resolved function is stored on the shared `MrDocsHelpers` global object
    and registered with Handlebars. When invoked, positional arguments are passed
    to the JavaScript function (the Handlebars options object is stripped to avoid
    expensive recursive conversion of symbol contexts).

    @param hbs The Handlebars instance to register the helper into
    @param name The name of the helper function
    @param ctx The JavaScript context to use
    @param script The JavaScript code that defines the helper function
    @return Success, or an error if the script could not be resolved to a function
*/
[[nodiscard]] MRDOCS_DECL
Expected<void, Error>
registerHelper(
    mrdocs::Handlebars& hbs,
    std::string_view name,
    Context& ctx,
    std::string_view script);

} // js
} // mrdocs


#endif
