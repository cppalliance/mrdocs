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

#include "Tool/ConfigImpl.hpp"
#include "Support/Debug.hpp"
#include "Support/Error.hpp"
#include "Support/Path.hpp"
#include "Support/YamlFwd.hpp"
#include <mrdox/Support/Path.hpp>
#include <clang/Tooling/AllTUsExecution.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>

#include <mrdox/Support/Report.hpp>

//------------------------------------------------
//
// YAML
//
//------------------------------------------------

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::ConfigImpl::FileFilter>
{
    static void mapping(IO &io,
        clang::mrdox::ConfigImpl::FileFilter& f)
    {
        io.mapOptional("include", f.include);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::ConfigImpl>
{
    static void mapping(
        IO& io, clang::mrdox::ConfigImpl& cfg)
    {
        io.mapOptional("ignore-failures",   cfg.ignoreFailures);
        io.mapOptional("multipage",         cfg.multiPage);
        io.mapOptional("verbose",           cfg.verboseOutput);
        io.mapOptional("with-private",      cfg.includePrivate);
        io.mapOptional("with-anonymous",    cfg.includeAnonymous);
        io.mapOptional("concurrency",       cfg.concurrency);

        io.mapOptional("defines",           cfg.additionalDefines_);
        io.mapOptional("source-root",       cfg.sourceRoot_);

        io.mapOptional("input",             cfg.input_);
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
    llvm::StringRef extraYaml_)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    if(! files::isAbsolute(workingDir_))
        formatError("working path \"{}\" is not absolute", workingDir_).Throw();
    workingDir = files::makeDirsy(files::normalizePath(workingDir_));

    if(auto err = files::requireDirectory(addonsDir_))
        formatError("addons path \"{}\" is not absolute", addonsDir_).Throw();
    MRDOX_ASSERT(files::isDirsy(addonsDir_));
    addonsDir = addonsDir_;

    configYaml = configYaml_;
    extraYaml = extraYaml_;

    // Parse the YAML strings
    {
        llvm::yaml::Input yin(configYaml, this, yamlDiagnostic);
        yin.setAllowUnknownKeys(true);
        yin >> *this;
        Error(yin.error()).maybeThrow();
    }
    {
        llvm::yaml::Input yin(extraYaml, this, yamlDiagnostic);
        yin.setAllowUnknownKeys(true);
        yin >> *this;
        Error(yin.error()).maybeThrow();
    }

    // Post-process as needed
    if( concurrency == 0)
        concurrency = llvm::thread::hardware_concurrency();

    // This has to be forward slash style
    sourceRoot_ = files::makePosixStyle(files::makeDirsy(
        files::makeAbsolute(sourceRoot_, workingDir)));

    // adjust input files
    for(auto& name : inputFileIncludes_)
        name = files::makePosixStyle(
            files::makeAbsolute(name, workingDir));

    threadPool_.reset(concurrency);
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
            files::makeAbsolute(filePath, workingDir));
    }
    else
    {
        temp = filePath;
    }
    if(! path::replace_path_prefix(temp, sourceRoot_, "", path::Style::posix))
        return false;
    MRDOX_ASSERT(files::isDirsy(sourceRoot_));
    prefixPath.assign(sourceRoot_.begin(), sourceRoot_.end());
    return true;
}

void
ConfigImpl::
yamlDiagnostic(
    llvm::SMDiagnostic const& D, void* Ctx)
{
    if(D.getKind() == llvm::SourceMgr::DK_Warning)
        return;
    if(D.getKind() == llvm::SourceMgr::DK_Error)
        llvm::errs() << D.getMessage();
    else
        llvm::outs() << D.getMessage();
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
            workingDir, addonsDir, configYaml, extraYaml);
        return config;
    }
    catch(Error const& err)
    {
        return err;
    }
}

Expected<std::shared_ptr<ConfigImpl const>>
loadConfigFile(
    std::string_view configFilePath,
    std::string_view addonsDir,
    std::string_view extraYaml)
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
            workingDir, addonsDir, *text, extraYaml);
        return config;
    }
    catch(Error const& err)
    {
        return err;
    }
}

} // mrdox
} // clang
