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
#include <mrdocs/Support/Handlebars.hpp>

namespace clang {
namespace mrdocs {
namespace adoc {

AdocGenerator::
AdocGenerator()
    : hbs::HandlebarsGenerator("Asciidoc", "adoc")
{}

std::string
AdocGenerator::
toString(hbs::HandlebarsCorpus const& c, doc::Node const& I) const
{
    std::string s;
    DocVisitor visitor(c, s);
    doc::visit(I, visitor);
    return s;
}

void
AdocGenerator::
escape(OutputRef& os, std::string_view str) const
{
    static constexpr std::string_view formattingChars = "\\`*_{}[]()#+-.!|";
    bool const needsEscape = str.find_first_of(formattingChars) != std::string_view::npos;
    if (needsEscape)
    {
        os << "pass:[";
        // Using passthroughs to pass content (without substitutions) can couple
        // your content to a specific output format, such as HTML.
        // In these cases, you should use conditional preprocessor directives
        // to route passthrough content for different output formats based on
        // the current backend.
        // If we would like to couple passthrough content to an HTML format,
        // then we'd use `HTMLEscape(os, str)` instead of `os << str`.
        os << str;
        os << "]";
    }
    else
    {
        os << str;
    }
}

} // adoc

std::unique_ptr<Generator>
makeAdocGenerator()
{
    return std::make_unique<adoc::AdocGenerator>();
}

} // mrdocs
} // clang
