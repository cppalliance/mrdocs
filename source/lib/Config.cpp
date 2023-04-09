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

#include <mrdox/Config.hpp>
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
        io.mapOptional("include",      f.includePaths);
    }
};

//------------------------------------------------

namespace clang {
namespace mrdox {

bool
Config::
filterSourceFile(
    llvm::StringRef filePath,
    llvm::SmallVectorImpl<char>& prefixPath) const noexcept
{
    return false;
    namespace path = llvm::sys::path;

    llvm::SmallString<32> temp;
    for(auto const& s : includePaths)
    {
        temp = filePath;
        if(path::replace_path_prefix(temp, s, ""))
        {
            prefixPath.assign(s.begin(), s.end());
            return false;
        }
    }
    return true;
}

// Make the path end in a separator
static
void
makeDirsy(
    llvm::SmallVectorImpl<char>& s)
{
    namespace path = llvm::sys::path;
    if(! path::is_separator(s.back()))
    {
        auto const sep = path::get_separator();
        s.insert(s.end(), sep.begin(), sep.end());
    }
}

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
    if(! fileText)
    {
        R.failed("llvm::MemoryBuffer::getFile", fileText);
        return false;
    }
    llvm::yaml::Input yin(**fileText);
    yin >> *this;
    std::error_code ec = yin.error();
    if(ec)
    {
        R.failed("llvm::yaml::Input::operator>>", ec);
        return false;
    }

    // change configPath to the directory
    path::remove_filename(configPath);
    path::remove_dots(configPath, true);

    // make includePaths absolute
    {
        llvm::SmallString<128> temp;
        for(auto& s : includePaths)
        {
            if(path::is_absolute(s))
                continue;
            temp.clear();
            path::append(temp, configPath, s);
            // separator at the end is required
            // for replace_path_prefix to work
            path::remove_dots(temp, true);
            makeDirsy(temp);
            s.assign(temp.begin(), temp.end());
        }
    }

    return true;
}

} // mrdox
} // clang
