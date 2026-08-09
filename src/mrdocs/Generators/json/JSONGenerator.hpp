//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GENERATORS_JSON_JSONGENERATOR_HPP
#define MRDOCS_LIB_GENERATORS_JSON_JSONGENERATOR_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Generator.hpp>

namespace mrdocs::json {

//------------------------------------------------

struct JSONGenerator : Generator
{
    std::string_view
    id() const noexcept override
    {
        return "json";
    }

    std::string_view
    displayName() const noexcept override
    {
        return "JavaScript Object Notation (JSON)";
    }

    std::string_view
    fileExtension() const noexcept override
    {
        return "json";
    }

    Expected<void>
    build(Corpus const& corpus, Config const& config) const override;
};

} // mrdocs::json

#endif
