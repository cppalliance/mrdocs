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

#include "Support/Path.hpp"
#include <mrdox/Config.hpp>
#include <mrdox/Error.hpp>
#include <clang/Tooling/AllTUsExecution.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/ThreadPool.h>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>
#include <atomic>

//------------------------------------------------

// Options from YAML

namespace clang {
namespace mrdox {

struct Config::Options
{
    struct FileFilter
    {
        std::vector<std::string> include;
    };

    bool verbose = true;
    bool include_private = false;
    std::string source_root;
    FileFilter input;
};

} // mrdox
} // clang

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::Config::Options::FileFilter>
{
    static void mapping(IO &io,
        clang::mrdox::Config::Options::FileFilter& f)
    {
        io.mapOptional("include", f.include);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::Config::Options>
{
    static void mapping(
        IO& io, clang::mrdox::Config::Options& opt)
    {
        io.mapOptional("verbose",      opt.verbose);
        io.mapOptional("private",      opt.include_private);
        io.mapOptional("source-root",  opt.source_root);
        io.mapOptional("input",        opt.input);
    }
};

//------------------------------------------------

namespace clang {
namespace mrdox {

//------------------------------------------------
//
// Config::WorkGroup
//
//------------------------------------------------

class Config::WorkGroup::
    Impl : public Base
{
public:
    explicit
    Impl(
        llvm::ThreadPool& threadPool)
        : group_(threadPool)
    {
    }

    llvm::ThreadPoolTaskGroup group_;
};

//------------------------------------------------
//
// Config::Impl
//
//------------------------------------------------

class Config::Impl : public Config
{
    llvm::ThreadPool mutable threadPool_;
    bool doAsync_ = true;

    friend class WorkGroup;

public:
    explicit
    Impl(
        llvm::StringRef configDir)
        : Config(configDir)
        , threadPool_(
            llvm::hardware_concurrency(
                tooling::ExecutorConcurrency))
    {
    }
};

//------------------------------------------------

Config::
WorkGroup::
~WorkGroup()
{
    impl_ = nullptr;
    config_ = nullptr;
}

Config::
WorkGroup::
WorkGroup(
    std::shared_ptr<Config const> config)
    : config_(std::dynamic_pointer_cast<
        Config::Impl const>(std::move(config)))
    , impl_(config_
        ? std::make_unique<Impl>(const_cast<
            llvm::ThreadPool&>(config_->threadPool_))
        : nullptr)
{
}

Config::
WorkGroup::
WorkGroup(
    WorkGroup const& other)
    : config_(other.config_)
    , impl_(other.config_
        ? std::make_unique<Impl>(const_cast<
            llvm::ThreadPool&>(config_->threadPool_))
        : nullptr)
{
}

auto
Config::
WorkGroup::
operator=(
    WorkGroup const& other) ->
        WorkGroup&
{
    if(this == &other)
        return *this;

    if(! other.config_)
    {
        config_ = nullptr;
        impl_ = nullptr;
        return *this;
    }

    auto impl = std::make_unique<Impl>(
        const_cast<llvm::ThreadPool&>(
            other.config_->threadPool_));
    impl_ = std::move(impl);
    config_ = other.config_;
    return *this;
}

void
Config::
WorkGroup::
post(
    std::function<void(void)> f)
{
    if(config_ && config_->doAsync_)
    {
        auto& impl = static_cast<Impl&>(*impl_);
        impl.group_.async(std::move(f));
    }
    else
    {
        f();
    }
}

void
Config::
WorkGroup::
wait()
{
    if(config_ && config_->doAsync_)
    {
        auto& impl = static_cast<Impl&>(*impl_);
        impl.group_.wait();
    }
}

//------------------------------------------------

llvm::SmallString<0>
Config::
normalizePath(
    llvm::StringRef pathName)
{
    namespace path = llvm::sys::path;

    llvm::SmallString<0> result;
    if(! path::is_absolute(pathName))
    {
        result = configDir();
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

Config::
Config(
    llvm::StringRef configDir)
    : configDir_(configDir)
{
}

llvm::Expected<std::shared_ptr<Config>>
Config::
createAtDirectory(
    llvm::StringRef dirPath)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    llvm::SmallString<0> s(dirPath);
    if(auto ec = fs::make_absolute(s))
        return makeError("fs::make_absolute('", s, "') returned ", ec.message());
    path::remove_dots(s, true);
    makeDirsy(s);
    convert_to_slash(s);
    return std::make_shared<Config::Impl>(s);
}

llvm::Expected<std::shared_ptr<Config>>
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
    auto config = createAtDirectory(s);
    if(! config)
        return config.takeError();

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
    (*config)->setVerbose(opt.verbose);
    (*config)->setIncludePrivate(opt.include_private);
    (*config)->setSourceRoot(opt.source_root);
    (*config)->setInputFileIncludes(opt.input.include);

    return config;
}

//------------------------------------------------
//
// Observers
//
//------------------------------------------------

bool
Config::
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
Config::
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

//------------------------------------------------
//
// Modifiers
//
//------------------------------------------------

void
Config::
setSourceRoot(
    llvm::StringRef dirPath)
{
    namespace path = llvm::sys::path;

    auto temp = normalizePath(dirPath);
    makeDirsy(temp, path::Style::posix);
    sourceRoot_ = temp.str();
}

void
Config::
setInputFileIncludes(
    std::vector<std::string> const& list)
{
    namespace path = llvm::sys::path;

    for(auto const& s0 : list)
        inputFileIncludes_.push_back(normalizePath(s0));
}

} // mrdox
} // clang
