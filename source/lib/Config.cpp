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

#include "utility.hpp"
#include <mrdox/Config.hpp>
#include <mrdox/Error.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::Config::filter>
{
    static void mapping(
        IO &io, clang::mrdox::Config::filter& f)
    {
        io.mapOptional("exclude", f);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::Config>
{
    static void mapping(
        IO &io, clang::mrdox::Config &f)
    {
        io.mapOptional("namespaces",   f.namespaces);
        io.mapOptional("files",        f.files);
        io.mapOptional("entities",     f.entities);
        io.mapOptional("project-name", f.ProjectName);
        io.mapOptional("public-only",  f.PublicOnly);
        io.mapOptional("output-dir",   f.OutDirectory);
        io.mapOptional("include",      f.sourceRoot_);
    }
};

//------------------------------------------------

namespace clang {
namespace mrdox {

//------------------------------------------------
//
// Observers
//
//------------------------------------------------

bool
Config::
filterSourceFile(
    llvm::StringRef filePath,
    llvm::SmallVectorImpl<char>& prefixPath) const noexcept
{
    namespace path = llvm::sys::path;

    llvm::SmallString<32> temp;
    temp = filePath;
    if(! path::replace_path_prefix(temp, sourceRoot_, ""))
        return true;

    prefixPath.assign(sourceRoot_.begin(), sourceRoot_.end());
    makeDirsy(prefixPath);
    return false;
}

//------------------------------------------------
//
// Modifiers
//
//------------------------------------------------

llvm::Error
Config::
setSourceRoot(
    llvm::StringRef dirPath)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    llvm::SmallString<0> temp(dirPath);
    if(! path::is_absolute(sourceRoot_))
    {
        std::error_code ec = fs::make_absolute(temp);
        if(ec)
            return makeError_("fs::make_absolute got '", ec, "'");
    }

    path::remove_dots(temp, true);

    // This is required for filterSourceFile to work
    makeDirsy(temp);
    sourceRoot_ = temp.str();

    return llvm::Error::success();
}

//------------------------------------------------

bool
Config::
loadFromFile(
    llvm::StringRef filePath,
    Reporter& R)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    // get absolute path to config file
    configPath = filePath;
    fs::make_absolute(configPath);

    // Read the YAML file and apply to this
    auto fileText = llvm::MemoryBuffer::getFile(configPath);
    if(R.error(fileText, "read the file '", configPath, "'"))
        return false;
    llvm::yaml::Input yin(**fileText);
    yin >> *this;
    if(R.error(yin.error(), "parse the YAML file"))
        return false;

    // change configPath to the directory
    path::remove_filename(configPath);
    path::remove_dots(configPath, true);

    // make sourceRoot absolute
    if(! path::is_absolute(sourceRoot_))
    {
        llvm::SmallString<128> temp;
        path::append(temp, configPath, sourceRoot_);
        // separator at the end is required
        // for replace_path_prefix to work
        path::remove_dots(temp, true);
        makeDirsy(temp);
        sourceRoot_.assign(temp.begin(), temp.end());
    }

    return true;
}

} // mrdox
} // clang
