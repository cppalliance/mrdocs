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

#include "ConfigImpl.hpp"
#include "Support/Debug.hpp"
#include "Support/Error.hpp"
#include "Support/Path.hpp"
#include "Support/YamlFwd.hpp"
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
        io.mapOptional("single-page",       cfg.singlePage);
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

Error
ConfigImpl::
construct(
    llvm::StringRef workingDir_,
    llvm::StringRef configYaml_,
    llvm::StringRef extraYaml_)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    workingDir = workingDir_;
    configYaml = configYaml_;
    extraYaml = extraYaml_;

    // Parse the YAML strings
    {
        llvm::yaml::Input yin(configYaml, this, yamlDiagnostic);
        yin.setAllowUnknownKeys(true);
        yin >> *this;
        if(auto ec = yin.error())
            return Error(ec);
    }
    {
        llvm::yaml::Input yin(extraYaml, this, yamlDiagnostic);
        yin.setAllowUnknownKeys(true);
        yin >> *this;
        if(auto ec = yin.error())
            return Error(ec);
    }

    // Post-process as needed
    if( concurrency == 0)
        concurrency = llvm::thread::hardware_concurrency();

    // fix source-root
    auto temp = normalizedPath(sourceRoot_);
    makeDirsy(temp, path::Style::posix);
    sourceRoot_ = temp.str();

    // fix input files
    for(auto& name : inputFileIncludes_)
        name = normalizedPath(name).str();

    return Error::success();
}

llvm::SmallString<0>
ConfigImpl::
normalizedPath(
    llvm::StringRef pathName)
{
    namespace path = llvm::sys::path;

    llvm::SmallString<0> result;
    if(! path::is_absolute(pathName))
    {
        result = workingDir;
        path::append(result, path::Style::posix, pathName);
        path::remove_dots(result, true, path::Style::posix);
    }
    else
    {
        result = pathName;
        path::remove_dots(result, true);
        convert_to_slash(result);
    }
    return result;
}

ConfigImpl::
ConfigImpl()
    : threadPool_(
        llvm::hardware_concurrency(
            tooling::ExecutorConcurrency))
{
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
shouldVisitFile(
    llvm::StringRef filePath,
    llvm::SmallVectorImpl<char>& prefixPath) const noexcept
{
    namespace path = llvm::sys::path;

    llvm::SmallString<32> temp;
    temp = filePath;
    if(! path::replace_path_prefix(temp, sourceRoot_, "", path::Style::posix))
        return false;
    prefixPath.assign(sourceRoot_.begin(), sourceRoot_.end());
    makeDirsy(prefixPath);
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
    llvm::StringRef workingDir,
    llvm::StringRef configYaml,
    llvm::StringRef extraYaml)
{
#if 0
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    SmallPathString temp(workingDir);
    path::remove_dots(temp, true, path::Style::native);
#endif

    auto config = std::make_shared<ConfigImpl>();
    if(auto err = config->construct(workingDir, configYaml, extraYaml))
        return err;
    return config;
}

Expected<std::shared_ptr<ConfigImpl const>>
loadConfigFile(
    std::string_view configFilePath,
    std::string_view extraYaml)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    SmallPathString temp(configFilePath);
    path::remove_dots(temp, true, path::Style::native);

    // ensure configFilePath is a regular file
    fs::file_status stat;
    if(auto ec = fs::status(temp, stat))
        return Error("fs::status(\"{}\") returned \"{}\"", temp, ec);
    if(stat.type() != fs::file_type::regular_file)
        return Error("\"{}\" is not a regular file", temp);

    // load the file into a string
    auto text = llvm::MemoryBuffer::getFile(temp);
    if(! text)
        return Error("MemoryBuffer::getFile(\"{}\") returned \"{}\"", temp, text.getError());

    // calculate the working directory
    SmallPathString workingDir(temp);
    path::remove_filename(workingDir);
    if(auto ec = fs::make_absolute(workingDir))
        return Error("fs::make_absolute(\"{}\") returned \"{}\"", workingDir, ec);
    makeDirsy(workingDir);

    // attempt to create the config
    auto config = std::make_shared<ConfigImpl>();
    if(auto err = config->construct(workingDir, (*text)->getBuffer(), extraYaml))
        return err;
    return config;
}

} // mrdox
} // clang
