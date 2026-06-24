//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "JsBinding.hpp"
#include "CorpusDom.hpp"

#include <lib/CorpusImpl.hpp>
#include <lib/Js/StdGlobals.hpp>

#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/JavaScript.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/Report.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mrdocs {

namespace {

// Install the `mrdocs` global carrying the `register_transform` and
// `register_generator` entry points before the script runs. A JavaScript
// function bridges to a `dom::Function`: transforms are captured in
// `transforms` (invoked once the script has run), generators are handed to
// the corpus, which keeps them runnable past this VM's lifetime.
void
registerJsExtensionApi(
    js::Scope& scope,
    CorpusImpl& corpus,
    std::vector<std::pair<std::string, dom::Value>>& transforms,
    std::size_t& generators)
{
    dom::Object api;
    api.set(
        "register_transform",
        dom::Value(dom::makeVariadicInvocable(
            [&transforms](dom::Array const& args)
                -> Expected<dom::Value, Error>
            {
                Expected<dom::Value, Error> result;
                if (args.size() < 2 ||
                    !args.get(0).isString() ||
                    !args.get(1).isFunction())
                {
                    result = Unexpected(Error(
                        "mrdocs.register_transform: expected (string id, "
                        "function)"));
                }
                else
                {
                    transforms.emplace_back(
                        std::string(args.get(0).getString().get()),
                        args.get(1));
                }
                return result;
            })));

    api.set(
        "register_generator",
        dom::Value(dom::makeVariadicInvocable(
            [&corpus, &generators](dom::Array const& args)
                -> Expected<dom::Value, Error>
            {
                Expected<dom::Value, Error> result;
                if (args.size() < 2 ||
                    !args.get(0).isString() ||
                    !args.get(1).isFunction())
                {
                    result = Unexpected(Error(
                        "mrdocs.register_generator: expected (string id, "
                        "function)"));
                }
                else
                {
                    corpus.registerScriptGenerator(
                        std::string(args.get(0).getString().get()),
                        args.get(1).getFunction());
                    ++generators;
                }
                return result;
            })));

    scope.setGlobal("mrdocs", dom::Value(std::move(api)));
}

// Invoke one registered transform with the `ctx` object, tagging any
// failure with the script path for context.
Expected<void>
invokeTransform(
    dom::Value const& transform,
    dom::Value const& ctx,
    std::string const& scriptPath)
{
    Expected<dom::Value> invoked = transform.getFunction().try_invoke(ctx);
    Expected<void> result;
    if (!invoked.has_value())
    {
        result = Unexpected(formatError(
            "extension '{}': {}",
            scriptPath, invoked.error().message()));
    }
    return result;
}

} // (anon)

Expected<void>
runOneJsExtension(CorpusImpl& corpus, std::string const& scriptPath)
{
    js::Context ctx;
    js::Scope scope(ctx);

    js::registerStdGlobals(scope);

    std::vector<std::pair<std::string, dom::Value>> transforms;
    // Generators are owned by the corpus, not held here, so this counts
    // them only to tell whether the script registered anything at all.
    std::size_t generators = 0;
    registerJsExtensionApi(scope, corpus, transforms, generators);

    MRDOCS_TRY(std::string script, files::getFileText(scriptPath));

    // Running the script is what calls `mrdocs.register_transform`.
    Expected<void> result = scope.script(script);
    if (!result.has_value())
    {
        result = Unexpected(formatError(
            "extension '{}': {}",
            scriptPath, result.error().message()));
    }
    else if (transforms.empty() && generators == 0)
    {
        // A discovered script that registers nothing is almost always a
        // mistake (a misspelled `mrdocs.register_transform` /
        // `mrdocs.register_generator`,
        // or a guard that skipped it), so flag it rather than silently
        // doing nothing.
        report::warn("extension '{}' registered nothing", scriptPath);
    }

    // Invoke each declared transform with its own `ctx`, in registration
    // order, stopping at the first failure. The corpus DOM is O(symbols),
    // so build it once and reuse it; only `ctx.params` differs per
    // transform.
    dom::Value const corpusDom = buildCorpusDom(corpus);
    for (auto const& [id, transform] : transforms)
    {
        if (result.has_value())
        {
            dom::Value ctx = buildTransformContext(corpusDom, corpus, id);
            result = invokeTransform(transform, ctx, scriptPath);
        }
    }

    // A generator registered above holds only a weak reference to this
    // VM, so hand the corpus a strong reference. That keeps the VM, and
    // therefore the generator, alive past this run until the corpus is
    // destroyed.
    if (generators > 0)
    {
        corpus.keepScriptVmAlive(std::make_shared<js::Context>(ctx));
    }
    return result;
}

} // mrdocs
