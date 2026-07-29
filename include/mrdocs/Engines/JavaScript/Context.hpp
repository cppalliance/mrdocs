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

#ifndef MRDOCS_API_ENGINES_JAVASCRIPT_CONTEXT_HPP
#define MRDOCS_API_ENGINES_JAVASCRIPT_CONTEXT_HPP

#include <mrdocs/Platform.hpp>
#include <memory>

namespace mrdocs {
namespace js {

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

} // js
} // mrdocs

#endif // MRDOCS_API_ENGINES_JAVASCRIPT_CONTEXT_HPP
