//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_HANDLEBARS_HELPERS_BUILTIN_HPP
#define MRDOCS_API_HANDLEBARS_HELPERS_BUILTIN_HPP

// Built-in helpers (if, unless, with, each, lookup, log, ...).

#include <mrdocs/Dom.hpp>
#include <mrdocs/Handlebars/Engine.hpp>
#include <mrdocs/Handlebars/Platform.hpp>

namespace mrdocs {
namespace handlebars {

/** Handlebars helper registry for templates.

    Everything under `helpers` mirrors the runtime helper registry shipped with
    the bundled Handlebars engine, so templates can call the same helpers in
    both Node-based dev mode and the embedded C++ rendering pipeline. Each
    category (built-in, logical, math, string, container, constructor, type)
    has its own header under `Handlebars/Helpers/`.
*/
namespace helpers {

/** Register all the built-in helpers into a Handlebars instance

    Individual built-in helpers can also be registered with the
    public `*_fn` functions in this namespace.

    This allows the user to override only some of the built-in
    helpers. In particular, this is important for mandatory
    helpers, such as `blockHelperMissing` and `helperMissing`.

    @see https://github.com/handlebars-lang/handlebars.js/tree/master/lib/handlebars/helpers
    @see https://handlebarsjs.com/guide/builtin-helpers.html

    @param hbs The Handlebars instance to register the helpers into
*/
MRDOCS_HANDLEBARS_DECL
void
registerBuiltinHelpers(Handlebars& hbs);

/** Register all the Antora helpers into a Handlebars instance

    This function registers all the helpers that are part of the
    default Antora UI.

    Individual Antora helpers can also be registered with the
    public `*_fn` functions in this namespace.

    Since the Antora helpers are not mandatory and include
    many functions not applicable to all applications,
    this allows the user to register only some of the Antora
    helpers.

    @see https://gitlab.com/antora/antora-ui-default/-/tree/master/src/helpers

    @param hbs The Handlebars instance to register the helpers into
*/
MRDOCS_HANDLEBARS_DECL
void
registerAntoraHelpers(Handlebars& hbs);

/** The `helperMissing` helper: reports a missing inline helper.

    @param arguments The helper arguments; the trailing options object names
    the missing helper.
*/
MRDOCS_HANDLEBARS_DECL
dom::Expected<dom::Value>
helper_missing_fn(dom::Array const& arguments);

/** The `blockHelperMissing` helper: renders a block for a missing block helper.

    @param context The block context value.
    @param options The Handlebars options object.
*/
MRDOCS_HANDLEBARS_DECL
dom::Expected<void>
block_helper_missing_fn(dom::Value const& context, dom::Value options);

} // namespace helpers
} // namespace handlebars
} // namespace mrdocs

#endif
