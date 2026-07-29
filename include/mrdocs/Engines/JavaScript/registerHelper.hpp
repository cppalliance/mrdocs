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

#ifndef MRDOCS_API_ENGINES_JAVASCRIPT_REGISTERHELPER_HPP
#define MRDOCS_API_ENGINES_JAVASCRIPT_REGISTERHELPER_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Engines/JavaScript/Context.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <string_view>

namespace mrdocs { namespace handlebars { class Handlebars; } }

namespace mrdocs {
namespace js {

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
    handlebars::Handlebars& hbs,
    std::string_view name,
    Context& ctx,
    std::string_view script);

} // js
} // mrdocs

#endif // MRDOCS_API_ENGINES_JAVASCRIPT_REGISTERHELPER_HPP
