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
#include <mrdocs/Support/Error.hpp>
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
    std::string_view configYaml,
    ReferenceDirectories const& dirs)
{
    MRDOCS_TRY(PublicSettings::load(s, configYaml));
    s.configDir = dirs.configDir;
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
        Config::Settings::load(s, configYaml, dirs).value();
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
        PublicSettings::OptionProperties const& opts)
    {
        using DT = std::decay_t<T>;
        if constexpr (std::ranges::range<T>)
        {
            bool const useDefault = value.empty() && std::holds_alternative<DT>(opts.defaultValue);
            if (useDefault) {
                value = std::get<DT>(opts.defaultValue);
            }
            MRDOCS_CHECK(!value.empty() || !opts.required,
                formatError("`{}` option is required", name));
            if constexpr (std::same_as<DT, std::string>)
            {
                if (!value.empty() &&
                    (opts.type == PublicSettings::OptionType::Path ||
                    opts.type == PublicSettings::OptionType::DirPath ||
                    opts.type == PublicSettings::OptionType::FilePath))
                {
                    // If the path is not absolute, we need to expand it
                    if (!files::isAbsolute(value)) {
                        auto exp = getBaseDir(value, dirs, useDefault, opts);
                        if (!exp)
                        {
                            MRDOCS_TRY(value, files::makeAbsolute(value));
                        }
                        else
                        {
                            std::string_view baseDir = *exp;
                            value = files::makeAbsolute(value, baseDir);
                        }
                    }
                    MRDOCS_CHECK(!opts.mustExist || files::exists(value),
                        formatError("`{}` option: path does not exist: {}", name, value));
                    MRDOCS_CHECK(opts.type != PublicSettings::OptionType::DirPath || files::isDirectory(value),
                        formatError("`{}` option: path should be a directory: {}", name, value));
                    MRDOCS_CHECK(opts.type != PublicSettings::OptionType::FilePath || !files::isDirectory(value),
                        formatError("`{}` option: path should be a regular file: {}", name, value));
                }
                else if (opts.type == PublicSettings::OptionType::String) {
                    if (name == "base-url")
                    {
                        if (!value.empty() && value.back() != '/') {
                            value.push_back('/');
                        }
                    }
                }
            }
            else if constexpr (std::same_as<DT, std::vector<std::string>>) {
                if (opts.type == PublicSettings::OptionType::ListPath) {
                    for (auto& v : value) {
                        if (!files::isAbsolute(v))
                        {
                            auto exp = getBaseDir(v, dirs, useDefault, opts);
                            if (!exp)
                            {
                                MRDOCS_TRY(v, files::makeAbsolute(v));
                            }
                            else
                            {
                                std::string_view baseDir = *exp;
                                v = files::makeAbsolute(v, baseDir);
                            }
                        }
                        MRDOCS_CHECK(!opts.mustExist || files::exists(v),
                            formatError("`{}` option: path does not exist: {}", name, v));
                        if (opts.commandLineSink && opts.filenameMapping.has_value())
                        {
                            auto const& map = opts.filenameMapping.value();
                            for (auto& [from, to] : map) {
                                auto f = files::getFileName(v);
                                if (f == from)
                                {
                                    auto* dest = fileMapDest(self, to);
                                    if (dest) {
                                        *dest = v;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else if constexpr (std::same_as<DT, int> || std::same_as<DT, unsigned>) {
            if (name == "concurrency" && std::cmp_equal(value, 0))
            {
                value = std::thread::hardware_concurrency();
            }
            MRDOCS_CHECK(!opts.minValue || std::cmp_greater_equal(value, *opts.minValue),
                formatError("`{}` option: value {} is less than minimum: {}", name, value, *opts.minValue));
            MRDOCS_CHECK(!opts.maxValue || std::cmp_less_equal(value, *opts.maxValue),
                formatError("`{}` option: value {} is greater than maximum: {}", name, value, *opts.maxValue));
        }

        // Booleans should already be validated because the struct
        // already has their default values
        return {};
    }

    static
    std::string*
    fileMapDest(PublicSettings& self, std::string_view mapDest)
    {
        if (mapDest == "config") {
            return &self.config;
        }
        if (mapDest == "compilationDatabase") {
            return &self.compilationDatabase;
        }
        return nullptr;
    }

    Expected<std::string_view>
    getBaseDir(
        std::string_view referenceDirKey,
        ReferenceDirectories const& dirs)
    {
        if (referenceDirKey == "config-dir") {
            return dirs.configDir;
        }
        else if (referenceDirKey == "cwd") {
            return dirs.cwd;
        }
        else if (referenceDirKey == "mrdocs-root") {
            return dirs.mrdocsRoot;
        }
        return Unexpected(formatError("unknown relative-to value: \"{}\"", referenceDirKey));
    }

    static
    std::string_view
    trimBaseDirReference(std::string_view s)
    {
        if (s.size() > 2 &&
            s.front() == '<' &&
            s.back() == '>') {
            s.remove_prefix(1);
            s.remove_suffix(1);
        }
        return s;
    };

    Expected<std::string_view>
    getBaseDir(
        std::string& value,
        ReferenceDirectories const& dirs,
        bool useDefault,
        PublicSettings::OptionProperties const& opts)
    {
        if (!useDefault) {
            // If we did not use the default value, we use "relativeto"
            // as the base path
            std::string_view relativeTo = opts.relativeto;
            relativeTo = trimBaseDirReference(relativeTo);
            return getBaseDir(relativeTo, dirs);
        }

        // If we used the default value, the base dir comes from
        // the first path segment of the value
        std::string_view referenceDirKey = value;
        auto pos = referenceDirKey.find('/');
        if (pos != std::string::npos) {
            referenceDirKey = referenceDirKey.substr(0, pos);
        }
        referenceDirKey = trimBaseDirReference(referenceDirKey);
        MRDOCS_TRY(std::string_view baseDir, getBaseDir(referenceDirKey, dirs));
        if (pos != std::string::npos) {
            value = value.substr(pos + 1);
        }
        return baseDir;
    }
};

Expected<void>
Config::Settings::
normalize(ReferenceDirectories const& dirs)
{
    return PublicSettings::normalize(dirs, PublicSettingsVisitor{});
}

} // mrdocs
} // clang
