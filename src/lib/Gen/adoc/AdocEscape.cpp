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

#include "AdocEscape.hpp"
#include <mrdocs/Support/Handlebars.hpp>
#include <ranges>

namespace clang::mrdocs::adoc {

namespace {

constexpr
std::optional<std::string_view>
HTMLNamedEntity(char const c)
{
    // If c has a named entity, we use it
    // Otherwise, we return std::nullopt
    switch (c)
    {
        // There's no named entity for '~' (U+007E / &#126;) in HTML
        // - "&tilde;" represents a small tilde (U+02DC)
        // - "&Tilde;" or "&sim;" represent the tilde operator (U+223C)
        // The 'tilde operator' (U+223C) is not the same character as
        // "tilde" (U+007E) although the same glyph might be used to
        // represent both.
        // case '~': return "&tilde;";
        case '^': return "&circ;";
        case '_': return "&lowbar;";
        case '*': return "&ast;";
        case '`': return "&grave;";
        case '#': return "&num;";
        case '[': return "&lsqb;";
        case ']': return "&rsqb;";
        case '{': return "&lcub;";
        case '}': return "&rcub;";
        case '<': return "&lt;";
        case '>': return "&gt;";
        case '\\': return "&bsol;";
        case '|': return "&verbar;";
        case '-': return "&hyphen;";
        case '=': return "&equals;";
        case '&': return "&amp;";
        case ';': return "&semi;";
        case '+': return "&plus;";
        case ':': return "&colon;";
        case '.': return "&period;";
        case '"': return "&quot;";
        case '\'': return "&apos;";
        case '/': return "&sol;";
        default:
            break;
        }
    return {};
}

} // (anon)

void
AdocEscape(OutputRef& os, std::string_view str)
{
    static constexpr char reserved[] = R"(~^_*`#[]{}<>\|-=&;+:."\'/)";
    for (char c: str)
    {
        if (std::ranges::find(reserved, c) != std::end(reserved))
        {
            // https://docs.asciidoctor.org/asciidoc/latest/subs/replacements/
            if (auto e = HTMLNamedEntity(c))
            {
                os << *e;
            }
            else
            {
                os << "&#" << static_cast<int>(c) << ';';
            }
        }
        else
        {
            os << c;
        }
    }
}

std::string
AdocEscape(std::string_view str) {
    std::string res;
    OutputRef os(res);
    AdocEscape(os, str);
    return res;
}


} // clang::mrdocs::adoc
