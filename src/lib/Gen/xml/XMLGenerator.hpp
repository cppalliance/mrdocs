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

#ifndef MRDOCS_LIB_GEN_XML_XMLGENERATOR_HPP
#define MRDOCS_LIB_GEN_XML_XMLGENERATOR_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/MetadataFwd.hpp>
#include <mrdocs/Metadata/Javadoc.hpp>
#include <optional>

namespace clang {
namespace mrdocs {
namespace xml {

//------------------------------------------------

struct XMLGenerator : Generator
{
    std::string_view
    id() const noexcept override
    {
        return "xml";
    }

    std::string_view
    displayName() const noexcept override
    {
        return "Extensible Markup Language (XML)";
    }

    std::string_view
    fileExtension() const noexcept override
    {
        return "xml";
    }

    Error
    buildOne(
        std::ostream& os,
        Corpus const& corpus) const override;
};

} // xml
} // mrdocs
} // clang

#endif
