//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_HANDLEBARS_OPTIONS_HPP
#define MRDOCS_API_HANDLEBARS_OPTIONS_HPP

// HandlebarsOptions: the per-render options passed to the engine.

#include <mrdocs/Dom.hpp>
#include <mrdocs/Handlebars/Platform.hpp>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace mrdocs {
namespace handlebars {

/** Options for handlebars

    This struct is analogous to the Handlebars.compile options.

    @see https://handlebarsjs.com/api-reference/compilation.html

*/
struct HandlebarsOptions
{
    /** Escape HTML entities or entities defined by the escape function
    */
    bool noEscape = false;

    /** Function to escape entities

        It's initialized with a reference to the HTMLEscape function overload
        that takes an OutputRef and a string_view. This function can be
        replaced with a custom function that escapes entities in a different
        way.

    */
    std::function<void(OutputRef&, std::string_view)> escapeFunction =
        static_cast<void(*)(OutputRef&, std::string_view)>(HTMLEscape);

    /** Templates will throw rather than ignore missing fields

        Run in strict mode. In this mode, templates will throw rather than
        silently ignore missing fields.

    */
    bool strict = false;

    /** Removes object existence checks when traversing paths

        This is a subset of strict mode that generates optimized
        templates when the data inputs are known to be safe.
    */
    bool assumeObjects = false;

    /** Disable the auto-indent feature

        By default, an indented partial-call causes the output of the
        whole partial being indented by the same amount.

        For instance, the partial:

        @code{.handlebars}
        A
        B
        C
        @endcode

        and the template:

        @code{.handlebars}
        <div>
            {{> partial}}
        </div>
        @endcode

        will render as:

        @code{.html}
        <div>
            A
            B
            C
        </div>
        @endcode

        because the partial is indented by the same amount as the
        partial call.

        This can be disabled by setting this option to true. In this case,
        the partial above would be rendered as:

        @code{.html}
        <div>
            A
        B
        C
        </div>


    */
    bool preventIndent = false;

    /** Disables standalone tag removal when set to true

        By default, Handlebars removes whitespace around block and partial
        expressions. For instance, the partial:

        @code{.handlebars}
        A
        @endcode

        and the template:

        @code{.handlebars}
        <div>
            {{> partial}}
        </div>
        @endcode

        will render as:

        @code{.html}
        <div>
            A</div>
        @endcode

        because the whitespace up to the newline after the partial is removed.

        A double newline is required to ensure that the whitespace is not
        removed. For instance, the template:

        @code{.handlebars}
        <div>
            {{> partial}}

        </div>
        @endcode

        will render as:

        @code{.html}
        <div>
            A
        </div>
        @endcode

        This can be disabled by setting this option to true.

        When set, blocks and partials that are on their own line will not
        remove the whitespace on that line.
    */
    bool ignoreStandalone = false;

    /** Disables implicit context for partials

        When enabled, partials that are not passed a context value will
        execute against an empty object.
    */
    bool explicitPartialContext = false;

    /** Enable recursive field lookup

        When enabled, fields will be looked up recursively in objects
        and arrays.

        This mode should be used to enable complete compatibility
        with Mustache templates.
    */
    bool compat = false;

    /** Enable tracking of ids

        When enabled, the ids of the expressions are tracked and
        passed to the helpers.

        Helpers often use this information to update the context
        path to the current expression, which can later be used to
        look up the value of the expression with ".." segments.
    */
    bool trackIds = false;

    /** Custom private data object

        This variable can be used to pass in an object to define custom
        private variables.
    */
    dom::Value data = nullptr;
};

} // namespace handlebars
} // namespace mrdocs

#endif
