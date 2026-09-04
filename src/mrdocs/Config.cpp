//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Support/CliOverride.hpp"
#include "Support/Filesystem/Temp.hpp"
#include <mrdocs/Config.hpp>
#include <mrdocs/Dom/Array.hpp>
#include <mrdocs/Dom/Object.hpp>
#include <mrdocs/Dom/Value.hpp>
#include <mrdocs/Support/Filesystem/Glob.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <mrdocs/Support/Report.hpp>
#include <mrdocs/Support/String/StringList.hpp>
#include <mrdocs/Support/TypeTraits/Concepts.hpp>
#include <mrdocs/Support/TypeTraits/TypeTraits.hpp>
#include <clang/Tooling/AllTUsExecution.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>
#include <charconv>
#include <map>
#include <ranges>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

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
    // A block scalar (YAML `|` / `>`) is always literal text, e.g. the
    // multi-line shim content in `missing-include-shims`. Take its value
    // verbatim, with no int/bool/null coercion.
    auto *ValueBlock = clang::dyn_cast<llvm::yaml::BlockScalarNode>(Value);
    if (ValueBlock)
    {
        return dom::Value(ValueBlock->getValue().str());
    }
    return nullptr;
}

/* Assign a value from the configuration to a typed schema member.

   This is the one place that turns a parsed value (a scalar, sequence, or
   mapping, already normalized to a `dom::Value`) into whatever type the
   option has, using reflection for the categories that need it
   (enumerators). Both the YAML loader and the command-line overrides route
   through it, so a value behaves the same whichever source it comes from.
   Free-form object options keep their DOM shape.

   @param out The option member to assign.
   @param v The value from the configuration file or command line.
   @return Nothing on success, otherwise a description of the mismatch.
*/
template <class T>
Expected<void>
assignConfigValue(T& out, dom::Value const& v)
{
    if constexpr (std::is_same_v<T, bool>)
    {
        // A boolean, or any truthy scalar (e.g. `1`), enables the flag.
        out = v.isBoolean() ? v.getBool() : v.isTruthy();
        return {};
    }
    else if constexpr (std::is_enum_v<T>)
    {
        if (!v.isString())
        {
            return Unexpected(formatError(
                "expected a string for an enumerated option"));
        }
        std::string_view const s = v.getString();
        Expected<void> result =
            Unexpected(formatError("invalid value \"{}\"", s));
        describe::for_each(
            describe::describe_enumerators<T>{},
            [&](auto d)
            {
                if (toString(d.value) == s)
                {
                    out = d.value;
                    result = {};
                }
            });
        return result;
    }
    else if constexpr (std::is_integral_v<T>)
    {
        if (!v.isInteger())
        {
            return Unexpected(formatError("expected an integer"));
        }
        out = static_cast<T>(v.getInteger());
        return {};
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        if (!v.isString())
        {
            return Unexpected(formatError("expected a string"));
        }
        out = std::string(v.getString());
        return {};
    }
    else if constexpr (std::is_same_v<T, StringList>)
    {
        // A string or a sequence of strings.
        std::vector<std::string> values;
        if (v.isArray())
        {
            dom::Array const& arr = v.getArray();
            for (std::size_t i = 0; i < arr.size(); ++i)
            {
                values.emplace_back(arr.get(i).getString());
            }
        }
        else if (v.isString())
        {
            values.emplace_back(v.getString());
        }
        out = StringList(std::move(values));
        return {};
    }
    else if constexpr (
        std::is_same_v<T, PathGlobPattern> ||
        std::is_same_v<T, SymbolGlobPattern>)
    {
        if (!v.isString())
        {
            return Unexpected(formatError("expected a glob pattern string"));
        }
        auto pattern = T::create(v.getString());
        if (!pattern)
        {
            return Unexpected(pattern.error());
        }
        out = std::move(pattern.value());
        return {};
    }
    else if constexpr (std::is_same_v<T, dom::Object>)
    {
        // Free-form option blocks (generator-options / transform-options)
        // keep their DOM shape.
        if (v.isObject())
        {
            out = v.getObject();
        }
        return {};
    }
    else if constexpr (mrdocs::specialization_of<T, std::vector>)
    {
        using Element = typename T::value_type;
        out.clear();
        if (v.isArray())
        {
            dom::Array const& arr = v.getArray();
            for (std::size_t i = 0; i < arr.size(); ++i)
            {
                Element element{};
                MRDOCS_TRY(assignConfigValue(element, arr.get(i)));
                out.push_back(std::move(element));
            }
        }
        return {};
    }
    else if constexpr (mrdocs::specialization_of<T, std::map>)
    {
        using Mapped = typename T::mapped_type;
        out.clear();
        if (v.isObject())
        {
            Expected<void> result;
            v.getObject().visit(
                [&](dom::String const& key, dom::Value const& value) -> bool
                {
                    Mapped mapped{};
                    if (auto e = assignConfigValue(mapped, value); !e)
                    {
                        result = Unexpected(e.error());
                        return false;
                    }
                    out.emplace(
                        std::string(std::string_view(key)), std::move(mapped));
                    return true;
                });
            return result;
        }
        return {};
    }
    else
    {
        return Unexpected(formatError(
            "unsupported configuration option type"));
    }
}

