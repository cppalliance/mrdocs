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
#include <clang/Tooling/AllTUsExecution.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>

//------------------------------------------------
//
// YAML
//
//------------------------------------------------

using ConfigImpl = clang::mrdocs::ConfigImpl;
using SettingsImpl = ConfigImpl::SettingsImpl;

template<>
struct llvm::yaml::MappingTraits<
    SettingsImpl::FileFilter>
{
    static void mapping(IO& io,
        SettingsImpl::FileFilter& f)
    {
        io.mapOptional("include", f.include);
        io.mapOptional("file-patterns", f.filePatterns);
    }
};

template<>
struct llvm::yaml::ScalarEnumerationTraits<
    SettingsImpl::ExtractPolicy>
{
    using Policy = SettingsImpl::ExtractPolicy;

    static void enumeration(IO& io,
        Policy& value)
    {
        io.enumCase(value, "always", Policy::Always);
        io.enumCase(value, "dependency", Policy::Dependency);
        io.enumCase(value, "never", Policy::Never);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    SettingsImpl::Filters::Category>
{
    static void mapping(IO &io,
        SettingsImpl::Filters::Category& f)
    {
        io.mapOptional("include", f.include);
        io.mapOptional("exclude", f.exclude);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    SettingsImpl::Filters>
{
    static void mapping(IO &io,
        SettingsImpl::Filters& f)
    {
        io.mapOptional("symbols", f.symbols);
    }
};

template<>
struct llvm::yaml::MappingTraits<SettingsImpl>
{
    static void mapping(IO& io,
        SettingsImpl& cfg)
    {
        io.mapOptional("include",           cfg.ignoreFailures);
        io.mapOptional("defines",           cfg.defines);
        io.mapOptional("ignore-failures",   cfg.ignoreFailures);


        // io.mapOptional("extract",           cfg.extract);
        io.mapOptional("referenced-declarations", cfg.referencedDeclarations);
        io.mapOptional("anonymous-namespaces",    cfg.anonymousNamespaces);
        io.mapOptional("inaccessible-members",    cfg.inaccessibleMembers);
        io.mapOptional("inaccessible-bases",      cfg.inaccessibleBases);

        io.mapOptional("generate",          cfg.generate);
        io.mapOptional("multipage",         cfg.multiPage);
        io.mapOptional("source-root",       cfg.sourceRoot);
        io.mapOptional("base-url",          cfg.baseURL);

        io.mapOptional("input",             cfg.input);

        io.mapOptional("filters",           cfg.filters);
    }
};

//------------------------------------------------

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
    llvm::StringRef workingDir,
    llvm::StringRef addonsDir,
    llvm::StringRef configYaml,
    llvm::StringRef extraYaml,
    std::shared_ptr<ConfigImpl const> baseConfig,
    ThreadPool& threadPool)
    : threadPool_(threadPool)
{
    if(baseConfig)
        settings_ = baseConfig->settings_;

    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    // Check working dir
    if (!files::isAbsolute(workingDir))
    {
        formatError("working path \"{}\" is not absolute", workingDir).Throw();
    }
    settings_.workingDir = files::makeDirsy(files::normalizePath(workingDir));

    // Addons directory
    {
        settings_.addonsDir = files::makeAbsolute(addonsDir).value();
        files::requireDirectory(settings_.addonsDir).maybeThrow();
        MRDOCS_ASSERT(files::isDirsy(settings_.addonsDir));
    }

    // Config strings
    settings_.configYaml = configYaml;
    settings_.extraYaml = extraYaml;
    configObj_ = toDomObject(settings_.configYaml);

    // Parse the YAML strings
    YamlReporter reporter;
    {
        llvm::yaml::Input yin(
            settings_.configYaml,
            &reporter,
            reporter);
        yin.setAllowUnknownKeys(true);
        yin >> settings_;
        Error(yin.error()).maybeThrow();
    }
    {
        llvm::yaml::Input yin(settings_.extraYaml,
            &reporter, reporter);
        yin.setAllowUnknownKeys(true);
        yin >> settings_;
        Error(yin.error()).maybeThrow();
    }

    // Source root has to be forward slash style
    settings_.sourceRoot = files::makePosixStyle(files::makeDirsy(
        files::makeAbsolute(settings_.sourceRoot, settings_.workingDir)));

    // Base-URL has to be dirsy with forward slash style
    if (!settings_.baseURL.empty() && settings_.baseURL.back() != '/')
    {
        settings_.baseURL.push_back('/');
    }

    // Adjust input files
    for (auto& name : settings_.input.include)
    {
        name = files::makePosixStyle(
            files::makeAbsolute(name, settings_.workingDir));
    }

    // Parse the filters
    for(std::string_view pattern : settings_.filters.symbols.exclude)
    {
        parseSymbolFilter(settings_.symbolFilter, pattern, true);
    }
    for(std::string_view pattern : settings_.filters.symbols.include)
    {
        parseSymbolFilter(settings_.symbolFilter, pattern, false);
    }
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
            files::makeAbsolute(filePath, settings_.workingDir));
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
    std::string_view workingDir,
    std::string_view addonsDir,
    std::string_view configYaml,
    ThreadPool& threadPool)
{
    return std::make_shared<ConfigImpl>(
        workingDir,
        addonsDir,
        configYaml,
        "",
        nullptr,
        threadPool);
}

static
Expected<std::shared_ptr<ConfigImpl const>>
loadConfig(
    std::string_view filePath,
    std::string_view addonsDir,
    std::string_view extraYaml,
    std::shared_ptr<ConfigImpl const> const& baseConfig,
    ThreadPool& threadPool)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    std::string normFilePath = files::normalizePath(filePath);

    // Load the config file into a string
    MRDOCS_TRY(auto absConfigPath, files::makeAbsolute(normFilePath));
    MRDOCS_TRY(auto configYaml, files::getFileText(absConfigPath));

    // Calculate the working directory
    auto workingDir = files::getParentDir(absConfigPath);

    // Attempt to create the config
    try
    {
        return std::make_shared<ConfigImpl>(
            workingDir,
            addonsDir,
            configYaml,
            extraYaml,
            baseConfig,
            threadPool);
    }
    catch(Exception const& ex)
    {
        return Unexpected(ex.error());
    }
}

Expected<std::shared_ptr<ConfigImpl const>>
loadConfigFile(
    std::string_view configFilePath,
    std::string_view addonsDir,
    std::string_view extraYaml,
    std::shared_ptr<ConfigImpl const> baseConfig,
    ThreadPool& threadPool)
{
    return loadConfig(
        configFilePath,
        addonsDir,
        extraYaml,
        baseConfig,
        threadPool);
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
