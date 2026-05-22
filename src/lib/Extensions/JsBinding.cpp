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
#include "SetMember.hpp"

#include <lib/CorpusImpl.hpp>

#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Support/JavaScript.hpp>
#include <mrdocs/Support/Path.hpp>

#include <string>
#include <utility>

namespace mrdocs {
namespace {

// The JS wrapper already knows how to expose a `dom::Function` as a
// callable JS value (`setGlobal` -> `toJsValue` -> `makeFunctionProxy`),
// so no escape hatch is needed: we just build the `mrdocs` API as a
// `dom::Object` containing `dom::Function` entries and set it as a
// global.
dom::Object
buildJsMrDocsApi(ExtensionState& state)
{
    // `ExtensionState` is a stack local in `runOneJsExtension`; capturing
    // by raw pointer here is safe because the API object, the script
    // execution, and the state all live within the same call frame.
    ExtensionState* statePtr = &state;
    dom::Object api;
    api.set("set", dom::Value(dom::makeVariadicInvocable(
        [statePtr](dom::Array const& args) -> Expected<dom::Value, Error>
        {
            if (args.size() < 3)
            {
                return Unexpected(Error(
                    "mrdocs.set: expected (symbol_id, field, value)"));
            }
            return setMemberImpl(
                *statePtr, args.get(0), args.get(1), args.get(2));
        })));
    return api;
}

} // (anon)

Expected<void>
runOneJsExtension(CorpusImpl& corpus, std::string const& scriptPath)
{
    js::Context ctx;
    ExtensionState state{ &corpus, {} };

    DomCorpus domCorpus(corpus);
    dom::Value corpusValue = buildCorpusDom(corpus, domCorpus, state);

    js::Scope scope(ctx);

    // Expose `mrdocs.set(...)` (and any future setters) as a global
    // object whose entries are `dom::Function`s; the JS wrapper turns
    // these into callable proxies via `makeFunctionProxy`.
    scope.setGlobal("mrdocs", dom::Value(buildJsMrDocsApi(state)));

    // Run the script (defines globals, including `transform_corpus`).
    MRDOCS_TRY(std::string script, files::getFileText(scriptPath));
    if (Expected<void> exp = scope.script(script); !exp)
    {
        return Unexpected(formatError(
            "extension '{}': {}",
            scriptPath, exp.error().message()));
    }

    Expected<js::Value> fn = scope.getGlobal("transform_corpus");
    if (!fn || !fn->isFunction())
    {
        return {};
    }

    Expected<js::Value> result = fn->call(corpusValue);
    if (!result)
    {
        return Unexpected(formatError(
            "extension '{}': {}",
            scriptPath, result.error().message()));
    }
    return {};
}

} // mrdocs
