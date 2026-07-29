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

#ifndef MRDOCS_API_ENGINES_JAVASCRIPT_SCOPE_HPP
#define MRDOCS_API_ENGINES_JAVASCRIPT_SCOPE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Engines/JavaScript/Context.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <cstdint>
#include <string_view>
#include <vector>

namespace mrdocs { namespace js { class Value; } }

namespace mrdocs {
namespace js {

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

} // js
} // mrdocs

#endif // MRDOCS_API_ENGINES_JAVASCRIPT_SCOPE_HPP
