//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "RunExtensions.hpp"

#include "AddonDiscovery.hpp"
#include "JsBinding.hpp"
#include "LuaBinding.hpp"

#include <lib/CorpusImpl.hpp>

#include <mrdocs/Support/Error.hpp>

#include <string>
#include <vector>

namespace mrdocs {
namespace {

Expected<void>
runOneExtension(CorpusImpl& corpus, std::string const& scriptPath)
{
    if (scriptPath.ends_with(".lua"))
    {
        return runOneLuaExtension(corpus, scriptPath);
    }
    if (scriptPath.ends_with(".js"))
    {
        return runOneJsExtension(corpus, scriptPath);
    }
    // collectExtensionScripts only emits .lua / .js paths, so reaching
    // here would mean an internal mismatch.
    return Unexpected(formatError(
        "extension '{}': unsupported file extension", scriptPath));
}

} // (anon)

Expected<void>
runExtensions(CorpusImpl& corpus)
{
    MRDOCS_TRY(std::vector<std::string> scripts,
        collectExtensionScripts(corpus.config));
    for (std::string const& path : scripts)
    {
        MRDOCS_TRY(runOneExtension(corpus, path));
    }
    return {};
}

} // mrdocs
