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

#include "HTMLGenerator.hpp"
#include "DocVisitor.hpp"
#include <mrdocs/Support/Handlebars.hpp>

namespace clang {
namespace mrdocs {
namespace html {

std::string
HTMLGenerator::
toString(hbs::HandlebarsCorpus const& c, doc::Node const& I) const
{
    std::string s;
    DocVisitor visitor(c, s);
    doc::visit(I, visitor);
    return s;
}

void
HTMLGenerator::
escape(OutputRef& os, std::string_view str) const
{
    HTMLEscape(os, str);
}

} // html

//------------------------------------------------

std::unique_ptr<Generator>
makeHTMLGenerator()
{
    return std::make_unique<html::HTMLGenerator>();
}

} // mrdocs
} // clang
