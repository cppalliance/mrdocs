//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_EXTENSIONS_TRANSFORMEXTENSIONCONTEXT_HPP
#define MRDOCS_API_EXTENSIONS_TRANSFORMEXTENSIONCONTEXT_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Transform.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>

namespace mrdocs {

/** What a transform receives when a plugin registers it as a callable.

    @ref PluginContext::registerTransform with an id and a callable passes the
    callable one of these. A transform script receives the same thing as
    `ctx`: the type is described, so each field is a key of `ctx` in Lua and in
    JavaScript, and a field added here reaches both. The exception is
    @ref report. A script gets it as `ctx.report`, an object with `warn`,
    `error` and `info` functions that the script binding adds by hand,
    because a function cannot be a field of a described type. The context
    carries what @ref Transform::apply receives as arguments, the
    transform's own options (which a transform class reads with
    @ref getTransformParams) and the reporter of the session that runs the
    transform. The pointers are never null and are valid for the duration of
    the call.
*/
struct MRDOCS_VISIBLE TransformExtensionContext
{
    /** The corpus to read and change.
    */
    Corpus* corpus = nullptr;

    /** The configuration that drove the build.
    */
    Config const* config = nullptr;

    /** The transform's own options.

        This is the `transform-options.<id>` block of the configuration for
        the id the transform was registered under, or an empty object when
        the configuration has none.
    */
    dom::Object params;

    /** The reporter of the run.

        It prints and counts what the transform reports with the rest of
        the run, as in `ctx.report->warn("{} has no brief", name)`.
    */
    report::Reporter* report = nullptr;
};

MRDOCS_DESCRIBE_STRUCT(
    TransformExtensionContext,
    (),
    (corpus, config, params))

/** Return the options block of a transform.

    This is the `transform-options.<id>` block of the configuration for the
    transform's @ref Transform::id, or an empty object when the configuration
    has none. It is the one place that reads `Config::transformOptions`: a
    transform class calls it from @ref Transform::apply, and the wrappers of
    callable and script transforms fill
    @ref TransformExtensionContext::params with it.

    @return The block, which shares its storage with the configuration.

    @param transform The transform whose block is looked up.
    @param config The configuration to read the block from.
*/
MRDOCS_DECL
dom::Object
getTransformParams(Transform const& transform, Config const& config);

/** Return the context a transform's callable or script receives.

    Every field is filled from the arguments, with
    @ref TransformExtensionContext::params read by @ref getTransformParams. It
    is the one place that builds a @ref TransformExtensionContext, so the
    wrappers of callable and script transforms hand over the same thing.

    @return The context, which points at the arguments.

    @param transform The transform the context is for.
    @param corpus The corpus the transform changes.
    @param config The configuration that drove the build.
    @param reporter The reporter of the run.
*/
MRDOCS_DECL
TransformExtensionContext
makeTransformExtensionContext(
    Transform const& transform,
    Corpus& corpus,
    Config const& config,
    report::Reporter& reporter);

} // mrdocs

#endif // MRDOCS_API_EXTENSIONS_TRANSFORMEXTENSIONCONTEXT_HPP
