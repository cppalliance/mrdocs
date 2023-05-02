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
#include <mrdox/Debug.hpp>
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
        io.mapOptional("concurrency",  cfg.concurrency_);
        io.mapOptional("defines",      cfg.additionalDefines_);
        io.mapOptional("single-page",  cfg.singlePage_);
        io.mapOptional("source-root",  cfg.sourceRoot_);
        io.mapOptional("verbose",      cfg.verbose_);
        io.mapOptional("with-private", cfg.includePrivate_);

        io.mapOptional("input",        cfg.input_);
    }
};

//------------------------------------------------

namespace clang {
namespace mrdox {

llvm::Error
ConfigImpl::
construct(
    llvm::StringRef workingDir,
    llvm::StringRef configYaml,
    llvm::StringRef extraYaml)
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

    configYaml_ = configYaml;
    extraYaml_ = extraYaml;

    // Read the YAML strings
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
            Config const>>
{
    namespace path = llvm::sys::path;

    auto config = std::make_shared<ConfigImpl>();

    if(auto Err = config->construct(
            workingDir, configYaml, extraYaml))
        return std::make_error_code(
            std::errc::invalid_argument);

    return config;
}

} // mrdox
} // clang
