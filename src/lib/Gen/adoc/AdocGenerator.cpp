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

#include "AdocGenerator.hpp"
#include "DocVisitor.hpp"

namespace clang {
namespace mrdocs {
namespace adoc {

AdocGenerator::
AdocGenerator()
    : hbs::HandlebarsGenerator("Asciidoc", "adoc", [](
        hbs::HandlebarsCorpus const& c,
        doc::Node const& I) -> std::string
    {
        std::string s;
        DocVisitor visitor(c, s);
        doc::visit(I, visitor);
        return s;
    })
{}

} // adoc

std::unique_ptr<Generator>
makeAdocGenerator()
{
    return std::make_unique<adoc::AdocGenerator>();
}

} // mrdocs
} // clang