// Apply the `--option` overrides parsed from argv onto a schema. Internal to
// this translation unit; the only public entry points are load / load_file,
// which route through it. Defined below, after the reflection helpers it uses.
Expected<void>
applyCommandLineOverrides(ConfigSchema& c, char const** argv);

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
    std::string_view const configYaml,
    char const** argv)
{
    // Walk the YAML mapping once and route each value straight to the schema
    // member with the matching key. Reflection over the schema (forEach) yields
    // the option's kebab-case key and its type, so a single value conversion
    // (assignConfigValue, shared with the command line) turns the parsed
    // scalar, sequence, or mapping into the option's type, including the
    // free-form object options. A key matching no option is recorded and
    // reported once the log level is configured (see reportUnknownConfigKeys),
    // because config loading runs before that.
    c.unknownConfigKeys.clear();
    llvm::SourceMgr SM;
    llvm::yaml::Stream stream(configYaml, SM);
    llvm::yaml::document_iterator I = stream.begin();
    llvm::yaml::MappingNode* mapping = nullptr;
    if (I != stream.end())
    {
        mapping = clang::dyn_cast<llvm::yaml::MappingNode>(I->getRoot());
    }
    Expected<void> result;
    // An empty document (or a non-mapping root) contributes no values, but the
    // command-line overrides below still apply, so the walk is simply skipped.
    if (mapping)
    {
        for (auto& pair : *mapping)
        {
            auto* keyNode =
                clang::dyn_cast<llvm::yaml::ScalarNode>(pair.getKey());
            if (!keyNode)
            {
                continue;
            }
            llvm::SmallString<32> keyStorage;
            std::string_view const key(keyNode->getValue(keyStorage));
            llvm::yaml::Node* valueNode = pair.getValue();
            dom::Value const value =
                valueNode ? toDom(valueNode) : dom::Value(dom::Kind::Undefined);
            bool matched = false;
            c.forEach(
                [&](std::string_view const name, auto& member)
                {
                    if (matched || name != key)
                    {
                        return;
                    }
                    matched = true;
                    if (result)
                    {
                        if (auto e = assignConfigValue(member, value); !e)
                        {
                            result = Unexpected(e.error());
                        }
                    }
                });
            if (!matched)
            {
                c.unknownConfigKeys.emplace_back(key);
            }
        }
    }
    // Apply the command-line overrides on top of the file's values (a no-op
    // when argv is null). This is the single place the command line is folded
    // into the configuration, so applyCommandLineOverrides stays private.
    if (!result)
    {
        return result;
    }
    return applyCommandLineOverrides(c, argv);
}

//------------------------------------------------

