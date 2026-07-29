//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Support/Filesystem/Temp.hpp"
#include <mrdocs/Config.hpp>
#include <mrdocs/Dom/Array.hpp>
#include <mrdocs/Dom/Object.hpp>
#include <mrdocs/Dom/Value.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Report.hpp>
#include <mrdocs/Support/TypeTraits/Concepts.hpp>
#include <clang/Tooling/AllTUsExecution.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>
#include <charconv>
#include <ranges>
#include <thread>
#include <utility>

namespace mrdocs {

namespace {

dom::Value
toDom(llvm::yaml::Node* Value);

dom::Object
toDomObject(llvm::yaml::MappingNode* Object)
{
    dom::Object obj;
    for (auto &Pair : *Object)
    {
        auto *KeyString = clang::dyn_cast<llvm::yaml::ScalarNode>(Pair.getKey());
        if (!KeyString) { continue; }
        llvm::SmallString<10> KeyStorage;
        llvm::StringRef KeyValue = KeyString->getValue(KeyStorage);
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
    llvm::SmallString<10> ScalarStorage;
    llvm::StringRef ScalarValue = Scalar->getValue(ScalarStorage);
    llvm::StringRef RawValue = Scalar->getRawValue();
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
    auto *ValueObject = clang::dyn_cast<llvm::yaml::MappingNode>(Value);
    if (ValueObject)
    {
        return toDomObject(ValueObject);
    }
    auto *ValueArray = clang::dyn_cast<llvm::yaml::SequenceNode>(Value);
    if (ValueArray)
    {
        return toDomArray(ValueArray);
    }
    auto *ValueString = clang::dyn_cast<llvm::yaml::ScalarNode>(Value);
    if (ValueString)
    {
        return toDomScalar(ValueString);
    }
    return nullptr;
}

} // (anon)

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
    auto *Object = clang::dyn_cast<llvm::yaml::MappingNode>(Root);
    if (!Object)
    {
        return {};
    }
    return toDomObject(Object);
}

//------------------------------------------------

Expected<void>
Config::
load(
    Config& c,
    std::string_view const configYaml)
{
    // Typed options from the YAML.
    MRDOCS_TRY(ConfigSchema::load(c, configYaml));

    // DOM view of the same YAML, preserving unknown keys for templates.
    c.configObj_ = toDomObject(configYaml);

    // The free-form per-generator options live only in the DOM; lift them
    // into the typed map (llvm::yaml cannot map their dynamic shape).
    c.generatorOptions.clear();
    if (dom::Value const go = c.configObj_.get("generator-options");
        go.isObject())
    {
        go.getObject().visit(
            [&c](dom::String const& key, dom::Value const& value)
            {
                if (value.isObject())
                {
                    c.generatorOptions.emplace(
                        std::string(std::string_view(key)),
                        value.getObject());
                }
            });
    }

    // The free-form per-transform options are lifted the same way.
    c.transformOptions.clear();
    if (dom::Value const to = c.configObj_.get("transform-options");
        to.isObject())
    {
        to.getObject().visit(
            [&c](dom::String const& key, dom::Value const& value)
            {
                if (value.isObject())
                {
                    c.transformOptions.emplace(
                        std::string(std::string_view(key)),
                        value.getObject());
                }
            });
    }
    return {};
}

//------------------------------------------------

Expected<void>
Config::
load_file(
    Config& c,
    std::string_view configPath)
{
    auto ft = files::getFileType(configPath);
    MRDOCS_CHECK(ft, formatError(
        "Config file does not exist: \"{}\"", ft.error(), configPath));
    if (ft.value() == files::FileType::regular)
    {
        c.config = configPath;
        std::string configYaml = files::getFileText(c.config).value();
        MRDOCS_TRY(Config::load(c, configYaml));
        return {};
    }
    MRDOCS_CHECK(ft.value() == files::FileType::not_found,
        formatError("Config file is not regular file: \"{}\"", configPath));
    return {};
}

struct ConfigSchemaVisitor {
    template <class T>
    Expected<void>
    operator()(
        ConfigSchema& self,
        std::string_view name,
        T& value,
        ReferenceDirectories const& dirs,
        ConfigSchema::OptionProperties const& opts) const {
        using DT = std::decay_t<T>;
        if constexpr (std::same_as<DT, StringList>)
        {
            bool const useDefault =
                value.empty() && std::holds_alternative<StringList>(opts.defaultValue);
            if (useDefault)
            {
                value = std::get<StringList>(opts.defaultValue);
            }
            // Accept the comma-separated scalar form (e.g. "xml,adoc").
            value.splitCommaSeparated();
            MRDOCS_CHECK(!value.empty() || !opts.required,
                formatError("`{}` option is required", name));
            return {};
        }
        else if constexpr (std::ranges::range<DT>)
        {
            bool const useDefault = value.empty() && std::holds_alternative<DT>(opts.defaultValue);
            if (useDefault) {
                value = std::get<DT>(opts.defaultValue);
            }
            MRDOCS_CHECK(!value.empty() || !opts.required,
                formatError("`{}` option is required", name));
            if constexpr (std::same_as<DT, std::string>)
            {
                return normalizeString(self, name, value, dirs, opts, useDefault);
            }
            else if constexpr (std::same_as<std::string, std::ranges::range_value_t<DT>>)
            {
                return normalizeStringRange(self, name, value, dirs, opts, useDefault);
            }
            else if constexpr (std::same_as<PathGlobPattern, std::ranges::range_value_t<DT>>)
            {
                for (auto& v : value)
                {
                    MRDOCS_TRY(normalizePathGlob(self, name, v, dirs, opts, useDefault));
                }
                return {};
            }
        }
        else if constexpr (std::same_as<DT, int> || std::same_as<DT, unsigned>)
        {
            return normalizeInteger(self, name, value, opts);
        }
        else
        {
            // Booleans and other types should already be validated because
            // the struct already has their default values and there's no
            // base path to prepend.
            return {};
        }
        return {};
    }

