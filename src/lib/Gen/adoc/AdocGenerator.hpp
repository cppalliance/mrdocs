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

#ifndef MRDOCS_LIB_GEN_ADOC_ADOCGENERATOR_HPP
#define MRDOCS_LIB_GEN_ADOC_ADOCGENERATOR_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Generator.hpp>
#include <lib/Gen/hbs/HandlebarsGenerator.hpp>

namespace clang::mrdocs::adoc {

class AdocGenerator final
    : public hbs::HandlebarsGenerator
{
public:
    std::string_view
    id() const noexcept override
    {
        return "adoc";
    }

    std::string_view
    fileExtension() const noexcept override
    {
        return "adoc";
    }


    std::string_view
    displayName() const noexcept override
    {
        return "Asciidoc";
    }

    void
    escape(OutputRef& os, std::string_view str) const override;
};

} // clang::mrdocs::adoc

#endif
