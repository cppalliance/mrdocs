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

#ifndef MRDOX_API_ADOC_ADOCGENERATOR_HPP
#define MRDOX_API_ADOC_ADOCGENERATOR_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Generator.hpp>

namespace clang {
namespace mrdox {
namespace adoc {

class AdocGenerator
    : public Generator
{
    struct MultiPageBuilder;
    struct SinglePageBuilder;

public:
    llvm::StringRef
    name() const noexcept override
    {
        return "adoc";
    }

    llvm::StringRef
    displayName() const noexcept override
    {
        return "Asciidoc";
    }

    llvm::StringRef
    extension() const noexcept override
    {
        return "adoc";
    }

    llvm::Error
    buildPages(
        llvm::StringRef outputPath,
        Corpus const& corpus,
        Reporter& R) const override;

    llvm::Error
    buildSinglePage(
        llvm::raw_ostream& os,
        Corpus const& corpus,
        Reporter& R,
        llvm::raw_fd_ostream* fd_os) const override;
};

} // adoc
} // mrdox
} // clang

#endif
