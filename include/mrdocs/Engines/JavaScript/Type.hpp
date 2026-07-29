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

#ifndef MRDOCS_API_ENGINES_JAVASCRIPT_TYPE_HPP
#define MRDOCS_API_ENGINES_JAVASCRIPT_TYPE_HPP

namespace mrdocs {

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
    (see utils/bootstrap/patches/jerryscript/CMakeLists.txt), and mrdocs provides
    TLS-based implementations that allow each thread to have its own active
    context, enabling parallel JavaScript execution.
*/
namespace js {

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

} // js
} // mrdocs

#endif // MRDOCS_API_ENGINES_JAVASCRIPT_TYPE_HPP
