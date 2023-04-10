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

#ifndef MRDOX_SOURCE_ASCIIDOC_HPP
#define MRDOX_SOURCE_ASCIIDOC_HPP

#include "Representation.h"
#include "Namespace.hpp"
#include <mrdox/Config.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Generator.hpp>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <string>

namespace clang {
namespace mrdox {

namespace {

struct AsciidocGenerator : Generator
{
    llvm::StringRef
    name() const noexcept override
    {
        return "Asciidoc";
    }

    llvm::StringRef
    extension() const noexcept override
    {
        return "adoc";
    }

#if 0
    bool
    build(
        llvm::StringRef rootPath,
        Corpus const& corpus,
        Config const& config,
        Reporter& R) const override;
#endif

    bool
    buildOne(
        llvm::StringRef fileName,
        Corpus const& corpus,
        Config const& config,
        Reporter& R) const override;

    bool
    buildString(
        std::string& dest,
        Corpus const& corpus,
        Config const& config,
        Reporter& R) const override;

    bool
    build(
        llvm::raw_ostream& os,
        Corpus const& corpus,
        Config const& config,
        Reporter& R) const;
};

//------------------------------------------------

class Writer
{
    Corpus const& corpus_;
    Config const& config_;
    Reporter& R_;

public:
    Writer(
        Corpus const& corpus,
        Config const& config,
        Reporter& R) noexcept
        : corpus_(corpus)
        , config_(config)
        , R_(R)
    {
    }

    bool build(llvm::StringRef rootDir);

    bool buildOne(llvm::raw_ostream& os);
};

} // (anon)

} // mrdox
} // clang

#endif
