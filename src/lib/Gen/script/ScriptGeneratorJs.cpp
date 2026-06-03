//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ScriptRunner.hpp"
#include "OutputSink.hpp"

#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/JavaScript.hpp>
#include <mrdocs/Support/Path.hpp>

#include <string>
#include <utility>

namespace mrdocs::script {

namespace {

// Build the `output` object passed as the second argument to `generate`.
// The JavaScript wrapper exposes a `dom::Function` as a callable proxy,
// so unlike the Lua side this needs no escape hatch: `write` is a variadic
// invocable that routes to the sink. The sink outlives the call (it is a
// local in `runJsGenerator`), so capturing it by pointer is safe.
dom::Object
buildJsOutputApi(OutputSink& sink)
{
    OutputSink* sinkPtr = &sink;
    dom::Object api;
    api.set("write", dom::Value(dom::makeVariadicInvocable(
        [sinkPtr](dom::Array const& args) -> Expected<dom::Value, Error>
        {
            if (args.size() < 2)
            {
                return Unexpected(Error(
                    "output.write: expected (path, contents)"));
            }
            dom::Value const path = args.get(0);
            dom::Value const body = args.get(1);
            if (!path.isString() || !body.isString())
            {
                return Unexpected(Error(
                    "output.write: path and contents must be strings"));
            }
            Expected<void> result = sinkPtr->write(
                path.getString().get(), body.getString().get());
            if (!result)
            {
                return Unexpected(result.error());
            }
            return dom::Value();
        })));
    return api;
}

} // (anon)

Expected<void>
runJsGenerator(
    dom::Value const& corpus,
    std::string const& scriptPath,
    OutputSink& sink,
    dom::Value const& config,
    dom::Value const& params)
{
    js::Context ctx;
    js::Scope scope(ctx);

    MRDOCS_TRY(std::string script, files::getFileText(scriptPath));
    if (Expected<void> exp = scope.script(script); !exp)
    {
        return Unexpected(formatError(
            "generator '{}': {}",
            scriptPath, exp.error().message()));
    }

    // Unlike an extension, a generator must define `generate`.
    Expected<js::Value> fn = scope.getGlobal("generate");
    if (!fn || !fn->isFunction())
    {
        return Unexpected(formatError(
            "generator '{}': script must define a 'generate' function",
            scriptPath));
    }

    Expected<js::Value> result =
        fn->call(corpus, buildJsOutputApi(sink), config, params);
    if (!result)
    {
        return Unexpected(formatError(
            "generator '{}': {}",
            scriptPath, result.error().message()));
    }
    return {};
}

} // mrdocs::script
