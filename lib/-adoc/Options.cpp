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
#include "Support/Yaml.hpp"
#include "Tool/ConfigImpl.hpp" // VFALCO This is a problem
#include <mrdox/Corpus.hpp>
#include <mrdox/Support/Path.hpp>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>

namespace clang {
namespace mrdox {
namespace adoc {

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

} // adoc
} // mrdox
} // clang

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::adoc::YamlKey>
{
    static void mapping(IO& io,
        clang::mrdox::adoc::YamlKey& yk)
    {
        auto& opt= yk.opt;
        io.mapOptional("safe-names",  opt.safe_names);
        io.mapOptional("template-dir",  opt.template_dir);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::adoc::YamlGenKey>
{
    static void mapping(IO& io,
        clang::mrdox::adoc::YamlGenKey& ygk)
    {
        clang::mrdox::adoc::YamlKey yk(ygk.opt);
        io.mapOptional("adoc",  yk);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdox::adoc::Options>
{
    static void mapping(IO& io,
        clang::mrdox::adoc::Options& opt)
    {
        clang::mrdox::adoc::YamlGenKey ygk(opt);
        io.mapOptional("generator", ygk);
    }
};

//------------------------------------------------

namespace clang {
namespace mrdox {
namespace adoc {

Expected<Options>
loadOptions(
    Corpus const& corpus)
{
    Options opt;

    YamlReporter reporter;

    // config
    {
        llvm::yaml::Input yin(
            corpus.config->configYaml,
                &reporter, reporter);
        yin.setAllowUnknownKeys(true);
        yin >> opt;
        if(auto ec = yin.error())
            return Error(ec);
    }

    // extra
    {
        llvm::yaml::Input yin(
            corpus.config->extraYaml,
                &reporter, reporter);
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
            corpus.config->workingDir);
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
                corpus.config->workingDir));
    }
    else
    {
        // VFALCO TODO get process executable
        // and form a path relative to that.
    }

    return opt;
}

} // adoc
} // mrdox
} // clang
