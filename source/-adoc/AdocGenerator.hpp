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

#ifndef MRDOX_TOOL_ADOC_ADOCGENERATOR_HPP
#define MRDOX_TOOL_ADOC_ADOCGENERATOR_HPP

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
    std::string_view
    id() const noexcept override
    {
        return "adoc";
    }

    std::string_view
    displayName() const noexcept override
    {
        return "Asciidoc";
    }

    std::string_view
    fileExtension() const noexcept override
    {
        return "adoc";
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

} // adoc
} // mrdox
} // clang

#endif
