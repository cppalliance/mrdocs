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

namespace clang {
namespace mrdox {

struct Config::Options
{
    struct filter { std::vector<std::string> include, exclude; };
    filter namespaces, files, entities;

    std::string source_root;
    std::vector<std::string> input_include;
};

} // mrdox
} // clang

#if 0
template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::Config::Options::filter>
{
    static void mapping(
        IO &io, clang::mrdox::Config::Options::filter& f)
    {
        io.mapOptional("exclude", f);
    }
};
#endif

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::Config::Options>
{
    static void mapping(
        IO& io, clang::mrdox::Config::Options& opt)
    {
        io.mapOptional("source-root",  opt.source_root);
        io.mapOptional("input",        opt.input_include);
    }
};

//------------------------------------------------

namespace clang {
namespace mrdox {

Config::
Config(
    llvm::StringRef configDir)
    : configDir_(configDir)
{
}

llvm::Expected<std::unique_ptr<Config>>
Config::
createAtDirectory(
    llvm::StringRef dirPath)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    llvm::SmallString<0> s(dirPath);
    std::error_code ec = fs::make_absolute(s);
    if(ec)
        return makeError("fs::make_absolute('", s, "') returned ", ec.message());
    path::remove_dots(s, true);
    makeDirsy(s);
    convert_to_slash(s);
    return std::unique_ptr<Config>(new Config(s));
}

llvm::Expected<std::unique_ptr<Config>>
Config::
loadFromFile(
    llvm::StringRef filePath)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    // check filePath is a regular file
    fs::file_status stat;
    if(auto ec = fs::status(filePath, stat))
        return makeError("fs::status('", filePath, "') returned ", ec.message());
    if(stat.type() != fs::file_type::regular_file)
        return makeError("path '", filePath, "' is not a regular file");

    // create initial config at config dir
    llvm::SmallString<0> s(filePath);
    path::remove_filename(s);
    llvm::Expected<std::unique_ptr<Config>> config = createAtDirectory(s);

    // Read the YAML file into Options
    Options opt;
    auto fileText = llvm::MemoryBuffer::getFile(filePath);
    if(! fileText)
        return makeError(fileText.getError().message(), " when loading file '", filePath, "' ");
    {
        llvm::yaml::Input yin(**fileText);
        yin >> opt;
        if(auto ec = yin.error())
            return makeError(ec.message(), " when parsing file '", filePath, "' ");
    }

    // apply opt to Config
    if(auto err = (*config)->setSourceRoot(opt.source_root))
        return err;

    return config;
}

//------------------------------------------------
//
// Observers
//
//------------------------------------------------

bool
Config::
shouldVisitFile(
    llvm::StringRef filePath,
    llvm::SmallVectorImpl<char>& prefixPath) const noexcept
{
    namespace path = llvm::sys::path;

    llvm::SmallString<32> temp;
    temp = filePath;
    if(! path::replace_path_prefix(temp, sourceRoot_, ""))
        return false;

    prefixPath.assign(sourceRoot_.begin(), sourceRoot_.end());
    makeDirsy(prefixPath);
    return true;
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
            return makeError("fs::make_absolute got '", ec, "'");
    }

    path::remove_dots(temp, true, path::Style::posix);

    // This is required for filterSourceFile to work
    makeDirsy(temp, path::Style::posix);
    sourceRoot_ = temp.str();

    return llvm::Error::success();
}

llvm::Error
Config::
setInputFileFilter(
    std::vector<std::string> const& list)
{
    namespace path = llvm::sys::path;

    for(auto const& s0 : list)
    {
        std::string s;
        if(path::is_absolute(s0))
            s = path::convert_to_slash(s0);
    }
    return llvm::Error::success();
}

} // mrdox
} // clang
