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
#include "lib/Support/Yaml.hpp"
#include <mrdocs/Support/Error.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/YAMLTraits.h>
#include <ranges>

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdocs::Config::Settings::FileFilter>
{
    static void mapping(IO& io,
        clang::mrdocs::Config::Settings::FileFilter& f)
    {
        io.mapOptional("include", f.include);
        io.mapOptional("file-patterns", f.filePatterns);
    }
};

template<>
struct llvm::yaml::ScalarEnumerationTraits<
    clang::mrdocs::Config::Settings::ExtractPolicy>
{
    using Policy = clang::mrdocs::Config::Settings::ExtractPolicy;

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
    clang::mrdocs::Config::Settings::Filters::Category>
{
    static void mapping(IO &io,
        clang::mrdocs::Config::Settings::Filters::Category& f)
    {
        io.mapOptional("include", f.include);
        io.mapOptional("exclude", f.exclude);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdocs::Config::Settings::Filters>
{
    static void mapping(IO &io,
        clang::mrdocs::Config::Settings::Filters& f)
    {
        io.mapOptional("symbols", f.symbols);
    }
};


template<>
struct llvm::yaml::MappingTraits<clang::mrdocs::Config::Settings>
{
    static void mapping(
        IO& io,
        clang::mrdocs::Config::Settings& s)
    {
        #define INCLUDE_OPTION_OBJECTS
        #define CMDLINE_OPTION(Name, Kebab, Desc)
        #define COMMON_OPTION(Name, Kebab, Desc) io.mapOptional(#Kebab, s.Name);
        #include <mrdocs/ConfigOptions.inc>
    }
};

namespace clang {
namespace mrdocs {

Config::
Config() noexcept
{
}

Config::
~Config() noexcept = default;

Expected<void>
loadConfig(Config::Settings& s, std::string_view configYaml)
{
    YamlReporter reporter;
    {
        llvm::yaml::Input yin(
            configYaml,
            &reporter,
            reporter);
        yin.setAllowUnknownKeys(true);
        yin >> s;
        Error e(yin.error());
        if (e.failed())
        {
            return Unexpected(e);
        }
    }
    return {};
}

} // mrdocs
} // clang
