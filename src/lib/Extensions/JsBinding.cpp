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

namespace mrdocs {

Expected<void>
runOneJsExtension(CorpusImpl& corpus, std::string const& scriptPath)
{
    js::Context ctx;
    js::Scope scope(ctx);

    js::registerStdGlobals(scope);

    // The corpus argument is a small navigable object: an array of
    // per-symbol proxies plus `get(id)` / `lookup(name)` functions.
    // Everything else a script does runs through that proxy: direct
    // reads via reflection, direct writes that mutate the live Symbol.
    dom::Value corpusValue = buildCorpusDom(corpus);

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
