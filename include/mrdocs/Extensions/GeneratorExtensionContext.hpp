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

#ifndef MRDOCS_API_EXTENSIONS_GENERATOREXTENSIONCONTEXT_HPP
#define MRDOCS_API_EXTENSIONS_GENERATOREXTENSIONCONTEXT_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <string>

namespace mrdocs {

/** What a generator receives when a plugin registers it as a callable.

    @ref PluginContext::registerGenerator with an id and a callable passes the
    callable one of these. A generator script receives the same thing as
    `ctx`: the type is described, so each field is a key of `ctx` in Lua and in
    JavaScript, and a field added here reaches both. A script also gets
    `ctx.output`, whose `write` and `append` functions write files under
    @ref GeneratorExtensionContext::outputPath, `ctx.stringify`, and
    `ctx.report`, whose `warn`, `error` and `info` functions report through
    @ref GeneratorExtensionContext::report. A function cannot be a field of a
    described type, so those three keys are the ones the script binding adds.
    The pointers are never null and are valid for the duration of the call.
*/
struct MRDOCS_VISIBLE GeneratorExtensionContext
{
    /** The symbols to emit.
    */
    Corpus const* corpus = nullptr;

    /** The configuration that drove the build.
    */
    Config const* config = nullptr;

    /** The absolute path the generator writes to.

        This is the location @ref getGeneratorOutputPath resolves for the
        generator's id. The generator may treat it as a file or as a
        directory.
    */
    std::string outputPath;

    /** The generator's own options.

        This is the `generator-options.<id>` block of the configuration for
        the id the generator was registered under, or an empty object when the
        configuration has none.
    */
    dom::Object params;

    /** The reporter of the run.

        It prints and counts what the generator reports with the rest of
        the run, as in `ctx.report->warn("{} has no brief", name)`.
    */
    report::Reporter* report = nullptr;
};

MRDOCS_DESCRIBE_STRUCT(
    GeneratorExtensionContext,
    (),
    (corpus, config, outputPath, params))

/** Return the context a generator's callable or script receives.

    Every field is filled from the arguments, with
    @ref GeneratorExtensionContext::outputPath read by
    @ref getGeneratorOutputPath and @ref GeneratorExtensionContext::params by
    @ref getGeneratorParams. It is the one place that builds a
    @ref GeneratorExtensionContext, so the wrappers of callable and script
    generators hand over the same thing.

    @return The context, which points at the arguments.

    @param generator The generator the context is for.
    @param corpus The symbols to emit.
    @param config The configuration that drove the build.
    @param reporter The reporter of the run.
*/
MRDOCS_DECL
GeneratorExtensionContext
makeGeneratorExtensionContext(
    Generator const& generator,
    Corpus const& corpus,
    Config const& config,
    report::Reporter& reporter);

} // mrdocs

#endif // MRDOCS_API_EXTENSIONS_GENERATOREXTENSIONCONTEXT_HPP
