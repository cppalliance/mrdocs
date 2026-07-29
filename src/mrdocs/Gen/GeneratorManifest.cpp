//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "GeneratorManifest.hpp"
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/YAMLParser.h>
#include <filesystem>

namespace mrdocs {

namespace {

// Read a scalar node into a `std::string`.
std::string
scalarText(llvm::yaml::ScalarNode& node)
{
    llvm::SmallString<32> buf;
    llvm::StringRef const text = node.getValue(buf);
    return std::string(text.data(), text.size());
}

// Parse a YAML mapping whose entries are non-empty byte-sequence keys
// mapped to replacement strings. An empty key is a hard error.
Expected<void>
parseEscape(
    llvm::yaml::MappingNode& node,
    GeneratorManifest& manifest,
    std::string_view yamlPath)
{
    Expected<void> result;
    for (llvm::yaml::KeyValueNode& entry : node)
    {
        if (result.has_value())
        {
            llvm::yaml::ScalarNode* const keyNode =
                llvm::dyn_cast_or_null<llvm::yaml::ScalarNode>(entry.getKey());
            llvm::yaml::ScalarNode* const valNode =
                llvm::dyn_cast_or_null<llvm::yaml::ScalarNode>(entry.getValue());
            if (!keyNode || !valNode)
            {
                result = Unexpected(formatError(
                    "{}: each 'escape' entry must be a scalar->scalar mapping",
                    yamlPath));
            }
            else
            {
                std::string key = scalarText(*keyNode);
                if (key.empty())
                {
                    result = Unexpected(formatError(
                        "{}: escape key must not be empty", yamlPath));
                }
                else
                {
                    manifest.escape.emplace_back(
                        std::move(key), scalarText(*valNode));
                }
            }
        }
    }
    return result;
}

// Dispatch a single top-level manifest key to its handler. Unknown keys
// are ignored so future schema additions are non-breaking.
Expected<void>
parseTopLevelEntry(
    llvm::yaml::KeyValueNode& pair,
    GeneratorManifest& manifest,
    std::string_view yamlPath)
{
    Expected<void> result;
    llvm::yaml::ScalarNode* const keyNode =
        llvm::dyn_cast_or_null<llvm::yaml::ScalarNode>(pair.getKey());
    if (keyNode)
    {
        llvm::SmallString<16> keyBuf;
        llvm::StringRef const key = keyNode->getValue(keyBuf);
        if (key == "escape")
        {
            llvm::yaml::MappingNode* const escNode =
                llvm::dyn_cast_or_null<llvm::yaml::MappingNode>(pair.getValue());
            if (!escNode)
            {
                result = Unexpected(formatError(
                    "{}: 'escape' must be a mapping", yamlPath));
            }
            else
            {
                result = parseEscape(*escNode, manifest, yamlPath);
            }
        }
        else if (key == "extends")
        {
            llvm::yaml::ScalarNode* const valNode =
                llvm::dyn_cast_or_null<llvm::yaml::ScalarNode>(pair.getValue());
            if (!valNode)
            {
                result = Unexpected(formatError(
                    "{}: 'extends' must be a scalar", yamlPath));
            }
            else
            {
                manifest.extends = scalarText(*valNode);
            }
        }
    }
    return result;
}

} // (anon)

Expected<GeneratorManifest>
loadGeneratorManifest(std::string_view yamlPath)
{
    MRDOCS_TRY(std::string text, files::getFileText(yamlPath));
    llvm::SourceMgr sm;
    llvm::yaml::Stream stream(text, sm);

    GeneratorManifest manifest;
    llvm::yaml::document_iterator docIt = stream.begin();
    if (docIt == stream.end())
    {
        return manifest;
    }
    llvm::yaml::Node* const rootNode = docIt->getRoot();
    if (rootNode == nullptr ||
        llvm::isa<llvm::yaml::NullNode>(rootNode))
    {
        // Empty document: a file with no content, only comments, or a
        // literal `null`. All of these mean "no rules".
        return manifest;
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
        MRDOCS_TRY(parseTopLevelEntry(pair, manifest, yamlPath));
    }
    return manifest;
}

namespace {

constexpr std::string_view metadataFileName = "mrdocs-generator.yml";

// Append every manifested subdirectory of `generatorDir` to `out`.
Expected<void>
scanGeneratorDir(
    std::string_view generatorDir,
    std::vector<DiscoveredManifest>& out)
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
        std::string const dir = it->path().string();
        std::string const yamlPath = files::appendPath(
            dir, std::string(metadataFileName));
        if (!files::exists(yamlPath))
        {
            continue;
        }
        MRDOCS_TRY(GeneratorManifest manifest, loadGeneratorManifest(yamlPath));
        out.push_back(DiscoveredManifest{ dir, std::move(manifest) });
    }
    return {};
}

} // (anon)

Expected<std::vector<DiscoveredManifest>>
discoverGeneratorManifests(std::vector<std::string> const& roots)
{
    std::vector<DiscoveredManifest> out;
    for (std::string const& root : roots)
    {
        std::string const dir = files::appendPath(root, "generator");
        if (files::exists(dir))
        {
            MRDOCS_TRY(scanGeneratorDir(dir, out));
        }
    }
    return out;
}

} // mrdocs
