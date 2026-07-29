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

#include "JsBinding.hpp"
#include "../Engines/StdGlobals.hpp"
#include "../Gen/script/ScriptGenerator.hpp"
#include <mrdocs/Dom.hpp>
#include <mrdocs/Engines/JavaScript.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Report.hpp>

#include <memory>
#include <string>
#include <utility>

namespace mrdocs {

namespace {

// Install the `mrdocs` global carrying the `register_transform` and
// `register_generator` entry points before the script runs. A JavaScript
// function bridges to a `dom::Function`: transforms are collected into
// `transforms` for the host to invoke later; generators are turned into
// self-contained ScriptGenerators, each holding `vm` (the engine handle)
// so it stays runnable past this call.
void
registerJsExtensionApi(
    js::Scope& scope,
    std::shared_ptr<void> const& vm,
    std::vector<std::pair<std::string, dom::Function>>& transforms,
    std::vector<std::unique_ptr<Generator>>& generators)
{
    dom::Object api;
    api.set(
        "register_transform",
        dom::Value(dom::makeVariadicInvocable(
            [&transforms](dom::Array const& args)
                -> Expected<dom::Value, dom::Error>
            {
                Expected<dom::Value, dom::Error> result;
                if (args.size() < 2 ||
                    !args.get(0).isString() ||
                    !args.get(1).isFunction())
                {
                    result = Unexpected(dom::Error(
                        "mrdocs.register_transform: expected (string id, "
                        "function)"));
                }
                else
                {
                    transforms.emplace_back(
                        std::string(args.get(0).getString().get()),
                        args.get(1).getFunction());
                }
                return result;
            })));

    // Capture the engine handle weakly. This callback is stored as a global
    // inside the engine (`mrdocs.register_generator`), so a strong capture
    // would make the engine own a handle back to itself: a reference cycle
    // that keeps `js::Context` alive forever, so `~Context` (and the
    // `cleanup()` that breaks the DomValueHolder cycle) never runs and the
    // whole interpreter heap leaks. A weak capture breaks that self-reference;
    // the callback only ever runs while the script is executing, when `vm` is
    // still alive, so the `lock()` always succeeds. The resulting strong handle
    // lives on in the ScriptGenerator, which sits outside the engine and so
    // does not close the cycle.
    api.set(
        "register_generator",
        dom::Value(dom::makeVariadicInvocable(
            [&generators, weakVm = std::weak_ptr<void>(vm)](
                dom::Array const& args)
                -> Expected<dom::Value, dom::Error>
            {
                Expected<dom::Value, dom::Error> result;
                if (args.size() < 2 ||
                    !args.get(0).isString() ||
                    !args.get(1).isFunction())
                {
                    result = Unexpected(dom::Error(
                        "mrdocs.register_generator: expected (string id, "
                        "function)"));
                }
                else
                {
                    generators.push_back(
                        std::make_unique<script::ScriptGenerator>(
                            std::string(args.get(0).getString().get()),
                            args.get(1).getFunction(),
                            weakVm.lock()));
                }
                return result;
            })));

    scope.setGlobal("mrdocs", dom::Value(std::move(api)));
}

} // (anon)

Expected<LoadedExtensions>
loadJsExtensions(std::string const& scriptPath)
{
    // The engine is a shared handle from the start, so both the transforms
    // and the generators can keep it alive without a separate owner.
    auto vm = std::make_shared<js::Context>();
    js::Scope scope(*vm);

    js::registerStdGlobals(scope);

    LoadedExtensions loaded;
    loaded.vm = vm;
    registerJsExtensionApi(scope, vm, loaded.transforms, loaded.generators);

    MRDOCS_TRY(std::string script, files::getFileText(scriptPath));

    // Running the script is what calls the `mrdocs.register_*` entry points.
    Expected<void> ran = scope.script(script);
    if (!ran.has_value())
    {
        return Unexpected(formatError(
            "extension '{}': {}", scriptPath, ran.error().message()));
    }

    if (loaded.transforms.empty() && loaded.generators.empty())
    {
        // A discovered script that registers nothing is almost always a
        // mistake (a misspelled `mrdocs.register_transform` /
        // `mrdocs.register_generator`, or a guard that skipped it), so flag
        // it rather than silently doing nothing.
        report::warn("extension '{}' registered nothing", scriptPath);
    }

    return loaded;
}

} // mrdocs
