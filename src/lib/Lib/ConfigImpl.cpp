//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/Lib/ConfigImpl.hpp"
#include "lib/Support/Debug.hpp"
#include "lib/Support/Error.hpp"
#include "lib/Support/Path.hpp"
#include "lib/Support/Yaml.hpp"
#include "lib/Support/Glob.hpp"
#include <mrdocs/Support/Path.hpp>
#include <utility>
#include <clang/Tooling/AllTUsExecution.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>

namespace clang {
namespace mrdocs {

namespace {

void
parseSymbolFilter(
    FilterNode& root,
    std::string_view str,
    bool excluded)
{
    // FIXME: this does not handle invalid qualified-ids
    std::vector<FilterPattern> parts;
    if(str.starts_with("::"))
        str.remove_prefix(2);
    do
    {
        std::size_t idx = str.find("::");
        parts.emplace_back(str.substr(0, idx));
        if(idx == std::string_view::npos)
            break;
        str.remove_prefix(idx + 2);
    }
    while(! str.empty());
    // merge the parsed patterns into the filter tree
    // mergeFilter(root, parts);
    root.mergePattern(parts, excluded);
}

dom::Object
toDomObject(std::string_view configYaml);

} // (anon)

ConfigImpl::
ConfigImpl(
    Config::Settings const& publicSettings,
    std::shared_ptr<ConfigImpl const> baseConfig,
    ThreadPool& threadPool,
    std::string_view execPath)
    : threadPool_(threadPool)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    // Copy the settings
    if(baseConfig)
    {
        settings_ = baseConfig->settings_;
    }
    dynamic_cast<Config::Settings&>(settings_) = publicSettings;

    // Check config dir
    if (!files::isAbsolute(settings_.configDir))
    {
        formatError("Config path \"{}\" is not absolute", settings_.configDir).Throw();
    }
    settings_.configDir = files::makeDirsy(files::normalizePath(settings_.configDir));

    if (settings_.libCxxPaths.empty())
    {
        // Set LibC++ path from process working directory
        std::string binDir = files::getParentDir(execPath);
        report::info("Using LibC++ include directory from \"{}\"", binDir);
        std::string libCxxDir = files::makeDirsy(files::appendPath(
                binDir, "..", "share", "mrdocs", "libcxx"));
        Error err = files::requireDirectory(libCxxDir);
        if (err)
        {
            formatError("Cannot find LibC++ include directory in \"{}\"", libCxxDir).Throw();
        }
        settings_.libCxxPaths.push_back(libCxxDir);
    }
    else
    {
        for (auto& path : settings_.libCxxPaths)
        {
            path = files::makeDirsy(files::normalizePath(path));
        }
    }

    // Addons directory
    settings_.addons = files::makeAbsolute(publicSettings.addons).value();
    files::requireDirectory(settings_.addons).maybeThrow();
    if (!files::isDirsy(settings_.addons))
    {
        formatError("Addons path \"{}\" is not a directory", settings_.addons).Throw();
    }

    // Config strings
    configObj_ = toDomObject(settings_.configYaml);

    // Source root has to be forward slash style
    settings_.sourceRoot = files::makePosixStyle(files::makeDirsy(
        files::makeAbsolute(settings_.sourceRoot, settings_.configDir)));

    // Base-URL has to be dirsy with forward slash style
    if (!settings_.baseURL.empty() && settings_.baseURL.back() != '/')
    {
        settings_.baseURL.push_back('/');
    }

    // Adjust input files
    for (auto& name : settings_.input.include)
    {
        name = files::makePosixStyle(
            files::makeAbsolute(name, settings_.configDir));
    }

    // Parse the filters
    for(std::string_view pattern : settings_.filters.symbols.exclude)
        parseSymbolFilter(settings_.symbolFilter, pattern, true);
    for(std::string_view pattern : settings_.filters.symbols.include)
        parseSymbolFilter(settings_.symbolFilter, pattern, false);
    for(std::string_view pattern: settings_.seeBelow)
        settings_.seeBelowFilter.emplace_back(pattern);
    for(std::string_view pattern: settings_.implementationDefined)
        settings_.implementationDefinedFilter.emplace_back(pattern);

