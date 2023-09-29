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

#include "lib/Lib/ConfigImpl.hpp"
#include "lib/Support/Debug.hpp"
#include "lib/Support/Error.hpp"
#include "lib/Support/Path.hpp"
#include "lib/Support/Yaml.hpp"
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

    if(! files::isAbsolute(workingDir))
        formatError("working path \"{}\" is not absolute", workingDir).Throw();
    settings_.workingDir = files::makeDirsy(files::normalizePath(workingDir));

    // Addons directory
    {
        settings_.addonsDir = files::makeAbsolute(addonsDir).value();
        files::requireDirectory(settings_.addonsDir).maybeThrow();
        MRDOX_ASSERT(files::isDirsy(settings_.addonsDir));
    }

    settings_.configYaml = configYaml;
    settings_.extraYaml = extraYaml;

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

    // This has to be forward slash style
    settings_.sourceRoot = files::makePosixStyle(files::makeDirsy(
        files::makeAbsolute(settings_.sourceRoot, settings_.workingDir)));

    // adjust input files
    for(auto& name : inputFileIncludes_)
        name = files::makePosixStyle(
            files::makeAbsolute(name, settings_.workingDir));
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

    auto temp = files::normalizePath(filePath);

    // load the config file into a string
    MRDOX_TRY(auto absPath, files::makeAbsolute(temp));
    MRDOX_TRY(auto configYaml, files::getFileText(absPath));

    // calculate the working directory
    auto workingDir = files::getParentDir(absPath);

    // attempt to create the config
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

} // mrdox
} // clang