    Expected<void>
    normalizeString(
        ConfigSchema& self,
        std::string_view const name,
        std::string& value,
        ReferenceDirectories const& dirs,
        ConfigSchema::OptionProperties const& opts,
        bool const usingDefault) const {
        if (!value.empty()
            && (opts.type == ConfigSchema::OptionType::Path
                || opts.type == ConfigSchema::OptionType::DirPath
                || opts.type == ConfigSchema::OptionType::FilePath))
        {
            MRDOCS_TRY(
                normalizeStringPath(self, name, value, dirs, opts, usingDefault));
        }
        else if (opts.type == ConfigSchema::OptionType::String)
        {
            // The base-url option should end with a slash
            if (name == "base-url")
            {
                if (!value.empty() && value.back() != '/')
                {
                    value.push_back('/');
                }
            }
        }
        return {};
    }

    static
    Expected<void>
    normalizeStringPath(
        ConfigSchema& self,
        std::string_view name,
        std::string& value,
        ReferenceDirectories const& dirs,
        ConfigSchema::OptionProperties const& opts,
        bool const usingDefault)
    {
        // If the path is not absolute, we need to expand it
        if (!files::isAbsolute(value))
        {
            // Find the base directory for this option
            if (auto expBaseDir
                = getBaseDir(value, dirs, self, usingDefault, opts);
                !expBaseDir)
            {
                // Can't find the base directory, make it absolute
                MRDOCS_TRY(value, files::makeAbsolute(value));
            }
            else
            {
                std::string_view baseDir = *expBaseDir;
                value = files::makeAbsolute(value, baseDir);
            }
        }
        // Make it POSIX style
        value = files::makePosixStyle(value);
        if (!opts.mustExist && opts.shouldExist && !files::exists(value))
        {
            report::warn(
                R"("{}" option: The directory or file "{}" does not exist)",
                name,
                value);
        }
        MRDOCS_CHECK(
            !opts.mustExist || files::exists(value),
            formatError("`{}` option: path does not exist: {}", name, value));
        MRDOCS_CHECK(
            opts.type != ConfigSchema::OptionType::DirPath
                || files::isDirectory(value),
            formatError(
                "`{}` option: path should be a directory: {}",
                name,
                value));
        MRDOCS_CHECK(
            opts.type != ConfigSchema::OptionType::FilePath
                || !files::isDirectory(value),
            formatError(
                "`{}` option: path should be a regular file: {}",
                name,
                value));

        return {};
    }

