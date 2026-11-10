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

#include "DataDrivenGenerators.hpp"
#include "AddonPaths.hpp"
#include "HandlebarsGenerator.hpp"
#include <mrdocs/Extensions/GeneratorRegistry.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Generators/GeneratorManifest.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/YAMLParser.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mrdocs::hbs {

namespace {

// Build an `EscapeMap` from the manifest's ordered `escape` rules.
EscapeMap
toEscapeMap(
    std::vector<std::pair<std::string, std::string>> const& rules)
{
    EscapeMap map;
    for (std::pair<std::string, std::string> const& rule : rules)
    {
        map.set(rule.first, rule.second);
    }
    return map;
}

} // (anon)

Expected<EscapeMap>
loadGeneratorMetadata(std::string_view yamlPath)
{
    MRDOCS_TRY(GeneratorManifest manifest, loadGeneratorManifest(yamlPath));
    return toEscapeMap(manifest.escape);
}

Expected<void>
discoverDataDrivenGenerators(
    Config const& settings,
    GeneratorRegistry& registry)
{
    MRDOCS_TRY(
        std::vector<DiscoveredManifest> found,
        discoverGeneratorManifests(addon_paths::addonRoots(settings)));

    // A directory whose name is already the id of a generator is skipped,
    // whether the registry holds it already (a built-in generator, or
    // one an extension installed) or an earlier root contributed it: the
    // first one wins, so a duplicate id is a silent skip, not an error.
    std::vector<std::pair<std::string, DiscoveredManifest const*>> pending;
    std::unordered_map<std::string, DiscoveredManifest const*> pendingById;
    for (DiscoveredManifest const& d : found)
    {
        std::string const name(files::getFileName(d.dir));
        if (registry.find(name) || pendingById.contains(name))
        {
            continue;
        }
        pending.emplace_back(name, &d);
        pendingById.emplace(name, &d);
    }

    // What a generator extends: the manifest of a directory about to be
    // added, or the generator the registry holds. The registry is
    // consulted here, before the generators are added, because a
    // generator is immutable once it is in the registry and a manifest can
    // extend one that comes later in the list.
    auto const extendsOf =
        [&](std::string_view id) -> std::optional<std::string>
    {
        if (auto const it = pendingById.find(std::string(id));
            it != pendingById.end())
        {
            return it->second->manifest.extends;
        }
        auto const* const hbsGenerator =
            dynamic_cast<HandlebarsGenerator const*>(
                registry.find(id));
        if (!hbsGenerator)
        {
            return std::nullopt;
        }
        return std::string(hbsGenerator->extends());
    };

    for (auto const& [name, d] : pending)
    {
        MRDOCS_TRY(registry.install(
            std::make_shared<HandlebarsGenerator>(
                name, name, name,
                toEscapeMap(d->manifest.escape),
                d->manifest.extends,
                addon_paths::extensionChain(name, extendsOf))));
    }
    return {};
}

} // namespace mrdocs::hbs
