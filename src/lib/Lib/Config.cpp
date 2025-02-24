//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "mrdocs/Config.hpp"
#include <mrdocs/Support/Report.hpp>
#include <mrdocs/Support/Path.hpp>
#include <llvm/Support/FileSystem.h>
#include <ranges>
#include <thread>

namespace clang {
namespace mrdocs {

Config::
Config() noexcept
{
}

Config::
~Config() noexcept = default;

Expected<void>
Config::Settings::
load(
    Config::Settings &s,
    std::string_view const configYaml,
    ReferenceDirectories const& dirs)
{
    MRDOCS_TRY(PublicSettings::load(s, configYaml));
    s.mrdocsRootDir = dirs.mrdocsRoot;
    s.cwdDir = dirs.cwd;
    s.configYaml = configYaml;
    return {};
}

Expected<void>
Config::Settings::
load_file(
    Config::Settings &s,
    std::string_view configPath,
    ReferenceDirectories const& dirs)
{
    auto ft = files::getFileType(configPath);
    MRDOCS_CHECK(ft, formatError(
        "Config file does not exist: \"{}\"", ft.error(), configPath));
    if (ft.value() == files::FileType::regular)
    {
        s.config = configPath;
        std::string configYaml = files::getFileText(s.config).value();
        MRDOCS_TRY(Config::Settings::load(s, configYaml, dirs));
        return {};
    }
    MRDOCS_CHECK(ft.value() == files::FileType::not_found,
        formatError("Config file is not regular file: \"{}\"", configPath));
    return {};
}

struct PublicSettingsVisitor {
    template <class T>
    Expected<void>
    operator()(
        PublicSettings& self,
        std::string_view name,
        T& value,
        ReferenceDirectories const& dirs,
        PublicSettings::OptionProperties const& opts) const {
        using DT = std::decay_t<T>;
        if constexpr (std::ranges::range<DT>)
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
        PublicSettings& self,
        std::string_view const name,
        std::string& value,
        ReferenceDirectories const& dirs,
        PublicSettings::OptionProperties const& opts,
        bool const usingDefault) const {
        if (!value.empty()
            && (opts.type == PublicSettings::OptionType::Path
                || opts.type == PublicSettings::OptionType::DirPath
                || opts.type == PublicSettings::OptionType::FilePath))
        {
            MRDOCS_TRY(
                normalizeStringPath(self, name, value, dirs, opts, usingDefault));
        }
        else if (opts.type == PublicSettings::OptionType::String)
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
        PublicSettings& self,
        std::string_view name,
        std::string& value,
        ReferenceDirectories const& dirs,
        PublicSettings::OptionProperties const& opts,
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
            opts.type != PublicSettings::OptionType::DirPath
                || files::isDirectory(value),
            formatError(
                "`{}` option: path should be a directory: {}",
                name,
                value));
        MRDOCS_CHECK(
            opts.type != PublicSettings::OptionType::FilePath
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
        PublicSettings& self,
        std::string_view,
        PathGlobPattern& value,
        ReferenceDirectories const& dirs,
        PublicSettings::OptionProperties const& opts,
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
                // Make the pattern absolute relative to the base directory
                std::string baseDir = *expBaseDir;
                baseDir = files::makePosixStyle(baseDir);
                absPattern = files::makeAbsolute(pattern, baseDir);
                MRDOCS_TRY(value, PathGlobPattern::create(absPattern));
            }
        }
        return {};
    }

    template <std::ranges::range T>
    requires std::same_as<std::ranges::range_value_t<T>, std::string>
    Expected<void>
    normalizeStringRange(
        PublicSettings& self,
        std::string_view name,
        T& values,
        ReferenceDirectories const& dirs,
        PublicSettings::OptionProperties const& opts,
        bool const usingDefault) const
    {
        if (opts.type == PublicSettings::OptionType::ListPath)
        {
            MRDOCS_TRY(normalizeStringPathRange(self, name, values, dirs, opts, usingDefault));
        }

        return {};
    }

    template <class T>
    Expected<void>
    normalizeStringPathRange(
        PublicSettings& self,
        std::string_view name,
        T& values,
        ReferenceDirectories const& dirs,
        PublicSettings::OptionProperties const& opts,
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
        PublicSettings& self,
        T& values,
        PublicSettings::OptionProperties const& opts) const
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
        PublicSettings& self,
        std::string_view name,
        T& value,
        PublicSettings::OptionProperties const& opts) const
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
                static_cast<unsigned>(PublicSettings::LogLevel::Trace) ==
                static_cast<unsigned>(report::Level::trace));
            static_assert(
                static_cast<unsigned>(PublicSettings::LogLevel::Fatal) ==
                static_cast<unsigned>(report::Level::fatal));
            MRDOCS_ASSERT(opts.deprecated);
            report::warn(
                "`report` option is deprecated, use `log-level` instead");
            auto const logLevel = static_cast<PublicSettings::LogLevel>(value);
            auto logLevelStr = PublicSettings::toString(logLevel);
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
        PublicSettings const& self)
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
        PublicSettings const& settings,
        bool const useDefault,
        PublicSettings::OptionProperties const& opts)
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
Config::Settings::
normalize(ReferenceDirectories const& dirs)
{
    return PublicSettings::normalize(dirs, PublicSettingsVisitor{});
}

std::string
Config::Settings::
configDir() const
{
    return files::getParentDir(config);
}

} // mrdocs
} // clang