    settings_.symbolFilter.finalize(false, false, false);
}

//------------------------------------------------

bool
ConfigImpl::
shouldVisitSymbol(
    llvm::StringRef filePath) const noexcept
{
    if (settings_.input.include.empty())
    {
        return true;
    }
    for (auto& p: settings_.input.include)
    {
        // Exact match
        if (filePath == p)
        {
            return true;
        }
        // Prefix match
        if (filePath.starts_with(p))
        {
            bool validPattern = std::ranges::any_of(
                    settings_.input.filePatterns,
                    [&](auto const &pattern) {
                        return globMatch(pattern, filePath);
                    });
            if (validPattern)
            {
                return true;
            }
        }
    }
    return false;
}

bool
ConfigImpl::
shouldExtractFromFile(
    llvm::StringRef filePath,
    std::string& prefixPath) const noexcept
{
    namespace path = llvm::sys::path;

    SmallPathString temp;
    if(! files::isAbsolute(filePath))
    {
        temp = files::makePosixStyle(
            files::makeAbsolute(filePath, settings_.configDir));
    }
    else
    {
        temp = filePath;
    }
    if(! path::replace_path_prefix(temp,
            settings_.sourceRoot, "", path::Style::posix))
        return false;
    MRDOCS_ASSERT(files::isDirsy(settings_.sourceRoot));
    prefixPath.assign(
        settings_.sourceRoot.begin(),
        settings_.sourceRoot.end());
    return true;
}

//------------------------------------------------

Expected<std::shared_ptr<ConfigImpl const>>
createConfig(
    std::string_view configDir,
    Config::Settings const& publicSettings,
    std::string_view configYaml,
    ThreadPool& threadPool,
    std::string_view execPath)
{
    return std::make_shared<ConfigImpl>(
        publicSettings,
        nullptr,
        threadPool,
        execPath);
}

static
Expected<std::shared_ptr<ConfigImpl const>>
loadConfig(
    Config::Settings const& publicSettings,
    std::shared_ptr<ConfigImpl const> const& baseConfig,
    ThreadPool& threadPool,
    std::string_view execPath)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;
    MRDOCS_CHECK(files::isAbsolute(publicSettings.config));
    MRDOCS_CHECK(files::isAbsolute(publicSettings.configDir));
    try
    {
        return std::make_shared<ConfigImpl>(
            publicSettings,
            baseConfig,
            threadPool,
            execPath);
    }
    catch(Exception const& ex)
    {
        return Unexpected(ex.error());
    }
}

Expected<std::shared_ptr<ConfigImpl const>>
loadConfigFile(
    Config::Settings const& publicSettings,
    std::shared_ptr<ConfigImpl const> const& baseConfig,
    ThreadPool& threadPool,
    std::string_view execPath)
{
    return loadConfig(
        publicSettings,
        baseConfig,
        threadPool,
        execPath);
}

namespace {
dom::Value
toDom(llvm::yaml::Node* Value);

dom::Object
toDomObject(llvm::yaml::MappingNode* Object)
{
    dom::Object obj;
    for (auto &Pair : *Object)
    {
        auto *KeyString = dyn_cast<llvm::yaml::ScalarNode>(Pair.getKey());
        if (!KeyString) { continue; }
        SmallString<10> KeyStorage;
        StringRef KeyValue = KeyString->getValue(KeyStorage);
        llvm::yaml::Node *Value = Pair.getValue();
        if (!Value) {
            obj.set(KeyValue, dom::Kind::Undefined);
            continue;
        }
        dom::Value value = toDom(Value);
        obj.set(KeyValue, value);
    }
    return obj;
}

dom::Array
toDomArray(llvm::yaml::SequenceNode* Array)
{
    dom::Array arr;
    for (auto &Node : *Array)
    {
        dom::Value value = toDom(&Node);
        arr.push_back(value);
    }
    return arr;
}

dom::Value
toDomScalar(llvm::yaml::ScalarNode* Scalar)
{
    SmallString<10> ScalarStorage;
    StringRef ScalarValue = Scalar->getValue(ScalarStorage);
    StringRef RawValue = Scalar->getRawValue();
    bool const isEscaped = RawValue.size() != ScalarValue.size();
    if (isEscaped)
    {
        return ScalarValue;
    }
    std::int64_t integer;
    auto res = std::from_chars(
        ScalarValue.begin(),
        ScalarValue.end(),
        integer);
    if (res.ec == std::errc())
    {
        return integer;
    }
    bool const isBool = ScalarValue == "true" || ScalarValue == "false";
    if (isBool)
    {
        return ScalarValue == "true";
    }
    bool const isNull = ScalarValue == "null";
    if (isNull)
    {
        return nullptr;
    }
    return ScalarValue;
}

dom::Value
toDom(llvm::yaml::Node* Value)
{
    auto *ValueObject = dyn_cast<llvm::yaml::MappingNode>(Value);
    if (ValueObject)
    {
        return toDomObject(ValueObject);
    }
    auto *ValueArray = dyn_cast<llvm::yaml::SequenceNode>(Value);
    if (ValueArray)
    {
        return toDomArray(ValueArray);
    }
    auto *ValueString = dyn_cast<llvm::yaml::ScalarNode>(Value);
    if (ValueString)
    {
        return toDomScalar(ValueString);
    }
    return nullptr;
}

/* Convert a YAML string to a DOM object.

   YAML forbids tab characters to use as indentation so
   only some JSON files are valid YAML.

   Also instead of providing built-in support for
   types such as `bool` or `int`, YAML uses strings
   for everything, which the specification defines
   as "scalar" values.

   When converting a scalar to a DOM value, only
   escaped strings are preserved as strings.
   Unescaped strings are converted to numbers
   if possible, and then to booleans if possible.
   This is done to preserve compatibility with
   JSON, allow the user to specify scalars as
   boolean or integer values, match the original
   intent of the author, and for scalar values
   to interoperate with other handlebars templates.

 */
dom::Object
toDomObject(std::string_view yaml)
{
    llvm::SourceMgr SM;
    llvm::yaml::Stream YAMLStream_(yaml, SM);
    llvm::yaml::document_iterator I = YAMLStream_.begin();
    if (I == YAMLStream_.end())
    {
        return {};
    }
    llvm::yaml::Node *Root = I->getRoot();
    auto *Object = dyn_cast<llvm::yaml::MappingNode>(Root);
    if (!Object)
    {
        return {};
    }
    return toDomObject(Object);
}
} // (anon)

dom::Object const&
ConfigImpl::object() const {
    return configObj_;
}

} // mrdocs
} // clang
