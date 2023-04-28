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

#ifndef MRDOX_API_BITCODE_BITCODEGENERATOR_HPP
#define MRDOX_API_BITCODE_BITCODEGENERATOR_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Generator.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Metadata/Javadoc.hpp>
#include <llvm/ADT/Optional.h>

namespace clang {
namespace mrdox {
namespace bitcode {

struct BitcodeGenerator : Generator
{
    llvm::StringRef
    name() const noexcept override
    {
        return "bitcode";
    }

    llvm::StringRef
    displayName() const noexcept override
    {
        return "LLVM Bitstream container";
    }

    llvm::StringRef
    extension() const noexcept override
    {
        return "bc";
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

} // bitcode
} // mrdox
} // clang

#endif
