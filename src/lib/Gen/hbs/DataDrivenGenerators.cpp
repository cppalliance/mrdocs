//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "DataDrivenGenerators.hpp"
#include "AddonPaths.hpp"
#include "HandlebarsGenerator.hpp"
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Path.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/YAMLParser.h>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace mrdocs::hbs {

namespace {

constexpr std::string_view metadataFileName = "mrdocs-generator.yml";

// Populate `map` from a YAML mapping whose entries are non-empty
// byte-sequence keys mapped to replacement strings. An empty key
// is a hard error.
Expected<void>
populateEscapeFromMapping(
    llvm::yaml::MappingNode& node,
    EscapeMap& map,
    std::string_view yamlPath)
{
    for (llvm::yaml::KeyValueNode& entry : node)
    {
        llvm::yaml::ScalarNode* keyNode =
            llvm::dyn_cast_or_null<llvm::yaml::ScalarNode>(entry.getKey());
        llvm::yaml::ScalarNode* valNode =
            llvm::dyn_cast_or_null<llvm::yaml::ScalarNode>(entry.getValue());
        if (!keyNode || !valNode)
        {
            return Unexpected(formatError(
                "{}: each 'escape' entry must be a scalar->scalar mapping",
                yamlPath));
        }
        llvm::SmallString<8> keyBuf;
        llvm::SmallString<32> valBuf;
        llvm::StringRef const keyStr = keyNode->getValue(keyBuf);
        llvm::StringRef const valStr = valNode->getValue(valBuf);
        if (keyStr.empty())
        {
            return Unexpected(formatError(
                "{}: escape key must not be empty",
                yamlPath));
        }
        map.set(
            std::string_view(keyStr.data(), keyStr.size()),
            std::string_view(valStr.data(), valStr.size()));
    }
    return {};
}

// Install a HandlebarsGenerator for the data-driven format in `dir`,
// when `dir` opts in by shipping an `mrdocs-generator.yml`.
//
// The presence of the manifest is the explicit opt-in: a directory
// under <addons>/generator/ becomes a generator only when it ships
// this file. Directories that hold shared assets (the built-in
// `common/` is the canonical example) simply don't declare a manifest,
// and discovery skips them.
//
// The generator registry is process-global and is not cleared between
// runs in the same process. `installGenerator` fails when the id is
// already taken, whether by a built-in or by a generator an earlier
// addon root installed under the same name. That is the
// first-writer-wins layering we want, so a duplicate id is a silent
// skip rather than an error (a null generator is the only other
// failure it reports, and we never pass one). In the test executable
// this also means the first test to install an id wins for the rest
// of the process; two fixtures cannot ship competing generators of
// the same name.
Expected<void>
maybeRegister(std::filesystem::path const& dir)
{
    std::string const yamlPath = files::appendPath(
        dir.string(), std::string(metadataFileName));
    if (!files::exists(yamlPath))
    {
        return {};
    }
    std::string const name = dir.filename().string();
    MRDOCS_TRY(EscapeMap escapeMap, loadGeneratorMetadata(yamlPath));
    (void)installGenerator(
        std::make_unique<HandlebarsGenerator>(
            name, name, name, std::move(escapeMap)));
    return {};
}

// Scan a single <root>/generator/ directory.
Expected<void>
scanGeneratorDir(std::string_view generatorDir)
{
    namespace fs = std::filesystem;
    std::error_code iterEc;
    fs::directory_iterator const end{};
    for (fs::directory_iterator it(generatorDir, iterEc);
         !iterEc && it != end;
         it.increment(iterEc))
    {
        std::error_code typeEc;
        if (!it->is_directory(typeEc))
        {
            continue;
        }
        MRDOCS_TRY(maybeRegister(it->path()));
    }
    return {};
}

} // (anon)

Expected<EscapeMap>
loadGeneratorMetadata(std::string_view yamlPath)
{
    MRDOCS_TRY(std::string text, files::getFileText(yamlPath));
    llvm::SourceMgr sm;
    llvm::yaml::Stream stream(text, sm);

    EscapeMap map;
    llvm::yaml::document_iterator docIt = stream.begin();
    if (docIt == stream.end())
    {
        return map;
    }
    llvm::yaml::Node* const rootNode = docIt->getRoot();
    if (rootNode == nullptr ||
        llvm::isa<llvm::yaml::NullNode>(rootNode))
    {
        // Empty document: file with no content, only comments, or a
        // literal `null`. All of these mean "no rules".
        return map;
    }
    llvm::yaml::MappingNode* const root =
        llvm::dyn_cast<llvm::yaml::MappingNode>(rootNode);
    if (!root)
    {
        return Unexpected(formatError(
            "{}: top-level YAML node must be a mapping", yamlPath));
    }

    for (llvm::yaml::KeyValueNode& pair : *root)
    {
        llvm::yaml::ScalarNode* const keyNode =
            llvm::dyn_cast_or_null<llvm::yaml::ScalarNode>(pair.getKey());
        if (!keyNode)
        {
            continue;
        }
        llvm::SmallString<16> keyBuf;
        if (keyNode->getValue(keyBuf) != "escape")
        {
            continue;
        }
        llvm::yaml::MappingNode* const escNode =
            llvm::dyn_cast_or_null<llvm::yaml::MappingNode>(pair.getValue());
        if (!escNode)
        {
            return Unexpected(formatError(
                "{}: 'escape' must be a mapping", yamlPath));
        }
        MRDOCS_TRY(populateEscapeFromMapping(*escNode, map, yamlPath));
    }
    return map;
}

Expected<void>
discoverDataDrivenGenerators(Config::Settings const& settings)
{
    std::vector<std::string> const roots = addon_paths::addonRoots(settings);
    for (std::string const& root : roots)
    {
        std::string const dir = files::appendPath(root, "generator");
        if (!files::exists(dir))
        {
            continue;
        }
        MRDOCS_TRY(scanGeneratorDir(dir));
    }
    return {};
}

} // namespace mrdocs::hbs