namespace {

// Whether `key` names an option that takes a command-line value. Object-map
// options have no scalar flag (their nested keys arrive as dotted overrides),
// so they are not command-line keys in this sense. Reflection makes the check
// cheap enough to run per token instead of precomputing a set.
bool
isCommandLineOptionKey(ConfigSchema const& c, std::string_view const key)
{
    bool found = false;
    c.forEach(
        [&](std::string_view const name, auto const& member)
        {
            using M = std::decay_t<decltype(member)>;
            if constexpr (!std::is_same_v<M, std::map<std::string, dom::Object>>)
            {
                if (name == key)
                {
                    found = true;
                }
            }
        });
    return found;
}

// Whether `key` names a boolean option, so a bare `--flag` reads as true.
bool
isBooleanOptionKey(ConfigSchema const& c, std::string_view const key)
{
    bool result = false;
    c.forEach(
        [&](std::string_view const name, auto const& member)
        {
            if constexpr (std::is_same_v<std::decay_t<decltype(member)>, bool>)
            {
                if (name == key)
                {
                    result = true;
                }
            }
        });
    return result;
}

Expected<void>
applyCommandLineOverrides(ConfigSchema& c, char const** argv)
{
    if (!argv)
    {
        return {};
    }

    // Gather the string value(s) supplied for each option, in order. A
    // repeated option accumulates (list options), a boolean flag with no
    // value reads as "true", and the `--key value` form consumes the next
    // token. Tokens that name no option (and positionals) are left to the
    // command-line parser.
    std::map<std::string, std::vector<std::string>, std::less<>> supplied;
    for (char const** p = argv; *p != nullptr; ++p)
    {
        std::string_view arg(*p);
        if (!arg.starts_with("--"))
        {
            continue;
        }
        arg.remove_prefix(2);
        auto const eq = arg.find('=');
        std::string_view const key =
            eq == std::string_view::npos ? arg : arg.substr(0, eq);
        // Dotted overrides address object options and are applied separately.
        if (key.find('.') != std::string_view::npos)
        {
            continue;
        }
        if (!isCommandLineOptionKey(c, key))
        {
            continue;
        }
        if (eq != std::string_view::npos)
        {
            supplied[std::string(key)].emplace_back(arg.substr(eq + 1));
        }
        else if (isBooleanOptionKey(c, key))
        {
            supplied[std::string(key)].emplace_back("true");
        }
        else if (*(p + 1) != nullptr)
        {
            ++p;
            supplied[std::string(key)].emplace_back(*p);
        }
    }

    // Assign each supplied option through the shared value conversion, and
    // fold the dotted object overrides onto the object-map options.
    Expected<void> result;
    c.forEach(
        [&](std::string_view const name, auto& member)
        {
            if (!result)
            {
                return;
            }
            using M = std::decay_t<decltype(member)>;
            if constexpr (std::is_same_v<M, std::map<std::string, dom::Object>>)
            {
                if (auto e = applyDottedObjectOverrides(member, name, argv); !e)
                {
                    result = Unexpected(e.error());
                }
            }
            else
            {
                auto const it = supplied.find(name);
                if (it == supplied.end())
                {
                    return;
                }
                std::vector<std::string> const& values = it->second;
                if constexpr (
                    std::is_same_v<M, std::map<std::string, std::string>>)
                {
                    // Each value is a `key=value` pair, the command-line form
                    // of a string map (e.g. `--missing-include-shims=k=v`).
                    for (std::string_view const entry : values)
                    {
                        auto const sep = entry.find('=');
                        if (sep == std::string_view::npos)
                        {
                            result = Unexpected(formatError(
                                "`--{}` override needs `key=value`, got \"{}\"",
                                name, entry));
                            return;
                        }
                        member[std::string(entry.substr(0, sep))] =
                            std::string(entry.substr(sep + 1));
                    }
                }
                else if constexpr (
                    std::is_same_v<M, StringList> ||
                    mrdocs::specialization_of<M, std::vector>)
                {
                    dom::Array arr;
                    for (std::string const& v : values)
                    {
                        arr.push_back(parseCliScalarValue(v));
                    }
                    if (auto e = assignConfigValue(member, dom::Value(arr)); !e)
                    {
                        result = Unexpected(e.error());
                    }
                }
                else if (auto e = assignConfigValue(
                             member, parseCliScalarValue(values.back())); !e)
                {
                    result = Unexpected(e.error());
                }
            }
        });
    return result;
}

} // (anon)

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

//------------------------------------------------

