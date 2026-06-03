//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_SCRIPT_SCRIPTRUNNER_HPP
#define MRDOCS_LIB_GEN_SCRIPT_SCRIPTRUNNER_HPP

#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Expected.hpp>
#include <string>

namespace mrdocs::script {

class OutputSink;

/** Run a Lua entry script's `generate(corpus, output, config, params)`.

    Build a Lua context, expose the output writer as the `output` global,
    evaluate the script, and call its `generate` function with the
    corpus, the writer, the resolved configuration, and the generator's
    own parameters. A missing `generate` function is an error.

    @param corpus The read-only corpus DOM passed as the first argument.
    @param scriptPath The absolute path to the Lua entry script.
    @param sink The file-writing API exposed to the script.
    @param config The resolved configuration DOM, as templates see it.
    @param params The generator's own parameters, from its manifest.
*/
Expected<void>
runLuaGenerator(
    dom::Value const& corpus,
    std::string const& scriptPath,
    OutputSink& sink,
    dom::Value const& config,
    dom::Value const& params);

/** Run a JS entry script's `generate(corpus, output, config, params)`.

    Build a JavaScript context, evaluate the script, and call its
    `generate` function with the corpus, an `output` object whose
    `write` method routes to the writer, the resolved configuration, and
    the generator's own parameters. A missing `generate` function is an
    error.

    @param corpus The read-only corpus DOM passed as the first argument.
    @param scriptPath The absolute path to the JavaScript entry script.
    @param sink The file-writing API exposed to the script.
    @param config The resolved configuration DOM, as templates see it.
    @param params The generator's own parameters, from its manifest.
*/
Expected<void>
runJsGenerator(
    dom::Value const& corpus,
    std::string const& scriptPath,
    OutputSink& sink,
    dom::Value const& config,
    dom::Value const& params);

} // mrdocs::script

#endif