    static
    Expected<void>
    normalizePathGlob(
        ConfigSchema& self,
        std::string_view,
        PathGlobPattern& value,
        ReferenceDirectories const& dirs,
        ConfigSchema::OptionProperties const& opts,
        bool const usingDefault)
    {
        // If the path is not absolute, we need to expand it
        if (std::string_view pattern = value.pattern();
            !files::isAbsolute(pattern))
        {
            // Find the base directory for this option
            std::string absPattern(pattern);
            if (auto expBaseDir = getBaseDir(absPattern, dirs, self, usingDefault, opts);
                expBaseDir)
            {
                // Make the pattern absolute relative to the base directory.
                // The join uses the OS-native separator (backslashes on
                // Windows), so re-POSIX-ify the result; PathGlobPattern::match
                // splits on `/` and `**` would otherwise be stopped by `\`.
                std::string baseDir = *expBaseDir;
                baseDir = files::makePosixStyle(baseDir);
                absPattern = files::makeAbsolute(pattern, baseDir);
                absPattern = files::makePosixStyle(absPattern);
                MRDOCS_TRY(value, PathGlobPattern::create(absPattern));
            }
        }
        return {};
    }

    template <std::ranges::range T>
    requires std::same_as<std::ranges::range_value_t<T>, std::string>
    Expected<void>
    normalizeStringRange(
        ConfigSchema& self,
        std::string_view name,
        T& values,
        ReferenceDirectories const& dirs,
        ConfigSchema::OptionProperties const& opts,
        bool const usingDefault) const
    {
        if (opts.type == ConfigSchema::OptionType::ListPath)
        {
            MRDOCS_TRY(normalizeStringPathRange(self, name, values, dirs, opts, usingDefault));
        }

        return {};
    }

    template <class T>
    Expected<void>
    normalizeStringPathRange(
        ConfigSchema& self,
        std::string_view name,
        T& values,
        ReferenceDirectories const& dirs,
        ConfigSchema::OptionProperties const& opts,
        bool const usingDefault) const
    {
        // Move command line sink values to appropriate destinations
        // Normalization happens later for each destination
        if (opts.commandLineSink && opts.filenameMapping.has_value())
        {
            MRDOCS_TRY(normalizeCmdLineSink(self, values, opts));
        }
        else
        {
            // General case, normalize each path
            for (auto& value : values)
            {
                MRDOCS_TRY(normalizeStringPath(self, name, value, dirs, opts, usingDefault));
            }
        }
        return {};
    }

    template <class T>
    Expected<void>
    normalizeCmdLineSink(
        ConfigSchema& self,
        T& values,
        ConfigSchema::OptionProperties const& opts) const
    {
        // Move command line sink values to appropriate destinations
        for (auto& value : values)
        {
            std::string_view filename = files::getFileName(value);
            auto it = opts.filenameMapping->find(std::string(filename));
            if (it == opts.filenameMapping->end())
            {
                report::warn("command line input: unknown destination for filename \"{}\"", filename);
                continue;
            }
            // Assign the value to the destination option of the map
            std::string const& destOption = it->second;
            bool foundOption = false;
            bool setOption = false;
            self.visit(
                [&]<typename U>(
                    std::string_view const optionName, U& optionValue)
            {
                if constexpr (std::convertible_to<U, std::string>)
                {
                    if (optionName == destOption)
                    {
                        foundOption = true;
                        if (optionValue.empty())
                        {
                            optionValue = value;
                            setOption = true;
                        }
                    }
                }
            });
            if (!foundOption)
            {
                report::warn("command line input: cannot find destination option \"{}\"", destOption);
            }
            else if (!setOption)
            {
                report::warn("command line input: destination option was \"{}\" already set", destOption);
            }
        }
        values.clear();
        return {};
    }

