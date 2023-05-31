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
#include "Support/Path.hpp"
#include "Support/YamlFwd.hpp"
#include "Support/Debug.hpp"
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

llvm::Error
ConfigImpl::
construct(
    llvm::StringRef workingDir,
    llvm::StringRef configYamlStr,
    llvm::StringRef extraYamlStr)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    // calculate working directory
    llvm::SmallString<64> s;
    if(workingDir.empty())
    {
        if(auto ec = fs::current_path(s))
            return makeError(ec);
    }
    else
    {
        s = workingDir;
    }
    path::remove_dots(s, true);
    makeDirsy(s);
    convert_to_slash(s);
    workingDir_ = s;

    configYamlStr_ = configYamlStr;
    extraYamlStr_ = extraYamlStr;

    configYaml = configYamlStr_;
    extraYaml = extraYamlStr_;

    // Parse the YAML strings
    {
        llvm::yaml::Input yin(configYaml, this, yamlDiagnostic);
        yin.setAllowUnknownKeys(true);
        yin >> *this;
        if(auto ec = yin.error())
            return makeError(ec);
    }
    {
        llvm::yaml::Input yin(extraYaml, this, yamlDiagnostic);
        yin.setAllowUnknownKeys(true);
        yin >> *this;
        if(auto ec = yin.error())
            return makeError(ec);
    }

    // Post-process as needed

    // fix source-root
    auto temp = normalizedPath(sourceRoot_);
    makeDirsy(temp, path::Style::posix);
    sourceRoot_ = temp.str();

    // fix input files
    for(auto& name : inputFileIncludes_)
        name = normalizedPath(name).str();

    return llvm::Error::success();
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
        result = workingDir();
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

auto
createConfigFromYAML(
    llvm::StringRef workingDir,
    llvm::StringRef configYaml,
    llvm::StringRef extraYaml) ->
        llvm::ErrorOr<std::shared_ptr<
            ConfigImpl const>>
{
    namespace path = llvm::sys::path;

    auto config = std::make_shared<ConfigImpl>();

    if(auto Err = config->construct(
            workingDir, configYaml, extraYaml))
        return std::make_error_code(
            std::errc::invalid_argument);

    return config;
}

std::shared_ptr<ConfigImpl const>
loadConfigFile(
    std::string_view fileName,
    std::string_view extraYaml,
    std::error_code& ec)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    // ensure fileName is a regular file
    fs::file_status stat;
    if((ec = fs::status(fileName, stat)))
        return {};
    if(stat.type() != fs::file_type::regular_file)
    {
        ec = std::make_error_code(
            std::errc::invalid_argument);
        return {};
    }

    // load the file into a string
    auto fileText = llvm::MemoryBuffer::getFile(fileName);
    if(! fileText)
    {
        ec = fileText.getError();
        return {};
    }

    // calculate the working directory
    llvm::SmallString<64> workingDir(fileName);
    path::remove_filename(workingDir);
    if((ec = fs::make_absolute(workingDir)))
        return {};

    // attempt to create the config
    auto result = createConfigFromYAML(
        workingDir, (*fileText)->getBuffer(), extraYaml);

    if(! result)
    {
        ec = result.getError();
        return {};
    }
    return result.get();
}

std::shared_ptr<ConfigImpl const>
loadConfigString(
    std::string_view workingDir,
    std::string_view configYaml,
    std::error_code& ec)
{
    auto result = createConfigFromYAML(
        workingDir, configYaml, "");
    if(! result)
    {
        ec = result.getError();
        return {};
    }
    ec = {};
    return result.get();
}

} // mrdox
} // clang
