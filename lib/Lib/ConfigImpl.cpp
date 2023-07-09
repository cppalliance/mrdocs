//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Lib/ConfigImpl.hpp"
#include "Support/Debug.hpp"
#include "Support/Error.hpp"
#include "Support/Path.hpp"
#include "Support/Yaml.hpp"
#include <mrdox/Support/Path.hpp>
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

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::ConfigImpl::SettingsImpl::FileFilter>
{
    static void mapping(IO &io,
        clang::mrdox::ConfigImpl::SettingsImpl::FileFilter& f)
    {
        io.mapOptional("include", f.include);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::ConfigImpl::SettingsImpl>
{
    static void mapping(IO& io,
        clang::mrdox::ConfigImpl::SettingsImpl& cfg)
    {
        io.mapOptional("concurrency",       cfg.concurrency);
        io.mapOptional("defines",           cfg.defines);
        io.mapOptional("ignore-failures",   cfg.ignoreFailures);
        io.mapOptional("include-anonymous", cfg.includeAnonymous);
        io.mapOptional("include-private",   cfg.includePrivate);
        io.mapOptional("multipage",         cfg.multiPage);
        io.mapOptional("source-root",       cfg.sourceRoot);

        io.mapOptional("input",             cfg.input);
    }
};

//------------------------------------------------

namespace clang {
namespace mrdox {

ConfigImpl::
ConfigImpl(
    llvm::StringRef workingDir_,
    llvm::StringRef addonsDir_,
    llvm::StringRef configYaml_,
    llvm::StringRef extraYaml_,
    ConfigImpl const* base)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    // copy the base settings if present
    if(base)
        settings_ = base->settings_;

    if(! files::isAbsolute(workingDir_))
        formatError("working path \"{}\" is not absolute", workingDir_).Throw();
    settings_.workingDir = files::makeDirsy(files::normalizePath(workingDir_));

    // Addons directory
    {
        settings_.addonsDir = files::makeAbsolute(addonsDir_).value();
        files::requireDirectory(settings_.addonsDir).maybeThrow();
        MRDOX_ASSERT(files::isDirsy(settings_.addonsDir));
    }

    settings_.configYaml = configYaml_;
    settings_.extraYaml = extraYaml_;

    // Parse the YAML strings
    YamlReporter reporter;
    {
        llvm::yaml::Input yin(settings_.configYaml,
            &reporter, reporter);
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

    // Post-process as needed
    if( settings_.concurrency == 0)
        settings_.concurrency = llvm::thread::hardware_concurrency();

    // This has to be forward slash style
    settings_.sourceRoot = files::makePosixStyle(files::makeDirsy(
        files::makeAbsolute(settings_.sourceRoot, settings_.workingDir)));

    // adjust input files
    for(auto& name : inputFileIncludes_)
        name = files::makePosixStyle(
            files::makeAbsolute(name, settings_.workingDir));

    threadPool_.reset(settings_.concurrency);
}

//------------------------------------------------

bool
ConfigImpl::
shouldVisitTU(
    llvm::StringRef filePath) const noexcept
{
    if(inputFileIncludes_.empty())
        return true;
    for(auto const& s : inputFileIncludes_)
        if(filePath == s)
            return true;
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
    MRDOX_ASSERT(files::isDirsy(settings_.sourceRoot));
    prefixPath.assign(
        settings_.sourceRoot.begin(),
        settings_.sourceRoot.end());
    return true;
}

//------------------------------------------------

Expected<std::shared_ptr<ConfigImpl const>>
createConfigFromYAML(
    std::string_view workingDir,
    std::string_view addonsDir,
    std::string_view configYaml,
    std::string_view extraYaml)
{
    try
    {
        auto config = std::make_shared<ConfigImpl>(
            workingDir, addonsDir, configYaml, extraYaml, nullptr);
        return config;
    }
    catch(Exception const& ex)
    {
        return ex.error();
    }
}

Expected<std::shared_ptr<ConfigImpl const>>
loadConfigFile(
    std::string_view configFilePath,
    std::string_view addonsDir,
    std::string_view extraYaml,
    std::shared_ptr<ConfigImpl const> base)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    auto temp = files::normalizePath(configFilePath);

    // load the config file into a string
    auto absPath = files::makeAbsolute(temp);
    if(! absPath)
        return absPath.error();
    auto text = files::getFileText(*absPath);
    if(! text)
        return text.error();

    // calculate the working directory
    auto workingDir = files::getParentDir(*absPath);

    // attempt to create the config
    try
    {
        auto config = std::make_shared<ConfigImpl>(
            workingDir, addonsDir, *text, extraYaml, base.get());
        return config;
    }
    catch(Exception const& ex)
    {
        return ex.error();
    }
}

} // mrdox
} // clang