    template <std::integral T>
    Expected<void>
    normalizeInteger(
        ConfigSchema& self,
        std::string_view name,
        T& value,
        ConfigSchema::OptionProperties const& opts) const
    {
        MRDOCS_CHECK(
            !opts.minValue || std::cmp_greater_equal(value, *opts.minValue),
            formatError(
                "`{}` option: value {} is less than minimum: {}",
                name,
                value,
                *opts.minValue));
        MRDOCS_CHECK(
            !opts.maxValue || std::cmp_less_equal(value, *opts.maxValue),
            formatError(
                "`{}` option: value {} is greater than maximum: {}",
                name,
                value,
                *opts.maxValue));

        if (name == "concurrency" && std::cmp_equal(value, 0))
        {
            value = std::thread::hardware_concurrency();
            return {};
        }

        if (name == "report" && std::cmp_not_equal(value, static_cast<unsigned>(-1)))
        {
            static_assert(
                static_cast<unsigned>(ConfigSchema::LogLevel::Trace) ==
                static_cast<unsigned>(report::Level::trace));
            static_assert(
                static_cast<unsigned>(ConfigSchema::LogLevel::Fatal) ==
                static_cast<unsigned>(report::Level::fatal));
            MRDOCS_ASSERT(opts.deprecated);
            report::warn(
                "`report` option is deprecated, use `log-level` instead");
            auto const logLevel = static_cast<ConfigSchema::LogLevel>(value);
            auto logLevelStr = ConfigSchema::toString(logLevel);
            report::warn("`report` option: setting `log-level` to \"{}\"", logLevelStr);
            self.logLevel = logLevel;
            return {};
        }
        return {};
    }

    static
    Expected<std::string>
    getBaseDir(
        std::string_view relativeTo,
        ReferenceDirectories const& dirs,
        ConfigSchema const& self)
    {
        if (relativeTo.empty())
        {
            return Unexpected(Error("relative-to value is empty"));
        }

        // Get base dir from the main reference directories
        if (relativeTo == "cwd")
        {
            return dirs.cwd;
        }
        if (relativeTo == "mrdocs-root")
        {
            return dirs.mrdocsRoot;
        }
        Expected<std::string> res =
            Unexpected(formatError("unknown relative-to value: \"{}\"", relativeTo));
        bool found = false;
        self.visit([&]<typename T>(std::string_view const optionName, T& value)
        {
            if constexpr (std::convertible_to<T, std::string_view>)
            {
                if (found)
                {
                    return;
                }
                if (relativeTo == optionName)
                {
                    std::string_view valueSv(value);
                    if (!value.empty())
                    {
                        res = value;
                        found = true;
                        return;
                    }
                    res = Unexpected(formatError(
                            "relative-to value \"{}\" is empty",
                            relativeTo));
                }
                else if (
                    relativeTo.size() == optionName.size() + 4 &&
                    relativeTo.starts_with(optionName) &&
                    relativeTo.ends_with("-dir"))
                {
                    std::string_view valueSv(value);
                    if (!value.empty())
                    {
                        bool const valueIsDir =
                            [&value]() {
                                if (files::exists(value))
                                {
                                    return files::isDirectory(value);
                                }
                                std::string_view const filename = files::getFileName(value);
                                return filename.find('.') == std::string::npos;
                            }();
                        if (valueIsDir)
                        {
                            res = value;
                        }
                        else
                        {
                            res = files::getParentDir(value);
                        }
                        found = true;
                        return;
                    }
                    res = Unexpected(formatError(
                            "relative-to value \"{}\" is empty",
                            relativeTo));
                }
            }
        });
        return res;
    }

