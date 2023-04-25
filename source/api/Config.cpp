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
#include <mrdox/Error.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>

//------------------------------------------------
//
// Options (YAML)
//
//------------------------------------------------

namespace clang {
namespace mrdox {

class Config::Options
{
public:
    struct FileFilter
    {
        std::vector<std::string> include;
    };

    bool verbose = true;
    bool include_private = false;
    std::string source_root;
    bool single_page = false;
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
        io.mapOptional("single-page",  opt.single_page);
        io.mapOptional("input",        opt.input);
    }
};

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
        llvm::ThreadPool& threadPool) noexcept
        : group_(threadPool)
    {
    }

    llvm::ThreadPoolTaskGroup group_;
};

Config::
WorkGroup::
Base::~Base() noexcept = default;

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
    Config const* config)
    : config_(config
        ? dynamic_cast<ConfigImpl const*>(
            config)->shared_from_this()
        : nullptr)
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
//
// Config
//
//------------------------------------------------

Config::
Config(
    llvm::StringRef configDir)
    : configDir_(configDir)
{
}

Config::
~Config() noexcept = default;

//------------------------------------------------

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
    return std::make_shared<ConfigImpl>(s);
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
    Config::Options opt;
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
    (*config)->setSinglePage(opt.single_page);

    return config;
}

} // mrdox
} // clang
