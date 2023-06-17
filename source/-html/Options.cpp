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

#include "Options.hpp"
#include "Tool/ConfigImpl.hpp" // VFALCO This is a problem
#include <mrdox/Corpus.hpp>
#include <mrdox/Support/Path.hpp>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>

namespace clang {
namespace mrdox {
namespace html {

struct YamlKey
{
    Options& opt;

    explicit
    YamlKey(
        Options& opt_) noexcept
        : opt(opt_)
    {
    }
};

struct YamlGenKey
{
    Options& opt;

    explicit
    YamlGenKey(
        Options& opt_)
        : opt(opt_)
    {
    }
};

} // html
} // mrdox
} // clang

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::html::YamlKey>
{
    static void mapping(IO& io,
        clang::mrdox::html::YamlKey& yk)
    {
        auto& opt= yk.opt;
        io.mapOptional("safe-names",  opt.safe_names);
        io.mapOptional("template-dir",  opt.template_dir);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::html::YamlGenKey>
{
    static void mapping(IO& io,
        clang::mrdox::html::YamlGenKey& ygk)
    {
        clang::mrdox::html::YamlKey yk(ygk.opt);
        io.mapOptional("html",  yk);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::html::Options>
{
    static void mapping(IO& io,
        clang::mrdox::html::Options& opt)
    {
        clang::mrdox::html::YamlGenKey ygk(opt);
        io.mapOptional("generator", ygk);
    }
};

//------------------------------------------------

namespace clang {
namespace mrdox {
namespace html {

Expected<Options>
loadOptions(
    Corpus const& corpus)
{
    Options opt;

    // config
    {
        llvm::yaml::Input yin(
            corpus.config.configYaml, nullptr,
                ConfigImpl::yamlDiagnostic);
        yin.setAllowUnknownKeys(true);
        yin >> opt;
        if(auto ec = yin.error())
            return Error(ec);
    }

    // extra
    {
        llvm::yaml::Input yin(
            corpus.config.extraYaml, nullptr,
                ConfigImpl::yamlDiagnostic);
        yin.setAllowUnknownKeys(true);
        yin >> opt;
        if(auto ec = yin.error())
            return Error(ec);
    }

    // adjust relative paths

    if(! opt.template_dir.empty())
    {
        opt.template_dir = files::makeAbsolute(
            opt.template_dir,
            corpus.config.workingDir);
    }
    else
    {
        // VFALCO TODO get process executable
        // and form a path relative to that.
    }

    if(! opt.template_dir.empty())
    {
        opt.template_dir = files::makeDirsy(
            files::makeAbsolute(
                opt.template_dir,
                corpus.config.workingDir));
    }
    else
    {
        // VFALCO TODO get process executable
        // and form a path relative to that.
    }

    return opt;
}

} // html
} // mrdox
} // clang