    static
    std::string_view
    trimBaseDirReference(std::string_view const s0)
    {
        std::string_view s = s0;
        if (s.size() > 2 &&
            s.front() == '<' &&
            s.back() == '>')
        {
            s.remove_prefix(1);
            s.remove_suffix(1);
        }
        return s;
    };

    static
    Expected<std::string>
    getBaseDir(
        std::string& value,
        ReferenceDirectories const& dirs,
        ConfigSchema const& settings,
        bool const useDefault,
        ConfigSchema::OptionProperties const& opts)
    {
        if (!useDefault) {
            // If we did not use the default value, we use "relativeto"
            // as the base path
            std::string_view relativeTo = opts.relativeTo;
            if (!relativeTo.starts_with('<') ||
                !relativeTo.ends_with('>'))
            {
                return Unexpected(formatError(
                    "option \"{}\" has no relativeTo dir '<>'",
                    value));
            }
            relativeTo = trimBaseDirReference(relativeTo);
            return getBaseDir(relativeTo, dirs, settings);
        }

        // If we used the default value, the base dir comes from
        // the first path segment of the value
        std::string_view referenceDirKey = value;
        auto const pos = referenceDirKey.find('/');
        if (pos != std::string::npos) {
            referenceDirKey = referenceDirKey.substr(0, pos);
        }
        if (!referenceDirKey.starts_with('<') ||
            !referenceDirKey.ends_with('>'))
        {
            return Unexpected(formatError(
                "default value \"{}\" has no ref dir '<>'",
                value));
        }
        referenceDirKey = trimBaseDirReference(referenceDirKey);
        MRDOCS_TRY(
            std::string_view const baseDir,
            getBaseDir(referenceDirKey, dirs, settings));
        if (pos != std::string::npos) {
            value = value.substr(pos + 1);
        }
        return std::string(baseDir);
    }
};

Expected<void>
Config::
normalize(ReferenceDirectories const& dirs)
{
    MRDOCS_TRY(ConfigSchema::normalize(dirs, ConfigSchemaVisitor{}));
    // Overlay the now-normalized typed values onto the DOM view.
    updateConfigDom();
    return {};
}

std::string
Config::
configDir() const
{
    return std::string(files::getParentDir(config));
}

void
Config::
updateConfigDom()
{
    // configObj_ already holds the keys present in the YAML; add the typed
    // value for each option the YAML did not set.
    this->visit([this]<class T>(std::string_view name, T& value) {
        MRDOCS_CHECK_OR(!configObj_.exists(name));
        if constexpr (std::convertible_to<T, std::string_view>)
        {
            configObj_.set(name, std::string(value));
        }
        else if constexpr (range_of_tuple_like<T>)
        {
            dom::Object obj;
            auto keys = value | std::views::keys;
            auto vals = value | std::views::values;
            auto zip = std::views::zip(keys, vals);
            for (auto const& [k, v] : zip)
            {
                obj.set(k, v);
            }
            configObj_.set(name, std::move(obj));
        }
        else if constexpr (std::ranges::range<T>)
        {
            using ValueType = std::ranges::range_value_t<T>;
            dom::Array arr;
            for (auto const& v : value)
            {
                if constexpr (
                    std::is_same_v<ValueType, PathGlobPattern> ||
                    std::is_same_v<ValueType, SymbolGlobPattern>)
                {
                    arr.emplace_back(v.pattern());
                }
                else
                {
                    arr.emplace_back(v);
                }
            }
            configObj_.set(name, std::move(arr));
        }
        else if constexpr (std::is_enum_v<T>)
        {
            configObj_.set(name, to_string(value));
        }
        else
        {
            configObj_.set(name, value);
        }
    });
}

} // mrdocs
