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

#ifndef MRDOX_TOOL_BITCODE_BITCODEGENERATOR_HPP
#define MRDOX_TOOL_BITCODE_BITCODEGENERATOR_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Generator.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Metadata/Javadoc.hpp>
#include <optional>

namespace clang {
namespace mrdox {
namespace bitcode {

struct BitcodeGenerator : Generator
{
    std::string_view
    id() const noexcept override
    {
        return "bitcode";
    }

    std::string_view
    displayName() const noexcept override
    {
        return "LLVM Bitstream container";
    }

    std::string_view
    fileExtension() const noexcept override
    {
        return "bc";
    }

    Error
    build(
        std::string_view outputPath,
        Corpus const& corpus) const override;

    Error
    buildOne(
        std::ostream& os,
        Corpus const& corpus) const override;
};

} // bitcode
} // mrdox
} // clang

#endif
