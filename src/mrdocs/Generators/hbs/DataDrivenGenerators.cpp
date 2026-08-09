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
#include <mrdocs/Generators/GeneratorManifest.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/YAMLParser.h>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
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
discoverDataDrivenGenerators(Config const& settings)
{
    MRDOCS_TRY(
        std::vector<DiscoveredManifest> found,
        discoverGeneratorManifests(addon_paths::addonRoots(settings)));
    for (DiscoveredManifest const& d : found)
    {
        // The generator registry is process-global and is not cleared
        // between runs in the same process. `installGenerator` fails when
        // the id is already taken, whether by a built-in or by an
        // earlier addon root's generator of the same name. That is the
        // first-writer-wins layering we want, so a duplicate id is a
        // silent skip rather than an error (a `null` generator is the only
        // other failure it reports, and we never pass one).
        std::string const name(files::getFileName(d.dir));
        (void)installGenerator(
            std::make_unique<HandlebarsGenerator>(
                name, name, name,
                toEscapeMap(d.manifest.escape),
                d.manifest.extends));
    }
    return {};
}

} // namespace mrdocs::hbs
