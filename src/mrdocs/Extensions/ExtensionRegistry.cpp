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

#include <mrdocs/Extensions/ExtensionRegistry.hpp>
#include "AddonDiscovery.hpp"
#include "CorpusDom.hpp"
#include "JsBinding.hpp"
#include "LoadedExtensions.hpp"
#include "LuaBinding.hpp"
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <string>
#include <utility>
#include <vector>

namespace mrdocs {

ExtensionRegistry::ExtensionRegistry() = default;
ExtensionRegistry::~ExtensionRegistry() = default;
ExtensionRegistry::ExtensionRegistry(ExtensionRegistry&&) noexcept = default;
ExtensionRegistry& ExtensionRegistry::operator=(ExtensionRegistry&&) noexcept = default;

namespace {

Expected<LoadedExtensions>
loadExtensionsFromScript(std::string const& scriptPath)
{
    if (scriptPath.ends_with(".lua"))
    {
        return loadLuaExtensions(scriptPath);
    }
    if (scriptPath.ends_with(".js"))
    {
        return loadJsExtensions(scriptPath);
    }
    // collectExtensionScripts only emits .lua / .js paths, so reaching
    // here would mean an internal mismatch.
    return Unexpected(formatError(
        "extension '{}': unsupported file extension", scriptPath));
}

} // (anon)

Expected<ExtensionRegistry>
ExtensionRegistry::load(Config const& config)
{
    MRDOCS_TRY(std::vector<std::string> scripts,
        collectExtensionScripts(config));

    ExtensionRegistry registry;
    for (std::string const& path : scripts)
    {
        MRDOCS_TRY(LoadedExtensions loaded, loadExtensionsFromScript(path));
        registry.scripts_.push_back(
            Script{std::move(loaded.vm), std::move(loaded.transforms)});
        for (std::unique_ptr<Generator>& g : loaded.generators)
        {
            registry.generators_.push_back(std::move(g));
        }
    }
    return registry;
}

Expected<void>
ExtensionRegistry::applyTransforms(Corpus& corpus, Config const& config) const
{
    // Invoke each registered transform once, in registration order across
    // scripts, stopping at the first failure. The corpus DOM is O(symbols),
    // so build it once per script and reuse it; only `ctx.params` differs
    // per transform. A transform sees `ctx.corpus` (per-symbol proxies plus
    // `get(id)` / `lookup(name)`), `ctx.config`, and its own `ctx.params`.
    // Writes (`ctx.corpus.symbols[i].name = "..."`) mutate the live Symbol
    // through the DOM's set path.
    for (Script const& script : scripts_)
    {
        if (script.transforms.empty())
        {
            continue;
        }
        dom::Value const corpusDom = buildCorpusDom(corpus);
        for (auto const& [id, transform] : script.transforms)
        {
            dom::Value ctx = buildTransformContext(corpusDom, config, id);
            Expected<dom::Value> invoked = transform.try_invoke(ctx);
            if (!invoked.has_value())
            {
                return Unexpected(formatError(
                    "extension transform '{}': {}",
                    id, invoked.error().message()));
            }
        }
    }
    return {};
}

std::vector<Generator const*>
ExtensionRegistry::generators() const
{
    std::vector<Generator const*> result;
    result.reserve(generators_.size());
    for (std::unique_ptr<Generator> const& g : generators_)
    {
        result.push_back(g.get());
    }
    return result;
}

Generator const*
ExtensionRegistry::findGenerator(std::string_view id) const noexcept
{
    for (std::unique_ptr<Generator> const& g : generators_)
    {
        if (g->id() == id)
        {
            return g.get();
        }
    }
    return nullptr;
}

} // mrdocs