Expected<void>
Config::
load_file(
    Config& c,
    std::string_view const configPath,
    ReferenceDirectories const& dirs,
    char const** argv)
{
    // Read the config file (an empty document when it is absent) and load it
    // with the command-line overrides applied on top, then finalize.
    std::string configYaml;
    auto ft = files::getFileType(configPath);
    MRDOCS_CHECK(ft, formatError(
        "Config file does not exist: \"{}\"", ft.error(), configPath));
    if (ft.value() == files::FileType::regular)
    {
        c.config = configPath;
        configYaml = files::getFileText(c.config).value();
    }
    else
    {
        MRDOCS_CHECK(ft.value() == files::FileType::not_found,
            formatError("Config file is not regular file: \"{}\"", configPath));
    }
    MRDOCS_TRY(Config::load(c, configYaml, argv));
    MRDOCS_TRY(c.normalize(dirs));
    // Startup forces the log level low (errors only) so option parsing stays
    // quiet; now that the configured level is known, restore it and surface
    // the warnings that were deferred until this point.
    report::setMinimumLevel(static_cast<report::Level>(c.logLevel));
    c.reportUnknownConfigKeys();
    c.reportDeprecatedOptions();
    return {};
}

Expected<void>
Config::
load_file(
    Config& c,
    std::string_view const configPath,
    ReferenceDirectories const& dirs)
{
    char const* argv[] = { nullptr };
    return Config::load_file(c, configPath, dirs, argv);
}

struct ConfigSchemaVisitor {
    // Where to leave the deprecated options this pass runs into. Owned by
    // the Config being normalized.
    std::vector<Config::DeprecatedOption>* deprecated = nullptr;

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

    /*  Note an option marked as deprecated, for
        `reportDeprecatedOptions` to announce.

        Saying it here would say it to nobody, as normalization runs while
        the reporting level is still forced down to errors only.
    */
    void
    recordIfDeprecated(
        std::string_view const name,
        bool const isDefault,
        ConfigSchema::OptionProperties const& opts) const
    {
        if (deprecated && opts.deprecated && !isDefault)
        {
            deprecated->push_back({std::string(name), *opts.deprecated});
        }
    }

    Expected<void>
    normalizeString(
        ConfigSchema& self,
        std::string_view const name,
        std::string& value,
        ReferenceDirectories const& dirs,
        ConfigSchema::OptionProperties const& opts,
        bool const usingDefault) const {
        recordIfDeprecated(name, usingDefault || value.empty(), opts);
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
        if (name == "tagfile" && !value.empty())
        {
            // The option this one was renamed to is declared before it,
            // and so is normalized first: the path handed over here is
            // the one that survives. Saying so is left to
            // reportDeprecatedOptions, because this runs while the
            // reporting level is still forced down to errors only.
            MRDOCS_ASSERT(opts.deprecated);
            self.outputTagfile = value;
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
            self.forEach(
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
        recordIfDeprecated(
            name,
            std::holds_alternative<unsigned>(opts.defaultValue)
                && std::cmp_equal(value, std::get<unsigned>(opts.defaultValue)),
            opts);
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
            auto const logLevel = static_cast<ConfigSchema::LogLevel>(value);
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
        self.forEach([&]<typename T>(std::string_view const optionName, T& value)
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
    deprecatedOptions.clear();
    MRDOCS_TRY(ConfigSchema::normalize(
        dirs, ConfigSchemaVisitor{&deprecatedOptions}));
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
reportUnknownConfigKeys() const
{
    MRDOCS_CHECK_OR(warnUnknownConfigKeys);
    auto const level = warnAsError
        ? report::Level::error
        : report::Level::warn;
    for (std::string const& key : unknownConfigKeys)
    {
        report::log(level, "unknown configuration key: \"{}\"", key);
    }
}

void
Config::
reportDeprecatedOptions() const
{
    auto const level = warnAsError
        ? report::Level::error
        : report::Level::warn;
    for (DeprecatedOption const& option : deprecatedOptions)
    {
        report::log(level, "`{}` option is deprecated: {}",
            option.name, option.advice);
    }
}

} // mrdocs
