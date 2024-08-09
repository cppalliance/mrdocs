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
    if(! ft)
    {
        return formatError(
            "Config file does not exist: \"{}\"", ft.error(), configPath);
    }

    if (ft.value() == files::FileType::regular)
    {
        s.config = configPath;
        std::string configYaml = files::getFileText(s.config).value();
        Config::Settings::load(s, configYaml, dirs).value();
    }
    else if(ft.value() != files::FileType::not_found)
    {
        return formatError(
            "Config file is not regular file: \"{}\"", configPath);
    }

    return {};
}

Expected<void>
Config::Settings::
normalize(ReferenceDirectories const& dirs)
{
    auto exp = PublicSettings::normalize(*this, dirs);
    if (!exp)
    {
        return exp.error();
    }

    // Base-URL has to be dirsy with forward slash style
    if (!baseUrl.empty() && baseUrl.back() != '/')
    {
        baseUrl.push_back('/');
    }

    return {};
}

} // mrdocs
} // clang
