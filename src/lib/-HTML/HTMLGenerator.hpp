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

#ifndef MRDOX_LIB_HTML_HTMLGENERATOR_HPP
#define MRDOX_LIB_HTML_HTMLGENERATOR_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Generator.hpp>

namespace clang {
namespace mrdox {
namespace html {

class HTMLGenerator
    : public Generator
{
public:
    std::string_view
    id() const noexcept override
    {
        return "html";
    }

    std::string_view
    displayName() const noexcept override
    {
        return "HTML";
    }

    std::string_view
    fileExtension() const noexcept override
    {
        return "html";
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

} // html
} // mrdox
} // clang

#endif
