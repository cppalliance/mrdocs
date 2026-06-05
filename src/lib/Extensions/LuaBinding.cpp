//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "LuaBinding.hpp"
#include "CorpusDom.hpp"

#include <lib/CorpusImpl.hpp>

#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Lua.hpp>
#include <mrdocs/Support/Path.hpp>

#include <utility>

namespace mrdocs {

Expected<void>
runOneLuaExtension(CorpusImpl& corpus, std::string const& scriptPath)
{
    lua::Context ctx;
    lua::Scope scope(ctx);

    // The corpus argument is a small navigable object: an array of
    // per-symbol proxies plus `get(id)` / `lookup(name)` functions.
    // Reads run through reflection on the live C++ Symbol; writes
    // (`sym.name = "..."`) mutate that Symbol directly through the
    // `__newindex` metamethod's `dom::Object::set` path.
    dom::Value corpusValue = buildCorpusDom(corpus);

    MRDOCS_TRY(std::string script, files::getFileText(scriptPath));
    MRDOCS_TRY(lua::Function chunk, scope.loadChunk(script, scriptPath));

    Expected<lua::Value> chunkResult = chunk.call();
    if (!chunkResult)
    {
        return Unexpected(chunkResult.error());
    }

    // Resolve `transform_corpus`. Prefer the chunk's return value
    // (the `return function(...) ... end` idiom); fall back to a
    // same-named global (the `function name(...)` idiom). If neither
    // yields a function, the extension has nothing to do.
    auto callTransform =
        [&](lua::Function&& fn) -> Expected<void>
        {
            Expected<lua::Value> result = fn.call(corpusValue);
            if (!result)
            {
                return Unexpected(formatError(
                    "extension '{}': {}",
                    scriptPath, result.error().message()));
            }
            return {};
        };

    if (chunkResult->isFunction())
    {
        return callTransform(lua::Function(std::move(*chunkResult)));
    }

    Expected<lua::Value> global = scope.getGlobal("transform_corpus");
    if (!global || !global->isFunction())
    {
        return {};
    }
    return callTransform(lua::Function(std::move(*global)));
}

} // mrdocs
