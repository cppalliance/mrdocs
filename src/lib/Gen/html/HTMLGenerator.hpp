//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_HTML_HTMLGENERATOR_HPP
#define MRDOCS_LIB_GEN_HTML_HTMLGENERATOR_HPP

#include <mrdocs/Platform.hpp>
#include <lib/Gen/hbs/HandlebarsGenerator.hpp>
#include <mrdocs/Generator.hpp>

namespace mrdocs::html {

class HTMLGenerator final
    : public hbs::HandlebarsGenerator
{
public:
    std::string_view
    id() const noexcept override
    {
        return "html";
    }

    std::string_view
    fileExtension() const noexcept override
    {
        return "html";
    }

    std::string_view
    displayName() const noexcept override
    {
        return "HTML";
    }

    void
    escape(OutputRef& os, std::string_view str) const override;
};

} // mrdocs::html

#endif
