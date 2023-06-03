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

        io.mapOptional("concurrency",       cfg.concurrency_);
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
    llvm::StringRef workingDirArg,
    llvm::StringRef configYamlArg,
    llvm::StringRef extraYamlArg)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    // calculate working directory
    llvm::SmallString<64> s;
    if(workingDirArg.empty())
    {
        if(auto ec = fs::current_path(s))
            return Error(ec);
    }
    else
    {
        s = workingDirArg;
    }
    path::remove_dots(s, true);
    makeDirsy(s);
    convert_to_slash(s);
    workingDir_ = s;
    workingDir = std::string_view(
        workingDir_.data(), workingDir_.size());

    configYamlStr_ = configYamlArg;
    extraYamlStr_ = extraYamlArg;

    configYaml = configYamlStr_;
    extraYaml = extraYamlStr_;

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
    auto config = std::make_shared<ConfigImpl>();
    if(auto err = config->construct(
            workingDir, configYaml, extraYaml))
        return err;
    return config;
}

Expected<std::shared_ptr<ConfigImpl const>>
loadConfigFile(
    std::string_view fileNameArg,
    std::string_view extraYaml)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    std::error_code ec;

    llvm::SmallString<512> fileName(fileNameArg);
    path::remove_dots(fileName, true);

    // ensure fileName is a regular file
    fs::file_status stat;
    if((ec = fs::status(fileName, stat)))
        return Error("fs::status(\"{}\") returned \"{}\"", fileName, ec);
    if(stat.type() != fs::file_type::regular_file)
        return Error("\"{}\" is not a regular file", fileName);

    // load the file into a string
    auto fileText = llvm::MemoryBuffer::getFile(fileName);
    if(! fileText)
        return Error("getFile(\"{}\") returned \"{}\"", fileName, fileText.getError());

    // calculate the working directory
    llvm::SmallString<64> workingDir(fileName);
    path::remove_filename(workingDir);
    if((ec = fs::make_absolute(workingDir)))
        return Error("fs::make_absolute(\"{}\") returned \"{}\"", workingDir, ec);

    // attempt to create the config
    return createConfigFromYAML(workingDir,
        (*fileText)->getBuffer(), extraYaml);
}

} // mrdox
} // clang
