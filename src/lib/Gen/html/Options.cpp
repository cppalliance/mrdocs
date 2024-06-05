//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Options.hpp"
#include "lib/Support/Yaml.hpp"
#include "lib/Lib/ConfigImpl.hpp" // VFALCO This is a problem
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Support/Path.hpp>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>

namespace clang {
namespace mrdocs {
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
} // mrdocs
} // clang

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdocs::html::YamlKey>
{
    static void mapping(IO& io,
        clang::mrdocs::html::YamlKey& yk)
    {
        auto& opt= yk.opt;
        io.mapOptional("safe-names",  opt.safe_names);
        io.mapOptional("template-dir",  opt.template_dir);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdocs::html::YamlGenKey>
{
    static void mapping(IO& io,
        clang::mrdocs::html::YamlGenKey& ygk)
    {
        clang::mrdocs::html::YamlKey yk(ygk.opt);
        io.mapOptional("html",  yk);
    }
};

template<>
struct llvm::yaml::MappingTraits<
    clang::mrdocs::html::Options>
{
    static void mapping(IO& io,
        clang::mrdocs::html::Options& opt)
    {
        clang::mrdocs::html::YamlGenKey ygk(opt);
        io.mapOptional("generator", ygk);
    }
};

//------------------------------------------------

namespace clang {
namespace mrdocs {
namespace html {

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
            return Unexpected(Error(ec));
    }

    // adjust relative paths
    if(! opt.template_dir.empty())
    {
        opt.template_dir = files::makeAbsolute(
            opt.template_dir,
            corpus.config->configDir);
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
                corpus.config->configDir));
    }
    else
    {
        // VFALCO TODO get process executable
        // and form a path relative to that.
    }

    return opt;
}

} // html
} // mrdocs
} // clang
